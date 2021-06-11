#pragma once

#include <Std/Singleton.hpp>

#include <Kernel/FileSystem/VirtualFileSystem.hpp>
#include <Kernel/Scheduler.hpp>

namespace Kernel
{
    using namespace Std;

    struct Task {
        enum class Type {
            ThreadRead,
        } m_type;

        union Data {
            struct ThreadRead {
                VirtualFileHandle& m_handle;
                Bytes m_buffer;
                Thread *m_thread;
            } m_thread_read;
        } m_data;
    };

    // Interrupt handlers can delegate work to this worker such that interrupts
    // can be unblocked sooner
    class Worker : public Singleton<Worker> {
    public:
        void add_task(Task task)
        {
            m_tasks.enqueue(task);
        }

    private:
        CircularQueue<Task, 64> m_tasks;

        friend Singleton<Worker>;
        Worker();

        void run();
    };
}
