#pragma once
#include <type_traits>
#include <functional>
#include <tuple>
#include <array>

#include "function_traits.h"

/**
 * @brief Callable invocation helpers: uniform dispatch, null-safe invoke,
 *        tuple/array iteration.
 */
namespace FunctionHelperNs {

    /**
     * @brief Invokes a member or free callable uniformly.
     *
     * When @p func is a member function pointer, @p obj is used as the receiver.
     * For free or static functions @p obj is ignored and @p args are forwarded
     * directly.
     */
    template <typename F, typename Obj, typename... Args>
    inline auto safe_invoke(F func, Obj&& obj, Args&&... args) {
        if constexpr (std::is_member_function_pointer_v<F>) {
            return std::invoke(func, std::forward<Obj>(obj), std::forward<Args>(args)...);
        } else {
            return std::invoke(func, std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Invokes a member function on @p obj only if @p obj is non-null.
     *
     * Returns false immediately when @p obj is nullptr.
     * If the invoked function returns bool that value is propagated; for void
     * functions true is returned on success.
     */
    template <typename Obj, typename Fn, typename... Args>
    inline bool invokeIfNotNull(Obj* obj, Fn function, Args&&... args) noexcept {
        static_assert(!std::is_pointer_v<Obj>, "Pointer-to-pointer input is not valid");
        static_assert(std::is_invocable_v<Fn, Obj*, Args...>, "Invalid parameter types");
        if (obj == nullptr)
            return false;
        if constexpr (std::is_same_v<bool, std::invoke_result_t<Fn, Obj*, Args...>>) {
            return std::invoke(function, obj, std::forward<Args>(args)...);
        } else {
            std::invoke(function, obj, std::forward<Args>(args)...);
            return true;
        }
    }

    /**
     * @brief Calls @p method on @p obj by unpacking the argument tuple @p tup.
     */
    template <typename Obj, typename Method, typename Tuple>
    inline void callFromTuple(Obj& obj, Method method, Tuple&& tup) {
        std::apply([&](auto&&... args) {
            (obj.*method)(std::forward<decltype(args)>(args)...);
        }, std::forward<Tuple>(tup));
    }

    namespace detail {

        template <typename F, typename T, std::size_t... I>
        constexpr void for_each_in_constexpr_array_impl(
            const std::array<T, sizeof...(I)>& arr, F&& f, std::index_sequence<I...>)
        {
            (f(arr[I]), ...);
        }

        template <typename Tuple, typename Func, std::size_t... I>
        inline void tuple_for_each_impl(Tuple&& t, Func&& f, std::index_sequence<I...>) {
            (f(std::get<I>(std::forward<Tuple>(t))), ...);
        }

    } // namespace detail

    /**
     * @brief Calls @p f once for every element of @p arr, in index order.
     *
     * Uses a compile-time index sequence so the loop can be fully unrolled.
     */
    template <typename F, typename T, std::size_t N>
    constexpr void for_each_in_constexpr_array(const std::array<T, N>& arr, F&& f) {
        detail::for_each_in_constexpr_array_impl(
            arr, std::forward<F>(f), std::make_index_sequence<N>{});
    }

    /**
     * @brief Calls @p f once for every element of tuple @p t.
     *
     * Each call receives the element as returned by std::get (possibly a
     * reference depending on the tuple's value category).
     */
    template <typename Tuple, typename Func>
    inline void tuple_for_each(Tuple&& t, Func&& f) {
        constexpr std::size_t N = std::tuple_size_v<std::remove_reference_t<Tuple>>;
        detail::tuple_for_each_impl(
            std::forward<Tuple>(t), std::forward<Func>(f),
            std::make_index_sequence<N>{});
    }

} // namespace FunctionHelperNs
