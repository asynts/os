#pragma

#include <Std/SortedSet.hpp>
#include <Std/Concepts.hpp>

namespace Std
{
    template<typename T>
    struct Hash {
    };

    template<typename T>
    concept HashDefinedByMember = requires (const T& t) {
        { t.hash() } -> Concepts::Same<u32>;
    };

    template<typename T>
    requires HashDefinedByMember<T>
    struct Hash<T> {
        static u32 compute(const T& value)
        {
            return value.hash();
        }
    };

    template<>
    struct Hash<u32> {
        static u32 compute(u32 value)
        {
            // https://github.com/skeeto/hash-prospector

            value ^= value >> 16;
            value *= 0x7feb352d;
            value ^= value >> 15;
            value *= 0x846ca68b;
            value ^= value >> 16;

            return value;
        }
    };
    template<>
    struct Hash<StringView> {
        static u32 compute(StringView value)
        {
            // http://www.cse.yorku.ca/~oz/hash.html

            u32 hash = 5381;

            for (char ch : value.iter())
                hash = ((hash << 5) + hash) + ch;

            return hash;
        }
    };
    template<>
    struct Hash<String> : Hash<StringView> {
    };

    template<typename T>
    requires Concepts::Integral<T> && Concepts::HasSizeOf<T, 4>
    struct Hash<T> {
        static u32 compute(T value)
        {
            return Hash<u32>::compute(bit_cast<u32>(value));
        }
    };

    template<typename T>
    class HashTable {
    public:
        void insert(const T& value)
        {
            m_set.insert({ Hash<T>::compute(value), value });
        }
        void insert(T&& value)
        {
            m_set.insert({ Hash<T>::compute(value), move(value) });
        }

        T* search(const T& value)
        {
            Node *node = m_set.search({ Hash<T>::compute(value), value });

            if (node)
                return &node->m_value;
            else
                return nullptr;
        }

        void remove(const T& value)
        {
            m_set.remove({ Hash<T>::compute(value), value });
        }

        usize size() const
        {
            return m_set.size();
        }

    private:
        struct Node {
            u32 m_hash;
            T m_value;

            bool operator<(const Node& other) const
            {
                if (m_hash < other.m_hash)
                    return true;

                if (m_hash > other.m_hash)
                    return false;

                return m_value < other.m_value;
            }
            bool operator>(const Node& other) const
            {
                if (m_hash > other.m_hash)
                    return true;

                if (m_hash < other.m_hash)
                    return false;

                return m_value > other.m_value;
            }
        };

        SortedSet<Node> m_set;
    };
}
