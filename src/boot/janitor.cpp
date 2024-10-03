///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : janitor
///

#include <thread>
#include <chrono>
#include <filesystem>
#include <csignal>

#include "../cpp/lib/log.hpp"
#include "../cpp/lib/env.hpp"
#include "../cpp/lib/fuse.hpp"
#include "../cpp/macro.hpp"
#include "../cpp/common.hpp"

namespace fs = std::filesystem;

std::atomic_bool G_CONTINUE(true);

void cleanup(int)
{
  G_CONTINUE = false;
} // cleanup

int main(int argc, char const* argv[])
{
  // Register signal handler
  signal(SIGTERM, cleanup);

  // Initialize logger
  fs::path path_file_log = std::string{ns_env::get_or_throw("FIM_DIR_MOUNT")} + ".janitor.log";
  ns_log::set_sink_file(path_file_log);

  ereturn_if(argc < 2, "Incorrect usage", EXIT_FAILURE);

  pid_t pid_parent = std::stoi(ns_env::get_or_throw("PID_PARENT"));

  // Create a novel session for the child process
  pid_t pid_session = setsid();
  ereturn_if(pid_session < 0, "Failed to create a novel session for janitor", EXIT_FAILURE);
  ns_log::info()("Session id is '{}'", pid_session);

  // Wait for parent process to exit
  while ( kill(pid_parent, 0) == 0 and G_CONTINUE )
  {
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(100ms);
  } // while
  ns_log::info()("Parent process with pid '{}' finished", pid_parent);

  // Cleanup mountpoints
  for (auto&& path_dir_mountpoint : std::vector<fs::path>(argv+1, argv+argc))
  {
    ns_log::info()("Un-mount '{}'", path_dir_mountpoint);
    ns_fuse::unmount(path_dir_mountpoint);
  } // for

  // Exit child
  exit(0);

} // main

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
