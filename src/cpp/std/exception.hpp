///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : exception
///

#pragma once

#include <concepts>
#include <exception>

namespace ns_exception
{

template<std::regular_invocable F>
requires std::is_default_constructible_v<std::invoke_result_t<F>>
decltype(auto) or_default(F&& f)
{
  using Type = std::invoke_result_t<F>;

  try { return f(); } catch (std::exception const&) {}

  return Type{};
} // function: to_optional

template<std::regular_invocable F, typename T>
requires std::is_default_constructible_v<T>
and std::is_convertible_v<T, std::invoke_result_t<F>>
decltype(auto) or_else(F&& f, T&& t)
{
  try { return f(); } catch (std::exception const&) {}

  return t;
} // function: to_optional

} // namespace ns_exception

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
