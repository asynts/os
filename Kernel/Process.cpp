#include <Kernel/Process.hpp>
#include <Kernel/Scheduler.hpp>
#include <Kernel/Loader.hpp>
#include <Kernel/Interface/System.hpp>
#include <Kernel/FileSystem/FlashFileSystem.hpp>
#include <Kernel/FileSystem/MemoryFileSystem.hpp>

namespace Kernel
{
    Process& Process::active_process()
    {
        auto& thread = Scheduler::the().active_thread();
        return thread.m_process.must();
    }

    Process& Process::create(StringView name, ElfWrapper elf)
    {
        Vector<String> arguments;
        arguments.append(name);

        Vector<String> variables;

        return Process::create(name, elf, arguments, variables);
    }
    Process& Process::create(StringView name, ElfWrapper elf, const Vector<String>& arguments, const Vector<String>& variables)
    {
        // FIXME: Allocate argv/envp on the stack

        i32 argc = arguments.size();

        auto *argv = new Vector<char*>;
        for (auto& argument : arguments.iter())
            argv->append((new String { argument })->data());
        argv->append(nullptr);

        auto *envp = new Vector<char*>;
        for (auto& variable : variables.iter())
            envp->append((new String { variable })->data());
        envp->append(nullptr);

        auto process = make<Process>(name);

        Thread thread { String::format("Process: {}", name), move(process) };

        return Scheduler::the().create_thread(move(thread), [name, elf, argc, argv, envp] () mutable {
            dbgln("Loading executable for process '{}' from {}", name, elf.base());

            auto& process = Process::active_process();

            process.m_executable = load_executable_into_memory(elf);
            auto& executable = process.m_executable.must();

            auto& thread = Scheduler::the().active_thread();
            VERIFY(thread.m_regions.size() == 0);

            // Flash
            thread.m_regions.append(Region {
                .rbar = {
                    .region = 0,
                    .valid = 0,
                    .addr = 0x10000000 >> 8,
                },
                .rasr = {
                    .enable = 1,
                    .size = 20,
                    .srd = 0b00000000,
                    .attrs_b = 1,
                    .attrs_c = 1,
                    .attrs_s = 1,
                    .attrs_tex = 0b000,
                    .attrs_ap = 0b111,
                    .attrs_xn = 0,
                },
            });

            // RAM
            VERIFY(__builtin_popcount(executable.m_writable_size) == 1);
            VERIFY(executable.m_writable_base % executable.m_writable_size == 0);
            thread.m_regions.append(Region {
                .rbar = {
                    .region = 0,
                    .valid = 0,
                    .addr = executable.m_writable_base >> 8,
                },
                .rasr = {
                    .enable = 1,
                    .size = MPU::compute_size(executable.m_writable_size),
                    .srd = 0b00000000,
                    .attrs_b = 1,
                    .attrs_c = 1,
                    .attrs_s = 1,
                    .attrs_tex = 0b000,
                    .attrs_ap = 0b011,
                    .attrs_xn = 1,
                },
            });

            // ROM
            thread.m_regions.append(Region {
                .rbar = {
                    .region = 0,
                    .valid = 0,
                    .addr = 0b00000000 >> 8,
                },
                .rasr = {
                    .enable = 1,
                    .size = 13,
                    .svd = 0b00000000,
                    .attrs_b = 1,
                    .attrs_c = 1,
                    .attrs_s = 1,
                    .attrs_tex = 0b000,
                    .attrs_ap = 0b111,
                    .attrs_xn = 0,
                },
            });

            dbgln("Handing over execution to process '{}' at {}", name, process.m_executable.must().m_entry);
            dbgln("  Got argv={} and envp={}", argv->data(), envp->data());

            hand_over_to_loaded_executable(process.m_executable.must(), thread.m_regions, argc, argv->data(), envp->data());

            VERIFY_NOT_REACHED();
        }).m_process.must();
    }

    i32 Process::sys$read(i32 fd, u8 *buffer, usize count)
    {
        if (fd > 2)
            dbgln("[Process::sys$read] fd={} buffer={} count={}", fd, buffer, count);

        auto& handle = get_file_handle(fd);
        return handle.read({ buffer, count }).must();
    }

    i32 Process::sys$write(i32 fd, const u8 *buffer, usize count)
    {
        if (fd > 2)
            dbgln("[Process::sys$write] fd={} buffer={} count={}", fd, buffer, count);

        auto& handle = get_file_handle(fd);
        return handle.write({ buffer, count }).must();
    }

