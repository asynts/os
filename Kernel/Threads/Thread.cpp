#include <Kernel/Threads/Thread.hpp>
#include <Kernel/Threads/Scheduler.hpp>
#include <Kernel/Interface/System.hpp>
#include <Kernel/Process.hpp>
#include <Kernel/HandlerMode.hpp>
#include <Kernel/FileSystem/MemoryFileSystem.hpp>
#include <Kernel/PageAllocator.hpp>
#include <Kernel/FileSystem/FlashFileSystem.hpp>

namespace Kernel
{
    Thread::Thread(String name)
        : m_name(move(name))
    {
        auto& flash_region = m_regions.append({});
        flash_region.rbar.region = 0;
        flash_region.rbar.valid = 0;
        flash_region.rbar.addr = 0x10000000 >> 5;
        flash_region.rasr.enable = 1;
        flash_region.rasr.size = 20;
        flash_region.rasr.srd = 0b00000000;
        flash_region.rasr.attrs_b = 1;
        flash_region.rasr.attrs_c = 1;
        flash_region.rasr.attrs_s = 1;
        flash_region.rasr.attrs_tex = 0b000;
        flash_region.rasr.attrs_ap = 0b111;
        flash_region.rasr.attrs_xn = 0;
    }

    OwnedPageRange Thread::allocate_stack_via_page_allocator()
    {
        return PageAllocator::the().allocate(PageAllocator::stack_power).must());
    }

    void Thread::setup_context_impl(StackWrapper stack_wrapper, void (*callback)(void*), void* argument)
    {
        constexpr u32 xpsr_thumb_mode = 1 << 24;

        FullRegisterContext context;

        context.xpsr.m_storage = xpsr_thumb_mode;
        context.pc.m_storage = u32(callback);
        context.lr.m_storage = 0;
        context.ip.m_storage = 0;
        context.r3.m_storage = 0;
        context.r2.m_storage = 0;
        context.r1.m_storage = 0;
        context.r0.m_storage = u32(argument);

        context.r4.m_storage = 0;
        context.r5.m_storage = 0;
        context.r6.m_storage = 0;
        context.r7.m_storage = 0;
        context.r8.m_storage = 0;
        context.r9.m_storage = 0;
        context.r10.m_storage = 0;
        context.r11.m_storage = 0;

        stack_wrapper.align(8);

        VERIFY(!m_stashed_context.is_valid());
        m_stashed_context = stack_wrapper.push_value(context);
    }

    void Thread::mark_blocked()
    {
        m_blocked = true;
    }

    void Thread::mark_unblocked()
    {
        m_blocked = false;
    }

    void Thread::block()
    {
        VERIFY(&Scheduler::the().active() == this);

        m_blocked = true;
        Scheduler::the().trigger();
    }

    void Thread::wakeup()
    {
        VERIFY(&Scheduler::the().active() != this);

        m_blocked = false;
        Scheduler::the().add_thread(*this);
    }

    Thread& Thread::active()
    {
        return Scheduler::the().active();
    }

    i32 Thread::syscall(u32 syscall, TypeErasedValue arg1, TypeErasedValue arg2, TypeErasedValue arg3)
    {
        auto *eargs = arg3.pointer<ExtendedSystemCallArguments>();

        switch (syscall) {
        case _SC_read:
            return sys$read(arg1.fd(), arg2.pointer<u8>(), arg3.value<usize>());
        case _SC_write:
            return sys$write(arg1.fd(), arg2.pointer<const u8>(), arg3.value<usize>());
        case _SC_open:
            return sys$open(arg1.cstring(), arg2.value<u32>(), arg3.value<u32>());
        case _SC_close:
            return sys$close(arg1.fd());
        case _SC_fstat:
            return sys$fstat(arg1.fd(), arg2.pointer<UserlandFileInfo>());
        case _SC_get_working_directory:
            return sys$get_working_directory(arg1.pointer<u8>(), arg2.pointer<usize>());
        case _SC_posix_spawn:
            return sys$posix_spawn(
                arg1.pointer<i32>(),
                arg2.cstring(),
                eargs->arg3.pointer<const UserlandSpawnFileActions>(),
                eargs->arg4.pointer<const UserlandSpawnAttributes>(),
                eargs->arg5.pointer<char*>(),
                eargs->arg6.pointer<char*>());
        case _SC_wait:
            return sys$wait(arg1.pointer<i32>());
        case _SC_exit:
            return sys$exit(arg1.value<i32>());
        case _SC_chdir:
            return sys$chdir(arg1.cstring());
        }

        FIXME();
    }

    i32 Thread::sys$read(i32 fd, u8 *buffer, usize count)
    {
        auto& handle = m_process->get_file_handle(fd);

        auto result = handle.read({ buffer, count });

        if (result.is_error()) {
            return -result.error();
        } else {
            return static_cast<i32>(result.value());
        }
    }

    i32 Thread::sys$write(i32 fd, const u8 *buffer, usize count)
    {
        auto& handle = m_process->get_file_handle(fd);

        auto result = handle.write({ buffer, count });

        if (result.is_error()) {
            return -result.error();
        } else {
            return static_cast<i32>(result.value());
        }
    }

    i32 Thread::sys$open(const char *pathname, u32 flags, u32 mode)
    {
        dbgln("[Process::sys$open] pathname={} flags={} mode={}", pathname, flags, mode);

        Path path = pathname;

        if (!path.is_absolute())
            path = m_process->m_working_directory / path;

        auto file_opt = Kernel::FileSystem::try_lookup(path);

        if (file_opt.is_error()) {
            if (file_opt.error() == ENOENT && (flags & O_CREAT)) {
                auto parent_opt = Kernel::FileSystem::try_lookup(path.parent());

                if (parent_opt.is_error()) {
                    dbgln("[Process::sys$open] error={}", ENOTDIR);
                    return -ENOTDIR;
                }

                auto& new_file = *new Kernel::MemoryFile;
                dynamic_cast<Kernel::VirtualDirectory*>(parent_opt.value())->m_entries.set(path.filename(), &new_file);

                auto& new_handle = new_file.create_handle();
                return m_process->add_file_handle(new_handle);
            }

            dbgln("[Process::sys$open] error={}", file_opt.error());
            return -file_opt.error();
        }

        VirtualFile *file = file_opt.value();

        if ((flags & O_DIRECTORY)) {
            if ((file->m_mode & ModeFlags::Format) != ModeFlags::Directory) {
                dbgln("[Process::sys$open] error={}", ENOTDIR);
                return -ENOTDIR;
            }
        }

        if ((flags & O_TRUNC)) {
            if ((file->m_mode & ModeFlags::Format) != ModeFlags::Regular) {
                ASSERT((file->m_mode & ModeFlags::Format) == ModeFlags::Directory);
                dbgln("[Process::sys$open] error={}", EISDIR);
                return -EISDIR;
            }

            file->truncate();
        }

        auto& handle = file->create_handle();
        return m_process->add_file_handle(handle);
    }

    i32 Thread::sys$close(i32 fd)
    {
        dbgln("[Process::sys$close] fd={}", fd);

        // FIXME

        return 0;
    }

    i32 Thread::sys$fstat(i32 fd, UserlandFileInfo *statbuf)
    {
        dbgln("[Process::sys$fstat] fd={} statbuf={}", fd, statbuf);

        auto& handle = m_process->get_file_handle(fd);

        // FIXME: For device files this will be incorrect
        auto& file = handle.file();

        statbuf->st_dev = file.m_filesystem;
        statbuf->st_rdev = file.m_device_id;
        statbuf->st_size = file.m_size;
        statbuf->st_blksize = 0xdead;
        statbuf->st_blocks = 0xdead;

        statbuf->st_ino = file.m_ino;
        statbuf->st_mode = file.m_mode;
        statbuf->st_uid = file.m_owning_user;
        statbuf->st_gid = file.m_owning_group;

        return 0;
    }

    i32 Thread::sys$get_working_directory(u8 *buffer, usize *buffer_size)
    {
        auto string = m_process->m_working_directory.string();

        if (string.size() + 1 > *buffer_size) {
            *buffer_size = string.size() + 1;
            return -ERANGE;
        } else {
            string.strcpy_to({ (char*)buffer, *buffer_size });
            *buffer_size = string.size() + 1;
            return 0;
        }
    }

    i32 Thread::sys$posix_spawn(
        i32 *pid,
        const char *pathname,
        const UserlandSpawnFileActions *file_actions,
        const UserlandSpawnAttributes *attrp,
        char **argv,
        char **envp)
    {
        FIXME_ASSERT(file_actions == nullptr);
        FIXME_ASSERT(attrp == nullptr);

        dbgln("sys$posix_spawn(%p, %s, %p, %p, %p, %p)", pid, pathname, file_actions, attrp, argv, envp);

        Vector<String> arguments;
        while (*argv != nullptr)
            arguments.append(*argv++);

        Vector<String> environment;
        while (*envp != nullptr)
            arguments.append(*envp++);

        Path path { pathname };

        if (!path.is_absolute())
            path = m_process->m_working_directory / path;

        HashMap<String, String> system_to_host;
        system_to_host.set("/bin/Shell.elf", "Userland/Shell.1.elf");
        system_to_host.set("/bin/Example.elf", "Userland/Example.1.elf");
        system_to_host.set("/bin/Editor.elf", "Userland/Editor.1.elf");

        auto& file = dynamic_cast<FlashFile&>(FileSystem::lookup(path));
        ElfWrapper elf { file.m_data.data(), system_to_host.get_opt(path.string()).must() };

        auto& new_process = Kernel::Process::create(pathname, move(elf), arguments, environment);
        new_process.m_parent = m_process;
        new_process.m_working_directory = m_process->m_working_directory;

        dbgln("[Process::sys$posix_spawn] Created new process PID {} running {}", new_process.m_process_id, path);

        *pid = new_process.m_process_id;
        return 0;
    }

    i32 Thread::sys$wait(i32 *status)
    {
        // FIXME: Syncronization
        for (;;) {
            if (m_process->m_terminated_children.size() > 0) {
                auto terminated_child_process = m_process->m_terminated_children.dequeue();
                *status = terminated_child_process.m_status;

                return terminated_child_process.m_process_id;
            }
        }
    }

    i32 Thread::sys$exit(i32 status)
    {
        dbgln("sys$exit({})", status);

        if (m_process->m_parent) {
            m_process->m_parent->m_terminated_children.enqueue({ m_process->m_process_id, status });
            ASSERT(m_process->m_parent->m_terminated_children.size() > 0);
        }

        m_die_at_next_opportunity = true;

        // We are currently executing in the worker thread. When the worker is done, it will unblock
        // the thread causing it to terminate and then terminate as well.
        return -1;
    }

    i32 Thread::sys$chdir(const char *pathname)
    {
        Path path { pathname };

        if (!path.is_absolute())
            path = m_process->m_working_directory / path;

        auto file_opt = Kernel::FileSystem::try_lookup(path);

        if (file_opt.is_error())
            return -file_opt.error();

        if ((file_opt.value()->m_mode & ModeFlags::Format) != ModeFlags::Directory)
            return -ENOTDIR;

        m_process->m_working_directory = path;

        return 0;
    }
}
