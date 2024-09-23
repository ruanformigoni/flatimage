///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : permissions
///

#pragma once

#include "../reserved.hpp"
#include "../match.hpp"

namespace ns_reserved::ns_permissions
{

namespace
{

namespace fs = std::filesystem;

}

// struct Bits {{{
struct Bits
{
  uint64_t home        : 1 = 0;
  uint64_t media       : 1 = 0;
  uint64_t audio       : 1 = 0;
  uint64_t wayland     : 1 = 0;
  uint64_t xorg        : 1 = 0;
  uint64_t dbus_user   : 1 = 0;
  uint64_t dbus_system : 1 = 0;
  uint64_t udev        : 1 = 0;
  uint64_t usb         : 1 = 0;
  uint64_t input       : 1 = 0;
  uint64_t gpu         : 1 = 0;
  uint64_t network     : 1 = 0;
  Bits() = default;
  void set(std::string permission, bool value)
  {
    std::ranges::transform(permission, permission.begin(), [](char c) { return std::tolower(c); });
    (void) ns_match::match(permission
      , ns_match::equal("home")        >>= [&,this]{ home = value; }
      , ns_match::equal("media")       >>= [&,this]{ media = value; }
      , ns_match::equal("audio")       >>= [&,this]{ audio = value; }
      , ns_match::equal("wayland")     >>= [&,this]{ wayland = value; }
      , ns_match::equal("xorg")        >>= [&,this]{ xorg = value; }
      , ns_match::equal("dbus_user")   >>= [&,this]{ dbus_user = value; }
      , ns_match::equal("dbus_system") >>= [&,this]{ dbus_system = value; }
      , ns_match::equal("udev")        >>= [&,this]{ udev = value; }
      , ns_match::equal("usb")         >>= [&,this]{ usb = value; }
      , ns_match::equal("input")       >>= [&,this]{ input = value; }
      , ns_match::equal("gpu")         >>= [&,this]{ gpu = value; }
      , ns_match::equal("network")     >>= [&,this]{ network = value; }
    );
  } // set
  std::vector<std::string> to_vector_string()
  {
    std::vector<std::string> out;
    if ( home )        { out.push_back("home"); }
    if ( media )       { out.push_back("media"); }
    if ( audio )       { out.push_back("audio"); }
    if ( wayland )     { out.push_back("wayland"); }
    if ( xorg )        { out.push_back("xorg"); }
    if ( dbus_user )   { out.push_back("dbus_user"); }
    if ( dbus_system ) { out.push_back("dbus_system"); }
    if ( udev )        { out.push_back("udev"); }
    if ( usb )         { out.push_back("usb"); }
    if ( input )       { out.push_back("input"); }
    if ( gpu )         { out.push_back("gpu"); }
    if ( network )     { out.push_back("network"); }
    return out;
  }
}; // }}}

// write() {{{
inline std::optional<std::string> write(fs::path const& path_file_binary
  , int64_t begin
  , int64_t end
  , Bits bits
)
{
  return ns_reserved::write(path_file_binary, begin, end, reinterpret_cast<char*>(&bits), sizeof(bits));
} // write() }}}

// read() {{{
inline std::expected<Bits,std::string> read(fs::path const& path_file_binary
  , uint64_t begin
  , uint64_t end)
{
  Bits bits;
  char buffer[sizeof(Bits)];
  qreturn_if(sizeof(Bits) > (end - begin), std::unexpected("Not enough space for read"));
  auto err = ns_reserved::read(path_file_binary, begin, sizeof(bits), buffer);
  qreturn_if(err, std::unexpected(*err));
  std::memcpy(&bits, buffer, sizeof(bits));
  return bits;
} // read() }}}

// class Permissions {{{
class Permissions
{
  private:
    fs::path const& m_path_file_binary;
    int64_t m_begin;
    int64_t m_end;
  public:
    Permissions(fs::path const& path_file_binary
      , int64_t begin
      , int64_t end
    ) : m_path_file_binary(path_file_binary)
      , m_begin(begin)
      , m_end(end)
    {}
    template<ns_concept::Iterable R>
    inline void set(R&& r)
    {
      ns_reserved::ns_permissions::Bits bits;
      std::ranges::for_each(r, [&](auto&& e){ bits.set(e, true); });
      auto error = ns_reserved::ns_permissions::write(m_path_file_binary, m_begin, m_end, bits);
      ereturn_if(error, "Error to write permission bits: {}"_fmt(*error));
    }

    template<ns_concept::Iterable R>
    inline void add(R&& r)
    {
      auto expected = ns_reserved::ns_permissions::read(m_path_file_binary, m_begin, m_end);
      ereturn_if(not expected, "Could not read permission bits: {}"_fmt(expected.error()));
      std::ranges::for_each(r, [&](auto&& e){ expected->set(e, true); });
      auto error = write(m_path_file_binary, m_begin, m_end, *expected);
      ereturn_if(error, "Error to write permission bits: {}"_fmt(*error));
    }

    template<ns_concept::Iterable R>
    inline void del(R&& r)
    {
      auto expected = read(m_path_file_binary, m_begin, m_end);
      ereturn_if(not expected, "Could not read permission bits: {}"_fmt(expected.error()));
      std::ranges::for_each(r, [&](auto&& e){ expected->set(e, false); });
      auto error = write(m_path_file_binary, m_begin, m_end, *expected);
      ereturn_if(error, "Error to write permission bits: {}"_fmt(*error));
    }

    inline std::expected<Bits, std::string> get()
    {
      return read(m_path_file_binary, m_begin, m_end);
    }

    inline std::vector<std::string> to_vector_string()
    {
      std::vector<std::string> out;
      auto expected = read(m_path_file_binary, m_begin, m_end);
      ereturn_if(not expected, "Failed to read permissions: {}"_fmt(expected.error()), out);
      return expected->to_vector_string();
    }
}; // }}}

} // namespace ns_reserved::ns_permissions

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