    i32 Process::sys$open(const char *pathname, u32 flags, u32 mode)
    {
        dbgln("[Process::sys$open] pathname={} flags={} mode={}", pathname, flags, mode);

        Path path = pathname;

        if (!path.is_absolute())
            path = m_working_directory / path;

        auto file_opt = Kernel::FileSystem::try_lookup(path);

        if (file_opt.is_error()) {
            if (file_opt.error() == ENOENT && (flags & O_CREAT)) {
                dbgln("[sys$open] path={}", path);
                dbgln("[sys$open] path={} parent={}", path, path.parent());

                auto parent_opt = Kernel::FileSystem::try_lookup(path.parent());

                if (parent_opt.is_error())
                    return -ENOTDIR;

                auto& new_file = *new Kernel::MemoryFile;
                dynamic_cast<Kernel::VirtualDirectory*>(parent_opt.value())->m_entries.set(path.filename(), &new_file);

                auto& new_handle = new_file.create_handle();
                return add_file_handle(new_handle);
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
                return -EISDIR;
            }

            file->truncate();
        }

        auto& handle = file->create_handle();
        return add_file_handle(handle);
    }

    i32 Process::sys$close(i32 fd)
    {
        dbgln("[Process::sys$close] fd={}", fd);

        // FIXME

        return 0;
    }

    i32 Process::sys$fstat(i32 fd, UserlandFileInfo *statbuf)
    {
        dbgln("[Process::sys$fstat] fd={} statbuf={}", fd, statbuf);

        auto& handle = get_file_handle(fd);
        auto& file = handle.file();

        statbuf->st_dev = FileSystemId::Invalid;
        statbuf->st_rdev = 0xdead;
        statbuf->st_size = 0xdead;
        statbuf->st_blksize = 0xdead;
        statbuf->st_blocks = 0xdead;

        statbuf->st_ino = file.m_ino;
        statbuf->st_mode = file.m_mode;
        statbuf->st_uid = file.m_owning_user;
        statbuf->st_gid = file.m_owning_group;

        return 0;
    }

    i32 Process::sys$wait(i32 *status)
    {
        if (m_terminated_children.size() > 0) {
            auto terminated_child_process = m_terminated_children.dequeue();
            *status = terminated_child_process.m_status;

            return terminated_child_process.m_process_id;
        }

        Scheduler::the().donate_my_remaining_cpu_slice();
        return -EINTR;
    }

    i32 Process::sys$exit(i32 status)
    {
        if (m_parent) {
            m_parent->m_terminated_children.enqueue({ m_process_id, status });

            ASSERT(m_parent->m_terminated_children.size() > 0);
        }

        Scheduler::the().terminate_active_thread();

        // Since we are in handler mode, we can't instantly terminate but instead
        // have to leave handler mode for PendSV to fire.
        //
        // Returning normally should clean up the stack and should be functionally
        // equivalent to doing the complex stuff required to terminate instantly.
        return -1;
    }

    i32 Process::sys$chdir(const char *pathname)
    {
        Path path { pathname };

        if (!path.is_absolute())
            path = m_working_directory / path;

        auto file_opt = Kernel::FileSystem::try_lookup(path);

        if (file_opt.is_error())
            return -file_opt.error();

        if ((file_opt.value()->m_mode & ModeFlags::Format) != ModeFlags::Directory)
            return -ENOTDIR;

        m_working_directory = path;

        return 0;
    }

    i32 Process::sys$posix_spawn(
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
            path = m_working_directory / path;

        HashMap<String, String> system_to_host;
        system_to_host.set("/bin/Shell.elf", "Userland/Shell.1.elf");
        system_to_host.set("/bin/Example.elf", "Userland/Example.1.elf");
        system_to_host.set("/bin/Editor.elf", "Userland/Editor.1.elf");

        auto& file = dynamic_cast<FlashFile&>(FileSystem::lookup(path));
        ElfWrapper elf { file.m_data.data(), system_to_host.get_opt(path.string()).must() };

        auto& new_process = Kernel::Process::create(pathname, move(elf), arguments, environment);
        new_process.m_parent = this;
        new_process.m_working_directory = m_working_directory;

        dbgln("[Process::sys$posix_spawn] Created new process PID {} running {}", new_process.m_process_id, path);

        *pid = new_process.m_process_id;
        return 0;
    }

    i32 Process::sys$get_working_directory(u8 *buffer, usize *buffer_size)
    {
        auto string = m_working_directory.string();

        if (string.size() + 1 > *buffer_size) {
            *buffer_size = string.size() + 1;
            return -ERANGE;
        } else {
            string.strcpy_to({ (char*)buffer, *buffer_size });
            *buffer_size = string.size() + 1;
            return 0;
        }
    }
}
