///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : reserved
///

#pragma once

#include <expected>
#include <filesystem>
#include <cstdint>
#include <fstream>
#include "../common.hpp"
#include "../macro.hpp"

namespace ns_reserved
{

namespace
{
  
namespace fs = std::filesystem;

} // namespace ns_reserved

// write() {{{
// Writes data to file in binary format
// Returns space left after write
inline std::error<std::string> write(fs::path const& path_file_binary
  , uint64_t offset
  , uint64_t size
  , const char* data
  , uint64_t length)
{
  qreturn_if(length > size, std::make_optional("Size of data exceeds available space"));

  std::ofstream file_binary(path_file_binary, std::ios::binary | std::ios::in | std::ios::out);
  qreturn_if(not file_binary.is_open(), std::make_optional("Failed to open input file"));

  file_binary.seekp(offset);

  file_binary.write(data, length);

  file_binary.close();

  return std::nullopt;
} // write() }}}

// read() {{{
// Reads data from a file in binary format
// Returns a std::string built with the data
inline std::expected<uint64_t,std::string> read(fs::path const& path_file_binary
  , uint64_t offset
  , uint64_t length
  , char* data)
{
  // Open binary file
  std::ifstream file_binary(path_file_binary, std::ios::binary | std::ios::in);
  qreturn_if(not file_binary.is_open(), std::unexpected("Failed to open input file"));
  // Advance towards data
  file_binary.seekg(offset);
  // Read data
  file_binary.read(data, length);
  // Check how many bytes were actually read
  std::streamsize bytes_read = file_binary.gcount();
  file_binary.close();
  // Return number of read bytes
  return bytes_read;
} // read() }}}

} // namespace 

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
