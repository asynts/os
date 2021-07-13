### TODO

#### Next Version

-   Context switch using PendSV? I think this note refered to context switching
    in thread mode and if that could utilize the supervisor mode?

-   Group `PageRange`s together in `PageAllocator::deallocate`.

-   Add passive locking primitives

-   Add active locking primitives

#### Bugs

-   We have a ton of memory leaks in the filesystem, e.g. `VirtualFile::create_handle_impl`.

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

#### Future features

-   Keep documentation about interrupt safe functions and which functions can be called in which boot stage

-   Add `MemoryAllocator::allocate_eternal` which doesn't create MTRACE logs

  - Run inside QEMU

  - Write userland applications in Zig

#### Future tweaks (Userland)

  - Implement a proper malloc

#### Future tweaks (Kernel)

  - Setup MPU for supervisor mode

  - HardFault in usermode crashes kernel

  - Stack smash protection with MPU

      - Build with `-fstack-protector`?

#### Future tweaks (Build)

  - Alignment of `.stack`, `.heap` sections is lost in `readelf`

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
