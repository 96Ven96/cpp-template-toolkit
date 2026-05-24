#pragma once
#include <type_traits>
#include <optional>
#include <iterator>

#include <QVariant>
#include <QVariantList>
#include <QString>
#include <QDate>
#include <QTime>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#  include <QtNumeric>
#else
#  include <QtGlobal>
#endif

#if __cplusplus >= 202002L
#  include <string_view>
#endif

#include "function_traits.h"

/**
 * @brief Type-conversion utilities: Qt ↔ std ↔ QVariant conversions.
 *
 * Qt-specific conversions live in the nested Qt_ sub-namespace to make the
 * dependency on Qt explicit at call sites.  Generic pointer helpers are in
 * the outer namespace.
 */
namespace ConversionTypesNs {

    // =========================================================================
    // Qt_ — Qt-specific conversions
    // =========================================================================

    namespace Qt_ {

        /**
         * @brief Converts any string-like value to QString.
         *
         * Handled types:
         *  - QString, QLatin1String       → returned as-is
         *  - const char*                  → fromUtf8 (null-safe)
         *  - char[N]                      → fromUtf8
         *  - std::string                  → fromStdString
         *  - std::array<char, N>          → QLatin1String (must be null-terminated)
         *  - arithmetic                   → QString::number
         *  - std::string_view             → fromLatin1 (C++20)
         *  - std::u8string_view           → fromUtf8   (C++20)
         *  - std::u16string_view          → fromUtf16  (C++20)
         *  - std::u32string_view          → code-point by code-point, no surrogate pair support
         */
        template <typename T>
        [[nodiscard]] inline decltype(auto) toQString(const T& val) noexcept {
            using DecayT = std::decay_t<T>;
            if constexpr (std::is_same_v<DecayT, QString> || std::is_same_v<DecayT, QLatin1String>) {
                return (val);
            } else if constexpr (std::is_same_v<DecayT, const char*>) {
                return (val == nullptr) ? QString{} : QString::fromUtf8(val);
            } else if constexpr (FunctionTraitsNs::is_c_array_of_char_v<DecayT>) {
                return QString::fromUtf8(val);
            } else if constexpr (std::is_same_v<DecayT, std::string>) {
                return QString::fromStdString(val);
            } else if constexpr (FunctionTraitsNs::is_std_array_char_v<DecayT>) {
                Q_ASSERT(val.at(val.size() - 1) == '\0');
                return QLatin1String(val.data());
            } else if constexpr (std::is_arithmetic_v<DecayT>) {
                return QString::number(val);
#if __cplusplus >= 202002L
            } else if constexpr (std::same_as<DecayT, std::string_view>) {
                return QString::fromLatin1(val.data(), static_cast<int>(val.size()));
            } else if constexpr (std::same_as<DecayT, std::u8string_view>) {
                return QString::fromUtf8(
                    reinterpret_cast<const char*>(val.data()), static_cast<int>(val.size()));
            } else if constexpr (std::same_as<DecayT, std::u16string_view>) {
                return QString::fromUtf16(val.data(), static_cast<int>(val.size()));
            } else if constexpr (std::same_as<DecayT, std::u32string_view>) {
                QString out;
                out.reserve(static_cast<int>(val.size()));
                for (char32_t cp : val)
                    out += QChar(static_cast<uint>(cp)); // NOTE: no surrogate-pair handling for U+10000+
                return out;
#endif
            } else {
                return QString{val};
            }
        }

        /**
         * @brief Tries to extract a value of type VarT from a QVariant.
         *
         * Returns std::nullopt when the variant is invalid or the conversion
         * fails.  Handles char specially (extracts the first Latin-1 character
         * of the string representation).  On Qt 5 provides type-specific
         * conversion paths for common types.
         */
        template <typename VarT>
        [[nodiscard]] inline std::optional<VarT> fromVariant(const QVariant& val) noexcept {
            if (!val.isValid()) return std::nullopt;

            if constexpr (std::is_same_v<VarT, char>) {
                const QString str = val.toString();
                if (str.isEmpty()) return std::nullopt;
                return static_cast<char>(str.at(0).toLatin1());
            } else {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                if (!val.canConvert<VarT>()) return std::nullopt;
                return val.value<VarT>();
#else
                if constexpr (std::is_same_v<VarT, QString>) {
                    if (!val.canConvert(QMetaType::QString)) return std::nullopt;
                    return val.toString();
                } else if constexpr (std::is_same_v<VarT, bool>) {
                    // bool before is_integral because is_integral<bool> == true
                    return val.toBool();
                } else if constexpr (std::is_integral_v<VarT>) {
                    bool ok;
                    auto v = val.toLongLong(&ok);
                    return ok ? std::optional<VarT>{static_cast<VarT>(v)} : std::nullopt;
                } else if constexpr (std::is_floating_point_v<VarT>) {
                    bool ok;
                    double v = val.toDouble(&ok);
                    return ok ? std::optional<VarT>{static_cast<VarT>(v)} : std::nullopt;
                } else if constexpr (std::is_enum_v<VarT>) {
                    auto valOpt = fromVariant<std::underlying_type_t<VarT>>(val);
                    return valOpt
                        ? std::optional<VarT>{static_cast<VarT>(*valOpt)}
                        : std::nullopt;
                } else {
                    if (!val.canConvert(qMetaTypeId<VarT>())) return std::nullopt;
                    return val.value<VarT>();
                }
#endif
            }
        }

