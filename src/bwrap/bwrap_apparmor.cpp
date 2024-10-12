///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : bwrap_apparmor
///

#include <filesystem>

#include "../cpp/lib/env.hpp"
#include "../cpp/lib/log.hpp"
#include "../cpp/lib/subprocess.hpp"
#include "../cpp/common.hpp"
#include "../cpp/macro.hpp"

constexpr std::string_view const profile_bwrap =
R"(abi <abi/4.0>,
include <tunables/global>
profile bwrap /opt/flatimage/bwrap flags=(unconfined) {
  userns,
}
)";

namespace fs = std::filesystem;

int main(int argc, char const* argv[])
{
  // Check arguments
  ereturn_if(argc != 3, "Incorrect # of arguments for bwrap-apparmor", EXIT_FAILURE);
  // Set log file location
  fs::path path_file_log = std::string{argv[1]} + ".bwrap-apparmor.log";
  ns_log::set_sink_file(path_file_log);
  // Find apparmor_parser
  auto opt_path_file_apparmor_parser = ns_subprocess::search_path("apparmor_parser");
  ereturn_if(not opt_path_file_apparmor_parser, "Could not find apparmor_parser", EXIT_FAILURE);
  // Define paths
  fs::path path_file_bwrap_src{argv[2]};
  fs::path path_dir_bwrap{"/opt/flatimage"};
  fs::path path_file_bwrap_dst{path_dir_bwrap / "bwrap"};
  fs::path path_file_profile{"/etc/apparmor.d/flatimage"};
  // Try to create /opt/bwrap directory
  qreturn_if(not lec(fs::exists, path_dir_bwrap) and not lec(fs::create_directories, path_dir_bwrap), EXIT_FAILURE);
  // Try copy bwrap to /opt/bwrap
  qreturn_if(not lec(fs::copy_file, path_file_bwrap_src, path_file_bwrap_dst, fs::copy_options::overwrite_existing), EXIT_FAILURE);
  // Try to set permissions for bwrap binary ( chmod 755 )
  lec(fs::permissions
    , path_file_bwrap_dst
    , fs::perms::owner_all
      | fs::perms::group_read | fs::perms::group_exec
      | fs::perms::others_read | fs::perms::others_exec
    , fs::perm_options::replace
  );
  // Try to create profile
  std::ofstream file_profile(path_file_profile);
  ereturn_if(not file_profile.is_open(), "Could not open profile file", EXIT_FAILURE);
  file_profile << profile_bwrap;
  file_profile.close();
  // Reload profile
  auto ret = ns_subprocess::Subprocess(*opt_path_file_apparmor_parser)
    .with_args("-r", path_file_profile)
    .spawn()
    .wait();
  // Exit code same as from call to opt_path_file_apparmor_parser
  return (ret)? *ret : EXIT_FAILURE;
} // main
