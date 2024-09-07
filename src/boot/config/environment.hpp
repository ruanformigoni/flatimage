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
    | std::views::filter([](auto&& e){ return std::ranges::count_if(e, [](char c){ return c == '='; }) == 1; });
  return std::vector<std::string>(view.begin(), view.end());
} // validate

} // namespace

inline void del(fs::path const& path_file_config_environment, std::vector<std::string> const& entries)
{
  for(auto&& entry : entries)
  {
    ns_db::Db(path_file_config_environment, ns_db::Mode::UPDATE).set_erase_if(ns_functional::StartsWith(entry + "="));
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

inline std::expected<std::string, std::string> expand(std::string_view var)
{
  std::string expanded;

  auto opt_path_sh = ns_subprocess::search_path("sh");
  qreturn_if(not opt_path_sh, std::unexpected("Could not find 'dash' binary"));

  auto ret = ns_subprocess::Subprocess(*opt_path_sh)
    .with_piped_outputs()
    .with_stdout_handle([&](auto&& e){ expanded = e; })
    .with_args("-c", "echo {}"_fmt(var))
    .spawn()
    .wait();
  qreturn_if(not ret, std::unexpected("echo subprocess quit abnormally"));
  qreturn_if(*ret != 0, std::unexpected("echo subprocess quit with code '{}'"_fmt(*ret)));

  return expanded;
}

inline std::vector<std::string> get(fs::path const& path_file_config_environment)
{
  std::vector<std::string> environment = ns_db::Db(path_file_config_environment, ns_db::Mode::READ).as_vector();
  // Expand variables
  for(auto& variable : environment)
  {
    if(auto expanded = expand(variable))
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
