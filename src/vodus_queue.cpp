template <typename T, size_t Capacity>
struct Queue
{
    T items[Capacity];
    size_t begin;
    size_t count;

    T &operator[](size_t index)
    {
        return items[(begin + index) % Capacity];
    }

    const T &operator[](size_t index) const
    {
        return items[(begin + index) % Capacity];
    }

    T &first()
    {
        assert(count > 0);
        return items[begin];
    }

    void enqueue(T x)
    {
        assert(count < Capacity);
        items[(begin + count) % Capacity] = x;
        count++;
    }

    T dequeue()
    {
        assert(count > 0);
        T result = items[begin];
        begin = (begin + 1) % Capacity;
        count--;
        return result;
    }
};
