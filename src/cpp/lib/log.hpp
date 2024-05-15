///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : log
///

#pragma once

#include <filesystem>
#include <fstream>

#include "../common.hpp"
#include "../std/concept.hpp"
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
  auto expected_path_file_self = ns_filesystem::ns_path::file_self();
  if(not expected_path_file_self)
  {
    throw std::runtime_error(expected_path_file_self.error());
  } // if

  m_level = Level::QUIET;

  // File to save logs into
  m_file = fs::path{expected_path_file_self->string() + ".log"};
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

// class: Location {{{
class Location
{
  private:
    fs::path m_str_file;
    uint32_t m_line;

  public:
    Location(
        char const* str_file = __builtin_FILE()
      , uint32_t line = __builtin_LINE()
    )
      : m_str_file(fs::path(str_file).filename())
      , m_line(line)
    {}

    auto get() const
    {
      return "{}::{}"_fmt(m_str_file, m_line);
    } // get
}; // class: Location }}}

class info
{
  private:
    Location m_loc;

  public:
    info(Location location = {}) : m_loc(location) {}
    template<ns_concept::StringRepresentable T, typename... Args >
    requires ( ( ns_concept::StringRepresentable<Args> or ns_concept::IterableConst<Args> ) and ... )
    void operator()(T&& format, Args&&... args)
    {
      print(instance.m_os, "I::{}::{}\n"_fmt(m_loc.get(), format), args...);
      print_if((instance.m_level >= Level::INFO), "I::{}::{}\n"_fmt(m_loc.get(), format), std::forward<Args>(args)...);
    } // info
};

class error
{
  private:
    Location m_loc;

  public:
    error(Location location = {}) : m_loc(location) {}
    template<ns_concept::StringRepresentable T, typename... Args>
    requires ( ( ns_concept::StringRepresentable<Args> or ns_concept::IterableConst<Args> ) and ... )
    void operator()(T&& format, Args&&... args)
    {
      print(instance.m_os, "E::{}::{}\n"_fmt(m_loc.get(), format), args...);
      print_if((instance.m_level >= Level::ERROR), "E::{}::{}\n"_fmt(m_loc.get(), format), std::forward<Args>(args)...);
    } // error
};

class debug
{
  private:
    Location m_loc;

  public:
    debug(Location location = {}) : m_loc(location) {}
    template<ns_concept::StringRepresentable T, typename... Args>
    requires ( ( ns_concept::StringRepresentable<Args> or ns_concept::IterableConst<Args> ) and ... )
    void operator()(T&& format, Args&&... args)
    {
      print(instance.m_os, "D::{}::{}\n"_fmt(m_loc.get(), format), args...);
      print_if((instance.m_level >= Level::DEBUG), "D::{}::{}\n"_fmt(m_loc.get(), format), std::forward<Args>(args)...);
    } // debug
};

template<typename F, typename... Args>
requires std::is_invocable_v<F, Args...>
decltype(auto) exception(F&& f, Args... args)
{
  auto expected = ns_exception::to_expected(f, std::forward<Args>(args)...);
  if ( not expected ) { error{}(expected.error()); }
  return expected;
} // debug

} // namespace ns_log

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
