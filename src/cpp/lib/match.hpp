///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : match
///

#pragma once

#include <functional>
#include <optional>

#include "../macro.hpp"

namespace ns_match
{

// class compare {{{
template<typename T, template<typename U> typename Comp = std::equal_to>
class compare
{
  private:
    std::reference_wrapper<T> reference_t;
  public:
    // Constructor
    compare(T&& t) : reference_t(t) {}
    // Comparison
    template<typename U>
    bool operator()(U&& u) const
    {
      return Comp<U>()(u, reference_t.get());
    }
}; // }}}

// deduction guide class compare {{{
template<typename T, template<typename U> typename Comp = std::equal_to>
compare(T&&) -> compare<std::decay_t<T>, Comp>;
// }}}

// operator>> {{{
template<typename T, typename U>
decltype(auto) operator>>(compare<T> const& partial_eq, U const& u)
{
  // Make it lazy
  return [&](auto&& e) { return (partial_eq(e))? std::make_optional<U>(u) : std::nullopt; };
} // }}}

// operator>>= {{{
template<typename T, typename U>
decltype(auto) operator>>=(compare<T> const& partial_eq, U const& u)
{
  // Make it lazy
  return [&](auto&& e) { return (partial_eq(e))? std::make_optional<std::invoke_result_t<U>>(u()) : std::nullopt; };
} // }}}

// match() {{{
template<typename T, typename... Args>
inline decltype(auto) match(T&& t, Args&&... args)
{
  decltype(std::get<0>(std::forward_as_tuple(args...))(t)) result = std::nullopt;

  // Lambda to check and return std::optional if it has a value
  auto check_and_return = [&](auto&& arg) -> bool
  {
    result = arg(t);
    return result.has_value();
  };

  // Use fold expression to evaluate each argument
  (check_and_return(args) || ...);

  // Check if has a match
  ethrow_if(not result.has_value(), "Could not match '{}'"_fmt(t));

  return result;
} // match() }}}

} // namespace ns_match

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
