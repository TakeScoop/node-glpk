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
// Class EventEmitter::Receiver::Worker
void EventEmitter::Receiver::Worker::HandleOKCallback() {
    v8::Local<v8::Value> info[] = {Nan::New<v8::String>(value_).ToLocalChecked()};
    callback->Call(1, info);
}

/*******************************************************************************/
// Class EventEmitter::Receiver
void EventEmitter::Receiver::notify(const std::string& value) {
    Worker* worker = new Worker(callback_, value);
    Nan::AsyncQueueWorker(worker);
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

/*******************************************************************************/
// Class AsyncEventEmittingWorker
void AsyncEventEmittingCWorker::HandleProgressCallback(const ProgressReport* report, size_t) {
    emitter_->emit(report->first, report->second);
    delete report;
}

void AsyncEventEmittingCWorker::Execute(const ExecutionProgress& p) {
    // XXX(jrb): This will not work if the C library is multithreaded, as the c_emitter_func_ will be uninitialized in
    // any threads other than the one we're running in right now
    c_emitter_func_ = [&p](const char* ev, const char* val) { p.Send(new ProgressReport{ev, val}, 0); };
    ExecuteWithEmitter(this->emit);
}

}  // namespace NodeGLPK
