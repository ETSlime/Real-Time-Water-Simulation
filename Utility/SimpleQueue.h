#pragma once
//=============================================================================
//
// お行儀よく順番を守るキューちゃん [SimpleQueue.h]
// Author : 
// FIFO（先入れ先出し）ルールでアイテムを並べてくれる、きちんとさんな自作キュークラスっ
// 循環バッファで効率よく動いて、リサイズもばっちり対応するお利口メイドタイプなの~
//
//=============================================================================

template<typename T>
class SimpleQueue 
{
private:
    T* array;
    int capacity;
    int size;
    int head;
    int tail;

public:
    SimpleQueue() : array(nullptr), capacity(0), size(0), head(0), tail(0) {}

    ~SimpleQueue() 
    {
        if (array)
            delete[] array;
    }

    void enqueue(const T& item) 
    {
        if (size >= capacity) 
        {
            int newCapacity = capacity == 0 ? 4 : capacity * 2;
            T* newArray = new T[newCapacity];

            // 配列の要素を新しい配列に再配置
            int current = head;
            for (int i = 0; i < size; i++) 
            {
                newArray[i] = array[current];
                current = (current + 1) % capacity;
            }

            delete[] array;
            array = newArray;
            capacity = newCapacity;
            head = 0;
            tail = size;
        }

        array[tail] = item;
        tail = (tail + 1) % capacity;
        size++;
    }

    void dequeue() 
    {
        if (size == 0)
            return;

        head = (head + 1) % capacity;
        size--;

        if (size == 0) 
        {
            head = 0;
            tail = 0;
        }
    }

    // 配列の先頭要素
    T& front() 
    {
        return array[head];
    }

    // 配列の最後の要素
    T& back() 
    {
        return array[(tail - 1 + capacity) % capacity];
    }

    void clear() 
    {
        delete[] array;
        array = nullptr;
        size = 0;
        capacity = 0;
        head = 0;
        tail = 0;
    }

    int getSize() const 
    {
        return size;
    }

    bool isEmpty() const 
    {
        return size == 0;
    }
};
