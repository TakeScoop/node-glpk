#pragma once
#ifndef _SHARED_QUEUE_H
#define _SHARED_QUEUE_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>

template <typename T>
class SharedQueue {
 public:
    SharedQueue() : abort_{false}, q_{} {}

    T* front() {
        std::unique_lock<std::mutex> guard{lock_};
        if (unlocked_wait(guard)) {
            return &(q_.front());
        } else {
            return nullptr;
        }
    }

    void pop_front() {
        std::unique_lock<std::mutex> guard{lock_};
        if (unlocked_wait(guard)) {
            q_.pop_front();
        }
        return;
    }

    void push_back(const T& v) {
        std::unique_lock<std::mutex> guard{lock_};
        q_.push_back(v);
        guard.unlock();
        cond_wait_.notify_one();
    }

    void push_back(const T&& v) {
        std::unique_lock<std::mutex> guard{lock_};
        q_.push_back(std::move(v));
        guard.unlock();
        cond_wait_.notify_one();
    }

    bool empty() {
        std::unique_lock<std::mutex> guard{lock_};
        return q_.empty();
    }

    size_t size() {
        std::unique_lock<std::mutex> guard{lock_};
        return q_.size();
    }

    void abort() {
        abort_ = true;
        cond_wait_.notify_all();
    }

 private:
    bool unlocked_wait(std::unique_lock<std::mutex>& guard) {
        while (!abort_.load() && q_.empty()) {
            cond_wait_.wait(guard);
        }
        return !q_.empty();
    }

    std::atomic<bool> abort_;
    std::deque<T> q_;
    std::mutex lock_;
    std::condition_variable cond_wait_;
};

#endif
