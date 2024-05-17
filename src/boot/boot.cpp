///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : boot
///

#include <unistd.h>
#include <filesystem>

#include "../cpp/units.hpp"
#include "../cpp/lib/log.hpp"
#include "../cpp/units.hpp"
#include "../cpp/std/env.hpp"
#include "../cpp/std/filesystem.hpp"
#include "../cpp/lib/subprocess.hpp"
#include "../cpp/lib/ext2/check.hpp"
#include "../cpp/lib/ext2/mount.hpp"
#include "../cpp/lib/ext2/size.hpp"

namespace fs = std::filesystem;

// setup_environment() {{{
void setup_environment()
{
  ns_env::set("PID", std::to_string(getpid()), ns_env::Replace::Y);
  ns_env::set("FIM_DIST", "TRUNK", ns_env::Replace::Y);

  // User type
  ns_env::set_mutual_exclusion("FIM_ROOT", "FIM_NORM", "1");

  // Mount mode
  ns_env::set_mutual_exclusion("FIM_RO", "FIM_RW", "1");

  // Debugging
  ns_env::set_mutual_exclusion("FIM_DEBUG", "FIM_NDEBUG", "1");

  // Filesystem offset
  ns_env::check("FIM_OFFSET");

  // Global directory used to setup flatimage '/tmp/fim'
  ns_env::check("FIM_DIR_GLOBAL");

  // Directory with ext/dwarfs/overlayfs filesystem mounts
  ns_env::check("FIM_DIR_MOUNTS");

  // Directory of the ext filesystem mount
  ns_env::check("FIM_DIR_MOUNT");

  // Temporary directory of the current instance
  ns_env::check("FIM_DIR_TEMP");

  // Temporary binary directory of the current app
  ns_env::check("FIM_DIR_TEMP_BIN");

  // Path to the main flatimage file of the current app
  ns_env::check("FIM_FILE_BINARY");

  // Paths internal to the image
  fs::path path_dir_mount = ns_env::get("FIM_DIR_MOUNT");
  ns_env::set("FIM_DIR_STATIC", path_dir_mount / "fim/static", ns_env::Replace::Y);
  ns_env::set("FIM_FILE_CONFIG", path_dir_mount / "fim/config", ns_env::Replace::Y);
  ns_env::set("FIM_FILE_PERMS", path_dir_mount / "fim/perms", ns_env::Replace::Y);
  ns_env::set("FIM_DIR_DWARFS", path_dir_mount / "fim/dwarfs", ns_env::Replace::Y);
  ns_env::set("FIM_DIR_HOOKS", path_dir_mount / "fim/hooks", ns_env::Replace::Y);

  // Path and basename for the main binary
  fs::path path_file_binary = ns_env::get("FIM_FILE_BINARY");
  ns_env::set("FIM_BASENAME_BINARY", path_file_binary.filename(), ns_env::Replace::Y);
  ns_env::set("FIM_DIR_BINARY", path_file_binary.parent_path(), ns_env::Replace::Y);

  // Bash binary and bashrc files
  fs::path path_dir_temp = ns_env::get("FIM_DIR_TEMP");
  ns_env::set("BASHRC_FILE", path_dir_temp / ".bashrc", ns_env::Replace::Y);
  fs::path path_dir_temp_bin = ns_env::get("FIM_DIR_TEMP_BIN");
  ns_env::set("FIM_FILE_BASH", path_dir_temp_bin / "bash", ns_env::Replace::Y);

  // Default host configuration directories
  fs::path path_dir_host_config = path_file_binary.parent_path() / (std::string{"."} + path_file_binary.filename().c_str() + ".config");
  ns_env::set("FIM_DIR_HOST_CONFIG", path_dir_host_config, ns_env::Replace::Y);
  ns_env::set("FIM_DIR_HOST_OVERLAYS", path_dir_host_config / "overlays", ns_env::Replace::Y);

  // Directories with mountpoints for respective filesystem types
  fs::path path_dir_mounts = ns_env::get("FIM_DIR_MOUNTS");
  ns_env::set("FIM_DIR_MOUNTS_DWARFS", path_dir_mounts / "dwarfs", ns_env::Replace::Y);
  ns_env::set("FIM_DIR_MOUNTS_OVERLAYFS", path_dir_mounts / "overlayfs", ns_env::Replace::Y);

  // Runtime directories (only exist inside the container, binds to host)
  fs::path path_dir_runtime = "/tmp/fim/run";
  ns_env::set("FIM_DIR_RUNTIME", path_dir_runtime, ns_env::Replace::Y);
  ns_env::set("FIM_DIR_RUNTIME_HOST", path_dir_runtime / "host", ns_env::Replace::Y);
  fs::path path_dir_runtime_mounts = path_dir_runtime / "mounts";
  ns_env::set("FIM_DIR_RUNTIME_MOUNTS", path_dir_runtime_mounts, ns_env::Replace::Y);
  ns_env::set("FIM_DIR_RUNTIME_MOUNTS_DWARFS", path_dir_runtime_mounts / "dwarfs", ns_env::Replace::Y);
  ns_env::set("FIM_DIR_RUNTIME_MOUNTS_OVERLAYFS", path_dir_runtime_mounts / "overlayfs", ns_env::Replace::Y);

  // Give static tools priority in PATH
  ns_env::prepend("PATH", path_dir_temp_bin.string() + ":");

  // Compression
  ns_env::set("FIM_COMPRESSION_LEVEL", "4", ns_env::Replace::N);
  ns_env::set("FIM_SLACK_MINIMUM", "50", ns_env::Replace::N);
  ns_env::set("FIM_COMPRESSION_DIRS", "/usr:/opt", ns_env::Replace::N);
} // setup_environment() }}}

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
  if ( const char* env_fim_debug = ns_env::get("FIM_DEBUG"); env_fim_debug && std::string_view{env_fim_debug} == "1" )
  {
    ns_log::set_level(ns_log::Level::DEBUG);
  } // if

  // Setup environment variables
  setup_environment();

  // Check filesystem
  fs::path path_file_binary = ns_env::get("FIM_FILE_BINARY");
  uint64_t offset_path_file_binary = std::stoi(ns_env::get("FIM_OFFSET"));
  ns_ext2::ns_check::check(path_file_binary, offset_path_file_binary);

  // Mount filesystem as RO
  fs::path path_dir_mount = ns_env::get("FIM_DIR_MOUNT");
  ns_ext2::ns_mount::mount_ro(path_file_binary, path_dir_mount, offset_path_file_binary);

  // Copy tools
  fs::path path_dir_temp_bin = ns_env::get("FIM_DIR_TEMP_BIN");
  copy_tools(path_dir_mount / "fim/static", path_dir_temp_bin);

  // Un-mount
  ns_ext2::ns_mount::unmount(path_dir_mount);

  // Keep at least FIM_SLACK_MINIMUM of extra free space
  const char* env_fim_slack_minimum = ns_env::get("FIM_SLACK_MINIMUM");
  ereturn_if(not env_fim_slack_minimum, "FIM_SLACK_MINIMUM is not defined", 1);
  unsigned long long fim_slack_minimum = std::stoll(env_fim_slack_minimum);
  ereturn_if(fim_slack_minimum < 0, "Invalid value '{}' for FIM_SLACK_MINIMUM"_fmt(fim_slack_minimum), 1);
  ns_ext2::ns_size::resize_free_space(path_file_binary
    , offset_path_file_binary
    , ns_units::from_mebibytes(fim_slack_minimum).to_bytes()
  );

  return EXIT_SUCCESS;
} // main() }}}

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
