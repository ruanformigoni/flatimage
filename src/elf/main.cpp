//
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : main.cpp
// @created     : Monday Jan 23, 2023 21:18:26 -03
//
// @description : Main elf for fim
//

#include <algorithm>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <elf.h>
#define FMT_HEADER_ONLY
#include <memory>

#include "../cpp/common.hpp"

#include "boot.h" // boot script
#include "killer.h" // cleanup script

#if defined(__LP64__)
#define ElfW(type) Elf64_ ## type
#else
#define ElfW(type) Elf32_ ## type
#endif

// Git commit hash
#ifndef COMMIT
#define COMMIT "unknown"
#endif

// Compilation timestamp
#ifndef TIMESTAMP
#define TIMESTAMP "unknown"
#endif

// Unix environment variables
extern char** environ;

// Aliases {{{
namespace fs = std::filesystem;
using u64 = unsigned long;
// }}}

// Literals {{{
auto operator"" _err(const char* c_str, std::size_t)
{ return [=](auto&&... args){ print(c_str, args...); exit(1); }; }
// }}}

// fn: create_temp_dir {{{
std::string create_temp_dir(std::string const& prefix)
{
  fs::create_directories(prefix);
  std::string temp_dir_template = prefix + "XXXXXX";
  auto temp_dir_template_cstr = std::unique_ptr<char[]>(new char[temp_dir_template.size() + 1]);
  std::strcpy(temp_dir_template_cstr.get(), temp_dir_template.c_str());
  char* temp_dir_cstr = mkdtemp(temp_dir_template_cstr.get());
  if (temp_dir_cstr == NULL) { "Failed to create temporary dir {}"_err(temp_dir_template_cstr.get()); }
  return std::string{temp_dir_cstr};
} // function: create_temp_dir }}}

// fn: write_from_offset {{{
void write_from_offset(std::string const& f_in_str, std::string const& f_out_str, std::pair<u64,u64> offset)
{
  std::ifstream f_in{f_in_str, std::ios::binary};
  std::ofstream f_out{f_out_str, std::ios::binary};

  if ( ! f_in.good() ) { "Failed to open startup file {}\n"_err(f_in_str); }
  if ( ! f_out.good() ) { "Failed to open target file {}\n"_err(f_out_str); }

  // Calculate the size of the data to read
  u64 size = offset.second - offset.first;

  // Seek to the start offset in the input file
  f_in.seekg(offset.first, std::ios::beg);

  // Read in chunks
  const size_t size_buf = 4096;
  char buffer[size_buf];

  while( size > 0 )
  {
    std::streamsize read_size = static_cast<std::streamsize>(std::min(static_cast<u64>(size_buf), size));

    f_in.read(buffer, read_size);
    f_out.write(buffer, read_size);

    size -= read_size;
  } // while
} // function: write_from_offset

// }}}

// fn: read_elf_header {{{
//  Read the current binary's data. The first part of the file is the
//  executable, and the rest is the rw filesystem, therefore, we need to
//  discover where the filesystem stfim (offset) to mount it.

u64 read_elf_header(const char* elfFile, u64 offset = 0) {
  // Either Elf64_Ehdr or Elf32_Ehdr depending on architecture.
  ElfW(Ehdr) header;

  FILE* file = fopen(elfFile, "rb");
  fseek(file, offset, SEEK_SET);
  if(file)
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
  }
  "Could not read elf header from {}"_err(elfFile); exit(1);
} // }}}

