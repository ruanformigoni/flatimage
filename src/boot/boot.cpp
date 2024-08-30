///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : boot
///

// Version
#ifndef VERSION
#define VERSION "unknown"
#endif

// Git commit hash
#ifndef COMMIT
#define COMMIT "unknown"
#endif

// Compilation timestamp
#ifndef TIMESTAMP
#define TIMESTAMP "unknown"
#endif

#include <elf.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <filesystem>

#include "../cpp/units.hpp"
#include "../cpp/lib/linux.hpp"
#include "../cpp/lib/env.hpp"
#include "../cpp/lib/log.hpp"
#include "../cpp/lib/elf.hpp"
#include "../cpp/lib/ext2/check.hpp"
#include "../cpp/lib/ext2/size.hpp"

#include "parser.hpp"
#include "desktop.hpp"
#include "portal.hpp"
#include "setup.hpp"

// Unix environment variables
extern char** environ;

namespace fs = std::filesystem;

// copy_tools() {{{
void copy_tools(ns_setup::FlatimageSetup const& config)
{
  // Mount filesystem as RO
  [[maybe_unused]] auto mount = ns_filesystems::Filesystems(config);
  // Check if path_dir_static exists and is directory
  ethrow_if(not fs::is_directory(config.path_dir_static), "'{}' does not exist or is not a directory"_fmt(config.path_dir_static));
  // Check if path_dir_app_bin exists and is directory
  ethrow_if(not fs::is_directory(config.path_dir_app_bin), "'{}' does not exist or is not a directory"_fmt(config.path_dir_app_bin));
  // Copy programs
  for (auto&& path_file_src : fs::directory_iterator(config.path_dir_static)
    | std::views::filter([&](auto&& e){ return fs::is_regular_file(e); }))
  {
    fs::path path_file_dst = config.path_dir_app_bin / path_file_src.path().filename();
    if ( fs::copy_file(path_file_src, path_file_dst, fs::copy_options::update_existing) )
    {
      ns_log::debug()("Copy '{}' -> '{}'", path_file_src, path_file_dst);
    } // if
  } // for
} // copy_tools() }}}

// relocate() {{{
void relocate(char** argv)
{
  // This part of the code is executed to write the runner,
  // rightafter the code is replaced by the runner.
  // This is done because the current executable cannot mount itself.

  // Get path to called executable
  ethrow_if(!std::filesystem::exists("/proc/self/exe"), "Error retrieving executable path for self");
  auto path_absolute = fs::read_symlink("/proc/self/exe");

  // Create base dir
  fs::path path_dir_base = "/tmp/fim";
  ethrow_if (not fs::exists(path_dir_base) and not fs::create_directories(path_dir_base)
    , "Failed to create directory {}"_fmt(path_dir_base)
  );

  // Make the temporary directory name
  fs::path path_dir_app = path_dir_base / "app" / "{}_{}"_fmt(COMMIT, TIMESTAMP);
  ethrow_if(not fs::exists(path_dir_app) and not fs::create_directories(path_dir_app)
    , "Failed to create directory {}"_fmt(path_dir_app)
  );

  // Create bin dir
  fs::path path_dir_app_bin = path_dir_app / "bin";
  ethrow_if(not fs::exists(path_dir_app_bin) and not fs::create_directories(path_dir_app_bin),
    "Failed to create directory {}"_fmt(path_dir_app_bin)
  );

  // Set variables
  ns_env::set("FIM_DIR_GLOBAL", path_dir_base.c_str(), ns_env::Replace::Y);
  ns_env::set("FIM_DIR_APP", path_dir_app.c_str(), ns_env::Replace::Y);
  ns_env::set("FIM_DIR_APP_BIN", path_dir_app_bin.c_str(), ns_env::Replace::Y);
  ns_env::set("FIM_FILE_BINARY", path_absolute.c_str(), ns_env::Replace::Y);

  // Create instance directory
  fs::path path_dir_instance_prefix = "{}/{}/"_fmt(path_dir_app, "instance");

  // Create temp dir to mount filesystems into
  fs::path path_dir_instance = ns_linux::mkdtemp(path_dir_instance_prefix);
  ns_env::set("FIM_DIR_INSTANCE", path_dir_instance.c_str(), ns_env::Replace::Y);

  // Path to directory with mount points
  fs::path path_dir_mount = path_dir_instance / "mount";
  ns_env::set("FIM_DIR_MOUNT", path_dir_mount.c_str(), ns_env::Replace::Y);
  ethrow_if(not fs::exists(path_dir_mount) and not fs::create_directory(path_dir_mount)
    , "Could not mount directory '{}'"_fmt(path_dir_mount)
  );

  // Path to ext mount dir is a directory called 'ext'
  fs::path path_dir_mount_ext = path_dir_mount / "ext";
  ns_env::set("FIM_DIR_MOUNT_EXT", path_dir_mount_ext.c_str(), ns_env::Replace::Y);
  ethrow_if(not fs::exists(path_dir_mount_ext) and not fs::create_directory(path_dir_mount_ext)
    , "Could not mount directory '{}'"_fmt(path_dir_mount_ext)
  );

  // Starting offsets
  uint64_t offset_beg = 0;
  uint64_t offset_end = ns_elf::skip_elf_header(path_absolute.c_str());

  // Write by binary offset
  auto f_write_bin = [&](fs::path path_file, uint64_t offset_end)
  {
    // Update offsets
    offset_beg = offset_end;
    offset_end = ns_elf::skip_elf_header(path_absolute.c_str(), offset_beg) + offset_beg;
    // Write binary only if it doesnt already exist
    if ( ! fs::exists(path_file) )
    {
      ns_elf::copy_binary(path_absolute.string(), path_file, {offset_beg, offset_end});
    }
    // Set permissions
    fs::permissions(path_file.c_str(), fs::perms::owner_all | fs::perms::group_all);
    // Return new values for offsets
    return std::make_pair(offset_beg, offset_end);
  };

  // Write binaries
  auto start = std::chrono::high_resolution_clock::now();
  std::tie(offset_beg, offset_end) = f_write_bin(path_dir_instance / "ext.boot" , 0);
  std::tie(offset_beg, offset_end) = f_write_bin(path_dir_app_bin  / "fuse2fs"  , offset_end);
  std::tie(offset_beg, offset_end) = f_write_bin(path_dir_app_bin  / "e2fsck"   , offset_end);
  std::tie(offset_beg, offset_end) = f_write_bin(path_dir_app_bin  / "bash"     , offset_end);
  auto end = std::chrono::high_resolution_clock::now();

  // Filesystem starts here
  ns_env::set("FIM_OFFSET", std::to_string(offset_end).c_str(), ns_env::Replace::Y);
  ns_log::debug()("FIM_OFFSET: {}", offset_end);

  // Option to show offset and exit (to manually mount the fs with fuse2fs)
  if( getenv("FIM_MAIN_OFFSET") ){ "{}"_print(offset_end); exit(0); }

  // Print copy duration
  if ( getenv("FIM_DEBUG") != nullptr )
  {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    "Copy binaries finished in '{}' ms"_print(elapsed.count());
  } // if

  // Launch Runner
  execve("{}/ext.boot"_fmt(path_dir_instance).c_str(), argv, environ);
} // relocate() }}}

