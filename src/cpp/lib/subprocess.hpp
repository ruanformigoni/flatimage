///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : subprocess
///

#pragma once

#include <cstring>
#include <functional>
#include <sys/wait.h>
#include <vector>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <ranges>
#include <thread>

#include "../macro.hpp"
#include "../std/env.hpp"

namespace ns_subprocess
{

namespace
{

namespace fs = std::filesystem;

} // namespace 

// search_path() {{{
inline std::optional<std::string> search_path(std::string const& s)
{
  const char* cstr_path = ns_env::get("PATH");
  ereturn_if(cstr_path == nullptr, "PATH: Could not read PATH", std::nullopt);

  std::string str_path{cstr_path};
  auto view = str_path | std::views::split(':');
  auto it = std::find_if(view.begin(), view.end(), [&](auto&& e)
  {
    ns_log::debug("PATH: Check for {}", fs::path(e.begin(), e.end()) / s);
    return fs::exists(fs::path(e.begin(), e.end()) / s);
  });

  if (it != view.end())
  {
    auto result = fs::path((*it).begin(), (*it).end()) / s;
    ns_log::debug("PATH: Found '{}'", result);
    return result;
  } // if

  ns_log::debug("PATH: Could not find '{}'", s);
  return std::nullopt;
} // search_path()}}}

// class Subprocess {{{
class Subprocess
{
  private:
    std::string m_program;
    std::vector<std::string> m_args;
    std::vector<std::string> m_env;
    std::optional<std::function<void(std::string)>> m_fstdout;
    std::optional<std::function<void(std::string)>> m_fstderr;

  public:
    template<ns_concept::AsString T>
    Subprocess(T&& t);

    Subprocess& env_clear();

    template<ns_concept::AsString K, ns_concept::AsString V>
    Subprocess& with_var(K&& k, V&& v);

    template<ns_concept::AsString K>
    Subprocess& rm_var(K&& k);

    template<ns_concept::AsString... T>
    Subprocess& with_args(T&&... t);

    template<typename T>
    requires (not ns_concept::AsString<T>) && ns_concept::IterableConst<T>
    Subprocess& with_args(T&& t);

    template<typename F>
    Subprocess& with_stdout_handle(F&& f);

    template<typename F>
    Subprocess& with_stderr_handle(F&& f);

