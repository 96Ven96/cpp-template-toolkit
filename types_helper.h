#pragma once
#include <type_traits>
#include <iterator>
#include <vector>
#include <unordered_set>
#include <cstdint>

#if __cplusplus >= 202002L
#  include "concepts_std.h"
#endif

/**
 * @brief Type-level and runtime helpers for enums, index validation, pointer
 *        containers, contiguous-range detection, and dimension equalisation.
 */
namespace TypesHelperNs {

    // =========================================================================
    // Enum utilities
    // =========================================================================

    /**
     * @brief Compile-time check that every enum value is a distinct power-of-two bit.
     *
     * Triggers a static_assert if any value is zero, not a single bit, or
     * overlaps another value in the list.
     *
     * @code
     *   static_assert(TypesHelperNs::check_bitwise_unique_v<
     *       MyFlags, MyFlags::Read, MyFlags::Write, MyFlags::Exec>);
     * @endcode
     */
    template <typename Enum, Enum... Values>
    struct check_bitwise_unique;

    template <typename Enum>
    struct check_bitwise_unique<Enum> { static constexpr bool value = true; };

    template <typename Enum, Enum First, Enum... Rest>
    struct check_bitwise_unique<Enum, First, Rest...> {
    private:
        static constexpr auto first_val       = static_cast<std::underlying_type_t<Enum>>(First);
        static constexpr bool is_power_of_two = (first_val != 0) && ((first_val & (first_val - 1)) == 0);
        static constexpr bool no_overlap      =
            (((static_cast<std::underlying_type_t<Enum>>(Rest) & first_val) == 0) && ...);
        static_assert(is_power_of_two, "Enum value must be a single bit (power of two).");
        static_assert(no_overlap,      "Enum values must be bitwise distinct.");
    public:
        static constexpr bool value =
            is_power_of_two && no_overlap && check_bitwise_unique<Enum, Rest...>::value;
    };

    template <typename Enum, Enum... Values>
    inline constexpr bool check_bitwise_unique_v = check_bitwise_unique<Enum, Values...>::value;

    /**
     * @brief Compares two (possibly different) enum values through their
     *        underlying integer types.
     *
     * Useful when two distinct enums encode the same logical domain and a
     * direct comparison would not compile.
     */
    template <typename FirstEnum, typename SecondEnum>
    [[nodiscard]] constexpr bool areEqualEnum(FirstEnum first, SecondEnum second) noexcept {
        return static_cast<std::underlying_type_t<FirstEnum>>(first)
            == static_cast<std::underlying_type_t<SecondEnum>>(second);
    }

    /**
     * @brief Applies a bitwise operation to a variadic list of same-type enum values.
     *
     * The operation is performed on the underlying integer type and the raw
     * integer is returned (not cast back to the enum).
     *
     * @code
     *   auto flags = TypesHelperNs::bitwiseOnEnum(std::bit_or<>{},
     *                    MyFlag::Read, MyFlag::Write);
     * @endcode
     */
    template <typename BitwiseOp, typename Val, typename... Vals>
#if __cplusplus >= 202002L
        requires (StdConceptNs_::AllSameType<Val, Vals...>
                  && std::is_enum_v<Val>
                  && std::is_integral_v<std::underlying_type_t<Val>>
                  && std::invocable<BitwiseOp,
                         std::underlying_type_t<Val>,
                         std::underlying_type_t<Val>>)
#endif
    [[nodiscard]] inline constexpr auto bitwiseOnEnum(BitwiseOp op, Val val, Vals... vals) noexcept {
        using IntT = std::underlying_type_t<Val>;
        IntT res = static_cast<IntT>(val);
        ((res = op(res, static_cast<IntT>(vals))), ...);
        return res;
    }

    /// @brief Bitwise OR of multiple same-type enum values (returns underlying integer).
    template <typename Val, typename... Vals>
    [[nodiscard]] inline constexpr auto bitwiseOrOnEnum(Val val, Vals... vals) noexcept
    { return bitwiseOnEnum(std::bit_or<>{}, val, vals...); }

    /// @brief Bitwise AND of multiple same-type enum values (returns underlying integer).
    template <typename Val, typename... Vals>
    [[nodiscard]] inline constexpr auto bitwiseAndOnEnum(Val val, Vals... vals) noexcept
    { return bitwiseOnEnum(std::bit_and<>{}, val, vals...); }

