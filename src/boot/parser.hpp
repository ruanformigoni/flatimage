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
#include "../cpp/macro.hpp"

#include "config/environment.hpp"
#include "config/config.hpp"
#include "cmd/layers.hpp"
#include "cmd/desktop.hpp"
#include "cmd/bind.hpp"
#include "cmd/help.hpp"
#include "filesystems.hpp"

namespace ns_parser
{

namespace
{

namespace fs = std::filesystem;

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

ENUM(CmdLayerOp,CREATE,ADD);
struct CmdLayer
{
  CmdLayerOp op;
  std::vector<std::string> args;
};

struct CmdCommit
{
};

ENUM(CmdCaseFoldOp,ON,OFF);
struct CmdCaseFold
{
  CmdCaseFoldOp op;
};

struct CmdNone {};

using CmdType = std::variant<CmdRoot
  , CmdExec
  , CmdPerms
  , CmdEnv
  , CmdDesktop
  , CmdLayer
  , ns_cmd::ns_bind::CmdBind
  , CmdCommit
  , CmdCaseFold
  , CmdBoot
  , CmdNone
>;
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
    println(msg_help);
    throw std::runtime_error(msg_exception.data());
  };

  return ns_match::match(std::string_view{argv[1]},
    ns_match::equal("fim-exec") >>= [&]
    {
      f_error(argc < 3, ns_cmd::ns_help::exec_usage(), "Incorrect number of arguments");
      return CmdType(CmdExec(argv[2], (argc > 3)? VecArgs(argv+3, argv+argc) : VecArgs{}));
    },
    ns_match::equal("fim-root") >>= [&]
    {
      f_error(argc < 3, ns_cmd::ns_help::root_usage(), "Incorrect number of arguments");
      return CmdType(CmdRoot(argv[2], (argc > 3)? VecArgs(argv+3, argv+argc) : VecArgs{}));
    },
    // Configure permissions for the container
    ns_match::equal("fim-perms") >>= [&]
    {
      // Check if is list subcommand
      f_error(argc != 3 and argc != 4, ns_cmd::ns_help::perms_usage(), "Incorrect number of arguments");
      // Get op
      CmdPermsOp op = CmdPermsOp(argv[2]);
      // Check if is list
      qreturn_if( op == CmdPermsOp::LIST,  CmdType(CmdPerms{ .op = op, .permissions = {} }));
      // Check if is other command with valid args
      f_error(argc != 4, ns_cmd::ns_help::perms_usage(), "Incorrect number of arguments");
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
      f_error(argc < 3, ns_cmd::ns_help::env_usage(), "Incorrect number of arguments");
      // Get op
      CmdEnvOp op = CmdEnvOp(argv[2]);
      // Check if is list
      qreturn_if( op == CmdEnvOp::LIST,  CmdType(CmdEnv{ .op = op, .environment = {} }));
      // Check if is other command with valid args
      f_error(argc < 4, ns_cmd::ns_help::env_usage(), "Incorrect number of arguments");
      return CmdType(CmdEnv({
        .op = op,
        .environment = std::vector<std::string>(argv+3, argv+argc)
      }));
    },
    // Configure environment
    ns_match::equal("fim-desktop") >>= [&]
    {
      // Check if is other command with valid args
      f_error(argc != 4,  ns_cmd::ns_help::desktop_usage(), "Incorrect number of arguments");
      CmdDesktop cmd;
      cmd.op = CmdDesktopOp(argv[2]);
      if ( cmd.op == CmdDesktopOp::SETUP )
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
    // Manage layers
    ns_match::equal("fim-layer") >>= [&]
    {
      f_error(argc < 3, ns_cmd::ns_help::layer_usage(), "Incorrect number of arguments");

      // Create cmd
      CmdLayer cmd;

      // Get op
      cmd.op = CmdLayerOp(argv[2]);

      if ( cmd.op == CmdLayerOp::ADD )
      {
        f_error(argc < 4, ns_cmd::ns_help::layer_usage(), "add requires exactly one argument");
        ns_vector::push_back(cmd.args, argv[3]);
      } // if
      else
      {
        f_error(argc < 5, ns_cmd::ns_help::layer_usage(), "add requires exactly two arguments");
        ns_vector::push_back(cmd.args, argv[3], argv[4]);
      } // else
      return CmdType(cmd);
    },
    // Bind a path or device to inside the flatimage
    ns_match::equal("fim-bind") >>= [&]
    {
      f_error(argc < 3, ns_cmd::ns_help::bind_usage(), "Incorrect number of arguments");
      // Create command
      ns_cmd::ns_bind::CmdBind cmd;
      // Check op
      cmd.op = ns_cmd::ns_bind::CmdBindOp(argv[2]);
      cmd.data = ns_match::match(cmd.op
        , ns_match::equal(ns_cmd::ns_bind::CmdBindOp::ADD) >>= [&]
        {
          f_error(argc != 6, ns_cmd::ns_help::bind_usage(), "Incorrect number of arguments");
          return ns_cmd::ns_bind::data_t(ns_cmd::ns_bind::bind_t(std::string{argv[3]}, argv[4], argv[5]));
        }
        , ns_match::equal(ns_cmd::ns_bind::CmdBindOp::DEL) >>= [&]
        {
          f_error(argc != 4, ns_cmd::ns_help::bind_usage(), "Incorrect number of arguments");
          f_error(not std::ranges::all_of(std::string_view{argv[3]}, [](char c){ return std::isdigit(c); })
              , ns_cmd::ns_help::bind_usage()
              , "Invalid index specifier"
          );
          return ns_cmd::ns_bind::data_t(ns_cmd::ns_bind::index_t(std::stoi(argv[3])));
        }
        , ns_match::equal(ns_cmd::ns_bind::CmdBindOp::LIST) >>= ns_cmd::ns_bind::data_t(std::false_type{})
      );
      return CmdType(cmd);
    },
    // Commit current files to a novel compressed layer
    ns_match::equal("fim-commit") >>= [&]
    {
      f_error(argc != 2, ns_cmd::ns_help::commit_usage(), "Incorrect number of arguments");
      return CmdType(CmdCommit{});
    },
    // Enables or disable ignore case for paths (useful for wine)
    ns_match::equal("fim-casefold") >>= [&]
    {
      f_error(argc != 3, ns_cmd::ns_help::commit_usage(), "Incorrect number of arguments");
      return CmdType(CmdCaseFold{CmdCaseFoldOp(argv[2])});
    },
    // Set the default startup command
    ns_match::equal("fim-boot", "fim-cmd") >>= [&]
    {
      f_error(argc < 3, ns_cmd::ns_help::boot_usage(), "Incorrect number of arguments");
      return CmdType(CmdBoot(argv[2], (argc > 3)? VecArgs(argv+3, argv+argc) : VecArgs{}));
    },
    // Use the default startup command
    ns_match::equal("fim-ns_cmd::ns_help::help") >>= [&]
    {
      if ( argc < 3 ) { f_error(true, ns_cmd::ns_help::help_usage(), "fim-help"); } // if
      (void) ns_match::match(std::string_view{argv[2]},
        ns_match::equal("exec")     >>= [&]{ f_error(true, ns_cmd::ns_help::exec_usage(), ""); },
        ns_match::equal("root")     >>= [&]{ f_error(true, ns_cmd::ns_help::root_usage(), ""); },
        ns_match::equal("perms")    >>= [&]{ f_error(true, ns_cmd::ns_help::perms_usage(), ""); },
        ns_match::equal("env")      >>= [&]{ f_error(true, ns_cmd::ns_help::env_usage(), ""); },
        ns_match::equal("desktop")  >>= [&]{ f_error(true, ns_cmd::ns_help::desktop_usage(), ""); },
        ns_match::equal("layer")    >>= [&]{ f_error(true, ns_cmd::ns_help::layer_usage(), ""); },
        ns_match::equal("bind")     >>= [&]{ f_error(true, ns_cmd::ns_help::bind_usage(), ""); },
        ns_match::equal("commit")   >>= [&]{ f_error(true, ns_cmd::ns_help::commit_usage(), ""); },
        ns_match::equal("casefold") >>= [&]{ f_error(true, ns_cmd::ns_help::casefold_usage(), ""); },
        ns_match::equal("boot")     >>= [&]{ f_error(true, ns_cmd::ns_help::boot_usage(), ""); }
      );
      return CmdType(CmdNone{});
    }
  );
} // parse() }}}

// parse_cmds() {{{
inline int parse_cmds(ns_config::FlatimageConfig config, int argc, char** argv)
{
  // Parse args
  auto variant_cmd = ns_parser::parse(argc, argv);

  auto f_bwrap = [&](std::string const& program
    , std::vector<std::string> const& args
    , std::vector<std::string> const& environment
    , std::set<ns_bwrap::ns_permissions::Permission> const& permissions)
  {
    // Create bwrap instance
    ns_bwrap::Bwrap bwrap = ns_bwrap::Bwrap(config.is_root
      , config.path_dir_mount_overlayfs
      , config.path_file_bashrc
      , program
      , args
      , environment);

    // Include root binding and custom user-defined bindings
    (void) bwrap
      .with_bind_ro("/", config.path_dir_runtime_host)
      .with_binds_from_file(config.path_file_config_bindings);

    // Check if should enable GPU
    if ( permissions.contains(ns_bwrap::ns_permissions::Permission::GPU) )
    {
      (void) bwrap.with_bind_gpu(config.path_dir_mount_overlayfs, config.path_dir_runtime_host);
    }

    // Run bwrap
    bwrap.run(permissions);
  };

  // Execute a command as a regular user
  if ( auto cmd = ns_variant::get_if_holds_alternative<ns_parser::CmdExec>(*variant_cmd) )
  {
    // Mount filesystem as RO
    auto mount = ns_filesystems::Filesystems(config);
    mount.spawn_janitor();
    // Execute specified command
    auto permissions = ns_exception::or_default([&]{ return ns_bwrap::ns_permissions::get(config.path_file_config_permissions); });
    auto environment = ns_exception::or_default([&]{ return ns_config::ns_environment::get(config.path_file_config_environment); });
    f_bwrap(cmd->program, cmd->args, environment, permissions);
  } // if
  // Execute a command as root
  else if ( auto cmd = ns_variant::get_if_holds_alternative<ns_parser::CmdRoot>(*variant_cmd) )
  {
    // Mount filesystem as RO
    auto mount = ns_filesystems::Filesystems(config);
    mount.spawn_janitor();
    // Execute specified command as 'root'
    config.is_root = true;
    auto permissions = ns_exception::or_default([&]{ return ns_bwrap::ns_permissions::get(config.path_file_config_permissions); });
    auto environment = ns_exception::or_default([&]{ return ns_config::ns_environment::get(config.path_file_config_environment); });
    f_bwrap(cmd->program, cmd->args, environment, permissions);
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
    [[maybe_unused]] auto mount = ns_filesystems::Filesystems(config);
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
    [[maybe_unused]] auto mount = ns_filesystems::Filesystems(config);
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
    // Determine open mode
    switch( cmd->op )
    {
      case ns_parser::CmdDesktopOp::ENABLE:
      {
        auto opt_should_enable = ns_variant::get_if_holds_alternative<std::set<ns_desktop::EnableItem>>(cmd->arg);
        ethrow_if(not opt_should_enable.has_value(), "Could not get items to configure desktop integration");
        ns_desktop::enable(config, *opt_should_enable);
      }
      break;
      case ns_parser::CmdDesktopOp::SETUP:
      {
        auto opt_path_file_src_json = ns_variant::get_if_holds_alternative<fs::path>(cmd->arg);
        ethrow_if(not opt_path_file_src_json.has_value(), "Could not convert variant value to fs::path");
        ns_desktop::setup(config, *opt_path_file_src_json, config.path_file_config_desktop);
      } // case
      break;
    } // switch
  } // if
  // Manager layers
  else if ( auto cmd = ns_variant::get_if_holds_alternative<ns_parser::CmdLayer>(*variant_cmd) )
  {
    // Update log level
    if ( not ns_env::get("FIM_DEBUG") )
    {
      ns_log::set_level(ns_log::Level::INFO);
    } // if
    if ( cmd->op == CmdLayerOp::ADD )
    {
      ns_layers::add(config.path_file_binary, cmd->args.front());
    } // if
    else
    {
      ns_layers::create(cmd->args.at(0), cmd->args.at(1), config.layer_compression_level);
    } // else
  } // else if
  // Bind a device or file to the flatimage
  else if ( auto cmd = ns_variant::get_if_holds_alternative<ns_cmd::ns_bind::CmdBind>(*variant_cmd) )
  {
    // Update log level
    if ( not ns_env::get("FIM_DEBUG") )
    {
      ns_log::set_level(ns_log::Level::INFO);
    } // if

    // Mount filesystem as RW
    [[maybe_unused]] auto mount = ns_filesystems::Filesystems(config);

    // Perform selected op
    switch(cmd->op)
    {
      case ns_cmd::ns_bind::CmdBindOp::ADD: ns_cmd::ns_bind::add(config.path_file_config_bindings, *cmd); break;
      case ns_cmd::ns_bind::CmdBindOp::DEL: ns_cmd::ns_bind::del(config.path_file_config_bindings, *cmd); break;
      case ns_cmd::ns_bind::CmdBindOp::LIST: ns_cmd::ns_bind::list(config.path_file_config_bindings); break;
    } // switch

  } // else if
  // Commit changes as a novel layer into the flatimage
  else if ( auto cmd = ns_variant::get_if_holds_alternative<ns_parser::CmdCommit>(*variant_cmd) )
  {
    // Update log level
    if ( not ns_env::get("FIM_DEBUG") )
    {
      ns_log::set_level(ns_log::Level::INFO);
    } // if
    // Set source directory and target compressed file
    fs::path path_file_layer = config.path_dir_host_config / "layer.tmp";
    fs::path path_dir_src = config.path_dir_data_overlayfs / "upperdir";
    // Create filesystem based on the contents of src
    ns_layers::create(path_dir_src, path_file_layer, config.layer_compression_level);
    // Include filesystem in the image
    ns_layers::add(config.path_file_binary, path_file_layer);
    // Remove compressed filesystem
    fs::remove(path_file_layer);
    // Remove upper directory
    fs::remove_all(path_dir_src);
  } // else if
  // Enable or disable casefold (useful for wine)
  else if ( auto cmd = ns_variant::get_if_holds_alternative<ns_parser::CmdCaseFold>(*variant_cmd) )
  {
    // Update log level
    if ( not ns_env::get("FIM_DEBUG") )
    {
      ns_log::set_level(ns_log::Level::INFO);
    } // if
    [[maybe_unused]] auto mount = ns_filesystems::Filesystems(config);
    ns_db::from_file(config.path_file_config_casefold, [&](auto& db)
    {
      db("enable") = std::string{cmd->op};
    }, ns_db::Mode::UPDATE_OR_CREATE);
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
    [[maybe_unused]] auto mount = ns_filesystems::Filesystems(config);
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
    auto mount = ns_filesystems::Filesystems(config);
    mount.spawn_janitor();
    // Build exec command
    ns_parser::CmdExec cmd_exec;
    // Fetch default command from database or fallback to bash
    ns_exception::or_else([&]
    {
      ns_db::from_file(config.path_file_config_boot, [&](auto& db)
      {
        cmd_exec.program = db["program"];
        cmd_exec.args = db["args"].as_vector();
        // Expand 'program'
        if ( auto expected = ns_config::ns_environment::expand(cmd_exec.program) )
        {
          cmd_exec.program = *expected;
        } // if
        else
        {
          ns_log::error()("Failed to expand 'program': {}", expected.error());
        } // else
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
    auto environment = ns_exception::or_default([&]{ return ns_config::ns_environment::get(config.path_file_config_environment); });
    f_bwrap(cmd_exec.program, cmd_exec.args, environment, permissions);
  } // else if

  return EXIT_SUCCESS;
} // parse_cmds() }}}

} // namespace ns_parser

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
