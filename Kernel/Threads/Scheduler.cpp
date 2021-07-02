#include <Kernel/Threads/Scheduler.hpp>
#include <Kernel/Loader.hpp>
#include <Kernel/HandlerMode.hpp>

#include <hardware/structs/scb.h>
#include <hardware/structs/systick.h>

namespace Kernel
{
    extern "C"
    {
        FullRegisterContext* scheduler_next(FullRegisterContext *context)
        {
            if (debug_scheduler)
                dbgln("[scheduler_next]");

            Thread& active_thread = Scheduler::the().active();
            active_thread.stash_context(*context);

            Thread& next_thread = Scheduler::the().schedule();
            context = &next_thread.unstash_context();

            return context;
        }

        void isr_systick()
        {
            if (Scheduler::the().m_enabled)
                scb_hw->icsr = M0PLUS_ICSR_PENDSVSET_BITS;
        }
    }

    Scheduler::Scheduler()
        : m_default_thread(Thread::construct("Default Thread (Core 0)"))
    {
        m_default_thread->setup_context([] {
            for (;;) {
                asm volatile ("wfi");
            }
        });

        if (scheduler_slow)
            systick_hw->rvr = 0x00f00000;
        else
            systick_hw->rvr = 0x000f0000;

        systick_hw->csr = 1 << M0PLUS_SYST_CSR_CLKSOURCE_LSB
                        | 1 << M0PLUS_SYST_CSR_TICKINT_LSB
                        | 1 << M0PLUS_SYST_CSR_ENABLE_LSB;
    }

    Thread& Scheduler::schedule()
    {
        VERIFY(is_executing_in_handler_mode());

        if (!m_active_thread->m_die_at_next_opportunity) {
            m_queued_threads.enqueue(m_active_thread);
            m_active_thread = nullptr;
        } else {
            if (debug_scheduler)
                dbgln("[Scheduler::schedule] Dropping thread '{}' ({})", m_active_thread->m_name, m_active_thread);
        }

        // FIXME: This algorithm is a bit fishy

        bool all_threads_blocking = true;
        for (size_t i = 0; i < m_queued_threads.size(); ++i) {
            auto& thread = m_queued_threads[i];
            if (!thread->m_blocked && !thread->m_die_at_next_opportunity) {
                all_threads_blocking = false;
                break;
            }
        }

        RefPtr<Thread> next;

        if (all_threads_blocking) {
            next = m_default_thread;
        } else {
            for (;;) {
                next = m_queued_threads.dequeue();

                if (next->m_blocked) {
                    m_queued_threads.enqueue(next);
                } else if (next->m_die_at_next_opportunity) {
                    continue;
                } else {
                    break;
                }
            }
        }

        if (debug_scheduler)
            dbgln("[Scheduler::schedule] Switching to '{}' ({})", next->m_name, next);

        m_active_thread = next;

        if (m_active_thread->m_privileged) {
            asm volatile("msr control, %0;"
                         "isb;"
                :
                : "r"(0b10));
        } else {
            asm volatile("msr control, %0;"
                         "isb;"
                :
                : "r"(0b11));
        }

        setup_mpu(m_active_thread->m_regions);

        VERIFY(next != nullptr);
        return *next;
    }

    void Scheduler::trigger()
    {
        VERIFY(m_enabled);
        scb_hw->icsr = M0PLUS_ICSR_PENDSVSET_BITS;
        VERIFY_NOT_REACHED();
    }

    void Scheduler::loop()
    {
        auto dummy_thread = Thread::construct("Dummy");

        dummy_thread->setup_context([] {
            Scheduler::the().active().m_die_at_next_opportunity = true;
            Scheduler::the().m_enabled = true;
            Scheduler::the().trigger();

            VERIFY_NOT_REACHED();
        });

        m_active_thread = dummy_thread;

        FullRegisterContext& context = dummy_thread->unstash_context();

        u32 control = 0b10;
        asm volatile("msr control, %0;"
                     "isb;"
                     "mov r0, %1;"
                     "blx restore_context_from_thread_mode;"
            :
            : "r"(control), "r"(&context)
            : "r0");

        VERIFY_NOT_REACHED();
    }
}
