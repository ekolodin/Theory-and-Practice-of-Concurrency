#include <utility>
#include <atomic>

template<typename T>
class LockFreeStack {
    struct Node {
        T item_;
        std::atomic<Node *> next_{nullptr};

        explicit Node(T item) : item_(std::move(item)) {
        }
    };

public:
    LockFreeStack() = default;

    ~LockFreeStack() {
        Helper(garbage_);
        Helper(top_);
    }

    void Push(T item) {
        auto new_top = new Node(item);
        Node *curr_top = top_;
        new_top->next_ = curr_top;

        while (!top_.compare_exchange_strong(curr_top, new_top)) {
            new_top->next_ = curr_top;
        }
    }

    bool Pop(T &item) {
        Node *curr_top = top_;

        while (true) {
            if (!curr_top) {
                return false;
            }

            if (top_.compare_exchange_strong(curr_top, curr_top->next_)) {
                item = curr_top->item_;
                Push_garbage(curr_top);
                return true;
            }
        }
    }

private:
    void Push_garbage(Node *node) {
        Node *curr_top = garbage_;
        node->next_ = curr_top;

        while (!garbage_.compare_exchange_strong(curr_top, node)) {
            node->next_ = curr_top;
        }
    }

    void Helper(std::atomic<Node *> &ptr) {
        while (true) {
            Node *curr_top = ptr;
            if (!curr_top) {
                break;
            }
            ptr.store(ptr.load()->next_);
            delete curr_top;
        }
    }

private:
    std::atomic<Node *> top_{nullptr};
    std::atomic<Node *> garbage_{nullptr};
};

int main() {

    return 0;
}