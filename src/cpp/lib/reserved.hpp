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

// Writes data to file in binary format
// Returns space left after write
inline std::optional<std::string> write(fs::path const& path_file_binary
  , uint64_t begin
  , uint64_t end
  , const char* data
  , uint64_t length)
{
  qreturn_if(length > (end - begin), std::make_optional("Size of data exceeds available space"));

  std::ofstream file_binary(path_file_binary, std::ios::binary | std::ios::in | std::ios::out);
  qreturn_if(not file_binary.is_open(), std::make_optional("Failed to open input file"));

  file_binary.seekp(begin);

  file_binary.write(data, length);

  file_binary.close();

  return std::nullopt;
}

// Reads data from a file in binary format
// Returns a std::string built with the data
inline std::optional<std::string> read(fs::path const& path_file_binary
  , uint64_t begin
  , uint64_t length
  , char* data)
{
  std::ifstream file_binary(path_file_binary, std::ios::binary | std::ios::in);

  qreturn_if(not file_binary.is_open(), std::make_optional("Failed to open input file"));

  file_binary.seekg(begin);

  file_binary.read(data, length);

  file_binary.close();

  return std::nullopt;
}

} // namespace 

} // namespace ns_reserved

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
