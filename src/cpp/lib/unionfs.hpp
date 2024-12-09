///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : unionfs
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

namespace ns_unionfs
{

class UnionFs
{
  private:
    std::unique_ptr<ns_subprocess::Subprocess> m_subprocess;
    fs::path m_path_dir_mountpoint;

  public:
    UnionFs(std::vector<fs::path> const& vec_path_dir_layer
        , fs::path const& path_dir_upperdir
        , fs::path const& path_dir_mountpoint
        , pid_t pid_to_die_for
      )
      : m_path_dir_mountpoint(path_dir_mountpoint)
    {
      ethrow_if (not fs::exists(path_dir_upperdir) and not fs::create_directories(path_dir_upperdir)
        , "Could not create modifications dir for unionfs"
      );

      ethrow_if (not fs::exists(path_dir_mountpoint) and not fs::create_directories(path_dir_mountpoint)
        , "Could not create mountpoint for unionfs"
      );

      // Find unionfs
      auto opt_path_file_unionfs = ns_subprocess::search_path("unionfs");
      ethrow_if (not opt_path_file_unionfs, "Could not find 'unionfs' in PATH");

      // Create subprocess
      m_subprocess = std::make_unique<ns_subprocess::Subprocess>(*opt_path_file_unionfs);

      // Create string to represent layers argumnet
      // First layer is the writteable one
      // Layers are overlayed as top_branch:lower_branch:...:lowest_branch
      std::string arg_layers="{}=RW"_fmt(path_dir_upperdir);
      for (auto&& path_dir_layer : vec_path_dir_layer)
      {
        arg_layers += ":{}=RO"_fmt(path_dir_layer);
      } // for

      // Include arguments and spawn process
      std::ignore = m_subprocess->
         with_args("-f")
        .with_args("-o", "cow")
        .with_args(arg_layers)
        .with_args(m_path_dir_mountpoint)
        .with_die_on_pid(pid_to_die_for)
        .spawn();
      // Wait for mount
      ns_fuse::wait_fuse(path_dir_mountpoint);
    } // unionfs

    ~UnionFs()
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
    } // ~UnionFs
}; // class: UnionFs



} // namespace ns_unionfs

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
