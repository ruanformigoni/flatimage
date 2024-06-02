///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : image
///

#pragma once

#include <filesystem>
#include <boost/gil.hpp>
#include <boost/gil/extension/io/jpeg.hpp>
#include <boost/gil/extension/io/png.hpp>
#include <boost/gil/extension/numeric/sampler.hpp>
#include <boost/gil/extension/numeric/resample.hpp>

#include "log.hpp"
#include "match.hpp"
#include "../std/exception.hpp"

namespace ns_image
{

namespace
{

namespace fs = std::filesystem;

// resize_impl() {{{
inline void resize_impl(fs::path const& path_file_src
  , fs::path const& path_file_dst
  , uint32_t width
  , uint32_t height
  , bool should_preserve_aspect_ratio)
{
  namespace gil = boost::gil;

  // Create icon directory and set file name
  ns_log::info()("Reading image {}", path_file_src);
  ethrow_if(not fs::is_regular_file(path_file_src), "File '{}' does not exist or is not a regular file"_fmt(path_file_src));

  gil::rgba8_image_t img;
  ns_match::match(path_file_src.extension()
    , ns_match::equal(".jpg", ".jpeg") >>= [&]{ gil::read_and_convert_image(path_file_src, img, gil::jpeg_tag()); }
    , ns_match::equal(".png") >>= [&]{ gil::read_and_convert_image(path_file_src, img, gil::png_tag()); }
  );

  ns_log::info()("Image size is {}x{}", std::to_string(img.width()), std::to_string(img.height()));

  if ( should_preserve_aspect_ratio )
  {
    // Calculate desired and current aspected ratios
    double src_aspect = static_cast<double>(img.width()) / img.height();
    double dst_aspect = static_cast<double>(width) / height;

    // Calculate novel dimensions that preserve the aspect ratio
    int width_new  = (src_aspect >  dst_aspect)? static_cast<int>(src_aspect * height) : width;
    int height_new = (src_aspect <= dst_aspect)? static_cast<int>(width / src_aspect ) : height;

    // Resize
    gil::rgba8_image_t img_resized(width_new, height_new);
    ns_log::info()("Image  aspect ratio is {}", std::to_string(src_aspect));
    ns_log::info()("Target aspect ratio is {}", std::to_string(dst_aspect));
    ns_log::info()("Resizing image to {}x{}", std::to_string(width_new), std::to_string(height_new));
    gil::resize_view(gil::const_view(img), gil::view(img_resized), gil::bilinear_sampler());

    // Write to disk
    ns_log::info()("Saving image to {}", path_file_dst);
    gil::write_view(path_file_dst, gil::const_view(img_resized), gil::png_tag());
  } // if
  else
  {
    // Resize
    gil::rgba8_image_t img_resized(width, height);
    ns_log::info()("Resizing image to {}x{}", std::to_string(width), std::to_string(height));
    gil::resize_view(gil::const_view(img), gil::view(img_resized), gil::bilinear_sampler());
    // Write to disk
    ns_log::info()("Saving image to {}", path_file_dst);
    gil::write_view(path_file_dst, gil::const_view(img_resized), gil::png_tag());

  } // else
} // resize_impl() }}}

} // namespace

// resize() {{{
inline decltype(auto) resize(fs::path const& path_file_src
  , fs::path const& path_file_dst
  , uint32_t width
  , uint32_t height
  , bool should_preserve_aspect_ratio = false)
{
  return ns_exception::to_expected([&]{ resize_impl(path_file_src, path_file_dst, width, height, should_preserve_aspect_ratio); });
} // resize() }}}

} // namespace ns_image

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
