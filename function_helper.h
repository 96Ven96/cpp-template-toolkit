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

        // Materialises a callable whose parameters are exactly Args..., forwarding to a
        // generic body. The Args... are deduced from the std::tuple via partial
        // specialisation (you cannot deduce a pack from a type alias inside a function).
        template <typename ArgsTuple>
        struct typed_callable_factory;

        template <typename... Args>
        struct typed_callable_factory<std::tuple<Args...>> {
            template <typename Body>
            static auto make(Body body) {
                // The returned lambda exposes CONCRETE parameters (Args...), so an API that
                // inspects a functor's signature (e.g. Qt's new-style connect) accepts it;
                // inside, it just forwards those typed arguments to the caller's generic body.
                return [body = std::move(body)](Args... args) -> decltype(auto) {
                    return (body(args...));   // parens: defensive decltype(auto) ref-preservation
                };
            }
        };

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

    /**
     * @brief Builds a callable with concrete parameter types taken from a tuple type.
     *
     * Given @p ArgsTuple == std::tuple<Args...>, returns a callable taking exactly
     * (Args...) that forwards to the generic @p body. Useful when an API needs a
     * concretely-typed functor but you only have a generic handler — most notably Qt's
     * new-style connect(), which cannot bind a generic `[](auto&&...)` lambda because it
     * cannot deduce its arity.
     *
     * Pairs naturally with CallableFn::input_t / member_input_t, which yield the argument
     * tuple of a signal/slot.
     *
     * @code
     *   using Args = FunctionTraitsNs::CallableFn::input_t<decltype(&Sender::valueChanged)>;
     *   auto slot  = FunctionHelperNs::makeTypedCallable<Args>(
     *                    [](auto&&... a){ // handle a... // });
     *   QObject::connect(sender, &Sender::valueChanged, ctx, slot);
     * @endcode
     */
    template <typename ArgsTuple, typename Body>
    [[nodiscard]] inline auto makeTypedCallable(Body body) {
        return detail::typed_callable_factory<ArgsTuple>::make(std::move(body));
    }

} // namespace FunctionHelperNs
