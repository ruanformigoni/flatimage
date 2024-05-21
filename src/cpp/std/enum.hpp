///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : enum
///

#pragma once

#include "concepts.hpp"

namespace ns_enum
{

// options_and() {{{
template<ns_concept::Enum... Args>
constexpr bool options_and(Args&&... flags)
{
  return ( static_cast<int>(flags) & ... );
} // options_and() }}}

// options_or() {{{
template<ns_concept::Enum... Args>
constexpr bool options_or(Args&&... flags)
{
  return ( static_cast<int>(flags) | ... );
} // options_or() }}}

} // namespace ns_enum

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
