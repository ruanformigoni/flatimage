///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : concepts
///

#pragma once

#include <string>
#include <type_traits>

namespace ns_concept
{
template<typename T>
concept Enum = std::is_enum_v<T>;

template<typename T>
concept IterableForward = std::forward_iterator<T>;

template<typename T>
concept StringConvertible = std::is_convertible_v<std::decay_t<T>, std::string>;

template<typename T>
concept StringConstructible = std::constructible_from<std::string, std::decay_t<T>>;

template<typename T>
concept Numeric = std::integral<std::decay_t<T>>
  or std::floating_point<std::decay_t<T>>;

template<typename T>
concept StreamInsertable = requires(T t, std::ostream& os)
{
  { os << t } -> std::same_as<std::ostream&>;
};

template<typename T>
concept AsString = StringConvertible<T> or StringConstructible<T> or Numeric<T> or StreamInsertable<T>;

} // namespace ns_concept

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
