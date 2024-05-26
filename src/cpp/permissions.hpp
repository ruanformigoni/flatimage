///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : permissions
///

#pragma once

#include <set>

#include "enum.hpp"
#include "config.hpp"

#include "lib/db.hpp"

namespace ns_permissions
{

ENUM(Permission,HOME,MEDIA,AUDIO,WAYLAND,XORG,DBUS_USER,DBUS_SYSTEM,UDEV,USB,INPUT,GPU,NETWORK);

template<ns_concept::Iterable R>
inline void set(ns_config::FlatimageConfig const& config, R&& r)
{
  ns_db::from_file(config.path_file_config_perms, [&](ns_db::Db& db)
  {
    std::ranges::for_each(r, [&](auto&& e){ db.insert_if_not_exists(e); });
  }, ns_db::Mode::CREATE);
}

template<ns_concept::Iterable R>
inline void add(ns_config::FlatimageConfig const& config, R&& r)
{
  ns_db::from_file(config.path_file_config_perms, [&](ns_db::Db& db)
  {
    std::ranges::for_each(r, [&](auto&& e){ db.insert_if_not_exists(e); });
  }, ns_db::Mode::UPDATE_OR_CREATE);
}

template<ns_concept::Iterable R>
inline void del(ns_config::FlatimageConfig const& config, R&& r)
{
  ns_db::from_file(config.path_file_config_perms, [&](ns_db::Db& db)
  {
    std::ranges::for_each(r, [&](auto&& e){ db.erase(e); });
  }, ns_db::Mode::UPDATE);
}

inline std::set<Permission> get(ns_config::FlatimageConfig const& config)
{
  std::set<Permission> permissions;
  ns_db::from_file(config.path_file_config_perms, [&](ns_db::Db& db)
  {
    std::for_each(db.cbegin(), db.cend(), [&](auto&& e){ permissions.insert(Permission(std::string{e})); });
  }, ns_db::Mode::READ);
  return permissions;
}

} // namespace ns_permissions

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
