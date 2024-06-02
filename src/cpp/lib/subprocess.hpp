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

#include "../lib/env.hpp"
#include "../macro.hpp"
#include "../std/vector.hpp"

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
    ns_log::debug()("PATH: Check for {}", fs::path(e.begin(), e.end()) / s);
    return fs::exists(fs::path(e.begin(), e.end()) / s);
  });

  if (it != view.end())
  {
    auto result = fs::path((*it).begin(), (*it).end()) / s;
    ns_log::debug()("PATH: Found '{}'", result);
    return result;
  } // if

  ns_log::debug()("PATH: Could not find '{}'", s);
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
    bool m_with_piped_outputs;

    std::optional<int> with_pipes_parent(pid_t pid, int pipestdout[2], int pipestderr[2]);
    void with_pipes_child(int pipestdout[2], int pipestderr[2]);
  public:
    template<ns_concept::StringRepresentable T>
    Subprocess(T&& t);

    Subprocess& env_clear();

    template<ns_concept::StringRepresentable K, ns_concept::StringRepresentable V>
    Subprocess& with_var(K&& k, V&& v);

    template<ns_concept::StringRepresentable K>
    Subprocess& rm_var(K&& k);

    template<typename... T>
    requires (sizeof...(T) > 1)
    Subprocess& with_args(T&&... t);

    template<typename T>
    Subprocess& with_args(T&& t);

    template<typename... T>
    requires (sizeof...(T) > 1)
    Subprocess& with_env(T&&... t);

    template<typename T>
    Subprocess& with_env(T&& t);

    Subprocess& with_piped_outputs();

    template<typename F>
    Subprocess& with_stdout_handle(F&& f);

    template<typename F>
    Subprocess& with_stderr_handle(F&& f);

    std::optional<int> spawn();
}; // Subprocess }}}

// Subprocess::Subprocess {{{
template<ns_concept::StringRepresentable T>
Subprocess::Subprocess(T&& t)
  : m_program(ns_string::to_string(t))
  , m_with_piped_outputs(false)
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
template<ns_concept::StringRepresentable K, ns_concept::StringRepresentable V>
Subprocess& Subprocess::with_var(K&& k, V&& v)
{
  rm_var(k);
  m_env.push_back("{}={}"_fmt(k,v));
  return *this;
} // with_var() }}}

// rm_var() {{{
template<ns_concept::StringRepresentable K>
Subprocess& Subprocess::rm_var(K&& k)
{
  // Find variable
  auto it = std::ranges::find_if(m_env, [&](std::string const& e)
  {
    auto vec = ns_vector::from_string(e, '=');
    qreturn_if(vec.empty(), false);
    return vec.front() == k;
  });

  // Erase if found
  if ( it != std::ranges::end(m_env) )
  {
    ns_log::debug()("Erased var entry: {}", *it);
    m_env.erase(it);
  } // if

  return *this;
} // rm_var() }}}

// with_args() {{{
template<typename... T>
requires (sizeof...(T) > 1)
Subprocess& Subprocess::with_args(T&&... t)
{
  return ( with_args(std::forward<T>(t)), ... );
} // with_args }}}

// with_args() {{{
template<typename T>
Subprocess& Subprocess::with_args(T&& t)
{
  if constexpr ( ns_concept::SameAs<T, std::string> )
  {
    this->m_args.push_back(std::forward<T>(t));
  } // if
  else if constexpr ( ns_concept::IterableConst<T> )
  {
    std::copy(t.begin(), t.end(), std::back_inserter(m_args));
  } // else if
  else if constexpr ( ns_concept::StringRepresentable<T> )
  {
    this->m_args.push_back(ns_string::to_string(std::forward<T>(t)));
  } // else if
  else
  {
    static_assert(false, "Could not determine argument type");
  } // else

  return *this;
} // with_args }}}

// with_env() {{{
template<typename... T>
requires (sizeof...(T) > 1)
Subprocess& Subprocess::with_env(T&&... t)
{
  return ( with_env(std::forward<T>(t)), ... );
} // with_env }}}

