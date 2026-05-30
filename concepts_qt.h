#pragma once
#include <array>
#include <type_traits>
#include <iterator>

#include <QString>
#include <QVariant>
#include <QAbstractItemModel>   // for DeriveFromQAbstractItemModel + Qt::UserRole
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#  include <QtNumeric>
#else
#  include <QtGlobal>
#endif

#if __cplusplus >= 202002L
#include <concepts>
#include <string_view>
#include "concepts_std.h"
#include "constexpr_helper.h"  // for ConstexprHelperNs::constexprAllUnique
#include "function_traits.h"   // for FunctionTraitsNs::is_std_array_v

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

    // -------------------------------------------------------------------------
    // Qt model role validation
    // -------------------------------------------------------------------------

    /**
     * @brief Returns @c true if @p val is a valid custom model role
     *        (i.e. strictly greater than @c Qt::UserRole).
     *
     * Qt reserves all roles from 0 through @c Qt::UserRole (256) for its
     * own use. Custom models must use values above that threshold.
     *
     * @tparam T Any integral type (@c int, @c qint32, @c std::size_t, …).
     */
    template <typename T>
        requires (std::is_integral_v<T>)
    [[nodiscard]] consteval bool IsValidRole(T val) noexcept {
        return val > static_cast<T>(Qt::UserRole);
    }

    /**
     * @brief Enum overload of IsValidRole — checks via the underlying integer.
     *
     * Allows scoped-enum role constants to be validated without an explicit
     * cast at the call site.
     */
    template <typename T>
        requires (std::is_enum_v<T>)
    [[nodiscard]] consteval bool IsValidRole(T val) noexcept {
        using UnderT = std::underlying_type_t<T>;
        return IsValidRole(static_cast<UnderT>(val));
    }

    /**
     * @brief Returns @c true if every element of @p vals is unique (O(N²)).
     *
     * Intended for compile-time validation of small arrays. Role lists
     * rarely exceed a few dozen entries, so the quadratic cost is
     * irrelevant in practice.
     *
     * @tparam T Element type — must support @c operator==.
     * @tparam N Compile-time array size.
     */
    template <typename T, std::size_t N>
    [[nodiscard]] consteval bool AreUniques(const std::array<T, N>& vals) noexcept {
        return ConstexprHelperNs::constexprAllUnique(vals);
    }

    /**
     * @brief Returns @c true if every element in @p vals is a valid, unique
     *        custom model role.
     *
     * Combines @ref IsValidRole (each value > @c Qt::UserRole) with
     * @ref AreUniques (no duplicate roles). Use this to guard the role
     * array of a @c QAbstractItemModel subclass at compile time.
     *
     * @tparam T Integral or enum type.
     * @tparam N Compile-time array size.
     */
    template <typename T, std::size_t N>
    [[nodiscard]] consteval bool AreValidModelRole(const std::array<T, N>& vals) noexcept {
        for (std::size_t i = 0; i < N; ++i)
            if (!IsValidRole(vals[i])) return false;
        return AreUniques(vals);
    }

    /**
     * @brief Satisfied when @p ModelRoleList is a @c std::array whose
     *        elements are all valid, unique custom model roles
     *        (> @c Qt::UserRole).
     *
     * Typical use: enforce the role table of a @c QAbstractItemModel
     * subclass at compile time.
     *
     * @code
     *   enum class MyRole : int {
     *       Name  = Qt::UserRole + 1,
     *       Value = Qt::UserRole + 2,
     *   };
     *   static constexpr auto kRoles = std::array{MyRole::Name, MyRole::Value};
     *   static_assert(QtConceptNs_::IsValidModelRoleList<kRoles>);
     * @endcode
     */
    template <auto ModelRoleList>
    concept IsValidModelRoleList =
        FunctionTraitsNs::is_std_array_v<decltype(ModelRoleList)>
        && AreValidModelRole(ModelRoleList);

    /**
     * @brief Satisfied when T is (or publicly derives from)
     *        @c QAbstractItemModel.
     *
     * Use as a constraint on generic model parameters to ensure they
     * expose the standard Qt item-model API.
     */
    template <typename T>
    concept DeriveFromQAbstractItemModel = std::is_base_of_v<QAbstractItemModel, T>;

} // namespace QtConceptNs_

#endif // __cplusplus >= 202002L
