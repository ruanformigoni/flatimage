///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : mount
///

#pragma once

#include "../../std/enum.hpp"
#include "../subprocess.hpp"

namespace ns_ext2::ns_mount
{

ENUM(Mode, RO, RW);

namespace
{

namespace fs = std::filesystem;

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

}

// class Mount {{{
class Mount
{
  private:
    Mode m_mode;
    fs::path m_path_dir_mount;

  public:
    Mount(Mount const&) = delete;
    Mount(Mount&&) = delete;
    Mount& operator=(Mount const&) = delete;
    Mount& operator=(Mount&&) = delete;
    Mount(fs::path const& path_file_image, fs::path path_dir_mount, Mode mode, uint64_t offset)
      : m_mode(mode)
      , m_path_dir_mount(path_dir_mount)
    {
      mount(m_mode, path_file_image, path_dir_mount, offset);
    } // Mount

    ~Mount()
    {
      unmount(m_path_dir_mount);
    } // Mount
}; // class: Mount }}}

} // namespace ns_ext2::ns_mount

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
