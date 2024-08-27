///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : boot
///

// Git commit hash
#ifndef COMMIT
#define COMMIT "unknown"
#endif

// Compilation timestamp
#ifndef TIMESTAMP
#define TIMESTAMP "unknown"
#endif

#if defined(__LP64__)
#define ElfW(type) Elf64_ ## type
#else
#define ElfW(type) Elf32_ ## type
#endif

#include <elf.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
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

// Unix environment variables
extern char** environ;

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

// create_temp_dir() {{{
std::string create_temp_dir(std::string const& prefix)
{
  fs::create_directories(prefix);
  std::string temp_dir_template = prefix + "XXXXXX";
  auto temp_dir_template_cstr = std::unique_ptr<char[]>(new char[temp_dir_template.size() + 1]);
  std::strcpy(temp_dir_template_cstr.get(), temp_dir_template.c_str());
  char* temp_dir_cstr = mkdtemp(temp_dir_template_cstr.get());
  ethrow_if(temp_dir_cstr == NULL, "Failed to create temporary dir {}"_fmt(temp_dir_template_cstr.get()));
  return temp_dir_cstr;
} // function: create_temp_dir }}}

// write_from_offset() {{{
void write_from_offset(std::string const& f_in_str, std::string const& f_out_str, std::pair<uint64_t,uint64_t> offset)
{
  std::ifstream f_in{f_in_str, std::ios::binary};
  std::ofstream f_out{f_out_str, std::ios::binary};

  ereturn_if(not f_in.good() , "Failed to open in file {}\n"_fmt(f_in_str));
  ereturn_if(not f_out.good(), "Failed to open out file {}\n"_fmt(f_out_str));

  // Calculate the size of the data to read
  uint64_t size = offset.second - offset.first;

  // Seek to the start offset in the input file
  f_in.seekg(offset.first, std::ios::beg);

  // Read in chunks
  const size_t size_buf = 4096;
  char buffer[size_buf];

  while( size > 0 )
  {
    std::streamsize read_size = static_cast<std::streamsize>(std::min(static_cast<uint64_t>(size_buf), size));

    f_in.read(buffer, read_size);
    f_out.write(buffer, read_size);

    size -= read_size;
  } // while
} // function: write_from_offset

// }}}

// read_elf_header() {{{
//  Read the current binary's data. The first part of the file is the
//  executable, and the rest is the rw filesystem, therefore, we need to
//  discover where the filesystem stfim (offset) to mount it.

