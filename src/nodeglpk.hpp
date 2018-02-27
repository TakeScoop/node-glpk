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
    HookInfo(std::shared_ptr<NodeEvent::EventEmitter> emit, const ReentrantCWorker::ExecutionProgressSender* send,
             eventemitter_fn_r func)
        : emitter(emit), sender(send), fn(func) {}

    /// An emitter. Will be used if not-null, ignoring anything else.
    std::shared_ptr<NodeEvent::EventEmitter> emitter;

    /// The sender. both sender and fn must be set together. Only used if emitter is nullptr
    const ReentrantCWorker::ExecutionProgressSender* sender;

    /// The function to invoke (with sender as first argument). Only used if emitter is nullptr
    eventemitter_fn_r fn;
};

int stdoutTermHook(void*, const char* s);
int eventTermHook(void* info, const char* s);
void _ErrorHook(void* s);

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
    /// function (ugly style function pointer) that can have it's address taken)
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

    /// Set the info for the current thread.
    ///
    /// @param[in] info - The info to set for this thread
    ///
    /// @returns the prior info, use this to restore the prior info back when you're done
    static std::shared_ptr<HookInfo> SetInfo(std::shared_ptr<HookInfo> info) {
        auto oldInfo = info_;
        info_ = info;
        glp_error_hook(_ErrorHook, static_cast<void*>(info.get()));
        glp_term_hook(NodeHookCallback, static_cast<void*>(info_.get()));
        return oldInfo;
    }

    /// @returns the current info
    static std::shared_ptr<HookInfo> Current() { return info_; }

    /// free the environment of the current thread
    static void ClearEnv() {
        glp_free_env();
        info_ = nullptr;
    }

 private:
    static std::vector<term_hook_fn> term_hooks_;
    static NodeEvent::uv_rwlock lock_;
    static thread_local std::shared_ptr<HookInfo> info_;
};

/// TermHookGuard is an RAII container for HookInfo management. Ensures the prior info is restored regardless of how you
/// exit a block. Declare the guard in the same scope (or nested scope) of wherever you allocate the NodeInfo you are
/// setting to ensure you never have a dangling pointer
///
class TermHookGuard {
 public:
    explicit TermHookGuard(std::shared_ptr<HookInfo> info) : oldinfo_(nullptr) {
        if (info && TermHookManager::Current() != info) {
            oldinfo_ = TermHookManager::SetInfo(info);
        }
    }
    ~TermHookGuard() noexcept { TermHookManager::SetInfo(oldinfo_); }

 private:
    std::shared_ptr<HookInfo> oldinfo_;
};

/// TermHookThreadGuard is similar to TermHookGuard but instead of restoring the prior info, deletes the environment of
/// the thread. This guard is suitable for the worker-thread method, where cleanup should happen when the thread
/// completes.
class TermHookThreadGuard {
 public:
    explicit TermHookThreadGuard(std::shared_ptr<HookInfo> info) {
        // Ensure this thread's env is setup and has our hooks
        if(info && TermHookManager::Current() != info) { 
            TermHookManager::SetInfo(info);  
        }
    }

    ~TermHookThreadGuard() noexcept { TermHookManager::ClearEnv(); }
};

class MemStatsGuard {
 public:
    explicit MemStatsGuard(std::shared_ptr<glp_memstats> memstats) : memstats_(glp_set_memstats(memstats.get())) {}
    ~MemStatsGuard() noexcept { glp_set_memstats(memstats_); }
 private:
     glp_memstats* memstats_;
};

/// EventEmitterDecorator decorates a Nan::AsyncWorker so that the Execute() method is wrapped with a TermHookThreadGuard
class EventEmitterDecorator : public ReentrantCWorker {
 public:
    EventEmitterDecorator(Nan::AsyncWorker* decorated, std::shared_ptr<NodeEvent::EventEmitter> emitter)
        : ReentrantCWorker(nullptr, emitter), decorated_(decorated) {}

    virtual void HandleOKCallback() override { }
    virtual void HandleErrorCallback() override { } 

    virtual void WorkComplete() override {
        decorated_->WorkComplete();
        ReentrantCWorker::WorkComplete();
    }

    virtual void ExecuteWithEmitter(const ExecutionProgressSender* sender, eventemitter_fn_r fn) override {
        auto info = std::make_shared<HookInfo>(nullptr, sender, fn);
        TermHookThreadGuard hookguard{info};
        decorated_->Execute();
    }

    virtual void Destroy() override {
        decorated_->Destroy();
        ReentrantCWorker::Destroy();
    }

 private:
     Nan::AsyncWorker* decorated_;
};

class MemStatsDecorator : public Nan::AsyncWorker {
 public:
    MemStatsDecorator(Nan::AsyncWorker* decorated, std::shared_ptr<glp_memstats> memstats)
        : Nan::AsyncWorker(nullptr), decorated_(decorated), memstats_(memstats) {}

    virtual void HandleOKCallback() override { }
    virtual void HandleErrorCallback() override { } 

    virtual void WorkComplete() override {
        MemStatsGuard mguard{memstats_};
        decorated_->WorkComplete();
        Nan::AsyncWorker::WorkComplete();
    }

    virtual void Execute() override {
        MemStatsGuard mguard{memstats_};
        decorated_->Execute();
    }

    virtual void Destroy() override {
        MemStatsGuard mguard{memstats_};
        decorated_->Destroy();
        Nan::AsyncWorker::Destroy();
    }

 private:
     Nan::AsyncWorker* decorated_;
     std::shared_ptr<glp_memstats> memstats_;
};


}  // namespace NodeGLPK
#endif
