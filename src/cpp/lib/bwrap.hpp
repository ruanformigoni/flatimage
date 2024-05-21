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
  // PRINT    = 1 << 0,
  // WAITFILE = 1 << 1,
  // CHECKERR = 1 << 2,
}; // enum

class Bwrap
{
  private:
    fs::path m_path_file_program;
    std::vector<std::string> m_path_file_program_args;
    std::vector<std::string> m_args;
    std::unordered_map<std::string, std::string> m_env;
    bool m_is_root;

    void set_xdg_runtime_dir();
    void bind_root();
    void bind_home();
    void bind_media();
    void bind_audio();
    void bind_wayland();
    void bind_xorg();
    void bind_dbus_user();
    void bind_dbus_system();
    void bind_udev();
    void bind_input();
    void bind_usb();
    void bind_network();
    void bind_gpu();
    void bind_runtime_mounts();

  public:
    template<ns_concept::AsString... Args>
    Bwrap(bool is_root
        , fs::path const& path_dir_root
        , fs::path const& path_file_program
        , Args&&... args);
}; // class: Bwrap

// set_xdg_runtime_dir() {{{
inline void Bwrap::set_xdg_runtime_dir()
{
  const char* env_xdg_runtime_dir = ns_env::get("XDG_RUNTIME_DIR");
  ereturn_if(not env_xdg_runtime_dir, "XDG_RUNTIME_DIR is undefined");
  ns_vector::push_back(m_args, "--setenv", "XDG_RUNTIME_DIR", env_xdg_runtime_dir);
} // set_xdg_runtime_dir() }}}

// bind_root() {{{
inline void Bwrap::bind_root()
{
  const char* env_fim_dir_runtime_host = ns_env::get("FIM_DIR_RUNTIME_HOST");
  ereturn_if(not env_fim_dir_runtime_host, "FIM_DIR_RUNTIME_HOST is undefined");
  ns_vector::push_back(m_args, "--ro-bind-try", "/", env_fim_dir_runtime_host);
} // bind_root() }}}

// bind_home() {{{
inline void Bwrap::bind_home()
{
  qreturn_if(m_is_root);
  const char* env_home = ns_env::get("HOME");
  ereturn_if(not env_home, "HOME is undefined");
  ns_vector::push_back(m_args, "--ro-bind-try", env_home, env_home);
} // bind_home() }}}

// bind_media() {{{
inline void Bwrap::bind_media()
{
  ns_vector::push_back(m_args, "--ro-bind-try", "/media", "/media");
  ns_vector::push_back(m_args, "--ro-bind-try", "/run/media", "/run/media");
  ns_vector::push_back(m_args, "--ro-bind-try", "/mnt", "/mnt");
} // bind_media() }}}

// bind_audio() {{{
inline void Bwrap::bind_audio()
{
  // Get XDG_RUNTIME_DIR
  const char* env_xdg_runtime_dir = ns_env::get("XDG_RUNTIME_DIR");
  ereturn_if(not env_xdg_runtime_dir, "XDG_RUNTIME_DIR is undefined");

  // Try to bind pulse socket
  fs::path path_socket_pulse = fs::path(env_xdg_runtime_dir) / "pulse/native";
  ns_vector::push_back(m_args, "--bind-try", path_socket_pulse, path_socket_pulse);
  ns_vector::push_back(m_args, "--setenv", "PULSE_SERVER", "unix:" + path_socket_pulse.string());

  // Try to bind pipewire socket
  fs::path path_socket_pipewire = fs::path(env_xdg_runtime_dir) / "pipewire-0";
  ns_vector::push_back(m_args, "--bind-try", path_socket_pipewire, path_socket_pipewire);

  // Other paths required to sound
  ns_vector::push_back(m_args, "--dev-bind-try", "/dev/dsp", "/dev/dsp");
  ns_vector::push_back(m_args, "--bind-try", "/dev/snd", "/dev/snd");
  ns_vector::push_back(m_args, "--bind-try", "/dev/shm", "/dev/shm");
  ns_vector::push_back(m_args, "--bind-try", "/proc/asound", "/proc/asound");
} // bind_audio() }}}

// bind_wayland() {{{
inline void Bwrap::bind_wayland()
{
  // Get XDG_RUNTIME_DIR
  const char* env_xdg_runtime_dir = ns_env::get("XDG_RUNTIME_DIR");
  ereturn_if(not env_xdg_runtime_dir, "XDG_RUNTIME_DIR is undefined");

  // Get WAYLAND_DISPLAY
  const char* env_wayland_display = ns_env::get("WAYLAND_DISPLAY");
  ereturn_if(not env_wayland_display, "WAYLAND_DISPLAY is undefined");

  // Get wayland socket
  fs::path path_socket_wayland = fs::path(env_xdg_runtime_dir) / env_wayland_display;

  // Bind
  ns_vector::push_back(m_args, "--bind-try", path_socket_wayland, path_socket_wayland);
  ns_vector::push_back(m_args, "--setenv", "WAYLAND_DISPLAY", env_wayland_display);
} // bind_wayland() }}}

// bind_xorg() {{{
inline void Bwrap::bind_xorg()
{
  // Get DISPLAY
  const char* env_display = ns_env::get("DISPLAY");
  ereturn_if(not env_display, "DISPLAY is undefined");

  // Get XAUTHORITY
  const char* env_xauthority = ns_env::get("XAUTHORITY");
  ereturn_if(not env_xauthority, "XAUTHORITY is undefined");

  // Bind
  ns_vector::push_back(m_args, "--ro-bind-try", env_xauthority, env_xauthority);
  ns_vector::push_back(m_args, "--setenv", "XAUTHORITY", env_xauthority);
  ns_vector::push_back(m_args, "--setenv", "DISPLAY", env_display);
} // bind_xorg() }}}

