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

namespace
{

template<typename T, template<typename...> typename U>
inline constexpr bool is_instance_of_v = std::false_type {};

template<template<typename...> typename U, typename... Args>
inline constexpr bool is_instance_of_v<U<Args...>,U> = std::true_type {};

} // namespace

template<typename T>
concept BooleanTestable = requires(T t)
{
  { static_cast<bool>(t) } -> std::same_as<bool>;
};

template<typename T>
concept Enum = std::is_enum_v<T>;

template <typename T>
concept Variant = requires(T){ std::variant_size_v<T>; };

template<typename T, typename... U>
concept SameAs = ( std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<U>> and ... );

template<typename... U>
concept Uniform = sizeof...(U) > 0
 and std::conjunction_v<std::is_same<std::tuple_element_t<0, std::tuple<U...>>, U>...>;

template<typename T, template<typename...> typename U>
concept IsInstanceOf = is_instance_of_v<T, U>;

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
concept StringRepresentable = StringConvertible<T> or StringConstructible<T> or Numeric<T> or StreamInsertable<T>;

} // namespace ns_concept

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
