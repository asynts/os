#pragma once

#include <Std/CircularQueue.hpp>
#include <Std/RefPtr.hpp>

#include <Kernel/Forward.hpp>
#include <Kernel/Threads/Thread.hpp>

namespace Kernel
{
    // FIXME: Must not be accessed in handler mode

    // FIXME: Public interface, enforce lock guards

    // FIXME: Syncronize lock and unlock themselves

    // FIXME: Fix semantics of Thread::block

    class KernelMutex
    {
    public:
        ~KernelMutex();

        void lock();
        void unlock();

    private:
        RefPtr<Thread> m_holding_thread;
        CircularQueue<RefPtr<Thread>, 16> m_waiting_threads;
    };
}
