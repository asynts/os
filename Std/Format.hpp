#pragma once

#include <Std/Span.hpp>
#include <Std/StringView.hpp>
#include <Std/Array.hpp>
#include <Std/Vector.hpp>
#include <Std/Concepts.hpp>
#include <Std/String.hpp>
#include <Std/Lexer.hpp>

namespace Std {
    class StringBuilder;
    class String;

    template<typename T>
    struct Formatter {
        using __no_formatter_defined = void;
    };

    template<typename T, typename = void>
    struct HasFormatter {
        static constexpr bool value = true;
    };
    template<typename T>
    struct HasFormatter<T, typename Formatter<T>::__no_formatter_defined> {
        static constexpr bool value = false;
    };

    using FormatFunction = void(*)(StringBuilder&, const void*);

    struct TypeErasedFormatParameter {
        const void *m_value;
        FormatFunction m_format;
    };

    class TypeErasedFormatParams {
    public:
        const TypeErasedFormatParameter& param(usize index) const
        {
            ASSERT(index < m_params.size());
            return m_params[index];
        }

    protected:
        void set_params(Span<TypeErasedFormatParameter> params)
        {
            m_params = params;
        }

    private:
        Span<TypeErasedFormatParameter> m_params;
    };

    template<typename... Parameters>
    class VariadicFormatParams : public TypeErasedFormatParams {
    public:
        explicit VariadicFormatParams(const Parameters&... parameters)
            : m_params { TypeErasedFormatParameter {
                &parameters,
                [](StringBuilder& builder, const void *value)
                {
                    Formatter<Parameters>::format(builder, *reinterpret_cast<const Parameters*>(value));
                },
            }... }
        {
            this->set_params(m_params.span());
        }

    private:
        Array<TypeErasedFormatParameter, sizeof...(Parameters)> m_params;
    };

    void vformat(StringBuilder&, StringView fmtstr, TypeErasedFormatParams);

    class String {
    public:
        String()
        {
            m_buffer_size = 1;
            m_buffer = new char[m_buffer_size];
            m_buffer[0] = 0;
        }
        String(StringView view)
        {
            m_buffer_size = view.size() + 1;
            m_buffer = new char[m_buffer_size];
            view.strcpy_to({ m_buffer, m_buffer_size });
        }
        String(const char *str)
            : String(StringView { str })
        {
        }
        String(const String& other)
        {
            m_buffer = nullptr;
            m_buffer_size = 0;

            *this = other;
        }
        String(String&& other)
        {
            *this = move(other);
        }
        ~String()
        {
            delete[] m_buffer;
        }

        void strcpy_to(Span<char> other) const
        {
            return view().strcpy_to(other);
        }

        template<typename... Parameters>
        static String format(StringView fmtstr, const Parameters&...);

        // FIXME: Do we want to provide this overload?
        char* data() { return m_buffer; }

        const char* data() const { return m_buffer; }
        usize size() const { return m_buffer_size - 1; }

        const char* cstring() const { return m_buffer; }

        Span<const char> span() const { return { data(), size() }; }

        StringView view() const { return { data(), size() }; }

        operator StringView() const { return view(); }

        String& operator=(String&& other)
        {
            delete[] m_buffer;

            m_buffer = exchange(other.m_buffer, nullptr);
            m_buffer_size = exchange(other.m_buffer_size, 0);
            return *this;
        }
        String& operator=(const String& other)
        {
            delete[] m_buffer;

            m_buffer = new char[other.m_buffer_size];
            m_buffer_size = other.m_buffer_size;

            memcpy(m_buffer, other.m_buffer, m_buffer_size);
            return *this;
        }

        int operator<=>(const String& other) const { return view() <=> other.view(); }

        // FIXME: The compile should be able to generate this?
        bool operator==(const String& other) const { return (*this <=> other) == 0; }

    private:
        char *m_buffer = nullptr;
        usize m_buffer_size;
    };

    class StringBuilder {
    public:
        void append(char value)
        {
            m_data.append(value);
        }
        void append(StringView value)
        {
            for (char ch : value.iter())
                m_data.append(ch);
        }
        template<typename... Parameters>
        void appendf(StringView fmtstr, const Parameters&... parameters)
        {
            vformat(*this, fmtstr, VariadicFormatParams { parameters... });
        }

