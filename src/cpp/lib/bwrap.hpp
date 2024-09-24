///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : bwrap
///

#pragma once

#include <filesystem>
#include <sys/types.h>
#include <pwd.h>

#include "../std/vector.hpp"
#include "../std/functional.hpp"
#include "../macro.hpp"
#include "log.hpp"
#include "db.hpp"
#include "match.hpp"
#include "subprocess.hpp"
#include "env.hpp"
#include "reserved/permissions.hpp"

namespace ns_bwrap
{

namespace
{

namespace fs = std::filesystem;

}

namespace ns_permissions
{

using PermissionBits = ns_reserved::ns_permissions::Bits;
using Permissions = ns_reserved::ns_permissions::Permissions;

} // namespace ns_permissions

class Bwrap
{
  private:
    // Program to run, its arguments and environment
    fs::path m_path_file_program;
    std::vector<std::string> m_program_args;
    std::vector<std::string> m_program_env;

    // XDG_RUNTIME_DIR
    fs::path m_path_dir_xdg_runtime;

    // Arguments and environment to bwrap
    std::vector<std::string> m_args;

    // Run bwrap with uid and gid equal to 0
    bool m_is_root;

    void set_xdg_runtime_dir();

  public:
    template<ns_concept::StringRepresentable... Args>
    Bwrap(bool is_root
      , fs::path const& path_dir_root
      , fs::path const& path_file_bashrc
      , fs::path const& path_file_program
      , std::vector<std::string> const& program_args
      , std::vector<std::string> const& program_env);
    Bwrap(Bwrap const&) = delete;
    Bwrap(Bwrap&&) = delete;
    Bwrap& operator=(Bwrap const&) = delete;
    Bwrap& operator=(Bwrap&&) = delete;
    Bwrap& symlink_nvidia(fs::path const& path_dir_root_guest, fs::path const& path_dir_root_host);
    Bwrap& with_binds_from_file(fs::path const& path_file_bindings);
    Bwrap& bind_home();
    Bwrap& bind_media();
    Bwrap& bind_audio();
    Bwrap& bind_wayland();
    Bwrap& bind_xorg();
    Bwrap& bind_dbus_user();
    Bwrap& bind_dbus_system();
    Bwrap& bind_udev();
    Bwrap& bind_input();
    Bwrap& bind_usb();
    Bwrap& bind_network();
    Bwrap& with_bind_gpu(fs::path const& path_dir_root_guest, fs::path const& path_dir_root_host);
    Bwrap& with_bind(fs::path const& src, fs::path const& dst);
    Bwrap& with_bind_ro(fs::path const& src, fs::path const& dst);
    void run(ns_permissions::PermissionBits const& permissions);
}; // class: Bwrap

// Bwrap() {{{
template<ns_concept::StringRepresentable... Args>
inline Bwrap::Bwrap(
      bool is_root
    , fs::path const& path_dir_root
    , fs::path const& path_file_bashrc
    , fs::path const& path_file_program
    , std::vector<std::string> const& program_args
    , std::vector<std::string> const& program_env)
  : m_path_file_program(path_file_program)
  , m_program_args(program_args)
  , m_is_root(is_root)
{
  // Push passed environment
  std::ranges::for_each(program_env, [&](auto&& e){ ns_log::info()("ENV: {}", e); m_program_env.push_back(e); });

  // Configure some environment variables
  m_program_env.push_back("TERM=xterm");

  if ( struct passwd *pw = getpwuid(getuid()); pw )
  {
    m_program_env.push_back("HOST_USERNAME=pw->pw_name");
  } // if

  // Create custom bashrc file
  std::ofstream of{path_file_bashrc};
  if ( of.good() )
  {
    of << R"(export PS1="[flatimage-${FIM_DIST,,}] \W → ")";
  } // if
  of.close();
  ns_env::set("BASHRC_FILE", path_file_bashrc.c_str(), ns_env::Replace::Y);

  // Check if root exists and is a directory
  ethrow_if(not fs::is_directory(path_dir_root)
    , "'{}' does not exist or is not a directory"_fmt(path_dir_root)
  );

  // Basic bindings
  if ( m_is_root ) { ns_vector::push_back(m_args, "--uid", "0", "--gid", "0"); }
  ns_vector::push_back(m_args, "--bind", path_dir_root, "/");
  ns_vector::push_back(m_args, "--dev", "/dev");
  ns_vector::push_back(m_args, "--proc", "/proc");
  ns_vector::push_back(m_args, "--bind", "/tmp", "/tmp");
  ns_vector::push_back(m_args, "--bind", "/sys", "/sys");
  ns_vector::push_back(m_args, "--bind-try", "/etc/group", "/etc/group");

  // Check if XDG_RUNTIME_DIR is set or try to set it manually
  set_xdg_runtime_dir();
} // Bwrap() }}}

// set_xdg_runtime_dir() {{{
inline void Bwrap::set_xdg_runtime_dir()
{
  m_path_dir_xdg_runtime = ns_env::get_or_else("XDG_RUNTIME_DIR", "/run/user/{}" + ns_string::to_string(getuid()));
  ns_log::info()("XDG_RUNTIME_DIR: {}", m_path_dir_xdg_runtime);
  m_program_env.push_back("XDG_RUNTIME_DIR={}"_fmt(m_path_dir_xdg_runtime));
  ns_vector::push_back(m_args, "--setenv", "XDG_RUNTIME_DIR", m_path_dir_xdg_runtime);
} // set_xdg_runtime_dir() }}}

// symlink_nvidia() {{{
inline Bwrap& Bwrap::symlink_nvidia(fs::path const& path_dir_root_guest, fs::path const& path_dir_root_host)
{
  auto f_find_and_bind = [&]<typename... Args>(fs::path const& path_dir_search, Args&&... args)
  {
    std::vector<std::string_view> keywords{std::forward<Args>(args)...};
    ireturn_if(not fs::exists(path_dir_search), "Search path does not exist: '{}'"_fmt(path_dir_search));
    for(auto&& path_file_entry : fs::directory_iterator(path_dir_search) | std::views::transform([](auto&& e){ return e.path(); }))
    {
      // Skip directories
      qcontinue_if(fs::is_directory(path_file_entry));
      // Skip files that do not match keywords
      qcontinue_if(not std::ranges::any_of(keywords, [&](auto&& f){ return path_file_entry.filename().string().contains(f); }));
      // Symlink target is the file and the end of the symlink chain
      auto path_file_entry_realpath = ns_filesystem::ns_path::realpath(path_file_entry);
      econtinue_if(not path_file_entry_realpath, "Broken symlink: '{}'"_fmt(path_file_entry));
      // Create target and symlink names
      fs::path path_link_target = path_dir_root_host / path_file_entry_realpath->relative_path();
      fs::path path_link_name = path_dir_root_guest / path_file_entry.relative_path();
      // File already exists in the container as a regular file or directory, skip
      qcontinue_if(fs::exists(path_link_name) and not fs::is_symlink(path_link_name));
      // Create parent directories
      std::error_code ec;
      fs::create_directories(path_link_name.parent_path(), ec);
      econtinue_if (ec, ec.message());
      // Remove existing link
      fs::remove(path_link_name, ec);
      // Symlink
      econtinue_if(symlink(path_link_target.c_str(), path_link_name.c_str()) < 0, "{}: {}"_fmt(strerror(errno), path_link_name));
      // Log symlink successful
      ns_log::debug()("PERM(NVIDIA): {} -> {}", path_link_name, path_link_target);
    } // for
  };

  // Bind files
  f_find_and_bind("/usr/lib", "nvidia", "cuda", "nvcuvid", "nvoptix");
  f_find_and_bind("/usr/lib/x86_64-linux-gnu", "nvidia", "cuda", "nvcuvid", "nvoptix");
  f_find_and_bind("/usr/lib/i386-linux-gnu", "nvidia", "cuda", "nvcuvid", "nvoptix");
  f_find_and_bind("/usr/bin", "nvidia");
  f_find_and_bind("/usr/share", "nvidia");
  f_find_and_bind("/usr/share/vulkan/icd.d", "nvidia");
  f_find_and_bind("/usr/lib32", "nvidia", "cuda");

  // Bind devices
  for(auto&& entry : fs::directory_iterator("/dev")
    | std::views::transform([](auto&& e){ return e.path(); })
    | std::views::filter([](auto&& e){ return e.filename().string().contains("nvidia"); }))
  {
    ns_vector::push_back(m_args, "--dev-bind-try", entry, entry);
  } // for

  return *this;
} // symlink_nvidia() }}}

// with_binds_from_file() {{{
inline Bwrap& Bwrap::with_binds_from_file(fs::path const& path_file_bindings)
{
  // Load bindings from the filesystem if any
  ns_exception::ignore([&]
  {
    auto db = ns_db::Db(path_file_bindings, ns_db::Mode::READ);
    for(auto&& key : ns_db::Db(path_file_bindings, ns_db::Mode::READ).keys())
    {
      auto const& binding = db[key];
      m_args.push_back(ns_match::match(std::string{binding["type"]}
        , ns_match::equal("ro") >>= std::string{"--ro-bind-try"}
        , ns_match::equal("rw") >>= std::string{"--bind-try"}
        , ns_match::equal("dev") >>= std::string{"--dev-bind-try"}
      ));
      m_args.push_back(ns_env::expand(binding["src"]).value_or(binding["src"]));
      m_args.push_back(ns_env::expand(binding["dst"]).value_or(binding["dst"]));
    } // for
  });

  return *this;
} // with_binds_from_file() }}}

// with_bind() {{{
inline Bwrap& Bwrap::with_bind(fs::path const& src, fs::path const& dst)
{
  ns_vector::push_back(m_args, "--bind-try", src, dst);
  return *this;
} // with_bind() }}}

// with_bind_ro() {{{
inline Bwrap& Bwrap::with_bind_ro(fs::path const& src, fs::path const& dst)
{
  ns_vector::push_back(m_args, "--ro-bind-try", src, dst);
  return *this;
} // with_bind_ro() }}}

// bind_home() {{{
inline Bwrap& Bwrap::bind_home()
{
  if ( m_is_root ) { return *this; }
  ns_log::debug()("PERM(HOME)");
  const char* str_dir_home = ns_env::get_or_throw("HOME");
  ns_vector::push_back(m_args, "--bind-try", str_dir_home, str_dir_home);
  return *this;
} // bind_home() }}}

// bind_media() {{{
inline Bwrap& Bwrap::bind_media()
{
  ns_log::debug()("PERM(MEDIA)");
  ns_vector::push_back(m_args, "--bind-try", "/media", "/media");
  ns_vector::push_back(m_args, "--bind-try", "/run/media", "/run/media");
  ns_vector::push_back(m_args, "--bind-try", "/mnt", "/mnt");
  return *this;
} // bind_media() }}}

// bind_audio() {{{
inline Bwrap& Bwrap::bind_audio()
{
  ns_log::debug()("PERM(AUDIO)");

  // Try to bind pulse socket
  fs::path path_socket_pulse = m_path_dir_xdg_runtime / "pulse/native";
  ns_vector::push_back(m_args, "--bind-try", path_socket_pulse, path_socket_pulse);
  ns_vector::push_back(m_args, "--setenv", "PULSE_SERVER", "unix:" + path_socket_pulse.string());

  // Try to bind pipewire socket
  fs::path path_socket_pipewire = m_path_dir_xdg_runtime / "pipewire-0";
  ns_vector::push_back(m_args, "--bind-try", path_socket_pipewire, path_socket_pipewire);

  // Other paths required to sound
  ns_vector::push_back(m_args, "--dev-bind-try", "/dev/dsp", "/dev/dsp");
  ns_vector::push_back(m_args, "--bind-try", "/dev/snd", "/dev/snd");
  ns_vector::push_back(m_args, "--bind-try", "/dev/shm", "/dev/shm");
  ns_vector::push_back(m_args, "--bind-try", "/proc/asound", "/proc/asound");

  return *this;
} // bind_audio() }}}

// bind_wayland() {{{
inline Bwrap& Bwrap::bind_wayland()
{
  ns_log::debug()("PERM(WAYLAND)");
  // Get WAYLAND_DISPLAY
  const char* env_wayland_display = ns_env::get("WAYLAND_DISPLAY");
  dreturn_if(not env_wayland_display, "WAYLAND_DISPLAY is undefined", *this);

  // Get wayland socket
  fs::path path_socket_wayland = m_path_dir_xdg_runtime / env_wayland_display;

  // Bind
  ns_vector::push_back(m_args, "--bind-try", path_socket_wayland, path_socket_wayland);
  ns_vector::push_back(m_args, "--setenv", "WAYLAND_DISPLAY", env_wayland_display);

  return *this;
} // bind_wayland() }}}

// bind_xorg() {{{
inline Bwrap& Bwrap::bind_xorg()
{
  ns_log::debug()("PERM(XORG)");
  // Get DISPLAY
  const char* env_display = ns_env::get("DISPLAY");
  dreturn_if(not env_display, "DISPLAY is undefined", *this);

  // Get XAUTHORITY
  const char* env_xauthority = ns_env::get("XAUTHORITY");
  dreturn_if(not env_xauthority, "XAUTHORITY is undefined", *this);

  // Bind
  ns_vector::push_back(m_args, "--ro-bind-try", env_xauthority, env_xauthority);
  ns_vector::push_back(m_args, "--setenv", "XAUTHORITY", env_xauthority);
  ns_vector::push_back(m_args, "--setenv", "DISPLAY", env_display);

  return *this;
} // bind_xorg() }}}

// bind_dbus_user() {{{
inline Bwrap& Bwrap::bind_dbus_user()
{
  ns_log::debug()("PERM(DBUS_USER)");
  // Get DBUS_SESSION_BUS_ADDRESS
  const char* env_dbus_session_bus_address = ns_env::get("DBUS_SESSION_BUS_ADDRESS");
  dreturn_if(not env_dbus_session_bus_address, "DBUS_SESSION_BUS_ADDRESS is undefined", *this);

  // Path to current session bus
  std::string str_dbus_session_bus_path = env_dbus_session_bus_address;

  // Variable has expected contents similar to: 'unix:path=/run/user/1000/bus,guid=bb1adf978ae9c14....'
  // Erase until the first '=' (inclusive)
  if ( auto pos = str_dbus_session_bus_path.find('/'); pos != std::string::npos )
  {
    str_dbus_session_bus_path.erase(0, pos);
  } // if

  // Erase from the first ',' (inclusive)
  if ( auto pos = str_dbus_session_bus_path.find(','); pos != std::string::npos )
  {
    str_dbus_session_bus_path.erase(pos);
  } // if

  // Bind
  ns_vector::push_back(m_args, "--setenv", "DBUS_SESSION_BUS_ADDRESS", env_dbus_session_bus_address);
  ns_vector::push_back(m_args, "--bind-try", str_dbus_session_bus_path, str_dbus_session_bus_path);

  return *this;
} // bind_dbus_user() }}}

// bind_dbus_system() {{{
inline Bwrap& Bwrap::bind_dbus_system()
{
  ns_log::debug()("PERM(DBUS_SYSTEM)");
  ns_vector::push_back(m_args, "--bind-try", "/run/dbus/system_bus_socket", "/run/dbus/system_bus_socket");
  return *this;
} // bind_dbus_system() }}}

// bind_udev() {{{
inline Bwrap& Bwrap::bind_udev()
{
  ns_log::debug()("PERM(UDEV)");
  ns_vector::push_back(m_args, "--bind-try", "/run/udev", "/run/udev");
  return *this;
} // bind_udev() }}}

// bind_input() {{{
inline Bwrap& Bwrap::bind_input()
{
  ns_log::debug()("PERM(INPUT)");
  ns_vector::push_back(m_args, "--dev-bind-try", "/dev/input", "/dev/input");
  ns_vector::push_back(m_args, "--dev-bind-try", "/dev/uinput", "/dev/uinput");
  return *this;
} // bind_input() }}}

// bind_usb() {{{
inline Bwrap& Bwrap::bind_usb()
{
  ns_log::debug()("PERM(USB)");
  ns_vector::push_back(m_args, "--dev-bind-try", "/dev/bus/usb", "/dev/bus/usb");
  ns_vector::push_back(m_args, "--dev-bind-try", "/dev/usb", "/dev/usb");
  return *this;
} // bind_usb() }}}

// bind_network() {{{
inline Bwrap& Bwrap::bind_network()
{
  ns_log::debug()("PERM(NETWORK)");
  ns_vector::push_back(m_args, "--bind-try", "/etc/host.conf", "/etc/host.conf");
  ns_vector::push_back(m_args, "--bind-try", "/etc/hosts", "/etc/hosts");
  ns_vector::push_back(m_args, "--bind-try", "/etc/nsswitch.conf", "/etc/nsswitch.conf");
  ns_vector::push_back(m_args, "--bind-try", "/etc/resolv.conf", "/etc/resolv.conf");
  return *this;
} // bind_network() }}}

// with_bind_gpu() {{{
inline Bwrap& Bwrap::with_bind_gpu(fs::path const& path_dir_root_guest, fs::path const& path_dir_root_host)
{
  ns_log::debug()("PERM(GPU)");
  ns_vector::push_back(m_args, "--dev-bind-try", "/dev/dri", "/dev/dri");
  symlink_nvidia(path_dir_root_guest, path_dir_root_host);
  return *this;
} // with_bind_gpu() }}}

// run() {{{
inline void Bwrap::run(ns_permissions::PermissionBits const& permissions)
{
  // Configure bindings
  ns_functional::call_if(permissions.home        , [&]{ bind_home()        ; });
  ns_functional::call_if(permissions.media       , [&]{ bind_media()       ; });
  ns_functional::call_if(permissions.audio       , [&]{ bind_audio()       ; });
  ns_functional::call_if(permissions.wayland     , [&]{ bind_wayland()     ; });
  ns_functional::call_if(permissions.xorg        , [&]{ bind_xorg()        ; });
  ns_functional::call_if(permissions.dbus_user   , [&]{ bind_dbus_user()   ; });
  ns_functional::call_if(permissions.dbus_system , [&]{ bind_dbus_system() ; });
  ns_functional::call_if(permissions.udev        , [&]{ bind_udev()        ; });
  ns_functional::call_if(permissions.input       , [&]{ bind_input()       ; });
  ns_functional::call_if(permissions.usb         , [&]{ bind_usb()         ; });
  ns_functional::call_if(permissions.network     , [&]{ bind_network()     ; });

  // Find bwrap in PATH
  auto opt_path_file_bwrap = ns_subprocess::search_path("bwrap");
  ethrow_if(not opt_path_file_bwrap.has_value(), "Could not find bwrap");

  // Run Bwrap
  auto ret = ns_subprocess::Subprocess(*opt_path_file_bwrap)
    .with_args(m_args)
    .with_args(m_path_file_program)
    .with_args(m_program_args)
    .with_env(m_program_env)
    .spawn()
    .wait();
  if ( not ret ) { ns_log::error()("bwrap was signalled"); }
  if ( *ret != 0 ) { ns_log::error()("bwrap exited with non-zero exit code '{}'", *ret); }
} // run() }}}

} // namespace ns_bwrap

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
