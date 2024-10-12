///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : fifo
///

#pragma once

#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <unistd.h>

#include "log.hpp"
#include "../macro.hpp"

namespace ns_fifo
{

namespace
{

namespace fs = std::filesystem;

} // namespace

template<typename T>
[[nodiscard]] std::expected<int,std::string> open_and_write(fs::path const& path_file_fifo, std::span<T> data)
{
  // Open fifo or return error
  int fd_pid = open(path_file_fifo.c_str(), O_WRONLY);
  qreturn_if (fd_pid < 0, std::unexpected("Could not open fifo: '{}'"_fmt(strerror(errno))));

  // Close file & return error on write fail
  auto count_bytes = write(fd_pid, data.data(), data.size());
  qreturn_if (count_bytes <= 0, (close(fd_pid), std::unexpected("Could not write pid to fifo: '{}'"_fmt(strerror(errno)))));

  // Close fifo and return no error
  close(fd_pid);
  return count_bytes;
} // function: open_and_read

template<typename T>
[[nodiscard]] std::expected<int,std::string> open_and_read(fs::path const& path_file_fifo, std::span<T> data)
{
  // Open fifo or return error
  int fd_pid = open(path_file_fifo.c_str(), O_RDONLY);
  qreturn_if (fd_pid < 0, std::unexpected("Could not open fifo: '{}'"_fmt(strerror(errno))));

  // Close file & return error on write fail
  int count_bytes = read(fd_pid, data.data(), data.size());
  qreturn_if (count_bytes <= 0, (close(fd_pid), std::unexpected("Could not write pid to fifo: '{}'"_fmt(strerror(errno)))));

  // Close fifo and return no error
  close(fd_pid);
  return count_bytes;
} // function: open_and_read

} // namespace ns_fifo

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
