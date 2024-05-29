///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : exception
///

#pragma once

#include <concepts>
#include <exception>
#include <optional>

namespace ns_exception
{

template<std::regular_invocable F>
void ignore(F&& f)
{
  try { f(); } catch (std::exception const&) {}
} // function: ignore

template<std::regular_invocable F>
requires std::is_default_constructible_v<std::invoke_result_t<F>>
auto or_default(F&& f) -> std::invoke_result_t<F>
{
  try { return f(); } catch (std::exception const&) {}
  return std::invoke_result_t<F>{};
} // function: or_default

template<std::regular_invocable F, typename T>
requires std::is_default_constructible_v<T>
and std::is_convertible_v<T, std::invoke_result_t<F>>
T or_else(F&& f, T&& t)
{
  try { return f(); } catch (std::exception const&) {}
  return t;
} // function: or_else

template<std::regular_invocable F>
auto to_optional(F&& f) -> std::optional<std::invoke_result_t<F>>
{
  try { return std::make_optional(f()); } catch (std::exception const&) {}
  return std::nullopt;
} // function: to_optional

} // namespace ns_exception

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
