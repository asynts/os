#pragma once

#include <Std/Singleton.hpp>
#include <Std/Span.hpp>
#include <Std/CircularQueue.hpp>

#include <Kernel/Forward.hpp>
#include <Kernel/Result.hpp>
#include <Kernel/PageAllocator.hpp>

namespace Kernel::Interrupt
{
    constexpr bool debug_uart = false;

    class UART : public Singleton<UART> {
    public:
        void trigger();

        KernelResult<usize> read(Bytes);
        KernelResult<usize> write(ReadonlyBytes);

        static constexpr usize buffer_size = 1 * KiB;
        static constexpr usize buffer_power = power_of_two(buffer_size);

        static constexpr u32 input_dma_channel = 0;

    private:
        Optional<OwnedPageRange> m_input_buffer;
        usize m_input_buffer_consume_offset_raw = 0;

        usize input_buffer_consume_offset();
        usize input_buffer_avaliable_offset();
        usize input_buffer_size();

        friend Singleton<UART>;
        UART();

        void configure_dma();
        void configure_uart();
    };
}
