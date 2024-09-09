///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : portal_guest
///

#include <csignal>
#include <fcntl.h>
#include <filesystem>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

#include "../cpp/macro.hpp"
#include "../cpp/lib/env.hpp"
#include "../cpp/lib/log.hpp"
#include "../cpp/lib/db.hpp"
#include "../cpp/lib/ipc.hpp"

#define BUFFER_SIZE 16384

extern char** environ;

namespace fs = std::filesystem;

// create_fifo() {{{
std::expected<fs::path, std::string> create_fifo(fs::path const& path_dir_fifo, std::string_view prefix)
{
  ereturn_if(not fs::exists(path_dir_fifo) and not fs::create_directories(path_dir_fifo)
    , strerror(errno)
    , std::unexpected(strerror(errno))
  );
  fs::path path_file_fifo = path_dir_fifo / "{}_XXXXXX"_fmt(prefix);

  // Generate a unique filename using mkstemp
  int fd_temp = mkstemp(const_cast<char*>(path_file_fifo.c_str()));
  ereturn_if(fd_temp == -1, strerror(errno), std::unexpected(strerror(errno)));

  // Close and remove the temporary file created by mkstemp
  close(fd_temp);
  unlink(path_file_fifo.c_str());

  // Create fifo
  ereturn_if(mkfifo(path_file_fifo.c_str(), 0666), strerror(errno), std::unexpected(strerror(errno)));

  return path_file_fifo;
} // create_fifo() }}}

// fifo_read_nonblock() {{{
bool fifo_read_nonblock(std::ostream& os, int fd)
{
  char buffer[BUFFER_SIZE];
  ssize_t count_bytes = read(fd, &buffer, sizeof(buffer));
  if (count_bytes == -1 and (errno == EWOULDBLOCK or errno == EAGAIN))
  {
    return true;
  } // if
  else if (count_bytes == -1)
  {
    perror("read");
    return false;
  } // else if
  else if (count_bytes == 0)
  {
    ns_log::debug()("No data received, the process either finished or did not start");
    return true;
  } // else if
  else
  {
    ns_log::debug()("Read '{}' bytes", count_bytes);
    os << std::string(buffer, buffer+count_bytes);
    return true;
  } // else
} // fifo_read_nonblock() }}}

// fifo_to_ostream() {{{
auto fifo_to_ostream(pid_t pid_child, std::ostream& os, fs::path const& path_file_fifo)
{
  using namespace std::chrono_literals;
  int fd = open(path_file_fifo.string().c_str(), O_RDONLY | O_NONBLOCK);
  ereturn_if(fd == -1, strerror(errno));
  while( fifo_read_nonblock(os, fd) )
  {
    dbreak_if(kill(pid_child, 0) < 0, "Pid has exited");
    std::this_thread::sleep_for(100ms);
  } // for
  close(fd);
} // fifo_to_ostream() }}}

