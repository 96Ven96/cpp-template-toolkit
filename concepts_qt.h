#pragma once
#include <type_traits>
#include <iterator>

#include <QString>
#include <QVariant>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#  include <QtNumeric>
#else
#  include <QtGlobal>
#endif

#if __cplusplus >= 202002L
#include <concepts>
#include <string_view>
#include "concepts_std.h"

/**
 * @brief Concepts that require Qt headers.
 *
 * For pure standard-library constraints see StdConceptNs_ in concepts_std.h.
 */
namespace QtConceptNs_ {

    /// @brief True if T (cvref-stripped) is exactly QString.
    template <typename T>
    concept IsQString = std::same_as<std::remove_cvref_t<T>, QString>;

    /**
     * @brief True if T behaves like a Qt container: has value_type, begin(),
     *        end() and a size() that returns a size_type.
     *
     * Qt containers (QVector, QList, …) are covered here because they do not
     * always satisfy std::input_or_output_iterator on the iterators they expose.
     */
    template <typename T>
    concept IsAQtContainer = requires(T container) {
        typename std::remove_cvref_t<T>::value_type;
        { container.begin() };
        { container.end() };
        { container.size() } -> std::convertible_to<typename T::size_type>;
    };

    /**
     * @brief True if T is any iterable container — std or Qt.
     *
     * std containers are checked via the full iterator-category requirements;
     * Qt containers fall back to IsAQtContainer which relaxes those requirements.
     */
    template <typename T>
    concept IsAContainer = []<typename U = std::remove_cvref_t<T>>() {
        return requires(U container) {
            typename U::value_type;
            typename U::iterator;
            { container.begin() } -> std::input_or_output_iterator;
            { container.end() }   -> std::input_or_output_iterator;
        } || IsAQtContainer<U>;
    }();

    /// @brief True if T is any string-like type: Qt (QString) or std (string / string_view).
    template <typename T>
    concept IsStringLike = IsQString<T> || StdConceptNs_::IsStdStringLike<T>;

    /**
     * @brief True if SignalT is a Qt signal belonging to ObjT.
     *
     * Checks: is a member function pointer, belongs to ObjT, and returns void.
     */
    template <typename SignalT, typename ObjT>
    concept QtSignalLike =
        std::is_member_function_pointer_v<SignalT> &&
        std::is_same_v<ObjT, typename QtPrivate::FunctionPointer<SignalT>::Object> &&
        std::is_same_v<void, typename QtPrivate::FunctionPointer<SignalT>::ReturnType>;

    /// @brief True if QVariant::fromValue(m) is well-formed for type M.
    template <typename M>
    concept QVariantCompatible = requires(M m) {
        { QVariant::fromValue(m) } -> std::same_as<QVariant>;
    };

    // -------------------------------------------------------------------------
    // Validator concepts
    // -------------------------------------------------------------------------

    /// @brief True if Validator is nullptr_t (disabled / no-op validator).
    template <auto Validator>
    concept IsNullValidator =
        std::is_same_v<std::decay_t<decltype(Validator)>, std::nullptr_t>;

    /// @brief True if Validator is a callable that accepts ValT and returns bool.
    template <auto Validator, typename ValT>
    concept IsCallableValidator =
        std::is_invocable_r_v<bool, decltype(Validator), ValT>;

    /// @brief Validator is either nullptr (disabled) or a bool(ValT) callable.
    template <auto Validator, typename ValT>
    concept IsValidValidator =
        IsNullValidator<Validator> || IsCallableValidator<Validator, ValT>;

    /**
     * @brief Returns true if @p val passes the compile-time Validator.
     *
     * When Validator is nullptr the function always returns true (no-op path
     * is optimised away at compile time).
     */
    template <auto Validator, typename ValT>
        requires IsValidValidator<Validator, ValT>
    [[nodiscard]] inline bool isValidValue(ValT val) noexcept {
        if constexpr (!IsNullValidator<Validator>) {
            return Validator(val);
        } else {
            return true;
        }
    }

} // namespace QtConceptNs_

#endif // __cplusplus >= 202002L
