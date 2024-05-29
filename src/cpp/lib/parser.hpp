///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : parser
// @created     : Saturday Jan 20, 2024 23:29:45 -03
///

#pragma once

#include <set>
#include <string>

#include "match.hpp"
#include "../macro.hpp"
#include "../units.hpp"
#include "../enum.hpp"
#include "../std/vector.hpp"
#include "../std/functional.hpp"
#include "../config/permissions.hpp"

namespace ns_parser
{

namespace
{

namespace fs = std::filesystem;

inline const char* str_app_descr = "Flatimage - Portable Linux Applications\n";
inline const char* str_root_usage =
"fim-root:\n   Executes the command as the root user\n"
"Usage:\n   fim-root program-name [program-args...]\n"
"Example:\n   fim-root bash";
inline const char* str_exec_usage =
"fim-exec:\n   Executes the command as a regular user\n"
"Usage:\n   fim-exec program-name [program-args...]\n"
"Example:\n   fim-exec bash";
inline const char* str_resize_usage =
"fim-resize:\n   Resizes the free space of the image to have at least the provided value\n"
"Usage:\n   fim-resize [0-9]+<M|G>\n"
"Example:\n   fim-resize 500M";
inline const char* str_perms_usage =
"fim-perms:\n   Edit current permissions for the flatimage\n"
"Usage:\n   fim-perms add|del|set <perms>...\n"
"   fim-perms list\n"
"Permissions:\n   home,media,audio,wayland,xorg,dbus_user,dbus_system,udev,usb,gpu,network\n"
"Example:\n   fim-perms add home,media";
inline const char* str_env_usage =
"fim-env:\n   Edit current permissions for the flatimage\n"
"Usage:\n   fim-env add|set <'key=value'>...\n"
"   fim-env del <key>\n"
"   fim-env list\n"
"Example:\n   fim-env add 'APP_NAME=hello-world' 'PS1=my-app> ' 'HOME=$FIM_DIR_HOST_CONFIG/home'";
inline const char* str_desktop_usage =
"fim-desktop:\n   Configure the desktop integration\n"
"Usage:\n   fim-desktop setup <json-file>\n"
"   fim-desktop enable <1|0>\n"
"Example:\n   fim-desktop setup ./desktop.json";

inline std::string cmd_error(std::string_view str)
{
  std::stringstream ss;
  ss << str_app_descr << str;
  return ss.str();
}

inline void check_or_print_and_throw(bool cond, std::string_view msg_err, std::string_view msg_exception)
{
  if ( not cond )
  {
    print("{}\n", msg_err);
    throw std::runtime_error(msg_exception.data());
  } // if
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
  std::set<ns_config::ns_permissions::Permission> permissions;
};

ENUM(CmdEnvOp,SET,ADD,DEL,LIST);
struct CmdEnv
{
  CmdEnvOp op;
  std::vector<std::string> environment;
};

ENUM(CmdDesktopOp,SETUP,ENABLE);
struct CmdDesktop
{
  CmdDesktopOp op;
  std::variant<fs::path,bool> arg;
};

using CmdType = std::variant<CmdRoot,CmdExec,CmdResize,CmdPerms,CmdEnv,CmdDesktop>;
// }}}

// parse() {{{
inline std::optional<CmdType> parse(int argc, char** argv)
{
  ireturn_if(argc < 2, "No command specified", std::nullopt);

  ireturn_if(not std::string_view{argv[1]}.starts_with("fim-"), "Not a flatimage command, ignore", std::nullopt);

  using VecArgs = std::vector<std::string>;

  return ns_match::match(std::string_view{argv[1]},
    ns_match::compare("fim-exec") >>= [&]
    {
      check_or_print_and_throw(argc >= 3, cmd_error(str_exec_usage), "Incorrect number of arguments");
      return CmdType(CmdExec(argv[2], (argc > 3)? VecArgs(argv+3, argv+argc) : VecArgs{}));
    },
    ns_match::compare("fim-root") >>= [&]
    {
      check_or_print_and_throw(argc >= 3, cmd_error(str_root_usage), "Incorrect number of arguments");
      return CmdType(CmdRoot(argv[2], (argc > 3)? VecArgs(argv+3, argv+argc) : VecArgs{}));
    },
    ns_match::compare("fim-resize") >>= [&]
    {
      check_or_print_and_throw(argc >= 3, cmd_error(str_resize_usage), "Incorrect number of arguments");
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
    // Configure permissions for the container
    ns_match::compare("fim-perms") >>= [&]
    {
      // Check if is list subcommand
      check_or_print_and_throw(argc >= 3, cmd_error(str_perms_usage), "Incorrect number of arguments");
      // Get op
      CmdPermsOp op = CmdPermsOp(argv[2]);
      // Check if is list
      qreturn_if( op == CmdPermsOp::LIST,  CmdType(CmdPerms{ .op = op, .permissions = {} }));
      // Check if is other command with valid args
      check_or_print_and_throw(argc >= 4, cmd_error(str_perms_usage), "Incorrect number of arguments");
      CmdPerms cmd_perms;
      cmd_perms.op = op;
      std::ranges::for_each(ns_vector::from_string(argv[3], ',')
        , [&](auto&& e){ cmd_perms.permissions.insert(ns_config::ns_permissions::Permission(e)); }
      );
      return CmdType(cmd_perms);
    },
    // Configure environment
    ns_match::compare("fim-env") >>= [&]
    {
      // Check if is list subcommand
      check_or_print_and_throw(argc >= 3, cmd_error(str_env_usage), "Incorrect number of arguments");
      // Get op
      CmdEnvOp op = CmdEnvOp(argv[2]);
      // Check if is list
      qreturn_if( op == CmdEnvOp::LIST,  CmdType(CmdEnv{ .op = op, .environment = {} }));
      // Check if is other command with valid args
      check_or_print_and_throw(argc >= 4, cmd_error(str_env_usage), "Incorrect number of arguments");
      return CmdType(CmdEnv({
        .op = op,
        .environment = std::vector<std::string>(argv+3, argv+argc)
      }));
    },
    // Configure environment
    ns_match::compare("fim-desktop") >>= [&]
    {
      // Check if is other command with valid args
      check_or_print_and_throw(argc >= 4, cmd_error(str_desktop_usage), "Incorrect number of arguments");
      CmdDesktopOp op = CmdDesktopOp(argv[2]);
      // If is enable, should be either 0 or 1
      ethrow_if((op == CmdDesktopOp::ENABLE
        and std::string_view{argv[3]} != "0"
        and std::string_view{argv[3]} != "1"),
        "Invalid value for enable, should be either 0 or 1"
      );
      CmdDesktop cmd;
      cmd.op = op;
      if ( op == CmdDesktopOp::SETUP ) { cmd.arg = fs::path{argv[3]}; } else { cmd.arg = std::string_view(argv[3]) == "1"; }
      return CmdType(cmd);
    }
  );
} // parse() }}}

} // namespace ns_parser

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
