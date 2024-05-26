///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : parser
// @created     : Saturday Jan 20, 2024 23:29:45 -03
///

#pragma once

#include <set>
#include <string>

#include "match.hpp"
#include "../permissions.hpp"
#include "../macro.hpp"
#include "../units.hpp"
#include "../enum.hpp"
#include "../std/vector.hpp"

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
inline const char* str_perms_usage = "Edit current permissions for the flatimage\n"
"   Usage: fim-perms add perm\n"
"          fim-perms del perm\n"
"          fim-perms set perm1,perm2,perm3,...\n"
"          fim-perms list\n"
"   Permissions: home,media,audio,wayland,xorg,dbus_user,dbus_system,udev,usb,gpu,network\n"
"   Example: fim-perms add home\n";

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

ENUM(CmdPermsOp,SET,ADD,DEL,LIST);

struct CmdPerms
{
  CmdPermsOp op;
  std::set<ns_permissions::Permission> permissions;
};

using CmdType = std::variant<CmdRoot,CmdExec,CmdResize,CmdPerms>;
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
      ethrow_if(argc < 3, (ns_log::error(cmd_error(str_exec_usage)), "Incorrect number of arguments"));
      return CmdType(CmdExec(argv[2], (argc > 3)? VecArgs(argv+3, argv+argc) : VecArgs{}));
    },
    ns_match::compare(std::string_view("fim-root")) >>= [&]
    {
      ethrow_if(argc < 3, (ns_log::error(cmd_error(str_root_usage)), "Incorrect number of arguments"));
      return CmdType(CmdRoot(argv[2], (argc > 3)? VecArgs(argv+3, argv+argc) : VecArgs{}));
    },
    ns_match::compare(std::string_view("fim-resize")) >>= [&]
    {
      ethrow_if(argc < 3, (ns_log::error(cmd_error(str_resize_usage)), "Incorrect number of arguments"));
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
    },
    ns_match::compare(std::string_view("fim-perms")) >>= [&]
    {
      // Check if is list subcommand
      CmdPermsOp op = CmdPermsOp(argv[2]);
      ethrow_if(argc < 3, (ns_log::error(cmd_error(str_perms_usage)), "Incorrect number of arguments"));
      if ( op == CmdPermsOp::LIST )
      {
        return CmdType(CmdPerms{ .op = op, .permissions = {} });
      } // if
      // Check if is other command with valid args
      ethrow_if(argc < 4, (ns_log::error(cmd_error(str_perms_usage)), "Incorrect number of arguments"));
      CmdPerms cmd_perms;
      cmd_perms.op = op;
      std::ranges::for_each(ns_vector::from_string(argv[3], ',')
        , [&](auto&& e){ cmd_perms.permissions.insert(ns_permissions::Permission(e)); }
      );
      return CmdType(cmd_perms);
    }
  );
} // parse() }}}

} // namespace ns_parser

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
