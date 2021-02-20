macro_rules! hardware_register {
    ($name:ident @ $address:expr) => {
        const $name: volatile::Volatile<u32, volatile::access::WriteOnly> volatile::Volatile::new_write_only(*($address as *mut u32))
    };
}

pub fn halt() -> ! {
    loop {
        unsafe {
            asm!("wfi")
        }
    }
}

pub mod gpio {
    hardware_register!(GPIO_OUT_SET @ 0xd0000014);
    hardware_register!(GPIO_OUT_CLR @ 0xd0000018);
    hardware_register!(GPIO_OE_SET @ 0xd0000024);
    hardware_register!(GPIO_OE_CLR @ 0xd0000028);

    pub enum Pin {
        StatusLed = 25
    }

    pub enum Direction {
        Input,
        Output,
    }

    pub enum State {
        Low,
        High,
    }

    pub fn set_direction(Pin pin, Direction direction)
    {
        unsafe {
            if direction == Direction::Output {
                GPIO_OE_SET.write(1 << (pin as usize));
            } else {
                GPIO_OE_CLR.write(1 << (pin as usize));
            }
        }
    }

    pub fn set_output(Pin pin, State state)
    {
        unsafe {
            if state == State::High {
                GPIO_OUT_SET.write(1 << (pin as usize));
            } else {
                GPIO_OUT_CLR.write(1 << (pin as usize));
            }
        }
    }
}
