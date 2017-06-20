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
#include <uv.h>

#include "cemitter.h"
#include "shared_ringbuffer.hpp"

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
    /// Receiver represents a callback that will receive events that are fired
    class Receiver {
     public:
        /// @params[in] callback - the callback to fire, should take a single argument (which will be a string)
        explicit Receiver(Nan::Callback* callback) : callback_(callback) {}
        /// notify the callback by building an AsyncWorker and scheduling it via Nan::AsyncQueueWorker()
        ///
        /// @param[in] value - the string value to send to the callback
        void notify(const std::string& value);

     private:
        /// A worker is an implementation of Nan::AsyncWorker that uses the HandleOKCallback method (which is guaranteed
        /// to execute in the thread that can access v8 structures) to invoke the callback passed in with the string
        /// value passed to the constructor

        Nan::Callback* callback_;
    };

    /// ReceiverList is a list of receivers. Access to the list is proteced via shared mutex
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

// Unfortunately, the AsyncProgressWorker in NAN uses a single element which it populates and notifies the handler to
// check. If lots of things happen quickly, that element will be overwritten before the handler has a chance to notice,
// and events will be lost. This version of AsynProgressWorker provides a ringbuffer (to avoid
// reallocations and poor locality of reference)
template <class T, size_t SIZE>
class AsyncQueuedProgressWorker : public Nan::AsyncWorker {
 public:
    class ExecutionProgressSender {
     public:
        void Send(const T* data, size_t count) const { worker_.SendProgress(data, count); }

     private:
        friend void AsyncQueuedProgressWorker::Execute();
        explicit ExecutionProgressSender(AsyncQueuedProgressWorker& worker) : worker_(worker) {}
        ExecutionProgressSender() = delete;
        AsyncQueuedProgressWorker& worker_;
    };

    explicit AsyncQueuedProgressWorker(Nan::Callback* callback) : AsyncWorker(callback), buffer_() {
        async_ = new uv_async_t();
        uv_async_init(uv_default_loop(), async_, asyncNotifyProgressQueue);
        async_->data = this;
    }

    void HandleProgressQueue() {
        std::pair<const T*, size_t> elem;
        while (this->buffer_.dequeue_nonblocking(elem)) {
            if (callback) {
                HandleProgressCallback(elem.first, elem.second);
            } else {
                // XXX(jrb) using delete[] because AsyncProgressWorkerBase does the same, and so the presumption is
                // apparently that all data will be an array
                delete[] elem.first;
            }
        }
    }
    virtual void Destroy() override {
        // NOTABUG: this is how Nan does it...
        uv_close(reinterpret_cast<uv_handle_t*>(async_), AsyncClose);
    }

    virtual void Execute(const ExecutionProgressSender& progress) = 0;
    virtual void HandleProgressCallback(const T* data, size_t size) = 0;

 private:
    void Execute() final override {
        ExecutionProgressSender sender{*this};
        Execute(sender);
    }

    void SendProgress(const T* data, size_t size) {
        // use non_blocking and just drop any excessive items
        buffer_.enqueue_nonblocking({data, size});
    }

    // This now runs this instances HandleProgressQueue() in the v8-safe thread asynchronously
    static NAUV_WORK_CB(asyncNotifyProgressQueue) {
        auto worker = static_cast<AsyncQueuedProgressWorker*>(async->data);
        worker->HandleProgressQueue();
    }

    static void AsyncClose(uv_handle_t* handle) {
        auto worker = static_cast<AsyncQueuedProgressWorker*>(handle->data);
        // TODO(jrb): see note for async_
        // NOTABUG: this is how Nan does it...
        delete reinterpret_cast<uv_async_t*>(handle);
        delete worker;
    }

    RingBuffer<std::pair<const T*, size_t>, SIZE> buffer_;
    // TODO(jrb): std::unique_ptr<uv_async_t> async_;
    // unsure of the lifetime of the object; in several places it looks like leaks of workers would be made far worse
    // due to leaking the underlying handle too; so probably best to delete the handle via Destroy and then go around
    // and try to find all the leaks of the Worker's themselves and delete (or smart-pointer them) them so their DTOR's
    // do get called
    uv_async_t* async_;
};

template <size_t SIZE>
class AsyncEventEmittingCWorker : public AsyncQueuedProgressWorker<ProgressReport, SIZE> {
 public:
    AsyncEventEmittingCWorker(Nan::Callback* cb, std::shared_ptr<EventEmitter> emitter)
        : AsyncQueuedProgressWorker<ProgressReport, SIZE>(cb), emitter_(emitter) {}

    virtual void ExecuteWithEmitter(eventemitter_fn) = 0;

    virtual void HandleProgressCallback(const ProgressReport* report, size_t size) override {
        emitter_->emit(report[0].first, report[0].second);
        if (size > 0) {
            delete[] report;
        }
    }

    virtual void Execute(const typename AsyncQueuedProgressWorker<ProgressReport, SIZE>::ExecutionProgressSender&
                             sender) final override {
        // XXX(jrb): This will not work if the C library is multithreaded, as the c_emitter_func_ will be uninitialized
        // in any threads other than the one we're running in right now
        c_emitter_func_ = [&sender](const char* ev, const char* val) {
            // base class uses delete[], so we have to make sure we use new[]
            auto reports = new ProgressReport[1];
            reports[0] = {ev, val};

            sender.Send(reports, 1);
        };
        ExecuteWithEmitter(this->emit);
    }

 private:
    static void emit(const char* ev, const char* val) { c_emitter_func_(ev, val); }
    std::shared_ptr<EventEmitter> emitter_;
    // XXX(jrb): This will not work if the C library is multithreaded
    static thread_local std::function<void(const char*, const char*)> c_emitter_func_;
};

}  // end namespace NodeGLPK

#endif
