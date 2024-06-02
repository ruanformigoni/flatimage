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
#include "../../boot/desktop/desktop.hpp"

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
"   fim-desktop enable <items...>\n"
"items:\n   entry,mimetype,icon\n"
"Example:\n   fim-desktop setup entry,mimetype,icon";
inline const char* str_boot_usage =
"fim-boot:\n   Configure the default startup command\n"
"Usage:\n   fim-boot <command> [args...]\n"
"Example:\n   fim-boot echo test";

inline void cmd_error(std::string_view str)
{
  std::stringstream ss;
  ss << str_app_descr << str;
  println(ss.str());
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
  std::variant<fs::path,std::set<ns_desktop::EnableItem>> arg;
};

struct CmdBoot
{
  std::string program;
  std::vector<std::string> args;
};

struct CmdNone {};


using CmdType = std::variant<CmdRoot,CmdExec,CmdResize,CmdPerms,CmdEnv,CmdDesktop,CmdBoot,CmdNone>;
// }}}

// parse() {{{
inline nonstd::expected<CmdType, std::string> parse(int argc , char** argv)
{
  using VecArgs = std::vector<std::string>;

  if ( argc < 2 or not std::string_view{argv[1]}.starts_with("fim-"))
  {
    return CmdNone{};
  } // if

  return ns_match::match(std::string_view{argv[1]},
    ns_match::equal("fim-exec") >>= [&]
    {
      ethrow_if(argc < 3, (cmd_error(str_exec_usage), "Incorrect number of arguments"));
      return CmdType(CmdExec(argv[2], (argc > 3)? VecArgs(argv+3, argv+argc) : VecArgs{}));
    },
    ns_match::equal("fim-root") >>= [&]
    {
      ethrow_if(argc < 3, (cmd_error(str_root_usage), "Incorrect number of arguments"));
      return CmdType(CmdRoot(argv[2], (argc > 3)? VecArgs(argv+3, argv+argc) : VecArgs{}));
    },
    ns_match::equal("fim-resize") >>= [&]
    {
      ethrow_if(argc < 3, (cmd_error(str_resize_usage), "Incorrect number of arguments"));
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
        cmd_error(str_resize_usage);
        throw std::runtime_error("Invalid argument for filesystem size");
      } // else
      return CmdType(CmdResize(size));
    },
    // Configure permissions for the container
    ns_match::equal("fim-perms") >>= [&]
    {
      // Check if is list subcommand
      ethrow_if(argc < 3, (cmd_error(str_perms_usage), "Incorrect number of arguments"));
      // Get op
      CmdPermsOp op = CmdPermsOp(argv[2]);
      // Check if is list
      qreturn_if( op == CmdPermsOp::LIST,  CmdType(CmdPerms{ .op = op, .permissions = {} }));
      // Check if is other command with valid args
      ethrow_if(argc < 4, (cmd_error(str_perms_usage), "Incorrect number of arguments"));
      CmdPerms cmd_perms;
      cmd_perms.op = op;
      std::ranges::for_each(ns_vector::from_string(argv[3], ',')
        , [&](auto&& e){ cmd_perms.permissions.insert(ns_config::ns_permissions::Permission(e)); }
      );
      return CmdType(cmd_perms);
    },
    // Configure environment
    ns_match::equal("fim-env") >>= [&]
    {
      // Check if is list subcommand
      ethrow_if(argc < 3, (cmd_error(str_env_usage), "Incorrect number of arguments"));
      // Get op
      CmdEnvOp op = CmdEnvOp(argv[2]);
      // Check if is list
      qreturn_if( op == CmdEnvOp::LIST,  CmdType(CmdEnv{ .op = op, .environment = {} }));
      // Check if is other command with valid args
      ethrow_if(argc < 4, (cmd_error(str_env_usage), "Incorrect number of arguments"));
      return CmdType(CmdEnv({
        .op = op,
        .environment = std::vector<std::string>(argv+3, argv+argc)
      }));
    },
    // Configure environment
    ns_match::equal("fim-desktop") >>= [&]
    {
      // Check if is other command with valid args
      if ( argc < 4 ) { cmd_error(str_desktop_usage); }
      ethrow_if(argc < 4, "Incorrect number of arguments");
      CmdDesktopOp op = CmdDesktopOp(argv[2]);
      CmdDesktop cmd;
      cmd.op = op;
      if ( op == CmdDesktopOp::SETUP )
      {
        cmd.arg = fs::path{argv[3]};
      } // if
      else
      {
        // Get comma separated argument list
        auto vec_strs = ns_vector::from_string(std::string_view(argv[3]), ',');
        std::set<ns_desktop::EnableItem> set_enum;
        std::ranges::transform(vec_strs, std::inserter(set_enum, set_enum.end()), [](auto&& e){ return ns_desktop::EnableItem(e); });
        cmd.arg = set_enum;
      } // else
      return CmdType(cmd);
    },
    // Set the default startup command
    ns_match::equal("fim-boot", "fim-cmd") >>= [&]
    {
      ethrow_if(argc < 3, (cmd_error(str_boot_usage), "Incorrect number of arguments"));
      return CmdType(CmdBoot(argv[2], (argc > 3)? VecArgs(argv+3, argv+argc) : VecArgs{}));
    },
    // Use the default startup command
    ns_match::finally() >>= [&]
    {
      return CmdType(CmdNone{});
    }
  );
} // parse() }}}

} // namespace ns_parser

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
