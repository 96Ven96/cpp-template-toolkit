# TemplateHelper

A small, header-only **C++ template metaprogramming toolkit**.

The core of the library (five of the eight headers) is pure standard C++17/20
and has no third-party dependencies. The remaining three headers add an
**optional Qt integration layer** (Qt 5.15+ / 6.x) for the kind of patterns
that recur in any Qt codebase — `Q_PROPERTY` setters that emit a signal only
on actual changes, safe `QVariant` round-trips, ownership helpers, and so on.

You can use the toolkit in a non-Qt project by simply not including the
`*_qt.h` / `qt_*` / `conversion_types.h` headers.

It collects patterns I have found myself rewriting across several real
projects — function-signature introspection, compile-time string builders,
flag-enum validation, null-safe invocation, contiguous-range partitioning —
and packages them as a set of small, well-scoped, individually documented
headers.

The library is **header-only**, requires no build step, and supports C++17
as a baseline, with optional C++20 / C++23 paths that are enabled
automatically when the compiler advertises them.

---

## Why this exists

A lot of "small" C++ patterns get rewritten in every project — slightly
differently each time, never quite documented. Two examples.

**Compile-time identifier from a number**, useful for generating keys, paths
or static strings:

```cpp
constexpr auto kVersion = ConstexprHelperNs::to_array_from_int<42>();
// kVersion == {'4','2','\0'}, usable as a constexpr string_view
```

**Q_PROPERTY setter, the Qt staple**:

```cpp
// Without the toolkit:
void MyObject::setName(const QString& v) {
    if (m_name == v) return;
    m_name = v;
    emit nameChanged();
}

// With the toolkit:
void MyObject::setName(const QString& v) {
    QtSupportHelperNs::set_value(v, m_name, &MyObject::nameChanged, this);
}
```

The same philosophy applies to every module: each one removes a small piece
of boilerplate that is otherwise rewritten by hand in every project.

---

## Possible applications

### Pure C++ (no Qt required)

- **Generic function introspection** — `function_signature` extracts return
  type, argument tuple and arity from any callable, with full coverage of
  member-function qualifiers. Useful for building reflection-like utilities,
  serializers, RPC layers, or test frameworks.
- **Compile-time configuration** — integer-to-array conversion, array
  concatenation and `consteval` search support generating identifiers,
  paths or lookup tables entirely at compile time.
- **Bit-flag enums** — `check_bitwise_unique` validates flag-enum
  declarations at compile time; `bitwiseOnEnum` composes flags safely
  through their underlying type.
- **Safer callable utilities** — `safe_invoke` uniformly dispatches member
  vs. free functions; `invokeIfNotNull` guards against null receivers;
  `tuple_for_each` iterates compile-time tuples.
- **Range partitioning** — `computeContiguousRanges` splits an arbitrary
  range into maximal contiguous sub-ranges according to a user predicate
  (selection ranges in UIs, time-series gap detection, etc.).
- **Educational / portfolio reference** — every header is self-contained
  and documented; the code base demonstrates a wide spectrum of modern C++
  techniques (concepts, fold expressions, `consteval`, SFINAE-by-trait,
  member-pointer dissection, perfect forwarding, etc.).

### Qt-specific (Qt headers required)

- **Qt application development** — drastically reduce boilerplate in
  models, view-models and any class that exposes `Q_PROPERTY` properties.
- **Serialization layers** — `toVariant` / `fromVariant` give a single,
  type-safe path between domain types and `QVariant`-based storage
  (`QSettings`, JSON, QML interop).
- **Generic algorithms over Qt and STL containers** — concepts such as
  `IsAContainer` let templated code accept both worlds transparently.

---

## File overview

The repository is split into eight focused headers plus one umbrella file
that includes them all for backward compatibility.

The "Qt required?" column tells you at a glance which headers you can pull
into a non-Qt project:

| Header | Namespace | Qt required? |
|---|---|---|
| `concepts_std.h` | `StdConceptNs_` | No |
| `function_traits.h` | `FunctionTraitsNs` | No |
| `function_helper.h` | `FunctionHelperNs` | No |
| `constexpr_helper.h` | `ConstexprHelperNs` | No |
| `types_helper.h` | `TypesHelperNs` | No |
| `concepts_qt.h` | `QtConceptNs_` | Yes |
| `conversion_types.h` | `ConversionTypesNs` | Yes |
| `qt_support_helper.h` | `QtSupportHelperNs` | Yes |

### `concepts_std.h` — namespace `StdConceptNs_`

Pure-standard-library concepts (C++20+).
Contains: `IsStdStringLike`, `AllSameType`, `IsScopedEnum`,
`IndexableConstexprContainer`.

### `concepts_qt.h` — namespace `QtConceptNs_`

Concepts that require Qt headers (C++20+).
Contains: `IsQString`, `IsAQtContainer`, `IsAContainer`, `IsStringLike`,
`QtSignalLike`, `QVariantCompatible`, the validator-family concepts
(`IsNullValidator`, `IsCallableValidator`, `IsValidValidator`) and the
helper function `isValidValue`.

### `function_traits.h` — namespace `FunctionTraitsNs`

Type traits for inspecting callables and member pointers.
- Sub-namespace `CallableFn` exposes `function_signature`, a trait that
  extracts return type, argument tuple and arity from **any** callable —
  free function, function pointer, `std::function`, lambda, or member
  function with every combination of `const` / `volatile` / `&` / `&&` /
  `noexcept`.
