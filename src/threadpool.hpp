#pragma once
#ifndef _GLPK_THREADPOOL_HPP
#define _GLPK_THREADPOOL_HPP

#include <functional>
#include <memory>
#include <mutex>
#include <thread>

#include "sharedqueue.hpp"

namespace NodeGLPK {

class ThreadPool {
 public:
    typedef std::function<void()> Work;

    class Worker {
     public:
        void operator()() noexcept {}

     private:
        std::atomic<bool> running_;
        std::thread job_;
        SharedQueue<Work> queue_;
    };
};
}  // end namespace NodeGLPK

#endif
