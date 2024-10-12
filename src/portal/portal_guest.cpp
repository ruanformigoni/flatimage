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
#include "../cpp/lib/fifo.hpp"

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
  ereturn_if(mkfifo(path_file_fifo.c_str(), 0666) < 0, strerror(errno), std::unexpected(strerror(errno)));

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
void fifo_to_ostream(pid_t pid_child, std::ostream& os, fs::path const& path_file_fifo)
{
  using namespace std::chrono_literals;
  // Fork
  pid_t ppid = getpid();
  pid_t pid = fork();
  ereturn_if(pid < 0, "Could not fork '{}'"_fmt(strerror(errno)));
  // Parent ends here
  qreturn_if( pid > 0 );
  // Die with parent
  eabort_if(prctl(PR_SET_PDEATHSIG, SIGKILL) < 0, strerror(errno));
  eabort_if(::kill(ppid, 0) < 0, "Parent died, prctl will not have effect: {}"_fmt(strerror(errno)));
  ns_log::debug()("{} dies with {}", getpid(), ppid);
  // Open fifo to read
  int fd = open(path_file_fifo.string().c_str(), O_RDONLY | O_NONBLOCK);
  ereturn_if(fd == -1, strerror(errno));
  // Send fifo to ostream
  while( fifo_read_nonblock(os, fd) )
  {
    dbreak_if(kill(pid_child, 0) < 0, "Pid has exited");
    std::this_thread::sleep_for(100ms);
  } // for
  // Close fifo
  close(fd);
  // Exit normally
  exit(0);
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

  // Wait for daemon to write pid
  std::this_thread::sleep_for(500ms);

  // Retrieve child pid
  pid_t pid_child;
  auto expected_pid_child = ns_fifo::open_and_read(path_file_fifo_pid->c_str(), std::span(&pid_child, sizeof(pid_child)));
  ereturn_if(not expected_pid_child, expected_pid_child.error(), EXIT_FAILURE);
  ns_log::debug()("Child pid: {}", pid_child);

  // Connect to stdout and stderr with fifos
  fifo_to_ostream(pid_child, std::cout, *path_file_fifo_stdout);
  fifo_to_ostream(pid_child, std::cout, *path_file_fifo_stderr);

  // Open exit code fifo
  int exit_code{};
  auto expected_exit_code = ns_fifo::open_and_read(path_file_fifo_exit->c_str(), std::span(&exit_code, sizeof(exit_code)));
  ereturn_if(not expected_exit_code, expected_exit_code.error(), EXIT_FAILURE);
  return exit_code;
} // main() }}}

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
