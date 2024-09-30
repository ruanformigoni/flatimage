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

#include "config/config.hpp"
#include "parser.hpp"
#include "portal.hpp"

// Unix environment variables
extern char** environ;

namespace fs = std::filesystem;

// class OnlinePreProcessing {{{
struct OnlinePreProcessing
{
  private:
    void casefold(ns_config::FlatimageConfig const& config);
    void tools(ns_config::FlatimageConfig const& config);
  public:
    OnlinePreProcessing(ns_config::FlatimageConfig const& config)
    {
      [[maybe_unused]] auto mount = ns_filesystems::Filesystems(config);
      ns_log::exception([&]{ casefold(config); });
      ns_log::exception([&]{ tools(config); });
    }
}; // class: OnlinePreProcessing }}}

// tools() {{{
void OnlinePreProcessing::tools(ns_config::FlatimageConfig const& config)
{
  // Check if path_dir_static exists and is directory
  ethrow_if(not fs::is_directory(config.path_dir_static), "'{}' does not exist or is not a directory"_fmt(config.path_dir_static));
  // Check if path_dir_app_bin exists and is directory
  ethrow_if(not fs::is_directory(config.path_dir_app_bin), "'{}' does not exist or is not a directory"_fmt(config.path_dir_app_bin));
  // Copy programs
  std::error_code ec;
  for (auto&& path_file_src : fs::directory_iterator(config.path_dir_static)
    | std::views::filter([&](auto&& e){ return fs::is_regular_file(e) or fs::is_symlink(e); }))
  {
    fs::path path_file_dst = config.path_dir_app_bin / path_file_src.path().filename();
    if ( fs::is_symlink(path_file_src) )
    {
      ns_log::debug()("Symlink '{}' -> '{}'", path_file_src, path_file_dst);
      fs::copy_symlink(path_file_src, path_file_dst, ec);
      if ( ec ) { ns_log::debug()(ec.message()); ec.clear(); }
    }
    else if ( fs::is_regular_file(path_file_src) )
    {
      fs::copy_file(path_file_src, path_file_dst, fs::copy_options::skip_existing, ec);
      ns_log::debug()("Copy '{}' -> '{}'", path_file_src, path_file_dst);
      if ( ec ) { ns_log::debug()(ec.message()); ec.clear(); }
    } // if
  } // for
} // tools() }}}

// casefold() {{{
void OnlinePreProcessing::casefold(ns_config::FlatimageConfig const& config)
{
  // Check if should enable casefold
  if ( auto expected = ns_db::query_nothrow(config.path_file_config_casefold, "enable"); expected and *expected == "ON" )
  {
    ns_env::set("FIM_CASEFOLD", "1", ns_env::Replace::Y);
  } // if
  else
  {
    ns_log::debug()("ciopfs is disabled");
  } // else
} // casefold() }}}

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
  fs::path path_file_dwarfs_aio = path_dir_app_bin / "dwarfs_aio";
  std::error_code ec;
  std::tie(offset_beg, offset_end) = f_write_bin(path_dir_instance / "ext.boot" , 0);
  std::tie(offset_beg, offset_end) = f_write_bin(path_file_dwarfs_aio, offset_end);
  fs::create_symlink(path_file_dwarfs_aio, path_dir_app_bin / "dwarfs", ec);
  fs::create_symlink(path_file_dwarfs_aio, path_dir_app_bin / "mkdwarfs", ec);
  auto end = std::chrono::high_resolution_clock::now();

  // Filesystem starts here
  ns_env::set("FIM_OFFSET", std::to_string(offset_end).c_str(), ns_env::Replace::Y);
  ns_log::debug()("FIM_OFFSET: {}", offset_end);

  // Option to show offset and exit (to manually mount the fs with fuse2fs)
  if( getenv("FIM_MAIN_OFFSET") ){ println(offset_end); exit(0); }

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
  ns_config::FlatimageConfig config = ns_config::config();

  // Set log file
  ns_log::set_sink_file(config.path_dir_mount.string() + ".boot.log");

  // Perform pre-processing step
  OnlinePreProcessing{config};

  // Start portal
  ns_portal::Portal portal = ns_portal::Portal(config.path_dir_instance / "ext.boot");

  // Refresh desktop integration
  ns_log::exception([&]{ ns_desktop::integrate(config); });

  // Parse flatimage command if exists
  ns_parser::parse_cmds(config, argc, argv);

  // Wait until flatimage is not busy
  if (auto error = ns_subprocess::wait_busy_file(config.path_file_binary); error)
  {
    ns_log::error()(*error);
  } // if
} // boot() }}}

// main() {{{
int main(int argc, char** argv)
{
  // Initialize logger level
  if ( ns_env::exists("FIM_DEBUG", "1") )
  {
    ns_log::set_level(ns_log::Level::DEBUG);
  } // if

  // Print version and exit
  if ( argc > 1 && std::string{argv[1]} == "fim-version" )
  {
    println(VERSION);
    return EXIT_SUCCESS;
  } // if
  ns_env::set("FIM_VERSION", VERSION, ns_env::Replace::Y);

  // Get path to self
  auto expected_path_file_self = ns_filesystem::ns_path::file_self();
  if(not expected_path_file_self)
  {
    println(expected_path_file_self.error());
    return EXIT_FAILURE;
  } // if

  // If it is outside /tmp, move the binary
  fs::path path_file_self = *expected_path_file_self;
  if (std::distance(path_file_self.begin(), path_file_self.end()) < 2 or *std::next(path_file_self.begin()) != "tmp")
  {
    relocate(argv);
    // This function should not reach the return statement due to evecve
    return EXIT_FAILURE;
  } // if

  // Boot the main program
  if ( auto expected = ns_exception::to_expected([&]{ boot(argc, argv); }); not expected )
  {
    println("Program exited with error: {}", expected.error());
    return EXIT_FAILURE;
  } // if

  return EXIT_SUCCESS;
} // main() }}}

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
