///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : mount
///

#pragma once

#include "../subprocess.hpp"

namespace ns_ext2::ns_mount
{

namespace
{

namespace fs = std::filesystem;

enum class Mode
{
  RO,
  RW,
};

// mount() {{{
inline int mount(Mode const& mode, fs::path const& path_file_image, fs::path const& path_dir_mount, uint64_t offset)
{
  // Check if image exists and is a regular file
  ereturn_if(not fs::is_regular_file(path_file_image)
    , "'{}' does not exist or is not a regular file"_fmt(path_file_image)
    , -1
  );

  // Check if mountpoint exists and is directory
  ereturn_if(not fs::is_directory(path_dir_mount)
    , "'{}' does not exist or is not a directory"_fmt(path_dir_mount)
    , -1
  );

  // Find command in PATH
  auto opt_path_file_fuse2fs = ns_subprocess::search_path("fuse2fs");
  ereturn_if(not opt_path_file_fuse2fs.has_value(), "Could not find fuse2fs", 1);

  // Execute command
  auto ret = ns_subprocess::Subprocess(*opt_path_file_fuse2fs)
    .with_piped_outputs()
    .with_args((mode == Mode::RO)? "-oro,fakeroot,offset={}"_fmt(offset) : "-ofakeroot,offset={}"_fmt(offset))
    .with_args(path_file_image, path_dir_mount)
    .spawn()
    .wait();

  if ( not ret.has_value() ) { ret = 1; }

  return *ret;
} // mount() }}}

}

// mount_ro() {{{
inline int mount_ro(fs::path const& path_file_image, fs::path const& path_dir_mount, uint64_t offset)
{
  return mount(Mode::RO, path_file_image, path_dir_mount, offset);
} // mount_ro() }}}

// mount_rw() {{{
inline int mount_rw(fs::path const& path_file_image, fs::path const& path_dir_mount, uint64_t offset)
{
  return mount(Mode::RW, path_file_image, path_dir_mount, offset);
} // mount_rw() }}}

// unmount() {{{
inline int unmount(fs::path const& path_dir_mount)
{
  // Check if mountpoint exists and is directory
  ereturn_if(not fs::is_directory(path_dir_mount)
    , "'{}' does not exist or is not a directory"_fmt(path_dir_mount)
    , -1
  );

  // Find command in PATH
  auto opt_path_file_fuse2fs = ns_subprocess::search_path("fusermount");
  ereturn_if(not opt_path_file_fuse2fs.has_value(), "Could not find fusermount", 1);

  // Execute command
  auto ret = ns_subprocess::Subprocess(*opt_path_file_fuse2fs)
    .with_piped_outputs()
    .with_args("-u", path_dir_mount)
    .spawn()
    .wait();

  if ( not ret.has_value() ) { ret = 1; }

  return *ret;
} // unmount() }}}

} // namespace ns_ext2::ns_mount

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
