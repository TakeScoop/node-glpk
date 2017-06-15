#pragma once
#ifndef _GLPK_EVENTEMITTER_H
#define _GLPK_EVENTEMITTER_H

#include <vector>
#include <string>
#include <mutex>
#include <unordered_map>

#include <node.h>
#include <node_object_wrap.h>
#include <nan.h>

// In keeping with the style of this module, I'm putting all definitions in the
// header. I might look to moving definitions into a cpp, and just leaving
// declerations here.
//
// TODO(jrb): 
// - implement C bindings to pass to bound emit method to C does real work
// - Implement testing (gtest/gmock)
// - 

namespace NodeGLPK {
    class EventEmitter {
        public:
            EventEmitter() : receivers_lock_(), receivers_() {}
            virtual ~EventEmitter() noexcept = default;

            /// Set a callback for a given event name
            ///
            /// @param[in] ev - event name
            /// @param[in] cb - callback
            virtual void on(const std::string& ev, std::shared_ptr<Nan::Callback> cb) {
                std::unique_lock<std::mutex> master_lock{receivers_lock_};
                auto it = receivers_.find(ev);
                if(it == receivers_.end()) {
                    receivers_.emplace(ev, ReceiverList());
                    it = receivers_.find(ev);
                }
                master_lock.unlock();

                it->second.emplace_back(cb);
            }

            /// Emit a value to any registered callbacks for the event
            ///
            /// @param[in] ev - event name
            /// @param[in] value - a string to emit
            ///
            /// @returns true if the event has listeners, false otherwise
            virtual bool emit(const std::string& ev, const std::string value) {
                std::unique_lock<std::mutex> master_lock{receivers_lock_};
                auto it = receivers_.find(ev);
                if(it == receivers_.end()) {
                    return false;
                }
                master_lock.unlock();

                it->second.emit(value);
                return true;
            }



        private:
            class Receiver {
                public:
                    explicit Receiver(std::shared_ptr<Nan::Callback> callback) :
                        callback_(callback)
                    {}

                    void notify(std::string value) {
                        ReceiverWorker* worker = new ReceiverWorker(callback_, value);
                        Nan::AsyncQueueWorker(worker);
                    }

                private:
                    class ReceiverWorker : public Nan::AsyncWorker {
                        public:
                            explicit ReceiverWorker(std::shared_ptr<Nan::Callback> cb, const std::string& value) :
                                Nan::AsyncWorker(cb.get()),
                                value_(value)
                            {}

                            void Execute() {
                            }

                            void HandleOKCallback() {
                                v8::Local<v8::Value> info[] = {Nan::New<v8::String>(value_).ToLocalChecked()};
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
                        receivers_list_.emplace_back(std::make_unique<Receiver>(cb));
                    }

                    void emit(std::string value) {
                        std::lock_guard<std::mutex> guard{receivers_list_lock_};
                        for(auto& receiver : receivers_list_) {
                            receiver->notify(value);
                        }
                    }

                private:
                    std::vector<std::unique_ptr<Receiver> > receivers_list_;
                    std::mutex receivers_list_lock_;
            };

            std::mutex receivers_lock_;
            std::unordered_map<std::string, ReceiverList> receivers_;





    };
}

#endif

