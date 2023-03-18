//
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : main.cpp
// @created     : Monday Jan 23, 2023 21:18:26 -03
//
// @description : Main elf for arts
//

#include <deque>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <iterator>
#include <filesystem>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <optional>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <elf.h>
#include <fmt/ranges.h>

#if defined(__LP64__)
#define ElfW(type) Elf64_ ## type
#else
#define ElfW(type) Elf32_ ## type
#endif

// Unix environment variables
extern char** environ;

// Aliases {{{
namespace fs = std::filesystem;
using u64 = unsigned long;
// }}}

// Literals {{{
auto operator"" _fmt(const char* c_str, std::size_t)
{ return [=](auto&&... args){ return fmt::format(fmt::runtime(c_str), args...); }; }

auto operator"" _err(const char* c_str, std::size_t)
{ return [=](auto&&... args){ fmt::print(fmt::runtime(c_str), args...); exit(1); }; }
// }}}

// fn: create_temp_dir {{{
std::string create_temp_dir()
{
  // Create temp dir
  char dir_template[] = "/tmp/tmpdir.XXXXXX";
  char *dir_tmp = mkdtemp(dir_template);
  if (dir_tmp == NULL) { "Failed to create temporary dir"_err(); }
  return dir_tmp;
} // function: create_temp_dir }}}

// fn: write_from_offset {{{
void write_from_offset(std::string const& f_in_str, std::string const& f_out_str, std::pair<u64,u64> offset)
{
  std::ifstream f_in{f_in_str, f_in.binary | f_in.in};
  std::ofstream f_ou{f_out_str, f_ou.binary | f_ou.out};

  if ( ! f_in.good() or ! f_ou.good() ) { "Failed to open startup files\n"_err(); }

  f_in.seekg(offset.second);
  auto end = f_in.tellg();
  f_in.seekg(offset.first);

  for(char ch; f_in.tellg() != end;)
  {
    f_in.get(ch);
    f_ou.write(&ch, sizeof(char));
  }

  f_ou.close(); f_in.close();
} // function: write_from_offset

// }}}

// fn: read_elf_header {{{
//  Read the current binary's data. The first part of the file is the
//  executable, and the rest is the rw filesystem, therefore, we need to
//  discover where the filesystem starts (offset) to mount it.

u64 read_elf_header(const char* elfFile, u64 offset = 0) {
  // Either Elf64_Ehdr or Elf32_Ehdr depending on architecture.
  ElfW(Ehdr) header;

  FILE* file = fopen(elfFile, "rb");
  fseek(file, offset, SEEK_SET);
  if(file) {
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
int main(int argc, char** argv)
{

  // Get caller path and choose branch to execute {{{

  // Get arguments from 1..n-1
  std::deque<std::string> deque_args (argv, argv+argc);
  deque_args.pop_front();
  std::for_each(deque_args.begin(), deque_args.end(), [](auto& e){ e.push_back(' '); });
  std::string str_args;
  std::for_each(deque_args.begin(), deque_args.end(), [&](auto&& e){ str_args.append(e); });

  // /home/user/../dir/main
  auto path_absolute = fs::canonical(fs::path{argv[0]});

  // }}}

  // Launch program {{{
  if ( auto str_offset_fs = getenv("ARTS_LAUNCH") )
  {
    // This part of the code executes the runner, it mounts the image as a
    // readable/writeable filesystem, and then executes the application
    // contained inside of it.

    //
    // Get bin dir
    //
    char cstr_dir_bin[] = "/tmp/arts";

    //
    // Get temp dir
    //
    char* cstr_dir_temp = getenv("ARTS_TEMP");
    if ( cstr_dir_temp == NULL ) { "Could not open tempdir to mount image\n"_err(); }
    std::string str_dir_temp{cstr_dir_temp};
    std::string str_dir_mount{"{}/{}"_fmt(cstr_dir_temp,"mount")};
    fs::create_directory(str_dir_mount);

    //
    // Get offset
    //
    int offset {std::stoi(str_offset_fs)};

    // Option to show offset and exit
    if( getenv("ARTS_OFFSET") ){ fmt::print("{}\n", offset); exit(0); }

    //
    // Extract boot script
    //
    if(system("{}/ext2rd -o{} {} ./arts/boot:{}/boot"_fmt(cstr_dir_bin, offset, path_absolute.c_str(), cstr_dir_temp).c_str()) != 0)
    {
      "Could not extract arts boot script from {} to {}"_err(path_absolute.c_str(), cstr_dir_temp);
    }

    //
    // Set environment
    //
    setenv("ARTS_BIN", cstr_dir_bin, 1);
    setenv("ARTS_MOUNT", str_dir_mount.c_str(), 1);
    setenv("ARTS_OFFSET", str_offset_fs, 1);
    setenv("ARTS_TEMP", str_dir_temp.c_str(), 1);
    setenv("ARTS_FILE", path_absolute.c_str(), 1);

    //
    // Execute application
    //
    system("{}/boot {}"_fmt(str_dir_temp, str_args).c_str());
  } // }}}
  
  else // Write Runner {{{
  {
    // This part of the code is executed to write the runner,
    // rightafter the code is replaced by the runner.
    // This is done because the current executable cannot mount itself.

    //
    // Create binary dir
    //
    std::string str_dir_bin = "/tmp/arts";
    fs::create_directory(str_dir_bin);

    //
    // Create temp dir
    //
    std::string str_dir_temp = create_temp_dir();

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
    std::tie(offset_beg, offset_end) = f_write_bin(str_dir_temp, "runner", 0);
    std::tie(offset_beg, offset_end) = f_write_bin(str_dir_bin, "proot", offset_end);
    std::tie(offset_beg, offset_end) = f_write_bin(str_dir_bin, "ext2rd", offset_end);
    std::tie(offset_beg, offset_end) = f_write_bin(str_dir_bin, "fuse2fs", offset_end);
    std::tie(offset_beg, offset_end) = f_write_bin(str_dir_bin, "dwarfs", offset_end);
    std::tie(offset_beg, offset_end) = f_write_bin(str_dir_bin, "mkdwarfs", offset_end);

    //
    // Launch Runner
    //
    setenv("ARTS_LAUNCH", std::to_string(offset_end).c_str(), 1);
    setenv("ARTS_TEMP", str_dir_temp.c_str(), 1);
    execve("{}/runner"_fmt(str_dir_temp).c_str(), argv, environ);
  }
  // }}}

  return EXIT_SUCCESS;
} // }}}

// cmd: !mkdir -p build && cd build && conan install .. --build=missing && cd .. || cd ..
// cmd: !cmake -H. -Bbuild -DCMAKE_EXPORT_COMPILE_COMMANDS=1
// cmd: !cmake --build build

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=80 et :*/
