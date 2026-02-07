#pragma once
#include <vector>
#include <mutex>
#include <condition_variable>

using namespace std;

// Thread-safe bounded queue using ring buffer
// T must be copyable

template<typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(size_t capacity)
        : buffer_(capacity), capacity_(capacity), head_(0), tail_(0), count_(0), closed_(false) {}

    bool push(const T& item) {
        unique_lock<mutex> lock(mtx_);
        not_full_cv_.wait(lock, [this] { return count_ < capacity_ || closed_; });
        if (closed_) return false;
        
        bool was_empty = (count_ == 0); // Optimization: Check state before update
        
        buffer_[tail_] = item;
        tail_ = (tail_ + 1) % capacity_;
        ++count_;
        
        lock.unlock(); // Unlock before notifying to reduce contention
        if (was_empty) not_empty_cv_.notify_one(); // Only notify if consumer might be sleeping
        return true;
    }

    bool pop(T& out) {
        unique_lock<mutex> lock(mtx_);
        not_empty_cv_.wait(lock, [this] { return count_ > 0 || closed_; });
        if (count_ == 0 && closed_) return false;
        
        bool was_full = (count_ == capacity_); // Optimization: Check state before update
        
        out = buffer_[head_];
        head_ = (head_ + 1) % capacity_;
        --count_;
        
        lock.unlock(); // Unlock before notifying
        if (was_full) not_full_cv_.notify_one(); // Only notify if producer might be sleeping
        return true;
    }

    void close() {
        lock_guard<mutex> lock(mtx_);
        closed_ = true;
        not_full_cv_.notify_all();
        not_empty_cv_.notify_all();
    }

    size_t size() const {
        lock_guard<mutex> lock(mtx_);
        return count_;
    }

private:
    vector<T> buffer_;
    size_t capacity_;
    size_t head_, tail_, count_;
    bool closed_;
    mutable mutex mtx_;
    condition_variable not_full_cv_, not_empty_cv_;
};
