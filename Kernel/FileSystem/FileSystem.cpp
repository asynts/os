#include <Kernel/FileSystem/FileSystem.hpp>
#include <Kernel/FileSystem/MemoryFileSystem.hpp>

namespace Kernel::FileSystem
{
    VirtualDirectoryEntry& lookup(Path path)
    {
        VERIFY(path.is_absolute());

        VirtualDirectoryEntry *directory_entry = &MemoryFileSystem::the().root();

        for (auto& component : path.components()) {
            directory_entry->ensure_loaded();

            directory_entry = directory_entry->m_entries.lookup(component).must();
        }

        return *directory_entry;
    }
}
