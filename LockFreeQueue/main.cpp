#include <utility>
#include <atomic>
#include <iostream>


template<typename T>
class LockFreeQueue {
    struct Node {
        T item_{};
        std::atomic<Node *> next_{nullptr};

        Node() = default;

        explicit Node(T item, Node *next = nullptr)
                : item_(std::move(item)), next_(next) {
        }
    };

public:
    LockFreeQueue() {
        auto *dummy = new Node{};
        head_ = dummy;
        tail_ = dummy;
        garbage_ = dummy;
    }

    ~LockFreeQueue() {
        while (true) {
            if (!garbage_.load()) {
                break;
            }
            auto ptr = garbage_.load();
            garbage_.store(garbage_.load()->next_);
            delete ptr;
        }
    }

    void Enqueue(T item) {
        counter_.fetch_add(1);
        Cleaner(head_);
        auto new_tail = new Node(item);
        Node *curr_tail = nullptr;

        while (true) {
            curr_tail = tail_;
            if (!curr_tail->next_) {
                Node *temp_ptr = nullptr;
                if (curr_tail->next_.compare_exchange_weak(temp_ptr, new_tail)) {
                    break;
                }
            } else {
                tail_.compare_exchange_weak(curr_tail, curr_tail->next_);
            }
        }

        tail_.compare_exchange_strong(curr_tail, new_tail);
        counter_.fetch_sub(1);
    }

    bool Dequeue(T &item) {
        counter_.fetch_add(1);
        Cleaner(head_);

        while (true) {
            Node *curr_head = head_;
            Node *curr_tail = tail_;

            if (curr_head == curr_tail) {
                if (!curr_head->next_) {
                    counter_.fetch_sub(1);
                    return false;
                } else {
                    tail_.compare_exchange_weak(curr_head, curr_head->next_);
                }
            } else {
                if (head_.compare_exchange_weak(curr_head, curr_head->next_)) {
                    item = curr_head->next_.load()->item_;
                    counter_.fetch_sub(1);
                    return true;
                }
            }
        }
    }

private:
    void Cleaner(Node *curr_head) {
        if (counter_.load() == 1) {
            while (true) {
                if (garbage_.load() == curr_head) {
                    return;
                }
                auto ptr = garbage_.load();
                garbage_.store(garbage_.load()->next_);
                delete ptr;
            }
        }
    }

private:
    std::atomic<Node *> garbage_{nullptr};
    std::atomic<Node *> head_{nullptr};
    std::atomic<Node *> tail_{nullptr};

    std::atomic<int> counter_{0};
};

int main() {
    LockFreeQueue<int> lockFreeQueue;
    for (int i = 0; i < 10; ++i) {
        lockFreeQueue.Enqueue(i);
    }

    for (int i = 0; i < 10; ++i) {
        int a = -1;
        lockFreeQueue.Dequeue(a);
        std::cout << a << ' ';
    }

    return 0;
}
