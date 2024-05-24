///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : parser
// @created     : Saturday Jan 20, 2024 23:29:45 -03
///

#pragma once

#include "match.hpp"
#include "../macro.hpp"

namespace ns_parser
{

enum class EnumCmd
{
  ROOT,
  EXEC,
  RESIZE,
}; // enum class Operation

inline const char* str_app = "Flatimage";
inline const char* str_app_descr = "Flatimage - Portable Linux Applications";
inline const char* str_root_usage = "Executes the command as the root user\n"
"Usage: fim-root program-name [program-args...]";
inline const char* str_exec_usage = "Executes the command as a regular user\n"
"Usage: fim-exec program-name [program-args...]";
inline const char* str_resize_usage = "Resizes the free space of the image to match the provided value\n"
"Usage: fim-resize size[M,G]\n"
"Example: fim-resize 500M";

struct CmdRoot
{
  std::string program;
  std::vector<std::string> args;
  static std::string error()
  {
    std::stringstream ss;
    ss << str_app << str_app_descr << str_root_usage;
    return ss.str();
  }
};

struct CmdExec
{
  std::string program;
  std::vector<std::string> args;
  static std::string error()
  {
    std::stringstream ss;
    ss << str_app << str_app_descr << str_exec_usage;
    return ss.str();
  }
};

struct CmdResize
{
  uint64_t size;
  static std::string error()
  {
    std::stringstream ss;
    ss << str_app << str_app_descr << str_resize_usage;
    return ss.str();
  }
};

using CmdType = std::variant<CmdRoot,CmdExec,CmdResize>;

inline std::optional<CmdType> parse(int argc, char** argv)
{
  ireturn_if(argc < 2, "No command specified", std::nullopt);

  ireturn_if(std::string{argv[1]}.starts_with("fim-"), "Not a flatimage command, ignore", std::nullopt);

  using VecArgs = std::vector<std::string>;

  return ns_match::match(std::string_view{argv[1]},
    ns_match::compare(std::string_view("fim-exec")) >>= [&]
    {
      ethrow_if(argc < 3, CmdExec::error());
      return CmdType(CmdExec(argv[2], (argc > 3)? VecArgs(argv+3, argv+argc) : VecArgs{}));
    },
    ns_match::compare(std::string_view("fim-root")) >>= [&]
    {
      ethrow_if(argc < 3, CmdRoot::error());
      return CmdType(CmdRoot(argv[2], (argc > 3)? VecArgs(argv+3, argv+argc) : VecArgs{}));
    },
    ns_match::compare(std::string_view("fim-resize")) >>= [&]
    {
      ethrow_if(argc < 3, CmdResize::error());
      return CmdType(CmdResize(std::stoll(argv[2])));
    }
  );
} // function: parse

} // namespace ns_parser

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
