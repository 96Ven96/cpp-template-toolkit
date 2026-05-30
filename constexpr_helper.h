#pragma once
#include <array>
#include <type_traits>
#include <limits>

#if __cplusplus >= 202002L
#  include <string_view>
#  include "concepts_std.h"
#  include "function_traits.h"
#endif

/**
 * @brief Compile-time helpers: integer ↔ char-array conversion, array
 *        concatenation / joining, and constexpr search.
 */
namespace ConstexprHelperNs {

    /**
     * @brief Returns the number of decimal digits of @p val (minimum 1).
     *
     * Evaluated at compile time; suitable as a constexpr array-size expression.
     */
    [[nodiscard]] inline constexpr std::size_t intLength(unsigned int val) noexcept {
        std::size_t len = 1;
        while (val >= 10) { val /= 10; ++len; }
        return len;
    }

    /**
     * @brief Converts a compile-time integer to a null-terminated std::array<char>.
     *
     * The resulting array is suitable for constexpr string manipulation without
     * heap allocation.
     *
     * @code
     *   constexpr auto arr = ConstexprHelperNs::to_array_from_int<-42>();
     *   // arr == {'-','4','2','\0'}
     * @endcode
     */
    template <auto Val>
    [[nodiscard]] constexpr auto to_array_from_int() noexcept {
        using ValT = decltype(Val);
        static_assert(std::is_integral_v<ValT>, "Val must be an integral type");
        constexpr bool IsNegative        = Val < 0;
        constexpr unsigned int NegOffset = IsNegative ? 1u : 0u;
        constexpr auto AbsVal            = IsNegative ? -Val : Val;
        constexpr std::size_t N          = intLength(static_cast<unsigned int>(AbsVal)) + 1u + NegOffset;

        std::array<char, N> out{};
        ValT tmp = AbsVal;
        if constexpr (IsNegative) out[0] = '-';
        for (std::size_t i = 0; i < N - 1u - NegOffset; ++i) {
            out[N - 2u - i] = char('0' + (tmp % 10));
            tmp /= 10;
        }
        out[N - 1] = '\0';
        return out;
    }

#if __cplusplus >= 202002L
    /**
     * @brief Returns a std::string_view over a char array, excluding the trailing
     *        '\0' if present.
     *
     * Pairs naturally with to_array_from_int() and join().
     */
    template <std::size_t N>
    [[nodiscard]] constexpr std::string_view stringViewFromConstexpr(
        const std::array<char, N>& a) noexcept
    {
        const bool has_null = (a[N - 1] == '\0');
        return std::string_view{a.data(), N - (has_null ? 1u : 0u)};
    }
#endif

    // -------------------------------------------------------------------------
    // Array concatenation
    // -------------------------------------------------------------------------

    /// @brief Concatenates two std::array values into a single new array.
    template <typename T, std::size_t N, std::size_t M>
    [[nodiscard]] constexpr auto concat(
        const std::array<T, N>& first, const std::array<T, M>& second) noexcept
    {
        std::array<T, N + M> res{};
        for (std::size_t i = 0; i < N; ++i) res[i]     = first[i];
        for (std::size_t i = 0; i < M; ++i) res[N + i] = second[i];
        return res;
    }

    /// @brief Concatenates any number of same-element-type std::array values.
    template <typename T, std::size_t... Ns>
    [[nodiscard]] constexpr auto concatAll(const std::array<T, Ns>&... arrs) noexcept {
        std::array<T, (Ns + ...)> res{};
        std::size_t idx = 0;
        auto append = [&](const auto& arr) constexpr {
            for (const auto& el : arr) res[idx++] = el;
        };
        (append(arrs), ...);
        return res;
    }

    /// @brief Sum of std::tuple_size_v across all @p Args (must be array-like types).
    template <typename... Args>
    [[nodiscard]] constexpr std::size_t totalSize(const Args&...) noexcept {
        static_assert(sizeof...(Args) > 0, "At least one argument is required");
        return (std::tuple_size_v<Args> + ...);
    }

