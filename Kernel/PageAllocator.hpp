#pragma once

#include <Std/Singleton.hpp>
#include <Std/Array.hpp>
#include <Std/Optional.hpp>
#include <Std/Format.hpp>

namespace Kernel
{
    using namespace Std;

    struct PageRange {
        usize m_power;
        uptr m_base;

        const u8* data() const { return reinterpret_cast<const u8*>(m_base); }
        u8* data() { return reinterpret_cast<u8*>(m_base); }

        usize size() const { return 1 << m_power; }

        ReadonlyBytes bytes() const { return { data(), size() }; }
        Bytes bytes() { return { data(), size() }; }
    };

    class PageAllocator : public Singleton<PageAllocator> {
    public:
        static constexpr usize max_power = 18;

        Optional<PageRange> allocate(usize power)
        {
            usize size = 1 << power;

            dbgln("[PageAllocator::allocate] power={}", power);

            ASSERT(power <= max_power);

            if (m_blocks[power] != nullptr) {
                uptr base = reinterpret_cast<uptr>(m_blocks[power]);

                dbgln("[PageAllocator::allocate] Found suitable block {}", base);

                m_blocks[power] = m_blocks[power]->m_next;
                return PageRange { power, base };
            }

            ASSERT(power < max_power);

            auto block_opt = allocate(power + 1);
            if (!block_opt.is_valid()) {
                return {};
            }
            auto block = block_opt.value();

            deallocate(PageRange{ power - 1, block.m_base + size });

            return PageRange { power, block.m_base };
        }

        void deallocate(PageRange range)
        {
            ASSERT(range.m_power <= max_power);

            auto *block_ptr = reinterpret_cast<Block*>(range.m_base);
            block_ptr->m_next = m_blocks[range.m_power];
            m_blocks[range.m_power] = block_ptr;
        }

        void dump()
        {
            dbgln("[PageAllocator] blocks:");
            for (usize power = 0; power < max_power; ++power) {
                dbgln("  [{}]: {}", power, m_blocks[power]);
            }
        }

    private:
        friend Singleton<PageAllocator>;
        PageAllocator();

        // There is quite a bit of trickery going on here:
        //
        //   - The address of this block is encoded indirectly in the address of this object
        //
        //   - The size of this block is encoded indirection in the index used to access m_blocks
        struct Block {
            Block *m_next;
        };

        Array<Block*, max_power + 1> m_blocks;
    };
}
