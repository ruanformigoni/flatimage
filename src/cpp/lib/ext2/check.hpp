///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : check
///

#pragma once

#include "../subprocess.hpp"

#include "../../std/string.hpp"

namespace ns_ext2::ns_check
{

namespace
{

namespace fs = std::filesystem;

} // namespace

// check() {{{
inline int check(fs::path const& path_file_image, uint64_t offset)
{
  // Check if image exists and is a regular file
  ereturn_if(not fs::is_regular_file(path_file_image)
    , "'{}' does not exist or is not a regular file"_fmt(path_file_image)
    , -1
  );

  // Find command in PATH
  auto opt_path_file_e2fsck = ns_subprocess::search_path("e2fsck");
  ereturn_if(not opt_path_file_e2fsck.has_value(), "Could not find e2fsck", 1);

  // Execute command
  auto ret = ns_subprocess::Subprocess(*opt_path_file_e2fsck)
    .with_piped_outputs()
    .with_args("-y")
    .with_args(path_file_image.string() + "?offset=" + ns_string::to_string(offset))
    .spawn();

  if ( not ret.has_value() ) { ret = 1; }

  return *ret;
} // check() }}}

// check_force() {{{
inline int check_force(fs::path const& path_file_image, uint64_t offset)
{
  // Check if image exists and is a regular file
  ereturn_if(not fs::is_regular_file(path_file_image)
    , "'{}' does not exist or is not a regular file"_fmt(path_file_image)
    , -1
  );

  // Find command in PATH
  auto opt_path_file_e2fsck = ns_subprocess::search_path("e2fsck");
  ereturn_if(not opt_path_file_e2fsck.has_value(), "Could not find e2fsck", 1);

  // Execute command
  auto ret = ns_subprocess::Subprocess(*opt_path_file_e2fsck)
    .with_piped_outputs()
    .with_args("-fy")
    .with_args(path_file_image.string() + "?offset=" + ns_string::to_string(offset))
    .spawn();

  if ( not ret.has_value() ) { ret = 1; }

  return *ret;
} // check_force() }}}

} // namespace ns_ext2::ns_check

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
