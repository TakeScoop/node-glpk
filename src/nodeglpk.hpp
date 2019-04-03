#pragma once
#ifndef _NODE_GLPK_NODEGLPK_H
#define _NODE_GLPK_NODEGLPK_H

#include <memory>
#include <thread>
#include <vector>
#include <iostream>

#include <eventemitter.hpp>

#include "glpk/glpk.h"
#include "glpk/env/glpenv.h"


namespace NodeGLPK {
typedef int (*term_hook_fn)(void* info, const char* s);
typedef NodeEvent::AsyncEventEmittingReentrantCWorker<32> ReentrantCWorker;

/// The state information passed to all term_hooks. 
struct HookInfo {
    explicit HookInfo(std::shared_ptr<NodeEvent::EventEmitter> emit) : emitter(emit) {}
        
    HookInfo(const ReentrantCWorker::ExecutionProgressSender* send,
             eventemitter_fn_r func) : sender(send), fn(func) {}

    /// An emitter. Will be used if not-null, ignoring anything else.
    std::shared_ptr<NodeEvent::EventEmitter> emitter;

    /// The sender. both sender and fn must be set together. Only used if emitter is nullptr
    const ReentrantCWorker::ExecutionProgressSender* sender;

    /// The function to invoke (with sender as first argument). Only used if emitter is nullptr
    eventemitter_fn_r fn;
};

/**
 * MemoryStatistics is a threadsafe container for keeping track the various memory counters that can exist.
 * This is suitable for use as a global/singleton static object.
 */
class MemoryStatistics {
 public:
     MemoryStatistics() : counters_{0,0,0,0}, lock_{} {}
     ~MemoryStatistics() noexcept = default;

     size_t count() {
         NodeEvent::shared_lock<NodeEvent::uv_rwlock> guard{lock_};
         return counters_.mem_count;
     }

     size_t cpeak() {
         NodeEvent::shared_lock<NodeEvent::uv_rwlock> guard{lock_};
         return counters_.mem_cpeak;
     }

     size_t total() {
         NodeEvent::shared_lock<NodeEvent::uv_rwlock> guard{lock_};
         return counters_.mem_total;
     }

     size_t tpeak() {
         NodeEvent::shared_lock<NodeEvent::uv_rwlock> guard{lock_};
         return counters_.mem_tpeak;
     }

     void updateCounters(struct glp_memory_counters& before, struct glp_memory_counters& now) {
         std::unique_lock<NodeEvent::uv_rwlock> guard{lock_};

         counters_.mem_count = counters_.mem_count - before.mem_count + now.mem_count;
         counters_.mem_cpeak = counters_.mem_cpeak - before.mem_cpeak + now.mem_cpeak;
         counters_.mem_total = counters_.mem_total - before.mem_total + now.mem_total;
         counters_.mem_tpeak = counters_.mem_tpeak - before.mem_tpeak + now.mem_tpeak;
     }

     /**
      * @param[in] env_state - Environment state object
      * @param[out] counters - The counters to update
      */
     void removeStateCounters(std::shared_ptr<glp_environ_state_t> env_state, glp_memory_counters& counters) {
         struct glp_memory_counters new_counters = glp_counters_from_state(env_state.get());
         counters = new_counters;
         counters.mem_count = 0; // When we set env_state_ to null, these get freed
         counters.mem_total = 0; // When we set env_state_ to null, these get freed
         this->updateCounters(new_counters, counters);
     }

 private:
    struct glp_memory_counters counters_;
    NodeEvent::uv_rwlock lock_;
};

int stdoutTermHook(void*, const char* s);
int eventTermHook(void* info, const char* s);
void _ErrorHook(void* s);

extern MemoryStatistics _global_memory_statistics;

/// TermHookManager provides static methods for managing TermHooks, and hook registration
class TermHookManager {
 public:
    /// The callback registered with glp_term_hook. Runs through all the registered hooks and calls them in order
    ///
    /// @param[in] info - The info from the thread-local env
    /// @param[in] s - the string being logged to term_hook
    ///
    /// @returns an integer; if any hook returns non-zero, will return that; 0 otherwise.
    static int NodeHookCallback(void* info, const char* s) {
        NodeEvent::shared_lock<NodeEvent::uv_rwlock> guard{lock_};
        int ret = 0;
        for (auto hook : term_hooks_) {
            int r = hook(info, s);
            if (r) {
                ret = r;
            }
        }
        return ret;
    }

