#include "eventemitter.hpp"

namespace NodeGLPK {

/*******************************************************************************/
// Class EventEmitter
void EventEmitter::on(const std::string& ev, Nan::Callback* cb) {
    std::unique_lock<std::shared_timed_mutex> master_lock{receivers_lock_};
    auto it = receivers_.find(ev);

    if (it == receivers_.end()) {
        receivers_.emplace(ev, std::make_shared<ReceiverList>());
        it = receivers_.find(ev);
    }
    master_lock.unlock();

    it->second->emplace_back(cb);
}

bool EventEmitter::emit(const std::string& ev, const std::string& value) {
    std::shared_lock<std::shared_timed_mutex> master_lock{receivers_lock_};
    auto it = receivers_.find(ev);
    if (it == receivers_.end()) {
        return false;
    }
    master_lock.unlock();

    it->second->emit(value);
    return true;
}

/*******************************************************************************/
// Class EventEmitter::Receiver
void EventEmitter::Receiver::notify(const std::string& value) {
    v8::Local<v8::Value> info[] = {Nan::New<v8::String>(value).ToLocalChecked()};
    callback_->Call(1, info);
}

/*******************************************************************************/
// Class EventEmitter::ReceiverList
void EventEmitter::ReceiverList::emplace_back(Nan::Callback* cb) {
    std::lock_guard<std::shared_timed_mutex> guard{receivers_list_lock_};
    receivers_list_.emplace_back(std::make_shared<Receiver>(cb));
}

void EventEmitter::ReceiverList::emit(const std::string& value) {
    std::lock_guard<std::shared_timed_mutex> guard{receivers_list_lock_};
    for (auto& receiver : receivers_list_) {
        receiver->notify(value);
    }
}

}  // namespace NodeGLPK
