#pragma once
#include <type_traits>
#include <iterator>
#include <array>

#if __cplusplus >= 202002L
#include <concepts>
#include <string_view>

/// @brief Concepts with no Qt dependency — pure standard-library constraints.
namespace StdConceptNs_ {

    /// @brief True if T (cvref-stripped) is std::string or std::string_view.
    template <typename T>
    concept IsStdStringLike =
        std::same_as<std::remove_cvref_t<T>, std::string> ||
        std::same_as<std::remove_cvref_t<T>, std::string_view>;

    /// @brief True if every type in the pack is the same as Val.
    template <typename Val, typename... Vals>
    concept AllSameType = (std::is_same_v<Val, Vals> && ...);

    /**
     * @brief True if T is a scoped enum (enum class / enum struct).
     *
     * Uses std::is_scoped_enum_v in C++23; falls back to a manual check in C++20.
     */
    template <typename T>
    concept IsScopedEnum =
#if __cplusplus >= 202302L
        std::is_scoped_enum_v<T>
#else
        std::is_enum_v<T> && !std::is_convertible_v<T, std::underlying_type_t<T>>
#endif
    ;

    /**
     * @brief True if C is a size()-aware, index-accessible container whose value_type matches V.
     *
     * Designed for consteval algorithms (e.g. constexprIndexOf) that operate on
     * std::array or similar compile-time containers.
     */
    template <class C, class V>
    concept IndexableConstexprContainer =
        requires(const C& c, std::size_t i) {
            typename std::remove_cvref_t<C>::value_type;
            { c.size() } -> std::convertible_to<std::size_t>;
            { c[i] }     -> std::convertible_to<const std::remove_cvref_t<V>&>;
        } && std::same_as<
            std::remove_cvref_t<V>,
            std::remove_cvref_t<typename std::remove_cvref_t<C>::value_type>>;

} // namespace StdConceptNs_

#endif // __cplusplus >= 202002L
