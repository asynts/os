#![no_std]
#![no_main]

use hardware::gpio;

#[no_mangle]
pub fn boot() -> ! {
    gpio::set_direction(gpio::Pin::StatusLed, gpio::Direction::Output);
    gpio::set_output(gpio::Pin::StatusLed, gpio::State::High);

    hardware::halt();
}
