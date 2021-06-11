#pragma once

#include <Std/Forward.hpp>

namespace Kernel
{
    inline bool is_executing_in_thread_mode() {
        u32 ipsr;
        asm ("mrs %0, ipsr;"
             "isb;"
            : "=r"(ipsr));

        return ipsr == 0;
    }

    // FIXME: Some of this code is redundant with the scheduler

    template<typename T, void (T::*Method)()>
    void call_member_function(T& object)
    {
        (object.*Method)();
    }

    template<typename Callback>
    void execute_in_handler_mode(Callback&& callback)
    {
        auto wrapper = [callback = move(callback)]() mutable {
            callback();
        };

        if (is_executing_in_thread_mode()) {
            auto *function = call_member_function<decltype(wrapper), &decltype(wrapper)::operator()>;

            asm volatile("mov r0, %0;"
                         "mov r1, %1;"
                         "bl _handler_trampoline"
                :
                : "r"(&wrapper), "r"(function)
                : "r0", "r1", "lr");
        } else {
            wrapper();
        }
    }

    template<typename Callback>
    void execute_in_thread_mode(Callback&& callback)
    {
        FIXME();
    }
}
