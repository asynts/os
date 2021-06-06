#pragma once

#include <Std/Singleton.hpp>
#include <Std/Array.hpp>

namespace Kernel
{
    using namespace Std;

    class PageAllocator : public Singleton<PageAllocator> {
    public:

    private:
        friend Singleton<PageAllocator>;
        PageAllocator()
        {
            for (auto& block : m_blocks.span().iter()) {
                block = nullptr;
            }

            // The RP2040 has 32 KiB RAM at 0x20000000
            m_blocks[15] = reinterpret_cast<Block*>(0x20000000);
            m_blocks[15]->m_next = nullptr;
        }

        // There is quite a bit of trickery going on here:
        //
        //   - The address of this block is encoded indirectly in the address of this object
        //
        //   - The size of this block is encoded indirection in the index used to access m_blocks
        struct Block {
            Block *m_next;
        };

        Array<Block*, 16> m_blocks;
    };
}
