#pragma once

#include <Std/Forward.hpp>

namespace Kernel
{
    using namespace Std;

    class Thread;

    // Defined here because of circular dependency between KernelMutex, Thread and PageAllocator
    struct PageRange {
        usize m_power;
        uptr m_base;

        // FIXME: Are these methods needed?

        const u8* data() const { return reinterpret_cast<const u8*>(m_base); }
        u8* data() { return reinterpret_cast<u8*>(m_base); }

        usize size() const { return 1 << m_power; }

        ReadonlyBytes bytes() const { return { data(), size() }; }
        Bytes bytes() { return { data(), size() }; }
    };
    class OwnedPageRange {
    public:
        explicit OwnedPageRange(PageRange range)
            : m_range(range)
        {
        }
        OwnedPageRange(const OwnedPageRange&) = delete;
        OwnedPageRange(OwnedPageRange&& other)
        {
            m_range = move(other.m_range);
        }
        ~OwnedPageRange();

        OwnedPageRange& operator=(const OwnedPageRange&) = delete;

        OwnedPageRange& operator=(OwnedPageRange&& other)
        {
            m_range = move(other.m_range);
            return *this;
        }

        usize size() const { return m_range->size(); }

        const u8* data() const { return m_range->data(); }
        u8* data() { return m_range->data(); }

        ReadonlyBytes bytes() const { return m_range->bytes(); }
        Bytes bytes() { return m_range->bytes(); }

        Optional<PageRange> m_range;
    };
}
