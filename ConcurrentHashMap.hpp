#ifndef HASH_MAP_HPP
#define HASH_MAP_HPP

#include "HashBucket.hpp"
#include <vector>
#include <cstdint>
#include <functional>
#include <mutex>

constexpr size_t HASH_SIZE_DEFAULT = 2027; // prime number for better distribution

template <typename K, typename V, typename F = std::hash<K>>
class ConcurrentHashMap
{
private:
    std::vector<HashBucket<K, V>> buckets;
    F hashFn;
    const size_t hashSize;

    size_t computeHash(const K &key) const
    {
        return hashFn(key) % hashSize;
    }

public:
    ConcurrentHashMap(size_t hashSize_ = HASH_SIZE_DEFAULT) : buckets(hashSize_), hashSize(hashSize_) {}

    ~ConcurrentHashMap() = default;

    ConcurrentHashMap(const ConcurrentHashMap &) = delete;
    ConcurrentHashMap(ConcurrentHashMap &&) = delete;
    ConcurrentHashMap &operator=(const ConcurrentHashMap &) = delete;
    ConcurrentHashMap &operator=(ConcurrentHashMap &&) = delete;

    bool find(const K &key, V &value) const
    {
        return buckets[computeHash(key)].find(key, value);
    }

    // If key already exists, update the value, else do nothing 
    void insert(const K &key, const V &value)
    {
        buckets[computeHash(key)].insert(key, value);
    }

    void erase(const K &key)
    {
        buckets[computeHash(key)].erase(key);
    }

    void clear()
    {
        for (auto &bucket : buckets)
        {
            bucket.clear();
        }
    }
};

#endif