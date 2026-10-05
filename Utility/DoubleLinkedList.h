#pragma once
//=============================================================================
//
// おりこうさんな手作りダブルリンクリストちゃん [DoubleLinkedList.h]
// Author : 
// ノードを前後に行ったり来たりしながら、自由自在に追加・削除ができちゃう可変リストちゃんですっ！
// イテレータ対応で range-based for にも対応してる、お利口さんなデータ構造だよ
//
//=============================================================================

template<typename T>
class Node 
{
public:
    T data;
    Node* prev;
    Node* next;

    Node(T val) : data(val), prev(nullptr), next(nullptr) {}
};

template<typename T>
class DoubleLinkedList 
{

private:
    Node<T>* head;
    Node<T>* tail;
    int size;

public:
    DoubleLinkedList() : head(nullptr), tail(nullptr), size(0) {}

    ~DoubleLinkedList() 
    {
        clear();
    }

    DoubleLinkedList(const DoubleLinkedList&) = delete;
    DoubleLinkedList& operator=(const DoubleLinkedList&) = delete;

    Node<T>* getHead() const 
    {
        return head;
    }

    Node<T>* getBack() const
    {
        return tail;
    }

    void push_back(const T& item) 
    {
        Node<T>* newNode = new Node<T>(item);
        if (tail == nullptr) 
        {
            head = tail = newNode;
        }
        else {
            tail->next = newNode;
            newNode->prev = tail;
            tail = newNode;
        }
        size++;
    }

    void push_front(const T& item) 
    {
        Node<T>* newNode = new Node<T>(item);
        if (head == nullptr) 
        {
            head = tail = newNode;
        }
        else {
            newNode->next = head;
            head->prev = newNode;
            head = newNode;
        }
        size++;
    }

    void pop_back() 
    {
        if (tail == nullptr) return;
        Node<T>* temp = tail;
        tail = tail->prev;
        if (tail == nullptr) 
        {
            head = nullptr;
        }
        else 
        {
            tail->next = nullptr;
        }
        delete temp;
        size--;
    }

    void pop_front() 
    {
        if (head == nullptr) return;
        Node<T>* temp = head;
        head = head->next;
        if (head == nullptr) 
        {
            tail = nullptr;
        }
        else 
        {
            head->prev = nullptr;
        }
        delete temp;
        size--;
    }

    void remove(Node<T>* node) 
    {
        if (node == nullptr) return;

        if (node == head) {
            pop_front();
        }
        else if (node == tail) {
            pop_back();
        }
        else 
        {
            if (node->prev) 
            {
                node->prev->next = node->next;
            }
            if (node->next) 
            {
                node->next->prev = node->prev;
            }
            delete node;
            size--;
        }
    }

    void remove(const T* ptrToData)
    {
        Node<T>* current = head;
        while (current != nullptr)
        {
            if (&(current->data) == ptrToData)
            {
                remove(current);
                return;
            }
            current = current->next;
        }
    }

    void clear() 
    {
        Node<T>* current = head;
        while (current != nullptr) 
        {
            Node<T>* next = current->next;
            delete current;
            current = next;
        }
        head = tail = nullptr;
        size = 0;
    }

    int getSize() const 
    {
        return size;
    }

    class Iterator
    {
    private:
        Node<T>* current;
    public:
        Iterator(Node<T>* node) : current(node) {}

        T& operator*() const { return current->data; }
        T* operator->() const { return &(current->data); }

        Iterator& operator++()
        {
            if (current) current = current->next;
            return *this;
        }

        Iterator operator++(int)
        {
            Iterator temp = *this;
            ++(*this);
            return temp;
        }

        Iterator& operator--()
        {
            if (current) current = current->prev;
            return *this;
        }

        Iterator operator--(int)
        {
            Iterator temp = *this;
            --(*this);
            return temp;
        }

        bool operator==(const Iterator& other) const
        {
            return current == other.current;
        }

        bool operator!=(const Iterator& other) const
        {
            return current != other.current;
        }

        Node<T>* getNode() const { return current; }
    };

    Iterator begin() { return Iterator(head); }
    Iterator end() { return Iterator(nullptr); }

    Iterator begin() const { return Iterator(head); }
    Iterator end() const { return Iterator(nullptr); }

    Iterator erase(Iterator pos)
    {
        Node<T>* node = pos.getNode();
        if (node == nullptr) return end();

        Iterator next(node->next);
        remove(node);
        return next;
    }

    // range-based for
    using iterator = Iterator;
    using const_iterator = Iterator;
};
