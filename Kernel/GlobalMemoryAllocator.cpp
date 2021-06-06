#include <Kernel/GlobalMemoryAllocator.hpp>
#include <Kernel/PageAllocator.hpp>

namespace Kernel
{
    GlobalMemoryAllocator::GlobalMemoryAllocator()
        : MemoryAllocator({ reinterpret_cast<u8*>(PageAllocator::the().allocate(14).must()), 1 << 14 })
    {
        dbgln("[GlobalMemoryAllocator::GlobalMemoryAllocator] Allocated area {}", m_heap);
    }
}

void* operator new(usize size)
{
    void *address = __builtin_return_address(0);
    return Kernel::GlobalMemoryAllocator::the().allocate(size, true, address);
}
void* operator new[](usize size)
{
    void *address = __builtin_return_address(0);
    return Kernel::GlobalMemoryAllocator::the().allocate(size, true, address);
}
void operator delete(void* pointer)
{
    void *address = __builtin_return_address(0);
    return Kernel::GlobalMemoryAllocator::the().deallocate(reinterpret_cast<u8*>(pointer), true, address);
}
void operator delete[](void* pointer)
{
    void *address = __builtin_return_address(0);
    return Kernel::GlobalMemoryAllocator::the().deallocate(reinterpret_cast<u8*>(pointer), true, address);
}
void operator delete(void* pointer, usize)
{
    void *address = __builtin_return_address(0);
    return Kernel::GlobalMemoryAllocator::the().deallocate(reinterpret_cast<u8*>(pointer), true, address);
}
void operator delete[](void* pointer, usize)
{
    void *address = __builtin_return_address(0);
    return Kernel::GlobalMemoryAllocator::the().deallocate(reinterpret_cast<u8*>(pointer), true, address);
}

extern "C"
void* malloc(usize size)
{
    void *address = __builtin_return_address(0);
    return Kernel::GlobalMemoryAllocator::the().allocate(size, true, address);
}

extern "C"
void* calloc(usize nmembers, usize size)
{
    void *address = __builtin_return_address(0);
    u8 *pointer = Kernel::GlobalMemoryAllocator::the().allocate(nmembers * size, true, address);

    __builtin_memset(pointer, 0, nmembers * size);

    return pointer;
}

extern "C"
void free(void *pointer)
{
    void *address = __builtin_return_address(0);
    return Kernel::GlobalMemoryAllocator::the().deallocate(reinterpret_cast<u8*>(pointer), true, address);
}

extern "C"
void* realloc(void *pointer, usize size)
{
    void *address = __builtin_return_address(0);
    return Kernel::GlobalMemoryAllocator::the().reallocate(reinterpret_cast<u8*>(pointer), size, true, address);
}

extern "C"
void* reallocarray(void *pointer, usize nmembers, usize size)
{
    void *address = __builtin_return_address(0);
    return Kernel::GlobalMemoryAllocator::the().reallocate(reinterpret_cast<u8*>(pointer), nmembers * size, true, address);
}