- Top-level traits: `is_member_of_class`, `is_member_object_of_class`,
  `member_object_type`, `is_std_optional`, `optional_value`,
  `is_nullary_invocable_tuple`, `is_invocable_tuple`,
  `are_connectable_for_input`, `underlying_or_self_t`,
  `is_c_array_of_char`, `is_std_array_char`.

### `function_helper.h` — namespace `FunctionHelperNs`

Callable invocation helpers.
Contains: `safe_invoke` (uniform dispatch between free and member
functions), `invokeIfNotNull` (null-safe member invocation),
`callFromTuple` (tuple-unpacking call), `for_each_in_constexpr_array`,
`tuple_for_each`.

### `constexpr_helper.h` — namespace `ConstexprHelperNs`

Compile-time string / array utilities.
Contains: `intLength`, `to_array_from_int`, `stringViewFromConstexpr`,
`concat`, `concatAll`, `totalSize`, `join` (char-array joining with a
separator), and the C++20 `consteval` search functions `constexprIndexOf`
and `hasFoundAnIndex`.

### `types_helper.h` — namespace `TypesHelperNs`

Type-level and runtime helpers for enums, indices and containers.
Contains: `check_bitwise_unique`, `areEqualEnum`, `bitwiseOnEnum` (with
`bitwiseOr/And…` and `…WithCast` variants), `isValidIndex` /
`areValidIndices`, `areValidPointers`, `isUniquePointerContainer`,
`computeContiguousRanges`, `equalizeDimension` /
`equalizeDimensions`.

### `conversion_types.h` — namespace `ConversionTypesNs`

Conversions between Qt, STL and `QVariant`.
- Sub-namespace `Qt_`: `toQString` (accepts every common string-like
  type), `fromVariant`, `toVariant`, `isValidFormat<QDate|QTime>`.
- Top-level: `toVariantList`, `toVariantListWithCallable` (for ranges and
  containers), `objectPtr` (returns a pointer whether the input is already
  one or not).

### `qt_support_helper.h` — namespace `QtSupportHelperNs`

The largest module, focused on `Q_PROPERTY` ergonomics.
- **Type aliases** for getter / setter / signal / validator member
  pointers (`GetParFnRefNoArgs`, `SetFunctionConstRef`,
  `SignalParameter`, `ValidationLambda`, etc.).
- **Comparison defaults**: `g_EquivalentFunctionDefault`,
  `g_EquivalentQrealDefault`, `g_CompareFunctionDefault`,
  `getCompareFunctionForSetValueDefault` (NaN-safe, fuzzy compare for
  floats).
- **Core helpers**: `assign_if_diff`, `set_value` (with and without a
  validator), `assignOwnership`, `deletePointerFromVariant`,
  `toggleConnection` (compile-time and runtime variants).
- **Key/accessor structs** (`KeyGetFn…`, `KeySetFn…`) used by
  serialisation helpers.
- The legacy `setValue` overloads are kept with `[[deprecated]]` for
  migration.

### `templatedefines.h`

Umbrella include that pulls in every module above. Existing code that
references this single header continues to compile unchanged.

---

## Usage examples

**Property setter with validation**

```cpp
bool MyModel::setAge(int v) {
    return QtSupportHelperNs::set_value(
        v, m_age, &MyModel::ageChanged, this,
        [](int x) { return x >= 0 && x < 150; });
}
```

**Compile-time identifier generation**

```cpp
constexpr auto kVersion = ConstexprHelperNs::to_array_from_int<42>();
constexpr auto kKey     = ConstexprHelperNs::join(
    std::array{'.', '\0'},
    std::array{'a','p','p','\0'},
    std::array{'v','\0'},
    kVersion);
// kKey == "app.v.42"
```

**Bit-flag enum validation**

```cpp
enum class Perm : unsigned { Read = 1, Write = 2, Exec = 4 };
static_assert(TypesHelperNs::check_bitwise_unique_v<
    Perm, Perm::Read, Perm::Write, Perm::Exec>);
```

**Generic `QVariant` round-trip**

```cpp
QVariant v = ConversionTypesNs::Qt_::toVariant(MyEnum::Value);
auto back  = ConversionTypesNs::Qt_::fromVariant<MyEnum>(v);
```

---

## Requirements

- A C++17 compiler (GCC 9+, Clang 10+, MSVC 19.20+).
- **Optional**: Qt 5.15 or Qt 6.x — only required if you include the three
  Qt-aware headers (`concepts_qt.h`, `conversion_types.h`,
  `qt_support_helper.h`). The other five headers compile against any
  standard C++ toolchain.
- **Optional**: C++20 / C++23 — enables the concept-constrained APIs and
  `consteval` helpers automatically.

No build system is needed; drop the headers into your include path.

---

## Status

This started as a single growing header (`templatedefines.h`) used in a
real C++/Qt project at work. It has been **rewritten and reorganised**
into the current form so that the Qt-free and Qt-aware parts are cleanly
separated, the patterns are easy to read, easy to reuse, and easy to
document. A handful of small bugs in the original were fixed along the
way (most notably a wrongly-returned value in date-format validation and
a `static`-vs-`inline` issue on header-defined functions).

The code is published as a portfolio reference — it is not intended to be
a fully maintained library. Feedback and suggestions are welcome.

---

## License

Released under the [MIT License](LICENSE) — free to use, modify, and
redistribute, including in commercial and closed-source projects. The only
requirement is to keep the copyright notice in copies and derivatives.

If you find any of this useful, that's reward enough — no attribution beyond
what the licence requires is expected.

---

## Contact

- GitHub — [@96Ven96](https://github.com/96Ven96)
- LinkedIn — [Leonardo Rossi](https://www.linkedin.com/in/leonardo-rossi-262641203)