// bind_dbus_user() {{{
inline void Bwrap::bind_dbus_user()
{
  // Get DBUS_SESSION_BUS_ADDRESS
  const char* env_dbus_session_bus_address = ns_env::get("DBUS_SESSION_BUS_ADDRESS");
  ereturn_if(not env_dbus_session_bus_address, "DBUS_SESSION_BUS_ADDRESS is undefined");

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
} // bind_dbus_user() }}}

// bind_dbus_system() {{{
inline void Bwrap::bind_dbus_system()
{
  ns_vector::push_back(m_args, "--bind-try", "/run/dbus/system_bus_socket", "/run/dbus/system_bus_socket");
} // bind_dbus_system() }}}

// bind_udev() {{{
inline void Bwrap::bind_udev()
{
  ns_vector::push_back(m_args, "--bind-try", "/run/udev", "/run/udev");
} // bind_udev() }}}

// bind_input() {{{
inline void Bwrap::bind_input()
{
  ns_vector::push_back(m_args, "--dev-bind-try", "/dev/input", "/dev/input");
  ns_vector::push_back(m_args, "--dev-bind-try", "/dev/uinput", "/dev/uinput");
} // bind_input() }}}

// bind_usb() {{{
inline void Bwrap::bind_usb()
{
  ns_vector::push_back(m_args, "--dev-bind-try", "/dev/bus/usb", "/dev/bus/usb");
  ns_vector::push_back(m_args, "--dev-bind-try", "/dev/usb", "/dev/usb");
} // bind_usb() }}}

// bind_network() {{{
inline void Bwrap::bind_network()
{
  ns_vector::push_back(m_args, "--bind-try", "/etc/host.conf", "/etc/host.conf");
  ns_vector::push_back(m_args, "--bind-try", "/etc/hosts", "/etc/hosts");
  ns_vector::push_back(m_args, "--bind-try", "/etc/nsswitch.conf", "/etc/nsswitch.conf");
  ns_vector::push_back(m_args, "--bind-try", "/etc/resolv.conf", "/etc/resolv.conf");
} // bind_network() }}}

// bind_gpu() {{{
inline void Bwrap::bind_gpu()
{
  ns_vector::push_back(m_args, "--dev-bind-try", "/dev/dri", "/dev/dri");
  
  // Nvidia symlinks
} // bind_gpu() }}}

// bind_runtime_mounts() {{{
inline void Bwrap::bind_runtime_mounts()
{
  const char* env_fim_dir_mounts = ns_env::get("FIM_DIR_MOUNTS");
  const char* env_fim_dir_runtime_mounts = ns_env::get("FIM_DIR_RUNTIME_MOUNTS");
  ereturn_if(not env_fim_dir_mounts, "FIM_DIR_MOUNTS is undefined");
  ereturn_if(not env_fim_dir_runtime_mounts, "FIM_DIR_RUNTIME_MOUNTS is undefined");
  ns_vector::push_back(m_args, "--bind-try", env_fim_dir_mounts, env_fim_dir_runtime_mounts);
} // bind_runtime_mounts() }}}

// Bwrap() {{{
template<ns_concept::AsString... Args>
inline Bwrap::Bwrap(bool is_root
    , fs::path const& path_dir_root
    , fs::path const& path_file_program
    , Args&&... args)
  : m_path_file_program(path_file_program)
  , m_path_file_program_args(std::vector<std::string>{ns_string::to_string(args)...})
  , m_is_root(is_root)
{
  uid_t user_id = getuid();

  // Configure some environment variables
  m_env["TERM"] = "xterm";

  if ( not ns_env::get("XDG_RUNTIME_DIR") )
  {
    m_env["XDG_RUNTIME_DIR"] = "/run/user/{}"_fmt(ns_string::to_string(user_id));
  } // if
  
  if ( struct passwd *pw = getpwuid(user_id); pw )
  {
    m_env["HOST_USERNAME"] = pw->pw_name;
  } // if

  if ( char const* env_path = ns_env::get("PATH") )
  {
    m_env["PATH"] = std::string{env_path} + ":/sbin:/usr/sbin:/usr/local/sbin:/bin:/usr/bin:/usr/local/bin";
  } // if

  // Create custom bashrc file
  if ( const char* env_bashrc_file = ns_env::get("BASHRC_FILE"); env_bashrc_file )
  {
    std::ofstream of{env_bashrc_file};
    if ( of.good() )
    {
      of << "export PS1=\"(flatimage@\"${FIM_DIST,,}\") → \"";
    } // if
    of.close();
  } // if

  // Check if root exists and is a directory
  ethrow_if(not fs::is_directory(path_dir_root)
    , "'{}' does not exist or is not a directory"_fmt(path_dir_root)
  );

  if ( is_root ) { ns_vector::push_back(m_args, "--uid", "0", "--gid", "0"); }
  ns_vector::push_back(m_args, "--bind", path_dir_root, "/");
  ns_vector::push_back(m_args, "--dev", "/dev");
  ns_vector::push_back(m_args, "--proc", "/proc");
  ns_vector::push_back(m_args, "--bind", "/tmp", "/tmp");
  ns_vector::push_back(m_args, "--bind", "/sys", "/sys");
  ns_vector::push_back(m_args, "--bind-try", "/etc/group", "/etc/group");

  set_xdg_runtime_dir();
  bind_root();
  bind_home();
  bind_media();
  bind_audio();
  bind_wayland();
  bind_xorg();
  bind_dbus_user();
  bind_dbus_system();
  bind_udev();
  bind_input();
  bind_usb();
  bind_network();
  bind_gpu();
  bind_runtime_mounts();

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
} // Bwrap() }}}

} // namespace ns_bwrap

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
