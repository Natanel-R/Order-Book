#pragma once
#include <atomic>


template <typename T>
struct alignas(16) TaggedPointer
{
    T* ptr;
    uint64_t version;
};


template <typename T>
class MemoryPool
{
private:
    T* memoryPool_;
    std::atomic<TaggedPointer<T>> head_;
    size_t capacity;

public:
    MemoryPool(size_t capacity)
    {
        this->capacity = capacity;
        memoryPool_ = new T[capacity];
        for (size_t i = 0; i < capacity - 1; ++i)
        {
            T* current_block = &memoryPool_[i];
            T* next_block = &memoryPool_[i+1];
            T** pointer_to_next = reinterpret_cast<T**>(current_block);
            *pointer_to_next = next_block;
        }
        T** pointer_to_last = reinterpret_cast<T**>(&memoryPool_[capacity-1]);
        *pointer_to_last = nullptr;
        head_ = TaggedPointer<T>{&memoryPool_[0], 0};
    }
    ~MemoryPool()
    {
        delete[] memoryPool_;
    }
    T* allocate()
    {
        TaggedPointer<T> expected = head_.load();
        T* next_block;
        do
        {
            if (expected.ptr == nullptr) return nullptr;
            T** pointer_to_next = reinterpret_cast<T**>(expected.ptr);
            next_block = *pointer_to_next;

        }
        while(!head_.compare_exchange_weak(expected, TaggedPointer<T>{next_block, expected.version + 1}));
        return expected.ptr;
    }
    void deallocate(T* memory)
    {
        if (memory == nullptr) return;
        TaggedPointer<T> expected = head_.load();
        T** new_block = reinterpret_cast<T**>(memory);
        do 
        {
            *new_block = expected.ptr;
        }
        while (!head_.compare_exchange_weak(expected, TaggedPointer<T>{memory, expected.version + 1}));
    }
};






