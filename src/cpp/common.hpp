
///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : common
///

#pragma once

#include <format>
#include <iostream>
#include <cstdlib>
#include "std/string.hpp"

namespace
{

template<typename... Args>
struct format_args
{
  // Create a tuple where each type is the result of ns_string::to_string(Args...)
  std::tuple<decltype(ns_string::to_string(std::declval<Args>()))...> m_tuple_args;

  // Initializes the tuple elements to be string representations of the arguments
  format_args(Args&&... args)
    : m_tuple_args(ns_string::to_string(std::forward<Args>(args))...)
  {} // format_args

  // Get underlying arguments
  auto operator*()
  {
    return std::apply([](auto&&... e) { return std::make_format_args(e...); }, m_tuple_args);
  } // operator*
};

} // namespace

// User defined literals {{{

// Print to stdout
inline auto operator""_print(const char* c_str, std::size_t)
{
  return [=]<typename... Args>(Args&&... args)
  {
    std::cout << std::vformat(c_str, *format_args<Args...>(std::forward<Args>(args)...)) << '\n';
  };
}

// Print and exit
inline auto operator""_exit(const char* c_str, std::size_t)
{
  return [=]<typename... Args>(Args&&... args)
  {
    std::cerr << std::vformat(c_str, *format_args<Args...>(std::forward<Args>(args)...)) << '\n';
  };
}

// Format strings with user-defined literals
inline decltype(auto) operator ""_fmt(const char* str, size_t)
{
  return [str]<typename... Args>(Args&&... args)
  {
    return std::vformat(str, *format_args<Args...>(std::forward<Args>(args)...)) ;
  };
} //

// Throw with message
inline decltype(auto) operator ""_throw(const char* str, size_t)
{
  return [str]<typename... Args>(Args&&... args)
  {
    throw std::runtime_error(std::vformat(str, *format_args<Args...>(std::forward<Args>(args)...)));
  };
} 

// }}}

template<ns_concept::AsString T, typename... Args>
inline void print(std::ostream& os, T&& t, Args&&... args)
{
  if constexpr ( sizeof...(args) > 0 )
  {
    os << std::vformat(std::forward<T>(t), *format_args<Args...>(std::forward<Args>(args)...));
  } // if
  else
  {
    os << t;
  } // if
}

template<ns_concept::AsString T, typename... Args>
inline void print(T&& t, Args&&... args)
{
  if constexpr ( sizeof...(args) > 0 )
  {
    std::cout << std::vformat(std::forward<T>(t), *format_args<Args...>(std::forward<Args>(args)...));
  } // if
  else
  {
    std::cout << t;
  } // if
}

template<typename... Args>
inline void print_if(bool cond, Args&&... args)
{
  if ( cond )
  {
    print(std::forward<Args>(args)...);
  } // if
}

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
