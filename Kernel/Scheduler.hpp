#pragma once

#include <Std/Singleton.hpp>
#include <Std/String.hpp>
#include <Std/CircularQueue.hpp>
#include <Std/OwnPtr.hpp>
#include <Std/ArmedScopeGuard.hpp>

#include <Kernel/Process.hpp>
#include <Kernel/SystemHandler.hpp>
#include <Kernel/MPU.hpp>
#include <Kernel/PageAllocator.hpp>

namespace Kernel
{
    using namespace Std;

    // FIXME: I need to figure out who should own the stack
    struct StackWrapper {
        explicit StackWrapper(Bytes bytes)
            : m_bytes(bytes)
            , m_top(bytes.data() + bytes.size())
        {
        }

        u8* reserve(usize count)
        {
            ASSERT(m_bytes.data() + count <= m_top);
            return m_top -= count;
        }

        u8* push(ReadonlyBytes bytes)
        {
            u8 *data = reserve(bytes.size());
            bytes.copy_to({ data, bytes.size() });
            return data;
        }

        template<typename T>
        T* push_value(const T& value)
        {
            u8 *data = reserve(sizeof(value));
            return new (data) T { value };
        }

        char* push_cstring(const char *cstring)
        {
            reserve(__builtin_strlen(cstring) + 1);

            // FIXME: Is this necessary?
            char *cstring_on_stack = (char*)align(4);

            __builtin_strcpy(cstring_on_stack, cstring);
            return cstring_on_stack;
        }

        u8* align(u32 boundary)
        {
            static_assert(sizeof(u8*) == sizeof(u32));
            if (u32(m_top) % boundary != 0)
                reserve(u32(m_top) % boundary);

            return m_top;
        }

        u8* top()
        {
            return m_top;
        }

    private:
        Bytes m_bytes;
        u8 *m_top;
    };

    // FIXME: When doing the whole page allocator thing, move this into a more sensible location
    struct Region {
        MPU::RBAR rbar;
        MPU::RASR rasr;
    };

    struct ThreadUnblockInfo {
        enum class Type {
            Syscall,
        } m_type;

        union Data {
            struct SystemCall {
                u32 m_retval;
            } m_syscall;
        } m_data;
    };

    class Thread {
    public:
        Thread(StringView name, Optional<NonnullOwnPtr<Process>> process = {}, Optional<FullRegisterContext*> context = {})
            : m_name(name)
            , m_process(move(process))
            , m_context(context)
        {
        }

        // FIXME: RAII
        void free_owned_ranges()
        {
            dbgln("[Thread::free_owned_ranges] name={}", m_name);

            for (auto range : m_owned_ranges.iter()) {
                PageAllocator::the().deallocate(range);
            }
            m_owned_ranges.clear();
        }

        bool m_blocked = false;
        Optional<ThreadUnblockInfo> m_unblock_info;

        void unblock(ThreadUnblockInfo info)
        {
            VERIFY(m_blocked);

            m_unblock_info = info;
            m_blocked = false;
        }

        Vector<PageRange> m_owned_ranges;

        String m_name;
        Optional<NonnullOwnPtr<Process>> m_process;
        Optional<FullRegisterContext*> m_context;
        Vector<Region> m_regions;
        bool m_privileged = false;
        bool m_die_at_next_opportunity = false;
    };

    class SchedulerGuard;

    class Scheduler : public Singleton<Scheduler> {
    public:
        template<typename T, void (T::*Method)()>
        static void wrap_member_function_call_magic(void *object)
        {
            (reinterpret_cast<T*>(object)->*Method)();
        }

        Thread& active_thread()
        {
            return m_threads.front();
        }

        void terminate_active_thread();
        void donate_my_remaining_cpu_slice();

        Thread& create_thread(Thread&& thread)
        {
            VERIFY(thread.m_context.is_valid());
            return m_threads.enqueue(move(thread));
        }

        template<typename Callback>
        Thread& create_thread(Thread&& thread, Callback&& callback)
        {
            auto wrapper = [callback_ = move(callback)]() mutable {
                dbgln("Calling wrapper in create_thread");

                callback_();
                FIXME();
            };

            VERIFY(thread.m_owned_ranges.size() == 0);

            auto stack_range = PageAllocator::the().allocate(power_of_two(0x800)).must();
            StackWrapper stack { { reinterpret_cast<u8*>(stack_range.m_base), stack_range.size() } };
            thread.m_owned_ranges.append(stack_range);

            dbgln("[Scheduler::create_thread] Allocated stack for thread '{}'", thread.m_name);

            u8 *moved_wrapper = stack.reserve(sizeof(decltype(wrapper)));
            new (moved_wrapper) decltype(wrapper) { move(wrapper) };

            void (*wrapper_wrapper_function_pointer)(void*) = wrap_member_function_call_magic<decltype(wrapper), &decltype(wrapper)::operator()>;

            return create_thread_impl(move(thread), stack, wrapper_wrapper_function_pointer, moved_wrapper);
        }

        template<typename Callback>
        Thread& create_thread(StringView name, Callback&& callback)
        {
            Thread thread { String::format("Kernel: {}", name) };

            return create_thread(move(thread), move(callback));
        }

        [[noreturn]]
        void loop();

        // Used by PendSV exception handler should return Thread.m_stack.m_current_stack
        FullRegisterContext* schedule_next(FullRegisterContext*);

        bool enabled() const { return m_enabled; }

    private:
        friend Singleton<Scheduler>;
        Scheduler();

        Thread& create_thread_impl(Thread&& thread, StackWrapper, void (*callback)(void*), void *this_);

        CircularQueue<Thread, 8> m_threads;

        friend SchedulerGuard;
        volatile bool m_enabled;
    };

    class SchedulerGuard {
    public:
        SchedulerGuard()
        {
            m_armed = exchange(Scheduler::the().m_enabled, false);
        }
        ~SchedulerGuard()
        {
            if (m_armed)
                Scheduler::the().m_enabled = true;
        }
        SchedulerGuard(SchedulerGuard&& other)
        {
            m_armed = exchange(other.m_armed, false);
        }

        SchedulerGuard(const SchedulerGuard&) = delete;
        SchedulerGuard& operator=(const SchedulerGuard&) = delete;
        SchedulerGuard& operator=(SchedulerGuard&&) = delete;

    private:
        bool m_armed;
    };
}
