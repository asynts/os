#include <Kernel/KernelMutex.hpp>

#include <Kernel/Threads/Scheduler.hpp>

namespace Kernel
{
    KernelMutex::~KernelMutex()
    {
        VERIFY(m_waiting_threads.size() == 0);
    }

    void KernelMutex::lock()
    {
        // We must not hold a strong reference here, otherwise, this thread could not be
        // terminated without being rescheduled. Not that this should happen, but...
        Thread& active_thread = Scheduler::the().active();

        if (m_holding_thread.is_null()) {
            m_holding_thread = active_thread;
        } else {
            m_waiting_threads.enqueue(active_thread);
            active_thread.block();
        }
    }

    void KernelMutex::unlock()
    {
        m_holding_thread.clear();

        if (m_waiting_threads.size() > 0) {
            RefPtr<Thread> next_thread = m_waiting_threads.dequeue();

            FIXME_ASSERT(m_holding_thread.is_null());
            m_holding_thread = next_thread;

            next_thread->wakeup();
        }
    }
}
