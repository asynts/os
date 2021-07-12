#pragma once

#include <Std/Singleton.hpp>
#include <Std/Array.hpp>
#include <Std/Optional.hpp>
#include <Std/Format.hpp>

#include <Kernel/Forward.hpp>
#include <Kernel/KernelMutex.hpp>

namespace Kernel
{
    constexpr bool debug_page_allocator = false;

    class PageAllocator : public Singleton<PageAllocator> {
    public:
        static constexpr usize max_power = 18;
        static constexpr usize stack_power = power_of_two(0x800);

        Optional<OwnedPageRange> allocate(usize power)
        {
            m_mutex.lock();
            Optional<PageRange> range_opt = allocate_locked(power);
            m_mutex.unlock();

            if (range_opt.is_valid())
                return OwnedPageRange { range_opt.value() };
            else
                return {};
        }

        void deallocate(PageRange range)
        {
            m_mutex.lock();
            deallocate_locked(range);
            m_mutex.unlock();
        }

        void dump()
        {
            if (debug_page_allocator) {
                dbgln("[PageAllocator] blocks:");
                for (usize power = 0; power < max_power; ++power) {
                    dbgln("  [{}]: {}", power, m_blocks[power]);
                }
            }
        }

    private:
        friend Singleton<PageAllocator>;
        PageAllocator();

        void deallocate_locked(PageRange range)
        {
            if (debug_page_allocator)
                dbgln("[PageAllocator::deallocate] power={} base={}", range.m_power, range.m_base);

            ASSERT(range.m_power <= max_power);

            auto *block_ptr = reinterpret_cast<Block*>(range.m_base);
            block_ptr->m_next = m_blocks[range.m_power];
            m_blocks[range.m_power] = block_ptr;
        }

        Optional<PageRange> allocate_locked(usize power)
        {
            usize size = 1 << power;

            if (debug_page_allocator)
                dbgln("[PageAllocator::allocate] power={}", power);

            ASSERT(power <= max_power);

            if (m_blocks[power] != nullptr) {
                uptr base = reinterpret_cast<uptr>(m_blocks[power]);

                if (debug_page_allocator)
                    dbgln("[PageAllocator::allocate] Found suitable block {}", base);

                m_blocks[power] = m_blocks[power]->m_next;
                return PageRange { power, base };
            }

            ASSERT(power < max_power);

            auto block_opt = allocate_locked(power + 1);
            if (!block_opt.is_valid()) {
                return {};
            }
            auto block = block_opt.value();

            deallocate_locked(PageRange{ power, block.m_base + size });

            return PageRange { power, block.m_base };
        }

        KernelMutex m_mutex;

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
