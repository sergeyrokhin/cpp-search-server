#pragma once
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "log_duration.h"
#include "test_framework.h"

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
private:
    struct Bucket {
        mutex b_mut;
        map<Key, Value> b_map;
    };

public:
    static_assert(is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        lock_guard<mutex> guard;
        Value& ref_to_value;

        Access(const Key& key, Bucket& bucket)
            : guard(bucket.b_mut)
            , ref_to_value(bucket.b_map[key]) {
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
        auto lock = lock_guard(bucket.b_mut);
        bucket.b_map.erase(key);
    }

    map<Key, Value> BuildOrdinaryMap() {
        map<Key, Value> result;
        for (auto& [b_mut, b_map] : buckets_) {
            lock_guard g(b_mut);
            result.insert(b_map.begin(), b_map.end());
        }
        return result;
    }

private:
    std::vector<Bucket> buckets_;
};
