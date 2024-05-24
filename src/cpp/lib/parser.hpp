///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : parser
// @created     : Saturday Jan 20, 2024 23:29:45 -03
///

#pragma once

#include "match.hpp"
#include "../macro.hpp"
#include "../units.hpp"

namespace ns_parser
{

namespace
{

inline const char* str_app = "Flatimage\n";
inline const char* str_app_descr = "Flatimage - Portable Linux Applications\n";
inline const char* str_root_usage = "Executes the command as the root user\n"
"   Usage: fim-root program-name [program-args...]";
inline const char* str_exec_usage = "Executes the command as a regular user\n"
"   Usage: fim-exec program-name [program-args...]\n";
inline const char* str_resize_usage = "Resizes the free space of the image to match the provided value\n"
"   Usage: fim-resize size[M,G]\n"
"   Example: fim-resize 500M\n";

inline std::string cmd_error(std::string_view str)
{
  std::stringstream ss;
  ss << str_app << str_app_descr << str;
  return ss.str();
}

} // namespace


// Cmds {{{
struct CmdRoot
{
  std::string program;
  std::vector<std::string> args;
};

struct CmdExec
{
  std::string program;
  std::vector<std::string> args;
};

struct CmdResize
{
  uint64_t size;
};

using CmdType = std::variant<CmdRoot,CmdExec,CmdResize>;
// }}}

// parse() {{{
inline std::optional<CmdType> parse(int argc, char** argv)
{
  ireturn_if(argc < 2, "No command specified", std::nullopt);

  ireturn_if(not std::string_view{argv[1]}.starts_with("fim-"), "Not a flatimage command, ignore", std::nullopt);

  using VecArgs = std::vector<std::string>;

  return ns_match::match(std::string_view{argv[1]},
    ns_match::compare(std::string_view("fim-exec")) >>= [&]
    {
      ethrow_if(argc < 3, cmd_error(str_exec_usage));
      return CmdType(CmdExec(argv[2], (argc > 3)? VecArgs(argv+3, argv+argc) : VecArgs{}));
    },
    ns_match::compare(std::string_view("fim-root")) >>= [&]
    {
      ethrow_if(argc < 3, cmd_error(str_root_usage));
      return CmdType(CmdRoot(argv[2], (argc > 3)? VecArgs(argv+3, argv+argc) : VecArgs{}));
    },
    ns_match::compare(std::string_view("fim-resize")) >>= [&]
    {
      ethrow_if(argc < 3, cmd_error(str_resize_usage));
      // Get size string
      std::string str_size = argv[2];
      // Convert to appropriate unit
      int64_t size{};
      if ( str_size.ends_with("M") )
      {
        str_size.pop_back();
        size = ns_units::from_mebibytes(std::stoll(str_size)).to_bytes();
      } // if
      else if ( str_size.ends_with("G")  )
      {
        size = ns_units::from_gibibytes(std::stoll(str_size)).to_bytes();
      } // else
      else
      {
        throw std::runtime_error(cmd_error(str_resize_usage));
      } // else
      return CmdType(CmdResize(size));
    }
  );
} // parse() }}}

} // namespace ns_parser

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
