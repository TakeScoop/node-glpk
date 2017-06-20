#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "../src/events/shared_ringbuffer.hpp"

namespace {

using namespace std;

class RingbufferTest : public ::testing::Test {
 protected:
    RingbufferTest() {}
    virtual ~RingbufferTest() {}
    virtual void SetUp() {}
    virtual void TearDown() {}

 public:
    std::atomic<bool> write_done;
    std::atomic<bool> read_done;
};

TEST_F(RingbufferTest, EnqueueDequeueNonBlocking) {
    RingBuffer<std::string, 2> buf;

    // Enqueue 2 elements
    bool success = buf.enqueue_nonblocking("Test 1");
    EXPECT_EQ(true, success);

    success = buf.enqueue_nonblocking("Test 2");
    EXPECT_EQ(true, success);

    // Queue is full, should return false
    success = buf.enqueue_nonblocking("Test 3");
    EXPECT_EQ(false, success);

    std::string out;
    // Dequeue 2 elements
    success = buf.dequeue_nonblocking(out);
    EXPECT_EQ(true, success);
    EXPECT_EQ("Test 1", out);

    success = buf.dequeue_nonblocking(out);
    EXPECT_EQ(true, success);
    EXPECT_EQ("Test 2", out);

    // Queue is empty, should return false
    success = buf.dequeue_nonblocking(out);
    EXPECT_EQ(false, success);
}

TEST_F(RingbufferTest, WriteQueueDequeueBlocking) {
    this->write_done = false;
    std::atomic<bool> next{false};

    std::mutex lock;
    RingBuffer<std::string, 2> buf;

    std::unique_lock<std::mutex> guard{lock};
    std::condition_variable cond;
    auto writer = std::thread([this, &buf, &guard, &next, &cond]() {
        buf.enqueue("Test 1");  // doesn't block
        buf.enqueue("Test 2");  // doesn't block
        next = true;
        cond.notify_one();

        buf.enqueue("Test 3");  // blocks
        this->write_done = true;
        cond.notify_one();
    });

    auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();

    auto timer = std::thread([&cond]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        cond.notify_one();
    });
    timer.detach();

    while (!(next.load()) && (now - start < std::chrono::seconds(1))) {
        cond.wait(guard);
        now = std::chrono::steady_clock::now();
    }
    EXPECT_EQ(false, this->write_done.load());

    std::string out;
    bool success = buf.dequeue_nonblocking(out);
    EXPECT_EQ(true, success);
    EXPECT_EQ("Test 1", out);

    start = std::chrono::steady_clock::now();
    now = std::chrono::steady_clock::now();

    timer = std::thread([&cond]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        cond.notify_one();
    });
    timer.detach();

    while (!(this->write_done.load()) && (now - start < std::chrono::seconds(1))) {
        cond.wait(guard);
        now = std::chrono::steady_clock::now();
    }
    EXPECT_EQ(true, this->write_done.load());
    writer.join();
}

TEST_F(RingbufferTest, ReadQueueDequeueBlocking) {
    this->read_done = false;
    std::atomic<bool> next{false};

    std::mutex lock;
    RingBuffer<std::string, 2> buf;

    std::unique_lock<std::mutex> guard{lock};
    std::condition_variable cond;
    auto reader = std::thread([this, &buf, &cond]() {
        string v = buf.dequeue();
        EXPECT_EQ("Test 1", v);
        this->read_done = true;
        cond.notify_one();
    });

    // Make sure we give the reader time to block on something
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    buf.enqueue_nonblocking("Test 1");

    auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();

    auto timer = std::thread([&cond]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        cond.notify_one();
    });
    timer.detach();

    while (!(read_done.load()) && (now - start < std::chrono::seconds(1))) {
        cond.wait(guard);
        now = std::chrono::steady_clock::now();
    }
    EXPECT_EQ(true, this->read_done.load());
    reader.join();
}
}  // namespace

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
