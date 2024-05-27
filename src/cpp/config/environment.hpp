///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : environment
///

#pragma once

#include <set>

#include "../enum.hpp"
#include "../setup.hpp"

#include "../lib/db.hpp"

namespace ns_config::ns_environment
{

struct Entry
{
  std::string key;
  std::string value;
};

inline void add(ns_setup::FlatimageSetup const& config, Entry const& entry)
{
  ns_db::Db(config.path_file_config_environment, ns_db::Mode::UPDATE_OR_CREATE)(entry.key) = entry.value;
}

inline void del(ns_setup::FlatimageSetup const& config, std::string const& key)
{
  ns_db::Db(config.path_file_config_environment, ns_db::Mode::UPDATE).erase(key);
}

inline std::vector<Entry> get(ns_setup::FlatimageSetup const& config)
{
  std::vector<Entry> ret;
  for ( auto&& [key,value] : ns_db::Db(config.path_file_config_environment, ns_db::Mode::READ).items())
  {
    ret.emplace_back(key,value);
  } // for
  return ret;
}

} // namespace ns_config::ns_environment


/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
