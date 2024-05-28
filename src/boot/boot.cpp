///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : boot
///

#include <unistd.h>
#include <filesystem>

#include "../cpp/units.hpp"
#include "../cpp/setup.hpp"
#include "../cpp/units.hpp"
#include "../cpp/config/permissions.hpp"
#include "../cpp/config/environment.hpp"
#include "../cpp/std/env.hpp"
#include "../cpp/std/variant.hpp"
#include "../cpp/std/functional.hpp"
#include "../cpp/std/exception.hpp"
#include "../cpp/lib/log.hpp"
#include "../cpp/lib/ext2/check.hpp"
#include "../cpp/lib/ext2/mount.hpp"
#include "../cpp/lib/ext2/size.hpp"
#include "../cpp/lib/bwrap.hpp"
#include "../cpp/lib/parser.hpp"

namespace fs = std::filesystem;


// copy_tools() {{{
void copy_tools(fs::path const& path_dir_tools, fs::path const& path_dir_temp_bin)
{
  // Check if path_dir_tools exists and is directory
  ereturn_if(not fs::is_directory(path_dir_tools), "'{}' does not exist or is not a directory"_fmt(path_dir_tools));
  // Check if path_dir_temp_bin exists and is directory
  ereturn_if(not fs::is_directory(path_dir_temp_bin), "'{}' does not exist or is not a directory"_fmt(path_dir_temp_bin));
  // Copy programs
  for (auto&& path_file_src : fs::directory_iterator(path_dir_tools)
    | std::views::filter([&](auto&& e){ return fs::is_regular_file(e); }))
  {
    fs::path path_file_dst = path_dir_temp_bin / path_file_src.path().filename();
    if ( fs::copy_file(path_file_src, path_file_dst, fs::copy_options::update_existing) )
    {
      ns_log::debug("Copy '{}' -> '{}'", path_file_src, path_file_dst);
    } // if
  } // for
} // copy_tools() }}}

// main() {{{
int main(int argc, char** argv)
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
  copy_tools(config.path_dir_static, config.path_dir_temp_bin);

  // Un-mount
  ns_ext2::ns_mount::unmount(config.path_dir_mount_ext2);

  // Keep at least the provided slack amount of extra free space
  ns_ext2::ns_size::resize_free_space(config.path_file_binary
    , config.offset_ext2
    , ns_units::from_mebibytes(config.ext2_slack_minimum).to_bytes()
  );

  // Parse args
  std::optional<ns_parser::CmdType> opt_cmd = ns_parser::parse(argc, argv);

  // Check if any was passed
  if ( not opt_cmd ) { return EXIT_SUCCESS; } // if

  // Execute a command as a regular user
  if ( auto cmd = ns_variant::get_if_holds_alternative<ns_parser::CmdExec>(*opt_cmd) )
  {
    // Mount filesystem as RW
    ns_ext2::ns_mount::mount_rw(config.path_file_binary, config.path_dir_mount_ext2, config.offset_ext2);
    // Execute specified command
    auto permissions = ns_exception::or_default([&]{ return ns_config::ns_permissions::get(config); });
    auto environment = ns_exception::or_default([&]{ return ns_config::ns_environment::get(config); });
    ns_bwrap::Bwrap(config, cmd->program, cmd->args, environment).run(permissions);
  } // if
  // Execute a command as root
  else if ( auto cmd = ns_variant::get_if_holds_alternative<ns_parser::CmdRoot>(*opt_cmd) )
  {
    // Mount filesystem as RW
    ns_ext2::ns_mount::mount_rw(config.path_file_binary, config.path_dir_mount_ext2, config.offset_ext2);
    // Execute specified command as 'root'
    config.is_root = true;
    auto permissions = ns_exception::or_default([&]{ return ns_config::ns_permissions::get(config); });
    auto environment = ns_exception::or_default([&]{ return ns_config::ns_environment::get(config); });
    ns_bwrap::Bwrap(config, cmd->program, cmd->args, environment).run(permissions);
  } // if
  // Resize the image to contain at least the provided free space
  else if ( auto cmd = ns_variant::get_if_holds_alternative<ns_parser::CmdResize>(*opt_cmd) )
  {
    // Resize to fit the provided amount of free space
    ns_ext2::ns_size::resize_free_space(config.path_file_binary, config.offset_ext2, cmd->size);
  } // if
  // Configure permissions
  else if ( auto cmd = ns_variant::get_if_holds_alternative<ns_parser::CmdPerms>(*opt_cmd) )
  {
    // Mount filesystem as RW
    ns_ext2::ns_mount::mount_rw(config.path_file_binary, config.path_dir_mount_ext2, config.offset_ext2);
    // Create config dir if not exists
    fs::create_directories(config.path_file_config_environment.parent_path());
    // Determine open mode
    switch( cmd->op )
    {
      case ns_parser::CmdPermsOp::ADD: ns_config::ns_permissions::add(config, cmd->permissions); break;
      case ns_parser::CmdPermsOp::SET: ns_config::ns_permissions::set(config, cmd->permissions); break;
      case ns_parser::CmdPermsOp::DEL: ns_config::ns_permissions::del(config, cmd->permissions); break;
      case ns_parser::CmdPermsOp::LIST: std::ranges::for_each(ns_config::ns_permissions::get(config), ns_functional::PrintLn{}); break;
    } // switch
  } // if
  // Configure environment
  else if ( auto cmd = ns_variant::get_if_holds_alternative<ns_parser::CmdEnv>(*opt_cmd) )
  {
    // Mount filesystem as RW
    ns_ext2::ns_mount::mount_rw(config.path_file_binary, config.path_dir_mount_ext2, config.offset_ext2);
    // Create config dir if not exists
    fs::create_directories(config.path_file_config_environment.parent_path());
    // Determine open mode
    switch( cmd->op )
    {
      case ns_parser::CmdEnvOp::ADD: ns_config::ns_environment::add(config, cmd->environment); break;
      case ns_parser::CmdEnvOp::SET: ns_config::ns_environment::set(config, cmd->environment); break;
      case ns_parser::CmdEnvOp::DEL: ns_config::ns_environment::del(config, cmd->environment); break;
      case ns_parser::CmdEnvOp::LIST: std::ranges::for_each(ns_config::ns_environment::get(config), ns_functional::PrintLn{}); break;
    } // switch
  } // if


  return EXIT_SUCCESS;
} // main() }}}

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
