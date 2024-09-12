///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : desktop
///

#pragma once

#include <filesystem>

#include "../../cpp/std/exception.hpp"
#include "../../cpp/std/functional.hpp"
#include "../../cpp/lib/db.hpp"
#include "../../cpp/lib/subprocess.hpp"
#include "../../cpp/lib/image.hpp"
#include "../../cpp/lib/ext2/size.hpp"
#include "../../cpp/lib/env.hpp"
#include "../../cpp/macro.hpp"
#include "../filesystems.hpp"

namespace ns_desktop
{

namespace
{

namespace fs = std::filesystem;

// struct Desktop {{{
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
}; // Desktop }}}

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
    auto ret = ns_subprocess::Subprocess(*opt_mime_database)
      .with_args(path_dir_home / ".local/share/mime")
      .spawn()
      .wait();
    if ( not ret ) { ns_log::error()("update-mime-database was signalled"); }
    if ( *ret != 0 ) { ns_log::error()("update-mime-database exited with non-zero exit code '{}'", *ret); }
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

// integrate_bash() {{{
void integrate_bash(fs::path const& path_dir_home)
{
  fs::path path_file_bashrc = path_dir_home / ".bashrc";

  // If a backup was already made, then the integration process was completed
  fs::path path_file_bashrc_backup = path_dir_home / ".bashrc.flatimage.bak";
  dreturn_if(fs::exists(path_file_bashrc_backup), "FlatImage backup exists in {}"_fmt(path_file_bashrc_backup));

  // Location where flatimage stores desktop entries, icons, and mimetypes.
  fs::path path_dir_data = path_dir_home / ".local" / "share";

  // Check if XDG_DATA_HOME contain ~/.local/share
  if (const char * str_xdg_data_home = ns_env::get("XDG_DATA_HOME"); str_xdg_data_home != nullptr)
  {
    dreturn_if(fs::path(str_xdg_data_home) == path_dir_data, "Found '{}' in XDG_DATA_HOME"_fmt(path_dir_data));
  } // if

  // Check if XDG_DATA_DIRS contain ~/.local/share
  if (const char * str_xdg_data_dirs = ns_env::get("XDG_DATA_DIRS"); str_xdg_data_dirs != nullptr)
  {
    auto vec_path_dirs = ns_vector::from_string<std::vector<fs::path>>(str_xdg_data_dirs, ':');
    auto search = std::ranges::find(vec_path_dirs, path_dir_data, [](fs::path const& e){ return fs::canonical(e); });
    dreturn_if(search != std::ranges::end(vec_path_dirs), "Found '{}' in XDG_DATA_DIRS"_fmt(path_dir_data));
  } // if

  // Backup
  fs::copy_file(path_file_bashrc, path_file_bashrc_backup);
  ns_log::info()("Saved a backup of ~/.bashrc in '{}'", path_file_bashrc_backup);

  // Integrate
  std::ofstream of_bashrc{path_file_bashrc, std::ios::app};
  of_bashrc << "export XDG_DATA_DIRS=\"$HOME/.local/share:$XDG_DATA_DIRS\"";
  of_bashrc.close();
  ns_log::info()("Modified XDG_DATA_DIRS in ~/.bashrc");
} // integrate_bash }}}

} // namespace

ENUM(EnableItem, ENTRY, MIMETYPE, ICON);

// integrate() {{{
inline void integrate(ns_config::FlatimageConfig const& config)
{
  // Mount filesystem
  [[maybe_unused]] auto mount = ns_filesystems::Filesystems(config, ns_filesystems::Filesystems::FilesystemsLayer::EXT_RO);

  // Check if is enabled
  auto expected_enable_item = ns_exception::to_expected([&]
  {
    return ns_db::Db(config.path_file_config_desktop, ns_db::Mode::READ)["enable"].as_vector();
  });
  ethrow_if(not expected_enable_item, expected_enable_item.error());

  // Get expected items
  std::set<EnableItem> set_enable_items;
  std::ranges::transform(*expected_enable_item
    , std::inserter(set_enable_items, set_enable_items.begin())
    , [](auto&& e){ return EnableItem(e); }
  );

  // Get desktop info
  Desktop desktop = Desktop(config.path_file_config_desktop, config.path_dir_mount_ext);

  // Get HOME directory
  const char* cstr_home = ns_env::get("HOME");
  ethrow_if(not cstr_home, "Environment variable HOME is not set");

  // Check if XDG_DATA_DIRS contains ~/.local/bin
  const char* cstr_shell = ns_env::get("SHELL");
  if ( cstr_shell )
  {
    std::string str_shell{cstr_shell};
    if ( cstr_shell and str_shell.ends_with("bash") )
    {
      integrate_bash(cstr_home);
    } // if
    else
    {
      ns_log::error()("Unsupported shell '{}' for integration", str_shell);
    } // else
  }
  else
  {
    ns_log::error()("SHELL environment variable is undefined");
  } // else

  // Create desktop entry
  if(set_enable_items.contains(EnableItem::ENTRY))
  {
    ns_log::info()("Integrating desktop entry...");
    integrate_desktop_entry(desktop, cstr_home, config.path_file_binary);
  } // if

  // Create and update mime
  if(set_enable_items.contains(EnableItem::MIMETYPE))
  {
    ns_log::info()("Integrating mime database...");
    integrate_mime_database(desktop, cstr_home, config.path_file_binary);
  } // if

  // Create desktop icons
  if(set_enable_items.contains(EnableItem::ICON))
  {
    ns_log::info()("Integrating desktop icons...");
    integrate_icons(desktop, cstr_home);
  } // if
} // integrate() }}}

// setup() {{{
inline void setup(ns_config::FlatimageConfig const& config, fs::path const& path_file_json_src, fs::path const& path_file_json_dst)
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

  // Make space available to fit icon
  ns_ext2::ns_size::resize_free_space(config.path_file_binary
    , config.offset_ext2
    ,  fs::file_size(path_file_icon) + ns_units::from_mebibytes(config.ext2_slack_minimum).to_bytes()
  );

  // Mount filesystem
  [[maybe_unused]] auto mount = ns_filesystems::Filesystems(config, ns_filesystems::Filesystems::FilesystemsLayer::EXT_RW);

  // Create config dir if not exists
  fs::create_directories(config.path_file_config_desktop.parent_path());

  // Copy the icon to inside the image
  fs::copy_file(path_file_icon, "{}/fim/desktop/icon.{}"_fmt(config.path_dir_mount_ext, *opt_str_ext), fs::copy_options::overwrite_existing);

  // Copy configuration file
  fs::copy_file(path_file_json_src, path_file_json_dst, fs::copy_options::overwrite_existing);

  // Update icon path in the json file
  ns_db::from_file(path_file_json_dst, [&](auto& db){ db("icon") = "/fim/desktop/icon.{}"_fmt(*opt_str_ext); }, ns_db::Mode::UPDATE);
} // setup() }}}

// enable() {{{
inline void enable(ns_config::FlatimageConfig const& config, std::set<EnableItem> set_enable_items)
{
  // Mount filesystem
  [[maybe_unused]] auto mount = ns_filesystems::Filesystems(config, ns_filesystems::Filesystems::FilesystemsLayer::EXT_RW);
  std::vector<std::string> vec_enable_items;
  std::ranges::transform(set_enable_items, std::back_inserter(vec_enable_items), [](auto&& e){ return std::string{e}; });
  ns_db::Db(config.path_file_config_desktop, ns_db::Mode::UPDATE_OR_CREATE)("enable") = vec_enable_items;
} // enable() }}}

} // namespace ns_desktop

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
