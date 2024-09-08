///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : portal_host
///

#include <fcntl.h>
#include <sys/wait.h>
#include <thread>
#include <vector>
#include <string>
#include <csignal>
#include <filesystem>
#include <unistd.h>

#include "../cpp/lib/log.hpp"
#include "../cpp/lib/ipc.hpp"
#include "../cpp/lib/db.hpp"
#include "../cpp/lib/env.hpp"
#include "../cpp/macro.hpp"

namespace fs = std::filesystem;

extern char** environ;

bool G_QUIT = false;

// signal_handler() {{{
void signal_handler(int)
{
  G_QUIT = true;
} // signal_handler() }}}

// search_path() {{{
std::optional<fs::path> search_path(fs::path query)
{
  const char* env_path = std::getenv("PATH");
  ereturn_if( env_path == nullptr, "PATH environment variable not found", std::nullopt);

  const char* env_dir_global_bin = std::getenv("FIM_DIR_GLOBAL_BIN");
  const char* env_dir_static = std::getenv("FIM_DIR_STATIC");

  if ( query.is_absolute() )
  {
    return_if_else(fs::exists(query), query, std::nullopt);
  } // if

  std::string path(env_path);
  std::istringstream istream_path(path);
  std::string str_getline;

  while (std::getline(istream_path, str_getline, ':'))
  {
    fs::path path_parent = str_getline;
    qcontinue_if(env_dir_static and path_parent == fs::path(env_dir_static));
    qcontinue_if(env_dir_global_bin and path_parent == fs::path(env_dir_global_bin));
    fs::path path_full = path_parent / query;
    ireturn_if(fs::exists(path_full), "Found '{}' in PATH"_fmt(path_full), path_full);
  } // while

  return std::nullopt;
} // search_path() }}}

// fork_execve() {{{
// Fork & execve child
void fork_execve(std::string msg)
{
  auto db = ns_db::Db(msg);

  // Get command
  std::vector<std::string> vec_argv = db["command"].as_vector();

  // Ignore on empty command
  if ( vec_argv.empty() ) { return; }

  // Create child
  pid_t pid = fork();

  // Failed to fork
  ereturn_if(pid == -1, "Failed to fork");

  // Is parent
  if (pid > 0)
  {
    // Wait for child to finish
    int status;
    int ret_waitpid = waitpid(pid, &status, 0);
    // Check for failures
    ereturn_if(ret_waitpid == -1, "waitpid failed");
    ereturn_if(not WIFEXITED(status), "child did not terminate normally");
    // Get exit code
    int code = WEXITSTATUS(status);
    // Send exit code of child through a fifo
    std::string str_exit = db["exit"];
    int fd_exit = open(str_exit.c_str(), O_WRONLY);
    ereturn_if(fd_exit == -1, "Failed to open exit fifo");
    int ret_write = write(fd_exit, &code, sizeof(code));
    close(fd_exit);
    ereturn_if(ret_write == -1, "Failed to write to exit FIFO");
    return;
  } // if

  std::string str_stdout_fifo = db["stdout"];
  std::string str_stderr_fifo = db["stderr"];

  // Open FIFOs
  int fd_stdout = open(str_stdout_fifo.c_str(), O_WRONLY);
  int fd_stderr = open(str_stderr_fifo.c_str(), O_WRONLY);
  eexit_if(fd_stdout == -1 or fd_stderr == -1, strerror(errno), 1);

  // Redirect stdout and stderr
  eexit_if(dup2(fd_stdout, STDOUT_FILENO) < 0, strerror(errno), 1);
  eexit_if(dup2(fd_stderr, STDERR_FILENO) < 0, strerror(errno), 1);

  // Close the original file descriptors
  close(fd_stdout);
  close(fd_stderr);

  // Search for command in PATH and replace vec_argv[0] with the full path to the binary
  auto opt_path_file_command = search_path(vec_argv[0]);
  eexit_if(not opt_path_file_command, "'{}' not found in PATH"_fmt(vec_argv[0]), 1);
  vec_argv[0] = opt_path_file_command->c_str();

  // Create arguments for execve
  auto argv_custom = std::make_unique<const char*[]>(vec_argv.size()+1);
  // Set last arg to nullptr
  argv_custom[vec_argv.size()] = nullptr;
  // Copy entries
  for(size_t i = 0; i < vec_argv.size(); ++i)
  {
    argv_custom[i] = vec_argv[i].c_str();
  } // for

  // Fetch environment from db
  fs::path path_file_environment = db["environment"].as_string();
  std::vector<std::string> vec_environment;
  std::ifstream file_environment(path_file_environment);
  ereturn_if(not file_environment.is_open(), "Could not open {}"_fmt(path_file_environment));
  for (std::string entry; std::getline(file_environment, entry);)
  {
    vec_environment.push_back(entry);
  } // for
  file_environment.close();
  // Create environment for execve
  auto env_custom = std::make_unique<const char*[]>(vec_environment.size()+1);
  // Set last arg to nullptr
  env_custom[vec_environment.size()] = nullptr;
  // Copy entries
  for(size_t i = 0; i < vec_environment.size(); ++i)
  {
    env_custom[i] = vec_environment[i].c_str();
  } // for

  // Perform execve
  execve(argv_custom[0], (char**) argv_custom.get(), (char**) env_custom.get());

  // Child should stop here
  exit(1);
} // fork_execve() }}}

// validate() {{{
decltype(auto) validate(std::string_view msg) noexcept
{
  try
  {
    auto db = ns_db::Db(msg);
    return db["command"].is_array()
      and db["stdout"].is_string()
      and db["stderr"].is_string()
      and db["exit"].is_string()
      and db["environment"].is_string();
  } // try
  catch(...)
  {
    return false;
  } // catch
} // validate() }}}

// main() {{{
int main(int argc, char** argv)
{
  // Configure logger
  fs::path path_file_log = std::string{ns_env::get_or_throw("FIM_DIR_MOUNT")} + ".portal.daemon.log";
  ns_log::set_sink_file(path_file_log);
  ns_log::set_level((ns_env::exists("FIM_DEBUG", "1"))? ns_log::Level::DEBUG : ns_log::Level::ERROR);

  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);

  // Check args
  ereturn_if(argc != 2, "Incorrect number of arguments", EXIT_FAILURE);

  // Create ipc instance
  auto ipc = ns_ipc::Ipc::host(argv[1]);

  std::vector<std::jthread> commands;

  // Recover messages
  while (not G_QUIT)
  {
    auto opt_msg = ipc.recv();

    ibreak_if(opt_msg == std::nullopt, "Empty message");

    ns_log::info()("Recovered message: {}", *opt_msg);

    econtinue_if(not validate(*opt_msg), "Failed to validate message");

    commands.emplace_back(std::jthread([=]{ fork_execve(*opt_msg); }));
  } // while

  return EXIT_SUCCESS;
} // main() }}}

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
