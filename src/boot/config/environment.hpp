///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : environment
///

#pragma once

#include <ranges>
#include <filesystem>

#include "../../cpp/std/functional.hpp"
#include "../../cpp/lib/db.hpp"
#include "../../cpp/lib/subprocess.hpp"

namespace ns_config::ns_environment
{

namespace
{

namespace fs = std::filesystem;

inline std::vector<std::string> keys(std::vector<std::string> const& entries)
{
  auto view = entries
    | std::views::transform([](auto&& e){ return e.substr(0, e.find('=')); });
  return std::vector<std::string>(view.begin(), view.end());
} // keys

inline std::vector<std::string> validate(std::vector<std::string> const& entries)
{
  auto view = entries
    | std::views::filter([](auto&& e){ return std::ranges::count_if(e, [](char c){ return c == '='; }) > 0; });
  return std::vector<std::string>(view.begin(), view.end());
} // validate

} // namespace

inline void del(fs::path const& path_file_config_environment, std::vector<std::string> const& entries)
{
  for(auto&& entry : entries)
  {
    ns_db::Db(path_file_config_environment, ns_db::Mode::UPDATE).array_erase_if(ns_functional::StartsWith(entry + "="));
  } // for
}

inline void set(fs::path const& path_file_config_environment, std::vector<std::string> entries)
{
  entries = validate(entries);
  if ( fs::exists(path_file_config_environment) )
  {
    ns_exception::ignore([&]{ del(path_file_config_environment, keys(entries)); });
  } // if
  ns_db::Db(path_file_config_environment, ns_db::Mode::CREATE).set_insert(entries);
}

inline void add(fs::path const& path_file_config_environment, std::vector<std::string> entries)
{
  entries = validate(entries);
  ns_exception::ignore([&]{ del(path_file_config_environment, keys(entries)); });
  ns_db::Db(path_file_config_environment, ns_db::Mode::UPDATE_OR_CREATE).set_insert(entries);
}

inline std::vector<std::string> get(fs::path const& path_file_config_environment)
{
  std::vector<std::string> environment = ns_db::Db(path_file_config_environment, ns_db::Mode::READ).as_vector();
  // Expand variables
  for(auto& variable : environment)
  {
    if(auto expanded = ns_env::expand(variable))
    {
      variable = *expanded;
    } // if
    else
    {
      ns_log::error()("Failed to expand variable: {}", expanded.error());
    } // else
  } // for
  return environment;
}

} // namespace ns_config::ns_environment


/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
