///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : desktop
///

#pragma once

#include <string>
#include <filesystem>
#include "../reserved.hpp"
#include "../../macro.hpp"

namespace ns_reserved::ns_desktop
{

namespace
{

namespace fs = std::filesystem;

}

// write() {{{
inline std::error<std::string> write(fs::path const& path_file_binary
  , uint64_t offset
  , uint64_t size
  , const char* raw_json
  , uint64_t length
)
{
  qreturn_if(length > size, "Not enough space to fit json data");
  return ns_reserved::write(path_file_binary, offset, size, raw_json, length);
} // write() }}}

// read() {{{
inline std::expected<std::unique_ptr<char[]>,std::string> read(fs::path const& path_file_binary
  , uint64_t offset
  , uint64_t size)
{
  std::string raw_json;
  auto buffer = std::make_unique<char[]>(size);
  auto expected_read = ns_reserved::read(path_file_binary, offset, size, buffer.get());
  qreturn_if(not expected_read, std::unexpected(expected_read.error()));
  return buffer;
} // read() }}}

} // namespace ns_reserved::ns_desktop

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