// main() {{{
int main(int argc, char** argv)
{
  using namespace std::chrono_literals;

  // Set log level
  ns_log::set_level(ns_env::exists("FIM_DEBUG", "1")? ns_log::Level::DEBUG : ns_log::Level::QUIET);

  // Check args
  ereturn_if( argc < 2, "Incorrect number arguments", EXIT_FAILURE);

  // Get file path for IPC
  const char* str_file_portal = getenv("FIM_PORTAL_FILE");
  ereturn_if( str_file_portal == nullptr, "Could not read FIM_PORTAL_FILE", EXIT_FAILURE);

  // Create ipc instance
  auto ipc = ns_ipc::Ipc::guest(str_file_portal);

  // Mount dir
  const char* str_dir_mount = getenv("FIM_DIR_MOUNT");
  ereturn_if( str_dir_mount == nullptr, "Could not read FIM_DIR_MOUNT", EXIT_FAILURE);
  fs::path path_dir_mount(str_dir_mount);

  // Set log file
  fs::path path_file_log = path_dir_mount / "portal" / "logs" / std::to_string(getpid());
  std::error_code ec;
  fs::create_directories(path_file_log.parent_path(), ec);
  if(ec) { ns_log::error()("Error to create log file: {}", ec.message()); }
  ns_log::set_sink_file(path_file_log);

  // Create a fifo for stdout, for stderr, and for the exit code
  auto path_file_fifo_stdout = create_fifo(path_dir_mount / "portal" / "fifo", "stdout");
  auto path_file_fifo_stderr = create_fifo(path_dir_mount / "portal" / "fifo", "stderr");
  auto path_file_fifo_exit = create_fifo(path_dir_mount / "portal" / "fifo", "exit");
  auto path_file_fifo_pid = create_fifo(path_dir_mount / "portal" / "fifo", "pid");
  ereturn_if(not path_file_fifo_stdout, "Could not open stdout fifo", EXIT_FAILURE);
  ereturn_if(not path_file_fifo_stderr, "Could not open stderr fifo", EXIT_FAILURE);
  ereturn_if(not path_file_fifo_exit, "Could not open exit fifo", EXIT_FAILURE);
  ereturn_if(not path_file_fifo_pid, "Could not open pid fifo", EXIT_FAILURE);

  // Save environment
  fs::path path_file_env = fs::path{str_dir_mount} / "portal" / "environments" / std::to_string(getpid());
  fs::create_directories(path_file_env.parent_path(), ec);
  ereturn_if(ec, "Could not open file '{}': '{}'"_fmt(path_file_env, ec.message()), EXIT_FAILURE);
  std::ofstream ofile_env(path_file_env);
  ereturn_if(not ofile_env.good(), "Could not open file '{}'"_fmt(path_file_env), EXIT_FAILURE);
  for(char **env = environ; *env != NULL; ++env)
  {
    ofile_env << *env << '\n';
  } // for
  ofile_env.close();

  // Send message
  auto db = ns_db::Db("{}");
  db("command") = std::vector(argv+1, argv+argc);
  db("stdout") = path_file_fifo_stdout->c_str();
  db("stderr") = path_file_fifo_stderr->c_str();
  db("exit") = path_file_fifo_exit->c_str();
  db("pid") = path_file_fifo_pid->c_str();
  db("environment") = path_file_env;
  ipc.send(db.dump());

  // Retrieve child pid
  pid_t pid_child;
  int fd_pid = open(path_file_fifo_pid->c_str(), O_RDONLY);
  ssize_t count_bytes = read(fd_pid, &pid_child, sizeof(pid_child));
  ereturn_if(count_bytes == 0, "Could not read pid for child process", EXIT_FAILURE);
  ereturn_if(count_bytes < 0, "Could not read pid for child process: '{}'"_fmt(strerror(errno)), EXIT_FAILURE);
  ns_log::debug()("Child pid: {}", pid_child);

  // Open exit code fifo
  int fd_exit = open(path_file_fifo_exit->c_str(), O_RDONLY | O_NONBLOCK);

  // Connect to stdout and stderr with fifos
  auto thread_stdout = std::jthread([=]{ fifo_to_ostream(pid_child, std::cout, *path_file_fifo_stdout); });
  auto thread_stderr = std::jthread([=]{ fifo_to_ostream(pid_child, std::cout, *path_file_fifo_stderr); });

  // Wait for daemon to write pid
  std::this_thread::sleep_for(500ms);

  // Retrieve exit code, process finished here
  ereturn_if(fd_exit < 0, strerror(errno), EXIT_FAILURE);
  int exit_code;
  ssize_t bytes_read = read(fd_exit, &exit_code, sizeof(exit_code));
  ereturn_if(bytes_read == 0, "Could not read exit code: '{}'"_fmt(strerror(errno)), EXIT_FAILURE);
  ereturn_if(bytes_read < 0, "failed to read fifo: '{}'"_fmt(strerror(errno)), EXIT_FAILURE);

  // Close exit code fifo
  return exit_code;
} // main() }}}

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
