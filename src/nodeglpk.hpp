#pragma once
#ifndef _NODE_GLPK_NODEGLPK_H
#define _NODE_GLPK_NODEGLPK_H

#include <memory>
#include <thread>
#include <vector>

#include <eventemitter.hpp>

#include "glpk/glpk.h"


namespace NodeGLPK {
typedef int (*term_hook_fn)(void* info, const char* s);
typedef NodeEvent::AsyncEventEmittingReentrantCWorker<32> ReentrantCWorker;

/// The state information passed to all term_hooks. 
struct HookInfo {
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

    /// An Idempotent function for ensuring that the thread-local env is setup
    /// Pass in info to set it for the thread (ignoring any prior values)
    ///
    /// @param[in] info - A hook info that will be valid for the life of the thread or until set to nullptr
    static void ThreadInitDefaultHooks(HookInfo* info) {
        glp_error_hook(_ErrorHook, info);
        if (info) {
            TermHookManager::SetInfo(info);
        }
        TermHookManager::AddHook(stdoutTermHook);
        TermHookManager::AddHook(eventTermHook);
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
    static HookInfo* SetInfo(HookInfo* info) {
        auto oldInfo = info_;
        info_ = info;
        glp_term_hook(NodeHookCallback, static_cast<void*>(info_));
        return oldInfo;
    }

    /// @returns the current info
    static HookInfo* Current() { return info_; }

 private:
    static std::vector<term_hook_fn> term_hooks_;
    static NodeEvent::uv_rwlock lock_;
    static thread_local HookInfo* info_;
};

/// TermHookGuard is an RAII container for HookInfo management. Ensures the prior info is restored regardless of how you
/// exit a block. Declare the guard in the same scope (or nested scope) of wherever you allocate the NodeInfo you are
/// setting to ensure you never have a dangling pointer
///
/// NOTE: This container does *NOT* free an info object for you; use stack-allocated info's; or wrap them in smart
/// pointers.
class TermHookGuard {
 public:
    explicit TermHookGuard(HookInfo* info) : oldinfo_(nullptr) {
        // Ensure this thread's env is setup and has our hooks
        TermHookManager::ThreadInitDefaultHooks(nullptr);  
        if (info && TermHookManager::Current() != info) {
            oldinfo_ = TermHookManager::SetInfo(info);
        }
    }
    ~TermHookGuard() noexcept { TermHookManager::SetInfo(oldinfo_); glp_free_env(); }

 private:
    HookInfo* oldinfo_;
};

}  // namespace NodeGLPK
#endif
