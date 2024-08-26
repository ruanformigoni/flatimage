///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : desktop
///

#pragma once

#include <filesystem>

#include "../cpp/std/exception.hpp"
#include "../cpp/std/functional.hpp"
#include "../cpp/lib/db.hpp"
#include "../cpp/lib/subprocess.hpp"
#include "../cpp/lib/image.hpp"
#include "../cpp/lib/env.hpp"
#include "../cpp/macro.hpp"

namespace ns_desktop
{

namespace
{

namespace fs = std::filesystem;

struct Desktop
{
  std::string name;
  fs::path path_file_icon;
  std::vector<std::string> vec_categories;
  Desktop(fs::path const& path_file_json, fs::path const& path_dir_mount_ext2)
  {
    ns_db::from_file(path_file_json, [&](auto&& db)
    {
      name = db["name"];
      std::string str_path_icon = std::string{db["icon"]};
      if ( str_path_icon.starts_with('/') ) { str_path_icon = str_path_icon.substr(1); }
      path_file_icon = path_dir_mount_ext2 / str_path_icon;
      ns_db::Db const& db_categories = db["categories"];
      std::for_each(db_categories.begin(), db_categories.end(), ns_functional::PushBack(vec_categories));
    }, ns_db::Mode::READ);
  } // Desktop
}; // Desktop

// integrate_desktop_entry() {{{
decltype(auto) integrate_desktop_entry(Desktop const& desktop
  , fs::path const& path_dir_home
  , fs::path const& path_file_binary)
{
  // Create path to entry
  fs::path path_file_desktop = path_dir_home / ".local/share/applications/flatimage-{}.desktop"_fmt(desktop.name);

  // Create parent directories for entry
  std::error_code ec_entry;
  fs::create_directories(path_file_desktop.parent_path(), ec_entry);
  ereturn_if(ec_entry, "Failed to create parent directories with code {}"_fmt(ec_entry.value()));

  // Create desktop entry
  std::ofstream file_desktop{path_file_desktop};
  println(file_desktop, "[Desktop Entry]");
  println(file_desktop, "Name={}"_fmt(desktop.name));
  println(file_desktop, "Type=Application");
  println(file_desktop, "Comment=FlatImage distribution of \"{}\""_fmt(desktop.name));
  println(file_desktop, "TryExec={}"_fmt(path_file_binary));
  println(file_desktop, "Exec=\"{}\" %F"_fmt(path_file_binary));
  println(file_desktop, "Icon=application-flatimage_{}"_fmt(desktop.name));
  println(file_desktop, "MimeType=application/flatimage_{}"_fmt(desktop.name));
  println(file_desktop, "Categories={};"_fmt(ns_string::from_container(desktop.vec_categories, ';')));
  file_desktop.close();

} // integrate_desktop_entry() }}}

// integrate_mime_database() {{{
decltype(auto) integrate_mime_database(Desktop const& desktop
  , fs::path const& path_dir_home
  , fs::path const& path_file_binary)
{
  // Create path to mimetype
  fs::path path_entry_mimetype = path_dir_home / ".local/share/mime/packages/flatimage-{}.xml"_fmt(desktop.name);

  // Create parent directories for mime
  std::error_code ec_mime;
  fs::create_directories(path_entry_mimetype.parent_path(), ec_mime);
  ereturn_if(ec_mime, "Failed to create parent directories with code {}"_fmt(ec_mime.value()));

  // Create mimetype file
  bool is_update_mime_database = not fs::exists(path_entry_mimetype);
  std::ofstream file_mimetype{path_entry_mimetype};
  println(file_mimetype, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
  println(file_mimetype, "<mime-info xmlns=\"http://www.freedesktop.org/standards/shared-mime-info\">");
  println(file_mimetype, "  <mime-type type=\"application/flatimage_{}\">"_fmt(desktop.name));
  println(file_mimetype, "    <comment>FlatImage Application</comment>");
  println(file_mimetype, "    <magic>");
  println(file_mimetype, "      <match value=\"ELF\" type=\"string\" offset=\"1\">");
  println(file_mimetype, "        <match value=\"0x46\" type=\"byte\" offset=\"8\">");
  println(file_mimetype, "          <match value=\"0x49\" type=\"byte\" offset=\"9\">");
  println(file_mimetype, "            <match value=\"0x01\" type=\"byte\" offset=\"10\"/>");
  println(file_mimetype, "          </match>");
  println(file_mimetype, "        </match>");
  println(file_mimetype, "      </match>");
  println(file_mimetype, "    </magic>");
  println(file_mimetype, "    <glob pattern=\"{}\"/>"_fmt(path_file_binary.filename()));
  println(file_mimetype, "    <sub-class-of type=\"application/x-executable\"/>");
  println(file_mimetype, "    <generic-icon name=\"application-x-executable\"/>");
  println(file_mimetype, "  </mime-type>");
  println(file_mimetype, "</mime-info>");
  file_mimetype.close();

  // Update mime database
  if ( is_update_mime_database )
  {
    auto opt_mime_database = ns_subprocess::search_path("update-mime-database");
    ereturn_if(not opt_mime_database.has_value(), "Could not find 'update-mime-database'");
    ns_subprocess::Subprocess(*opt_mime_database)
      .with_args(path_dir_home / ".local/share/mime")
      .spawn();
  } // if
} // integrate_mime_database() }}}

// integrate_icons_svg() {{{
void integrate_icons_svg(Desktop const& desktop, fs::path const& path_dir_home)
{
  fs::path const& path_file_icon = desktop.path_file_icon;

    // Path to mimetype icon
    fs::path path_icon_mimetype = path_dir_home
      / ".local/share/icons/hicolor/scalable/mimetypes"
      / "application-flatimage_{}.svg"_fmt(desktop.name);

    // Path to app icon
    fs::path path_icon_app = path_dir_home
      / ".local/share/icons/hicolor/scalable/apps"
      / "application-flatimage_{}.svg"_fmt(desktop.name);

    if (std::error_code e; (fs::copy_file(path_file_icon, path_icon_mimetype, fs::copy_options::skip_existing, e), e) )
    {
      ns_log::error()("Could not copy file '{}' with exit code '{}'", path_icon_mimetype, e.value());
    } // if

    if (std::error_code e; (fs::copy_file(path_file_icon, path_icon_app, fs::copy_options::skip_existing, e), e) )
    {
      ns_log::error()("Could not copy file '{}' with exit code '{}'", path_icon_app, e.value());
    } // if
} // integrate_icons_svg() }}}

// integrate_icons_png() {{{
void integrate_icons_png(Desktop const& desktop, fs::path const& path_dir_home)
{
  fs::path const& path_file_icon = desktop.path_file_icon;

  for(auto&& i : std::array{16,22,24,32,48,64,96,128,256})
  {
    // Path to mimetype icon
    fs::path path_icon_mimetype = path_dir_home
      / ".local/share/icons/hicolor/{}x{}/mimetypes"_fmt(i,i)
      / "application-flatimage_{}.png"_fmt(desktop.name);

    // Path to app icon
    fs::path path_icon_app = path_dir_home
      / ".local/share/icons/hicolor/{}x{}/apps"_fmt(i,i)
      / "application-flatimage_{}.png"_fmt(desktop.name);

    if ( not fs::exists(path_icon_mimetype) )
    {
      auto result = ns_image::resize(path_file_icon, path_icon_mimetype, i, i, true);
      ereturn_if(not result.has_value(), result.error())
    } // if

    if (std::error_code e; (fs::copy_file(path_icon_mimetype, path_icon_app, fs::copy_options::skip_existing, e), e) )
    {
      ns_log::error()("Could not copy file '{}' with exit code '{}'", path_icon_app, e.value());
    } // if
  } // for
} // integrate_icons_png() }}}

// integrate_icons() {{{
void integrate_icons(Desktop const& desktop, fs::path const& path_dir_home)
{
  if ( desktop.path_file_icon.extension().string().ends_with(".svg") )
  {
    integrate_icons_svg(desktop, path_dir_home);
  }
  else
  {
    integrate_icons_png(desktop, path_dir_home);
  } // else
} // integrate_icons() }}}

} // namespace

ENUM(EnableItem, ENTRY, MIMETYPE, ICON);

// integrate() {{{
inline void integrate(fs::path const& path_file_json
  , fs::path const& path_file_binary
  , fs::path const& path_dir_mount_ext2)
{
  // Check if is enabled
  auto expected_enable_item = ns_exception::to_expected([&]
  {
    return ns_db::Db(path_file_json, ns_db::Mode::READ)["enable"].as_vector();
  });
  ethrow_if(not expected_enable_item, expected_enable_item.error());

  // Get expected items
  std::set<EnableItem> set_enable_items;
  std::ranges::transform(*expected_enable_item
    , std::inserter(set_enable_items, set_enable_items.begin())
    , [](auto&& e){ return EnableItem(e); }
  );

  // Get desktop info
  Desktop desktop = Desktop(path_file_json, path_dir_mount_ext2);

  // Get HOME directory
  const char* HOME = ns_env::get("HOME");
  ethrow_if(not HOME, "Environment variable HOME is not set");

  // Create desktop entry
  if(set_enable_items.contains(EnableItem::ENTRY))
  {
    ns_log::info()("Integrating desktop entry...");
    integrate_desktop_entry(desktop, HOME, path_file_binary);
  } // if

  // Create and update mime
  if(set_enable_items.contains(EnableItem::MIMETYPE))
  {
    ns_log::info()("Integrating mime database...");
    integrate_mime_database(desktop, HOME, path_file_binary);
  } // if

  // Create desktop icons
  if(set_enable_items.contains(EnableItem::ICON))
  {
    ns_log::info()("Integrating desktop icons...");
    integrate_icons(desktop, HOME);
  } // if
} // integrate() }}}

// setup() {{{
inline void setup(fs::path const& path_dir_mount_ext2, fs::path const& path_file_json_src, fs::path const& path_file_json_dst)
{
  // Application icon
  fs::path path_file_icon;

  // Validate entries
  ns_db::from_file(path_file_json_src, [&](auto&& db)
  {
    // Validate name field
    ethrow_if(not db["name"].is_string(), "Field 'name' is not a string");
    ns_log::info()("Application name: {}", db["name"]);
    // Validate icon field
    ethrow_if(not db["icon"].is_string(), "Field 'icon' is not a string");
    ns_log::info()("Application icon: {}", db["icon"]);
    path_file_icon = db["icon"];
    ethrow_if( not fs::exists(path_file_icon) or not fs::is_regular_file(path_file_icon)
      , "icon '{}' does not exist or is not a regular file"_fmt(path_file_icon)
    );
    // Validate categories array
    ethrow_if(not db["categories"].is_array(), "Field categories is not an array");
    ns_log::info()("Application categories: {}", db["categories"].as_vector());
  }, ns_db::Mode::READ);

  // Get icon extension
  std::optional<std::string> opt_str_ext = (path_file_icon.extension() == ".svg")? std::make_optional("svg")
    : (path_file_icon.extension() == ".png")? std::make_optional("png")
    : (path_file_icon.extension() == ".jpg" or path_file_icon.extension() == ".jpeg")? std::make_optional("jpeg")
    : std::nullopt;

  // Check if file type is valid
  ethrow_if(not opt_str_ext, "Icon extension '{}' is not supported"_fmt(path_file_icon.extension()));

  // Copy the icon to inside the image
  fs::copy_file(path_file_icon, "{}/fim/desktop/icon.{}"_fmt(path_dir_mount_ext2, *opt_str_ext), fs::copy_options::overwrite_existing);

  // Copy configuration file
  fs::copy_file(path_file_json_src, path_file_json_dst, fs::copy_options::overwrite_existing);

  // Update icon path in the json file
  ns_db::from_file(path_file_json_dst, [&](auto& db){ db("icon") = "/fim/desktop/icon.{}"_fmt(*opt_str_ext); }, ns_db::Mode::UPDATE);

} // setup() }}}

// enable() {{{
inline void enable(fs::path const& path_file_json, std::set<EnableItem> set_enable_items)
{
  std::vector<std::string> vec_enable_items;
  std::ranges::transform(set_enable_items, std::back_inserter(vec_enable_items), [](auto&& e){ return std::string{e}; });
  ns_db::Db(path_file_json, ns_db::Mode::UPDATE_OR_CREATE)("enable") = vec_enable_items;
} // enable() }}}

} // namespace ns_desktop

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
