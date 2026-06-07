#pragma once
#include <type_traits>
#include <functional>
#include <optional>
#include <cmath>
#include <cstdio>

#include <QObject>
#include <QVariant>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#  include <QtNumeric>
#else
#  include <QtGlobal>
#endif

#include "conversion_types.h"
#include "function_traits.h"   // CallableFn::input_t — type a slot from a signal's signature
#include "function_helper.h"   // makeTypedCallable — materialise that typed slot

#include <memory>              // std::shared_ptr (self-owning connection bundle)
#include <QList>

/**
 * @brief Qt Q_PROPERTY helpers: type aliases for getters/setters/signals,
 *        assign-if-changed, set_value, ownership assignment, and connection
 *        toggles.
 *
 * The set_value family is the primary API; the older setValue overloads are
 * kept with [[deprecated]] for migration purposes.
 */
namespace QtSupportHelperNs {

    // =========================================================================
    // Common type aliases
    // =========================================================================

    /// @brief Pairs an error code with an optional result value.
    template <typename Err, typename T>
    using ReturnT = std::pair<Err, std::optional<T>>;

    // Getter function pointer aliases
    template <typename Obj, typename T>
    using GetParFnRefNoArgs  = const T& (Obj::*)() const;
    template <typename Obj, typename T, typename... Args>
    using GetParFnRefArgs    = const T& (Obj::*)(Args...) const;
    template <typename Obj, typename T>
    using GetParFnCopyNoArgs = T (Obj::*)() const;
    template <typename Obj, typename T, typename... Args>
    using GetParFnCopyArgs   = T (Obj::*)(Args...) const;

    // Setter function pointer aliases
    template <class Obj, typename T>
    using SetFunctionCopy     = void (Obj::*)(T) noexcept;
    template <class Obj, typename T>
    using SetFunctionConstRef = void (Obj::*)(const T&) noexcept;

    // Signal function pointer aliases
    template <class Obj, typename T>
    using SignalParameter  = void (Obj::*)(const T&);
    template <class Obj>
    using SignalNoParameter = void (Obj::*)();

    // Validator aliases
    template <class Obj, typename T>
    using ValidationMethod = bool (Obj::*)(const T&) const;
    template <typename T>
    using ValidationLambda = std::function<bool(const T&)>;

    // Comparison / equivalence function aliases
    template <typename T>
    using EquivalentFunction = std::function<bool(const T&, const T&)>;
    template <typename T>
    using CompareFunction = std::function<int(const T&, const T&)>;
    template <typename T>
    using Comparator = CompareFunction<T>; ///< @deprecated Use CompareFunction<T>.

    // Iterator-based accessor aliases (for container-mapping APIs)
    template <template <typename> class Container, typename InputType, typename ReturnType>
    using AccessFromIterator =
        std::function<ReturnType&&(typename Container<InputType>::iterator)>;
    template <template <typename> class Container, typename InputType, typename ReturnType>
    using AccessFromConstIterator =
        std::function<const ReturnType&&(typename Container<InputType>::const_iterator)>;

    // =========================================================================
    // MemberFunctionArgument — extracts the single argument type of a setter
    // =========================================================================

    /**
     * @brief Extracts the argument type of a single-argument void member function.
     *
     * Covers the four common setter signatures:
     *   void(T), void(T) noexcept, void(const T&), void(const T&) noexcept.
     */
    template <typename T> struct MemberFunctionArgument;
    template <typename C, typename A>
    struct MemberFunctionArgument<void(C::*)(A)>              { using type = A; };
    template <typename C, typename A>
    struct MemberFunctionArgument<void(C::*)(A) noexcept>     { using type = A; };
    template <typename C, typename A>
    struct MemberFunctionArgument<void(C::*)(const A&)>       { using type = A; };
    template <typename C, typename A>
    struct MemberFunctionArgument<void(C::*)(const A&) noexcept> { using type = A; };

    // =========================================================================
    // Default comparison / equivalence functions
    // =========================================================================

