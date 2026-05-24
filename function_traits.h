#pragma once
#include <type_traits>
#include <functional>
#include <tuple>
#include <array>
#include <optional>

/**
 * @brief Type traits for inspecting callable signatures and member pointers.
 *
 * Sub-namespace CallableFn provides function_signature, a trait that extracts
 * return type, argument tuple and arity from any callable (free function,
 * std::function, lambda, member function — including all cv/ref/noexcept
 * combinations).
 *
 * The outer namespace provides general-purpose traits for member pointers,
 * enums, optional, and char-array detection.
 */
namespace FunctionTraitsNs {

    // -------------------------------------------------------------------------
    // remove_cvref_t backport
    // -------------------------------------------------------------------------

#if __cplusplus < 202002L
    /// @brief Backport of std::remove_cvref_t for C++17.
    template <class T>
    using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;
#else
    template <class T>
    using remove_cvref_t = std::remove_cvref_t<T>;
#endif

    // =========================================================================
    // CallableFn — signature introspection
    // =========================================================================

    namespace CallableFn {

        // Primary: forward to operator() for lambdas and function objects.
        template <typename T>
        struct function_signature : function_signature<decltype(&T::operator())> {};

        // Free function pointer
        template <typename R, typename... Args>
        struct function_signature<R(*)(Args...)> {
            using return_type = R;
            using input_type  = std::tuple<Args...>;
            static constexpr std::size_t arity = sizeof...(Args);
        };

        template <typename R, typename... Args>
        struct function_signature<R(*)(Args...) noexcept> {
            using return_type = R;
            using input_type  = std::tuple<Args...>;
            static constexpr std::size_t arity = sizeof...(Args);
        };

        // std::function
        template <typename R, typename... Args>
        struct function_signature<std::function<R(Args...)>> {
            using return_type = R;
            using input_type  = std::tuple<Args...>;
            static constexpr std::size_t arity = sizeof...(Args);
        };

// Generates a function_signature specialization for member functions with
// the given cv-qualifier, ref-qualifier and noexcept specifier.
#define FTRAITS_MEMBER_SIG(cv, ref, noex)                                     \
        template <typename C, typename R, typename... Args>                   \
        struct function_signature<R(C::*)(Args...) cv ref noex> {             \
            using return_type = R;                                             \
            using input_type  = std::tuple<Args...>;                          \
            static constexpr std::size_t arity = sizeof...(Args);             \
        };

        FTRAITS_MEMBER_SIG(              ,   ,          )
        FTRAITS_MEMBER_SIG(              ,   , noexcept )
        FTRAITS_MEMBER_SIG(const         ,   ,          )
        FTRAITS_MEMBER_SIG(const         ,   , noexcept )
        FTRAITS_MEMBER_SIG(volatile      ,   ,          )
        FTRAITS_MEMBER_SIG(volatile      ,   , noexcept )
        FTRAITS_MEMBER_SIG(const volatile,   ,          )
        FTRAITS_MEMBER_SIG(const volatile,   , noexcept )

#if __cplusplus >= 201703L
        FTRAITS_MEMBER_SIG(      , & ,          )
        FTRAITS_MEMBER_SIG(      , & , noexcept )
        FTRAITS_MEMBER_SIG(      , &&,          )
        FTRAITS_MEMBER_SIG(      , &&, noexcept )
        FTRAITS_MEMBER_SIG(const , & ,          )
        FTRAITS_MEMBER_SIG(const , & , noexcept )
        FTRAITS_MEMBER_SIG(const , &&,          )
        FTRAITS_MEMBER_SIG(const , &&, noexcept )
#endif

#undef FTRAITS_MEMBER_SIG

        // --- Convenience aliases ---

        /// @brief Argument tuple of callable F.
        template <typename F>
        using input_t = typename function_signature<std::decay_t<F>>::input_type;

        /// @brief Return type of callable F.
        template <typename F>
        using return_t = typename function_signature<std::decay_t<F>>::return_type;

        /// @brief Argument count of callable F.
        template <typename F>
        constexpr std::size_t arity_v = function_signature<std::decay_t<F>>::arity;

        /// @brief Argument tuple of a member function pointer passed as a value.
        template <auto MemberFnPtr>
        using member_input_t = input_t<decltype(MemberFnPtr)>;

        /// @brief Return type of a member function pointer passed as a value.
        template <auto MemberFnPtr>
        using member_return_t = return_t<decltype(MemberFnPtr)>;

        /// @brief Argument count of a member function pointer passed as a value.
        template <auto MemberFnPtr>
        constexpr std::size_t member_arity_v = arity_v<decltype(MemberFnPtr)>;

        // Safe tuple element: yields void when Idx is out of range.
        template <typename Tuple, std::size_t Idx, bool IsValid>
        struct tuple_element_safe { using type = void; };
        template <typename Tuple, std::size_t Idx>
        struct tuple_element_safe<Tuple, Idx, true> { using type = std::tuple_element_t<Idx, Tuple>; };

        /// @brief std::tuple_element_t<Idx, Tuple>, or void if Idx >= tuple size.
        template <typename Tuple, std::size_t Idx>
        using tuple_element_safe_t = typename tuple_element_safe<
            Tuple, Idx, (Idx < std::tuple_size_v<Tuple>)>::type;

    } // namespace CallableFn

    // =========================================================================
    // Member pointer traits
    // =========================================================================

