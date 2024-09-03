///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : overlayfs
///

#pragma once

#include <filesystem>
#include <unistd.h>

#include "subprocess.hpp"

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
    Overlayfs(std::vector<fs::path> const& vec_path_dir_lowerdir
        , fs::path const& path_dir_modifications
        , fs::path const& path_dir_mountpoint
      )
      : m_path_dir_mountpoint(path_dir_mountpoint)
    {
      ethrow_if(vec_path_dir_lowerdir.empty(), "Empty lowerdirs vector");

      for(auto&& path_dir_lowerdir : vec_path_dir_lowerdir)
      {
        ethrow_if (not fs::exists(path_dir_lowerdir)
          , "Lowerdir does not exist for overlayfs mount"
        );
      } // for

      ethrow_if (not fs::exists(path_dir_modifications) and not fs::create_directories(path_dir_modifications)
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

      // Create modifications directory and work directory
      fs::path path_dir_upperdir = path_dir_modifications / "upperdir";
      fs::path path_dir_workdir = path_dir_modifications / "workdir";
      ethrow_if (not fs::exists(path_dir_upperdir) and not fs::create_directories(path_dir_upperdir)
        , "Could not create upperdir for overlayfs"
      );
      ethrow_if (not fs::exists(path_dir_workdir) and not fs::create_directories(path_dir_workdir)
        , "Could not create workdir for overlayfs"
      );

      // Create string to represent argument of lowerdirs
      std::string arg_lowerdir="lowerdir={}"_fmt(vec_path_dir_lowerdir.back());
      for (auto&& path_dir_lowerdir : std::ranges::subrange(vec_path_dir_lowerdir.rbegin()+1, vec_path_dir_lowerdir.rend()))
      {
        arg_lowerdir += ":{}"_fmt(path_dir_lowerdir);
      } // for


      // Include arguments and spawn process
      (void) m_subprocess->
         with_args("-o", "squash_to_uid={}"_fmt(user_id))
        .with_args("-o", "squash_to_gid={}"_fmt(group_id))
        .with_args("-o", arg_lowerdir)
        .with_args("-o", "upperdir={}"_fmt(path_dir_upperdir))
        .with_args("-o", "workdir={}"_fmt(path_dir_workdir))
        .with_args(m_path_dir_mountpoint)
        .spawn()
        .wait();
    } // Overlayfs

    ~Overlayfs()
    {
      // Find fusermount
      auto opt_path_file_fusermount = ns_subprocess::search_path("fusermount");
      ereturn_if (not opt_path_file_fusermount, "Could not find 'fusermount' in PATH");

      // Filesystem could be busy for a bit after un-mount of dwarfs
      using namespace std::chrono_literals;
      for(int i{0}; i < 10; ++i)
      {
        auto ret = ns_subprocess::Subprocess(*opt_path_file_fusermount)
          .with_piped_outputs()
          .with_args("-zu", m_path_dir_mountpoint)
          .spawn()
          .wait();
        dbreak_if(ret and *ret == 0, "Un-mounted filesystem '{}'"_fmt(*opt_path_file_fusermount));
        std::this_thread::sleep_for(100ms);
      } // if
    } // ~Overlayfs
}; // class: Overlayfs



} // namespace ns_overlayfs

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
