#pragma once
#ifndef _GLPK_EVENTEMITTER_H
#define _GLPK_EVENTEMITTER_H

#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <nan.h>
#include <node.h>
#include <node_object_wrap.h>

#include "cemitter.h"

// TODO(jrb):
// - Implement testing (gtest/gmock)

namespace NodeGLPK {

/// A base-class for thing that behave like EventEmitter in node
class EventEmitter {
 public:
    /// An error indicating the event name is not known
    class InvalidEvent : std::runtime_error {
     public:
        explicit InvalidEvent(const std::string& msg) : std::runtime_error(msg) {}
    };

    EventEmitter() : receivers_lock_(), receivers_() {}
    virtual ~EventEmitter() noexcept = default;

    /// Set a callback for a given event name
    ///
    /// @param[in] ev - event name
    /// @param[in] cb - callback
    virtual void on(const std::string& ev, Nan::Callback* cb);

    /// Emit a value to any registered callbacks for the event
    ///
    /// @param[in] ev - event name
    /// @param[in] value - a string to emit
    ///
    /// @returns true if the event has listeners, false otherwise
    virtual bool emit(const std::string& ev, const std::string& value);

 private:
    class Receiver {
     public:
        explicit Receiver(Nan::Callback* callback) : callback_(callback) {}
        void notify(const std::string& value);

     private:
        class Worker : public Nan::AsyncWorker {
         public:
            Worker(Nan::Callback* cb, const std::string& value) : Nan::AsyncWorker(cb), value_(value) {}
            void Execute() override {}
            void HandleOKCallback() override;

         private:
            const std::string& value_;
        };

        Nan::Callback* callback_;
    };

    class ReceiverList {
     public:
        ReceiverList() : receivers_list_(), receivers_list_lock_() {}
        void emplace_back(Nan::Callback* cb);
        void emit(const std::string& value);

     private:
        std::vector<std::shared_ptr<Receiver> > receivers_list_;
        std::shared_timed_mutex receivers_list_lock_;
    };

    std::shared_timed_mutex receivers_lock_;
    std::unordered_map<std::string, std::shared_ptr<ReceiverList> > receivers_;
};

typedef std::pair<std::string, std::string> ProgressReport;

class AsyncEventEmittingCWorker : public Nan::AsyncProgressWorkerBase<ProgressReport> {
 public:
    typedef Nan::AsyncProgressWorkerBase<ProgressReport>::ExecutionProgress ExecutionProgress;

    AsyncEventEmittingCWorker(Nan::Callback* cb, std::shared_ptr<EventEmitter> emitter)
        : Nan::AsyncProgressWorkerBase<ProgressReport>(cb), emitter_(emitter) {}

    virtual void HandleProgressCallback(const ProgressReport* data, size_t size) override;
    virtual void Execute(const ExecutionProgress& p) override;
    virtual void ExecuteWithEmitter(eventemitter_fn) = 0;

 private:
    static void emit(const char* ev, const char* val) { c_emitter_func_(ev, val); }
    std::shared_ptr<EventEmitter> emitter_;
    // XXX(jrb): This will not work if the C library is multithreaded
    static thread_local std::function<void(const char*, const char*)> c_emitter_func_;
};

}  // end namespace NodeGLPK

#endif