        /**
         * @brief Converts a value to QVariant.
         *
         * Special-cases char (stored as single-char QString), const char* /
         * char[N] (UTF-8 converted to QString), and enums (stored as their
         * underlying integer) to ensure round-trip compatibility with fromVariant.
         */
        template <typename T>
        [[nodiscard]] inline QVariant toVariant(const T& val) noexcept {
            if constexpr (std::is_same_v<T, char>) {
                return QVariant::fromValue(QString(val));
            } else if constexpr (std::is_same_v<T, const char*>
                                  || FunctionTraitsNs::is_c_array_of_char_v<T>) {
                return QVariant::fromValue(QString::fromUtf8(val));
            } else if constexpr (std::is_enum_v<T>) {
                return toVariant(static_cast<std::underlying_type_t<T>>(val));
            } else {
                return QVariant::fromValue(val);
            }
        }

        /**
         * @brief Returns true if @p format is a valid QDate or QTime format string.
         *
         * @tparam DateTime  Must be QDate or QTime.
         *
         * Validation is performed by formatting the current date/time and parsing
         * the result back; if the parse succeeds the format is valid.
         */
        template <typename DateTime>
        [[nodiscard]] inline bool isValidFormat(const QString& format) noexcept {
            constexpr bool IsDate = std::is_same_v<DateTime, QDate>;
            static_assert(IsDate || std::is_same_v<DateTime, QTime>,
                          "DateTime must be QDate or QTime");
            DateTime current = IsDate
                ? DateTime(QDate::currentDate())
                : DateTime(QTime::currentTime());
            QString    formatted = current.toString(format);
            DateTime   restored  = DateTime::fromString(formatted, format);
            return restored.isValid(); // was current.isValid() — always true (bug)
        }

    } // namespace Qt_

    // =========================================================================
    // QVariantList builders
    // =========================================================================

    namespace detail_ {

#if __cplusplus >= 202002L
        template <typename CallableT, typename ConstIt>
        concept CallableFromIteratorOrNull =
            std::is_same_v<CallableT, std::nullptr_t>
            || std::is_invocable_v<CallableT, typename std::iterator_traits<ConstIt>::value_type>
            || std::is_invocable_v<CallableT, ConstIt>;
#endif

        template <auto Callable, typename ConstIt>
#if __cplusplus >= 202002L
            requires CallableFromIteratorOrNull<decltype(Callable), ConstIt>
#endif
        [[nodiscard]] inline decltype(auto) valFromIterator(ConstIt it) noexcept {
            using CallableT  = decltype(Callable);
            using ContainerT = typename std::iterator_traits<ConstIt>::value_type;
            if constexpr (std::is_same_v<CallableT, std::nullptr_t>) {
                return (*it);
            } else if constexpr (std::is_invocable_v<CallableT, ContainerT>) {
                return std::invoke(Callable, *it);
            } else {
                return std::invoke(Callable, it);
            }
        }

        template <typename ValT>
        [[nodiscard]] inline QVariant toQVariantImpl(const ValT& val) noexcept {
            if constexpr (std::is_pointer_v<ValT>) {
                using NoPtrT = std::remove_pointer_t<ValT>;
#ifndef NDEBUG
                if constexpr (std::is_base_of_v<QObject, NoPtrT>) {
                    if (val->parent() == nullptr)
                        fprintf(stderr,
                            "[ConversionTypesNs] warning: converting QObject pointer to "
                            "QVariant with no parent set.\n");
                }
#endif
                return QVariant::fromValue(const_cast<NoPtrT*>(val));
            } else {
                return QVariant::fromValue(val);
            }
        }

    } // namespace detail_

    /**
     * @brief Converts a range [iFrom, iTo) to a QVariantList, applying Callable
     *        to each element before wrapping it in a QVariant.
     *
     * @tparam Callable  nullptr (identity) or a callable that accepts the
     *                   dereferenced value or the iterator directly.
     */
    template <auto Callable, typename ConstIt>
#if __cplusplus >= 202002L
        requires detail_::CallableFromIteratorOrNull<decltype(Callable), ConstIt>
#endif
    [[nodiscard]] inline QVariantList
    toVariantListWithCallable(ConstIt iFrom, ConstIt iTo) noexcept {
        Q_ASSERT_X(std::distance(iFrom, iTo) >= 0, Q_FUNC_INFO, "Invalid iterators");
        QVariantList list;
        list.reserve(static_cast<int>(std::distance(iFrom, iTo)));
        for (; iFrom != iTo; ++iFrom)
            list.append(detail_::toQVariantImpl(detail_::valFromIterator<Callable>(iFrom)));
        return list;
    }

    /// @brief Convenience overload that accepts a container directly.
    template <auto Callable, typename Container>
    [[nodiscard]] inline QVariantList
    toVariantListWithCallable(const Container& container) noexcept
    { return toVariantListWithCallable<Callable>(container.cbegin(), container.cend()); }

    /// @brief Converts a range to a QVariantList without any element transformation.
    template <typename ConstIt>
    [[nodiscard]] inline QVariantList toVariantList(ConstIt iFrom, ConstIt iTo) noexcept
    { return toVariantListWithCallable<nullptr>(iFrom, iTo); }

    /// @brief Converts a container to a QVariantList without any element transformation.
    template <typename Container>
    [[nodiscard]] inline QVariantList toVariantList(const Container& container) noexcept
    { return toVariantList(container.cbegin(), container.cend()); }

    // =========================================================================
    // Generic pointer helper
    // =========================================================================

    /**
     * @brief Returns a pointer to @p obj whether it is already a pointer or not.
     *
     * Removes the need to branch on value vs. pointer at call sites that must
     * work with both.
     */
    template <typename Obj>
    [[nodiscard]] inline auto objectPtr(Obj obj) noexcept {
        if constexpr (std::is_pointer_v<std::remove_reference_t<Obj>>) {
            return obj;
        } else {
            return &obj;
        }
    }

} // namespace ConversionTypesNs
