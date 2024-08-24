///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : boot
///

#include <unistd.h>
#include <filesystem>

#include "../cpp/units.hpp"
#include "../cpp/units.hpp"
#include "../cpp/lib/env.hpp"
#include "../cpp/std/variant.hpp"
#include "../cpp/std/functional.hpp"
#include "../cpp/std/exception.hpp"
#include "../cpp/lib/log.hpp"
#include "../cpp/lib/ext2/check.hpp"
#include "../cpp/lib/ext2/mount.hpp"
#include "../cpp/lib/ext2/size.hpp"
#include "../cpp/lib/bwrap.hpp"

#include "parser.hpp"
#include "desktop.hpp"
#include "setup.hpp"
#include "config/environment.hpp"

namespace fs = std::filesystem;


// copy_tools() {{{
void copy_tools(fs::path const& path_dir_tools, fs::path const& path_dir_temp_bin)
{
  // Check if path_dir_tools exists and is directory
  ethrow_if(not fs::is_directory(path_dir_tools), "'{}' does not exist or is not a directory"_fmt(path_dir_tools));
  // Check if path_dir_temp_bin exists and is directory
  ethrow_if(not fs::is_directory(path_dir_temp_bin), "'{}' does not exist or is not a directory"_fmt(path_dir_temp_bin));
  // Copy programs
  for (auto&& path_file_src : fs::directory_iterator(path_dir_tools)
    | std::views::filter([&](auto&& e){ return fs::is_regular_file(e); }))
  {
    fs::path path_file_dst = path_dir_temp_bin / path_file_src.path().filename();
    if ( fs::copy_file(path_file_src, path_file_dst, fs::copy_options::update_existing) )
    {
      ns_log::debug()("Copy '{}' -> '{}'", path_file_src, path_file_dst);
    } // if
  } // for
} // copy_tools() }}}

// parse_cmds() {{{
int parse_cmds(ns_setup::FlatimageSetup config, int argc, char** argv)
{
  // Parse args
  auto variant_cmd = ns_parser::parse(argc, argv);

  auto f_bwrap = [&](std::string const& program
    , std::vector<std::string> const& args
    , std::vector<std::string> const& environment
    , std::set<ns_bwrap::ns_permissions::Permission> const& permissions)
  {
    ns_bwrap::Bwrap(config.is_root
      , config.path_dir_mount_ext2
      , config.path_dir_runtime_host
      , config.path_dir_mounts
      , config.path_dir_runtime_mounts
      , config.path_dir_host_home
      , config.path_file_bashrc
      , program
      , args
      , environment)
      .run(permissions);
  };

  // Execute a command as a regular user
  if ( auto cmd = ns_variant::get_if_holds_alternative<ns_parser::CmdExec>(*variant_cmd) )
  {
    // Mount filesystem as RW
    ns_ext2::ns_mount::mount_rw(config.path_file_binary, config.path_dir_mount_ext2, config.offset_ext2);
    // Execute specified command
    auto permissions = ns_exception::or_default([&]{ return ns_bwrap::ns_permissions::get(config.path_file_config_permissions); });
    auto environment = ns_exception::or_default([&]{ return ns_config::ns_environment::get(config.path_file_config_environment); });
    f_bwrap(cmd->program, cmd->args, environment, permissions);
  } // if
  // Execute a command as root
  else if ( auto cmd = ns_variant::get_if_holds_alternative<ns_parser::CmdRoot>(*variant_cmd) )
  {
    // Mount filesystem as RW
    ns_ext2::ns_mount::mount_rw(config.path_file_binary, config.path_dir_mount_ext2, config.offset_ext2);
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
    ns_log::set_level(ns_log::Level::INFO);
    // Resize to fit the provided amount of free space
    ns_ext2::ns_size::resize_free_space(config.path_file_binary, config.offset_ext2, cmd->size);
  } // if
  // Configure permissions
  else if ( auto cmd = ns_variant::get_if_holds_alternative<ns_parser::CmdPerms>(*variant_cmd) )
  {
    // Update log level
    ns_log::set_level(ns_log::Level::INFO);
    // Mount filesystem as RW
    ns_ext2::ns_mount::mount_rw(config.path_file_binary, config.path_dir_mount_ext2, config.offset_ext2);
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
    ns_log::set_level(ns_log::Level::INFO);
    // Mount filesystem as RW
    ns_ext2::ns_mount::mount_rw(config.path_file_binary, config.path_dir_mount_ext2, config.offset_ext2);
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
    ns_log::set_level(ns_log::Level::INFO);
    // Mount filesystem as RW
    ns_ext2::ns_mount::mount_rw(config.path_file_binary, config.path_dir_mount_ext2, config.offset_ext2);
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
        ns_desktop::setup(config.path_dir_mount_ext2, *opt_path_file_src_json, config.path_file_config_desktop);
      } // case
      break;
    } // switch
  } // if
  // Update default command on database
  else if ( auto cmd = ns_variant::get_if_holds_alternative<ns_parser::CmdBoot>(*variant_cmd) )
  {
    // Update log level
    ns_log::set_level(ns_log::Level::INFO);
    // Mount filesystem as RW
    ns_ext2::ns_mount::mount_rw(config.path_file_binary, config.path_dir_mount_ext2, config.offset_ext2);
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
    // Mount filesystem as RW
    ns_ext2::ns_mount::mount_rw(config.path_file_binary, config.path_dir_mount_ext2, config.offset_ext2);
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

// boot() {{{
void boot(int argc, char** argv)
{
  // Set logger level
  if ( ns_env::exists("FIM_DEBUG", "1") )
  {
    ns_log::set_level(ns_log::Level::DEBUG);
  } // if

  // Setup environment variables
  ns_setup::FlatimageSetup config = ns_setup::setup();

  // Check filesystem
  ns_ext2::ns_check::check(config.path_file_binary, config.offset_ext2);

  // Mount filesystem as RO
  ns_ext2::ns_mount::mount_ro(config.path_file_binary, config.path_dir_mount_ext2, config.offset_ext2);

  // Copy tools
  ns_log::exception([&]{ copy_tools(config.path_dir_static, config.path_dir_temp_bin); });

  // Refresh desktop integration
  ns_log::exception([&]{ ns_desktop::integrate(
      config.path_file_config_desktop
    , config.path_file_binary
    , config.path_dir_mount_ext2);
  });

  // Un-mount
  ns_ext2::ns_mount::unmount(config.path_dir_mount_ext2);

  // Keep at least the provided slack amount of extra free space
  ns_ext2::ns_size::resize_free_space(config.path_file_binary
    , config.offset_ext2
    , ns_units::from_mebibytes(config.ext2_slack_minimum).to_bytes()
  );

  // Parse flatimage command if exists
  parse_cmds(config, argc, argv);
} // boot() }}}

// main() {{{
int main(int argc, char** argv)
{
  if ( auto expected = ns_log::exception([&]{ boot(argc, argv); }); not expected )
  {
    print("Program exited with error: {}\n", expected.error());
    return EXIT_FAILURE;
  } // if
  return EXIT_SUCCESS;
} // main() }}}

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
