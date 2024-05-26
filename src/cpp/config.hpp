///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : config
///

#pragma once

#include <unistd.h>
#include <filesystem>

#include "std/env.hpp"

namespace ns_config
{

namespace
{

namespace fs = std::filesystem;

} // namespace 

// struct FlatimageConfig {{{
struct FlatimageConfig
{
  std::string str_dist;
  bool is_root;
  bool is_readonly;
  bool is_debug;

  uint64_t offset_ext2;
  fs::path path_dir_global;
  fs::path path_dir_mounts;
  fs::path path_dir_mount_ext2;
  fs::path path_dir_temp;
  fs::path path_dir_temp_bin;
  fs::path path_file_binary;
  fs::path path_dir_binary;
  fs::path path_file_bashrc;
  fs::path path_file_bash;
  fs::path path_dir_host_home;
  fs::path path_dir_host_config;
  fs::path path_dir_host_overlayfs;
  fs::path path_dir_mounts_dwarfs;
  fs::path path_dir_mounts_overlayfs;
  fs::path path_dir_runtime;
  fs::path path_dir_runtime_host;
  fs::path path_dir_runtime_mounts;
  fs::path path_dir_runtime_mounts_dwarfs;
  fs::path path_dir_runtime_mounts_overlayfs;

  fs::path path_dir_static;
  fs::path path_file_config;
  fs::path path_file_config_perms;
  fs::path path_dir_dwarfs;
  fs::path path_dir_hooks;

  uint32_t dwarfs_compression_level;
  uint32_t ext2_slack_minimum;

  std::string env_path;
  std::string env_compression_dirs;
}; // }}}

// configure() {{{
inline FlatimageConfig configure()
{
  FlatimageConfig config;

  ns_env::set("PID", std::to_string(getpid()), ns_env::Replace::Y);
  ns_env::set("FIM_DIST", "TRUNK", ns_env::Replace::Y);

  // Distribution
  config.str_dist = "TRUNK";

  // Flags
  config.is_root = ns_env::exists("FIM_ROOT", "1");
  config.is_readonly = ns_env::exists("FIM_RO", "1");
  config.is_debug = ns_env::exists("FIM_DEBUG", "1");

  // Paths in /tmp
  config.offset_ext2   = std::stoll(ns_env::get_or_throw("FIM_OFFSET"));
  config.path_dir_global     = ns_env::get_or_throw("FIM_DIR_GLOBAL");
  config.path_dir_mounts     = ns_env::get_or_throw("FIM_DIR_MOUNTS");
  config.path_dir_mount_ext2 = ns_env::get_or_throw("FIM_DIR_MOUNT");
  config.path_dir_temp       = ns_env::get_or_throw("FIM_DIR_TEMP");
  config.path_dir_temp_bin   = ns_env::get_or_throw("FIM_DIR_TEMP_BIN");
  config.path_file_binary    = ns_env::get_or_throw("FIM_FILE_BINARY");
  config.path_dir_binary     = config.path_file_binary.parent_path();
  config.path_file_bashrc    = config.path_dir_temp / ".bashrc";
  config.path_file_bash      = config.path_dir_temp_bin / "bash";
  config.path_dir_mounts_dwarfs = config.path_dir_mounts / "dwarfs";
  config.path_dir_mounts_overlayfs = config.path_dir_mounts / "overlayfs";

  // Paths inside the ext2 filesystem
  config.path_dir_static        = config.path_dir_mount_ext2 / "fim/static";
  config.path_file_config       = config.path_dir_mount_ext2 / "fim/config.json";
  config.path_file_config_perms = config.path_dir_mount_ext2 / "fim/config/perms.json";
  config.path_dir_dwarfs        = config.path_dir_mount_ext2 / "fim/dwarfs";
  config.path_dir_hooks         = config.path_dir_mount_ext2 / "fim/hooks";

  // Paths on the host machine
  config.path_dir_host_home = ns_env::get_or_throw("HOME");
  config.path_dir_host_config = config.path_file_binary.parent_path()
    / (std::string{"."} + config.path_file_binary.filename().c_str() + ".config");
  config.path_dir_host_overlayfs = config.path_dir_host_config / "overlays";

  // Paths only available inside the container (runtime)
  config.path_dir_runtime = "/tmp/fim/run";
  config.path_dir_runtime_host = config.path_dir_runtime / "host";
  config.path_dir_runtime_mounts = config.path_dir_runtime / "mounts";
  config.path_dir_runtime_mounts_dwarfs = config.path_dir_runtime_mounts / "dwarfs";
  config.path_dir_runtime_mounts_overlayfs = config.path_dir_runtime_mounts / "overlayfs";

  // Environment
  config.env_path = config.path_dir_temp_bin.string() + ":" + ns_env::get_or_throw("PATH");
  config.env_path += ":/sbin:/usr/sbin:/usr/local/sbin:/bin:/usr/bin:/usr/local/bin";
  ns_env::set("PATH", config.env_path, ns_env::Replace::Y);

  // Filesystem configuration
  config.dwarfs_compression_level = std::stoi(ns_env::get_or_else("FIM_COMPRESSION_LEVEL", "4"));
  config.ext2_slack_minimum       = std::stoi(ns_env::get_or_else("FIM_SLACK_MINIMUM", "50"));
  config.env_compression_dirs     = ns_env::get_or_else("FIM_COMPRESSION_DIRS", "/usr:/opt");

  return config;
} // configure() }}}

} // namespace ns_config


/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
