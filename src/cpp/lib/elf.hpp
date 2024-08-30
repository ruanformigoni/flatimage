///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : elf
///

#pragma once

#include <string>
#include <cstdint>
#include <fstream>
#include <elf.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "log.hpp"

#include "../macro.hpp"
#include "../common.hpp"

namespace ns_elf
{

namespace fs = std::filesystem;

#if defined(__LP64__)
#define ElfW(type) Elf64_ ## type
#else
#define ElfW(type) Elf32_ ## type
#endif

// copy_binary() {{{
// Copies the binary data between [offset.first, offset.second] from path_file_input to path_file_output
inline void copy_binary(fs::path const& path_file_input, fs::path const& path_file_output, std::pair<uint64_t,uint64_t> offset)
{
  std::ifstream f_in{path_file_input, std::ios::binary};
  std::ofstream f_out{path_file_output, std::ios::binary};

  ereturn_if(not f_in.good() , "Failed to open in file {}\n"_fmt(path_file_input));
  ereturn_if(not f_out.good(), "Failed to open out file {}\n"_fmt(path_file_output));

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
} // function: copy_binary

// }}}

// skip_elf_header() {{{
// Skips the elf header starting from 'offset' and returns the offset to the first byte afterwards
inline uint64_t skip_elf_header(fs::path const& path_file_elf, uint64_t offset = 0) {
  // Either Elf64_Ehdr or Elf32_Ehdr depending on architecture.
  ElfW(Ehdr) header;

  FILE* file = fopen(path_file_elf.c_str(), "rb");
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

  ns_log::error()("Could not read elf header from {}", path_file_elf);

  exit(1);
} // }}}

} // namespace ns_elf

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
