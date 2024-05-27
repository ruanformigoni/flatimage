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
template<typename T>
class compare
{
  private:
    std::reference_wrapper<T> reference_t;
  public:
    // Constructor
    compare(T& t) : reference_t(t) {}
    // Comparison
    template<typename U, std::predicate<U,T> Comp = std::equal_to<>>
    requires std::is_default_constructible_v<Comp>
    bool operator()(U&& u) const
    {
      return Comp{}(std::forward<U>(u), reference_t.get());
    }
}; // }}}

// operator>> {{{
template<typename T, typename U>
requires std::is_default_constructible_v<U>
decltype(auto) operator>>(compare<T> const& partial_comp, U const& u)
{
  // Make it lazy
  return [&](auto&& e) { return (partial_comp(e))? std::make_optional<U>(u) : std::nullopt; };
} // }}}

// operator>>= {{{
template<typename T, typename U>
requires std::regular_invocable<U> and (not std::is_void_v<std::invoke_result_t<U>>)
decltype(auto) operator>>=(compare<T> const& partial_comp, U const& u)
{
  // Make it lazy
  return [&](auto&& e) { return (partial_comp(e))? std::make_optional<std::invoke_result_t<U>>(u()) : std::nullopt; };
} // }}}

// match() {{{
template<typename T, typename... Args>
requires ( std::is_invocable_v<Args,T> and ... )
and ( ns_concept::IsInstanceOf<std::invoke_result_t<Args,T>, std::optional> and ... )
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