    /// @brief Default equality predicate: uses operator==.
    template <typename T>
    inline const EquivalentFunction<T>& g_EquivalentFunctionDefault() noexcept {
        static EquivalentFunction<T> fn = [](const T& a, const T& b) -> bool {
            return a == b;
        };
        return fn;
    }

    /// @brief Default qreal equality: fuzzy compare with 1e-6 absolute tolerance.
    inline const EquivalentFunction<qreal>& g_EquivalentQrealDefault() noexcept {
        static EquivalentFunction<qreal> fn = [](const qreal& a, const qreal& b) -> bool {
            constexpr qreal eps = 1e-6;
            return qAbs(a - b) <= eps;
        };
        return fn;
    }

    /// @brief Default three-way comparator: returns -1 / 0 / +1.
    template <typename T>
    inline const CompareFunction<T>& g_CompareFunctionDefault() noexcept {
        static CompareFunction<T> fn = [](const T& a, const T& b) -> int {
            return a < b ? -1 : (a == b ? 0 : 1);
        };
        return fn;
    }

    /**
     * @brief Returns a comparator suited for set_value: 0 = equal, non-zero = different.
     *
     * For float and double, uses qFuzzyCompare; two NaN values are treated as
     * equal to avoid infinite-loop scenarios.
     */
    template <typename T>
    inline CompareFunction<T> getCompareFunctionForSetValueDefault() noexcept {
        return [](const T& a, const T& b) -> int {
            if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
                if (std::isnan(a) && std::isnan(b)) return 0;
                return qFuzzyCompare(a, b) ? 0 : -1;
            } else {
                return a == b ? 0 : -1;
            }
        };
    }

    // =========================================================================
    // Key / accessor structs (for serialisation helpers)
    // =========================================================================

    template <class Obj, typename T>
    struct KeyGetFnRefNoArg {
        const char*              key   = nullptr;
        GetParFnRefNoArgs<Obj,T> getFn = nullptr;
    };

    template <class Obj, typename T>
    struct KeyGetFnCopyNoArg {
        const char*               key   = nullptr;
        GetParFnCopyNoArgs<Obj,T> getFn = nullptr;
    };

    template <class Obj, typename T>
    struct KeySetFnConstRef {
        const char*                key   = nullptr;
        SetFunctionConstRef<Obj,T> setFn = nullptr;
    };

    template <class Obj, typename T>
    struct KeySetFnConstRefBackup {
        const char*                key       = nullptr;
        SetFunctionConstRef<Obj,T> setFn     = nullptr;
        const T*                   backupVal = nullptr;
    };

    template <class Obj, typename T>
    struct KeySetFnCopy {
        const char*           key   = nullptr;
        SetFunctionCopy<Obj,T> setFn = nullptr;
    };

    template <class Obj, typename T>
    struct KeySetFnCopyBackup {
        const char*            key       = nullptr;
        SetFunctionCopy<Obj,T> setFn     = nullptr;
        const T*               backupVal = nullptr;
    };

    // =========================================================================
    // Core helpers
    // =========================================================================

    /**
     * @brief Assigns @p newVal to @p dst only if they differ.
     *
     * Returns true when an assignment was performed, false when the values
     * were already equal.  Handles implicit conversions between compatible types.
     */
    template <typename T, typename M>
    inline bool assign_if_diff(T newVal, M& dst) noexcept {
        using DT = std::decay_t<T>;
        using DM = std::decay_t<M>;
        if constexpr (std::is_same_v<DT, DM> || std::is_convertible_v<DT, DM>) {
            if (newVal == dst) return false;
            dst = std::forward<T>(newVal);
            return true;
        } else {
            static_assert(std::is_constructible_v<DM, T>,
                          "T must be constructible to M");
            return assign_if_diff(DM{newVal}, dst);
        }
    }

    /**
     * @brief Sets a Q_PROPERTY member and emits @p signal if the value changed.
     *
     * @return true if the value changed and the signal was emitted.
     */
    template <typename Obj, typename T, typename M, typename Signal>
    inline bool set_value(T newVal, M& oldVal, Signal signal, Obj* refObj) noexcept {
        static_assert(std::is_base_of_v<QObject, Obj>, "Obj must derive from QObject");
        if (assign_if_diff(newVal, oldVal)) {
            emit (refObj->*signal)();
            return true;
        }
        return false;
    }

    /**
     * @brief set_value with an additional runtime validator.
     *
     * @p val is invoked before the assignment; if it returns false the property
     * is not changed.
     */
    template <typename Obj, typename T, typename M, typename Signal, typename Validator>
    inline bool set_value(T newVal, M& oldVal, Signal signal, Obj* refObj,
                          Validator val) noexcept {
        static_assert(std::is_invocable_r_v<bool, Validator, T>,
                      "Validator must have signature bool(T)");
        if (!std::invoke(val, newVal)) return false;
        return set_value(std::forward<T>(newVal), oldVal,
                         std::forward<Signal>(signal), refObj);
    }

    /**
     * @brief Sets parent QObject ownership on every element in @p container.
     *
     * Works with containers of raw QObject pointers; stores-by-value containers
     * print a debug warning because ownership semantics are unsafe in that case.
     */
    template <typename Container, typename Obj>
    inline void assignOwnership(Container& container, Obj parent) noexcept {
        static_assert(!std::is_const_v<std::remove_reference_t<Container>>,
                      "Cannot assign parent on a const container");
        using ObjT  = std::remove_pointer_t<std::decay_t<Obj>>;
        using ContT = std::remove_pointer_t<typename Container::value_type>;
        static_assert(std::is_base_of_v<QObject, ObjT>,  "Parent must be a QObject");
        static_assert(std::is_base_of_v<QObject, ContT>, "Children must be QObjects");

        ObjT* parentPtr = ConversionTypesNs::objectPtr(parent);
        for (auto& child : container) {
            ContT* childPtr;
            if constexpr (std::is_pointer_v<typename Container::value_type>) {
                childPtr = child;
            } else {
#ifndef NDEBUG
                std::fprintf(stderr,
                    "[QtSupportHelperNs] warning: container stores QObject by value"
                    " — ownership assignment may be unsafe.\n");
#endif
                childPtr = &child;
            }
            childPtr->setParent(parentPtr);
        }
    }

    /**
     * @brief Deletes an object stored as a typed pointer inside a QVariant.
     *
     * Returns false and asserts in debug mode if the cast fails (wrong type).
     */
    template <typename T>
    inline bool deletePointerFromVariant(const QVariant& val) noexcept {
        auto* ptr = qvariant_cast<T*>(val);
        if (ptr == nullptr) {
            Q_ASSERT_X(false, Q_FUNC_INFO, "invalid pointer to delete");
            return false;
        }
        delete ptr;
        return true;
    }

    // =========================================================================
    // Qt connection helpers
    // =========================================================================

    /**
     * @brief Connects or disconnects a signal/slot pair at compile time.
     *
     * @tparam Enable  true → QObject::connect, false → QObject::disconnect.
     */
    template <bool Enable, auto Signal, auto Slot, typename Sender, typename Receiver>
    inline void toggleConnection(Sender* sender, Receiver* receiver,
                                 Qt::ConnectionType type = Qt::AutoConnection)
    {
        if constexpr (Enable) {
            QObject::connect(sender, Signal, receiver, Slot, type);
        } else {
            QObject::disconnect(sender, Signal, receiver, Slot);
        }
    }

    /// @brief Runtime variant of toggleConnection.
    template <typename Sender, typename Signal, typename Receiver, typename Slot>
    inline void toggleConnection(bool enable,
                                 Sender* sender, Signal signal,
                                 Receiver* receiver, Slot slot,
                                 Qt::ConnectionType type = Qt::AutoConnection)
    {
        if (enable) QObject::connect(sender, signal, receiver, slot, type);
        else        QObject::disconnect(sender, signal, receiver, slot);
    }

    // =========================================================================
    // EphemeralConnections — a self-owning, single-shot group of connections
    // =========================================================================

    /**
     * @brief A group of QObject connections that are all torn down together, exactly once.
     *
     * Generalises the "single-shot connection" pattern: you arm an @e action plus one or
     * more @e terminators, and the first terminator whose predicate holds tears the whole
     * group down (running an optional finaliser first). It is the abstraction behind wiring
     * such as "send a value, then disconnect when it is confirmed, or when sending fails,
     * or when it is changed again".
     *
     * @par Self-owning lifetime
     * The connection handles live in a std::shared_ptr bundle that every terminator slot
     * captures. The group therefore stays alive as long as any of its connections is live,
     * and frees itself the instant it is dismissed — no manual bookkeeping, no dangling.
     * Lifetime is "fire and forget": the destructor does @b not disconnect; the group lives
     * until a terminator (or an explicit dismiss()) tears it down. Create it, arm it, move on.
     *
     * @note Designed for single-threaded (per-QObject-thread) use; the bundle is not guarded.
     *       Pass the receiver/context object so the connection also auto-disconnects if that
     *       object is destroyed (standard Qt context semantics).
     *
     * @code
     *   QtSupportHelperNs::EphemeralConnections{}
     *       .until(device, &Device::valueConfirmed, this,
     *              [expected](int v){ return v == expected; },   // predicate
     *              [this]{ emit committed(); })                   // finaliser
     *       .until(bus, &Bus::sendFailed, this,
     *              [id](quint32 r){ return r == id; }, []{})
     *       .until(device, &Device::valueChanged, device,
     *              []{ return true; }, []{});                     // unconditional reset
     * @endcode
     */
    class EphemeralConnections {
    public:
        EphemeralConnections()
            // Shared bundle: every terminator slot captures a copy of it, so the group's
            // connection handles outlive this object (fire-and-forget) and are freed only
            // when the group is torn down.
            : bundle_{std::make_shared<QList<QMetaObject::Connection>>()} {}

        /**
         * @brief Arms a plain "action" connection, kept alive with the group.
         *
         * Not a terminator: it never tears the group down. Use it for the wiring that should
         * live exactly as long as the group (it is removed when the group is dismissed).
         * @return *this, for fluent chaining.
         */
        template <typename Sender, typename Signal, typename Receiver, typename Slot>
        EphemeralConnections& on(Sender* sender, Signal signal, Receiver* receiver, Slot slot) {
            // Plain connection: its handle joins the bundle so it is removed together with the
            // group, but it never triggers teardown by itself.
            bundle_->append(QObject::connect(sender, signal, receiver, std::move(slot)));
            return *this;
        }

        /**
         * @brief Arms a "terminator".
         *
         * When @p signal fires, @p predicate is called with the signal's arguments; if it
         * returns true, @p onTrigger is called with the same arguments and then the @e whole
         * group is disconnected. For a parameterless signal, @p predicate and @p onTrigger
         * are called with no arguments.
         *
         * The slot is materialised with the signal's exact parameter types (via
         * FunctionHelperNs::makeTypedCallable over CallableFn::input_t) because Qt's
         * new-style connect cannot bind a generic lambda.
         * @return *this, for fluent chaining.
         */
        template <typename Sender, typename Signal, typename Receiver,
                  typename Predicate, typename OnTrigger>
        EphemeralConnections& until(Sender* sender, Signal signal, Receiver* receiver,
                                    Predicate predicate, OnTrigger onTrigger) {
            auto bundle = bundle_;   // copy the shared_ptr → the slot co-owns the group's lifetime
            using SignalArgs = FunctionTraitsNs::CallableFn::input_t<Signal>;  // std::tuple<Args...>
            auto slot = FunctionHelperNs::makeTypedCallable<SignalArgs>(
                [bundle, predicate = std::move(predicate), onTrigger = std::move(onTrigger)]
                (auto&&... args) {
                    if (!predicate(args...)) { return; }   // not the awaited condition → keep waiting
                    onTrigger(args...);                    // run the finaliser
                    dismissBundle(bundle);             // tear the whole group down (incl. this one)
                });
            bundle_->append(QObject::connect(sender, signal, receiver, std::move(slot)));
            return *this;
        }

        /// @brief Disconnects every connection in the group immediately.
        void dismiss() { dismissBundle(bundle_); }

    private:
        static void dismissBundle(const std::shared_ptr<QList<QMetaObject::Connection>>& bundle) {
            // Disconnecting the very connection whose slot is running right now is safe in Qt
            // (cleanup is deferred). Once every slot is gone, their captured shared_ptr copies
            // drop and the bundle frees itself — no manual lifetime management.
            for (const auto& connection : *bundle) { QObject::disconnect(connection); }
            bundle->clear();
        }

        std::shared_ptr<QList<QMetaObject::Connection>> bundle_;
    };

    // =========================================================================
    // Deprecated set_value overloads (kept for migration)
    // =========================================================================

    template <class Obj, typename T>
    [[deprecated("Use set_value")]]
    inline bool setValue(
        const T& newVal, T& oldVal, SignalParameter<Obj, T> signal, Obj* refObj,
        const CompareFunction<T>& areEqual = getCompareFunctionForSetValueDefault<T>()) noexcept
    {
        if (areEqual(oldVal, newVal) == 0) return false;
        oldVal = newVal;
        emit (refObj->*signal)(oldVal);
        return true;
    }

    template <class Obj, typename T>
    [[deprecated("Use set_value")]]
    inline bool setValue(
        const T& newVal, T& oldVal, SignalNoParameter<Obj> signal, Obj* refObj,
        const CompareFunction<T>& areEqual = getCompareFunctionForSetValueDefault<T>()) noexcept
    {
        if (areEqual(oldVal, newVal) == 0) return false;
        oldVal = newVal;
        emit (refObj->*signal)();
        return true;
    }

    template <class Obj, typename T>
    [[deprecated("Use set_value")]]
    inline bool setValue(
        const T& newVal, ValidationMethod<Obj, T> validate, T& oldVal,
        SignalNoParameter<Obj> signal, Obj* refObj,
        const CompareFunction<T>& areEqual = getCompareFunctionForSetValueDefault<T>()) noexcept
    {
        if (!(refObj->*validate)(newVal)) return false;
        return setValue<Obj, T>(newVal, oldVal, signal, refObj, areEqual);
    }

    template <class Obj, typename T>
    [[deprecated("Use set_value")]]
    inline bool setValue(
        const T& newVal, ValidationMethod<Obj, T> validate, T& oldVal,
        SignalParameter<Obj, T> signal, Obj* refObj,
        const CompareFunction<T>& areEqual = getCompareFunctionForSetValueDefault<T>()) noexcept
    {
        if (!(refObj->*validate)(newVal)) return false;
        return setValue<Obj, T>(newVal, oldVal, signal, refObj, areEqual);
    }

    template <class Obj, typename T>
    [[deprecated("Use set_value")]]
    inline bool setValue(
        const T& newVal, ValidationLambda<T> validate, T& oldVal,
        SignalNoParameter<Obj> signal, Obj* refObj,
        const CompareFunction<T>& areEqual = getCompareFunctionForSetValueDefault<T>()) noexcept
    {
        if (!validate(newVal)) return false;
        return setValue<Obj, T>(newVal, oldVal, signal, refObj, areEqual);
    }

    template <class Obj, typename T>
    [[deprecated("Use set_value")]]
    inline bool setValue(
        const T& newVal, ValidationLambda<T> validate, T& oldVal,
        SignalParameter<Obj, T> signal, Obj* refObj,
        const CompareFunction<T>& areEqual = getCompareFunctionForSetValueDefault<T>()) noexcept
    {
        if (!validate(newVal)) return false;
        return setValue<Obj, T>(newVal, oldVal, signal, refObj, areEqual);
    }

    template <class Obj, typename T, typename FuncBefore, typename... Args>
    [[deprecated("Use set_value")]]
    inline bool setValue(
        const T& newVal, T& oldVal, SignalNoParameter<Obj> signal, Obj* obj,
        const CompareFunction<T>& areEqual,
        FuncBefore funcBefore, Args&&... funcArgs) noexcept
    {
        if (areEqual(oldVal, newVal) == 0) return false;
        funcBefore(std::forward<Args>(funcArgs)...);
        oldVal = newVal;
        emit (obj->*signal)();
        return true;
    }

} // namespace QtSupportHelperNs
