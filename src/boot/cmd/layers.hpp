///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : layers
///

#pragma once

#include <filesystem>

#include "../../cpp/lib/subprocess.hpp"

namespace
{

namespace fs = std::filesystem;

}

namespace ns_layers
{

// fn: create() {{{
inline void create(fs::path const& path_dir_src, fs::path const& path_file_dst, uint64_t compression_level)
{
  // Find mksquashfs binary
  auto opt_path_file_mksquashfs = ns_subprocess::search_path("mksquashfs");
  ethrow_if(not opt_path_file_mksquashfs, "Could not find 'mksquashfs' binary");

  // Compress filesystem
  ns_log::info()("Compress filesystem to '{}'", path_file_dst);
  auto ret = ns_subprocess::Subprocess(*opt_path_file_mksquashfs)
    .with_args(path_dir_src, path_file_dst)
    .with_args("-comp", "zstd")
    .with_args("-Xcompression-level", compression_level)
    .spawn()
    .wait();
  ethrow_if(not ret, "mksquashfs process exited abnormally");
  ethrow_if(*ret != 0, "mksquashfs process exited with error code '{}'"_fmt(*ret));
} // fn: create() }}}

// fn: add() {{{
inline void add(fs::path const& path_file_binary, fs::path const& path_file_layer)
{
  // Open binary file for writing
  std::ofstream file_binary(path_file_binary, std::ios::app | std::ios::binary);
  std::ifstream file_layer(path_file_layer, std::ios::in | std::ios::binary);
  ereturn_if(not file_binary.is_open(), "Failed to open output file '{}'"_fmt(path_file_binary))
  ereturn_if(not file_layer.is_open(), "Failed to open input file '{}'"_fmt(path_file_layer))
  // Get byte size
  uint64_t file_size = fs::file_size(path_file_layer);
  // Write byte size
  file_binary.write(reinterpret_cast<char*>(&file_size), sizeof(file_size));
  char buff[8192];
  while( file_layer.read(buff, sizeof(buff)) or file_layer.gcount() > 0 )
  {
    file_binary.write(buff, file_layer.gcount());
    ereturn_if(not file_binary, "Error writing data to file");
  } // while
  ns_log::info()("Included novel layer from file '{}'", path_file_layer);
} // fn: add() }}}

} // namespace ns_layers

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