    std::optional<int> spawn(bool wait = false);
}; // Subprocess }}}

// Subprocess::Subprocess {{{
template<ns_concept::AsString T>
Subprocess::Subprocess(T&& t)
  : m_program(ns_string::to_string(t))
{
  // argv0 is program name
  m_args.push_back(m_program);
  // Copy environment
  for(char** i = environ; *i != nullptr; ++i)
  {
    m_env.push_back(*i);
  } // for
} // Subprocess }}}

// env_clear() {{{
inline Subprocess& Subprocess::env_clear()
{
  m_env.clear();
  return *this;
} // env_clear() }}}

// with_var() {{{
template<ns_concept::AsString K, ns_concept::AsString V>
Subprocess& Subprocess::with_var(K&& k, V&& v)
{
  m_env.push_back("{}={}"_fmt(k,v));
  return *this;
} // with_var() }}}

// rm_var() {{{
template<ns_concept::AsString K>
Subprocess& Subprocess::rm_var(K&& k)
{
  auto it = std::ranges::find_if(m_env, [&](std::string const& e){ return e.starts_with(ns_string::to_string(k)); });
  if ( it != std::ranges::end(m_env) ) { m_env.erase(it); }
  return *this;
} // rm_var() }}}

// with_args() {{{
template<ns_concept::AsString... T>
Subprocess& Subprocess::with_args(T&&... t)
{
  (this->m_args.push_back(ns_string::to_string(t)), ...);
  return *this;
} // with_args }}}

// with_args() {{{
template<typename T>
requires (not ns_concept::AsString<T>) && ns_concept::IterableConst<T>
Subprocess& Subprocess::with_args(T&& t)
{
  std::copy(t.begin(), t.end(), std::back_inserter(m_args));
  return *this;
} // with_args }}}

// with_stdout_handle() {{{
template<typename F>
Subprocess& Subprocess::with_stdout_handle(F&& f)
{
  this->m_fstdout = f;
  return *this;
} // with_stdout_handle() }}}

// with_stderr_handle() {{{
template<typename F>
Subprocess& Subprocess::with_stderr_handle(F&& f)
{
  this->m_fstderr = f;
  return *this;
} // with_stderr_handle }}}

// spawn() {{{
inline std::optional<int> Subprocess::spawn(bool wait)
{
  // Log
  ns_log::debug("Spawn command: {}", m_args);

  int pipestdout[2];
  int pipestderr[2];

  // Create pipe
  ereturn_if(pipe(pipestdout), strerror(errno), std::nullopt);
  ereturn_if(pipe(pipestderr), strerror(errno), std::nullopt);

  // Ignore on empty vec_argv
  if ( m_args.empty() ) { return std::nullopt; }

  // Create child
  pid_t pid = fork();

  // Failed to fork
  ereturn_if(pid == -1, "Failed to fork", std::nullopt);

  // Is parent
  if ( pid > 0 )
  {
    // Close write end
    ereturn_if(close(pipestdout[1]) == -1, "pipestdout[1]: {}"_fmt(strerror(errno)), std::nullopt);
    ereturn_if(close(pipestderr[1]) == -1, "pipestderr[1]: {}"_fmt(strerror(errno)), std::nullopt);

    auto f_read_pipe = [this](int id_pipe, std::string_view prefix, auto&& f)
    {
      // Check if 'f' is defined
      if ( not f ) { f = [&](auto&& e) { ns_log::debug("{}({}): {}", prefix, m_program, e); }; }
      // Apply f to incoming data from pipe
      char buffer[1024];
      ssize_t count;
      while ((count = read(id_pipe, buffer, sizeof(buffer))) != 0)
      {
        // Failed to read
        ebreak_if(count == -1, "broke parent read loop: {}"_fmt(strerror(errno)));
        // Split newlines and print each line with prefix
        std::ranges::for_each(std::string(buffer, count)
            | std::views::split('\n')
            | std::views::filter([&](auto&& e){ return not e.empty(); })
          , [&](auto&& e){ (*f)(std::string{e.begin(), e.end()});
        });
      } // while
      close(id_pipe);
    };

    auto thread_stdout = std::thread([=,this] { f_read_pipe(pipestdout[0], "stdout", this->m_fstdout); });
    auto thread_stderr = std::thread([=,this] { f_read_pipe(pipestderr[0], "stderr", this->m_fstderr); });

    thread_stdout.join();
    thread_stderr.join();

    // Wait for child process to finish
    if ( wait )
    {
      int status;
      waitpid(pid, &status, 0);
      return status;
    } // if

    // Does not wait for child to finish
    return std::nullopt;
  } // if

  // Close read end
  ereturn_if(close(pipestdout[0]) == -1, "pipestdout[0]: {}"_fmt(strerror(errno)), std::nullopt);
  ereturn_if(close(pipestderr[0]) == -1, "pipestderr[0]: {}"_fmt(strerror(errno)), std::nullopt);

  // Make the opened pipe the replace stdout
  ereturn_if(dup2(pipestdout[1], STDOUT_FILENO) == -1, "dup2(pipestdout[1]): {}"_fmt(strerror(errno)), std::nullopt);
  ereturn_if(dup2(pipestderr[1], STDERR_FILENO) == -1, "dup2(pipestderr[1]): {}"_fmt(strerror(errno)), std::nullopt);

  // Close original write end after duplication
  ereturn_if(close(pipestdout[1]) == -1, "pipestdout[1]: {}"_fmt(strerror(errno)), std::nullopt);
  ereturn_if(close(pipestderr[1]) == -1, "pipestderr[1]: {}"_fmt(strerror(errno)), std::nullopt);

  // Create arguments for execve
  auto argv_custom = std::make_unique<const char*[]>(m_args.size() + 1);

  // Copy arguments
  std::ranges::transform(m_args, argv_custom.get(), [](auto&& e) { return e.c_str(); });

  // Set last entry to nullptr
  argv_custom[m_args.size()] = nullptr;

  // Create environment for execve
  auto envp_custom = std::make_unique<const char*[]>(m_env.size() + 1);

  // Copy variables
  std::transform(m_env.begin(), m_env.end(), envp_custom.get(), [](auto&& e) { return e.c_str(); });

  // Set last entry to nullptr
  envp_custom[m_env.size()] = nullptr;

  // Perform execve
  execve(m_program.c_str(), (char**) argv_custom.get(), (char**) envp_custom.get());

  // Child should stop here
  exit(1);
} // spawn() }}}

} // namespace ns_subprocess

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
