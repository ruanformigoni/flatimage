///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : desktop
///

#pragma once

#include <string>
#include "../db.hpp"
#include "../../std/string.hpp"

namespace ns_db::ns_desktop
{

ENUM(IntegrationItem, ENTRY, MIMETYPE, ICON);

// struct Desktop {{{
struct Desktop
{
  private:
    std::string m_name;
    fs::path m_path_file_icon;
    std::set<IntegrationItem> m_set_integrations;
    std::set<std::string> m_set_categories;
    Desktop(std::string_view raw_json)
    {
      // Open DB
      auto db = ns_db::Db(raw_json);
      // Parse name (required)
      m_name = db["name"];
      // Parse enabled integrations (optional)
      if (db.contains("integrations"))
      {
        std::ranges::for_each(db["integrations"].values(), [&](auto&& e){ m_set_integrations.insert(e); });
      } // if
      // Parse icon path (required)
      m_path_file_icon = db["icon"];
      // Parse categories (required)
      ns_db::Db const& db_categories = db["categories"];
      std::ranges::for_each(db_categories.values(), [&](auto&& e){ m_set_categories.insert(e); });
    } // Desktop
  public:
    std::string const& get_name() const { return m_name; }
    fs::path const& get_path_file_icon() const { return m_path_file_icon; }
    std::set<IntegrationItem> const& get_integrations() const { return m_set_integrations; }
    std::set<std::string> const& get_categories() const { return m_set_categories; }
    void set_name(std::string_view str_name) { m_name = str_name; }
    void set_integrations(std::set<IntegrationItem> const& set_integrations) { m_set_integrations = set_integrations; }
    void set_categories(std::set<std::string> const& set_categories) { m_set_categories = set_categories; }
  friend std::expected<Desktop,std::string> deserialize(std::string_view raw_json) noexcept;
  friend std::expected<Desktop,std::string> deserialize(std::ifstream& stream_raw_json) noexcept;
  friend std::expected<std::string,std::string> serialize(Desktop const& desktop) noexcept;
}; // Desktop }}}

// deserialize() {{{
inline std::expected<Desktop,std::string> deserialize(std::string_view str_raw_json) noexcept
{
  return ns_exception::to_expected([&]{ return Desktop(str_raw_json); });
}
// deserialize() }}}

// deserialize() {{{
inline std::expected<Desktop,std::string> deserialize(std::ifstream& stream_raw_json) noexcept
{
  std::stringstream ss;
  ss << stream_raw_json.rdbuf();
  return ns_exception::to_expected([&] { return Desktop(ss.str()); });
}
// deserialize() }}}

// serialize() {{{
inline std::expected<std::string,std::string> serialize(Desktop const& desktop) noexcept
{
  return ns_exception::to_expected([&]
  {
    auto db = ns_db::Db("{}");
    db("name") = desktop.m_name;
    db("integrations") = desktop.m_set_integrations;
    db("icon") = desktop.m_path_file_icon;
    db("categories") = desktop.m_set_categories;
    return db.dump();
  });
} // serialize() }}}

} // namespace ns_db::ns_desktop

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
