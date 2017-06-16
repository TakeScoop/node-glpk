#pragma once
#ifndef _GLPK_EVENTEMITTER_H
#define _GLPK_EVENTEMITTER_H

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <nan.h>
#include <node.h>
#include <node_object_wrap.h>

// In keeping with the style of this module, I'm putting all definitions in the
// header. I might look to moving definitions into a cpp, and just leaving
// declerations here.
//
// TODO(jrb):
// - make notification happen in thread (threadpool)
// - implement C bindings to pass to bound emit method to C does real work
// - Implement testing (gtest/gmock)

namespace NodeGLPK {

/// A base-class for thing that behave like EventEmitter in node
class EventEmitter {
 public:
    /// An error indicating the event name is not known
    class InvalidEvent : std::runtime_error {
     public:
        explicit InvalidEvent(const std::string& msg)
            : std::runtime_error(msg) {}
    };

    EventEmitter() : receivers_lock_(), receivers_() {}
    virtual ~EventEmitter() noexcept = default;

    /// Set a callback for a given event name
    ///
    /// @param[in] ev - event name
    /// @param[in] cb - callback
    virtual void on(const std::string& ev, std::shared_ptr<Nan::Callback> cb) {
        std::unique_lock<std::mutex> master_lock{receivers_lock_};
        auto it = receivers_.find(ev);
        if (it == receivers_.end()) {
            receivers_.emplace(ev, ReceiverList());
            it = receivers_.find(ev);
        }
        master_lock.unlock();

        it->second.emplace_back(cb);
    }

 protected:
    /// Emit a value to any registered callbacks for the event
    ///
    /// @param[in] ev - event name
    /// @param[in] value - a string to emit
    ///
    /// @returns true if the event has listeners, false otherwise
    virtual bool emit(const std::string& ev, const std::string value) {
        std::unique_lock<std::mutex> master_lock{receivers_lock_};
        auto it = receivers_.find(ev);
        if (it == receivers_.end()) {
            return false;
        }
        master_lock.unlock();

        it->second.emit(value);
        return true;
    }

    /// For places where 2 locks at ~25ns per would be costly, use this before
    /// the critical part, and then emit to only those receivers who registered
    /// prior to the calling this function without any locks. If you need to
    /// emit multiple emitter types, call this for each one
    ///
    /// @param[in] ev - The event type you want to notify
    ///
    /// @returns a function which will emit to the receivers who had registered
    ///          prior to calling this function
    virtual std::function<void(std::string)> getFastEmitter(
        const std::string& ev) {
        std::unique_lock<std::mutex> master_lock{receivers_lock_};
        auto it = receivers_.find(ev);
        if (it == receivers_.end()) {
            throw InvalidEvent(ev);
        }
        master_lock.unlock();

        auto lst = it->second.copy();
        return [lst](std::string value) {
            for (auto& receiver : lst) {
                receiver->notify(value);
            }
        };
    }

 private:
    class Receiver {
     public:
        explicit Receiver(std::shared_ptr<Nan::Callback> callback)
            : callback_(callback) {}

        void notify(std::string value) {
            ReceiverWorker* worker = new ReceiverWorker(callback_, value);
            Nan::AsyncQueueWorker(worker);
        }

     private:
        class ReceiverWorker : public Nan::AsyncWorker {
         public:
            explicit ReceiverWorker(std::shared_ptr<Nan::Callback> cb,
                                    const std::string& value)
                : Nan::AsyncWorker(cb.get()), value_(value) {}

            void Execute() {}

            void HandleOKCallback() {
                v8::Local<v8::Value> info[] = {
                    Nan::New<v8::String>(value_).ToLocalChecked()};
                callback->Call(1, info);
            }

         private:
            const std::string& value_;
        };

        std::shared_ptr<Nan::Callback> callback_;
    };

    class ReceiverList {
     public:
        ReceiverList() : receivers_list_(), receivers_list_lock_() {}

        std::unique_lock<std::mutex> guard() {
            std::unique_lock<std::mutex> lockguard{receivers_list_lock_};
            return lockguard;
        }

        void emplace_back(std::shared_ptr<Nan::Callback> cb) {
            std::lock_guard<std::mutex> guard{receivers_list_lock_};
            receivers_list_.emplace_back(std::make_shared<Receiver>(cb));
        }

        void emit(std::string value) {
            std::lock_guard<std::mutex> guard{receivers_list_lock_};
            for (auto& receiver : receivers_list_) {
                receiver->notify(value);
            }
        }

        std::vector<std::shared_ptr<Receiver> > copy() {
            std::lock_guard<std::mutex> guard{receivers_list_lock_};

            std::vector<std::shared_ptr<Receiver> > lst;
            lst.reserve(receivers_list_.size());

            std::copy(receivers_list_.begin(), receivers_list_.end(),
                      lst.begin());
            return lst;
        }

     private:
        std::vector<std::shared_ptr<Receiver> > receivers_list_;
        std::mutex receivers_list_lock_;
    };

    std::mutex receivers_lock_;
    std::unordered_map<std::string, ReceiverList> receivers_;
};

}  // end namespace NodeGLPK

#endif
