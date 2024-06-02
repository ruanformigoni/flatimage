///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : string
///

#pragma once

#include <algorithm>
#include <sstream>
#include <format>
#include <vector>

#include "concept.hpp"

namespace ns_string
{

// replace_substrings() {{{
inline std::string replace_substrings(std::string string
  , std::string const& substring
  , std::string const& replacement)
{
  auto pos = string.find(substring);

  while ( pos != std::string::npos )
  {
    string.replace(pos, substring.size(), replacement);
    pos = string.find(substring);
  } // while

  return string;
} // replace_substrings()  }}}

// to_string() {{{
template<typename T>
inline std::string to_string(T&& t)
{
  if constexpr ( ns_concept::StringConvertible<T> )
  {
    return t;
  } // if
  else if constexpr ( ns_concept::StringConstructible<T> )
  {
    return std::string{t};
  } // else if
  else if constexpr ( ns_concept::Numeric<T> )
  {
    return std::to_string(t);
  } // else if 
  else if constexpr ( ns_concept::StreamInsertable<T> )
  {
    std::stringstream ss;
    ss << t;
    return ss.str();
  } // else if 
  else if constexpr ( ns_concept::IterableConst<T> )
  {
    std::stringstream ss;
    ss << '[';
    std::for_each(t.cbegin(), t.cend(), [&](auto&& e){ ss << std::format("'{}',", e); });
    ss << ']';
    return ss.str();
  } // else if 
  
  throw std::runtime_error(std::string{"Cannot convert valid string, type: "} + typeid(T).name());
} // to_string() }}}

// to_tuple() {{{
template<typename... Args>
inline auto to_tuple(Args&&... args)
{
  return std::make_tuple(to_string(std::forward<Args>(args))...);
} // to_tuple() }}}

// to_lower() {{{
template<ns_concept::StringRepresentable T>
std::string to_lower(T&& t)
{
  std::string ret = to_string(t);
  std::ranges::for_each(ret, [](auto& c){ c = std::tolower(c); });
  return ret;
} // to_lower() }}}

// to_upper() {{{
template<ns_concept::StringRepresentable T>
std::string to_upper(T&& t)
{
  std::string ret = to_string(t);
  std::ranges::for_each(ret, [](auto& c){ c = std::toupper(c); });
  return ret;
} // to_upper() }}}

// to_pair() {{{
template<ns_concept::StringRepresentable T>
std::pair<T,T> to_pair(T&& t, char delimiter)
{
  std::vector<T> tokens;
  std::string token;
  std::istringstream stream_token(ns_string::to_string(t));

  while (std::getline(stream_token, token, delimiter))
  {
    tokens.push_back(token);
  } // while

  ethrow_if(tokens.size() != 2, "Pair from '{}' with delimiter '{}' could not split to 2 elements", t, delimiter);
  return std::make_pair(tokens.front(), tokens.back());
} // to_pair() }}}

// from_container() {{{
template<typename T>
std::string from_container(T&& t, char sep = ' ')
{
  std::stringstream ret;
  for( auto it = t.begin(); it != t.end(); ++it )
  {
    ret << *it;
    if ( std::next(it) != t.end() ) { ret << sep; }
  } // if
  return ret.str();
} // from_container() }}}

} // namespace ns_string

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
