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
template<typename Comp, typename... Args>
requires std::is_default_constructible_v<Comp>
class compare
{
  private:
    std::tuple<std::reference_wrapper<Args>...> m_tuple;
  public:
    // Constructor
    compare(Args&... args) : m_tuple(std::reference_wrapper(args)...) {}
    // Comparison
    template<typename U>
    requires ( std::predicate<Comp,U,Args> and ... )
    bool operator()(U&& u) const
    {
      return ( Comp{}(std::forward<U>(u), std::get<std::reference_wrapper<Args>>(m_tuple).get()) or ... );
    }
}; // }}}

// struct less {{{
template<typename... Args>
struct less : public compare<std::less<>, Args...>
{
  less(Args&... args) : compare<std::less<>,Args...>(args...) {}
}; // }}}

// struct greater {{{
template<typename... Args>
struct greater : public compare<std::less<>, Args...>
{
  greater(Args&... args) : compare<std::greater<>,Args...>(args...) {}
}; // }}}

// struct equal {{{
template<typename... Args>
struct equal : public compare<std::equal_to<>, Args...>
{
  equal(Args&... args) : compare<std::equal_to<>,Args...>(args...) {}
}; // }}}

// struct always_true {{{
struct always_true
{
  template<typename... Args>
  bool operator()(Args&&...) { return true; }
}; // }}}

// struct finally {{{
template<typename... Args>
struct finally : public compare<always_true, Args...>
{
  finally(Args&... args) : compare<always_true,Args...>(args...) {}
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
template<typename... T, typename U>
decltype(auto) operator>>=(compare<T...> const& partial_comp, U const& u)
{
  // Make it lazy
  return [&](auto&& e)
  {
    if constexpr ( std::regular_invocable<U> and std::is_void_v<std::invoke_result_t<U>> )
    {
      return (partial_comp(e))? (u(), std::make_optional(true)) : std::nullopt;
    } // if
    else if constexpr ( std::regular_invocable<U> )
    {
      return (partial_comp(e))? std::make_optional<std::invoke_result_t<U>>(u()) : std::nullopt;
    } // else if
    else
    {
      return (partial_comp(e))? u : std::nullopt;
    } // else if
  };
} // }}}

// match() {{{
template<typename T, typename... Args>
requires ( sizeof...(Args) > 0 )
and ( std::is_invocable_v<Args,T> and ... )
and ( ns_concept::IsInstanceOf<std::invoke_result_t<Args,T>, std::optional> and ... )
inline auto match(T&& t, Args&&... args)
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
