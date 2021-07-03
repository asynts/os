#include <Kernel/Interrupt/UART.hpp>

#include <hardware/irq.h>
#include <hardware/uart.h>
#include <hardware/gpio.h>
#include <hardware/structs/uart.h>
#include <hardware/dma.h>

// FIXME: The queue is not protected, we should setup DMA to automatically
//        write everything into a buffer

namespace Kernel::Interrupt
{
    // Configure CHANNEL0 to receive data from UART0 if avaiable
    void UART::configure_dma()
    {
        dma_channel_claim(input_dma_channel);

        auto channel_config = dma_channel_get_default_config(input_dma_channel);
        channel_config_set_transfer_data_size(&channel_config, DMA_SIZE_8);
        channel_config_set_read_increment(&channel_config, false);
        channel_config_set_write_increment(&channel_config, true);
        channel_config_set_ring(&channel_config, true, buffer_power);

        // FIXME: Does this only trigger one byte read?
        channel_config_set_dreq(&channel_config, DREQ_UART0_RX);

        dma_channel_configure(
            input_dma_channel,
            &channel_config,
            m_input_buffer->data(),
            reinterpret_cast<const void*>(uart0_hw->dr),
            m_input_buffer->size(),
            false);
    }

    // Configure UART0 to enable DMA for receive
    void UART::configure_uart()
    {
        uart_init(uart0, 115200);

        gpio_set_function(PICO_DEFAULT_UART_TX_PIN, GPIO_FUNC_UART);
        gpio_set_function(PICO_DEFAULT_UART_RX_PIN, GPIO_FUNC_UART);

        // FIXME: There seems thre is an initial "junk" byte read, I've seen
        //        0xff and 0xfc
        uart_getc(uart0);

        uart_set_fifo_enabled(uart0, false);

        uart0_hw->dmacr =  UART_UARTDMACR_RXDMAE_BITS
                        | ~UART_UARTDMACR_TXDMAE_BITS
                        |  UART_UARTDMACR_DMAONERR_BITS;
    }

    UART::UART()
    {
        m_input_buffer = PageAllocator::the().allocate_owned(buffer_power).must();

        configure_dma();
        configure_uart();
    }

    KernelResult<usize> UART::read(Bytes bytes)
    {
        usize index;
        for (index = 0; index < input_buffer_size(); ++index) {
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
