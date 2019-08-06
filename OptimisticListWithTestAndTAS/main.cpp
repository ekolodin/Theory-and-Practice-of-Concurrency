#include <limits>
#include <mutex>
#include <thread>
#include <iostream>
#include <cassert>
#include <utility>

////////////////////////////////////////////////////////////////////////////////

class SpinLock {
public:
    void Lock() {
        while (locked_.load() || locked_.exchange(true)) {
            std::this_thread::yield();
        }
    }

    void Unlock() {
        locked_.store(false);
    }

    void lock() {
        Lock();
    }

    void unlock() {
        Unlock();
    }

private:
    std::atomic<bool> locked_{false};
};

////////////////////////////////////////////////////////////////////////////////

template<typename T>
struct KeyTraits {
    static T LowerBound() {
        return std::numeric_limits<T>::min();
    }

    static T UpperBound() {
        return std::numeric_limits<T>::max();
    }
};

struct StringKeyTraits {
    static std::string LowerBound() {
        return "";
    }

    static std::string UpperBound() {
        return std::string(256, std::numeric_limits<char>::max());
    }
};

////////////////////////////////////////////////////////////////////////////////

template<typename T, class TTraits = KeyTraits<T>>
class OptimisticLinkedSet {
private:
    struct Node {
        T key_;
        std::atomic<Node *> next_{};
        SpinLock spinlock_;
        std::atomic<bool> marked_{};

        explicit Node(T key, Node *next = nullptr) : key_(std::move(key)) {
            next_.store(next);
            marked_.store(false);
        }

        std::unique_lock<SpinLock> Lock() {
            return std::unique_lock<SpinLock> {spinlock_};
        }
    };

    struct EdgeCandidate {
        Node *pred_;
        Node *curr_;

        EdgeCandidate(Node *pred, Node *curr) : pred_(pred), curr_(curr) {
        }
    };

public:
    explicit OptimisticLinkedSet() {
        CreateEmptyList();
    }

    bool Insert(T key) {
        while (true) {
            EdgeCandidate edge = Locate(key);
            auto node_lock_pred = edge.pred_->Lock();

            if (Validate(edge)) {
                if (edge.curr_->key_ == key) {
                    return false;
                } else {
                    auto insert = new Node(key, edge.curr_);
                    edge.pred_->next_.store(insert);
                    size_.fetch_add(1);
                    return true;
                }
            }
        }
    }

    bool Remove(const T &key) {
        while (true) {
            EdgeCandidate edge = Locate(key);
            auto node_lock_pred = edge.pred_->Lock();
            auto node_lock_curr = edge.curr_->Lock();

            if (Validate(edge)) {
                if (edge.curr_->key_ != key) {
                    return false;
                } else {
                    edge.curr_->marked_.store(true);
                    edge.pred_->next_.store(edge.curr_->next_.load());
                    size_.fetch_sub(1);
                    return true;
                }
            }
        }
    }

    bool Contains(const T &key) const {
        EdgeCandidate edge = Locate(key);
        return edge.curr_->key_ == key && !edge.curr_->marked_.load();
    }


    size_t GetSize() const {
        return size_.load();
    }

private:
    void CreateEmptyList() {
        head_ = new Node(TTraits::LowerBound());
        head_->next_.store(new Node(TTraits::UpperBound()));
    }

    EdgeCandidate Locate(const T &key) const {
        auto pred = head_;
        auto curr = head_->next_.load();
        while (curr->key_ < key) {
            pred = curr;
            curr = curr->next_.load();
        }

        return EdgeCandidate(pred, curr);
    }

    bool Validate(const EdgeCandidate &edge) const {
        return !edge.pred_->marked_.load() && !edge.curr_->marked_.load() && edge.pred_->next_.load() == edge.curr_;
    }

private:
    Node *head_{nullptr};
    std::atomic<size_t> size_{0};
};

int main() {
    OptimisticLinkedSet<std::string, StringKeyTraits> set;
    for (int i = 0; i < 100001; ++i) {
        assert(set.Insert(std::to_string(i)));
        assert(set.Contains(std::to_string(i)));
        assert(set.Remove(std::to_string(i)));
        assert(!set.Contains(std::to_string(i)));
    }

    return 0;
}
