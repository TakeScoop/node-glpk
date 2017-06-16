#pragma once
#ifndef _GLPK_LOCKFREE_RINGBUFFER_H
#define _GLPK_LOCKFREE_RINGBUFFER_H

#include <atomic>
#include <memory>
#include <mutex>
#include <algorithm>

/// Multi-consumer, multi-producer, condition_variable signalled Shared
/// ringbuffer (contiguous memory)
template <typename T, size_t SIZE>
class RingBuffer {
    constexpr static bool divides_evenly(int v) {
        return (static_cast<size_t>(-1) % v) == 0;
    }

    static_assert(
        divides_evenly(SIZE),
        "SIZE does not divide 2^64, so behavior on overrun would be erratic");

 public:
    RingBuffer() : lock_(), notifier_(), read_idx_(0), write_idx_(0), buf_() {}

    /// Move val into the ringbuffer. If this fails, the object is still moved
    /// (and so, effectively lost at this point). If this is a problem, make a
    /// copy and move that copy
    ///
    /// @param[in] val - the value to try to push
    ///
    /// @returns true if successful, false if the buffer was full
    bool enqueue_nonblocking(T&& val) {
        std::unique_lock<std::mutex> guard{lock_};
        
        // head & tail are unsigned so this subtraction is in the group
        // \mathbb{Z}_{2^64}, and so is overrun safe
        if(write_idx_ - read_idx_ > buf_.size() - 1) {
            return false;
        }

        buf_[write_idx_ % buf_.size()] = std::move(val);
        ++write_idx_;

        guard.unlock();
        notifier_.notify_one();
    }

    void enqueue(T&& val) {
        while(!try_push_back(val));
    }

    bool deqeue_nonblocking(T& val) {
        std::unique_lock<std::mutex> guard{lock_};
        val = unlocked_dequeue();
    }

    /// Blocking attempt to dequeue an item 
    T&& dequeue() {
        std::unique_lock<std::mutex> guard{lock_};
        while(write_idx_ - read_idx_ >= read_idx_) {
            notifier_.wait(guard);
        }
        return unlocked_dequeue();
    }

 private:
    inline T&& unlocked_dequeue() {
        return std::move(buf_[read_idx_++]);
    }


    std::mutex lock_;
    std::condition_variable notifier_;
    size_t read_idx_;
    size_t write_idx_;
    std::array<T, SIZE> buf_;
};

#endif
