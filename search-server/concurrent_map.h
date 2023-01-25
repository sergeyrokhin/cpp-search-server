#pragma once
#include <cstdlib>
#include <map>
#include <vector>
#include <mutex>
#include <string>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
private:
    struct Bucket {
        std::mutex bucket_mutex;
        std::map<Key, Value> bucket_map;
    };

public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;

        Access(const Key& key, Bucket& bucket)
            : guard(bucket.bucket_mutex)
            , ref_to_value(bucket.bucket_map[key]) {
        }
    };

    explicit ConcurrentMap(size_t bucket_count)
        : buckets_(bucket_count) {
    }

    Access operator[](const Key& key) {
        auto& bucket = buckets_[static_cast<uint64_t>(key) % buckets_.size()];
        return { key, bucket };
    }

    void Erase(const Key& key) {
        auto& bucket = buckets_[static_cast<uint64_t>(key) % buckets_.size()];
        auto lock = std::lock_guard(bucket.bucket_mutex);
        bucket.bucket_map.erase(key);
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (auto& [bucket_mutex, bucket_map] : buckets_) {
            std::lock_guard g(bucket_mutex);
            result.insert(bucket_map.begin(), bucket_map.end());
        }
        return result;
    }

private:
    std::vector<Bucket> buckets_;
};