uint64_t read_elf_header(const char* elfFile, uint64_t offset = 0) {
  // Either Elf64_Ehdr or Elf32_Ehdr depending on architecture.
  ElfW(Ehdr) header;

  FILE* file = fopen(elfFile, "rb");
  fseek(file, offset, SEEK_SET);
  if (file)
  {
    // read the header
    fread(&header, sizeof(header), 1, file);
    // check so its really an elf file
    if (std::memcmp(header.e_ident, ELFMAG, SELFMAG) == 0) {
      fclose(file);
      offset = header.e_shoff + (header.e_ehsize * header.e_shnum);
      return offset;
    }
    // finally close the file
    fclose(file);
  } // if

  ns_log::error()("Could not read elf header from {}", elfFile);

  exit(1);
} // }}}

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

  // Create temp dir
  fs::path path_dir_app = path_dir_base / "app";
  ethrow_if(not fs::exists(path_dir_app) and not fs::create_directories(path_dir_app)
    , "Failed to create directory {}"_fmt(path_dir_app)
  );

  // Make the temporary directory name
  fs::path path_dir_temp = path_dir_base / "app" / "{}_{}"_fmt(COMMIT, TIMESTAMP);
  ethrow_if(not fs::create_directories(path_dir_temp)
    , "Failed to create directory {}"_fmt(path_dir_temp)
  );

  // Create bin dir
  fs::path path_dir_temp_bin = path_dir_temp / "bin";
  ethrow_if(not fs::exists(path_dir_temp_bin) and not fs::create_directories(path_dir_temp_bin),
    "Failed to create directory {}"_fmt(path_dir_temp_bin)
  );

  // Set variables
  ns_env::set("FIM_DIR_GLOBAL", path_dir_base.c_str(), ns_env::Replace::Y);
  ns_env::set("FIM_DIR_TEMP_BIN", path_dir_temp_bin.c_str(), ns_env::Replace::Y);
  ns_env::set("FIM_FILE_BINARY", path_absolute.c_str(), ns_env::Replace::Y);
  ns_env::set("FIM_DIR_TEMP", path_dir_temp.c_str(), ns_env::Replace::Y);

  // Create instance directory
  fs::path path_dir_instance_prefix = "{}/{}/"_fmt(path_dir_temp, "instance");

  // Create temp dir to mount filesystems into
  fs::path path_dir_mounts = create_temp_dir(path_dir_instance_prefix);
  ns_env::set("FIM_DIR_MOUNTS", path_dir_mounts.c_str(), ns_env::Replace::Y);

  // Path to ext mount dir is a directory called 'ext'
  fs::path path_dir_mount = path_dir_mounts / "ext";
  ns_env::set("FIM_DIR_MOUNT", path_dir_mount.c_str(), ns_env::Replace::Y);

  /// Create mount directory
  if ( not fs::exists(path_dir_mount) )
  {
    fs::create_directory(path_dir_mount);
  } // if
  ns_log::debug()("Mount directory: {}", path_dir_mount);

  // Starting offsets
  uint64_t offset_beg = 0;
  uint64_t offset_end = read_elf_header(path_absolute.c_str());

  // Write by binary offset
  auto f_write_bin = [&](fs::path path_file, uint64_t offset_end)
  {
    // Update offsets
    offset_beg = offset_end;
    offset_end = read_elf_header(path_absolute.c_str(), offset_beg) + offset_beg;
    // Write binary only if it doesnt already exist
    if ( ! fs::exists(path_file) )
    {
      write_from_offset(path_absolute.string(), path_file, {offset_beg, offset_end});
    }
    // Set permissions
    fs::permissions(path_file.c_str(), fs::perms::owner_all | fs::perms::group_all);
    // Return new values for offsets
    return std::make_pair(offset_beg, offset_end);
  };

  // Write binaries
  auto start = std::chrono::high_resolution_clock::now();
  std::tie(offset_beg, offset_end) = f_write_bin(path_dir_mounts   / "ext.boot" , 0);
  std::tie(offset_beg, offset_end) = f_write_bin(path_dir_temp_bin / "fuse2fs"  , offset_end);
  std::tie(offset_beg, offset_end) = f_write_bin(path_dir_temp_bin / "e2fsck"   , offset_end);
  std::tie(offset_beg, offset_end) = f_write_bin(path_dir_temp_bin / "bash"     , offset_end);
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
  execve("{}/ext.boot"_fmt(path_dir_mounts).c_str(), argv, environ);
} // relocate() }}}

// boot() {{{
void boot(int argc, char** argv)
{
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
  // Set logger level
  if ( ns_env::exists("FIM_DEBUG", "1") )
  {
    ns_log::set_level(ns_log::Level::DEBUG);
  } // if

  // Get path to self
  auto expected_path_file_self = ns_filesystem::ns_path::file_self();
  ereturn_if(not expected_path_file_self, expected_path_file_self.error(), EXIT_FAILURE);

  // If it is outside /tmp, move the binary
  // This function should not reach the return statement due to evecve
  fs::path path_file_self = *expected_path_file_self;
  if (std::distance(path_file_self.begin(), path_file_self.end()) < 2 or *std::next(path_file_self.begin()) != "tmp")
  {
    ns_log::debug()("Relocate program from {}", path_file_self);
    relocate(argv);
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
