#include <map>

#include <sys/stat.h>
#include <bsd/string.h>
#include <assert.h>

#include <fmt/format.h>

#include <LibElf/MemoryStream.hpp>

#include "FileSystem.hpp"

// FIXME: This is a huge mess, I need to completely rewrite this

FileSystem::FileSystem(Elf::Generator& generator)
    : m_generator(generator)
{
    m_data_index = m_generator.create_section(".embed", SHT_PROGBITS, SHF_ALLOC);
    m_data_relocs.emplace(m_generator, ".embed", generator.symtab().symtab_index(), *m_data_index);

    m_base_symbol = m_generator.symtab().add_symbol("__flash_base", Elf32_Sym {
        .st_value = 0,
        .st_size = 0,
        .st_info = ELF32_ST_INFO(STB_GLOBAL, STT_OBJECT),
        .st_other = STV_DEFAULT,
        .st_shndx = static_cast<uint16_t>(*m_data_index),
    });
}
FileSystem::~FileSystem()
{
    assert(m_finalized);
}
uint32_t FileSystem::add_host_file(std::string_view path, Kernel::ModeFlags mode)
{
    // FIXME: We don't have to map the file into memory
    Elf::MemoryStream stream;
    stream.write_bytes(Elf::mmap_file(path));

    return add_file(stream, mode);
}
uint32_t FileSystem::add_directory(std::map<std::string, uint32_t>& files, uint32_t inode_number)
{
    Elf::MemoryStream stream;

    std::vector<Elf32_Rel> directory_relocations;

    for (auto& [name, inode] : files) {
        Kernel::FlashDirectoryInfo info;

        strlcpy(info.m_name, name.c_str(), sizeof(info.m_name));
        info.m_info_raw = m_inode_to_offset[inode];

        size_t info_offset = stream.write_object(info);

        directory_relocations.push_back(Elf32_Rel {
            .r_offset = (uint32_t) (info_offset + offsetof(Kernel::FlashDirectoryInfo, m_info_raw)),
            .r_info = ELF32_R_INFO(*m_base_symbol, R_ARM_ABS32),
        });
    }

    size_t load_offset;
    inode_number = add_file(stream, Kernel::ModeFlags::Directory, inode_number, &load_offset);

    for (auto& relocation : directory_relocations) {
        relocation.r_offset += load_offset;
        m_data_relocs->add_entry(relocation);
    }

    return inode_number;
}
uint32_t FileSystem::add_root_directory(std::map<std::string, uint32_t>& files)
{
    return add_directory(files, 2);
}
uint32_t FileSystem::add_file(Elf::MemoryStream& stream, Kernel::ModeFlags mode, uint32_t inode_number, size_t *load_offset)
{
    if (inode_number == 0)
        inode_number = m_next_inode++;

    size_t data_offset = m_data_stream.write_bytes(stream);

    if (load_offset)
        *load_offset = data_offset;

    size_t data_symbol = m_generator.symtab().add_symbol(fmt::format("__flash_data_{}", inode_number), Elf32_Sym {
        .st_value = (uint32_t)data_offset,
        .st_size = (uint32_t)stream.size(),
        .st_info = ELF32_ST_INFO(STB_GLOBAL, STT_OBJECT),
        .st_other = STV_DEFAULT,
        .st_shndx = static_cast<uint16_t>(*m_data_index),
    });

    Kernel::FileInfo inode;
    inode.st_dev = Kernel::FileSystemId::Flash;
    inode.st_ino = inode_number;
    inode.st_mode = mode;
    inode.st_rdev = 0;
    inode.st_size = stream.size();
    inode.st_uid = 0;
    inode.st_gid = 0;

    // FIXME
    inode.st_blksize = 0;
    inode.st_blocks = 0;

    std::vector<Elf32_Rel> inode_relocations;

    inode.m_data = 0;
    inode_relocations.push_back(Elf32_Rel {
        .r_offset = offsetof(Kernel::FileInfo, m_data),
        .r_info = ELF32_R_INFO(data_symbol, R_ARM_ABS32),
    });

    size_t inode_offset = m_data_stream.write_object(inode);
    size_t inode_symbol = m_generator.symtab().add_symbol(fmt::format("__flash_info_{}", inode_number), Elf32_Sym {
        .st_value = static_cast<uint32_t>(inode_offset),
        .st_size = sizeof(Kernel::FileInfo),
        .st_info = ELF32_ST_INFO(STB_GLOBAL, STT_OBJECT),
        .st_other = STV_DEFAULT,
        .st_shndx = static_cast<uint16_t>(*m_data_index),
    });

    if (inode_number == 2) {
        m_generator.symtab().add_symbol("__flash_root", Elf32_Sym {
            .st_value = static_cast<uint32_t>(inode_offset),
            .st_size = sizeof(Kernel::FileInfo),
            .st_info = ELF32_ST_INFO(STB_GLOBAL, STT_OBJECT),
            .st_other = STV_DEFAULT,
            .st_shndx = static_cast<uint16_t>(*m_data_index),
        });
    }

    for (Elf32_Rel& relocation : inode_relocations)
    {
        relocation.r_offset += inode_offset;
        m_data_relocs->add_entry(relocation);
    }

    m_inode_to_offset[inode_number] = inode_offset;

    return inode.st_ino;
}
void FileSystem::finalize()
{
    assert(!m_finalized);
    m_finalized = true;

    m_data_relocs->finalize();
    m_generator.write_section(m_data_index.value(), m_data_stream);
}
