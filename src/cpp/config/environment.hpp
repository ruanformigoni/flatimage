///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : environment
///

#pragma once

#include <set>
#include <ranges>

#include "../enum.hpp"
#include "../setup.hpp"
#include "../std/functional.hpp"

#include "../lib/db.hpp"

namespace ns_config::ns_environment
{

namespace
{

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

inline void del(ns_setup::FlatimageSetup const& config, std::vector<std::string> const& entries)
{
  for(auto&& entry : entries)
  {
    ns_db::Db(config.path_file_config_environment, ns_db::Mode::UPDATE).set_erase_if(ns_functional::StartsWith(entry + "="));
  } // for
}

inline void set(ns_setup::FlatimageSetup const& config, std::vector<std::string> entries)
{
  entries = validate(entries);
  del(config, keys(entries));
  ns_db::Db(config.path_file_config_environment, ns_db::Mode::CREATE).set_insert(entries);
}

inline void add(ns_setup::FlatimageSetup const& config, std::vector<std::string> entries)
{
  entries = validate(entries);
  del(config, keys(entries));
  ns_db::Db(config.path_file_config_environment, ns_db::Mode::UPDATE_OR_CREATE).set_insert(entries);
}

inline std::vector<std::string> get(ns_setup::FlatimageSetup const& config)
{
  return ns_db::Db(config.path_file_config_environment, ns_db::Mode::READ).as_vector();
}

} // namespace ns_config::ns_environment


/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
