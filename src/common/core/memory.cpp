#include "memory.h"
#include <cstdlib> // for malloc/free

void LinearAllocator::init(size_t size)
{
    totalSize = size;
    usedSize = 0;
    memory = (unsigned char*)malloc(totalSize);
}

void* LinearAllocator::allocate(size_t size)
{
    if (usedSize + size > totalSize) return nullptr; // Out of memory
    void* ptr = memory + usedSize;
    usedSize += size;
    return ptr;
}

void LinearAllocator::reset()
{
    usedSize = 0;
}

void LinearAllocator::shutdown()
{
    if (memory)
    {
        free(memory);
        memory = nullptr;
        totalSize = 0;
        usedSize = 0;
    }
}