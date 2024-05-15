///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : subprocess
///

#pragma once

#include <cstring>
#include <sys/wait.h>
#include <vector>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <ranges>
#include <thread>

#include "../macro.hpp"

namespace ns_subprocess
{

// class Subprocess {{{
class Subprocess
{
  private:
    std::string m_program;
    std::vector<std::string> m_args;

  public:
    template<ns_concept::AsString T>
    Subprocess(T&& t)
      : m_program(ns_string::to_string(t))
    {
      // argv0 is program name
      m_args.push_back(m_program);
    } // Subprocess

    template<ns_concept::AsString T>
    Subprocess& with_arg(T&& t)
    {
      this->m_args.push_back(ns_string::to_string(t));
      return *this;
    } // with_arg

    template<ns_concept::IterableConst T>
    Subprocess& with_args(T&& t)
    {
      std::copy(t.begin(), t.end(), std::back_inserter(m_args));
      return *this;
    } // with_arg

    inline void spawn()
    {
      // Log
      ns_log::info("Spawn command: {}", m_args);

      int pipestdout[2];
      int pipestderr[2];

      // Create pipe
      ereturn_if(pipe(pipestdout), strerror(errno));
      ereturn_if(pipe(pipestderr), strerror(errno));

      // Ignore on empty vec_argv
      if ( m_args.empty() ) { return; }

      // Create child
      pid_t pid = fork();

      // Failed to fork
      ereturn_if(pid == -1, "Failed to fork");

      // Is parent
      if ( pid > 0 )
      {
        // Close write end
        ereturn_if(close(pipestdout[1]) == -1, "pipestdout[1]: {}"_fmt(strerror(errno)));
        ereturn_if(close(pipestderr[1]) == -1, "pipestderr[1]: {}"_fmt(strerror(errno)));

        auto thread_stdout = std::thread([=,this]
        {
          char buffer[1024];
          ssize_t count;
          while ((count = read(pipestdout[0], buffer, sizeof(buffer))) != 0)
          {
            ebreak_if(count == -1, "broke parent read loop: {}"_fmt(strerror(errno)));
            // Split newlines and print each line with m_program as a prefix
            for( auto&& i : std::string(buffer, count) | std::views::split('\n'))
            {
              "stdout({}): {}\n"_print(m_program, std::string{i.begin(), i.end()});
            } // for
          }
          close(pipestdout[0]);
        });

        auto thread_stderr = std::thread([=,this]
        {
          char buffer[1024];
          ssize_t count;
          while ((count = read(pipestderr[0], buffer, sizeof(buffer))) != 0)
          {
            ebreak_if(count == -1, "broke parent read loop: {}"_fmt(strerror(errno)));
            // Split newlines and print each line with m_program as a prefix
            for( auto&& i : std::string(buffer, count) | std::views::split('\n'))
            {
              "stderr({}): {}\n"_print(m_program, std::string{i.begin(), i.end()});
            } // for
          }
          close(pipestderr[0]);
        });

        thread_stdout.join();
        thread_stderr.join();

        // Wait for child process to finish
        int status;
        waitpid(pid, &status, 0);
        return;
      } // if

      // Close read end
      ereturn_if(close(pipestdout[0]) == -1, "pipestdout[0]: {}"_fmt(strerror(errno)));
      ereturn_if(close(pipestderr[0]) == -1, "pipestderr[0]: {}"_fmt(strerror(errno)));

      // Make the opened pipe the replace stdout
      ereturn_if(dup2(pipestdout[1], STDOUT_FILENO) == -1, "dup2(pipestdout[1]): {}"_fmt(strerror(errno)));
      ereturn_if(dup2(pipestderr[1], STDERR_FILENO) == -1, "dup2(pipestderr[1]): {}"_fmt(strerror(errno)));

      // Close original write end after duplication
      ereturn_if(close(pipestdout[1]) == -1, "pipestdout[1]: {}"_fmt(strerror(errno)));
      ereturn_if(close(pipestderr[1]) == -1, "pipestderr[1]: {}"_fmt(strerror(errno)));

      // Create arguments for execve
      const char **argv_custom = new const char* [m_args.size()+1];

      // Set last arg to nullptr
      argv_custom[m_args.size()] = nullptr;

      // Copy arguments
      for(size_t i = 0; i < m_args.size(); ++i)
      {
        argv_custom[i] = m_args[i].c_str();
      } // for

      // Perform execve
      execve(m_program.c_str(), (char**) argv_custom, environ);

      // If got here, execve failed
      delete[] argv_custom;

      // Child should stop here
      exit(1);
    } // spawn()
}; // Subprocess }}}

} // namespace ns_subprocess

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
