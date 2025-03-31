#ifndef HASHNODE_HPP
#define HASHNODE_HPP

template <typename K, typename V>
class HashNode
{

private:
    K key;
    V value;

public:
    std::unique_ptr<HashNode> next;
    HashNode(const K &key, const V &value) : key(key), value(value), next(nullptr) {}

    ~HashNode() = default;

    HashNode(const HashNode &) = delete;
    HashNode(HashNode &&) = delete;
    HashNode &operator=(const HashNode &) = delete;
    HashNode &operator=(HashNode &&) = delete;

    const K &getKey() const {return key;}
    const V &getValue() const { return value; }
    void setValue(const V &value) { this->value = value; }

    HashNode* getNext() const { return next.get(); }
};

#endif 
