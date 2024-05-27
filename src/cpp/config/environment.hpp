///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : environment
///

#pragma once

#include <set>

#include "../enum.hpp"
#include "../setup.hpp"
#include "../std/functional.hpp"

#include "../lib/db.hpp"

namespace ns_config::ns_environment
{

inline void set(ns_setup::FlatimageSetup const& config, std::string const& entry)
{
  ns_db::Db(config.path_file_config_environment, ns_db::Mode::CREATE).array_insert_unique(entry);
}

inline void add(ns_setup::FlatimageSetup const& config, std::string const& entry)
{
  ns_db::Db(config.path_file_config_environment, ns_db::Mode::UPDATE_OR_CREATE).array_insert_unique(entry);
}

inline void del(ns_setup::FlatimageSetup const& config, std::string const& key)
{
  ns_db::Db(config.path_file_config_environment, ns_db::Mode::UPDATE).array_erase_if(key, ns_functional::StartsWith(key + "="));
}

inline std::vector<std::string> get(ns_setup::FlatimageSetup const& config)
{
  auto db = ns_db::Db(config.path_file_config_environment, ns_db::Mode::READ);
  return std::vector<std::string>(db.begin(), db.end());
}

} // namespace ns_config::ns_environment


/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