    // -------------------------------------------------------------------------
    // join() — char-array joining with separator
    // -------------------------------------------------------------------------

    namespace detail {

        // Appends src into dest at offset, skipping the trailing '\0' of src.
        template <typename T, std::size_t N, std::size_t M>
        constexpr void pushInArr(
            const std::array<T, N>& src, std::size_t& offset, std::array<T, M>& dest) noexcept
        {
            static_assert(std::is_same_v<T, char>, "TODO: generalise for non-char types");
            static_assert(N >= 1, "Array must have at least one element");
            for (std::size_t i = 0; i < N - 1; ++i)   // skip trailing '\0'
                dest[offset + i] = src[i];
            offset += N - 1;
        }

        // Computes the exact size of the output of join<T, N, Arrays...>.
        template <typename T, std::size_t N, typename... Arrays>
        struct join_size {
            static_assert(std::is_same_v<T, char>, "TODO: generalise for non-char types");
            static constexpr std::size_t total_arrays_size = (std::tuple_size_v<Arrays> + ...);
            static constexpr std::size_t num_arrays        = sizeof...(Arrays);
            static constexpr std::size_t num_separators    = (num_arrays > 0) ? (num_arrays - 1) : 0;
            static constexpr std::size_t value =
                total_arrays_size          // total chars (including per-array '\0')
                - num_arrays               // remove per-array null terminators
                + num_separators * N       // insert separators (with their '\0')
                - num_separators           // remove per-separator null terminators
                + 1;                       // one final null terminator
        };

    } // namespace detail

    /**
     * @brief Joins char arrays with a separator into a single null-terminated array.
     *
     * Example:
     * @code
     *   constexpr auto slash = std::array{'/', '\0'};
     *   constexpr auto a     = std::array{'a', '\0'};
     *   constexpr auto b     = std::array{'b', '\0'};
     *   constexpr auto res   = ConstexprHelperNs::join(slash, a, b);
     *   // res == {'a', '/', 'b', '\0'}
     * @endcode
     *
     * @note Currently limited to char arrays. Non-char support is a TODO.
     */
    template <typename T, std::size_t N, typename... Args>
    [[nodiscard]] constexpr auto join(
        const std::array<T, N>& sep, const Args&... elements) noexcept
    {
        static_assert(std::is_same_v<T, char>, "TODO: generalise for non-char types");
        constexpr std::size_t ArgsCount   = sizeof...(Args);
        constexpr std::size_t TotalLength = detail::join_size<T, N, Args...>::value;

        std::array<T, TotalLength> res{};
        std::size_t offset = 0;
        std::size_t idx    = 0;

        (void)std::initializer_list<int>{
            (detail::pushInArr(elements, offset, res),
             (idx++ < ArgsCount - 1 ? detail::pushInArr(sep, offset, res) : (void)0),
             0)...
        };
        res[TotalLength - 1] = '\0';
        return res;
    }

    // -------------------------------------------------------------------------
    // Constexpr search (C++20)
    // -------------------------------------------------------------------------

#if __cplusplus >= 202002L

    /**
     * @brief Returns the index of @p value in @p arr, evaluated at compile time.
     *
     * Returns std::numeric_limits<std::size_t>::max() when not found.
     * Use hasFoundAnIndex() to check the result in a readable way.
     */
    template <typename ContainerT, typename ValT>
        requires StdConceptNs_::IndexableConstexprContainer<ContainerT, ValT>
    [[nodiscard]] consteval std::size_t
    constexprIndexOf(const ContainerT& arr, const ValT& value) noexcept {
        for (std::size_t i = 0; i < arr.size(); ++i) {
            if (arr[i] == value) return i;
        }
        return (std::numeric_limits<std::size_t>::max)();
    }

