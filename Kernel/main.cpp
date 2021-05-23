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

union MPU_CTRL {
    struct {
        u32 enable : 1;
        u32 hfnmiena : 1;
        u32 privdefena : 1;
        u32 reserved_1 : 29;
    };
    u32 raw;
};

union MPU_RBAR {
    struct {
        u32 region : 4;
        u32 valid : 1;
        u32 addr : 27;
    };
    u32 raw;
};

struct MPU_REGION {
    MPU_RASR rasr;
    MPU_RBAR rbar;
};

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

static void try_configure_mpu_1()
{
    dbgln("[try_configure_mpu_1]");

    mpu_hw->ctrl = 0;

    dump_mpu();

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
}

static void try_configure_mpu_2()
{
    dbgln("[try_configure_mpu_2]");

    MPU_CTRL ctrl;

    ctrl = static_cast<MPU_CTRL>(mpu_hw->ctrl);
    ctrl.enable = 0;

    dbgln("[{}] {}:{}", __PRETTY_FUNCTION__, __FILE__, __LINE__);

    mpu_hw->ctrl = ctrl.raw;

    dbgln("[{}] {}:{}", __PRETTY_FUNCTION__, __FILE__, __LINE__);

    mpu_hw->rnr = 0;

    dbgln("[{}] {}:{}", __PRETTY_FUNCTION__, __FILE__, __LINE__);

    mpu_hw->rbar = 0x10000000;

    dbgln("[{}] {}:{}", __PRETTY_FUNCTION__, __FILE__, __LINE__);

    auto rasr = static_cast<MPU_RASR>(mpu_hw->rasr);
    rasr.enable = 1;
    rasr.size = 20;
    rasr.srd = 0b11111111; // FIXME: Try 0b00000000
    rasr.attrs_b = 1;
    rasr.attrs_c = 1;
    rasr.attrs_s = 1;
    rasr.attrs_ap = 0b111;
    rasr.attrs_xn = 0;
    mpu_hw->rasr = rasr.raw;

    dbgln("[{}] {}:{}", __PRETTY_FUNCTION__, __FILE__, __LINE__);

    ctrl = static_cast<MPU_CTRL>(mpu_hw->ctrl);

    dbgln("[{}] {}:{}", __PRETTY_FUNCTION__, __FILE__, __LINE__);

    ctrl.enable = 1;
    ctrl.hfnmiena = 0;
    ctrl.privdefena = 0;

    dbgln("CTRL={}", ctrl.raw);
    mpu_hw->ctrl = ctrl.raw;

    dbgln("[{}] {}:{}", __PRETTY_FUNCTION__, __FILE__, __LINE__);
}

static void try_configure_mpu_3()
{
    dump_mpu();

    MPU_CTRL ctrl;

    ctrl = static_cast<MPU_CTRL>(mpu_hw->ctrl);
    ctrl.enable = 0;
    mpu_hw->ctrl = ctrl.raw;

    for (size_t i = 0; i < 8; ++i) {
        mpu_hw->rnr = i;
        mpu_hw->rbar = 0x00000000;
        mpu_hw->rasr = 0;
    }

    mpu_hw->rnr = 0;

    // FIXME: It seems this address is provided incorrectly?
    mpu_hw->rbar = 0x10000000;

    auto rasr = static_cast<MPU_RASR>(mpu_hw->rasr);
    rasr.attrs_ap = 0b111;
    rasr.attrs_b = 1;
    rasr.attrs_c = 1;
    rasr.attrs_s = 1;
    rasr.attrs_tex = 0b000;
    rasr.attrs_xn = 1; // FIXME: This should prevent execution
    rasr.size = 20;
    rasr.enable = 1;
    rasr.srd = 0b11111111;
    mpu_hw->rasr = rasr.raw;

    ctrl = static_cast<MPU_CTRL>(mpu_hw->ctrl);
    ctrl.enable = 1;
    ctrl.privdefena = 1;

    dump_mpu();
    mpu_hw->ctrl = ctrl.raw;

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
