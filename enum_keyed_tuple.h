#pragma once
#include <tuple>
#include <type_traits>
#include <array>
#include <cstddef>

#if __cplusplus >= 202002L
#include "constexpr_helper.h"

/**
 * @brief Enum-keyed heterogeneous tuple.
 *
 * EnumTypePair<E, T, K> binds enum constant K (of type E) to type T.
 * EnumTuple composes any number of such pairs into a type-safe
 * heterogeneous store where each slot is addressed by its enum key rather
 * than a numeric index.
 *
 * @code
 *   enum class Col { Name, Age };
 *   using Row = EnumKeyedTupleNs::EnumTuple<
 *       EnumKeyedTupleNs::EnumTypePair<Col, QString, Col::Name>,
 *       EnumKeyedTupleNs::EnumTypePair<Col, int,     Col::Age>>;
 *
 *   Row row;
 *   row.set<Col::Name>(QString("Alice"));
 *   row.set<Col::Age>(30);
 *   auto name = row.getC<Col::Name>();  // QString const&
 * @endcode
 */
namespace EnumKeyedTupleNs {

    /**
     * @brief Associates an enum constant with a stored type.
     *
     * @tparam Enum  Integral-backed enum type.
     * @tparam ObjT  The C++ type stored at this slot.
     * @tparam Key   The enum constant used as the key.
     */
    template <typename Enum, typename ObjT, Enum Key>
        requires (std::is_enum_v<Enum> && std::is_integral_v<std::underlying_type_t<Enum>>)
    struct EnumTypePair {
        using RefType = ObjT;
        static constexpr Enum RefEnum = Key;
    };

    /**
     * @brief Heterogeneous tuple with compile-time enum-key access.
     *
     * All EnumTypePair arguments must share the same enum type and carry
     * distinct keys — violations are caught by static_assert at instantiation.
     *
     * @tparam EnumPairArgs  Pack of EnumTypePair specialisations.
     */
    template <typename... EnumPairArgs>
    class EnumTuple {
    public:
        using StoringT = std::tuple<typename EnumPairArgs::RefType...>;

    private:
        using FirstPairT = std::tuple_element_t<0, std::tuple<EnumPairArgs...>>;
        static constexpr std::size_t Count = std::tuple_size_v<StoringT>;
        using EnumT = decltype(FirstPairT::RefEnum);

        static_assert((std::is_same_v<EnumT, decltype(EnumPairArgs::RefEnum)> && ...),
                      "All EnumTypePair args must use the same enum type");

        static constexpr std::array<EnumT, Count> IndexMapper{EnumPairArgs::RefEnum...};

        [[nodiscard]] static consteval bool allUnique() noexcept {
            for (std::size_t i = 0; i < Count; ++i)
                for (std::size_t j = i + 1; j < Count; ++j)
                    if (IndexMapper[i] == IndexMapper[j]) return false;
            return true;
        }
        static_assert(allUnique(), "All enum keys must be distinct");

    public:
        template <EnumT Enum_>
        [[nodiscard]] static consteval std::size_t indexOf() noexcept {
            constexpr auto Idx = ConstexprHelperNs::constexprIndexOf(IndexMapper, Enum_);
            static_assert(ConstexprHelperNs::hasFoundAnIndex(Idx), "Invalid enum key");
            return Idx;
        }

    private:
        template <typename T, EnumT Enum_>
        [[nodiscard]] static consteval bool isValidTypeAt() noexcept {
            constexpr auto Idx = indexOf<Enum_>();
            return std::is_same_v<std::tuple_element_t<Idx, StoringT>, std::remove_cvref_t<T>>;
        }

        template <EnumT Enum_, typename MyT>
            requires (std::is_same_v<std::remove_cvref_t<MyT>, EnumTuple>)
        [[nodiscard]] static decltype(auto) getImpl(MyT& ref) noexcept {
            return (std::get<indexOf<Enum_>()>(ref.m_storing));
        }

    public:
        template <EnumT Enum_> [[nodiscard]] decltype(auto) getC() const noexcept { return getImpl<Enum_>(*this); }
        template <EnumT Enum_> [[nodiscard]] decltype(auto) get()  noexcept       { return getImpl<Enum_>(*this); }

        template <EnumT Enum_, typename T>
        void set(T&& val) noexcept {
            static_assert(isValidTypeAt<T, Enum_>(), "Type mismatch for enum key");
            get<Enum_>() = std::forward<T>(val);
        }

    private:
        StoringT m_storing{};
    };

} // namespace EnumKeyedTupleNs

#endif // __cplusplus >= 202002L
