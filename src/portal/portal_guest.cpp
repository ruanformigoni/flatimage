///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : portal_guest
///

#include <fcntl.h>
#include <filesystem>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

#include "../cpp/macro.hpp"
#include "../cpp/lib/log.hpp"
#include "../cpp/lib/db.hpp"
#include "../cpp/lib/ipc.hpp"

namespace fs = std::filesystem;

// create_fifo() {{{
std::expected<fs::path, std::string> create_fifo(std::string_view prefix)
{
  fs::path path_file_fifo{"/tmp/fim/fifo/{}_XXXXXX"_fmt(prefix)};

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

// main() {{{
int main(int argc, char** argv)
{
  ns_log::set_level(ns_log::Level::ERROR);

  // Check args
  ereturn_if( argc < 2, "Incorrect number arguments", EXIT_FAILURE);

  // Get file path for IPC
  const char* env = getenv("FIM_FILE_BINARY");
  ereturn_if( env == nullptr, "Could not read FIM_FILE_BINARY", EXIT_FAILURE);

  // Create ipc instance
  auto ipc = ns_ipc::Ipc::guest(env);

  // Create a fifo for stdout, for stderr, and for the exit code
  auto path_file_fifo_stdout = create_fifo("stdout");
  auto path_file_fifo_stderr = create_fifo("stderr");
  auto path_file_fifo_exit = create_fifo("exit");
  qreturn_if(not path_file_fifo_stdout or not path_file_fifo_stderr or not path_file_fifo_exit, EXIT_FAILURE);

  auto db = ns_db::Db("{}");
  std::vector<std::string> vec_args;
  db("command") = std::vector(argv+1, argv+argc);
  db("stdout") = path_file_fifo_stdout->c_str();
  db("stderr") = path_file_fifo_stderr->c_str();
  db("exit") = path_file_fifo_exit->c_str();

  // Open fifos for reading
  auto thread_stdout = std::jthread([=]
  {
    int fd_stdout = open(path_file_fifo_stdout->c_str(), O_RDONLY);
    ereturn_if(fd_stdout == -1, strerror(errno));
    // Read a message from the FIFO
    while(true)
    {
      char buffer[16384];
      ssize_t bytes_read = read(fd_stdout, buffer, sizeof(buffer));
      ereturn_if(bytes_read == -1, "failed to read fifo");
      ireturn_if(bytes_read == 0, "stdout fifo has closed");
      std::cout << buffer;
    } // while
    close(fd_stdout);
  });

  auto thread_stderr = std::jthread([=]
  {
    int fd_stderr = open(path_file_fifo_stderr->c_str(), O_RDONLY);
    ereturn_if(fd_stderr == -1, strerror(errno));
    while(true)
    {
      char buffer[16384];
      ssize_t bytes_read = read(fd_stderr, buffer, sizeof(buffer));
      ereturn_if(bytes_read == -1, "failed to read fifo");
      ireturn_if(bytes_read == 0, "stderr fifo has closed");
      std::cerr << buffer;
    } // while
    close(fd_stderr);
  });

  // Send message
  ipc.send(db.as_string());

  // Read stdout and stderr while program is running
  thread_stdout.join();
  thread_stderr.join();

  // Retrieve exit code
  int fd_exit = open(path_file_fifo_exit->c_str(), O_RDONLY);
  ereturn_if(fd_exit == -1, strerror(errno), EXIT_FAILURE);
  int exit_code;
  ssize_t bytes_read = read(fd_exit, &exit_code, sizeof(exit_code));
  ereturn_if(bytes_read == -1, "failed to read fifo", EXIT_FAILURE);
  ereturn_if(bytes_read != sizeof(exit_code), "Incomplete read from FIFO", EXIT_FAILURE);
  close(fd_exit);
  return exit_code;
} // main() }}}

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
