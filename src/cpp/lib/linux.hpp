///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : linux
///

#pragma once

#include <cstring>
#include <string>
#include <filesystem>

#include "log.hpp"
#include "../common.hpp"
#include "../macro.hpp"

namespace ns_linux
{

namespace fs = std::filesystem;

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

} // namespace ns_linux

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
