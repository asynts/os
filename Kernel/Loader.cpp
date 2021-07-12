#include <Std/Format.hpp>

#include <Kernel/Loader.hpp>
#include <Kernel/GlobalMemoryAllocator.hpp>
#include <Kernel/HandlerMode.hpp>
#include <Kernel/PageAllocator.hpp>
#include <Kernel/Threads/Scheduler.hpp>

namespace Kernel
{
    LoadedExecutable load_executable_into_memory(ElfWrapper elf, Thread& thread)
    {
        PageAllocator::the().dump();

        LoadedExecutable executable;
        executable.m_host_path = elf.m_host_path;

        VERIFY(elf.header()->e_phnum == 3);
        VERIFY(elf.segments()[2].p_type == PT_ARM_EXIDX);

        auto& readonly_segment = elf.segments()[0];
        VERIFY(readonly_segment.p_type == PT_LOAD);
        VERIFY(readonly_segment.p_flags == (PF_R | PF_X));

        auto& writable_segment = elf.segments()[1];
        VERIFY(writable_segment.p_type == PT_LOAD);
        VERIFY(writable_segment.p_flags == (PF_R | PF_W));

        VERIFY(readonly_segment.p_memsz == readonly_segment.p_filesz);
        executable.m_readonly_size = readonly_segment.p_memsz;
        executable.m_readonly_base = elf.base_as_u32() + readonly_segment.p_offset;

        executable.m_writable_size = round_to_power_of_two(writable_segment.p_memsz);
        auto owned_writable_range = PageAllocator::the().allocate(power_of_two(executable.m_writable_size)).must();
        executable.m_writable_base = owned_writable_range.m_range->m_base;
        VERIFY(owned_writable_range.size() == executable.m_writable_size);
        thread.m_owned_page_ranges.append(move(owned_writable_range));

        __builtin_memcpy((u8*)executable.m_writable_base, elf.base() + writable_segment.p_offset, writable_segment.p_filesz);

        VERIFY(writable_segment.p_memsz >= writable_segment.p_filesz);
        __builtin_memset((u8*)executable.m_writable_base + writable_segment.p_filesz, 0, writable_segment.p_memsz - writable_segment.p_filesz);

        VERIFY(elf.header()->e_entry >= readonly_segment.p_vaddr);
        VERIFY(elf.header()->e_entry - readonly_segment.p_vaddr < readonly_segment.p_memsz);
        executable.m_entry = executable.m_readonly_base + (elf.header()->e_entry - readonly_segment.p_vaddr);

        executable.m_text_base = 0;
        executable.m_data_base = 0;
        executable.m_stack_base = 0;
        executable.m_bss_base = 0;
        for (usize section_index = 1; section_index < elf.header()->e_shnum; ++section_index) {
            auto& section = elf.sections()[section_index];

            if (__builtin_strcmp(elf.section_name_base() + section.sh_name, ".stack") == 0) {
                executable.m_stack_base = executable.m_writable_base + (section.sh_addr - writable_segment.p_vaddr);
                executable.m_stack_size = section.sh_size;

                // FIXME: I don't think we need this anymore with our own LibC
                ASSERT(executable.m_stack_size == 0x1100);

                continue;
            }
            if (__builtin_strcmp(elf.section_name_base() + section.sh_name, ".data") == 0) {
                executable.m_data_base = executable.m_writable_base + (section.sh_addr - writable_segment.p_vaddr);
                continue;
            }
            if (__builtin_strcmp(elf.section_name_base() + section.sh_name, ".bss") == 0) {
                executable.m_bss_base = executable.m_writable_base + (section.sh_addr - writable_segment.p_vaddr);
                continue;
            }

            if (__builtin_strcmp(elf.section_name_base() + section.sh_name, ".text") == 0) {
                executable.m_text_base = executable.m_readonly_base + (section.sh_addr - readonly_segment.p_vaddr);
                continue;
            }
        }
        VERIFY(executable.m_text_base);
        VERIFY(executable.m_data_base);
        VERIFY(executable.m_stack_base);
        VERIFY(executable.m_bss_base);

        dbgln("Loaded executable:");
        dbgln("  m_readonly_base={}", executable.m_readonly_base);
        dbgln("  m_writable_base={}", executable.m_writable_base);

        dbgln("GDB: add-symbol-file {} -s .text {} -s .data {} -s .bss {}",
            executable.m_host_path,
            executable.m_text_base,
            executable.m_data_base,
            executable.m_bss_base);

        return move(executable);
    }

    void setup_mpu(Vector<MPU::Region>& regions)
    {
        // FIXME: We should never have uninitialized regions
        if (regions.size() == 0) {
            // FIXME: This actually happens sometimes, what is going on?
            if (debug_loader)
                dbgln("[setup_mpu] Uninitialized regions, disabling MPU");
            auto ctrl = MPU::ctrl();
            ctrl.enable = 0;
            MPU::set_ctrl(ctrl);
            return;
        }

        mpu_hw->ctrl = 0;

        for (size_t index = 0; index < 8; ++index) {
            mpu_hw->rnr = index;
            mpu_hw->rbar = 0;
            mpu_hw->rasr = 0;
        }

        VERIFY(regions.size() <= 8);
        for (size_t index = 0; index < regions.size(); ++index) {
            VERIFY((regions[index].rbar.raw & 0b11111) == 0);
            mpu_hw->rbar = regions[index].rbar.raw | 1 << 4 | u32(index);

            auto rasr_active = MPU::rasr();
            auto rasr = regions[index].rasr;
            rasr.reserved_1 = rasr_active.reserved_1;
            rasr.reserved_2 = rasr_active.reserved_2;
            rasr.reserved_3 = rasr_active.reserved_3;
            rasr.reserved_4 = rasr_active.reserved_4;
            MPU::set_rasr(rasr);

            if (debug_loader) {
                dbgln("[setup_mpu] Initialized region region_base_address_register={} region_attribute_and_size_register={}",
                    regions[index].rbar.raw,
                    regions[index].rasr.raw);
            }
        }

        mpu_hw->ctrl = M0PLUS_MPU_CTRL_PRIVDEFENA_BITS
                     | M0PLUS_MPU_CTRL_HFNMIENA_RESET
                     | M0PLUS_MPU_CTRL_ENABLE_BITS;

        // FIXME: Does the MPU become active imediatelly, or do we have to poll here?

        if (debug_loader) {
            dbgln("[setup_mpu] Enabled MPU with {} regions", regions.size());

            MPU::dump();
        }
    }

    // FIXME: We are taking the wrong parameters here, take a thread? Cooperate with the scheduler?
    void hand_over_to_loaded_executable(const LoadedExecutable& executable, StackWrapper stack, Vector<MPU::Region> &regions, i32 argc, char **argv, char **envp)
    {
        VERIFY(is_executing_in_thread_mode());
        VERIFY(is_executing_privileged());

        setup_mpu(regions);

        Scheduler::the().active().m_privileged = false;

        // FIXME: Free old stack?!
        // FIXME: Make sure to drop the corresponding region as well

        asm volatile("msr psp, %0;"
                     "msr control, %1;"
                     "isb;"
                     "mov sb, %2;"
                     "mov r0, %4;"
                     "mov r1, %5;"
                     "mov r2, %6;"
                     "bx %3;"
            :
            : "r"(stack.top()), // FIXME: This is wrong!
              "r"(0b11),
              "r"(executable.m_writable_base),
              "r"(executable.m_entry),
              "r"(argc),
              "r"(argv),
              "r"(envp)
            : "r0", "r1", "r2", "sb");
    }
}
