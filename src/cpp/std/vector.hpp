///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : vector
///

#pragma once

#include <vector>

#include "concept.hpp"
#include "string.hpp"

namespace ns_vector
{

// This is available in C++23, not implemented yet on gcc however
template<ns_concept::IterableConst R1, ns_concept::IterableConst R2>
inline void append_range(R1& to, R2 const& from)
{
  std::ranges::for_each(from, [&](auto&& e){ to.push_back(e); });
} // append_range

template<ns_concept::IterableConst R, typename... Args>
inline void push_back(R& r, Args&&... args)
{
  ( r.push_back(std::forward<Args>(args)), ... );
} // push_back

template<ns_concept::Iterable R = std::vector<std::string>>
inline R from_string(ns_concept::StringRepresentable auto&& t, char delimiter)
{
  R tokens;
  std::string token;
  std::istringstream stream_token(ns_string::to_string(t));

  while (std::getline(stream_token, token, delimiter))
  {
    tokens.push_back(token);
  } // while

  return tokens;
} // from_string

} // namespace ns_vector

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
