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

// class Logger {{{
class Logger
{
  private:
    std::optional<std::ofstream> m_opt_os;
    Level m_level;
  public:
    Logger();
    Logger(Logger const&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger const&) = delete;
    Logger& operator=(Logger&&) = delete;
    void set_level(Level level);
    Level get_level() const;
    void set_sink_file(fs::path path_file_sink);
    std::optional<std::ofstream>& get_sink_file();
}; // class Logger }}}

// fn: Logger::Logger {{{
inline Logger::Logger()
  : m_level(Level::QUIET)
{
} // fn: Logger::Logger }}}

// fn: Logger::set_sink_file {{{
inline void Logger::set_sink_file(fs::path path_file_sink)
{
  // File to save logs into
  if ( const char* var = std::getenv("FIM_DEBUG"); var && std::string_view{var} == "1" )
  {
    "Logger file: {}\n"_print(path_file_sink);
  } // if

  // File output stream
  m_opt_os = std::ofstream{path_file_sink};

  if( m_opt_os->bad() ) { std::runtime_error("Could not open file '{}'"_fmt(path_file_sink)); };
} // fn: Logger::set_sink_file }}}

// fn: Logger::get_sink_file {{{
inline std::optional<std::ofstream>& Logger::get_sink_file()
{
  return m_opt_os;
} // fn: Logger::get_sink_file }}}

// fn: Logger::set_level {{{
inline void Logger::set_level(Level level)
{
  m_level = level;
} // fn: Logger::set_level }}}

// fn: Logger::get_level {{{
inline Level Logger::get_level() const
{
  return m_level;
} // fn: Logger::get_level }}}

static Logger logger;

} // namespace

// fn: set_level {{{
inline void set_level(Level level)
{
  logger.set_level(level);
} // fn: set_level }}}

// fn: set_sink_file {{{
inline void set_sink_file(fs::path path_file_sink)
{
  logger.set_sink_file(path_file_sink);
} // fn: set_sink_file }}}

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

// class info {{{
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
      auto& opt_ostream_sink = logger.get_sink_file();
      print_if(opt_ostream_sink, *opt_ostream_sink, "I::{}::{}\n"_fmt(m_loc.get(), format), args...);
      print_if((logger.get_level() >= Level::INFO), std::cout, "I::{}::{}\n"_fmt(m_loc.get(), format), std::forward<Args>(args)...);
    } // info
}; // class info }}}

// class error {{{
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
      auto& opt_ostream_sink = logger.get_sink_file();
      print_if(opt_ostream_sink, *opt_ostream_sink, "E::{}::{}\n"_fmt(m_loc.get(), format), args...);
      print_if((logger.get_level() >= Level::ERROR), std::cerr, "E::{}::{}\n"_fmt(m_loc.get(), format), std::forward<Args>(args)...);
    } // error
}; // class error }}}

// class debug {{{
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
      auto& opt_ostream_sink = logger.get_sink_file();
      print_if(opt_ostream_sink, *opt_ostream_sink, "D::{}::{}\n"_fmt(m_loc.get(), format), args...);
      print_if((logger.get_level() >= Level::DEBUG), std::cerr, "D::{}::{}\n"_fmt(m_loc.get(), format), std::forward<Args>(args)...);
    } // debug
}; // class debug }}}

// fn: exception {{{
inline void exception(auto&& fn)
{
  if (auto expected = ns_exception::to_expected(fn); not expected)
  {
    ns_log::error()(expected.error());
  } // if
} // }}}

} // namespace ns_log

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
