#pragma once

#include <Std/Forward.hpp>

#ifdef TEST
# include <new>
#endif

namespace Std {
    template<typename T>
    class Optional {
    public:
        Optional()
        {
            m_is_valid = false;
        }
        Optional(const T& value)
        {
            new (m_value) T { value };
            m_is_valid = true;
        }
        Optional(T&& value)
        {
            new (m_value) T { move(value) };
            m_is_valid = true;
        }
        Optional(const Optional& other)
        {
            *this = other;
        }
        Optional(Optional&& other)
        {
            *this = move(other);
        }
        ~Optional()
        {
            clear();
        }

        bool is_valid() const { return m_is_valid; }

        const T& value() const & { return *reinterpret_cast<const T*>(m_value); }
        T& value() & { return *reinterpret_cast<T*>(m_value); }
        T&& value() && { return *reinterpret_cast<T*>(m_value); }

        T value_or(T default_)
        {
            if (is_valid())
                return value();
            else
                return default_;
        }
        T must() &&
        {
            VERIFY(is_valid());

            T tmp = move(value());
            clear();

            return move(tmp);
        }
        T& must() &
        {
            VERIFY(is_valid());
            return value();
        }
        const T& must() const&
        {
            VERIFY(is_valid());
            return value();
        }

        void clear()
        {
            if (m_is_valid) {
                m_is_valid = false;
                value().~T();
            }
        }

        Optional& operator=(const T& other)
        {
            clear();

            m_is_valid = true;
            new (m_value) T { other };

            return *this;
        }
        Optional& operator=(T&& other)
        {
            clear();

            m_is_valid = true;
            new (m_value) T { move(other) };

            return *this;
        }

        Optional& operator=(const Optional& other)
        {
            clear();

            if (other.is_valid()) {
                m_is_valid = true;
                new (m_value) T { other.value() };
            }

            return *this;
        }
        Optional& operator=(Optional&& other)
        {
            clear();

            if (other.is_valid()) {
                m_is_valid = true;
                new (m_value) T { move(other.value()) };
                other.clear();
            }

            return *this;
        }
        const T* operator->() const { return &must(); }
        T* operator->() { return &must(); }

    private:
        alignas(alignof(T))
        u8 m_value[sizeof(T)];
        bool m_is_valid = false;
    };
}
