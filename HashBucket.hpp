#ifndef HASHBUCKET_HPP
#define HASHBUCKET_HPP

#include "HashNode.hpp"
#include <shared_mutex>
#include <mutex>

template <typename K, typename V>
class HashBucket
{
private:
    std::unique_ptr<HashNode<K, V>> head;
    mutable std::shared_mutex mtx;

public:
    HashBucket() : head(nullptr) {}

    ~HashBucket() { clear(); }

    bool find(const K &key, V &value) const
    {
        std::shared_lock lock(mtx);
        for (auto* node = head.get(); node != nullptr; node = node->getNext())
        {
            if (node->getKey() == key)
            {
                value = node->getValue();
                return true;
            }
        }
        return false;
    }

    void insert(const K &key, const V &value)
    {
        std::unique_lock lock(mtx);

        for (auto* node = head.get(); node != nullptr; node = node->getNext())
        {
            if (node->getKey() == key)
            {
                node->setValue(value);
                return;
            }
        }

        auto newNode = std::make_unique<HashNode<K, V>>(key, value);
        newNode->next = std::move(head);
        head = std::move(newNode);
    }

    void erase(const K &key)
    {
        std::unique_lock lock(mtx);

        // if removing the head 
        if (head && head->getKey() == key)
        {
            head = std::move(head->next);
            return;
        }

        for (auto* node = head.get(); node != nullptr; )
        {
            auto* nextNode = node->getNext();
            if (nextNode && nextNode->getKey() == key)
            {
                node->next = std::move(nextNode->next);
                return;
            }
            node = nextNode;
        }
    }


    void clear()
    {
        std::unique_lock lock(mtx);
        head.reset();
    }
};

#endif