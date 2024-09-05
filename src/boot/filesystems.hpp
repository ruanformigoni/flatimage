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
    fs::path m_path_dir_mount;
    std::vector<fs::path> m_vec_path_dir_mountpoints;
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
    void mount_overlayfs(std::vector<fs::path> const& vec_path_dir_lower
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

    Filesystems(ns_setup::FlatimageSetup const& config, FilesystemsLayer layer = FilesystemsLayer::OVERLAYFS);
    Filesystems(Filesystems const&) = delete;
    Filesystems(Filesystems&&) = delete;
    Filesystems& operator=(Filesystems const&) = delete;
    Filesystems& operator=(Filesystems&&) = delete;
    // In case the parent process fails to clean the mountpoints, this child does it
    void spawn_janitor();
}; // class Filesystems }}}

// fn: Filesystems::Filesystems {{{
inline Filesystems::Filesystems(ns_setup::FlatimageSetup const& config, FilesystemsLayer layer)
  : m_path_dir_mount(config.path_dir_mount)
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
  std::vector<fs::path> vec_path_dir_layers;
  // Push ext layer
  vec_path_dir_layers.push_back(config.path_dir_mount_ext);
  // Push additional layers mounted with dwarfs
  for (auto&& layer : m_layers)
  {
    vec_path_dir_layers.push_back(layer->get_dir_mountpoint());
  } // for
  mount_overlayfs(vec_path_dir_layers
    , config.path_dir_data_overlayfs
    , config.path_dir_mount_overlayfs);
} // fn Filesystems::Filesystems }}}

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
  m_vec_path_dir_mountpoints.push_back(path_dir_mount_ext);
} // fn: mount_ext2 }}}

// fn: mount_dwarfs {{{
inline void Filesystems::mount_dwarfs(fs::path const& path_dir_layers
  , fs::path const& path_dir_mount)
{
  // TODO Improve this when std::ranges::to becomes available in alpine

  // Get directories
  std::vector<fs::path> vec_path_dir_layer;
  for( auto&& entry :  fs::directory_iterator(path_dir_layers))
  {
    vec_path_dir_layer.push_back(entry.path());
  } // for

  // Filter non-directory files
  auto f_is_regular_file = std::views::filter([](auto&& e){ return fs::is_regular_file(e); });
  std::erase_if(vec_path_dir_layer, f_is_regular_file);

  // Sort paths
  std::ranges::sort(vec_path_dir_layer);

  // Mount one at the time
  for (auto&& entry : vec_path_dir_layer)
  {
    // Create sub-directory in path_dir_mount with the same name as the filesystem
    fs::path path_file_filesystem = entry;
    fs::path path_dir_submount = path_dir_mount / path_file_filesystem.filename();
    ns_log::info()("Mount '{}' in '{}'", path_file_filesystem.filename(), path_dir_submount);
    if ( not fs::exists(path_dir_submount) )
    {
      std::error_code ec;
      fs::create_directories(path_dir_submount, ec);
      econtinue_if(ec, "Could not create overlay mountpoint '{}'"_fmt(ec.message()));
    } // if
    m_layers.emplace_back(std::make_unique<ns_dwarfs::Dwarfs>(path_file_filesystem, path_dir_submount));
    m_vec_path_dir_mountpoints.push_back(path_dir_submount);
  } // for
} // fn: mount_dwarfs }}}

// fn: mount_overlayfs {{{
inline void Filesystems::mount_overlayfs(std::vector<fs::path> const& vec_path_dir_lower
  , fs::path const& path_dir_data
  , fs::path const& path_dir_mount)
{
  m_overlayfs = std::make_unique<ns_overlayfs::Overlayfs>(vec_path_dir_lower
    , path_dir_data
    , path_dir_mount
  );
  m_vec_path_dir_mountpoints.push_back(path_dir_mount);
} // fn: mount_overlayfs }}}

// fn: spawn_janitor {{{
inline void Filesystems::spawn_janitor()
{
  // Find janitor binary
  fs::path path_file_janitor = fs::path{ns_env::get_or_throw("FIM_DIR_APP_BIN")} / "janitor";

  // Fork and execve into the janitor process
  pid_t pid_parent = getpid();
  pid_t pid_fork = fork();
  ethrow_if(pid_fork < 0, "Failed to fork janitor");

  // Is parent
  dreturn_if(pid_fork > 0, "Spawned janitor with PID '{}'"_fmt(pid_fork));

  // Redirect stdout/stderr to a log file
  fs::path path_stdout = std::string{ns_env::get_or_throw("FIM_DIR_MOUNT")} + ".janitor.stdout.log";
  fs::path path_stderr = std::string{ns_env::get_or_throw("FIM_DIR_MOUNT")} + ".janitor.stderr.log";
  int fd_stdout = open(path_stdout.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
  int fd_stderr = open(path_stderr.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
  ereturn_if(fd_stdout < 0, "Failed to open stdout janitor file");
  ereturn_if(fd_stderr < 0, "Failed to open stderr janitor file");
  dup2(fd_stdout, STDOUT_FILENO);
  dup2(fd_stderr, STDERR_FILENO);
  close(fd_stdout);
  close(fd_stderr);
  close(STDIN_FILENO);

  // Keep parent pid in a variable
  ns_env::set("PID_PARENT", pid_parent, ns_env::Replace::Y);

  // Create args to janitor
  std::vector<std::string> vec_argv_custom;
  vec_argv_custom.push_back(path_file_janitor);
  std::copy(m_vec_path_dir_mountpoints.rbegin(), m_vec_path_dir_mountpoints.rend(), std::back_inserter(vec_argv_custom));
  auto argv_custom = std::make_unique<const char*[]>(vec_argv_custom.size() + 1);
  argv_custom[vec_argv_custom.size()] = nullptr;
  std::ranges::transform(vec_argv_custom, argv_custom.get(), [](auto&& e) { return e.c_str(); });

  // Execve to janitor
  execve(path_file_janitor.c_str(), (char**) argv_custom.get(), environ);

  // Exit process in case of an error
  exit(1);
} // fn: spawn_janitor }}}

} // namespace ns_filesystems

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
