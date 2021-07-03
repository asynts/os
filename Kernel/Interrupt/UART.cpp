#include <Kernel/Interrupt/UART.hpp>
#include <Kernel/PageAllocator.hpp>

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
    static void setup_dma()
    {
        constexpr usize buffer_size = 1 * KiB;
        constexpr usize buffer_power = power_of_two(buffer_size);

        auto page_range = PageAllocator::the().allocate(buffer_power).must();

        auto channel = dma_channel_claim(0);

        auto channel_config = dma_channel_get_default_config(channel);
        channel_config_set_transfer_data_size(&channel_config, DMA_SIZE_8);
        channel_config_set_read_increment(&channel_config, false);
        channel_config_set_write_increment(&channel_config, true);
        channel_config_set_ring(&channel_config, true, buffer_power);
        channel_config_set_dreq(&channel_config, DREQ_UART0_RX);

        dma_channel_configure(
            channel,
            &channel_config,
            page_range.data(),
            reinterpret_cast<const void*>(uart0_hw->dr),
            1,
            false);
    }

    // Configure UART0 to enable DMA for receive
    static void setup_uart()
    {
        uart_init(uart0, 115200);

        gpio_set_function(PICO_DEFAULT_UART_TX_PIN, GPIO_FUNC_UART);
        gpio_set_function(PICO_DEFAULT_UART_RX_PIN, GPIO_FUNC_UART);

        // FIXME: There seems thre is an initial "junk" byte read, I've seen
        //        0xff and 0xfc
        uart_getc(uart0);

        uart_set_fifo_enabled(uart0, false);

        uart0_hw->dmacr = UART_UARTDMACR_RXDMAE_BITS
                        | ~UART_UARTDMACR_TXDMAE_BITS
                        | UART_UARTDMACR_DMAONERR_BITS;
    }

    UART::UART()
    {
        setup_dma();
        setup_uart();
    }

    void UART::interrupt()
    {
        if (debug_uart)
            dbgln("[UART::interrupt]");

        while (uart_is_readable(uart0))
        {
            UART::the().m_input_queue.enqueue(uart_getc(uart0));
        }
    }

    KernelResult<usize> UART::read(Bytes bytes)
    {
        usize index;
        for (index = 0; index < min(bytes.size(), m_input_queue.size()); ++index) {
            // FIXME: This is not safe
            bytes[index] = m_input_queue.dequeue();
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
}
