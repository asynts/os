#include <Kernel/Interrupt/UART.hpp>

#include <hardware/irq.h>
#include <hardware/uart.h>
#include <hardware/gpio.h>
#include <hardware/structs/uart.h>
#include <hardware/dma.h>

namespace Kernel::Interrupt
{
    void UART::configure_dma()
    {
        const u32 channel = 0;

        // FIXME: Claim

        auto config = dma_channel_get_default_config(channel);
        channel_config_set_transfer_data_size(&config, DMA_SIZE_8);

        // The read address is the address of the UART data register which is constant
        channel_config_set_read_increment(&config, false);

        // Write into a ringbuffer with '2^buffer_power=buffer_size' elements
        channel_config_set_write_increment(&config, true);
        channel_config_set_ring(&config, true, buffer_power);

        // The UART signals when data is avaliable
        channel_config_set_dreq(&config, DREQ_UART0_RX);

        // Transmit '2^32 - 1' symbols, this should suffice for any practical case,
        // otherwise, the channel could be triggered again
        dma_channel_configure(
            channel,
            &config,
            m_input_buffer->data(),
            &uart0_hw->dr,
            UINT32_MAX,
            true);
    }

    void UART::configure_uart()
    {
        uart_init(uart0, 115200);

        gpio_set_function(PICO_DEFAULT_UART_TX_PIN, GPIO_FUNC_UART);
        gpio_set_function(PICO_DEFAULT_UART_RX_PIN, GPIO_FUNC_UART);

        // On my system there is one junk byte on boot
        uart_getc(uart0);
    }

    UART::UART()
    {
        m_input_buffer = PageAllocator::the().allocate(buffer_power).must();

        configure_uart();
        configure_dma();
    }

    KernelResult<usize> UART::read(Bytes bytes)
    {
        usize index;
        for (index = 0; index < min(input_buffer_size(), bytes.size()); ++index) {
            bytes[index] = m_input_buffer->bytes()[input_buffer_consume_offset()];
            ++m_input_buffer_consume_offset_raw;
        }

        return index;
    }

    KernelResult<usize> UART::write(ReadonlyBytes bytes)
    {
        for (usize index = 0; index < bytes.size(); ++index) {
            if (uart_is_writable(uart0)) {
                uart_putc_raw(uart0, static_cast<char>(bytes[index]));
            } else {
                return index;
            }
        }

        return bytes.size();
    }

    usize UART::input_buffer_consume_offset()
    {
        return m_input_buffer_consume_offset_raw % buffer_size;
    }

    usize UART::input_buffer_avaliable_offset()
    {
        VERIFY(dma_channel_hw_addr(input_dma_channel)->write_addr >= uptr(m_input_buffer->data()));
        return dma_channel_hw_addr(input_dma_channel)->write_addr - uptr(m_input_buffer->data());
    }

    usize UART::input_buffer_size()
    {
        FIXME_ASSERT(input_buffer_avaliable_offset() >= input_buffer_consume_offset());
        return input_buffer_avaliable_offset() - input_buffer_consume_offset();
    }
}
