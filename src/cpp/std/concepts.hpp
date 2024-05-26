///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : concepts
///

#pragma once

#include <string>
#include <type_traits>
#include <variant>

namespace ns_concept
{
template<typename T>
concept Enum = std::is_enum_v<T>;

template <typename T>
concept Variant = requires(T){ std::variant_size_v<T>; };

template<typename T, typename U>
concept SameAs = std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<U>>;

template<typename T>
concept Iterable = requires(T t)
{
  { t.begin() } -> std::input_iterator;
  { t.end() } -> std::input_iterator;
};

template<typename T>
concept IterableConst = requires(T t)
{
  { t.cbegin() } -> std::input_iterator;
  { t.cend() } -> std::input_iterator;
};

template <typename T, typename U>
concept IsContainerOf = requires
{
  typename T::value_type;
  requires std::same_as<typename T::value_type, U>;
  requires Iterable<T>;
};

template<typename T>
concept StringConvertible = std::is_convertible_v<std::decay_t<T>, std::string>;

template<typename T>
concept StringConstructible = std::constructible_from<std::string, std::decay_t<T>>;

template<typename T>
concept Numeric = std::integral<std::remove_cvref_t<T>> or std::floating_point<std::remove_cvref_t<T>>;

template<typename T>
concept StreamInsertable = requires(T t, std::ostream& os)
{
  { os << t } -> std::same_as<std::ostream&>;
};

template<typename T>
concept AsString = StringConvertible<T> or StringConstructible<T> or Numeric<T> or StreamInsertable<T>;

} // namespace ns_concept

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
