///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : mount
///

#pragma once

#include "../subprocess.hpp"
#include "../../std/enum.hpp"
#include "../../lib/fuse.hpp"

namespace ns_ext2::ns_mount
{

ENUM(Mode, RO, RW);

namespace
{

namespace fs = std::filesystem;

}

// class Mount {{{
class Mount
{
  private:
    Mode m_mode;
    fs::path m_path_dir_mountpoint;

  public:
    Mount(Mount const&) = delete;
    Mount(Mount&&) = delete;
    Mount& operator=(Mount const&) = delete;
    Mount& operator=(Mount&&) = delete;
    Mount(fs::path const& path_file_image, fs::path path_dir_mount, Mode mode, uint64_t offset)
      : m_mode(mode)
      , m_path_dir_mountpoint(path_dir_mount)
    {
      // Check if image exists and is a regular file
      ethrow_if(not fs::is_regular_file(path_file_image)
        , "'{}' does not exist or is not a regular file"_fmt(path_file_image)
      );

      // Check if mountpoint exists and is directory
      ethrow_if(not fs::is_directory(path_dir_mount)
        , "'{}' does not exist or is not a directory"_fmt(path_dir_mount)
      );

      // Find command in PATH
      auto opt_path_file_fuse2fs = ns_subprocess::search_path("fuse2fs");
      ethrow_if(not opt_path_file_fuse2fs.has_value(), "Could not find fuse2fs");

      // Execute command
      auto ret = ns_subprocess::Subprocess(*opt_path_file_fuse2fs)
        .with_piped_outputs()
        .with_args((mode == Mode::RO)? "-oro,fakeroot,offset={}"_fmt(offset) : "-ofakeroot,offset={}"_fmt(offset))
        .with_args(path_file_image, path_dir_mount)
        .spawn()
        .wait();

      ethrow_if(not ret.has_value(), "Ext mount process exited abnormally");
      ethrow_if(*ret != 0, "Ext mount process exited with non-zero exit code");
    } // Mount

    ~Mount()
    {
      ns_fuse::unmount(m_path_dir_mountpoint);
    } // Mount
}; // class: Mount }}}

} // namespace ns_ext2::ns_mount

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