// fn: main {{{
int main(int, char** argv)
{
  // Launch program {{{
  if ( auto str_offset_fs = getenv("FIM_MAIN_LAUNCH") )
  {
    // This part of the code executes the runner, it mounts the image as a
    // readable/writeable filesystem, and then executes the application
    // contained inside of it.

    //
    // Disable FIM_MAIN_LAUNCH in case of backend=host and execute a FIM image
    // from it
    //
    unsetenv("FIM_MAIN_LAUNCH");

    //
    // Get base dir
    //
    char* cstr_dir_base = getenv("FIM_DIR_GLOBAL");
    if ( cstr_dir_base == NULL ) { "FIM_DIR_GLOBAL dir variable is empty"_err(); }

    //
    // Get bin dir
    //
    char* cstr_dir_temp_bin = getenv("FIM_DIR_TEMP_BIN");
    if ( cstr_dir_temp_bin == NULL ) { "FIM_DIR_TEMP_BIN dir variable is empty"_err(); }

    //
    // Create instance dir as $FIM_DIR_TEMP/instance (/tmp/fim/app/xxxx.../instance)
    //
    //// Fetch tempdir location
    char* cstr_dir_temp = getenv("FIM_DIR_TEMP");
    if ( cstr_dir_temp == NULL ) { "Could not open tempdir to mount image\n"_err(); }
    fs::path path_dir_instance_prefix = "{}/{}/"_fmt(cstr_dir_temp, "instance");
    //// Create temp dir to mount filesystems into
    fs::path path_dir_mounts = create_temp_dir(path_dir_instance_prefix);
    //// Path to ext mount dir is a directory called 'ext'
    fs::path path_dir_mount = path_dir_mounts / "ext";
    /// Create dir
    fs::create_directory(path_dir_mount);

    //
    // Set extract path for helper scripts (boot,killer)
    //
    std::string str_boot_file = "{}.boot"_fmt(path_dir_mount.c_str());
    std::string str_killer_file = "{}.killer"_fmt(path_dir_mount.c_str());
    // Change to global directory if debug flag is enabled
    if ( getenv("FIM_DEBUG") )
    {
      str_boot_file = "{}/boot"_fmt(cstr_dir_temp);
      str_killer_file = "{}/killer"_fmt(cstr_dir_temp);
    }
    // Update env
    setenv("FIM_FPATH_BOOT", str_boot_file.c_str(), 1);
    setenv("FIM_FPATH_KILLER", str_killer_file.c_str(), 1);

    auto shbang = "#!{}/bash\n"_fmt(cstr_dir_temp_bin);

    //
    // Set boot script
    //
    if ( ! fs::exists(str_boot_file) )
    {
      std::ofstream script_boot(str_boot_file, std::ios::binary);
      fs::permissions(str_boot_file, fs::perms::all);
      // -1 to exclude the trailing null byte
      script_boot.write(shbang.c_str(), shbang.size());
      script_boot.write(reinterpret_cast<const char*>(_script_boot), sizeof(_script_boot) - 1);
      script_boot.close();
    }

    //
    // Set killer daemon script
    //
    if ( ! fs::exists(str_killer_file) )
    {
      std::ofstream script_killer(str_killer_file, std::ios::binary);
      fs::permissions(str_killer_file, fs::perms::all);
      // -1 to exclude the trailing null byte
      script_killer.write(shbang.c_str(), shbang.size());
      script_killer.write(reinterpret_cast<const char*>(_script_killer), sizeof(_script_killer) - 1);
      script_killer.close();
    }

    //
    // Set environment
    //
    setenv("FIM_DIR_MOUNTS", path_dir_mounts.c_str(), 1);
    setenv("FIM_DIR_MOUNT", path_dir_mount.c_str(), 1);
    setenv("FIM_OFFSET", str_offset_fs, 1);

    //
    // Execute application
    //
    execve(str_boot_file.c_str(), argv, environ);
  } // }}}
  else // Write Runner {{{
  {
    // This part of the code is executed to write the runner,
    // rightafter the code is replaced by the runner.
    // This is done because the current executable cannot mount itself.

    //
    // Get path to called executable
    //
    if (!std::filesystem::exists("/proc/self/exe")) {
      "Error retrieving executable path for self"_err();
    }
    auto path_absolute = fs::read_symlink("/proc/self/exe");

    //
    // Create base dir
    //
    std::string str_dir_base = "/tmp/fim";

    if ( ! fs::exists(str_dir_base) && ! fs::create_directories(str_dir_base) )
    {
      "Failed to create directory {}"_err(str_dir_base);
    }

    //
    // Create temp dir
    //
    std::string str_dir_app = str_dir_base + "/app/";
    if ( ! fs::exists(str_dir_app) && ! fs::create_directories(str_dir_app) )
    {
      "Failed to create directory {}"_err(str_dir_app);
    }

    // Get some metadata for uniqueness
    // // Creation timestamp
    struct stat st;
    if (stat(path_absolute.c_str(), &st) == -1)
    {
      perror("stat");
      "Failed to retrieve size of self '{}'"_err(path_absolute.c_str());
    }
    // // Stitch all to make the temporary directory name
    std::string str_dir_temp = str_dir_base + "/app/{}_{}"_fmt(COMMIT, TIMESTAMP);
    fs::create_directories(str_dir_temp);

    //
    // Create bin dir
    //
    std::string str_dir_temp_bin = str_dir_temp + "/bin/";
    if ( ! fs::exists(str_dir_temp_bin) && ! fs::create_directories(str_dir_temp_bin) )
    {
      "Failed to create directory {}"_err(str_dir_temp_bin);
    }

    //
    // Starting offsets
    //
    u64 offset_beg = 0;
    u64 offset_end = read_elf_header(path_absolute.c_str());

    //
    // Write by binary offset
    //
    auto f_write_bin = [&](std::string str_dir, std::string str_bin, u64 offset_end)
    {
      // Update offsets
      offset_beg = offset_end;
      offset_end = read_elf_header(path_absolute.c_str(), offset_beg) + offset_beg;
      // Create output path to write binary into
      std::string str_file = "{}/{}"_fmt(str_dir, str_bin);
      // Write binary only if it doesnt already exist
      if ( ! fs::exists(str_file) )
      {
        write_from_offset(path_absolute.string(), str_file, {offset_beg, offset_end});
      }
      // Set permissions
      fs::permissions(str_file.c_str(), fs::perms::owner_all | fs::perms::group_all);
      // Return new values for offsets
      return std::make_pair(offset_beg, offset_end);
    };

    //
    // Write binaries
    //
    auto start = std::chrono::high_resolution_clock::now();
    std::tie(offset_beg, offset_end) = f_write_bin(str_dir_temp, "main", 0);
    std::tie(offset_beg, offset_end) = f_write_bin(str_dir_temp_bin, "fuse2fs", offset_end);
    std::tie(offset_beg, offset_end) = f_write_bin(str_dir_temp_bin, "e2fsck", offset_end);
    std::tie(offset_beg, offset_end) = f_write_bin(str_dir_temp_bin, "bash", offset_end);
    auto end = std::chrono::high_resolution_clock::now();

    //
    // Option to show offset and exit
    //
    if( getenv("FIM_MAIN_OFFSET") ){ "{}"_print(offset_end); exit(0); }

    // Print copy duration
    if ( getenv("FIM_DEBUG") != nullptr )
    {
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
      "Copy binaries finished in '{}' ms"_print(elapsed.count());
    } // if

    //
    // Set env variables to execve
    //
    setenv("FIM_MAIN_LAUNCH", std::to_string(offset_end).c_str(), 1);
    setenv("FIM_DIR_GLOBAL", str_dir_base.c_str(), 1);
    setenv("FIM_DIR_TEMP_BIN", str_dir_temp_bin.c_str(), 1);
    setenv("FIM_FILE_BINARY", path_absolute.c_str(), 1);
    setenv("FIM_DIR_TEMP", str_dir_temp.c_str(), 1);

    //
    // Launch Runner
    //
    execve("{}/main"_fmt(str_dir_temp).c_str(), argv, environ);
  }
  // }}}

  return EXIT_SUCCESS;
} // }}}

// cmd: !mkdir -p build && cd build && conan install .. --build=missing && cd .. || cd ..
// cmd: !cmake -H. -Bbuild -DCMAKE_EXPORT_COMPILE_COMMANDS=1
// cmd: !cmake --build build

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=80 et :*/
