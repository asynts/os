### TODO

#### 0.1.0

  - Configure the MPU

  - Create a test application

  - Verify that test application runs on Linux

#### Future features

  - Run inside QEMU

#### Future tweaks (Everywhere)

  - Figure out the placement `new` and `reinterpret_cast` situation

#### Future tweaks (Userland)

  - Error message on assertion failure

  - Implement a proper malloc

  - `Editor.elf` should take an argument

#### Future tweaks (Kernel)

  - Free resources when threads and process are destroyed

  - Add a buddy page allocator; the curring "allocate twice as much and align"
    strategy is a joke

  - We are allocating the stack twice; is this still accurate with
    `posix_spawn`?

  - Lookup devices via `devno`

#### Future tweaks (Library)

  - `Result<int, int>` should require explicit calls to `from_value`, and
    `from_error`.  This is important for `KernelResult`

#### Future tweaks (Build)

  - Alignment of `.stack`, `.heap` sections is lost in `readelf`

  - Turn on all sorts of warnings

  - C++20 modules

  - Drop SDK entirely

      - Link `libsup++` or add a custom downcast?

  - Meson build

  - Try using LLDB instead of GDB

  - Don't leak includes from newlib libc

  - Build with `-fstack-protector`

  - Use LLVM/LLD for `FileEmbed`; Not sure what I meant with this, but LLVM
    surely has all the tools buildin that I need

  - GDB apparently has a secret 'proc' command that makes it possible to debug
    multiple processes.  This was mentioned in the DragonFlyBSD documentation,
    keyword: "inferiour"

  - Don't build the tests if we can't run them anyways

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
