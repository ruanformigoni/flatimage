///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : overlayfs
///

#pragma once

#include <filesystem>
#include <unistd.h>

#include "subprocess.hpp"
#include "fuse.hpp"

namespace
{

namespace fs = std::filesystem;

} // namespace

namespace ns_overlayfs
{

class Overlayfs
{
  private:
    std::unique_ptr<ns_subprocess::Subprocess> m_subprocess;
    fs::path m_path_dir_mountpoint;

  public:
    Overlayfs(fs::path const& path_dir_layers
        , fs::path const& path_dir_upperdir
        , fs::path const& path_dir_mountpoint
        , fs::path const& path_dir_workdir
        , pid_t pid_to_die_for
      )
      : m_path_dir_mountpoint(path_dir_mountpoint)
    {
      ethrow_if (not fs::exists(path_dir_layers), "Layers directory does not exist");

      std::vector<fs::path> vec_path_dir_lowerdir;
      for(auto&& path_dir_lowerdir : fs::directory_iterator(path_dir_layers))
      {
        vec_path_dir_lowerdir.push_back(path_dir_lowerdir);
      } // for
      std::ranges::sort(vec_path_dir_lowerdir);

      ethrow_if (not fs::exists(path_dir_upperdir) and not fs::create_directories(path_dir_upperdir)
        , "Could not create modifications dir for overlayfs"
      );

      ethrow_if (not fs::exists(path_dir_mountpoint) and not fs::create_directories(path_dir_mountpoint)
        , "Could not create mountpoint for overlayfs"
      );

      // Find overlayfs
      auto opt_path_file_overlayfs = ns_subprocess::search_path("overlayfs");
      ethrow_if (not opt_path_file_overlayfs, "Could not find 'overlayfs' in PATH");

      // Create subprocess
      m_subprocess = std::make_unique<ns_subprocess::Subprocess>(*opt_path_file_overlayfs);

      // Get user and group ids
      uid_t user_id = getuid();
      gid_t group_id = getgid();

      // Create string to represent argument of lowerdirs
      std::string arg_lowerdir="lowerdir={}"_fmt(vec_path_dir_lowerdir.back());
      for (auto&& path_dir_lowerdir : std::ranges::subrange(vec_path_dir_lowerdir.rbegin()+1, vec_path_dir_lowerdir.rend()))
      {
        arg_lowerdir += ":{}"_fmt(path_dir_lowerdir);
      } // for

      // Include arguments and spawn process
      std::ignore = m_subprocess->
         with_args("-f")
        .with_args("-o", "squash_to_uid={}"_fmt(user_id))
        .with_args("-o", "squash_to_gid={}"_fmt(group_id))
        .with_args("-o", arg_lowerdir)
        .with_args("-o", "upperdir={}"_fmt(path_dir_upperdir))
        .with_args("-o", "workdir={}"_fmt(path_dir_workdir))
        .with_args(m_path_dir_mountpoint)
        .with_die_on_pid(pid_to_die_for)
        .spawn();
      // Wait for mount
      ns_fuse::wait_fuse(path_dir_mountpoint);
    } // Overlayfs

    ~Overlayfs()
    {
      ns_fuse::unmount(m_path_dir_mountpoint);
      // Tell process to exit with SIGTERM
      if ( auto opt_pid = m_subprocess->get_pid() )
      {
        kill(*opt_pid, SIGTERM);
      } // if
      // Wait for process to exit
      auto ret = m_subprocess->wait();
      dreturn_if(not ret, "Mount '{}' exited unexpectedly"_fmt(m_path_dir_mountpoint));
      dreturn_if(ret and *ret != 0, "Mount '{}' exited with non-zero exit code '{}'"_fmt(m_path_dir_mountpoint, *ret));
    } // ~Overlayfs
}; // class: Overlayfs



} // namespace ns_overlayfs

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
