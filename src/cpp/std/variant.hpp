///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : variant
///

#pragma once

#include <variant>
#include <optional>

#include "concept.hpp"

namespace ns_variant
{

template<typename T, ns_concept::Variant U>
std::optional<T> get_if_holds_alternative(U const& var)
{
  return (std::holds_alternative<T>(var))? std::make_optional(std::get<T>(var)) : std::nullopt;
}


} // namespace ns_variant

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
