#include <atomic>
#include <thread>
#include <iostream>

class QueueSpinLock {
public:
    class LockGuard {
    public:
        explicit LockGuard(QueueSpinLock &spinlock) : spinlock_(spinlock) {
            AcquireLock();
        }

        ~LockGuard() {
            ReleaseLock();
        }

    private:
        void AcquireLock() {
            LockGuard *prev_tail = spinlock_.wait_queue_tail_.exchange(this);
            if (prev_tail) {
                prev_tail->next_ = this;
                while (!is_owner_) {
                    std::this_thread::yield();
                }
            }
        }

        void ReleaseLock() {
            auto ptr = this;
            if (!spinlock_.wait_queue_tail_.compare_exchange_strong(ptr, nullptr)) {
                while (!next_.load()) {
                    std::this_thread::yield();
                }
                next_.load()->is_owner_ = true;
            }
        }

    private:
        QueueSpinLock &spinlock_;

        std::atomic<bool> is_owner_{false};
        std::atomic<LockGuard *> next_{nullptr};
    };

private:
    std::atomic<LockGuard *> wait_queue_tail_{nullptr};
};

int main() {
    QueueSpinLock queueSpinLock;
    QueueSpinLock::LockGuard lockGuard(queueSpinLock);

    return 0;
}
