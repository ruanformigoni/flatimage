///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : filesystems
///

#pragma once

#include <memory>

#include "../cpp/lib/ext2/mount.hpp"
#include "../cpp/lib/overlayfs.hpp"

#include "setup.hpp"

namespace ns_filesystems
{

// class Filesystems {{{
class Filesystems
{
  private:
    std::unique_ptr<ns_ext2::ns_mount::Mount> m_ext2;
    std::unique_ptr<ns_overlayfs::Overlayfs> m_overlayfs;

  public:
    enum class FilesystemsLayer
    {
      EXT_RO,
      EXT_RW,
      DWARFS,
      OVERLAYFS
    };

    Filesystems(ns_setup::FlatimageSetup const& config, FilesystemsLayer layer = FilesystemsLayer::OVERLAYFS)
    {
      // Mount main filesystem
      m_ext2 = std::make_unique<ns_ext2::ns_mount::Mount>(config.path_file_binary
        , config.path_dir_mount_ext
        , (layer == FilesystemsLayer::EXT_RW)? ns_ext2::ns_mount::Mode::RW : ns_ext2::ns_mount::Mode::RO
        , config.offset_ext2
      );
      qreturn_if(layer == FilesystemsLayer::EXT_RO or layer == FilesystemsLayer::EXT_RW);

      // TODO Mount dwarfs layers
      qreturn_if(layer == FilesystemsLayer::DWARFS);

      // Mount overlayfs on top of read-only ext2 filesystem and dwarfs layers
      m_overlayfs = std::make_unique<ns_overlayfs::Overlayfs>(config.path_dir_mount_ext
        , config.path_dir_host_overlayfs
        , config.path_dir_mount_overlayfs
      );
    } // Filesystems

    Filesystems(Filesystems const&) = delete;
    Filesystems(Filesystems&&) = delete;
    Filesystems& operator=(Filesystems const&) = delete;
    Filesystems& operator=(Filesystems&&) = delete;
}; // class Filesystems }}}

} // namespace ns_filesystems

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
