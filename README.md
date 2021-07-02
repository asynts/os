### TODO

**Try setting goals with time estimates**

#### Next Version

-   In BSB we had the issue that context switches were not syncronized which
    caused issues sometimes. Ensure that context switching is properly
    syncronized.

#### Bugs

  - If we do `stat /dev/tty` we get invalid information, because `ConsoleFileHandle` always
    returns `ConsoleFile` instead of the actual file.

  - Sometimes we seem to mess something up, this is visible when `ConsoleFileHandle` has an invalid
    `this` pointer. I reproduced this by running:

    ~~~none
    Example.elf
    Example.elf
    Example.elf
    Example.elf
    Example.elf
    Example.elf
    Example.elf
    ~~~

  - Free loaded executables; we already free stacks of threads we should do
    this for executables too.

  - Typing very quickly can drop characters, this could be resolved with the DMA feature.

#### Future features

  - Add some license text, like MIT or something?

  - Run inside QEMU

  - Write userland applications in Zig

  - UART with DMA to increase buffer size, this could allow copy+paste into the
    window

#### Future tweaks (Userland)

  - Implement a proper malloc

#### Future tweaks (Kernel)

  - Context switch using PendSV? I think this note refered to context switching
    in thread mode and if that could utilize the supervisor mode?

  - Use RAII to manage `PageRange`s

  - Group `PageRange`s together in `PageAllocator::deallocate`.

  - Provide a shortcut for `Scheduler::the().active_thread()` similar: `Process::current()`.

  - Setup MPU for supervisor mode

  - HardFault in usermode crashes kernel

  - Stack smash protection with MPU

      - Build with `-fstack-protector`?

#### Future tweaks (Build)

  - Alignment of `.stack`, `.heap` sections is lost in `readelf`

  - Turn on all sorts of warnings

  - C++20 modules

  - Drop SDK entirely

      - Link `libsup++` or add a custom downcast?

  - Meson build

  - Try using LLDB instead of GDB

  - Don't leak includes from newlib libc

  - Use LLVM/LLD for `FileEmbed`; Not sure what I meant with this, but LLVM
    surely has all the tools buildin that I need

  - GDB apparently has a secret 'proc' command that makes it possible to debug
    multiple processes.  This was mentioned in the DragonFlyBSD documentation,
    keyword: "inferiour"

### Helpful Documentation

#### Stuff that I already looked at:

  - ARM ABI (https://github.com/ARM-software/abi-aa/releases)
  - ELF(5) man page (https://man7.org/linux/man-pages/man5/elf.5.html)
  - Ian Lance Taylor: Linkers (https://www.airs.com/blog/archives/38)

### Software

Installed via `pacman`:

~~~none
pacman -S --needed python-invoke arm-none-eabi-gcc
~~~

Some packages have to be manually build from AUR:

- TIO to connect to serial device (http://tio.github.io/)
