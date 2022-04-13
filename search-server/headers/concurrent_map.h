#include <algorithm>
#include <cstdlib>
#include <future>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <mutex>

#include "log_duration.h"

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");
    struct Bucket
    {
        std::mutex mut_;
        std::map<Key, Value> map_;
    };
    
    struct Access {
        std::lock_guard<std::mutex> lg; 
        Value& ref_to_value;
        Access(Bucket& bucket, const Key& key)
            : lg(bucket.mut_), ref_to_value(bucket.map_[key])
        {
            
        }
    };

    explicit ConcurrentMap(size_t bucket_count)
        : buckets_(bucket_count)
    {
    }

    Access operator[](const Key& key)
    {
        size_t ind = static_cast<size_t>(key) % buckets_.size();
        return { buckets_[ind], key };
    }

    std::map<Key, Value> BuildOrdinaryMap()
    {
        std::map<Key, Value> res;
        for (auto& bucket : buckets_)
        {
            std::lock_guard lg(bucket.mut_);
            res.insert(bucket.map_.begin(), bucket.map_.end());
        } 
        return res;
    }

private:
    std::vector<Bucket> buckets_;
};
