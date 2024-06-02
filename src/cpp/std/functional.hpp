///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : functional
///

#pragma once

#include "../common.hpp"
#include "string.hpp"
#include "concept.hpp"

namespace ns_functional
{

// struct PrintLn {{{
struct PrintLn
{
  template<typename T>
  requires ( ns_concept::StringRepresentable<T> or ns_concept::IterableConst<T> )
  void operator()(T&& t) { print("{}\n", ns_string::to_string(t)); }
}; // }}}

// class PushBack {{{
template<ns_concept::Iterable R>
class PushBack
{
  private:
    std::reference_wrapper<R> ref_container;
  public:
    PushBack(R& r) : ref_container(r) {}
    template<typename... Args>
    void operator()(Args&&... args) { ( ref_container.get().push_back(std::forward<Args>(args)), ... ); }
}; // }}}

// class StartsWith {{{
template<ns_concept::StringRepresentable S>
class StartsWith
{
  private:
    std::string str;
  public:
    StartsWith(S&& s) : str(ns_string::to_string(std::forward<S>(s))) {}
    template<typename T>
    bool operator()(T&& t) { return ns_string::to_string(t).starts_with(str); }
}; // }}}

// call_if() {{{
template<std::regular_invocable F>
inline auto call_if(bool cond, F&& f)
{
  if ( not cond ) { return std::nullopt; }

  if constexpr ( std::is_void_v<std::invoke_result_t<F>> )
  {
    f();
    return std::nullopt;
  } // if
  else
  {
    return std::make_optional(f());
  } // else
} // call_if() }}}

} // namespace ns_functional

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
