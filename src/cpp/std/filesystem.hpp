///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : path
///

#pragma once

#include <filesystem>
#include <numeric>

#define nsel_CONFIG_SELECT_EXPECTED nsel_EXPECTED_NONSTD
#include <nonstd/expected.hpp>

#include "../common.hpp"

namespace ns_filesystem
{

namespace fs = std::filesystem;

// namespace ns_path {{{
namespace ns_path
{

// canonical() {{{
// Try to make path canonical
inline nonstd::expected<fs::path, std::string> canonical(fs::path const& path)
{
  fs::path ret{path};

  // Adjust for relative path
  if (not ret.string().starts_with("/"))
  {
    ret = fs::path(fs::path{"./"} / ret);
  } // if

  // Make path cannonical
  std::error_code ec;
  ret = fs::canonical(ret, ec);
  if ( ec )
  {
    return nonstd::unexpected_type("Could not make cannonical path for parent of '{}'"_fmt(path));
  } // if

  return ret;
} // function: canonical }}}

// file_self() {{{
inline nonstd::expected<fs::path,std::string> file_self()
{
  std::error_code ec;

  auto path_file_self = fs::read_symlink("/proc/self/exe", ec);

  if ( ec )
  {
    return nonstd::unexpected_type("Failed to fetch location of self");
  } // if

  return path_file_self;
} // file_self() }}}

// dir_self() {{{
inline nonstd::expected<fs::path,std::string> dir_self()
{
  auto expected_path_file_self = file_self();

  if ( not expected_path_file_self )
  {
    return nonstd::unexpected_type(expected_path_file_self.error());
  } // if

  return expected_path_file_self->parent_path();
} // dir_self() }}}


} // namespace ns_path }}}

} // namespace ns_filesystem

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
