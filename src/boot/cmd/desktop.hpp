///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : desktop
///

#pragma once

#include <filesystem>

#include "../../cpp/lib/reserved/desktop.hpp"
#include "../../cpp/lib/reserved/notify.hpp"
#include "../../cpp/lib/db/desktop.hpp"
#include "../../cpp/lib/subprocess.hpp"
#include "../../cpp/lib/image.hpp"
#include "../../cpp/lib/env.hpp"
#include "../../cpp/lib/linux.hpp"
#include "../../cpp/macro.hpp"
#include "../config/config.hpp"

namespace ns_desktop
{

using IntegrationItem = ns_db::ns_desktop::IntegrationItem;

namespace
{

constexpr std::string_view const template_dir_mimetype = ".local/share/icons/hicolor/{}x{}/mimetypes";
constexpr std::string_view const template_dir_apps = ".local/share/icons/hicolor/{}x{}/apps";
constexpr std::string_view const template_file_icon = "application-flatimage_{}.png";
constexpr std::string_view const template_dir_mimetype_scalable = ".local/share/icons/hicolor/scalable/mimetypes";
constexpr std::string_view const template_dir_apps_scalable = ".local/share/icons/hicolor/scalable/apps";
constexpr std::string_view const template_file_icon_scalable = "application-flatimage_{}.svg";
constexpr const std::array<uint32_t, 9> arr_sizes {16,22,24,32,48,64,96,128,256};

namespace fs = std::filesystem;

#pragma pack(push, 1)
struct Image
{
  char m_ext[4];
  char m_data[ns_config::SIZE_RESERVED_IMAGE - 12];
  uint64_t m_size;
  Image() = default;
  Image(char* ext, char* data, uint64_t size)
    : m_size(size)
  {
    // xxx + '\0'
    std::copy(ext, ext+3, m_ext);
    ext[3] = '\0';
    std::copy(data, data+size, m_data);
  } // Image
};
#pragma pack(pop)

decltype(auto) get_path_file_icon_png(fs::path const& path_dir_home
  , std::string_view name_app
  , std::string_view template_dir
  , uint32_t size)
{
  return path_dir_home
    / std::vformat(template_dir, std::make_format_args(size, size))
    / std::vformat(template_file_icon, std::make_format_args(name_app));
} // function: get_path_file_icon_png

decltype(auto) get_path_file_icon_svg(fs::path const& path_dir_home
  , std::string_view name_app
  , std::string_view template_dir)
{
  return path_dir_home
    / template_dir
    / std::vformat(template_file_icon_scalable, std::make_format_args(name_app));
} // function: get_path_file_icon_svg


// read_json_from_binary() {{{
std::expected<std::string,std::string> read_json_from_binary(ns_config::FlatimageConfig const& config)
{
  auto expected_json = ns_reserved::ns_desktop::read(config.path_file_binary
    , config.offset_desktop.offset
    , config.offset_desktop.size
  );
  qreturn_if(not expected_json, expected_json.error());
  return std::string{expected_json->get()};
} // read_json_from_binary() }}}

// write_json_to_binary() {{{
std::error<std::string> write_json_to_binary(ns_config::FlatimageConfig const& config
  , std::string_view str_raw_json)
{
  return ns_reserved::ns_desktop::write(config.path_file_binary
    , config.offset_desktop.offset
    , config.offset_desktop.size
    , str_raw_json.data()
    , str_raw_json.size()
  );
} // write_json_to_binary() }}}

// read_image_from_binary() {{{
std::expected<std::pair<std::unique_ptr<char[]>,uint64_t>,std::string> read_image_from_binary(fs::path const& path_file_binary
  , uint64_t offset
  , uint64_t size)
{
  auto ptr_data = std::make_unique<char[]>(size);
  auto expected_bytes = ns_reserved::read(path_file_binary, offset, size, ptr_data.get());
  qreturn_if(not expected_bytes, std::unexpected(expected_bytes.error()));
  return std::make_pair(std::move(ptr_data), *expected_bytes);
} // read_image_from_binary() }}}

// write_image_to_binary() {{{
std::error<std::string> write_image_to_binary(ns_config::FlatimageConfig const& config
  , char* data
  , uint64_t size)
{
  return ns_reserved::write(config.path_file_binary
    , config.offset_desktop_image.offset
    , config.offset_desktop_image.size
    , data
    , size
  );
} // write_image_to_binary() }}}

// integrate_desktop_entry() {{{
decltype(auto) integrate_desktop_entry(ns_db::ns_desktop::Desktop const& desktop
  , fs::path const& path_dir_home
  , fs::path const& path_file_binary)
{
  // Create path to entry
  fs::path path_file_desktop = path_dir_home / ".local/share/applications/flatimage-{}.desktop"_fmt(desktop.get_name());

  // Create parent directories for entry
  std::error_code ec_entry;
  fs::create_directories(path_file_desktop.parent_path(), ec_entry);
  ereturn_if(ec_entry, "Failed to create parent directories with code {}"_fmt(ec_entry.value()));

  // Create desktop entry
  std::ofstream file_desktop{path_file_desktop};
  println(file_desktop, "[Desktop Entry]");
  println(file_desktop, "Name={}"_fmt(desktop.get_name()));
  println(file_desktop, "Type=Application");
  println(file_desktop, "Comment=FlatImage distribution of \"{}\""_fmt(desktop.get_name()));
  println(file_desktop, "TryExec={}"_fmt(path_file_binary));
  println(file_desktop, "Exec=\"{}\" %F"_fmt(path_file_binary));
  println(file_desktop, "Icon=application-flatimage_{}"_fmt(desktop.get_name()));
  println(file_desktop, "MimeType=application/flatimage_{}"_fmt(desktop.get_name()));
  println(file_desktop, "Categories={};"_fmt(ns_string::from_container(desktop.get_categories(), ';')));
  file_desktop.close();

} // integrate_desktop_entry() }}}

// integrate_mime_database() {{{
decltype(auto) integrate_mime_database(ns_db::ns_desktop::Desktop const& desktop
  , fs::path const& path_dir_home
  , fs::path const& path_file_binary)
{
  // Create path to mimetype
  fs::path path_entry_mimetype = path_dir_home / ".local/share/mime/packages/flatimage-{}.xml"_fmt(desktop.get_name());

  // Create parent directories for mime
  std::error_code ec_mime;
  fs::create_directories(path_entry_mimetype.parent_path(), ec_mime);
  ereturn_if(ec_mime, "Failed to create parent directories with code {}"_fmt(ec_mime.value()));

  // Create mimetype file
  bool is_update_mime_database = not fs::exists(path_entry_mimetype);
  std::ofstream file_mimetype{path_entry_mimetype};
  println(file_mimetype, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
  println(file_mimetype, "<mime-info xmlns=\"http://www.freedesktop.org/standards/shared-mime-info\">");
  println(file_mimetype, "  <mime-type type=\"application/flatimage_{}\">"_fmt(desktop.get_name()));
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
void integrate_icons_svg(ns_db::ns_desktop::Desktop const& desktop, fs::path const& path_dir_home, fs::path const& path_file_icon)
{
    // Path to mimetype icon
    fs::path path_icon_mimetype = get_path_file_icon_svg(path_dir_home, desktop.get_name(), template_dir_mimetype_scalable);
    // Path to app icon
    fs::path path_icon_app = get_path_file_icon_svg(path_dir_home, desktop.get_name(), template_dir_apps_scalable);
    // Copy mimetype icon
    if (std::error_code e; (fs::copy_file(path_file_icon, path_icon_mimetype, fs::copy_options::skip_existing, e), e) )
    {
      ns_log::error()("Could not copy file '{}' with exit code '{}'", path_icon_mimetype, e.value());
    } // if
    // Copy app icon
    if (std::error_code e; (fs::copy_file(path_file_icon, path_icon_app, fs::copy_options::skip_existing, e), e) )
    {
      ns_log::error()("Could not copy file '{}' with exit code '{}'", path_icon_app, e.value());
    } // if
} // integrate_icons_svg() }}}

// integrate_icons_png() {{{
void integrate_icons_png(ns_db::ns_desktop::Desktop const& desktop, fs::path const& path_dir_home, fs::path const& path_file_icon)
{
  for(auto&& size : arr_sizes)
  {
    // Path to mimetype icon
    fs::path path_icon_mimetype = get_path_file_icon_png(path_dir_home, desktop.get_name(), template_dir_mimetype, size);
    // Path to app icon
    std::error_code ec;
    fs::create_directories(path_icon_mimetype.parent_path(), ec);
    ereturn_if(ec, "Could not create parent directories of '{}'"_fmt(path_icon_mimetype));
    fs::path path_icon_app = get_path_file_icon_png(path_dir_home, desktop.get_name(), template_dir_apps, size);
    fs::create_directories(path_icon_app.parent_path(), ec);
    ereturn_if(ec, "Could not create parent directories of '{}'"_fmt(path_icon_app));
    // Avoid overwrite
    if ( not fs::exists(path_icon_mimetype) )
    {
      auto result = ns_image::resize(path_file_icon, path_icon_mimetype, size, size, true);
      econtinue_if(not result.has_value(), result.error())
    } // if
    // Duplicate icon to app directory
    if (std::error_code e; (fs::copy_file(path_icon_mimetype, path_icon_app, fs::copy_options::skip_existing, e), e) )
    {
      ns_log::error()("Could not copy file '{}' with exit code '{}'", path_icon_app, e.value());
    } // if
  } // for
} // integrate_icons_png() }}}

// integrate_icons() {{{
inline void integrate_icons(ns_config::FlatimageConfig const& config, ns_db::ns_desktop::Desktop const& desktop, fs::path const& path_dir_home)
{
  // Test if icons are integrated (if copied all up to the last one)
  dreturn_if ( fs::exists(path_dir_home
      / std::vformat(template_dir_mimetype, std::make_format_args(arr_sizes[8], arr_sizes[8]))
      / std::vformat(template_file_icon, std::make_format_args(desktop.get_name())))
    , "Icons are integrated with the system"
  );
  // Read picture from flatimage binary
  Image image;
  auto expected_data_image = read_image_from_binary(config.path_file_binary
    , config.offset_desktop_image.offset
    , sizeof(image)
  );
  ereturn_if(not expected_data_image, expected_data_image.error());
  std::memcpy(&image, expected_data_image->first.get(), sizeof(image));
  // Create temporary file to write image to
  auto expected_path_file_icon = ns_linux::mkstemps("/tmp", "XXXXXX.{}"_fmt(image.m_ext), 4);
  ereturn_if(not expected_path_file_icon, expected_path_file_icon.error());
  // Write image to temporary file
  std::ofstream file_icon(*expected_path_file_icon);
  ereturn_if(not file_icon.is_open(), "Could not open temporary image file for desktop integration");
  file_icon.write(image.m_data, image.m_size);
  file_icon.close();
  // Create icons
  if ( expected_path_file_icon->string().ends_with(".svg") )
  {
    integrate_icons_svg(desktop, path_dir_home, *expected_path_file_icon);
  }
  else
  {
    integrate_icons_png(desktop, path_dir_home, *expected_path_file_icon);
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

// integrate() {{{
inline void integrate(ns_config::FlatimageConfig const& config)
{
  auto expected_json = read_json_from_binary(config);
  ereturn_if(not expected_json, "Could not read desktop json from binary: {}"_fmt(expected_json.error()));
  std::string str_raw_json{*expected_json};
  ns_log::debug()("Json desktop data: {}", str_raw_json);

  auto desktop = ns_db::ns_desktop::deserialize(str_raw_json);
  ethrow_if(not desktop, "Could not parse json data: {}"_fmt(desktop.error()));

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
  if(desktop->get_integrations().contains(IntegrationItem::ENTRY))
  {
    ns_log::info()("Integrating desktop entry...");
    integrate_desktop_entry(*desktop, cstr_home, config.path_file_binary);
  } // if

  // Create and update mime
  if(desktop->get_integrations().contains(IntegrationItem::MIMETYPE))
  {
    ns_log::info()("Integrating mime database...");
    integrate_mime_database(*desktop, cstr_home, config.path_file_binary);
  } // if

  // Create desktop icons
  if(desktop->get_integrations().contains(IntegrationItem::ICON))
  {
    ns_log::info()("Integrating desktop icons...");
    integrate_icons(config, *desktop, cstr_home);
  } // if
  
  // Check if should notify
  if ( auto expected = ns_reserved::ns_notify::read(config.path_file_binary, config.offset_notify.offset) )
  {
    // Check for errors
    ereturn_if(not expected, "Could not read notify byte: '{}'"_fmt(expected.error()));
    // Check if is enabled
    dreturn_if(not *expected, "Notify is disabled");
    // Get bash binary
    auto path_file_binary_bash = ns_subprocess::search_path("bash");
    ereturn_if(not path_file_binary_bash, "Could not find bash in PATH");
    // Get possible icon paths
    fs::path path_dir_home{cstr_home};
    fs::path path_file_icon = get_path_file_icon_png(path_dir_home, desktop->get_name(), template_dir_apps, 64);
    if ( not fs::exists(path_file_icon) )
    {
      path_file_icon = get_path_file_icon_svg(path_dir_home, desktop->get_name(), template_dir_apps_scalable);
    } // if
    // Path to mimetype icon
    (void) ns_subprocess::Subprocess(*path_file_binary_bash)
      .with_piped_outputs()
      .with_args("-c", "notify-send -i \"{}\" \"Started '{}' flatimage\""_fmt(path_file_icon, desktop->get_name()))
      .spawn();
  } // if
} // integrate() }}}

// setup() {{{
inline void setup(ns_config::FlatimageConfig const& config, fs::path const& path_file_json_src)
{
  // Read input file
  std::ifstream file_json_src{path_file_json_src};
  ereturn_if(not file_json_src.is_open()
      , "Failed to open file '{}' for desktop integration"_fmt(path_file_json_src)
  );
  // Deserialize json
  auto expected_desktop = ns_db::ns_desktop::deserialize(file_json_src);
  ereturn_if(not expected_desktop, "Failed to deserialize json: {}"_fmt(expected_desktop.error()));
  // Application icon
  fs::path path_file_icon = expected_desktop->get_path_file_icon();
  std::optional<std::string> opt_str_ext = (path_file_icon.extension() == ".svg")? std::make_optional("svg")
    : (path_file_icon.extension() == ".png")? std::make_optional("png")
    : (path_file_icon.extension() == ".jpg" or path_file_icon.extension() == ".jpeg")? std::make_optional("jpg")
    : std::nullopt;
  ereturn_if(not opt_str_ext, "Icon extension '{}' is not supported"_fmt(path_file_icon.extension()));
  // Read original image
  auto expected_image_data = read_image_from_binary(path_file_icon, 0, fs::file_size(path_file_icon));
  ereturn_if(not expected_image_data, "Could not read source image: {}"_fmt(expected_image_data.error()));
  ereturn_if(expected_image_data->second >= config.offset_desktop_image.size
    , "File is too large, '{}' bytes"_fmt(config.offset_desktop_image.size)
  );
  // Create image struct to deserialize into binary format
  Image image{opt_str_ext->data(), expected_image_data->first.get(), expected_image_data->second};
  // Serialize image struct in binary format
  auto err = write_image_to_binary(config, reinterpret_cast<char*>(&image), sizeof(image));
  ereturn_if(err, "Could not write image data: {}"_fmt(*err));
  // Serialize json
  auto expected_str_raw_json = ns_db::ns_desktop::serialize(*expected_desktop);
  ereturn_if(not expected_desktop, "Failed to serialize desktop integration: {}"_fmt(expected_str_raw_json.error()));
  // Write json configuration
  auto error = write_json_to_binary(config, *expected_str_raw_json);
  ereturn_if(error, "Failed to write json: {}"_fmt(error));
} // setup() }}}

// enable() {{{
inline void enable(ns_config::FlatimageConfig const& config, std::set<IntegrationItem> set_integrations)
{
  // Read json
  auto expected_json = read_json_from_binary(config);
  ereturn_if(not expected_json, "Could not read desktop json: {}"_fmt(expected_json.error()));
  // Deserialize json
  auto desktop = ns_db::ns_desktop::deserialize(*expected_json);
  // Update integrations value
  desktop->set_integrations(set_integrations);
  // Serialize json
  auto expected_str_raw_json = ns_db::ns_desktop::serialize(*desktop);
  ereturn_if(not expected_str_raw_json, "Could not serialize json to enable desktop integration");
  // Write json
  auto error = write_json_to_binary(config, *expected_str_raw_json);
  ereturn_if(error, "Failed to write json: {}"_fmt(error));
} // enable() }}}

} // namespace ns_desktop

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
