#![no_std]
#![no_main]

#![feature(asm)]

pub mod gpio {
    const SIO_BASE: usize = 0xd0000000;
    const GPIO_OUT_SET: *mut u32 = (SIO_BASE + 0x014) as *mut u32;
    const GPIO_OUT_CLR: *mut u32 = (SIO_BASE + 0x018) as *mut u32;
    const GPIO_OE_SET: *mut u32 = (SIO_BASE + 0x024) as *mut u32;
    const GPIO_OE_CLR: *mut u32 = (SIO_BASE + 0x028) as *mut u32;

    pub enum Direction {
        Input,
        Output,
    }

    pub enum Pin {
        StatusLed = 25,
    }

    pub enum Value {
        Low,
        High,
    }

    pub fn set_direction(gpio: Pin, direciton: Direction)
    {
        unsafe {
            match direciton {
                Direction::Input => *GPIO_OE_SET = 1 << gpio as usize,
                Direction::Output => *GPIO_OE_CLR = 1 << gpio as usize,
            }
        }
    }

    pub fn set_value(gpio: Pin, value: Value)
    {
        unsafe {
            match value {
                Value::High => *GPIO_OUT_SET = 1 << gpio as usize,
                Value::Low => *GPIO_OUT_CLR = 1 << gpio as usize,
            }
        }
    }
}

pub fn halt() -> ! {
    loop {
        unsafe {
            asm!("wfi");
        }
    }
}

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    gpio::set_direction(gpio::Pin::StatusLed, gpio::Direction::Output);
    gpio::set_value(gpio::Pin::StatusLed, gpio::Value::High);

    halt();
}

// #[entry]
pub fn init() -> !{
    halt();
}
