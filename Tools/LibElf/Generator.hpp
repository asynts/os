#pragma once

#include <string_view>
#include <vector>

#include <elf.h>

#include "MemoryStream.hpp"
#include "StringTable.hpp"
#include "SymbolTable.hpp"

namespace Elf
{
    class Generator {
    public:
        Generator();
        ~Generator();

        Elf32_Shdr& section(size_t index) { return m_sections[index]; }
        SymbolTable& symtab() { return m_symtab.value(); }

        size_t append_section(std::string_view name, MemoryStream& stream, Elf32_Word type, Elf32_Word flags);
        size_t create_section(std::string_view name, Elf32_Word type, Elf32_Word flags);
        void write_section(size_t section_index, MemoryStream&);

        MemoryStream finalize();

    private:
        void create_undefined_section();

        void encode_sections(size_t& section_offset, size_t& shstrtab_section_index);
        void encode_header(size_t section_offset, size_t shstrtab_section_index);

        MemoryStream m_stream;

        bool m_finalized = false;
        std::vector<Elf32_Shdr> m_sections;

        std::optional<StringTable> m_shstrtab;
        std::optional<SymbolTable> m_symtab;
    };
}
