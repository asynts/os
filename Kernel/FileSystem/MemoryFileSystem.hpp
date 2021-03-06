#pragma once

#include <Std/Singleton.hpp>

#include <Kernel/FileSystem/VirtualFileSystem.hpp>
#include <Kernel/Interface/Types.hpp>

namespace Kernel
{
    class MemoryFileSystem;
    class MemoryFile;
    class MemoryDirectory;
    class MemoryFileHandle;

    class MemoryFileSystem final
        : public Singleton<MemoryFileSystem>
        , public VirtualFileSystem
    {
    public:
        VirtualFile& root() override;

        u32 next_ino() { return m_next_ino++; }

    private:
        friend Singleton<MemoryFileSystem>;
        MemoryFileSystem();

        MemoryDirectory *m_root;
        u32 m_next_ino = 2;
    };

    class MemoryFile final : public VirtualFile {
    public:
        MemoryFile()
        {
            m_filesystem = FileSystemId::Ram;
            m_ino = MemoryFileSystem::the().next_ino();
            m_mode = ModeFlags::Regular;
            m_size = 0;
        }

        ReadonlyBytes span() const { return m_data.span(); }
        Bytes span() { return m_data.span(); }

        VirtualFileHandle& create_handle_impl() override;

        void truncate() override
        {
            m_data.clear();
            m_size = 0;
        }

        void append(ReadonlyBytes bytes)
        {
            m_data.extend(bytes);
            m_size += bytes.size();
        }

    private:
        Vector<u8> m_data;
    };

    class MemoryFileHandle final : public VirtualFileHandle {
    public:
        explicit MemoryFileHandle(MemoryFile& file)
            : m_file(file)
            , m_offset(0)
        {
        }

        KernelResult<usize> read(Bytes bytes) override
        {
            usize nread = m_file.span().slice(m_offset).copy_trimmed_to(bytes);
            m_offset += nread;

            return nread;
        }
        KernelResult<usize> write(ReadonlyBytes bytes) override
        {
            m_file.append(bytes);
            m_offset += bytes.size();
            return bytes.size();
        }

        VirtualFile& file() override { return m_file; }

        MemoryFile& m_file;
        usize m_offset;
    };

    class MemoryDirectory final : public VirtualDirectory {
    public:
        MemoryDirectory()
        {
            m_filesystem = FileSystemId::Ram;
            m_ino = MemoryFileSystem::the().next_ino();
            m_mode = ModeFlags::Directory;
            m_size = 0xdead;

            m_entries.set(".", this);
            m_entries.set("..", this);
        }

        VirtualFileHandle& create_handle_impl() override;
    };

    class MemoryDirectoryHandle final : public VirtualFileHandle {
    public:
        explicit MemoryDirectoryHandle(MemoryDirectory& directory)
            : m_iterator(directory.m_entries.iter())
            , m_directory(directory)
        {
        }

        KernelResult<usize> read(Bytes bytes) override
        {
            ASSERT(bytes.size() == sizeof(UserlandDirectoryInfo));

            if (m_iterator.begin() == m_iterator.end())
                return KernelResult<usize>::from_value(0);

            auto& [name, file] = *m_iterator++;

            UserlandDirectoryInfo info;
            info.d_ino = file.must()->m_ino;
            name.strcpy_to({ info.d_name, sizeof(info.d_name) });
            return bytes_from(info).copy_to(bytes);
        }
        KernelResult<usize> write(ReadonlyBytes bytes) override
        {
            VERIFY_NOT_REACHED();
        }

        VirtualFile& file() override { return m_directory; }

        decltype(MemoryDirectory::m_entries)::Iterator m_iterator;
        MemoryDirectory& m_directory;
    };
}
