///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : squashfs
///

#pragma once

#include <filesystem>
#include "log.hpp"
#include "fuse.hpp"
#include "subprocess.hpp"
#include "../macro.hpp"

namespace ns_squashfs
{

namespace
{

namespace fs = std::filesystem;

};

// class SquashFs {{{
class SquashFs
{
  private:
    std::unique_ptr<ns_subprocess::Subprocess> m_subprocess;
    fs::path m_path_dir_mountpoint;

  public:
    SquashFs(SquashFs const&) = delete;
    SquashFs(SquashFs&&) = delete;
    SquashFs& operator=(SquashFs const&) = delete;
    SquashFs& operator=(SquashFs&&) = delete;

    SquashFs(fs::path const& path_file_image, fs::path const& path_dir_mount, uint64_t offset)
      : m_path_dir_mountpoint(path_dir_mount)
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
      auto opt_path_file_squashfs = ns_subprocess::search_path("squashfuse");
      ethrow_if(not opt_path_file_squashfs.has_value(), "Could not find squashfs");

      // Create command
      m_subprocess = std::make_unique<ns_subprocess::Subprocess>(*opt_path_file_squashfs);

      // Spawn command
      auto ret = m_subprocess->with_piped_outputs()
        .with_args("-o", "offset={}"_fmt(offset))
        .with_args(path_file_image, path_dir_mount)
        .spawn()
        .wait();
      ereturn_if(not ret, "Mount '{}' exited unexpectedly"_fmt(m_path_dir_mountpoint));
      ereturn_if(ret and *ret != 0, "Mount '{}' exited with non-zero exit code '{}'"_fmt(m_path_dir_mountpoint, *ret));
    } // SquashFs
    
    ~SquashFs()
    {
      ns_fuse::unmount(m_path_dir_mountpoint);
    } // SquashFs

    fs::path const& get_dir_mountpoint()
    {
      return m_path_dir_mountpoint;
    }
}; // class SquashFs }}}

} // namespace ns_squashfs

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
