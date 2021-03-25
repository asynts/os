#include <Kernel/DynamicLoader.hpp>
#include <Std/Format.hpp>

// FIXME: I want this to be a watchpoint, or a function, but not whatever this is.
// FIXME: The debugger assumes that this is the shell binary.
extern "C"
{
    volatile LoadedExecutable *volatile executable_for_debugger;
    [[gnu::noinline]]
    void inform_debugger_about_executable()
    {
        asm volatile("nop");
    }
}

LoadedExecutable load_executable_into_memory(ElfWrapper elf)
{
    LoadedExecutable executable;

    dbgln("Loading executable from %", elf.base());

    VERIFY(elf.header()->e_phnum == 3);
    VERIFY(elf.segments()[2].p_type == PT_ARM_EXIDX);

    Elf32_Phdr& text_segment = elf.segments()[0];
    VERIFY(text_segment.p_type == PT_LOAD);
    VERIFY(text_segment.p_flags == PF_R | PF_X);

    Elf32_Phdr& data_segment = elf.segments()[1];
    VERIFY(data_segment.p_type == PT_LOAD);
    VERIFY(data_segment.p_flags == PF_R | PF_W);

    executable.m_readonly_base = elf.base_as_u32() + text_segment.p_offset;
    dbgln("Putting readonly segment at % (inplace)", executable.m_readonly_base);

    u8 *data = new u8[data_segment.p_memsz];
    VERIFY(data != nullptr);

    dbgln("Allocated writable segment at % with size %", data, data_segment.p_memsz);

    executable.m_writable_base = u32(data);
    dbgln("Putting writable segment at % (allocated)", executable.m_writable_base);

    __builtin_memcpy(data, elf.base() + data_segment.p_offset, data_segment.p_filesz);

    VERIFY(data_segment.p_memsz >= data_segment.p_filesz);
    __builtin_memset(data + data_segment.p_filesz, 0, data_segment.p_memsz - data_segment.p_filesz);

    VERIFY(elf.header()->e_entry >= text_segment.p_vaddr);
    VERIFY(elf.header()->e_entry - text_segment.p_vaddr < text_segment.p_memsz);
    executable.m_entry = executable.m_readonly_base + (elf.header()->e_entry - text_segment.p_vaddr);
    dbgln("Putting entry point at %", executable.m_entry);

    executable.m_text_base = 0;
    executable.m_data_base = 0;
    executable.m_stack_base = 0;
    executable.m_bss_base = 0;
    for (usize section_index = 1; section_index < elf.header()->e_shnum; ++section_index) {
        Elf32_Shdr& section = elf.sections()[section_index];

        if (__builtin_strcmp(elf.section_name_base() + section.sh_name, ".stack") == 0) {
            executable.m_stack_base = executable.m_writable_base + section.sh_addr;
            executable.m_stack_size = 0x10000;
            continue;
        }
        if (__builtin_strcmp(elf.section_name_base() + section.sh_name, ".data") == 0) {
            executable.m_data_base = executable.m_writable_base + section.sh_addr;
            continue;
        }
        if (__builtin_strcmp(elf.section_name_base() + section.sh_name, ".bss") == 0) {
            executable.m_bss_base = executable.m_writable_base + section.sh_addr;
            continue;
        }

        if (__builtin_strcmp(elf.section_name_base() + section.sh_name, ".text") == 0) {
            executable.m_text_base = executable.m_readonly_base + section.sh_addr;
            continue;
        }
    }
    VERIFY(executable.m_text_base);
    VERIFY(executable.m_data_base);
    VERIFY(executable.m_stack_base);
    VERIFY(executable.m_bss_base);

    dbgln("Found text section at % in readonly segment", executable.m_text_base);
    dbgln("Found data section at % in writable segment", executable.m_data_base);
    dbgln("Found bss section at % in writable segment", executable.m_bss_base);
    dbgln("Found stack section at % in writable segment", executable.m_stack_base);

    executable_for_debugger = &executable;
    inform_debugger_about_executable();

    return executable;
}
