///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : filesystems
///

#pragma once

#include <memory>

#include "../cpp/lib/ext2/mount.hpp"
#include "../cpp/lib/overlayfs.hpp"
#include "../cpp/lib/dwarfs.hpp"

#include "setup.hpp"

namespace ns_filesystems
{

// class Filesystems {{{
class Filesystems
{
  private:
    std::unique_ptr<ns_ext2::ns_mount::Mount> m_ext2;
    std::vector<std::unique_ptr<ns_dwarfs::Dwarfs>> m_layers;
    std::unique_ptr<ns_overlayfs::Overlayfs> m_overlayfs;
    void mount_ext2(fs::path const& path_file_binary
      , fs::path const& path_dir_mount_ext
      , uint64_t offset_ext2
      , ns_ext2::ns_mount::Mode mode
    );
    void mount_dwarfs(fs::path const& path_dir_layers
      , fs::path const& path_dir_mount
    );
    void mount_overlayfs(fs::path const& path_dir_lower
      , fs::path const& path_dir_data
      , fs::path const& path_dir_mount
    );

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
      mount_ext2(config.path_file_binary
        , config.path_dir_mount_ext
        , config.offset_ext2
        , (layer == FilesystemsLayer::EXT_RW)? ns_ext2::ns_mount::Mode::RW : ns_ext2::ns_mount::Mode::RO
      );
      qreturn_if(layer == FilesystemsLayer::EXT_RO or layer == FilesystemsLayer::EXT_RW);

      // Mount dwarfs layers
      mount_dwarfs(config.path_dir_layers, config.path_dir_mount_layers);
      qreturn_if(layer == FilesystemsLayer::DWARFS);

      // Mount overlayfs on top of read-only ext2 filesystem and dwarfs layers
      mount_overlayfs(config.path_dir_mount_ext
        , config.path_dir_data_overlayfs
        , config.path_dir_mount_overlayfs);
    } // Filesystems

    Filesystems(Filesystems const&) = delete;
    Filesystems(Filesystems&&) = delete;
    Filesystems& operator=(Filesystems const&) = delete;
    Filesystems& operator=(Filesystems&&) = delete;
}; // class Filesystems }}}

// fn: mount_ext2 {{{
inline void Filesystems::mount_ext2(fs::path const& path_file_binary
  , fs::path const& path_dir_mount_ext
  , uint64_t offset_ext2
  , ns_ext2::ns_mount::Mode mode)
{
  // Mount main filesystem
  m_ext2 = std::make_unique<ns_ext2::ns_mount::Mount>(path_file_binary
    , path_dir_mount_ext
    , mode
    , offset_ext2
  );
} // fn: mount_ext2 }}}

// fn: mount_dwarfs {{{
inline void Filesystems::mount_dwarfs(fs::path const& path_dir_layers
  , fs::path const& path_dir_mount)
{
  std::vector<fs::path> vec_mountpoints;

  for (auto&& entry : fs::directory_iterator(path_dir_layers)
    | std::views::filter([](auto&& e){ return fs::is_regular_file(e); })
  )
  {
    // Create sub-directory in path_dir_mount with the same name as the filesystem
    fs::path path_file_filesystem = entry.path();
    fs::path path_dir_submount = path_dir_mount / path_file_filesystem.filename();
    ns_log::info()("Mount '{}' in '{}'", path_file_filesystem.filename(), path_dir_submount);
    if ( not fs::exists(path_dir_submount) )
    {
      std::error_code ec;
      fs::create_directories(path_dir_submount, ec);
      econtinue_if(ec, "Could not create overlay mountpoint '{}'"_fmt(ec.message()));
    } // if
    m_layers.emplace_back(std::make_unique<ns_dwarfs::Dwarfs>(path_file_filesystem, path_dir_submount));
  } // for
} // fn: mount_dwarfs }}}

// fn: mount_overlayfs {{{
inline void Filesystems::mount_overlayfs(fs::path const& path_dir_lower
  , fs::path const& path_dir_data
  , fs::path const& path_dir_mount)
{
  m_overlayfs = std::make_unique<ns_overlayfs::Overlayfs>(path_dir_lower
    , path_dir_data
    , path_dir_mount
  );
} // fn: mount_overlayfs }}}

} // namespace ns_filesystems

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
