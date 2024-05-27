///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : permissions
///

#pragma once

#include <set>

#include "../enum.hpp"
#include "../setup.hpp"

#include "../lib/db.hpp"

namespace ns_config::ns_permissions
{

ENUM(Permission,HOME,MEDIA,AUDIO,WAYLAND,XORG,DBUS_USER,DBUS_SYSTEM,UDEV,USB,INPUT,GPU,NETWORK);

template<ns_concept::Iterable R>
inline void set(ns_setup::FlatimageSetup const& config, R&& r)
{
  ns_db::Db(config.path_file_config_permissions, ns_db::Mode::CREATE).insert_if_not_exists(r);
}

template<ns_concept::Iterable R>
inline void add(ns_setup::FlatimageSetup const& config, R&& r)
{
  ns_db::Db(config.path_file_config_permissions, ns_db::Mode::UPDATE_OR_CREATE).insert_if_not_exists(r);
}

template<ns_concept::Iterable R>
inline void del(ns_setup::FlatimageSetup const& config, R&& r)
{
  ns_db::Db(config.path_file_config_permissions, ns_db::Mode::UPDATE).erase(r);
}

inline std::set<Permission> get(ns_setup::FlatimageSetup const& config)
{
  return ns_db::Db(config.path_file_config_permissions, ns_db::Mode::READ).as_set<Permission>();
}

} // namespace ns_config::ns_permissions

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
