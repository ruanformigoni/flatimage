///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : size
///

#pragma once

#include <cmath>
#include <filesystem>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/statvfs.h>
#include <ext2fs/ext2_fs.h>

#include "check.hpp"
#include "../subprocess.hpp"
#include "../../macro.hpp"
#include "../../units.hpp"

#define EXT2_SUPER_MAGIC 0xEF53
#define EXT2_SUPERBLOCK_OFFSET 1024
#define EXT2_SUPERBLOCK_SIZE 1024

namespace ns_ext2::ns_size
{

namespace
{

namespace fs = std::filesystem;

struct FilesystemFile
{
  int m_fd;
  FilesystemFile(int fd)
    : m_fd(fd) {};

  ~FilesystemFile(){ close(m_fd); };
}; // FilesystemFile

} // namespace

// resize_free_space() {{{
inline void resize_free_space(fs::path const& path_file_image, off_t offset, uint64_t bytes_size_free_min)
{
  // Check if image exists and is a regular file
  ereturn_if(not fs::is_regular_file(path_file_image)
    , "'{}' does not exist or is not a regular file"_fmt(path_file_image)
  );

  // Read filesystem file
  int fd = open(path_file_image.c_str(), O_RDONLY);
  ereturn_if(fd == -1, "Failed to open file {}: {}"_fmt(path_file_image, std::strerror(errno)));

  // RAII
  [[maybe_unused]] auto fd_resource = FilesystemFile(fd);

  // Seek filesystem in offset
  off_t pos_seek = lseek(fd, offset + EXT2_SUPERBLOCK_OFFSET, SEEK_SET);
  ereturn_if(pos_seek == (off_t) -1, "Failed to seek to superblock: {}"_fmt(std::strerror(errno)));

  // Read filesystem data block
  ext2_super_block sb;
  ereturn_if(read(fd, &sb, sizeof(sb)) != sizeof(sb), "Failed to read superblock: {}"_fmt(std::strerror(errno)));

  // Check if read was successful
  ereturn_if(sb.s_magic != EXT2_SUPER_MAGIC, "Not a valid ext2 filesystem");

  uint32_t block_size           = 1024 << sb.s_log_block_size;
  uint64_t blocks_total         = sb.s_blocks_count;
  uint64_t size_total           = blocks_total * block_size;
  uint64_t blocks_free_curr     = sb.s_free_blocks_count - sb.s_r_blocks_count;
  uint64_t blocks_free_min      = bytes_size_free_min / block_size;
  // Must be multiplied by an overhead of filesystem metadata
  uint64_t blocks_free_required = ( blocks_free_min - blocks_free_curr ) * 1.10;
  uint64_t size_free            = blocks_free_curr * block_size;

  // Find command in PATH
  auto opt_path_file_resize2fs = ns_subprocess::search_path("resize2fs");
  ereturn_if(not opt_path_file_resize2fs.has_value(), "Could not find resize2fs");

  // Resize by the free space
  uint64_t blocks_new =
    blocks_total                                            // Total number of blocks
    + blocks_free_required                                  // Number of required free blocks
    + ns_units::from_mebibytes(20).to_bytes() / block_size; // Additional free space
  // Increase by 5% the number of blocks, this is required because the number of free blocks is by
  // default less than the expected amount due metadata to overhead
  ns_log::debug()("Filesystem size statistics for: {}", path_file_image);
  ns_log::debug()("----------------------------------------------------------------");
  ns_log::debug()("State  | Type          | Value");
  ns_log::debug()("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
  ns_log::debug()("Current  blocks total  : {} ", blocks_total);
  ns_log::debug()("Current  blocks free   : {}", blocks_free_curr);
  ns_log::debug()("Target   blocks total  : {} ", blocks_new);
  ns_log::debug()("Target   blocks free   : {}",  blocks_free_min);
  ns_log::debug()("Current  size   total  : {} MiB", ns_units::from_bytes(size_total).to_mebibytes());
  ns_log::debug()("Current  size   free   : {} MiB", ns_units::from_bytes(size_free).to_mebibytes());
  ns_log::debug()("Target   size   total  : {} MiB", ns_units::from_bytes(blocks_new * block_size).to_mebibytes());
  ns_log::debug()("Target   size   free   : {} MiB", ns_units::from_bytes(bytes_size_free_min).to_mebibytes());
  ns_log::debug()("----------------------------------------------------------------");

  // Check if filesystem already has the requested minimum free space
  dreturn_if (blocks_free_curr >= blocks_free_min
    , "Filesystem already '{}' MiB of free space ({} MiB more than requested)"_fmt(
        ns_units::from_bytes(blocks_free_curr * block_size).to_mebibytes()
      , ns_units::from_bytes(blocks_free_curr * block_size).to_mebibytes()
      - ns_units::from_bytes(blocks_free_min * block_size).to_mebibytes())
  );

  // Check filesystem
  ns_check::check_force(path_file_image, offset);

  // Resize filesystem with resize2fs
  // resize2fs resizes by blocks if no unit is specified
  // so just need to divide the total number of bytes by the block size
  auto ret = ns_subprocess::Subprocess(*opt_path_file_resize2fs)
    .with_piped_outputs()
    .with_args(path_file_image.string() + "?offset=" + ns_string::to_string(offset))
    .with_args(blocks_new)
    .spawn()
    .wait();
  if ( not ret ) { ns_log::error()("resize2fs was signalled"); }
  if ( *ret != 0 ) { ns_log::error()("resize2fs exited with non-zero exit code '{}'", *ret); }

  // Check filesystem
  ns_check::check_force(path_file_image, offset);
} // resize_free_space }}}

} // namespace ns_ext2::ns_size

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
