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

namespace ns_log
{

enum class Level : int
{
  QUIET,
  VERBOSE,
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

  m_level = Level::DEBUG;

  // File to save logs into
  m_file = fs::path{path_file_self.string() + ".log"};

  "Logger file: {}"_print(m_file);

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

template<ns_concept::AsString... Args>
void info(ns_concept::AsString auto&& format, Args&&... args)
{
  print(instance.m_os, "I::{}\n"_fmt(format), args...);
  print_if((instance.m_level >= Level::VERBOSE), "I::{}\n"_fmt(format), std::forward<Args>(args)...);
} // info

template<ns_concept::AsString... Args>
void error(ns_concept::AsString auto&& format, Args&&... args)
{
  print(instance.m_os, "E::{}\n"_fmt(format), std::forward<Args>(args)...);
} // error

template<ns_concept::AsString... Args>
void debug(ns_concept::AsString auto&& format, Args&&... args)
{
  print(instance.m_os, "D::{}\n"_fmt(format), args...);
  print_if((instance.m_level >= Level::DEBUG), "D::{}\n"_fmt(format), std::forward<Args>(args)...);
} // debug

} // namespace ns_log

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