// boot() {{{
void boot(int argc, char** argv)
{
  // Setup environment variables
  ns_setup::FlatimageSetup config = ns_setup::setup();

  // Check filesystem
  ns_ext2::ns_check::check(config.path_file_binary, config.offset_ext2);

  // Copy tools
  if (auto expected = ns_log::exception([&]{ copy_tools(config); }); not expected)
  {
    ns_log::error()("Error while copying files '{}'", expected.error());
  } // if

  // Start portal
  ns_portal::Portal portal = ns_portal::Portal(config.path_dir_instance / "ext.boot");

  // Refresh desktop integration
  if (auto expected = ns_log::exception([&]{ ns_desktop::integrate(
      config.path_file_config_desktop
    , config.path_file_binary
    , config.path_dir_mount_ext);
  }); not expected)
  {
    ns_log::error()("Error in desktop integration '{}'", expected.error());
  } // if

  // Keep at least the provided slack amount of extra free space
  ns_ext2::ns_size::resize_free_space(config.path_file_binary
    , config.offset_ext2
    , ns_units::from_mebibytes(config.ext2_slack_minimum).to_bytes()
  );

  // Parse flatimage command if exists
  ns_parser::parse_cmds(config, argc, argv);
} // boot() }}}

// main() {{{
int main(int argc, char** argv)
{
  // Print version and exit
  if ( argc > 1 && std::string{argv[1]} == "fim-version" )
  {
    println(VERSION);
    return EXIT_SUCCESS;
  } // if
  ns_env::set("FIM_VERSION", VERSION, ns_env::Replace::Y);

  // Set logger level
  if ( ns_env::exists("FIM_DEBUG", "1") )
  {
    ns_log::set_level(ns_log::Level::DEBUG);
  } // if

  // Get path to self
  auto expected_path_file_self = ns_filesystem::ns_path::file_self();
  ereturn_if(not expected_path_file_self, expected_path_file_self.error(), EXIT_FAILURE);
  fs::path path_file_self = *expected_path_file_self;

  // If it is outside /tmp, move the binary
  if (std::distance(path_file_self.begin(), path_file_self.end()) < 2 or *std::next(path_file_self.begin()) != "tmp")
  {
    ns_log::debug()("Relocate program from {}", path_file_self);
    relocate(argv);
    // This function should not reach the return statement due to evecve
    return EXIT_FAILURE;
  } // if

  // Boot the main program
  ns_log::debug()("Boot program from {}", path_file_self);
  if ( auto expected = ns_log::exception([&]{ boot(argc, argv); }); not expected )
  {
    print("Program exited with error: {}\n", expected.error());
    return EXIT_FAILURE;
  } // if

  return EXIT_SUCCESS;
} // main() }}}

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
