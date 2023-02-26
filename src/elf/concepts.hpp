// vim: set expandtab fdm=marker ts=2 sw=2 tw=80 et :
//
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : concepts
// @created     : sábado jul 25, 2020 21:08:03 -03
//
// BSD 2-Clause License

// Copyright (c) 2020, Ruan Evangelista Formigoni
// All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:

// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.

// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <iostream>
#include <concepts>
#include <type_traits>
#include <iterator>
#include <string>

#include <fmt/ranges.h>

// namespace concepts {{{
namespace concepts
{

template<typename... P>
concept Printable =
requires(P&&... p)
{
  ((fmt::print("{}",std::forward<P>(p))), ...);
};

// Basic Types {{{
template<typename T, typename U = std::decay_t<T>>
concept Boolean = std::same_as<std::decay_t<T>,bool>;

template<typename T, typename U = std::decay_t<T>>
concept Integral = std::integral<U>;

template<typename T, typename U = std::decay_t<T>>
concept SignedIntegral = std::signed_integral<U>;

template<typename T, typename U = std::decay_t<T>>
concept Float = std::floating_point<U>;

template<typename T, typename U = std::decay_t<T>>
concept String =
requires(U u) { { std::string{u} } -> std::same_as<std::string>; };
// }}}

// Ranges {{{
template<typename T>
concept ForwardIterator = std::forward_iterator<T>;

template<typename T>
concept InputIterator = std::input_iterator<T>;

template<typename T>
concept Range = std::ranges::range<T>;

template<typename M, typename R = typename std::decay_t<M>::value_type::value_type>
concept Matrix = Range<M> && Range<R>;
// }}}

// Type Traits {{{

// Trait {{{
template<typename T, typename U = std::decay_t<T>>
concept Arithmetic =
requires(U u)
{
  { u+u } -> std::same_as<U>;
  { u-u } -> std::same_as<U>;
  { u*u } -> std::same_as<U>;
  { u/u } -> std::same_as<U>;
};

template<typename T>
concept Set =
requires
{
  typename std::decay_t<T>::key_type;
};

template<typename T>
concept Map =
requires
{
  typename std::decay_t<T>::key_type;
  typename std::decay_t<T>::mapped_type;
};

// }}}

// Relationship {{{

//
// Check if decayed types are the same
//
template<typename T, typename U>
concept SameAs = std::same_as<std::decay_t<T>,std::decay_t<U>>;

//
// Check if T is convertible to U
//
template<typename T, typename U>
concept ConvertibleTo = std::convertible_to<std::decay_t<T>,std::decay_t<U>>;

//
// Check if  U is a pair (T,T)
//
template<typename T, typename U>
concept IsPairOf = requires(U u)
{
  {u.first} -> std::convertible_to<std::decay_t<T>>;
  {u.second} -> std::convertible_to<std::decay_t<T>>;
};

//
// Check if  U is a pair ((T,T),(T,T),...)
//
template<typename T, typename... U>
concept IsPairsOf = requires(U... u)
{
  { ((u.first),...) } -> std::convertible_to<std::decay_t<T>>;
  { ((u.second),...) } -> std::convertible_to<std::decay_t<T>>;
};
// }}}

// Function {{{

//
// Check if Predicate P can be called with arguments of type Args...
//
template<typename F, typename... Args>
concept Predicate = std::predicate<F,Args...>;

template<typename F, typename Arg>
concept UnaryPredicate = std::predicate<F,Arg>;

//
// Check if a function F is callable with arguments Args...
//
template<typename F, typename... Args>
concept CallableWith =
  requires(F f, Args&&... args) { f(std::forward<Args>(args)...); };

//
// Check if a value T, is returned from a function F, with arguments Args...
//
template<typename T, typename F, typename... Args>
concept Returns = requires(F f, Args&&... args)
{
  { f(std::forward<Args>(args)...) } -> std::same_as<std::decay_t<T>>;
};
// }}}

// }}}

} // namespace concepts }}}
