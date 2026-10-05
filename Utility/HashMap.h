#pragma once
//=============================================================================
//
// キーと値をすばやく整理する図書館司書ちゃん [HashMap.h]
// Author : 
// ハッシュ関数と等価演算子を使って、データをすばやく検索・追加・削除できる自作マップクラスちゃんっ！
// 連想配列としての使いやすさとカスタマイズ性を両立した、かしこかわいい構造なの!
//
//=============================================================================
#include "main.h"

struct CharPtrHash
{
    unsigned int operator()(const char* str) const
    {
        unsigned long hash = 5381; // djb2アルゴリズムの初期値
        int c;
        if (str == nullptr)
            return -1;
        // 文字列を一文字ずつ処理
        while ((c = *str++))
        {
            hash = ((hash << 5) + hash) + c; // hash * 33 + c
        }

        return hash;
    }
};

struct CharPtrEquals
{
    bool operator()(const char* a, const char* b) const
    {
        return strcmp(a, b) == 0;
    }
};

struct HashUInt64
{
    size_t operator()(uint64_t key) const
    {
        return key ^ (key >> 33);
    }
};


struct EqualUInt64
{
    bool operator()(uint64_t a, uint64_t b) const
    {
        return a == b;
    }
};

struct HashUInt32
{
    size_t operator()(uint32_t key) const
    {
        return static_cast<size_t>(key ^ (key >> 16));
    }
};

struct EqualUInt32
{
    bool operator()(uint32_t a, uint32_t b) const
    {
        return a == b;
    }
};

struct Triangle; // 前方宣言
// const Triangle* 専用ハッシュ関数
struct TrianglePtrHash
{
    unsigned int operator()(const Triangle* ptr) const
    {
        if (ptr == nullptr)
            return 0xFFFFFFFFu; // 特殊値

        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);

        // Knuth multiplicative hash
        return static_cast<unsigned int>((addr >> 4) ^ (addr * 2654435761u));
    }
};

// const Triangle* 専用等価比較関数
struct TrianglePtrEquals
{
    bool operator()(const Triangle* a, const Triangle* b) const
    {
        return a == b; // ポインタアドレスが同じなら等しい
    }
};

template<typename Key, typename Value, typename HashFunc, typename EqualsFunc>
class HashMap
{
private:

    struct Data
    {
        Key key;
        Value value;

        Data(const Key& key, const Value& value) : key(key), value(value) {};
    };

    struct Node 
    {
        Data data;
        Node* next;
        Node(const Key& key, const Value& value) : data(key, value), next(nullptr) {}
    };

    Node** buckets;
    size_t size;
    size_t m_count = 0;

    HashFunc hashFunc;
    EqualsFunc equalsFunc;

public:

    HashMap()
        : size(32), hashFunc(HashFunc{}), equalsFunc(EqualsFunc{})
    {
        buckets = new Node * [size];
        for (size_t i = 0; i < size; ++i)
        {
            buckets[i] = nullptr;
        }
    }

    HashMap(size_t size, HashFunc hashFunc, EqualsFunc equalsFunc)
        : size(size), hashFunc(hashFunc), equalsFunc(equalsFunc)
    {
        buckets = new Node * [size];
        for (size_t i = 0; i < size; ++i)
        {
            buckets[i] = nullptr;
        }
    }

    // 移動コンストラクタ
    HashMap(HashMap&& other) noexcept
        : buckets(other.buckets),
        size(other.size),
        hashFunc(std::move(other.hashFunc)),
        equalsFunc(std::move(other.equalsFunc))
    {
        other.buckets = nullptr;
        other.size = 0;
    }

    // 移動代入演算子
    HashMap& operator=(HashMap&& other) noexcept
    {
        if (this != &other)
        {
            // 既存データの破棄
            this->clear();
            delete[] this->buckets;

            // リソースの移動
            this->buckets = other.buckets;
            this->size = other.size;
            this->hashFunc = std::move(other.hashFunc);
            this->equalsFunc = std::move(other.equalsFunc);

            // other を初期状態に戻す
            other.buckets = nullptr;
            other.size = 0;
        }
        return *this;
    }


