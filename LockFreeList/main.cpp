#include <limits>
#include <thread>
#include <iostream>


template<typename T>
class AtomicMarkedPointer {
    using PackedPointer = uintptr_t;

public:
    struct MarkedPointer {
        MarkedPointer(T *ptr, bool marked) : ptr_(ptr), marked_(marked) {
        }

        bool operator==(const MarkedPointer &that) const {
            return (ptr_ == that.ptr_) && (marked_ == that.marked_);
        }

        T *ptr_;
        bool marked_;
    };

public:
    explicit AtomicMarkedPointer(T *ptr = nullptr) : packed_ptr_{Pack({ptr, false})} {
    }

    MarkedPointer Load() const {
        return Unpack(packed_ptr_.load());
    }

    T *LoadPointer() const {
        const auto marked_ptr = Load();
        return marked_ptr.ptr_;
    }

    void Store(MarkedPointer marked_ptr) {
        packed_ptr_.store(Pack(marked_ptr));
    }

    void Store(T *ptr) {
        Store(MarkedPointer{ptr, false});
    }

    AtomicMarkedPointer &operator=(T *ptr) {
        Store(MarkedPointer{ptr, false});
        return *this;
    }

    bool TryMark(T *ptr) {
        return CompareAndSet({ptr, false}, {ptr, true});
    }

    bool IsMarked() const {
        return packed_ptr_.load() & 1u;

        // const auto curr_ptr = Load();
        // return curr_ptr.marked_;
    }

    // note: expected passed by value!
    bool CompareAndSet(MarkedPointer expected, MarkedPointer desired) {
        auto expected_packed = Pack(expected);
        return packed_ptr_.compare_exchange_strong(expected_packed, Pack(desired));
    }

private:
    static PackedPointer Pack(MarkedPointer marked_ptr) {
        return reinterpret_cast<uintptr_t>(marked_ptr.ptr_) ^
               (marked_ptr.marked_ ? 1 : 0);
    }

    static MarkedPointer Unpack(PackedPointer packed_ptr) {
        uintptr_t marked_bit = packed_ptr & 1u;
        T *raw_ptr = reinterpret_cast<T *>(packed_ptr ^ marked_bit);
        return {raw_ptr, marked_bit != 0};
    }

private:
    std::atomic<PackedPointer> packed_ptr_;
};


template<typename T>
struct KeyTraits {
    static T LowerBound() {
        return std::numeric_limits<T>::min();
    }

    static T UpperBound() {
        return std::numeric_limits<T>::max();
    }
};

template<typename T, class TTraits = KeyTraits<T>>
class LockFreeLinkedSet {
private:
    struct Node {
        T key_;
        AtomicMarkedPointer<Node> next_;

        explicit Node(const T &key, Node *next = nullptr) : key_(key), next_(next) {
        }

        bool IsMarked() const {
            return next_.IsMarked();
        }
    };

    struct EdgeCandidate {
        Node *pred_;
        Node *curr_;

        explicit EdgeCandidate(Node *pred = nullptr, Node *curr = nullptr) : pred_(pred), curr_(curr) {
        }
    };

public:
    explicit LockFreeLinkedSet() {
        CreateEmptyList();
    }

    bool Insert(T key) {
        auto *insert = new Node(key, nullptr);
        while (true) {
            EdgeCandidate edge = Locate(key);

            if (edge.curr_->key_ == key) {
                return false;
            } else {
                insert->next_.Store(edge.curr_);
                if (edge.pred_->next_.CompareAndSet({edge.curr_, false}, {insert, false})) {
                    size_.fetch_add(1);
                    return true;
                }
            }
        }
    }

    bool Remove(const T &key) {
        while (true) {
            EdgeCandidate edge = Locate(key);
            if (edge.curr_->key_ != key) {
                return false;
            } else {
                if (edge.curr_->next_.TryMark(edge.curr_->next_.Load().ptr_)) {
                    size_.fetch_sub(1);
                    return true;
                }
            }
        }
    }

    bool Contains(const T &key) const {
        EdgeCandidate edge = Locate(key);
        return edge.curr_->key_ == key;
    }

    int GetSize() const {
        return size_.load();
    }

private:
    void CreateEmptyList() {
        // create sentinel nodes
        head_ = new Node(TTraits::LowerBound());
        head_->next_ = new Node(TTraits::UpperBound());
    }

    EdgeCandidate Locate(const T &key) const {
        while (true) {
            auto pred = head_;
            auto curr = head_->next_.Load().ptr_;
            while (curr->key_ < key) {
                if (curr->IsMarked()) {
                    pred->next_.CompareAndSet({curr, false}, {curr->next_.Load().ptr_, false});
                    curr = curr->next_.Load().ptr_;
                } else {
                    pred = curr;
                    curr = curr->next_.Load().ptr_;
                }
            }
            if (!curr->IsMarked()) {
                return EdgeCandidate(pred, curr);
            }
            pred->next_.CompareAndSet({curr, false}, {curr->next_.Load().ptr_, false});
        }
    }

private:
    Node *head_{nullptr};
    std::atomic<int> size_{0};
};


int main() {
    LockFreeLinkedSet<int> list;
    std::cout << list.Insert(1);
    std::cout << list.Contains(1);
    std::cout << list.Remove(1) << '\n';
    std::cout << list.Contains(1);
    std::cout << list.GetSize();


    return 0;
}
