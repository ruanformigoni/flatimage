///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : layers
///

#pragma once

#include <filesystem>

#include "../../cpp/lib/subprocess.hpp"
#include "../../cpp/lib/ext2/size.hpp"
#include "../filesystems.hpp"

namespace
{

namespace fs = std::filesystem;

// fn: generate_sha() {{{
std::string generate_sha(fs::path const& path_file_layer)
{
  std::string str_sha256sum;
  auto opt_path_file_bash = ns_subprocess::search_path("bash");
  ethrow_if(not opt_path_file_bash, "Could not find 'bash' binary");
  auto opt_path_file_sha256sum = ns_subprocess::search_path("sha256sum");
  ethrow_if(not opt_path_file_sha256sum, "Could not find 'sha256sum' binary");
  auto ret = ns_subprocess::Subprocess(*opt_path_file_bash)
    .with_args("-c", *opt_path_file_sha256sum + " " + path_file_layer.string() + " | awk '{print $1}'" )
    .with_piped_outputs()
    .with_stdout_handle([&](auto&& str){ str_sha256sum = str; })
    .spawn()
    .wait();
  ethrow_if(not ret, "sha256sum process exited abnormally");
  ethrow_if(*ret != 0, "sha256sum process exited with error code '{}'"_fmt(*ret));
  ethrow_if(str_sha256sum.empty(), "Could not calculate SHA for layer");
  ns_log::info()("sha256sum for '{}' is '{}'", path_file_layer, str_sha256sum);
  return str_sha256sum;
} // fn: generate_sha() }}}

// fn: query_highest_layer_index() {{{
uint64_t query_highest_layer_index(ns_config::FlatimageConfig const& config, fs::path const& path_dir_layers)
{
  std::string str_index_highest{};
  [[maybe_unused]] auto mount = ns_filesystems::Filesystems(config, ns_filesystems::Filesystems::FilesystemsLayer::EXT_RO);
  std::vector<fs::path> vec_path_dir_layers;
  std::ranges::for_each(fs::directory_iterator(path_dir_layers)
    , [&](auto&& e){ vec_path_dir_layers.push_back(e); }
  );
  std::ranges::sort(vec_path_dir_layers);
  str_index_highest = vec_path_dir_layers.back().filename().string();
  str_index_highest.erase(str_index_highest.find('-'), std::string::npos);
  uint64_t int_index_highest = std::stoi(str_index_highest) + 1;
  ns_log::info()("Filesystem index: {}", int_index_highest);
  return int_index_highest;
} // fn: query_highest_layer_index() }}}

// fn: generate_layer_name() {{{
std::string generate_layer_name(ns_config::FlatimageConfig const& config, fs::path const& path_file_layer, fs::path const& path_dir_layers)
{
  std::string sha = generate_sha(path_file_layer);
  uint64_t index = query_highest_layer_index(config, path_dir_layers);
  return "{}-{}"_fmt(std::to_string(index), sha);
} // fn: generate_layer_name() }}}

}

namespace ns_layers
{

// fn: create() {{{
inline void create(fs::path const& path_dir_src, fs::path const& path_file_dst, uint64_t compression_level)
{
  // Find mkdwarfs binary
  auto opt_path_file_mkdwarfs = ns_subprocess::search_path("mkdwarfs");
  ethrow_if(not opt_path_file_mkdwarfs, "Could not find 'mkdwarfs' binary");

  // Compress filesystem
  ns_log::info()("Compress filesystem to '{}'", path_file_dst);
  auto ret = ns_subprocess::Subprocess(*opt_path_file_mkdwarfs)
    .with_args("-f")
    .with_args("-l", compression_level)
    .with_args("-i", path_dir_src)
    .with_args("-o", path_file_dst)
    .spawn()
    .wait();
  ethrow_if(not ret, "mkdwarfs process exited abnormally");
  ethrow_if(*ret != 0, "mkdwarfs process exited with error code '{}'"_fmt(*ret));
} // fn: create() }}}

// fn: add() {{{
inline void add(ns_config::FlatimageConfig const& config, fs::path const& path_file_layer)
{
  // Create novel path for layer
  fs::path path_file_layer_new = config.path_dir_layers / generate_layer_name(config, path_file_layer, config.path_dir_layers);

  // Make space available in the main filesystem to fit novel layer
  ns_ext2::ns_size::resize_free_space(config.path_file_binary
    , config.offset_ext2
    , fs::file_size(path_file_layer) + ns_units::from_mebibytes(config.ext2_slack_minimum).to_bytes()
  );

  // Mount filesystem as RW
  [[maybe_unused]] auto mount = ns_filesystems::Filesystems(config, ns_filesystems::Filesystems::FilesystemsLayer::EXT_RW);

  // Copy compressed filesystem to ext filesystem
  fs::copy_file(path_file_layer, path_file_layer_new);
} // fn: add() }}}

} // namespace ns_layers

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
