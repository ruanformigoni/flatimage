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

template<ns_concept::IterableConst R, typename... Args>
inline void push_back(R& r, Args&&... args)
{
  ( r.push_back(std::forward<Args>(args)), ... );
} // push_back

template<ns_concept::StringRepresentable T>
inline std::vector<std::string> from_string(T&& t, char delimiter)
{
  std::vector<std::string> tokens;
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