// with_env() {{{
template<typename T>
Subprocess& Subprocess::with_env(T&& t)
{
  if constexpr ( ns_concept::SameAs<T, std::string> )
  {
    this->m_env.push_back(std::forward<T>(t));
  } // if
  else if constexpr ( ns_concept::IterableConst<T> )
  {
    std::copy(t.begin(), t.end(), std::back_inserter(m_env));
  } // else if
  else if constexpr ( ns_concept::StringRepresentable<T> )
  {
    this->m_env.push_back(ns_string::to_string(std::forward<T>(t)));
  } // else if
  else
  {
    static_assert(false, "Could not determine argument type");
  } // else

  return *this;
} // with_env }}}

// with_piped_outputs() {{{
inline Subprocess& Subprocess::with_piped_outputs()
{
  m_with_piped_outputs = true;
  return *this;
} // with_piped_outputs() }}}

// with_pipes_parent() {{{
inline std::optional<int> Subprocess::with_pipes_parent(pid_t pid, int pipestdout[2], int pipestderr[2])
{
  // Close write end
  ereturn_if(close(pipestdout[1]) == -1, "pipestdout[1]: {}"_fmt(strerror(errno)), std::nullopt);
  ereturn_if(close(pipestderr[1]) == -1, "pipestderr[1]: {}"_fmt(strerror(errno)), std::nullopt);

  auto f_read_pipe = [this](int id_pipe, std::string_view prefix, auto&& f)
  {
    // Check if 'f' is defined
    if ( not f ) { f = [&](auto&& e) { ns_log::debug()("{}({}): {}", prefix, m_program, e); }; }
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

  auto thread_stdout = std::jthread([=,this] { f_read_pipe(pipestdout[0], "stdout", this->m_fstdout); });
  auto thread_stderr = std::jthread([=,this] { f_read_pipe(pipestderr[0], "stderr", this->m_fstderr); });

  // Wait for child process to finish
  int status;
  waitpid(pid, &status, 0);
  return status;
} // with_pipes_parent() }}}

// with_pipes_child() {{{
inline void Subprocess::with_pipes_child(int pipestdout[2], int pipestderr[2])
{
  // Close read end
  ereturn_if(close(pipestdout[0]) == -1, "pipestdout[0]: {}"_fmt(strerror(errno)));
  ereturn_if(close(pipestderr[0]) == -1, "pipestderr[0]: {}"_fmt(strerror(errno)));

  // Make the opened pipe the replace stdout
  ereturn_if(dup2(pipestdout[1], STDOUT_FILENO) == -1, "dup2(pipestdout[1]): {}"_fmt(strerror(errno)));
  ereturn_if(dup2(pipestderr[1], STDERR_FILENO) == -1, "dup2(pipestderr[1]): {}"_fmt(strerror(errno)));

  // Close original write end after duplication
  ereturn_if(close(pipestdout[1]) == -1, "pipestdout[1]: {}"_fmt(strerror(errno)));
  ereturn_if(close(pipestderr[1]) == -1, "pipestderr[1]: {}"_fmt(strerror(errno)));
} // with_pipes_child() }}}

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
inline std::optional<int> Subprocess::spawn()
{
  // Log
  ns_log::debug()("Spawn command: {}", m_args);

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

  // Setup pipe on child and parent
  // On parent, return exit code of child
  if ( m_with_piped_outputs && pid > 0)
  {
    // This is blocking, waits for child to exit
    return with_pipes_parent(pid, pipestdout, pipestderr);
  } // if

  // On child, just setup the pipe
  if ( m_with_piped_outputs && pid == 0)
  {
    // this is non-blocking, setup pipes and perform execve afterwards
    with_pipes_child(pipestdout, pipestderr);
  } // else

  // No custom pipe, wait for child to exit
  if ( pid > 0 )
  {
    // Wait for child process to finish
    int status;
    waitpid(pid, &status, 0);
    return status;
  } // if

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

  // Log error
  ns_log::error()("execve() failed: ", strerror(errno));

  // Child should stop here
  exit(1);
} // spawn() }}}

} // namespace ns_subprocess

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
