///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : ciopfs
///

#pragma once

#include <filesystem>
#include <unistd.h>

#include "subprocess.hpp"
#include "fuse.hpp"

namespace ns_ciopfs
{


namespace
{

namespace fs = std::filesystem;

} // namespace

class Ciopfs
{
  private:
    std::unique_ptr<ns_subprocess::Subprocess> m_subprocess;
    fs::path m_path_dir_upper;

  public:
    Ciopfs( fs::path const& path_dir_lower , fs::path const& path_dir_upper)
      : m_path_dir_upper(path_dir_upper)
    {
      ethrow_if(not fs::exists(path_dir_lower), "Lowerdir does not exist for ciopfs");

      std::error_code ec;
      ethrow_if(not fs::exists(path_dir_upper) and not fs::create_directories(path_dir_upper, ec)
        , "Upperdir does not exist for ciopfs: {}"_fmt(ec.message())
      );

      // Find Ciopfs
      auto opt_path_file_ciopfs = ns_subprocess::search_path("ciopfs");
      ethrow_if (not opt_path_file_ciopfs, "Could not find 'ciopfs' in PATH");

      // Create subprocess
      m_subprocess = std::make_unique<ns_subprocess::Subprocess>(*opt_path_file_ciopfs);


      // Include arguments and spawn process
      std::ignore = m_subprocess->
         with_args(path_dir_lower, path_dir_upper)
        .spawn()
        .wait();
    } // ciopfs

    ~Ciopfs()
    {
      ns_fuse::unmount(m_path_dir_upper);
    } // ~ciopfs
}; // class: ciopfs

} // namespace ns_ciopfs

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
