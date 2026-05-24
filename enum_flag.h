#pragma once

#include <bitset>
#include <bit>
#include <type_traits>
#include <limits>
#include <cstddef>

/**
 * @file enum_flag.h
 * @brief Type-safe bitset keyed by an enum.
 *
 * EnumFlagNs::EnumFlag<E> wraps a std::bitset whose bits are addressed by
 * values of the enum @c E. Each enum value is expected to be a single bit
 * (a power of two); pair this header with
 * @c TypesHelperNs::check_bitwise_unique to validate that condition at
 * the enum's definition site.
 *
 * @note Requires C++20 for std::countr_zero. std::bitset operations
 *       themselves only become @c constexpr in C++23 — see the per-member
 *       comments for what is usable in compile-time contexts.
 */

namespace EnumFlagNs {

    template <typename EnumT>
    class EnumFlag {
        static_assert(std::is_enum_v<EnumT>, "EnumFlag requires an enum type");

    public:
        using UnderT = std::underlying_type_t<EnumT>;
        static constexpr std::size_t Bits = std::numeric_limits<UnderT>::digits;
        using BitSet = std::bitset<Bits>;

    private:
        /**
         * @brief Convert a power-of-two enum value into a bit index in [0, Bits-1].
         *
         * Behaviour is undefined for enum values that are not a power of two
         * (use @c TypesHelperNs::check_bitwise_unique to enforce this at the
         * enum declaration).
         */
        static constexpr std::size_t index(EnumT enum_) noexcept {
            const auto raw = static_cast<std::make_unsigned_t<UnderT>>(enum_);
            // countr_zero(0) is well-defined and returns Bits, which would
            // be out of range for bitset::set — treat the zero value as a
            // no-op key, the caller is responsible for not passing it.
            return std::countr_zero(raw);
        }

    public:

        // ---------------------------------------------------------------------
        // Factories
        // ---------------------------------------------------------------------

        /**
         * @brief Build an EnumFlag with all listed enum values set.
         *
         * Runtime-callable on every standard. @c constexpr only takes effect
         * on C++23 toolchains because @c std::bitset::set becomes
         * @c constexpr there; on earlier standards it stays a regular call.
         */
        template <typename... Enums>
            requires ((std::is_same_v<std::decay_t<Enums>, EnumT> && ...))
        [[nodiscard]] static constexpr EnumFlag make_or(Enums... enums) noexcept {
            EnumFlag res;
            (res.set(enums, true), ...);
            return res;
        }

        /**
         * @brief Compile-time variant of @ref make_or that bypasses
         *        std::bitset and folds the underlying integer values directly.
         *
         * Always usable in @c constexpr / @c consteval contexts.
         */
        template <typename... Enums>
            requires ((std::is_same_v<std::decay_t<Enums>, EnumT> && ...))
        [[nodiscard]] static consteval EnumFlag make_or_consteval(Enums... enums) noexcept {
            const UnderT mask = static_cast<UnderT>((static_cast<UnderT>(enums) | ...));
            return EnumFlag{mask};
        }

        // ---------------------------------------------------------------------
        // Constructors
        // ---------------------------------------------------------------------

        constexpr EnumFlag() noexcept = default;

        /// @brief Construct directly from a raw underlying-type mask.
        explicit constexpr EnumFlag(UnderT mask) noexcept
            : m_bits{ static_cast<unsigned long long>(mask) } {}

        /// @brief Construct with a single enum value set.
        explicit constexpr EnumFlag(EnumT value) noexcept {
            set(value, true);
        }

        // ---------------------------------------------------------------------
        // Compound assignment (modifying)
        // ---------------------------------------------------------------------

        constexpr EnumFlag& operator|=(EnumT v) noexcept       { set(v, true);                          return *this; }
        constexpr EnumFlag& operator|=(const EnumFlag& o) noexcept { m_bits |= o.m_bits;                return *this; }
        constexpr EnumFlag& operator&=(const EnumFlag& o) noexcept { m_bits &= o.m_bits;                return *this; }
        constexpr EnumFlag& operator^=(EnumT v) noexcept       { m_bits.flip(index(v));                 return *this; }
        constexpr EnumFlag& operator^=(const EnumFlag& o) noexcept { m_bits ^= o.m_bits;                return *this; }

        // ---------------------------------------------------------------------
        // Binary operators (non-modifying)
        // ---------------------------------------------------------------------

        [[nodiscard]] constexpr EnumFlag operator|(EnumT v) const noexcept       { EnumFlag r = *this; r |= v; return r; }
        [[nodiscard]] constexpr EnumFlag operator|(const EnumFlag& o) const noexcept { EnumFlag r = *this; r |= o; return r; }
        [[nodiscard]] constexpr EnumFlag operator&(const EnumFlag& o) const noexcept { EnumFlag r = *this; r &= o; return r; }
        [[nodiscard]] constexpr EnumFlag operator^(EnumT v) const noexcept       { EnumFlag r = *this; r ^= v; return r; }
        [[nodiscard]] constexpr EnumFlag operator^(const EnumFlag& o) const noexcept { EnumFlag r = *this; r ^= o; return r; }
        [[nodiscard]] constexpr EnumFlag operator~() const noexcept              { EnumFlag r; r.m_bits = ~m_bits; return r; }

        [[nodiscard]] constexpr bool operator==(const EnumFlag& other) const noexcept {
            return m_bits == other.m_bits;
        }
        [[nodiscard]] constexpr bool operator!=(const EnumFlag& other) const noexcept {
            return !(*this == other);
        }

        // ---------------------------------------------------------------------
        // Bit-wise queries
        // ---------------------------------------------------------------------

        [[nodiscard]] constexpr bool test(EnumT v) const noexcept     { return m_bits.test(index(v)); }
        [[nodiscard]] constexpr bool contains(EnumT v) const noexcept { return test(v); }
        [[nodiscard]] constexpr bool empty() const noexcept           { return m_bits.none(); }
        [[nodiscard]] constexpr bool any() const noexcept             { return m_bits.any(); }
        [[nodiscard]] constexpr std::size_t count() const noexcept    { return m_bits.count(); }

        // ---------------------------------------------------------------------
        // Mutators
        // ---------------------------------------------------------------------

        constexpr void set(EnumT v, bool enable = true) noexcept { m_bits.set(index(v), enable); }
        constexpr void reset(EnumT v) noexcept                   { m_bits.reset(index(v)); }
        constexpr void clear() noexcept                          { m_bits.reset(); }

        // ---------------------------------------------------------------------
        // Raw access
        // ---------------------------------------------------------------------

        [[nodiscard]] constexpr UnderT mask() const noexcept {
            return static_cast<UnderT>(m_bits.to_ullong());
        }

        [[nodiscard]] constexpr const BitSet& bitsetC() const noexcept { return m_bits; }
        [[nodiscard]] BitSet& bitset() noexcept                        { return m_bits; }

    private:
        BitSet m_bits{};
    };

} // namespace EnumFlagNs
