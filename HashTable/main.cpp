#include <iostream>
#include <shared_mutex>
#include <vector>
#include <forward_list>

class ReaderWriterLock {
public:
    void lock_shared() {
        std::unique_lock<std::mutex> lock_(mutex_);
        while (writer_) {
            waiter_.wait(lock_);
        }
        ++readers_acquires_;
    }

    void unlock_shared() {
        std::unique_lock<std::mutex> lock_(mutex_);
        ++readers_releases_;
        if (readers_acquires_ == readers_releases_) {
            waiter_.notify_all();
        }
    }

    void lock() {
        std::unique_lock<std::mutex> lock_(mutex_);
        while (writer_) {
            waiter_.wait(lock_);
        }
        writer_ = true;
        while (readers_acquires_ != readers_releases_) {
            waiter_.wait(lock_);
        }
    }

    void unlock() {
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


template<typename T, class HashFunction = std::hash<T>>
class StripedHashSet {
private:
    using RWLock = ReaderWriterLock;

    using ReaderLocker = std::shared_lock<RWLock>;
    using WriterLocker = std::unique_lock<RWLock>;

    using Bucket = std::forward_list<T>;
    using Buckets = std::vector<Bucket>;

public:
    explicit StripedHashSet(const size_t concurrency_level = 4,
                            const size_t growth_factor = 2,
                            const double max_load_factor = 0.8)
            : concurrency_level_(concurrency_level),
              growth_factor_(growth_factor),
              max_load_factor_(max_load_factor),
              locks_(std::vector<RWLock>(concurrency_level)) {

        size_.store(0);
        buckets_.resize(concurrency_level);
    }

    bool Insert(T element) {
        auto hash = hashFunction_(element);
        auto stripe_lock = LockStripe<WriterLocker>(hash);
        auto expected_size = buckets_.size();

        if (std::find(GetBucket(GetBucketIndex(hash)).begin(), GetBucket(GetBucketIndex(hash)).end(), element) ==
            GetBucket(GetBucketIndex(hash)).end()) {
            GetBucket(GetBucketIndex(hash)).emplace_front(element);
            size_.fetch_add(1);
            stripe_lock.unlock();
            TryExpandTable(expected_size);
            return true;
        }
        return false;
    }

    bool Remove(const T &element) {
        auto hash = hashFunction_(element);
        auto stripe_lock = LockStripe<WriterLocker>(hash);
        auto ptr = std::find(GetBucket(GetBucketIndex(hash)).begin(), GetBucket(GetBucketIndex(hash)).end(), element);

        if (ptr != GetBucket(GetBucketIndex(hash)).end()) {
            GetBucket(GetBucketIndex(hash)).remove(element);
            size_.fetch_sub(1);
            return true;
        }
        return false;
    }

    bool Contains(const T &element) {
        auto hash = hashFunction_(element);
        auto stripe_lock = LockStripe<ReaderLocker>(hash);

        return std::find(GetBucket(GetBucketIndex(hash)).begin(), GetBucket(GetBucketIndex(hash)).end(), element) !=
               GetBucket(GetBucketIndex(hash)).end();
    }

    size_t GetSize() const {
        return size_.load();
    }

    size_t GetBucketCount() {
        auto hash = hashFunction_(1);
        auto stripe_lock = LockStripe<ReaderLocker>(hash);

        return buckets_.size();
    }

private:
    size_t GetStripeIndex(const size_t hash_value) const {
        return hash_value % concurrency_level_;
    }

    template<class Locker>
    Locker LockStripe(const size_t hash_value) {
        return Locker(locks_[GetStripeIndex(hash_value)]);
    }

    size_t GetBucketIndex(const size_t hash_value) const {
        return hash_value % buckets_.size();
    }

    Bucket &GetBucket(const size_t hash_value) {
        return buckets_[GetBucketIndex(hash_value)];
    }

    const Bucket &GetBucket(const size_t hash_value) const {
        return buckets_[GetBucketIndex(hash_value)];
    }

    bool MaxLoadFactorExceeded() const {
        return static_cast<double>(GetSize()) / buckets_.size() > max_load_factor_;
    }

    void TryExpandTable(const size_t expected_bucket_count) {
        WriterLocker writerLockers[concurrency_level_];
        writerLockers[0] = LockStripe<WriterLocker>(0);

        if (expected_bucket_count == buckets_.size()) {

            if (MaxLoadFactorExceeded()) {
                for (int i = 1; i < concurrency_level_; ++i) {
                    writerLockers[i] = LockStripe<WriterLocker>(i);
                }
                Buckets new_buckets(buckets_.size() * growth_factor_);
                for (int i = 0; i < buckets_.size(); ++i) {
                    for (auto element : GetBucket(i)) {
                        auto hash = hashFunction_(element);
                        new_buckets[hash % new_buckets.size()].emplace_front(element);
                    }
                }

                buckets_ = std::move(new_buckets);
            }
        }
    }

private:
    size_t concurrency_level_;
    size_t growth_factor_;
    double max_load_factor_;
    std::atomic<size_t> size_{};

    Buckets buckets_;
    std::vector<RWLock> locks_;
    HashFunction hashFunction_;
};

int main() {
    StripedHashSet<std::string> hashSet;
    std::cout << hashSet.Insert("Egor") << '\n';
    std::cout << hashSet.Insert("Egor") << '\n';
    std::cout << hashSet.Contains("Egor") << '\n';
    return 0;
}