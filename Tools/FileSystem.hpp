#pragma once

#include <span>
#include <optional>

#include <LibElf/Generator.hpp>
#include <LibElf/SymbolTable.hpp>
#include <LibElf/RelocationTable.hpp>

class FileSystem {
public:
    explicit FileSystem(Elf::Generator& generator);
    ~FileSystem();

    void add_file(std::string_view, std::span<const uint8_t>);

    void finalize();

private:
    Elf::Generator& m_generator;
    bool m_finalized = false;

    std::optional<Elf::RelocationTable> m_data_relocs;
    std::optional<Elf::RelocationTable> m_tab_relocs;
    std::optional<size_t> m_data_index;
    std::optional<size_t> m_tab_index;

    Elf::MemoryStream m_data_stream;
    Elf::MemoryStream m_tab_stream;

    size_t m_next_inode = 3;
};