    /// @brief bitwiseOrOnEnum cast to ReturnT.
    template <typename ReturnT, typename Val, typename... Vals>
    [[nodiscard]] inline constexpr auto bitwiseOrOnEnumWithCast(Val val, Vals... vals) noexcept
    { return static_cast<ReturnT>(bitwiseOrOnEnum(val, vals...)); }

    /// @brief bitwiseAndOnEnum cast to ReturnT.
    template <typename ReturnT, typename Val, typename... Vals>
    [[nodiscard]] inline constexpr auto bitwiseAndOnEnumWithCast(Val val, Vals... vals) noexcept
    { return static_cast<ReturnT>(bitwiseAndOnEnum(val, vals...)); }

    // =========================================================================
    // Index / pointer validation
    // =========================================================================

    /**
     * @brief Returns true if @p index is within bounds of @p container.
     *
     * The container size is obtained by invoking SizeFn at runtime.
     * Negative signed indices are treated as invalid.
     */
    template <class Container, typename NumT, auto SizeFn>
    [[nodiscard]] inline bool isValidIndexWithFn(const Container& container, NumT index) noexcept {
        static_assert(std::is_integral_v<std::decay_t<NumT>>, "Index must be an integer type");
        using SizeFnT = decltype(SizeFn);
        static_assert(std::is_invocable_v<SizeFnT, std::decay_t<Container>>,
                      "SizeFn must be invocable with Container");
        using SizeT = std::invoke_result_t<SizeFnT, std::decay_t<Container>>;
        static_assert(std::is_integral_v<SizeT>, "SizeFn must return an integer");

        const auto size = std::invoke(SizeFn, container);
        if constexpr (std::is_signed_v<NumT>) {
            if (index < 0) return false;
            return static_cast<std::make_unsigned_t<NumT>>(index)
                 < static_cast<std::make_unsigned_t<SizeT>>(size);
        } else {
            return static_cast<SizeT>(index) < size;
        }
    }

    /// @brief isValidIndexWithFn using Container::size as the size function.
    template <class Container, typename NumT>
    [[nodiscard]] inline bool isValidIndex(const Container& container, NumT index) noexcept {
        return isValidIndexWithFn<Container, NumT, &Container::size>(container, index);
    }

    /// @brief True if every index in @p indices is valid for @p container (using SizeFunc).
    template <auto SizeFunc, typename Container, typename... Indices>
    [[nodiscard]] inline bool areValidIndicesWithFn(
        const Container& container, Indices... indices) noexcept
    {
        return ((isValidIndexWithFn<Container, Indices, SizeFunc>(container, indices)) && ...);
    }

    /// @brief True if every index in @p indices is valid for @p container.
    template <typename Container, typename... Indices>
    [[nodiscard]] inline bool areValidIndices(
        const Container& container, Indices... indices) noexcept
    {
        return areValidIndicesWithFn<&std::decay_t<Container>::size>(container, indices...);
    }

    /// @brief True if every raw pointer in @p ptrs is non-null.
    template <typename... Ptrs>
    [[nodiscard]] inline bool areValidPointers(Ptrs... ptrs) noexcept {
        static_assert((std::is_pointer_v<Ptrs> && ...), "All arguments must be raw pointers");
        return (... && (ptrs != nullptr));
    }

    /**
     * @brief Returns true if no two pointers in @p container alias the same object.
     *
     * Uses an unordered_set for O(n) average-case complexity.
     */
    template <typename Container>
    [[nodiscard]] inline bool isUniquePointerContainer(const Container& container) noexcept {
        using ContT = std::decay_t<typename Container::value_type>;
        static_assert(std::is_pointer_v<ContT>, "Container must store pointers");
        std::unordered_set<std::uintptr_t> seen;
        for (const auto* ptr : container) {
            const auto addr = reinterpret_cast<std::uintptr_t>(ptr);
            if (!seen.insert(addr).second)
                return false;
        }
        return true;
    }

    // =========================================================================
    // Range utilities
    // =========================================================================

