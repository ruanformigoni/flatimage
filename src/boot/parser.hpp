///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : parser
// @created     : Saturday Jan 20, 2024 23:29:45 -03
///

#pragma once

#include <set>
#include <string>
#include <expected>

#include "../cpp/std/enum.hpp"
#include "../cpp/std/vector.hpp"
#include "../cpp/std/variant.hpp"
#include "../cpp/lib/match.hpp"
#include "../cpp/lib/bwrap.hpp"
#include "../cpp/lib/ext2/size.hpp"
#include "../cpp/units.hpp"
#include "../cpp/macro.hpp"

#include "config/environment.hpp"
#include "desktop.hpp"
#include "setup.hpp"
#include "filesystems.hpp"

namespace ns_parser
{

namespace
{

namespace fs = std::filesystem;

inline const char* str_app_descr = "Flatimage - Portable Linux Applications\n";
inline const char* str_root_usage =
"fim-root:\n   Executes a command as the root user\n"
"Usage:\n   fim-root program-name [program-args...]\n"
"Example:\n   fim-root bash";
inline const char* str_exec_usage =
"fim-exec:\n   Executes a command as a regular user\n"
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
"Permissions:\n  home,media,audio,wayland,xorg,dbus_user,dbus_system,udev,usb,input,gpu,network\n"
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
"Example:\n   fim-desktop enable entry,mimetype,icon";
inline const char* str_commit_usage =
"fim-commit:\n   Compresses current changes and inserts them into the FlatImage\n"
"Usage:\n   fim-commit";
inline const char* str_boot_usage =
"fim-boot:\n   Configure the default startup command\n"
"Usage:\n   fim-boot <command> [args...]\n"
"Example:\n   fim-boot echo test";

inline std::string cmd_error(std::string_view str)
{
  std::stringstream ss;
  ss << str_app_descr << str;
  return ss.str();
} // cmd_error

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
  std::set<ns_bwrap::ns_permissions::Permission> permissions;
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

struct CmdCommit
{
};

struct CmdNone {};

using CmdType = std::variant<CmdRoot,CmdExec,CmdResize,CmdPerms,CmdEnv,CmdDesktop,CmdCommit,CmdBoot,CmdNone>;
// }}}

// parse() {{{
inline std::expected<CmdType, std::string> parse(int argc , char** argv)
{
  using VecArgs = std::vector<std::string>;

  if ( argc < 2 or not std::string_view{argv[1]}.starts_with("fim-"))
  {
    return CmdNone{};
  } // if

  auto f_error = [&](bool cond, std::string_view msg_help, std::string_view msg_exception)
  {
    if ( not cond ) { return; }
    println(cmd_error(msg_help));
    throw std::runtime_error(msg_exception.data());
  };

  return ns_match::match(std::string_view{argv[1]},
    ns_match::equal("fim-exec") >>= [&]
    {
      f_error(argc < 3, str_exec_usage, "Incorrect number of arguments");
      return CmdType(CmdExec(argv[2], (argc > 3)? VecArgs(argv+3, argv+argc) : VecArgs{}));
    },
    ns_match::equal("fim-root") >>= [&]
    {
      f_error(argc < 3, str_root_usage, "Incorrect number of arguments");
      return CmdType(CmdRoot(argv[2], (argc > 3)? VecArgs(argv+3, argv+argc) : VecArgs{}));
    },
    ns_match::equal("fim-resize") >>= [&]
    {
      f_error(argc != 3, str_resize_usage, "Incorrect number of arguments");
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
        f_error(true, str_resize_usage, "Invalid argument for filesystem size");
      } // else
      return CmdType(CmdResize(size));
    },
    // Configure permissions for the container
    ns_match::equal("fim-perms") >>= [&]
    {
      // Check if is list subcommand
      f_error(argc != 3 and argc != 4, str_perms_usage, "Incorrect number of arguments");
      // Get op
      CmdPermsOp op = CmdPermsOp(argv[2]);
      // Check if is list
      qreturn_if( op == CmdPermsOp::LIST,  CmdType(CmdPerms{ .op = op, .permissions = {} }));
      // Check if is other command with valid args
      f_error(argc != 4, str_perms_usage, "Incorrect number of arguments");
      CmdPerms cmd_perms;
      cmd_perms.op = op;
      std::ranges::for_each(ns_vector::from_string(argv[3], ',')
        , [&](auto&& e){ cmd_perms.permissions.insert(ns_bwrap::ns_permissions::Permission(e)); }
      );
      return CmdType(cmd_perms);
    },
    // Configure environment
    ns_match::equal("fim-env") >>= [&]
    {
      // Check if is list subcommand
      f_error(argc < 3, str_env_usage, "Incorrect number of arguments");
      // Get op
      CmdEnvOp op = CmdEnvOp(argv[2]);
      // Check if is list
      qreturn_if( op == CmdEnvOp::LIST,  CmdType(CmdEnv{ .op = op, .environment = {} }));
      // Check if is other command with valid args
      f_error(argc < 4, str_env_usage, "Incorrect number of arguments");
      return CmdType(CmdEnv({
        .op = op,
        .environment = std::vector<std::string>(argv+3, argv+argc)
      }));
    },
    // Configure environment
    ns_match::equal("fim-desktop") >>= [&]
    {
      // Check if is other command with valid args
      f_error(argc != 4,  str_desktop_usage, "Incorrect number of arguments");
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
    // Commit current files to a novel compressed layer
    ns_match::equal("fim-commit") >>= [&]
    {
      f_error(argc != 2, str_commit_usage, "Incorrect number of arguments");
      return CmdType(CmdCommit{});
    },
    // Set the default startup command
    ns_match::equal("fim-boot", "fim-cmd") >>= [&]
    {
      f_error(argc != 3, str_boot_usage, "Incorrect number of arguments");
      return CmdType(CmdBoot(argv[2], (argc > 3)? VecArgs(argv+3, argv+argc) : VecArgs{}));
    },
    // Use the default startup command
    ns_match::finally() >>= [&]
    {
      return CmdType(CmdNone{});
    }
  );
} // parse() }}}

// parse_cmds() {{{
inline int parse_cmds(ns_setup::FlatimageSetup config, int argc, char** argv)
{
  // Parse args
  auto variant_cmd = ns_parser::parse(argc, argv);

  auto f_bwrap = [&](std::string const& program
    , std::vector<std::string> const& args
    , std::vector<std::string> const& environment
    , std::set<ns_bwrap::ns_permissions::Permission> const& permissions)
  {
    ns_bwrap::Bwrap(config.is_root
      , config.path_dir_mount_overlayfs
      , config.path_dir_runtime_host
      , config.path_file_bashrc
      , program
      , args
      , environment)
      .run(permissions);
  };

  // Execute a command as a regular user
  if ( auto cmd = ns_variant::get_if_holds_alternative<ns_parser::CmdExec>(*variant_cmd) )
  {
    // Mount filesystem as RO
    [[maybe_unused]] auto mount = ns_filesystems::Filesystems(config);
    // Execute specified command
    auto permissions = ns_exception::or_default([&]{ return ns_bwrap::ns_permissions::get(config.path_file_config_permissions); });
    auto environment = ns_exception::or_default([&]{ return ns_config::ns_environment::get(config.path_file_config_environment); });
    f_bwrap(cmd->program, cmd->args, environment, permissions);
  } // if
  // Execute a command as root
  else if ( auto cmd = ns_variant::get_if_holds_alternative<ns_parser::CmdRoot>(*variant_cmd) )
  {
    // Mount filesystem as RO
    [[maybe_unused]] auto mount = ns_filesystems::Filesystems(config);
    // Execute specified command as 'root'
    config.is_root = true;
    auto permissions = ns_exception::or_default([&]{ return ns_bwrap::ns_permissions::get(config.path_file_config_permissions); });
    auto environment = ns_exception::or_default([&]{ return ns_config::ns_environment::get(config.path_file_config_environment); });
    f_bwrap(cmd->program, cmd->args, environment, permissions);
  } // if
  // Resize the image to contain at least the provided free space
  else if ( auto cmd = ns_variant::get_if_holds_alternative<ns_parser::CmdResize>(*variant_cmd) )
  {
    // Update log level
    if ( not ns_env::get("FIM_DEBUG") )
    {
      ns_log::set_level(ns_log::Level::INFO);
    } // if
    // Resize to fit the provided amount of free space
    ns_ext2::ns_size::resize_free_space(config.path_file_binary, config.offset_ext2, cmd->size);
  } // if
  // Configure permissions
  else if ( auto cmd = ns_variant::get_if_holds_alternative<ns_parser::CmdPerms>(*variant_cmd) )
  {
    // Update log level
    if ( not ns_env::get("FIM_DEBUG") )
    {
      ns_log::set_level(ns_log::Level::INFO);
    } // if
    // Mount filesystem as RW
    [[maybe_unused]] auto mount = ns_filesystems::Filesystems(config, ns_filesystems::Filesystems::FilesystemsLayer::EXT_RW);
    // Create config dir if not exists
    fs::create_directories(config.path_file_config_permissions.parent_path());
    // Determine open mode
    switch( cmd->op )
    {
      case ns_parser::CmdPermsOp::ADD: ns_bwrap::ns_permissions::add(config.path_file_config_permissions, cmd->permissions); break;
      case ns_parser::CmdPermsOp::SET: ns_bwrap::ns_permissions::set(config.path_file_config_permissions, cmd->permissions); break;
      case ns_parser::CmdPermsOp::DEL: ns_bwrap::ns_permissions::del(config.path_file_config_permissions, cmd->permissions); break;
      case ns_parser::CmdPermsOp::LIST:
        std::ranges::for_each(ns_bwrap::ns_permissions::get(config.path_file_config_permissions), ns_functional::PrintLn{}); break;
    } // switch
  } // if
  // Configure environment
  else if ( auto cmd = ns_variant::get_if_holds_alternative<ns_parser::CmdEnv>(*variant_cmd) )
  {
    // Update log level
    if ( not ns_env::get("FIM_DEBUG") )
    {
      ns_log::set_level(ns_log::Level::INFO);
    } // if
    // Mount filesystem as RW
    [[maybe_unused]] auto mount = ns_filesystems::Filesystems(config, ns_filesystems::Filesystems::FilesystemsLayer::EXT_RW);
    // Create config dir if not exists
    fs::create_directories(config.path_file_config_environment.parent_path());
    // Determine open mode
    switch( cmd->op )
    {
      case ns_parser::CmdEnvOp::ADD: ns_config::ns_environment::add(config.path_file_config_environment, cmd->environment); break;
      case ns_parser::CmdEnvOp::SET: ns_config::ns_environment::set(config.path_file_config_environment, cmd->environment); break;
      case ns_parser::CmdEnvOp::DEL: ns_config::ns_environment::del(config.path_file_config_environment, cmd->environment); break;
      case ns_parser::CmdEnvOp::LIST:
        std::ranges::for_each(ns_config::ns_environment::get(config.path_file_config_environment), ns_functional::PrintLn{}); break;
    } // switch
  } // if
  // Configure desktop integration
  else if ( auto cmd = ns_variant::get_if_holds_alternative<ns_parser::CmdDesktop>(*variant_cmd) )
  {
    // Update log level
    if ( not ns_env::get("FIM_DEBUG") )
    {
      ns_log::set_level(ns_log::Level::INFO);
    } // if
    // Mount filesystem as RW
    [[maybe_unused]] auto mount = ns_filesystems::Filesystems(config, ns_filesystems::Filesystems::FilesystemsLayer::EXT_RW);
    // Create config dir if not exists
    fs::create_directories(config.path_file_config_desktop.parent_path());
    // Determine open mode
    switch( cmd->op )
    {
      case ns_parser::CmdDesktopOp::ENABLE:
      {
        auto opt_should_enable = ns_variant::get_if_holds_alternative<std::set<ns_desktop::EnableItem>>(cmd->arg);
        ethrow_if(not opt_should_enable.has_value(), "Could not get items to configure desktop integration");
        ns_desktop::enable(config.path_file_config_desktop, *opt_should_enable);
      }
      break;
      case ns_parser::CmdDesktopOp::SETUP:
      {
        auto opt_path_file_src_json = ns_variant::get_if_holds_alternative<fs::path>(cmd->arg);
        ethrow_if(not opt_path_file_src_json.has_value(), "Could not convert variant value to fs::path");
        ns_desktop::setup(config.path_dir_mount_ext, *opt_path_file_src_json, config.path_file_config_desktop);
      } // case
      break;
    } // switch
  } // if
  // Commit changes as a novel layer into the flatimage
  else if ( auto cmd = ns_variant::get_if_holds_alternative<ns_parser::CmdCommit>(*variant_cmd) )
  {
    // Update log level
    if ( not ns_env::get("FIM_DEBUG") )
    {
      ns_log::set_level(ns_log::Level::INFO);
    } // if
    // Compress changes
    auto opt_path_file_mkdwarfs = ns_subprocess::search_path("mkdwarfs");
    ethrow_if(not opt_path_file_mkdwarfs, "Could not find 'mkdwarfs' binary");
    // Compress filesystem
    fs::path path_file_layer = config.path_dir_host_config / "layer.temp";
    ns_log::info()("Compress filesystem to '{}'", path_file_layer);
    {
      auto ret = ns_subprocess::Subprocess(*opt_path_file_mkdwarfs)
        .with_args("-f")
        .with_args("-l", config.layer_compression_level)
        .with_args("-i", config.path_dir_data_overlayfs / "upperdir")
        .with_args("-o", path_file_layer)
        .spawn()
        .wait();
      ethrow_if(not ret, "mkdwarfs process exited abnormally");
      ethrow_if(ret and *ret != 0, "mkdwarfs process exited with error code '{}'"_fmt(*ret));
    }
    // Create SHA from file 
    std::string str_sha256sum;
    {
      auto opt_path_file_bash = ns_subprocess::search_path("bash");
      ethrow_if(not opt_path_file_bash, "Could not find 'bash' binary");
      auto opt_path_file_sha256sum = ns_subprocess::search_path("sha256sum");
      ethrow_if(not opt_path_file_sha256sum, "Could not find 'sha256sum' binary");
      auto ret = ns_subprocess::Subprocess(*opt_path_file_bash)
        .with_args("-c", *opt_path_file_sha256sum + " " + path_file_layer.string() + " | awk '{print $1}'" )
        .with_piped_outputs()
        .with_stdout_handle([&](auto&& str){ str_sha256sum = str; })
        .spawn()
        .wait();
      ethrow_if(not ret, "sha256sum process exited abnormally");
      ethrow_if(ret and *ret != 0, "sha256sum process exited with error code '{}'"_fmt(*ret));
      ethrow_if(str_sha256sum.empty(), "Could not calculate SHA for overlay");
    }
    ns_log::info()("Filesystem sha256sum: {}", str_sha256sum);
    std::string str_index_highest{};
    // Get highest filesystem index
    {
      // Mount filesystem as RW
      [[maybe_unused]] auto mount = ns_filesystems::Filesystems(config, ns_filesystems::Filesystems::FilesystemsLayer::EXT_RW);
      std::vector<fs::path> vec_path_dir_layers;
      std::ranges::for_each(fs::directory_iterator(config.path_dir_layers)
        , [&](auto&& e){ vec_path_dir_layers.push_back(e); }
      );
      std::ranges::sort(vec_path_dir_layers);
      str_index_highest = vec_path_dir_layers.back().filename().string();
      str_index_highest.erase(str_index_highest.find('-'), std::string::npos);
      str_index_highest = std::to_string(std::stoi(str_index_highest) + 1);
    }
    ns_log::info()("Filesystem index: {}", str_index_highest);
    // Make space available in the main filesystem to fit novel layer
    ns_ext2::ns_size::resize_free_space(config.path_file_binary
      , config.offset_ext2
      , fs::file_size(path_file_layer) + ns_units::from_mebibytes(config.ext2_slack_minimum).to_bytes()
    );
    // Move filesystem file to ext filesystem
    {
      // Mount filesystem as RW
      [[maybe_unused]] auto mount = ns_filesystems::Filesystems(config, ns_filesystems::Filesystems::FilesystemsLayer::EXT_RW);
      fs::copy_file(path_file_layer, config.path_dir_layers / "{}-{}"_fmt(str_index_highest, str_sha256sum) );
    }
  } // else if
  // Update default command on database
  else if ( auto cmd = ns_variant::get_if_holds_alternative<ns_parser::CmdBoot>(*variant_cmd) )
  {
    // Update log level
    if ( not ns_env::get("FIM_DEBUG") )
    {
      ns_log::set_level(ns_log::Level::INFO);
    } // if
    // Mount filesystem as RW
    [[maybe_unused]] auto mount = ns_filesystems::Filesystems(config, ns_filesystems::Filesystems::FilesystemsLayer::EXT_RW);
    // Create config dir if not exists
    fs::create_directories(config.path_file_config_desktop.parent_path());
    // Update database
    ns_db::from_file(config.path_file_config_boot, [&](auto& db)
    {
      db("program") = cmd->program;
      db("args") = cmd->args;
    }, ns_db::Mode::UPDATE_OR_CREATE);
  } // else if
  // Update default command on database
  else if ( auto cmd = ns_variant::get_if_holds_alternative<ns_parser::CmdNone>(*variant_cmd) )
  {
    // Mount filesystem as RO
    [[maybe_unused]] auto mount = ns_filesystems::Filesystems(config);
    // Build exec command
    ns_parser::CmdExec cmd_exec;
    // Fetch default command from database or fallback to bash
    ns_exception::or_else([&]
    {
      ns_db::from_file(config.path_file_config_boot, [&](auto& db)
      {
        cmd_exec.program = db["program"];
        auto& args = db["args"];
        std::for_each(args.cbegin(), args.cend(), [&](auto&& e){ cmd_exec.args.push_back(e); });
      }, ns_db::Mode::UPDATE_OR_CREATE);
    }, [&]
    {
      cmd_exec.program = "bash";
      cmd_exec.args = {};
    });
    // Append argv args
    if ( argc > 1 ) { std::for_each(argv+1, argv+argc, [&](auto&& e){ cmd_exec.args.push_back(e); }); } // if
    // Execute default command
    auto permissions = ns_exception::or_default([&]{ return ns_bwrap::ns_permissions::get(config.path_file_config_permissions); });
    auto environment = ns_exception::or_default([&]{ return ns_config::ns_environment::get(config.path_file_config_permissions); });
    f_bwrap(cmd_exec.program, cmd_exec.args, environment, permissions);
  } // else if

  return EXIT_SUCCESS;
} // parse_cmds() }}}

} // namespace ns_parser

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
