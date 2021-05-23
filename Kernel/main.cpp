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

union MPU_RASR {
    struct {
        u32 enable : 1;
        u32 size : 5;
        u32 reserved_4 : 2;
        u32 srd : 8;
        u32 attrs_b : 1;
        u32 attrs_c : 1;
        u32 attrs_s : 1;
        u32 attrs_tex : 3;
        u32 reserved_3 : 2;
        u32 attrs_ap : 3;
        u32 reserved_2 : 1;
        u32 attrs_xn : 1;
        u32 reserved_1 : 3;
    };
    u32 raw;
};

static void dump_mpu()
{
    dbgln("[dump_mpu]:");
    for (size_t i = 0; i < 8; ++i) {
        mpu_hw->rnr = i;
        dbgln("  [{}] RASR={} RBAR={}", i, (u32)mpu_hw->rasr, (u32)mpu_hw->rbar);
    }
}

static void try_out_mpu()
{
    auto *my_custom_region = new u8[2 * KiB];

    dbgln("[try_out_mpu] Allocated memory for custom region at {}", my_custom_region);

    my_custom_region += KiB - u32(my_custom_region) % KiB;

    dbgln("[try_out_mpu] Aligned memory region is {}", my_custom_region);

    // Disable MPU
    mpu_hw->ctrl = 0;

    dump_mpu();

    //- Setup region for flash
    mpu_hw->rnr = 0;

    mpu_hw->rbar = 0x10000000;

    auto rasr_0 = static_cast<MPU_RASR>(mpu_hw->rasr);
    rasr_0.attrs_xn = 0;
    rasr_0.attrs_ap = 0b111;
    rasr_0.attrs_tex = 0b000;
    rasr_0.attrs_c = 1;
    rasr_0.attrs_b = 1;
    rasr_0.attrs_s = 1;
    rasr_0.srd = 0b11111111;
    rasr_0.size = 20;
    rasr_0.enable = 1;
    mpu_hw->rasr = static_cast<u32>(rasr_0.raw);

    dump_mpu();

    mpu_hw->ctrl = 0 << 2  // Disable default map for privileged execution
                 | 0 << 1  // Disable MPU during HardFault/NMI
                 | 1 << 0; // Enable MPU

    // We crash immediatelly, when executing the next instruction
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");

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
