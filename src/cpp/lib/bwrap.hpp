///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : bwrap
///

#pragma once

#include <filesystem>
#include <sys/types.h>
#include <pwd.h>

#include "subprocess.hpp"

#include "../macro.hpp"

#include "../std/vector.hpp"

namespace ns_bwrap
{

namespace
{

namespace fs = std::filesystem;

}

enum class Permissions
{
  STORAGE     = 0,
}; // enum

class Bwrap
{
  private:
    fs::path m_path_file_program;
    fs::path m_path_dir_xdg_runtime;
    std::vector<std::string> m_path_file_program_args;
    std::vector<std::string> m_args;
    std::unordered_map<std::string, std::string> m_env;
    bool m_is_root;
    void set_xdg_runtime_dir();

  public:
    template<ns_concept::AsString... Args>
    Bwrap(bool is_root
      , fs::path const& path_dir_root
      , fs::path const& path_file_bashrc
      , fs::path const& path_file_program
      , Args&&... args);
    Bwrap& bind_root(fs::path const& path_dir_runtime_host);
    Bwrap& bind_home(fs::path const& path_dir_home);
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
    Bwrap& bind_gpu();
    Bwrap& bind_runtime_mounts(fs::path const& path_dir_mounts, fs::path const& path_dir_runtime_mounts);
    void run();
}; // class: Bwrap

// Bwrap() {{{
template<ns_concept::AsString... Args>
inline Bwrap::Bwrap(bool is_root
    , fs::path const& path_dir_root
    , fs::path const& path_file_bashrc
    , fs::path const& path_file_program
    , Args&&... args)
  : m_path_file_program(path_file_program)
  , m_path_file_program_args(std::vector<std::string>{ns_string::to_string(args)...})
  , m_is_root(is_root)
{
  // Configure some environment variables
  m_env["TERM"] = "xterm";

  if ( struct passwd *pw = getpwuid(getuid()); pw )
  {
    m_env["HOST_USERNAME"] = pw->pw_name;
  } // if

  // Create custom bashrc file
  std::ofstream of{path_file_bashrc};
  if ( of.good() )
  {
    of << "export PS1=\"(flatimage@\"${FIM_DIST,,}\") → \"";
  } // if
  of.close();
  ns_env::set("BASHRC_FILE", path_file_bashrc.c_str(), ns_env::Replace::Y);

  // Check if root exists and is a directory
  ethrow_if(not fs::is_directory(path_dir_root)
    , "'{}' does not exist or is not a directory"_fmt(path_dir_root)
  );

  // Basic bindings
  if ( is_root ) { ns_vector::push_back(m_args, "--uid", "0", "--gid", "0"); }
  ns_vector::push_back(m_args, "--bind", path_dir_root, "/");
  ns_vector::push_back(m_args, "--dev", "/dev");
  ns_vector::push_back(m_args, "--proc", "/proc");
  ns_vector::push_back(m_args, "--bind", "/tmp", "/tmp");
  ns_vector::push_back(m_args, "--bind", "/sys", "/sys");
  ns_vector::push_back(m_args, "--bind-try", "/etc/group", "/etc/group");

  set_xdg_runtime_dir();
} // Bwrap() }}}

// set_xdg_runtime_dir() {{{
inline void Bwrap::set_xdg_runtime_dir()
{
  m_env["XDG_RUNTIME_DIR"] = "/run/user/{}"_fmt(ns_env::get_or_else("XDG_RUNTIME_DIR", ns_string::to_string(getuid())));
  ns_vector::push_back(m_args, "--setenv", "XDG_RUNTIME_DIR", m_env["XDG_RUNTIME_DIR"]);
} // set_xdg_runtime_dir() }}}

// bind_root() {{{
inline Bwrap& Bwrap::bind_root(fs::path const& path_dir_runtime_host)
{
  ns_vector::push_back(m_args, "--ro-bind-try", "/", path_dir_runtime_host);
  return *this;
} // bind_root() }}}

// bind_home() {{{
inline Bwrap& Bwrap::bind_home(fs::path const& path_dir_home)
{
  if ( not m_is_root )
  {
    ns_vector::push_back(m_args, "--ro-bind-try", path_dir_home, path_dir_home);
  } // if
  return *this;
} // bind_home() }}}

// bind_media() {{{
inline Bwrap& Bwrap::bind_media()
{
  ns_vector::push_back(m_args, "--ro-bind-try", "/media", "/media");
  ns_vector::push_back(m_args, "--ro-bind-try", "/run/media", "/run/media");
  ns_vector::push_back(m_args, "--ro-bind-try", "/mnt", "/mnt");
  return *this;
} // bind_media() }}}

// bind_audio() {{{
inline Bwrap& Bwrap::bind_audio()
{
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
  ns_vector::push_back(m_args, "--bind-try", "/run/dbus/system_bus_socket", "/run/dbus/system_bus_socket");
  return *this;
} // bind_dbus_system() }}}

// bind_udev() {{{
inline Bwrap& Bwrap::bind_udev()
{
  ns_vector::push_back(m_args, "--bind-try", "/run/udev", "/run/udev");
  return *this;
} // bind_udev() }}}

// bind_input() {{{
inline Bwrap& Bwrap::bind_input()
{
  ns_vector::push_back(m_args, "--dev-bind-try", "/dev/input", "/dev/input");
  ns_vector::push_back(m_args, "--dev-bind-try", "/dev/uinput", "/dev/uinput");
  return *this;
} // bind_input() }}}

// bind_usb() {{{
inline Bwrap& Bwrap::bind_usb()
{
  ns_vector::push_back(m_args, "--dev-bind-try", "/dev/bus/usb", "/dev/bus/usb");
  ns_vector::push_back(m_args, "--dev-bind-try", "/dev/usb", "/dev/usb");
  return *this;
} // bind_usb() }}}

// bind_network() {{{
inline Bwrap& Bwrap::bind_network()
{
  ns_vector::push_back(m_args, "--bind-try", "/etc/host.conf", "/etc/host.conf");
  ns_vector::push_back(m_args, "--bind-try", "/etc/hosts", "/etc/hosts");
  ns_vector::push_back(m_args, "--bind-try", "/etc/nsswitch.conf", "/etc/nsswitch.conf");
  ns_vector::push_back(m_args, "--bind-try", "/etc/resolv.conf", "/etc/resolv.conf");
  return *this;
} // bind_network() }}}

// bind_gpu() {{{
inline Bwrap& Bwrap::bind_gpu()
{
  ns_vector::push_back(m_args, "--dev-bind-try", "/dev/dri", "/dev/dri");
  // TODO Nvidia symlinks
  return *this;
} // bind_gpu() }}}

// bind_runtime_mounts() {{{
inline Bwrap& Bwrap::bind_runtime_mounts(fs::path const& path_dir_mounts, fs::path const& path_dir_runtime_mounts)
{
  ns_vector::push_back(m_args, "--bind-try", path_dir_mounts, path_dir_runtime_mounts);
  return *this;
} // bind_runtime_mounts() }}}

// run() {{{
inline void Bwrap::run()
{
  // Find bwrap in PATH
  auto opt_path_file_bwrap = ns_subprocess::search_path("bwrap");
  ethrow_if(not opt_path_file_bwrap.has_value(), "Could not find bwrap");

  auto subprocess = ns_subprocess::Subprocess(*opt_path_file_bwrap)
    .with_args(m_args)
    .with_args(m_path_file_program)
    .with_args(m_path_file_program_args);

  for (auto&& [k,v] : m_env)
  {
    subprocess.with_var(k, v);
  } // for

  subprocess.spawn(true);
} // run() }}}

} // namespace ns_bwrap

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
