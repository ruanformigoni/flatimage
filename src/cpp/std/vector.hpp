///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : vector
///

#pragma once

#include "concepts.hpp"

namespace ns_vector
{

template<ns_concept::IterableConst R, typename... Args>
inline void push_back(R& r, Args&&... args)
{
  ( r.push_back(std::forward<Args>(args)), ... );
}

} // namespace ns_vector

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
