///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : setup
///

#pragma once

#include <unistd.h>
#include <filesystem>

#include "../cpp/lib/env.hpp"

namespace ns_setup
{

namespace
{

namespace fs = std::filesystem;

} // namespace

// struct FlatimageSetup {{{
struct FlatimageSetup
{
  std::string str_dist;
  bool is_root;
  bool is_readonly;
  bool is_debug;

  uint64_t offset_ext2;
  fs::path path_dir_global;
  fs::path path_dir_instance;
  fs::path path_dir_mount;
  fs::path path_dir_mount_ext;
  fs::path path_dir_app;
  fs::path path_dir_app_bin;
  fs::path path_file_binary;
  fs::path path_dir_binary;
  fs::path path_file_bashrc;
  fs::path path_file_bash;
  fs::path path_dir_host_home;
  fs::path path_dir_host_config;
  fs::path path_dir_host_overlayfs;
  fs::path path_dir_mount_dwarfs;
  fs::path path_dir_mount_overlayfs;
  fs::path path_dir_runtime;
  fs::path path_dir_runtime_host;
  fs::path path_dir_runtime_mounts;
  fs::path path_dir_runtime_mounts_dwarfs;
  fs::path path_dir_runtime_mounts_overlayfs;

  fs::path path_dir_static;
  fs::path path_file_config_boot;
  fs::path path_file_config_environment;
  fs::path path_file_config_permissions;
  fs::path path_file_config_desktop;
  fs::path path_dir_dwarfs;
  fs::path path_dir_hooks;

  uint32_t dwarfs_compression_level;
  uint32_t ext2_slack_minimum;

  std::string env_path;
  std::string env_compression_dirs;
}; // }}}

// setup() {{{
inline FlatimageSetup setup()
{
  FlatimageSetup setup;

  ns_env::set("PID", std::to_string(getpid()), ns_env::Replace::Y);
  ns_env::set("FIM_DIST", "TRUNK", ns_env::Replace::Y);

  // Distribution
  setup.str_dist = "TRUNK";

  // Flags
  setup.is_root = ns_env::exists("FIM_ROOT", "1");
  setup.is_readonly = ns_env::exists("FIM_RO", "1");
  setup.is_debug = ns_env::exists("FIM_DEBUG", "1");

  // Paths in /tmp
  setup.offset_ext2               = std::stoll(ns_env::get_or_throw("FIM_OFFSET"));
  setup.path_dir_global           = ns_env::get_or_throw("FIM_DIR_GLOBAL");
  setup.path_dir_instance         = ns_env::get_or_throw("FIM_DIR_INSTANCE");
  setup.path_dir_mount            = ns_env::get_or_throw("FIM_DIR_MOUNT");
  setup.path_dir_mount_ext       = ns_env::get_or_throw("FIM_DIR_MOUNT_EXT");
  setup.path_dir_app             = ns_env::get_or_throw("FIM_DIR_APP");
  setup.path_dir_app_bin         = ns_env::get_or_throw("FIM_DIR_APP_BIN");
  setup.path_file_binary          = ns_env::get_or_throw("FIM_FILE_BINARY");
  setup.path_dir_binary           = setup.path_file_binary.parent_path();
  setup.path_file_bashrc          = setup.path_dir_app / ".bashrc";
  setup.path_file_bash            = setup.path_dir_app_bin / "bash";
  setup.path_dir_mount_dwarfs    = setup.path_dir_instance / "dwarfs";
  setup.path_dir_mount_overlayfs = setup.path_dir_instance / "overlayfs";

  // Paths inside the ext2 filesystem
  setup.path_dir_static              = setup.path_dir_mount_ext / "fim/static";
  setup.path_file_config_boot        = setup.path_dir_mount_ext / "fim/config/boot.json";
  setup.path_file_config_environment = setup.path_dir_mount_ext / "fim/config/environment.json";
  setup.path_file_config_permissions = setup.path_dir_mount_ext / "fim/config/permissions.json";
  setup.path_file_config_desktop     = setup.path_dir_mount_ext / "fim/config/desktop.json";
  setup.path_dir_dwarfs              = setup.path_dir_mount_ext / "fim/dwarfs";
  setup.path_dir_hooks               = setup.path_dir_mount_ext / "fim/hooks";

  // Paths on the host machine
  setup.path_dir_host_home = ns_env::get_or_throw("HOME");
  setup.path_dir_host_config = setup.path_file_binary.parent_path()
    / (std::string{"."} + setup.path_file_binary.filename().c_str() + ".config");
  ns_env::set("FIM_DIR_HOST_CONFIG", setup.path_dir_host_config, ns_env::Replace::Y);
  setup.path_dir_host_overlayfs = setup.path_dir_host_config / "overlays";

  // Paths only available inside the container (runtime)
  setup.path_dir_runtime = "/tmp/fim/run";
  setup.path_dir_runtime_host = setup.path_dir_runtime / "host";
  setup.path_dir_runtime_mounts = setup.path_dir_runtime / "mounts";
  setup.path_dir_runtime_mounts_dwarfs = setup.path_dir_runtime_mounts / "dwarfs";
  setup.path_dir_runtime_mounts_overlayfs = setup.path_dir_runtime_mounts / "overlayfs";

  // Environment
  setup.env_path = setup.path_dir_app_bin.string() + ":" + ns_env::get_or_throw("PATH");
  setup.env_path += ":/sbin:/usr/sbin:/usr/local/sbin:/bin:/usr/bin:/usr/local/bin";
  ns_env::set("PATH", setup.env_path, ns_env::Replace::Y);

  // Filesystem configuration
  setup.dwarfs_compression_level = std::stoi(ns_env::get_or_else("FIM_COMPRESSION_LEVEL", "4"));
  setup.ext2_slack_minimum       = std::stoi(ns_env::get_or_else("FIM_SLACK_MINIMUM", "10"));
  setup.env_compression_dirs     = ns_env::get_or_else("FIM_COMPRESSION_DIRS", "/usr:/opt");

  return setup;
} // setup() }}}

} // namespace ns_setup


/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
