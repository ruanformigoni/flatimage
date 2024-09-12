///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : match
///

#pragma once

#include <functional>
#include <optional>

#include "log.hpp"
#include "../macro.hpp"
#include "../std/concept.hpp"

namespace ns_match
{

// class compare {{{
template<typename Comp, typename... Args>
requires std::is_default_constructible_v<Comp>
class compare
{
  private:
    std::tuple<std::decay_t<Args>...> m_tuple;
  public:
    // Constructor
    compare(Args&&... args)
      : m_tuple(std::forward<Args>(args)...)
    {}
    // Comparison
    template<typename U>
    requires ( std::predicate<Comp,U,Args> and ... )
    bool operator()(U&& u) const
    {
      return std::apply([&](auto&&... e){ return (Comp{}(e, u) or ...); }, m_tuple);
    } // operator()
}; // }}}

// fn: equal() {{{
template<typename... Args>
requires ns_concept::Uniform<std::remove_cvref_t<Args>...>
  and (not ns_concept::SameAs<char const*, std::decay_t<Args>...>)
[[nodiscard]] decltype(auto) equal(Args&&... args)
{
  return compare<std::equal_to<>,Args...>(std::forward<Args>(args)...);
}; // fn: equal() }}}

// fn: equal() {{{
template<typename... Args>
requires ns_concept::SameAs<char const*, std::decay_t<Args>...>
[[nodiscard]] decltype(auto) equal(Args&&... args)
{
  auto to_string = [](auto&& e){ return std::string(e); };
  return compare<std::equal_to<>, std::invoke_result_t<decltype(to_string), Args>...>(std::string(args)...);
}; // fn: equal() }}}

// operator>>= {{{
template<typename... T, typename U>
[[nodiscard]] decltype(auto) operator>>=(compare<T...> const& partial_comp, U const& u)
{
  // Make it lazy
  return [&](auto&& e)
  {
    if constexpr ( std::regular_invocable<U> )
    {
      if constexpr ( std::is_void_v<std::invoke_result_t<U>>  )
      {
        return (partial_comp(e))? (u(), std::make_optional(true)) : std::nullopt;
      } // if
      else
      {
        return (partial_comp(e))? std::make_optional<std::invoke_result_t<U>>(u()) : std::nullopt;
      } // else
    } // if
    else
    {
      return (partial_comp(e))? std::make_optional(u) : std::nullopt;
    } // else if
  };
} // }}}

// match() {{{
template<typename T, typename... Args>
requires ( sizeof...(Args) > 0 )
and ( std::is_invocable_v<Args,T> and ... )
and ( ns_concept::IsInstanceOf<std::invoke_result_t<Args,T>, std::optional> and ... )
[[nodiscard]] auto match(T&& t, Args&&... args)
  -> typename std::invoke_result_t<std::tuple_element_t<0,std::tuple<Args...>>, T>::value_type
{
  std::invoke_result_t<std::tuple_element_t<0,std::tuple<Args...>>, T> result = std::nullopt;

  // Use fold expression to evaluate each argument
  ((result = args(t), result.has_value()) || ...);

  // Check if has match
  ethrow_if(not result.has_value(), "Could not match '{}'"_fmt(t));

  return *result;
} // match() }}}

} // namespace ns_match

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
