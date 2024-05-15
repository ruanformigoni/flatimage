///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : portal_host
///

#include <vector>
#include <string>
#include <csignal>
#include <filesystem>
#include <unistd.h>

#include "../cpp/lib/log.hpp"
#include "../cpp/lib/ipc.hpp"
#include "../cpp/macro.hpp"

namespace fs = std::filesystem;

extern char** environ;

bool G_QUIT = false;

// signal_handler() {{{
void signal_handler(int)
{
  G_QUIT = true;
} // signal_handler() }}}

// fork_execve() {{{
// Fork & execve child
void fork_execve(std::vector<std::string>& vec_argv)
{
  // Ignore on empty vec_argv
  if ( vec_argv.empty() ) { return; }

  // Create child
  pid_t pid = fork();

  // Failed to fork
  ereturn_if(pid == -1, "Failed to fork");

  // Is parent
  qreturn_if(pid > 0);

  // Create arguments for execve
  const char **argv_custom = new const char* [vec_argv.size()+1];

  // Set last arg to nullptr
  argv_custom[vec_argv.size()] = nullptr;

  // Copy arguments
  for(size_t i = 0; i < vec_argv.size(); ++i)
  {
    argv_custom[i] = vec_argv[i].c_str();
  } // for

  // Perform execve
  execve(argv_custom[0], (char**) argv_custom, environ);

  // If got here, execve failed
  delete[] argv_custom;

  // Child should stop here
  exit(0);
} // fork_execve() }}}

// search_path() {{{
std::optional<fs::path> search_path(fs::path query)
{
  const char* env_path = assign_and_ereturn_if(std::getenv("PATH")
    , not env_path
    , "PATH environment variable not found"
    , std::nullopt
  );

  const char* env_dir_global_bin = assign_and_ereturn_if(std::getenv("FIM_DIR_GLOBAL_BIN")
    , not env_dir_global_bin
    , "FIM_DIR_GLOBAL_BIN environment variable not found"
    , std::nullopt
  );

  const char* env_dir_static = assign_and_ereturn_if(std::getenv("FIM_DIR_STATIC")
    , not env_dir_global_bin
    , "FIM_DIR_STATIC environment variable not found"
    , std::nullopt
  );

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
    qcontinue_if(path_parent == fs::path(env_dir_static));
    qcontinue_if(path_parent == fs::path(env_dir_global_bin));
    fs::path path_full = path_parent / query;
    ireturn_if(fs::exists(path_full), "Found '{}' in PATH"_fmt(path_full), path_full);
  } // while

  return std::nullopt;
} // search_path() }}}

// main() {{{
int main(int argc, char** argv)
{
  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);

  // Check args
  ereturn_if(argc != 2, "Incorrect number of arguments", EXIT_FAILURE);

  // Create ipc instance
  auto ipc = ns_ipc::Ipc::host(argv[1]);

  std::vector<std::string> argv_custom;

  // Recover messages
  while (not G_QUIT)
  {
    auto opt_msg = ipc.recv();

    ibreak_if(opt_msg == std::nullopt, "Empty message");

    ns_log::info()("Recovered message: {}", *opt_msg);

    ibreak_if(*opt_msg == "IPC_QUIT", "IPC_QUIT");

    if ( *opt_msg == "IPC_ARGV_START" )
    {
      argv_custom.clear();
    } // if
    else if ( *opt_msg == "IPC_ARGV_END" )
    {
      fork_execve(argv_custom);
      argv_custom.clear();
    } // if
    else
    {
      // If is first argument, search in PATH
      if ( argv_custom.empty() )
      {
        opt_msg = search_path(*opt_msg);
      } // if
      // Push to argument list
      argv_custom.push_back(*opt_msg);
    } // else
  } // while

  return EXIT_SUCCESS;
} // main() }}}

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
