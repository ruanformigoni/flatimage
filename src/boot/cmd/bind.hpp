///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : bind
///@created     : Wednesday Sep 11, 2024 15:00:09 -03
///

#pragma once

#include <filesystem>
#include "../../cpp/lib/db.hpp"
#include "../../cpp/std/variant.hpp"

namespace ns_cmd::ns_bind
{
ENUM(CmdBindOp,ADD,DEL,LIST);
ENUM(CmdBindType,RO,RW,DEV);

using index_t = uint64_t;
using bind_t = struct { CmdBindType type; std::string src; std::string dst; };
using data_t = std::variant<index_t,bind_t,std::false_type>;

struct CmdBind
{
  ns_cmd::ns_bind::CmdBindOp op;
  data_t data;
};

namespace
{

namespace fs = std::filesystem;

// fn: get_highest_index() {{{
inline index_t get_highest_index(ns_db::Db const& db)
{
  auto bindings = db.items()
    | std::views::transform([](auto&& e){ return e.key(); })
    | std::views::transform([](auto&& e){ return std::stoi(e); });
  auto it = std::max_element(bindings.begin()
    , bindings.end()
    , [](auto&& a, auto&& b){ return a < b; }
  );
  return ( it == bindings.end() )? 0 : *it;
} // fn: get_highest_index() }}}

} // namespace

// fn: add() {{{
inline void add(fs::path const& path_file_config, CmdBind const& cmd)
{
  // Get src and dst paths
  auto binds = ns_variant::get_if_holds_alternative<bind_t>(cmd.data);
  ereturn_if(not binds, "Invalid data type for 'add' command");

  // Find out the highest bind index
  auto db = ns_db::Db(path_file_config, ns_db::Mode::UPDATE_OR_CREATE);
  index_t idx = get_highest_index(db) + 1;
  ns_log::info()("Binding index is '{}'", idx);

  // Include bindings
  db(idx)("type") = (binds->type == CmdBindType::RO)? "ro"
    : (binds->type == CmdBindType::RW)? "rw"
    : "dev";
  db(idx)("src") = binds->src;
  db(idx)("dst") =  binds->dst;
} // fn: add() }}}

// fn: del() {{{
inline decltype(auto) del(fs::path const& path_file_config, CmdBind const& cmd)
{
  // Get index to delete
  auto index = ns_variant::get_if_holds_alternative<index_t>(cmd.data);
  ereturn_if(not index, "Invalid data type for 'del' command");

  // Open db
  auto db = ns_db::Db(path_file_config, ns_db::Mode::UPDATE_OR_CREATE);

  // Check if it exists
  auto items = db.items();
  auto it = std::ranges::find_if(items, [&](auto&& e){ return std::stoi(e.key()) == index; });
  ereturn_if(it == items.end(), "Specified index does not exist");

  // Erase value by overwriting
  while( std::next(it) != items.end() )
  {
    it.value() = std::next(it).value();
    ++it;
  } // while

  // Erase last index
  db.obj_erase(get_highest_index(db));
} // fn: del() }}}

// fn: list() {{{
inline decltype(auto) list(fs::path const& path_file_config)
{
  // Open db
  auto db = ns_db::Db(path_file_config, ns_db::Mode::UPDATE_OR_CREATE);
  // Print entries to stdout
  println(db.dump(2));
} // fn: list() }}}

} // namespace ns_cmd::ns_bind

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
