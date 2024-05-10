
///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : common
///

#pragma once

#include <format>
#include <iostream>
#include <cstdlib>
#include "string.hpp"

namespace
{

template<typename... Args>
auto format_args(Args&&... args)
{
  auto tuple_strings = ns_string::to_tuple(std::forward<Args>(args)...);
  return std::apply([](auto&&... args){ return std::make_format_args(args...); }, tuple_strings);
} // format_args

}

// User defined literals {{{

// Print to stdout
inline auto operator""_print(const char* c_str, std::size_t)
{
  return [=]<typename... Args>(Args&&... args)
  {
    std::cout << std::vformat(c_str, format_args(std::forward<Args>(args)...)) << '\n';
  };
}

// Print to stdout
inline auto operator""_exit(const char* c_str, std::size_t)
{
  return [=]<typename... Args>(Args&&... args)
  {
    std::cerr << std::vformat(c_str, format_args(std::forward<Args>(args)...)) << '\n';
  };
}

// Format strings with user-defined literals
inline decltype(auto) operator ""_fmt(const char* str, size_t)
{
  return [str]<typename... Args>(Args&&... args)
  {
    return std::vformat(str, format_args(std::forward<Args>(args)...)) ;
  };
} //

// Format strings with user-defined literals
inline decltype(auto) operator ""_throw(const char* str, size_t)
{
  return [str]<typename... Args>(Args&&... args)
  {
    throw std::runtime_error(std::vformat(str, format_args(std::forward<Args>(args)...)));
  };
} 

// }}}

template<ns_concept::AsString T, typename... Args>
inline void print(std::ostream& os, T&& t, Args&&... args)
{
  os << std::vformat(std::forward<T>(t), format_args(std::forward<Args>(args)...));
}

template<ns_concept::AsString T, typename... Args>
inline void print(T&& t, Args&&... args)
{
  std::cout << std::vformat(std::forward<T>(t), format_args(std::forward<Args>(args)...)) << '\n';
}

#define return_if(cond, ...) \
  if (cond) { return __VA_ARGS__; }

#define return_if_else(cond, val1, val2) \
  if (cond) { return val1; } else { return val2; }

#define break_if(cond) \
  if ( (cond) ) { break; }

#define continue_if(cond) \
  if ( (cond) ) { continue; }

#define assign_if(cond, var, val) \
  if ( cond ) { var = val; }

#define assign_or_return(val, cond, ret) \
  val; if ( not cond ) { return ret; }

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
