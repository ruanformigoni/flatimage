///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : log
///

#pragma once

#include <format>
#include <iostream>
#include "concepts.hpp"
#include "common.hpp"

namespace ns_log
{

template<ns_concept::AsString T, typename... Args>
void info(T&& t, Args&&... args)
{
  print(std::forward<T>(t), std::forward<Args>(args)...);
}

template<ns_concept::AsString T, typename... Args>
void err(T&& t, Args&&... args)
{
  print(std::forward<T>(t), std::forward<Args>(args)...);
}

} // namespace ns_log

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
