


template <typename T>
class MemoryPool
{
private:
    T* memoryPool_;
    T* head_;
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
        head_ = &memoryPool_[0];
    }
    ~MemoryPool()
    {
        delete[] memoryPool_;
    }
    T* allocate()
    {
        if (head_ == nullptr) return nullptr;
        T* temp = head_;
        T** pointer_to_next = reinterpret_cast<T**>(head_);
        head_ = *pointer_to_next;
        return temp;
    }
    void deallocate(T* memory)
    {
        if (memory == nullptr) return;
        T** new_block = reinterpret_cast<T**>(memory);
        *new_block = head_;
        head_ = memory;
    }

};






