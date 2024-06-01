///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : log
///

#pragma once

#include <format>
#include <iostream>
#include <filesystem>
#include <fstream>

#include "../common.hpp"
#include "../std/concepts.hpp"
#include "../std/filesystem.hpp"
#include "../std/exception.hpp"

namespace ns_log
{

enum class Level : int
{
  QUIET,
  ERROR,
  INFO,
  DEBUG,
};

namespace
{

namespace fs = std::filesystem;

struct Logger
{
  fs::path m_file;
  std::ofstream m_os;
  Level m_level;
  Logger();
};

inline Logger::Logger()
{
  // Dir to self
  fs::path path_file_self = ns_fs::ns_path::file_self()._ret;

  m_level = Level::QUIET;

  // File to save logs into
  m_file = fs::path{path_file_self.string() + ".log"};

  if ( const char* var = std::getenv("FIM_DEBUG"); var && std::string_view{var} == "1" )
  {
    "Logger file: {}\n"_print(m_file);
  } // if

  // File output stream
  m_os = std::ofstream{m_file};

  if( m_os.bad() ) { std::runtime_error("Could not open file '{}'"_fmt(m_file)); };
} // Logger

static Logger instance;

} // namespace

inline void set_level(Level level)
{
  instance.m_level = level;
} // info

template<ns_concept::StringRepresentable T, typename... Args>
requires ( ( ns_concept::StringRepresentable<Args> or ns_concept::IterableConst<Args> ) and ... )
void info(T&& format, Args&&... args)
{
  print(instance.m_os, "I::{}\n"_fmt(format), args...);
  print_if((instance.m_level >= Level::INFO), "I::{}\n"_fmt(format), std::forward<Args>(args)...);
} // info

template<ns_concept::StringRepresentable T, typename... Args>
requires ( ( ns_concept::StringRepresentable<Args> or ns_concept::IterableConst<Args> ) and ... )
void error(T&& format, Args&&... args)
{
  print(instance.m_os, "E::{}\n"_fmt(format), args...);
  print_if((instance.m_level >= Level::ERROR), "E::{}\n"_fmt(format), std::forward<Args>(args)...);
} // error

template<ns_concept::StringRepresentable T, typename... Args>
requires ( ( ns_concept::StringRepresentable<Args> or ns_concept::IterableConst<Args> ) and ... )
void debug(T&& format, Args&&... args)
{
  print(instance.m_os, "D::{}\n"_fmt(format), args...);
  print_if((instance.m_level >= Level::DEBUG), "D::{}\n"_fmt(format), std::forward<Args>(args)...);
} // debug

template<typename F, typename... Args>
requires std::is_invocable_v<F, Args...>
decltype(auto) exception(F&& f, Args... args)
{
  auto expected = ns_exception::to_expected(f, std::forward<Args>(args)...);
  if ( not expected ) { error(expected.error()); }
  return expected;
} // debug

} // namespace ns_log

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
