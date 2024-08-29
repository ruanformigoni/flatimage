///
/// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
/// @file        : portal
///

#include <thread>
#include <filesystem>

#include "../cpp/lib/env.hpp"
#include "../cpp/lib/subprocess.hpp"

namespace ns_portal
{

namespace
{

namespace fs = std::filesystem;

} // anonymous namespace

// struct Portal {{{
struct Portal
{
  std::unique_ptr<ns_subprocess::Subprocess> m_process;
  fs::path m_path_file_daemon;
  fs::path m_path_file_guest;

  Portal(fs::path const& path_file_reference)
  {
    // This is read by the guest to send commands to the daemon
    ns_env::set("FIM_PORTAL_FILE", path_file_reference, ns_env::Replace::Y);

    // Path to flatimage binaries
    const char* str_dir_app_bin = ns_env::get("FIM_DIR_APP_BIN");
    ethrow_if(not str_dir_app_bin, "FIM_DIR_APP_BIN is undefined");

    // Create paths to daemon and portal
    m_path_file_daemon = fs::path{str_dir_app_bin} / "fim_portal_daemon";
    m_path_file_guest = fs::path{str_dir_app_bin} / "fim_portal";
    ethrow_if(not fs::exists(m_path_file_daemon), "Daemon not found in {}"_fmt(m_path_file_daemon));
    ethrow_if(not fs::exists(m_path_file_guest), "Guest not found in {}"_fmt(m_path_file_guest));

    // Create a portal that uses the reference file to create an unique communication key
    m_process = std::make_unique<ns_subprocess::Subprocess>(m_path_file_daemon);

    // Spawn process to background
    (void) m_process->with_piped_outputs().with_args(path_file_reference).spawn();
  } // Portal

  ~Portal()
  {
    m_process->kill(SIGTERM);
  } // ~Portal()
}; // struct Portal }}}

} // namespace ns_portal

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
