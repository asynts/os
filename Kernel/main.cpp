#include <Std/Forward.hpp>
#include <Std/Format.hpp>

#include <Kernel/Loader.hpp>
#include <Kernel/ConsoleDevice.hpp>
#include <Kernel/FileSystem/MemoryFileSystem.hpp>
#include <Kernel/FileSystem/FlashFileSystem.hpp>
#include <Kernel/FileSystem/DeviceFileSystem.hpp>
#include <Kernel/Process.hpp>
#include <Kernel/GlobalMemoryAllocator.hpp>
#include <Kernel/Scheduler.hpp>
#include <Kernel/ConsoleDevice.hpp>

#include <hardware/structs/mpu.h>

void create_shell_process()
{
    auto& shell_file = dynamic_cast<Kernel::FlashFile&>(Kernel::FileSystem::lookup("/bin/Shell.elf"));
    Kernel::ElfWrapper elf { shell_file.m_data.data(), "Userland/Shell.1.elf" };
    Kernel::Process::create("/bin/Shell.elf", move(elf));
}

static void try_out_mpu()
{
    auto *my_custom_region = new u8[2 * KiB];

    dbgln("[try_out_mpu] Allocated memory for custom region at {}", my_custom_region);

    my_custom_region += KiB - u32(my_custom_region) % KiB;

    dbgln("[try_out_mpu] Aligned memory region is {}", my_custom_region);

    mpu_hw->ctrl = 0;

    mpu_hw->rnr = 0;

    mpu_hw->rbar = u32(my_custom_region);

    mpu_hw->rasr =     1 << 28  // Disable instruction fetch

        // FIXME: This seems to be completely ignored!
                 | 0b000 << 24  // Nobody can do anything

                 |      1 << 18 // Shareable (FIXME)
                 |      1 << 19 // Cacheable (FIXME)
                 |      1 << 20 // Bufferable (FIXME)
                 | 0b1111 << 8  // Enable all subregions
                 |      9 << 1  // 1 KiB = 2 ** 10 bytes
                 |      1 << 0; // Enable region

    mpu_hw->ctrl = 1 << 2  // Add default map for privileged execution
                 | 0 << 1  // Disable MPU during HardFault/NMI
                 | 1 << 0; // Enable MPU

    dbgln("[try_out_mpu] Just enabled the MPU, and still running");

    u32 control;
    asm volatile("mrs %0, control;"
                 "isb;"
        : "=r"(control));

    // We should be executing in privileged mode here
    ASSERT((control & 0b01) == 0);

    asm volatile("ldr r0, [%0];"
        :
        : "r"(my_custom_region)
        : "r0");

    dbgln("[try_out_mpu] We just read from the custom region and we are stull running");

    asm volatile("str r0, [%0];"
        :
        : "r"(my_custom_region)
        : "r0");

    dbgln("[try_out_mpu] We just wrote to the custom region and it worked!");

    mpu_hw->ctrl = 0;

    dbgln("[try_out_mpu] done, disabled mpu");
}

int main()
{
    Kernel::ConsoleFile::the();

    dbgln("\e[0;1mBOOT\e[0m");

    try_out_mpu();

    Kernel::GlobalMemoryAllocator::the();
    Kernel::Scheduler::the();

    Kernel::MemoryFileSystem::the();
    Kernel::FlashFileSystem::the();
    Kernel::DeviceFileSystem::the();

    dbgln("[main] Creating /example.txt");
    auto& example_file = *new Kernel::MemoryFile;
    auto& example_handle = example_file.create_handle();
    example_handle.write({ (const u8*)"Hello, world!\n", 14 });

    auto& root_file = Kernel::FileSystem::lookup("/");
    dynamic_cast<Kernel::VirtualDirectory&>(root_file).m_entries.set("example.txt", &example_file);

    create_shell_process();

    Kernel::Scheduler::the().loop();

    VERIFY_NOT_REACHED();
}