    /// @brief Checks that F is a member function pointer of class C.
    template <typename C, typename F> struct is_member_of_class : std::false_type {};
    template <typename C, typename R, typename... Args>
    struct is_member_of_class<C, R(C::*)(Args...)> : std::true_type {};
    template <typename C, typename F>
    inline constexpr bool is_member_of_class_v = is_member_of_class<C, F>::value; // was ::value() — compile error

    /// @brief Checks that M is a member object pointer of class C.
    template <typename C, typename M> struct is_member_object_of_class : std::false_type {};
    template <typename C, typename T>
    struct is_member_object_of_class<C, T C::*> : std::true_type {};
    template <typename C, typename M>
    inline constexpr bool is_member_object_of_class_v = is_member_object_of_class<C, M>::value;

    /// @brief Extracts the pointed-to type from a data member pointer.
    template <typename M> struct member_object_type;
    template <typename C, typename T>
    struct member_object_type<T C::*> { using type = T; };
    template <typename M>
    using member_object_type_t = typename member_object_type<std::remove_cv_t<M>>::type;

    // =========================================================================
    // Enum / optional helpers
    // =========================================================================

    /// @brief For enum types: the underlying integer type. For all others: T itself.
    template <typename U, bool IsEnum = std::is_enum_v<U>>
    struct get_convert_type_for_possible_enum { using type = U; };
    template <typename U>
    struct get_convert_type_for_possible_enum<U, true> { using type = std::underlying_type_t<U>; };
    template <typename U>
    using get_convert_type_for_possible_enum_t = typename get_convert_type_for_possible_enum<U>::type;

    /// @brief Detects std::optional<T>.
    template <typename T> struct is_std_optional : std::false_type {};
    template <typename T> struct is_std_optional<std::optional<T>> : std::true_type {};
    template <typename T>
    inline constexpr bool is_std_optional_v = is_std_optional<std::decay_t<T>>::value;

    /// @brief Unwraps std::optional<T> → T; passthrough for non-optional types.
    template <typename T> struct optional_value { using type = remove_cvref_t<T>; };
    template <typename U> struct optional_value<std::optional<U>> { using type = U; };
    template <typename T>
    using optional_value_t = typename optional_value<remove_cvref_t<T>>::type;

    // =========================================================================
    // Callable tuple traits
    // =========================================================================

    /// @brief True if every callable in the tuple is invocable with no arguments.
    template <typename Tuple> struct is_nullary_invocable_tuple;
    template <typename... Fns>
    struct is_nullary_invocable_tuple<std::tuple<Fns...>> {
        static constexpr bool value = (std::is_invocable_v<Fns> && ...);
    };

    /// @brief True if every callable in Tuple is invocable with Args...
    template <typename Tuple, typename... Args> struct is_invocable_tuple;
    template <typename... Fns, typename... Args>
    struct is_invocable_tuple<std::tuple<Fns...>, Args...> {
        static constexpr bool value = (std::is_invocable_v<Fns, Args...> && ...);
    };

    template <typename Tuple, typename... Args>
    inline constexpr bool is_nullary_invocable_tuple_v = is_nullary_invocable_tuple<Tuple>::value;
    template <typename Tuple, typename... Args>
    inline constexpr bool is_invocable_tuple_v = is_invocable_tuple<Tuple, Args...>::value;

    /// @brief True if FirstFn and SecondFn accept the same argument types.
    template <typename FirstFn, typename SecondFn>
    struct are_connectable_for_input {
        static constexpr bool value = std::is_same_v<CallableFn::input_t<FirstFn>,
                                                      CallableFn::input_t<SecondFn>>;
    };
    template <typename FirstFn, typename SecondFn>
    inline constexpr bool are_connectable_for_input_v = are_connectable_for_input<FirstFn, SecondFn>::value;

    /// @brief Underlying integer type for enums; T itself for non-enum types.
    template <typename T, bool IsEnum = std::is_enum_v<T>>
    struct underlying_or_self_impl { using type = T; };
    template <typename T>
    struct underlying_or_self_impl<T, true> { using type = typename std::underlying_type<T>::type; };
    template <typename T>
    using underlying_or_self_t = typename underlying_or_self_impl<T>::type;

    // =========================================================================
    // Char-array detection
    // =========================================================================

    /// @brief True if T (reference-stripped) is a C-style char array.
    template <typename T> struct is_c_array_of_char : std::false_type {};
    template <std::size_t N> struct is_c_array_of_char<char[N]>                : std::true_type {};
    template <std::size_t N> struct is_c_array_of_char<const char[N]>          : std::true_type {};
    template <std::size_t N> struct is_c_array_of_char<volatile char[N]>       : std::true_type {};
    template <std::size_t N> struct is_c_array_of_char<const volatile char[N]> : std::true_type {};
    template <typename T>
    inline constexpr bool is_c_array_of_char_v = is_c_array_of_char<std::remove_reference_t<T>>::value;

    /// @brief True if T is std::array<char, N> (any cv-qualification).
    template <typename T> struct is_std_array_char : std::false_type {};
    template <std::size_t N> struct is_std_array_char<std::array<char, N>>                : std::true_type {};
    template <std::size_t N> struct is_std_array_char<const std::array<char, N>>          : std::true_type {};
    template <std::size_t N> struct is_std_array_char<volatile std::array<char, N>>       : std::true_type {};
    template <std::size_t N> struct is_std_array_char<const volatile std::array<char, N>> : std::true_type {};
    template <typename T>
    inline constexpr bool is_std_array_char_v = is_std_array_char<std::remove_reference_t<T>>::value;

} // namespace FunctionTraitsNs
