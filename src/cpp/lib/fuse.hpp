///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : fuse
///

#pragma once

#include <cstring>
#include <filesystem>
#include <expected>
#include <sys/vfs.h>
#include <sys/mount.h>

#include "subprocess.hpp"

// Other codes available here:
// https://man7.org/linux/man-pages/man2/statfs.2.html
#define FUSE_SUPER_MAGIC 0x65735546

namespace ns_fuse
{

namespace
{

namespace fs = std::filesystem;

} // namespace 

// Check if a directory is mounted with fuse
inline std::expected<bool,std::string> is_fuse(fs::path const& path_dir_mount)
{
  struct statfs buf;

  if ( statfs(path_dir_mount.c_str(), &buf) < 0 )
  {
    return std::unexpected(strerror(errno));
  } // if

  return buf.f_type == FUSE_SUPER_MAGIC;
} // function: mountpoint

inline void unmount(fs::path const& path_dir_mountpoint)
{
  using namespace std::chrono_literals;

  // Find fusermount
  auto opt_path_file_fusermount = ns_subprocess::search_path("fusermount");
  ereturn_if (not opt_path_file_fusermount, "Could not find 'fusermount' in PATH");

  // Filesystem could be busy for a bit after un-mount of dwarfs
  for(auto is_fuse = ns_fuse::is_fuse(path_dir_mountpoint)
    ; is_fuse and *is_fuse == true
    ; is_fuse = ns_fuse::is_fuse(path_dir_mountpoint))
  {
    auto ret = ns_subprocess::Subprocess(*opt_path_file_fusermount)
      .with_piped_outputs()
      .with_args("-zu", path_dir_mountpoint)
      .spawn()
      .wait();
    if(ret and *ret == 0) { ns_log::debug()("Un-mounted filesystem '{}'"_fmt(path_dir_mountpoint)); }
    std::this_thread::sleep_for(100ms);
  } // for

} // function: unmount

} // namespace ns_fuse

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
