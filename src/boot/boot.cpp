///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : boot
///

#include <unistd.h>
#include <filesystem>

#include "../cpp/units.hpp"
#include "../cpp/config.hpp"
#include "../cpp/lib/log.hpp"
#include "../cpp/units.hpp"
#include "../cpp/std/env.hpp"
#include "../cpp/std/filesystem.hpp"
#include "../cpp/lib/subprocess.hpp"
#include "../cpp/lib/ext2/check.hpp"
#include "../cpp/lib/ext2/mount.hpp"
#include "../cpp/lib/ext2/size.hpp"
#include "../cpp/lib/bwrap.hpp"

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
int main()
{
  // Set logger level
  if ( ns_env::exists("FIM_DEBUG", "1") )
  {
    ns_log::set_level(ns_log::Level::DEBUG);
  } // if

  // Setup environment variables
  ns_config::FlatimageConfig config = ns_config::configure();

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

  // Mount filesystem as RW
  ns_ext2::ns_mount::mount_rw(config.path_file_binary, config.path_dir_mount_ext2, config.offset_ext2);

  // Run bwrap
  ns_bwrap::Permissions permissions;
  // permissions |= ns_bwrap::Permissions::HOME_RW;
  // permissions |= ns_bwrap::Permissions::HOME_RO;
  // permissions |= ns_bwrap::Permissions::MEDIA_RW;
  // permissions |= ns_bwrap::Permissions::MEDIA_RO;
  // permissions |= ns_bwrap::Permissions::AUDIO;
  // permissions |= ns_bwrap::Permissions::WAYLAND;
  // permissions |= ns_bwrap::Permissions::XORG;
  // permissions |= ns_bwrap::Permissions::DBUS_USER;
  // permissions |= ns_bwrap::Permissions::DBUS_SYSTEM;
  // permissions |= ns_bwrap::Permissions::UDEV;
  // permissions |= ns_bwrap::Permissions::INPUT;
  // permissions |= ns_bwrap::Permissions::USB;
  // permissions |= ns_bwrap::Permissions::GPU;

  ns_bwrap::Bwrap(config
    , permissions
    , config.path_dir_temp_bin / "bash")
    .bind_root(config.path_dir_runtime_host)
    .bind_home(config.path_dir_host_home)
    .bind_media()
    .bind_audio()
    .bind_wayland()
    .bind_xorg()
    .bind_dbus_user()
    .bind_dbus_system()
    .bind_udev()
    .bind_input()
    .bind_usb()
    .bind_network()
    .bind_gpu()
    .bind_runtime_mounts(config.path_dir_mounts, config.path_dir_runtime_mounts)
    .run();

  return EXIT_SUCCESS;
} // main() }}}

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