    ~HashMap()
    {
        for (size_t i = 0; i < size; ++i)
        {
            Node* current = buckets[i];
            while (current != nullptr) 
            {
                Node* toDelete = current;
                current = current->next;
                delete toDelete;
            }
        }
        delete[] buckets;
    }

    HashMap& operator=(const HashMap& other)
    {
        if (this == &other) return *this;

        this->clear();
        delete[] this->buckets;

        this->size = other.size;
        this->buckets = new Node * [this->size] {};
        for (size_t i = 0; i < this->size; ++i)
        {
            Node* current = other.buckets[i];
            Node** ptr = &this->buckets[i];
            while (current != nullptr)
            {
                Node* newNode = new Node(current->data.key, current->data.value);
                *ptr = newNode;
                ptr = &newNode->next;
                current = current->next;
            }
        }
        this->hashFunc = other.hashFunc;
        this->equalsFunc = other.equalsFunc;

        return *this;
    }


    void insert(const Key& key, const Value& value)
    {
        size_t index = hashFunc(key) % size;
        Node* current = buckets[index];
        while (current != nullptr)
        {
            if (equalsFunc(current->data.key, key))
            {
                current->data.value = value;
                return;
            }
            current = current->next;
        }

        Node* newNode = new Node(key, value);
        newNode->next = buckets[index];
        buckets[index] = newNode;

        ++m_count;
    }

    Value* search(const Key& key) const
    {
        size_t index = hashFunc(key) % size;
        Node* current = buckets[index];
        while (current != nullptr)
        {
            if (equalsFunc(current->data.key, key))
            {
                return &current->data.value;
            }
            current = current->next;
        }
        return nullptr;
    }

    Value& operator[](const Key& key)
    {
        size_t index = hashFunc(key) % size;
        Node* current = buckets[index];

        // Search for the key in the linked list
        while (current != nullptr)
        {
            if (equalsFunc(current->data.key, key))
            {
                return current->data.value;
            }
            current = current->next;
        }

        // Key not found, create new node with default value
        insert(key, Value{});
        return buckets[index]->data.value; // Return the newly inserted node's value
    }

    int count(const Key& key) const
    {
        size_t index = hashFunc(key) % size;
        Node* current = buckets[index];
        while (current != nullptr)
        {
            if (equalsFunc(current->data.key, key))
            {
                return 1;
            }
            current = current->next;
        }
        return 0;
    }

    size_t getSize() const
    {
        return m_count;
    }

    size_t bucketCount() const
    {
        return size;
    }

    float load_factor() const
    {
        if (size == 0) return 0.0f;
        return static_cast<float>(m_count) / static_cast<float>(size);
    }

    int used_bucket_count() const
    {
        int count = 0;
        for (int i = 0; i < size; ++i)
        {
            if (buckets[i] != nullptr)
                ++count;
        }
        return count;
    }

    int max_chain_length() const
    {
        int maxLen = 0;
        for (int i = 0; i < size; ++i)
        {
            int len = 0;
            Node* current = buckets[i];
            while (current)
            {
                ++len;
                current = current->next;
            }
            if (len > maxLen)
                maxLen = len;
        }
        return maxLen;
    }

    int collision_count() const
    {
        return static_cast<int>(m_count) - used_bucket_count();
    }

    float average_chain_length() const
    {
        int totalUsed = used_bucket_count();
        if (totalUsed == 0) return 0.0f;
        return static_cast<float>(m_count) / totalUsed;
    }


    bool empty() const
    {
        for (size_t i = 0; i < size; ++i)
        {
            if (buckets[i] != nullptr)
                return false;
        }
        return true;
    }

    bool contains(const Key& key) const 
    { 
        return count(key) != 0; 
    }

    bool remove(const Key& key)
    {
        unsigned int index = hashFunc(key) % size;
        Node* current = buckets[index];
        Node* previous = nullptr;

        while (current != nullptr)
        {
            if (equalsFunc(current->data.key, key))
            {
                if (previous == nullptr) 
                {
                    buckets[index] = current->next;
                }
                else {
                    previous->next = current->next;
                }
                delete current;
                --m_count;
                return true;
            }
            previous = current;
            current = current->next;
        }
        return false;
    }

