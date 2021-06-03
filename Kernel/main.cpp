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

int main()
{
    Kernel::ConsoleFile::the();

    dbgln("\e[0;1mBOOT\e[0m");

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