    /// @brief Returns true if @p idx is a valid result from constexprIndexOf.
    [[nodiscard]] consteval bool hasFoundAnIndex(std::size_t idx) noexcept {
        return idx != (std::numeric_limits<std::size_t>::max)();
    }

    // -------------------------------------------------------------------------
    // Constexpr min / max
    // -------------------------------------------------------------------------

    /**
     * @brief Returns the minimum or maximum element of a non-empty array,
     *        with the comparison direction selected at compile time.
     *
     * For enum types, comparison is performed on the underlying integer type
     * via @c FunctionTraitsNs::get_convert_type_for_possible_enum_t; all other
     * types are compared directly.  The sentinel initial value uses
     * @c std::numeric_limits::lowest() (not @c min()) so the function is
     * correct for both integer and floating-point underlying types.
     *
     * @tparam GetMin  @c true → return the minimum element;
     *                 @c false → return the maximum element.
     * @tparam T       Element type.  Must satisfy @c std::totally_ordered, or
     *                 be an enum whose underlying type does.
     * @tparam N       Array size; must be greater than zero.
     *
     * @code
     *   constexpr std::array vals = {3, 1, 4, 1, 5};
     *   static_assert(ConstexprHelperNs::constexprBound<true>(vals)  == 1); // min
     *   static_assert(ConstexprHelperNs::constexprBound<false>(vals) == 5); // max
     * @endcode
     */
    template <bool GetMin, typename T, std::size_t N>
    [[nodiscard]] constexpr T constexprBound(const std::array<T, N>& vals) noexcept {
        static_assert(N > 0, "Array must be non-empty");
        using UnderT = FunctionTraitsNs::get_convert_type_for_possible_enum_t<T>;
        static_assert(std::totally_ordered<UnderT>, "T must be a totally ordered type");
        UnderT result = []() -> UnderT {
            if constexpr (GetMin)
                return (std::numeric_limits<UnderT>::max)();
            else
                return (std::numeric_limits<UnderT>::lowest)();
        }();
        for (const T & ele : vals) {
            const UnderT converted = static_cast<UnderT>(ele);
            if constexpr (GetMin) {
                if (converted < result) result = converted;
            } else {
                if (converted > result) result = converted;
            }
        }
        return static_cast<T>(result);
    }

    /// @brief Minimum element of @p vals. Shorthand for @c constexprBound<true>.
    template <typename T, std::size_t N>
    [[nodiscard]] constexpr T constexprMin(const std::array<T, N> & vals) noexcept
    { return constexprBound<true>(vals); }

    /// @brief Maximum element of @p vals. Shorthand for @c constexprBound<false>.
    template <typename T, std::size_t N>
    [[nodiscard]] constexpr T constexprMax(const std::array<T, N> & vals) noexcept
    { return constexprBound<false>(vals); }

    // -------------------------------------------------------------------------
    // Uniqueness check
    // -------------------------------------------------------------------------

    /**
     * @brief Returns true if all elements of @p arr are distinct (O(N²)).
     *
     * Suitable for compile-time validation of small arrays (e.g. enum key
     * lists, model role tables).  The quadratic cost is irrelevant in practice
     * because the arrays involved rarely exceed a few dozen entries.
     *
     * @tparam T  Element type — must support @c operator==.
     * @tparam N  Compile-time array size.
     *
     * @code
     *   constexpr std::array vals = {1, 2, 3};
     *   static_assert(ConstexprHelperNs::constexprAllUnique(vals));
     * @endcode
     */
    template <typename T, std::size_t N>
    [[nodiscard]] consteval bool constexprAllUnique(const std::array<T, N>& arr) noexcept {
        for (std::size_t i = 0; i < N; ++i)
            for (std::size_t j = i + 1; j < N; ++j)
                if (arr[i] == arr[j]) return false;
        return true;
    }

#endif // __cplusplus >= 202002L

} // namespace ConstexprHelperNs
