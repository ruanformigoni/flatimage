///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : functional
///

#include "../common.hpp"
#include "string.hpp"
#include "concepts.hpp"

namespace ns_functional
{

struct Print
{
  template<typename T>
  requires ( ns_concept::StringRepresentable<T> or ns_concept::IterableConst<T> )
  void operator()(T&& t)
  {
    print("{}\n", ns_string::to_string(t));
  }
};

} // namespace ns_functional

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
