///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : log
///

#pragma once

#include <format>
#include <iostream>
#include <filesystem>
#include <fstream>

#include "../std/concepts.hpp"
#include "../common.hpp"

namespace ns_log
{

namespace fs = std::filesystem;

class Logger
{
  private:
    fs::path m_file;
    std::ofstream m_os;
  public:
    template<ns_concept::AsString T>
    Logger(T&& t);
};

template<ns_concept::AsString T>
Logger::Logger(T&& t)
  : m_file(ns_string::to_string(std::forward<T>(t)))
{
  m_os = std::ofstream{m_file};
  throw_if(m_os.bad(), "Could not open file '{}'"_fmt(m_file));
} // Logger

template<ns_concept::AsString T, typename... Args>
void info(T&& t, Args&&... args)
{
  print(std::forward<T>(t), std::forward<Args>(args)...);
}

template<ns_concept::AsString T, typename... Args>
void err(T&& t, Args&&... args)
{
  print(std::forward<T>(t), std::forward<Args>(args)...);
}

} // namespace ns_log

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
