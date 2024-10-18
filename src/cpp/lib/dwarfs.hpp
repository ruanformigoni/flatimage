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
      auto opt_file_dwarfs = ns_subprocess::search_path("dwarfs");
      ethrow_if(not opt_file_dwarfs.has_value(), "Could not find dwarfs");

      // Create command
      m_subprocess = std::make_unique<ns_subprocess::Subprocess>(*opt_file_dwarfs);

      // Spawn command
      (void) m_subprocess->with_piped_outputs()
        .with_args(path_file_image, path_dir_mount, "-f", "-o", "auto_unmount,offset={},imagesize={}"_fmt(offset, size_image))
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
      dreturn_if(not ret, "Mount '{}' exited unexpectedly"_fmt(m_path_dir_mountpoint));
      dreturn_if(ret and *ret != 0, "Mount '{}' exited with non-zero exit code '{}'"_fmt(m_path_dir_mountpoint, *ret));
    } // Dwarfs

    fs::path const& get_dir_mountpoint()
    {
      return m_path_dir_mountpoint;
    }
}; // class Dwarfs }}}

// is_dwarfs() {{{
inline bool is_dwarfs(fs::path const& path_file_dwarfs, uint64_t offset = 0)
{
  // Open file
  std::ifstream file_dwarfs(path_file_dwarfs, std::ios::binary | std::ios::in);
  ereturn_if(not file_dwarfs.is_open(), "Could not open file '{}'"_fmt(path_file_dwarfs), false);
  // Adjust offset
  file_dwarfs.seekg(offset);
  ereturn_if(not file_dwarfs, "Failed to seek offset '{}' in file '{}'"_fmt(offset, path_file_dwarfs), false);
  // Read initial 'DWARFS' identifier
  std::array<char,6> header;
  ereturn_if(not file_dwarfs.read(header.data(), header.size()), "Could not read bytes from file '{}'"_fmt(path_file_dwarfs), false);
  // Check for a successful read
  ereturn_if(file_dwarfs.gcount() != header.size(), "Short read for file '{}'"_fmt(path_file_dwarfs), false);
  // Check for match
  return std::ranges::equal(header, std::string_view("DWARFS"));
} // is_dwarfs() }}}


} // namespace ns_dwarfs

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
