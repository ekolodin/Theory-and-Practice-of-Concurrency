#include <iostream>
#include <mutex>

class ReaderWriterLock {
public:
    void ReaderLock() {
        std::unique_lock<std::mutex> lock_(mutex_);
        while (writer_) {
            waiter_.wait(lock_);
        }
        ++readers_acquires_;
    }

    void ReaderUnlock() {
        std::unique_lock<std::mutex> lock_(mutex_);
        ++readers_releases_;
        if (readers_acquires_ == readers_releases_) {
            waiter_.notify_all();
        }
    }

    void WriterLock() {
        std::unique_lock<std::mutex> lock_(mutex_);
        while (writer_) {
            waiter_.wait(lock_);
        }
        writer_ = true;
        while (readers_acquires_ != readers_releases_) {
            waiter_.wait(lock_);
        }
    }

    void WriterUnlock() {
        std::unique_lock<std::mutex> lock_(mutex_);
        writer_ = false;
        waiter_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable waiter_;
    int readers_acquires_{0};
    int readers_releases_{0};
    bool writer_{false};
};

int main() {
    std::cout << "Hello, World!" << std::endl;
    return 0;
}