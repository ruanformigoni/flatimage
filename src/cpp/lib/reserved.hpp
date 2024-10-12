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
[[nodiscard]] inline std::error<std::string> write(fs::path const& path_file_binary
  , uint64_t offset
  , uint64_t size
  , const char* data
  , uint64_t length)
{
  qreturn_if(length > size, std::make_optional("Size of data exceeds available space"));
  // Open output binary file
  std::ofstream file_binary(path_file_binary, std::ios::binary | std::ios::in | std::ios::out);
  qreturn_if(not file_binary.is_open(), std::make_optional("Failed to open input file"));
  // Write blank data
  std::vector<char> blank(size, 0);
  qreturn_if(not file_binary.seekp(offset), std::make_optional("Failed to seek offset to blank"));
  qreturn_if(not file_binary.write(blank.data(), size), std::make_optional("Failed to write blank data"));
  // Write data with length
  qreturn_if(not file_binary.seekp(offset), std::make_optional("Failed to seek offset to write data"));
  qreturn_if(not file_binary.write(data, length), std::make_optional("Failed to write data"));
  // Success
  return std::nullopt;
} // write() }}}

// read() {{{
// Reads data from a file in binary format
// Returns a std::string built with the data
[[nodiscard]] inline std::expected<uint64_t,std::string> read(fs::path const& path_file_binary
  , uint64_t offset
  , uint64_t length
  , char* data)
{
  // Open binary file
  std::ifstream file_binary(path_file_binary, std::ios::binary | std::ios::in);
  qreturn_if(not file_binary.is_open(), std::unexpected("Failed to open input file"));
  // Advance towards data
  qreturn_if(not file_binary.seekg(offset), std::unexpected("Failed to seek to file offset for read"));
  // Read data
  qreturn_if(not file_binary.read(data, length), std::unexpected("Failed to read data from binary file"));
  // Check how many bytes were actually read
  std::streamsize bytes_read = file_binary.gcount();
  // Return number of read bytes
  return bytes_read;
} // read() }}}

} // namespace 

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
