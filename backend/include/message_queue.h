#pragma once

#include "common.h"
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <chrono>

#ifdef HAS_BOOST_LOCKFREE
#include <boost/lockfree/queue.hpp>
#endif

template <typename T>
class MessageQueue {
public:
    explicit MessageQueue(size_t capacity = 10000)
        : capacity_(capacity), running_(true) {}

    bool push(const T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.size() >= capacity_) {
            queue_.pop_front();
        }
        queue_.push_back(item);
        lock.unlock();
        cv_.notify_one();
        return true;
    }

    bool push(T&& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.size() >= capacity_) {
            queue_.pop_front();
        }
        queue_.push_back(std::move(item));
        lock.unlock();
        cv_.notify_one();
        return true;
    }

    bool try_pop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        item = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    bool wait_pop(T& item, int timeout_ms = 1000) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                        [this] { return !queue_.empty() || !running_; })) {
            return false;
        }
        if (queue_.empty()) return false;
        item = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    void stop() {
        running_ = false;
        cv_.notify_all();
    }

    bool is_running() const {
        return running_;
    }

private:
    size_t capacity_;
    std::atomic<bool> running_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<T> queue_;
};

struct QueuedSensorData {
    SensorData data;
    bool processed_by_ballistic{false};
    bool processed_by_accuracy{false};
    bool processed_by_alarm{false};
};

using SensorQueue = MessageQueue<SensorData>;
using AlertQueue = MessageQueue<Alert>;
using AccuracyQueue = MessageQueue<AccuracyAnalysis>;
