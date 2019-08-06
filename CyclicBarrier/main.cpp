#include <iostream>
#include <thread>


class CyclicBarrier {
public:
    explicit CyclicBarrier(const size_t num_threads) : num_threads_(num_threads), threads_waiting_(num_threads) {
    }

    void PassThrough() {
        std::unique_lock<std::mutex> lock_(mutex_);
        wait_till_others_go_.wait(lock_, [this] { return threads_go_ == 0 || threads_go_ == num_threads_; });

        --threads_waiting_;
        if (threads_waiting_ > 0) {
            waiter_.wait(lock_, [this] { return threads_waiting_ == num_threads_; });
            ++threads_go_;
            if (threads_go_ == num_threads_) {
                wait_till_others_go_.notify_all();
            }
        } else {
            threads_go_ = 1;
            threads_waiting_ = num_threads_;
            waiter_.notify_all();
        }
    }

private:
    size_t num_threads_;
    size_t threads_waiting_;
    size_t threads_go_{0};
    std::condition_variable waiter_;
    std::condition_variable wait_till_others_go_;
    std::mutex mutex_;
};


int main() {

    return 0;
}