        char* data() { return m_data.data(); }
        usize size() { return m_data.size(); }

        StringView view() const { return m_data.span(); }
        String string() const { return view(); }
        ReadonlyBytes bytes() const { return view().bytes(); }

    private:
        Vector<char, 256> m_data;
    };

    template<typename... Parameters>
    String String::format(StringView fmtstr, const Parameters&... parameters)
    {
        StringBuilder builder;
        builder.appendf(fmtstr, parameters...);
        return builder.string();
    }

    inline void vformat(StringBuilder& builder, StringView fmtstr, TypeErasedFormatParams params)
    {
        usize next_parameter_index = 0;
        usize curly_brace_level = 0;
        Lexer lexer { fmtstr };

        while (!lexer.eof()) {
            if (curly_brace_level == 0) {
                if (lexer.try_consume("{{")) {
                    builder.append('{');
                    continue;
                }

                if (lexer.try_consume("}}")) {
                    builder.append('}');
                    continue;
                }

                if (lexer.try_consume('{')) {
                    curly_brace_level = 1;
                    continue;
                }

                ASSERT(lexer.peek_or_null() != '}');

                builder.append(lexer.consume());
            } else {
                if (lexer.try_consume('{')) {
                    ++curly_brace_level;
                    continue;
                }

                if (lexer.try_consume('}')) {
                    --curly_brace_level;

                    if (curly_brace_level == 0) {
                        auto& parameter = params.param(next_parameter_index++);
                        parameter.m_format(builder, parameter.m_value);
                    }

                    continue;
                }

                lexer.consume();
            }
        }
    }

    void dbgln_raw(StringView);

    template<typename... Parameters>
    void dbgln(StringView fmtstr, const Parameters&... parameters)
    {
        StringBuilder builder;
        vformat(builder, fmtstr, VariadicFormatParams { parameters... });
        dbgln_raw(builder.view());
    }

    template<typename... Parameters>
    void dbgln(const char *fmtstr, const Parameters&... parameters)
    {
        return dbgln(StringView { fmtstr }, parameters...);
    }

    inline void dbgln()
    {
        dbgln_raw("");
    }

    template<typename T>
    requires Concepts::Integral<T>
    struct Formatter<T> {
        static void format(StringBuilder&, T);
    };

    template<typename T>
    struct Formatter<T*> {
        static void format(StringBuilder& builder, const T *value)
        {
            if constexpr(sizeof(T*) == 4) {
                return Formatter<u32>::format(builder, reinterpret_cast<u32>(value));
            } else {
                static_assert(sizeof(T*) == 8);
                return Formatter<u64>::format(builder, reinterpret_cast<u64>(value));
            }
        }
    };
    template<typename T, usize Size>
    struct Formatter<T[Size]> : Formatter<T*> {
    };
    template<typename T>
    struct Formatter<T[]> : Formatter<T*> {
    };

    template<>
    struct Formatter<StringView> {
        static void format(StringBuilder&, StringView);
    };
    template<>
    struct Formatter<String> : Formatter<StringView> {
    };
    template<>
    struct Formatter<const char*> : Formatter<StringView> {
    };
    template<usize Size>
    struct Formatter<char[Size]> : Formatter<StringView> {
    };

    template<>
    struct Formatter<bool> {
        static void format(StringBuilder&, bool);
    };
    template<>
    struct Formatter<char> {
        static void format(StringBuilder&, char);
    };

    template<>
    struct Formatter<Path> {
        static void format(StringBuilder&, const Path&);
    };
    template<>
    struct Formatter<StringBuilder> {
        static void format(StringBuilder& builder, const StringBuilder& value)
        {
            builder.append(value.view());
        }
    };

    template<typename T>
    struct Formatter<Span<T>> {
        static void format(StringBuilder& builder, Span<T> value)
        {
            builder.appendf("({}, {})", value.data(), value.size());
        }
    };

    template<typename T>
    struct Formatter<Optional<T>> {
        static void format(StringBuilder& builder, const Optional<T>& value)
        {
            if (value.is_valid())
                builder.appendf("{}", value.value());
            else
                builder.append("nil");
        }
    };
}

using Std::dbgln;
