#pragma once
#include <cstddef> // for size_t

class LinearAllocator
{
public:
    void init(size_t totalSize);
    void* allocate(size_t size);
    void reset();
    void shutdown();

private:
    unsigned char* memory = nullptr;
    size_t totalSize = 0;
    size_t usedSize = 0;
};