#include <Kernel/Worker.hpp>
#include <Kernel/Scheduler.hpp>

namespace Kernel
{
    Worker::Worker()
    {
        Thread thread { "Kernel::Worker" };

        Scheduler::the().create_thread(move(thread), [this] {
            this->run();
        });
    }

    void Worker::run()
    {
        for (;;) {
            if (m_tasks.size() > 0) {
                auto task = m_tasks.dequeue();

                FIXME_ASSERT(task.m_type == Task::Type::ThreadRead);

                usize nread = task.m_data.m_thread_read.m_handle.read(task.m_data.m_thread_read.m_buffer).must();

                task.m_data.m_thread_read.m_thread->unblock({
                    .m_type = ThreadUnblockInfo::Type::Syscall,
                    .m_data = {
                        .m_syscall = {
                            .m_retval = nread,
                        },
                    },
                });
            }
        }
    }
}