    void clear()
    {
        for (size_t i = 0; i < size; ++i)
        {
            Node* current = buckets[i];
            while (current != nullptr)
            {
                Node* toDelete = current;
                current = current->next;
                delete toDelete;
            }
            buckets[i] = nullptr; // Reset the bucket pointer after clearing
        }
        m_count = 0;
    }

    Value& at(const Key& key)
    {
        unsigned int index = hashFunc(key) % size;
        Node* current = buckets[index];
        while (current != nullptr)
        {
            if (equalsFunc(current->data.key, key))
            {
                return current->data.value;
            }
            current = current->next;
        }
        throw std::out_of_range("Key not found in HashMap");
    }

    const Value& at(const Key& key) const
    {
        size_t index = hashFunc(key) % size;
        Node* current = buckets[index];
        while (current != nullptr)
        {
            if (equalsFunc(current->data.key, key))
            {
                return current->data.value;
            }
            current = current->next;
        }
        throw std::out_of_range("Key not found in HashMap");
    }


    class Iterator
    {
        friend class HashMap;

    private:
        Node** buckets;
        Node* current;
        size_t index;
        size_t size;

    public:
        Iterator(Node** buckets, size_t size, size_t index = 0, Node* node = nullptr)
            : buckets(buckets), size(size), index(index), current(node)
        {
            if (current == nullptr)
            {
                // Advance to the first valid node
                for (; this->index < size; ++this->index)
                {
                    if (buckets[this->index])
                    {
                        current = buckets[this->index];
                        break;
                    }
                }
            }
        }

        bool operator!=(const Iterator& other) const
        {
            return current != other.current;
        }

        bool operator==(const Iterator& other) const
        {
            return current == other.current;
        }

        Iterator& operator++()
        {
            if (current && current->next)
            {
                current = current->next;
            }
            else
            {
                do {
                    ++index;
                    if (index >= size) {
                        current = nullptr;
                        break;
                    }
                    current = buckets[index];
                } while (current == nullptr);
            }
            return *this;
        }

        Iterator operator++(int)
        {
            Iterator temp = *this;
            ++(*this);
            return temp;
        }


        Data& operator*() const
        {
            return current->data;
        }

        Data* operator->() const
        {
            return &current->data;
        }
    };

    Iterator begin() const
    {
        return Iterator(buckets, size);
    }

    Iterator end() const
    {
        return Iterator(buckets, size, size);
    }

    Iterator find(const Key& key) const
    {
        size_t index = hashFunc(key) % size;
        Node* current = buckets[index];
        while (current != nullptr)
        {
            if (equalsFunc(current->data.key, key))
            {
                return Iterator(buckets, size, index, current);
            }
            current = current->next;
        }
        return end();
    }

    Iterator erase(const Iterator& it)
    {
        // 無効なイテレーターだったら end() を返して即終了
        if (it.current == nullptr) return end();

        Key key = it->key;

        // キーからハッシュ値を計算して対応するバケットインデックスを取得する
        unsigned int index = hashFunc(key) % size;

        // remove() を使ってノードを削除
        remove(key);

        // バケットの中を探索して、削除されたキーとは違う最初のノードを探す
        Node* nextNode = buckets[index];
        while (nextNode != nullptr)
        {
            if (!equalsFunc(nextNode->data.key, key))
            {
                // 次の有効なノードを持つイテレーターを返す
                return Iterator(buckets, size, index, nextNode);
            }
            nextNode = nextNode->next;
        }

        // 現在のバケットに次がなければ、次のバケットを探す
        for (size_t i = index + 1; i < size; ++i)
        {
            if (buckets[i] != nullptr)
            {
                // 最初に見つけた非空バケットの先頭ノードから再開
                return Iterator(buckets, size, i, buckets[i]);
            }
        }

        // すべて見終わったら end() を返す
        return end();
    }
};