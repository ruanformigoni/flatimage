///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : linux
///

#pragma once

#include <cstring>
#include <string>
#include <filesystem>
#include <unistd.h>

#include "log.hpp"
#include "../common.hpp"
#include "../macro.hpp"

namespace ns_linux
{

namespace
{

namespace fs = std::filesystem;

}

// mkdtemp() {{{
// Creates a temporary directory in path_dir_parent with the template provided by 'dir_template'
[[nodiscard]] inline fs::path mkdtemp(fs::path const& path_dir_parent, std::string dir_template = "XXXXXX")
{
  std::error_code ec;
  fs::create_directories(path_dir_parent, ec);
  ethrow_if(ec, "Failed to create temporary dir {}: '{}'"_fmt(path_dir_parent, ec.message()));
  fs::path path_dir_template = path_dir_parent / dir_template;
  const char* cstr_path_dir_temp = ::mkdtemp(path_dir_template.string().data()); 
  ethrow_if(cstr_path_dir_temp == nullptr, "Failed to create temporary dir {}"_fmt(path_dir_template));
  return cstr_path_dir_temp;
} // function: mkdtemp }}}

// mkstemp() {{{
[[nodiscard]] inline std::expected<fs::path, std::string> mkstemps(fs::path const& path_dir_parent
  , std::string file_template = "XXXXXX"
  , int suffixlen = 0
)
{
  std::string str_template = path_dir_parent / file_template;
  int fd = ::mkstemps(str_template.data(), suffixlen);
  qreturn_if(fd < 0, std::unexpected(strerror(errno)));
  close(fd);
  return fs::path{str_template};
}
// mkstemp() }}}

} // namespace ns_linux

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
