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
  ns_exception::ignore([&]{ del(path_file_config_environment, keys(entries)); });
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
    auto opt_path_dash = ns_subprocess::search_path("sh");
    ebreak_if(not opt_path_dash, "Could not find 'dash' binary");
    auto ret = ns_subprocess::Subprocess(*opt_path_dash)
      .with_piped_outputs()
      .with_stdout_handle([&](auto&& e){ variable = e; })
      .with_args("-c", "echo {}"_fmt(variable))
      .spawn()
      .wait();
    econtinue_if(not ret, "echo subprocess quit abnormally");
    econtinue_if(*ret != 0, "echo subprocess quit with code '{}'"_fmt(*ret));
  } // for
  return environment;
}

} // namespace ns_config::ns_environment


/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
