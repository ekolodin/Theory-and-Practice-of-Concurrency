#include <iostream>
#include <deque>

class QueueClosed : public std::runtime_error {
public:
    QueueClosed() : std::runtime_error("Queue closed for Puts") {}
};

template<typename T, class Container = std::deque<T>>
class BlockingQueue {
public:
    explicit BlockingQueue(const size_t capacity = 0) : capacity_(capacity) {}

    void Put(T item) {
        std::unique_lock<std::mutex> lock_(mutex_);
        while (IsFull() && !closed_) {
            wait_for_place_.wait(lock_);
        }
        if (closed_) {
            throw QueueClosed();
        }

        items_.push_back(std::move(item));
        wait_for_item_.notify_one();
    }

    bool Get(T &item) {
        std::unique_lock<std::mutex> lock_(mutex_);
        if (IsEmpty() && closed_) {
            return false;
        }
        while (IsEmpty() && !closed_) {
            wait_for_item_.wait(lock_);
        }
        if (IsEmpty()) {
            return false;
        }
        item = std::move(items_.front());
        items_.pop_front();
        wait_for_place_.notify_one();
        return true;
    }

    void Close() {
        std::unique_lock<std::mutex> lock_(mutex_);
        closed_ = true;
        wait_for_item_.notify_all();
        wait_for_place_.notify_all();
    }

private:
    bool IsFull() const {
        return capacity_ == 0 ? false : (items_.size() == capacity_);
    }

    bool IsEmpty() const {
        return items_.empty();
    }

private:
    size_t capacity_;
    Container items_;
    bool closed_{false};
    std::condition_variable wait_for_place_;
    std::condition_variable wait_for_item_;
    std::mutex mutex_;
};

int main() {
    std::cout << "Hello, World!" << std::endl;
    return 0;
}