    /**
     * @brief Partitions [iFrom, iTo) into maximal contiguous sub-ranges.
     *
     * @p isContiguous can accept either (Iterator, Iterator) or
     * (const ValueType&, const ValueType&); the correct overload is selected
     * at compile time.
     *
     * @return Vector of [rangeBegin, rangeEnd] inclusive pairs.
     */
    template <typename Iterator, typename IsContiguousLogic>
    inline std::vector<std::pair<Iterator, Iterator>>
    computeContiguousRanges(Iterator iFrom, Iterator iTo, IsContiguousLogic&& isContiguous) {
        using ValT = typename std::iterator_traits<Iterator>::value_type;
        constexpr bool ByIterator = std::is_invocable_r_v<bool, IsContiguousLogic, Iterator, Iterator>;
        constexpr bool ByValue    = std::is_invocable_r_v<bool, IsContiguousLogic, const ValT&, const ValT&>;
        static_assert(ByIterator || ByValue,
            "isContiguous must accept (Iterator, Iterator) or (const Value&, const Value&)");

        std::vector<std::pair<Iterator, Iterator>> ranges;
        if (std::distance(iFrom, iTo) <= 0)
            return ranges;

        Iterator iStart = iFrom;
        Iterator iPrev  = iFrom;
        for (auto iCurrent = std::next(iFrom); iCurrent != iTo; ++iCurrent) {
            const bool cont = ByIterator ? isContiguous(iPrev, iCurrent)
                                         : isContiguous(*iPrev, *iCurrent);
            if (!cont) {
                ranges.emplace_back(iStart, iPrev);
                iStart = iCurrent;
            }
            iPrev = iCurrent;
        }
        ranges.emplace_back(iStart, iPrev);
        return ranges;
    }

    // =========================================================================
    // Dimension equalisation
    // =========================================================================

    /**
     * @brief Calls addEleFn or rmEleFn until the dimension of @p objToApply
     *        matches the reference dimension from @p refObj.
     *
     * Useful for synchronising row/column counts between a model and a view
     * widget without writing explicit loops at each call site.
     */
    template <typename RefObj, typename GetRefDim,
              typename ObjToApply, typename GetDimApp,
              typename AddEleFn, typename RmEleFn>
    inline void equalizeDimension(
        const RefObj& refObj, GetRefDim getRefDim,
        ObjToApply& objToApply, GetDimApp getDimApp,
        AddEleFn addEleFn, RmEleFn rmEleFn) noexcept
    {
        static_assert(std::is_invocable_v<GetRefDim, const RefObj&>,
                      "GetRefDim must accept (const RefObj&)");
        static_assert(std::is_invocable_v<GetDimApp, const ObjToApply&>,
                      "GetDimApp must accept (const ObjToApply&)");
        static_assert(std::is_invocable_v<AddEleFn, ObjToApply&>,
                      "AddEleFn must accept (ObjToApply&)");
        static_assert(std::is_invocable_v<RmEleFn, ObjToApply&>,
                      "RmEleFn must accept (ObjToApply&)");

        auto refDim = std::invoke(getRefDim, refObj);
        auto appDim = std::invoke(getDimApp, objToApply);
        if (refDim == appDim) return;

        auto delta = appDim - refDim;
        for (int i = static_cast<int>(delta < 0 ? -delta : delta); i > 0; --i) {
            if (delta > 0) std::invoke(rmEleFn,  objToApply);
            else           std::invoke(addEleFn, objToApply);
        }
    }

    /// @brief Equalises both row and column dimensions of @p appObj to match @p refObj.
    template <typename RefObj, typename GetRowRef, typename GetColRef,
              typename AppObj, typename GetRowApp, typename GetColApp,
              typename AddRowFn, typename RmRowFn, typename AddColFn, typename RmColFn>
    inline void equalizeDimensions(
        const RefObj& refObj, GetRowRef getRefRow, GetColRef getRefCol,
        AppObj& appObj, GetRowApp getAppRow, GetColApp getAppCol,
        AddRowFn addRowFn, RmRowFn rmRowFn, AddColFn addColFn, RmColFn rmColFn) noexcept
    {
        equalizeDimension(refObj, getRefRow, appObj, getAppRow, addRowFn, rmRowFn);
        equalizeDimension(refObj, getRefCol, appObj, getAppCol, addColFn, rmColFn);
    }

} // namespace TypesHelperNs
