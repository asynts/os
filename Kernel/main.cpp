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

#include <Kernel/MPU.hpp>

void create_shell_process()
{
    auto& shell_file = dynamic_cast<Kernel::FlashFile&>(Kernel::FileSystem::lookup("/bin/Shell.elf"));
    Kernel::ElfWrapper elf { shell_file.m_data.data(), "Userland/Shell.1.elf" };
    Kernel::Process::create("/bin/Shell.elf", move(elf));
}

static void dump_mpu()
{
    u32 rnr = mpu_hw->rnr;

    dbgln("[dump_mpu]: CTRL={} RNR={} TYPE={}", (u32)mpu_hw->ctrl, rnr, (u32)mpu_hw->type);
    for (size_t i = 0; i < 8; ++i) {
        mpu_hw->rnr = i;
        dbgln("  [{}] RASR={} RBAR={}", i, (u32)mpu_hw->rasr, (u32)mpu_hw->rbar);
    }

    mpu_hw->rnr = rnr;
}

static void try_configure_mpu_3()
{
    dump_mpu();

    {
        auto ctrl = Kernel::MPU::ctrl();
        ctrl.enable = 0;
        Kernel::MPU::set_ctrl(ctrl);
    }

    for (size_t i = 0; i < 8; ++i) {
        mpu_hw->rnr = i;
        mpu_hw->rbar = 0x00000000;
        mpu_hw->rasr = 0;
    }

    mpu_hw->rnr = 0;

    // FIXME: It seems this address is provided incorrectly?
    mpu_hw->rbar = 0x10000000;

    auto rasr = Kernel::MPU::rasr();
    rasr.attrs_ap = 0b111;
    rasr.attrs_b = 1;
    rasr.attrs_c = 1;
    rasr.attrs_s = 1;
    rasr.attrs_tex = 0b000;
    rasr.attrs_xn = 1; // FIXME: This should prevent execution
    rasr.size = 20;
    rasr.enable = 1;
    rasr.srd = 0b00000000;
    Kernel::MPU::set_rasr(rasr);

    {
        auto ctrl = Kernel::MPU::ctrl();
        ctrl.enable = 1;
        ctrl.privdefena = 1;

        dump_mpu();
        Kernel::MPU::set_ctrl(ctrl);
    }

    // If we hit the breakpoint, that proves that we can execute from the flash
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("bkpt #0");
}

static void try_out_mpu()
{
    // try_configure_mpu_1();
    // try_configure_mpu_2();
    try_configure_mpu_3();
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
