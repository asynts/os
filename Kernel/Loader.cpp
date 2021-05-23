#include <Std/Format.hpp>

#include <Kernel/Loader.hpp>
#include <Kernel/Scheduler.hpp>
#include <Kernel/GlobalMemoryAllocator.hpp>
#include <Kernel/HandlerMode.hpp>

#include <hardware/structs/mpu.h>

namespace Kernel
{
    // FIXME: Reference to pointer
    static void allocate_for_mpu(u8 **pointer, usize& size)
    {
        usize requested_size = size;
        usize rounded_size = round_to_power_of_two(requested_size);
        usize allocated_size = 2 * rounded_size;

        *pointer = new u8[allocated_size];
        *pointer += rounded_size - reinterpret_cast<u32>(*pointer) % rounded_size;

        size = rounded_size;

        VERIFY(u32(*pointer) % size == 0);
        VERIFY(__builtin_popcount(size) == 1);
    }

    LoadedExecutable load_executable_into_memory(ElfWrapper elf)
    {
        LoadedExecutable executable;
        executable.m_host_path = elf.m_host_path;

        VERIFY(elf.header()->e_phnum == 3);
        VERIFY(elf.segments()[2].p_type == PT_ARM_EXIDX);

        auto& readonly_segment = elf.segments()[0];
        VERIFY(readonly_segment.p_type == PT_LOAD);
        VERIFY(readonly_segment.p_flags == PF_R | PF_X);

        auto& writable_segment = elf.segments()[1];
        VERIFY(writable_segment.p_type == PT_LOAD);
        VERIFY(writable_segment.p_flags == PF_R | PF_W);

        executable.m_readonly_base = elf.base_as_u32() + readonly_segment.p_offset;
        executable.m_readonly_size = readonly_segment.p_memsz;

        executable.m_writable_size = writable_segment.p_memsz;
        allocate_for_mpu((u8**)&executable.m_writable_base, executable.m_writable_size);

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

    LoadedExecutable LoadedExecutable::clone()
    {
        LoadedExecutable copy;

        copy.m_writable_size = m_writable_size;
        copy.m_writable_base = u32(new u8[m_writable_size]);
        memcpy((void*)copy.m_writable_base, (void*)m_writable_base, m_writable_size);

        copy.m_readonly_size = m_readonly_size;
        copy.m_readonly_base = m_readonly_base;

        copy.m_entry = copy.m_readonly_base + (m_entry - m_readonly_base);

        copy.m_data_base = copy.m_writable_base + (m_data_base - m_writable_base);
        copy.m_text_base = copy.m_readonly_base + (m_text_base - m_readonly_base);
        copy.m_bss_base = copy.m_writable_base + (m_bss_base - m_writable_base);

        copy.m_stack_size = m_stack_size;
        copy.m_stack_base = u32(new u8[m_stack_size]);
        memcpy((void*)copy.m_stack_base, (void*)m_stack_base, m_stack_size);

        return copy;
    }

    void setup_mpu(Vector<Region>& regions)
    {
        // FIXME: We should never have uninitialized regions
        if (regions.size() == 0) {
            dbgln("[setup_mpu] Uninitialized regions, disabling MPU");
            mpu_hw->ctrl = 0;
            return;
        }

        mpu_hw->ctrl = 0;

        VERIFY(regions.size() <= 8);
        for (size_t index = 0; index < regions.size(); ++index) {
            mpu_hw->rnr = index;
            mpu_hw->rbar = regions[index].region_base_address_register();
            mpu_hw->rasr = regions[index].region_attribute_and_size_register();

            dbgln("[setup_mpu] Initialized region region_base_address_register={} region_attribute_and_size_register={}",
                regions[index].region_base_address_register(),
                regions[index].region_attribute_and_size_register());
        }

        mpu_hw->ctrl = M0PLUS_MPU_CTRL_PRIVDEFENA_BITS
                     | M0PLUS_MPU_CTRL_HFNMIENA_RESET
                     | M0PLUS_MPU_CTRL_ENABLE_BITS;

        // FIXME: Does the MPU become active imediatelly, or do we have to poll here?

        dbgln("[setup_mpu] Enabled MPU with {} regions", regions.size());
    }

    // FIXME: We are taking the wrong parameters here, take a thread? Cooperate with the scheduler?
    void hand_over_to_loaded_executable(const LoadedExecutable& executable, Vector<Region> &regions, i32 argc, char **argv, char **envp)
    {
        // FIXME: What happens to the current execution context when we hand over?

        // Avoid a ton of edge cases by executing in handler mode
        execute_in_handler_mode([&] {
            dbgln("[hand_over_to_loaded_executable] Calling 'setup_mpu'");
            setup_mpu(regions);
            dbgln("[hand_over_to_loaded_executable] Returned from 'setup_mpu'");

            StackWrapper stack { { (u8*)executable.m_stack_base, executable.m_stack_size } };
            stack.align(8);

            constexpr u32 xpsr_thumb_mode = 1 << 24;

            auto *context = stack.push_value(ExceptionRegisterContext{});
            context->pc.m_storage = executable.m_entry;
            context->xpsr.m_storage = xpsr_thumb_mode;
            context->r0.m_storage = bit_cast<u32>(argc);
            context->r1.m_storage = bit_cast<u32>(argv);
            context->r2.m_storage = bit_cast<u32>(envp);

            dbgln("[hand_over_to_loaded_executable] Setting up process stack pointer");

            // Setup stack pointer
            asm volatile(
                "msr psp, %0;"
                "isb;"
                :
                : "r"(stack.top()));

            dbgln("[hand_over_to_loaded_executable] Droping privileges");

            // Drop privileges, we continue to use the main stack pointer because we
            // are in handler mode
            asm volatile(
                "msr control, %0;"
                "isb;"
                :
                : "r"(0b11));

            // This is safe, because we are executing in handler mode
            Scheduler::the().active_thread().m_context.clear();
            Scheduler::the().active_thread().m_privileged = false;

            dbgln("[hand_over_to_loaded_executable] Handing over");

            // Hand over to executable
            asm volatile(
                "mov sb, %0;"
                "bx %1;"
                :
                : "r"(executable.m_writable_base), "r"(0xfffffffd)
                : "sb");

            VERIFY_NOT_REACHED();
        });
    }
}
