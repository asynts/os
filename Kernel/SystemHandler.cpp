#include <Std/Forward.hpp>
#include <Kernel/Interface/syscalls.h>
#include <Kernel/ConsoleDevice.hpp>
#include <Kernel/FileSystem/MemoryFileSystem.hpp>
#include <Kernel/File.hpp>
#include <Kernel/Process.hpp>

#define STDIN_FILENO 0
#define STDOUT_FILENO 1

class TypeErasedArgument {
public:
    i32 fd() const { return *reinterpret_cast<const i32*>(&m_value); }
    usize size() const { return m_value; }

    template<typename T>
    T* pointer() { return reinterpret_cast<T*>(m_value); }

    template<typename T>
    T value() { return static_cast<T>(m_value); }

private:
    u32 m_value;
};

extern "C"
isize syscall_handler(u32 syscall, TypeErasedArgument arg1, TypeErasedArgument arg2, TypeErasedArgument arg3)
{
    if (syscall == _SC_read) {
        i32 fd = arg1.fd();
        auto *buffer = arg2.pointer<u8>();
        usize count = arg3.size();

        if (fd > 2)
            dbgln("[syscall_handler] read(%, %, %)", fd, buffer, count);

        auto& handle = Kernel::Process::current().get_file_handle(fd);
        auto nread =  handle.read({ buffer, count });

        return nread;
    }

    if (syscall == _SC_write) {
        i32 fd = arg1.fd();
        auto *buffer = arg2.pointer<const u8>();
        usize count = arg3.size();

        if (fd > 2)
            dbgln("[syscall_handler] write(%, %, %)", fd, buffer, count);

        auto& handle = Kernel::Process::current().get_file_handle(fd);
        return handle.write({ buffer, count });
    }

    if (syscall == _SC_open) {
        const char *path = arg1.pointer<const char>();
        u32 flags = arg2.value<u32>();
        u32 mode = arg3.value<u32>();

        dbgln("[syscall_handler] open(%, %, %)", path, flags, mode);

        auto absolute_path = Kernel::compute_absolute_path(path);

        auto& entry = Kernel::MemoryFileSystem::the().lookup_path(absolute_path.view());

        if (flags & O_DIRECTORY) {
            if (entry.m_info->m_mode & S_IFMT != S_IFDIR)
                return -ENOTDIR;
        }

        auto& file = *new Kernel::File { entry };
        auto& handle = file.create_handle();
        i32 fd = Kernel::Process::current().add_file_handle(handle);

        return fd;
    }

    if (syscall == _SC_close) {
        dbgln("[syscall_handler] _SC_close not implemented");
        return 0;
    }

    VERIFY_NOT_REACHED();
}
