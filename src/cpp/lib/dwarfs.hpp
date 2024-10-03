///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : dwarfs
///

#pragma once

#include <filesystem>
#include "log.hpp"
#include "fuse.hpp"
#include "subprocess.hpp"
#include "../macro.hpp"

namespace ns_dwarfs
{

namespace
{

namespace fs = std::filesystem;

};

// class Dwarfs {{{
class Dwarfs
{
  private:
    std::unique_ptr<ns_subprocess::Subprocess> m_subprocess;
    fs::path m_path_dir_mountpoint;

  public:
    Dwarfs(Dwarfs const&) = delete;
    Dwarfs(Dwarfs&&) = delete;
    Dwarfs& operator=(Dwarfs const&) = delete;
    Dwarfs& operator=(Dwarfs&&) = delete;

    Dwarfs(fs::path const& path_file_image, fs::path const& path_dir_mount, uint64_t offset, uint64_t size_image, pid_t pid_to_die_for)
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
      auto opt_path_file_bash = ns_subprocess::search_path("bash");
      ethrow_if(not opt_path_file_bash.has_value(), "Could not find bash");

      // Create command
      m_subprocess = std::make_unique<ns_subprocess::Subprocess>(*opt_path_file_bash);

      // Spawn command
      (void) m_subprocess->with_piped_outputs()
        .with_args("-c")
        .with_args("dwarfs {} {} -f -o auto_unmount,offset={},imagesize={}"_fmt(path_file_image, path_dir_mount, offset, size_image))
        .with_die_on_pid(pid_to_die_for)
        .spawn();
      // Wait for mount
      ns_fuse::wait_fuse(path_dir_mount);
    } // Dwarfs
    
    ~Dwarfs()
    {
      // Un-mount
      ns_fuse::unmount(m_path_dir_mountpoint);
      // Tell process to exit with SIGTERM
      if ( auto opt_pid = m_subprocess->get_pid() )
      {
        kill(*opt_pid, SIGTERM);
      } // if
      // Wait for process to exit
      auto ret = m_subprocess->wait();
      ereturn_if(not ret, "Mount '{}' exited unexpectedly"_fmt(m_path_dir_mountpoint));
      ereturn_if(ret and *ret != 0, "Mount '{}' exited with non-zero exit code '{}'"_fmt(m_path_dir_mountpoint, *ret));
    } // Dwarfs

    fs::path const& get_dir_mountpoint()
    {
      return m_path_dir_mountpoint;
    }
}; // class Dwarfs }}}

} // namespace ns_dwarfs

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