    /// Add a hook to the list of hooks that will be run. Duplicate hooks will be ignored (must be a C compatible
    /// function (ugly style function pointer) that can have its address taken)
    ///
    /// @param[in] hook - The hook to add
    static void AddHook(term_hook_fn hook) {
        std::unique_lock<NodeEvent::uv_rwlock> guard{lock_};
        for (auto existingHook : term_hooks_) {
            // Skip duplicates
            if (hook == existingHook) {
                return;
            }
        }
        term_hooks_.push_back(hook);
    }

    /// Clear all hooks registered with the TermHookManager
    static void ClearHooks() {
        std::unique_lock<NodeEvent::uv_rwlock> guard{lock_};
        term_hooks_.clear();
    }

 private:
    static std::vector<term_hook_fn> term_hooks_;
    static NodeEvent::uv_rwlock lock_;
};


class GLPKEnvStateGuard {
 public:
    GLPKEnvStateGuard(std::shared_ptr<glp_environ_state_t> state, std::shared_ptr<HookInfo> info) : env_state_(state) {
        using namespace std;
        glp_env_tls_init_r(env_state_.get(), static_cast<void*>(info.get()));
    }
    ~GLPKEnvStateGuard() noexcept {
        struct glp_memory_counters counters1 = glp_counters_from_state(env_state_.get());
        glp_env_tls_finalize_r(env_state_.get());
        struct glp_memory_counters counters2 = glp_counters_from_state(env_state_.get());
        _global_memory_statistics.updateCounters(counters1, counters2);
    }
 private:
    std::shared_ptr<glp_environ_state_t> env_state_;
};


static inline std::shared_ptr<glp_environ_state_t> make_shared_environ_state(std::shared_ptr<HookInfo> info) {
    auto state = std::shared_ptr<glp_environ_state_t>(
        glp_init_env_state(static_cast<void*>(info.get()), TermHookManager::NodeHookCallback), glp_free_env_state);
    return state;
}

class GLPKEnvStateDecorator : public ReentrantCWorker {
 public:
    GLPKEnvStateDecorator(Nan::AsyncWorker* decorated, std::shared_ptr<NodeEvent::EventEmitter> emitter,
                          std::shared_ptr<glp_environ_state_t> env_state)
        : ReentrantCWorker(nullptr, emitter), decorated_(decorated), env_state_(env_state), emitter_(emitter) {}

    virtual void HandleOKCallback() override { }
    virtual void HandleErrorCallback() override { } 

    virtual void WorkComplete() override {
        auto info = std::make_shared<HookInfo>(emitter_);
        GLPKEnvStateGuard stateguard{env_state_, info};
        decorated_->WorkComplete();
        ReentrantCWorker::WorkComplete();
    }

    virtual void ExecuteWithEmitter(const ExecutionProgressSender* sender, eventemitter_fn_r fn) override {
        auto info = std::make_shared<HookInfo>(sender, fn);
        GLPKEnvStateGuard stateguard{env_state_, info};
        decorated_->Execute();
    }

    virtual void Destroy() override {
        auto info = std::make_shared<HookInfo>(emitter_);
        GLPKEnvStateGuard stateguard{env_state_, info};
        decorated_->Destroy();
        ReentrantCWorker::Destroy();
    }

 private:
     Nan::AsyncWorker* decorated_;
     std::shared_ptr<glp_environ_state_t> env_state_;
     std::shared_ptr<NodeEvent::EventEmitter> emitter_;
};


}  // namespace NodeGLPK
#endif
