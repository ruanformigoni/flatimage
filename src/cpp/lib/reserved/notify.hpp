///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : notify
///

#pragma once

#include <string>
#include <filesystem>
#include "../reserved.hpp"
#include "../../macro.hpp"

namespace ns_reserved::ns_notify
{

namespace
{

namespace fs = std::filesystem;

}

// write() {{{
inline std::error<std::string> write(fs::path const& path_file_binary
  , uint64_t offset
  , uint64_t size
  , char value
)
{
  qreturn_if(size < 1, "Not enough space to fit json data");
  return ns_reserved::write(path_file_binary, offset, size, &value, sizeof(char));
} // write() }}}

// read() {{{
inline std::expected<char,std::string> read(fs::path const& path_file_binary, uint64_t offset)
{
  char buffer;
  auto expected_read = ns_reserved::read(path_file_binary, offset, sizeof(buffer), &buffer);
  qreturn_if(not expected_read, std::unexpected(expected_read.error()));
  return buffer;
} // read() }}}

} // namespace ns_reserved::ns_notify

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
