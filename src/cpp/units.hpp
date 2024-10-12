///
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : units
///

#pragma once

namespace ns_units
{

namespace
{

class Bytes
{
  private:
    unsigned long long m_bytes;
  public:
    Bytes(unsigned long long bytes)
      : m_bytes(bytes) {}

    inline unsigned long long to_bytes();
    inline unsigned long long to_kibibytes();
    inline unsigned long long to_mebibytes();
    inline unsigned long long to_gibibytes();
}; // 

} // namespace

inline unsigned long long Bytes::to_bytes()
{
  return m_bytes;
}

inline unsigned long long Bytes::to_kibibytes()
{
  return m_bytes / 1024;
}

inline unsigned long long Bytes::to_mebibytes()
{
  return m_bytes / 1024 / 1024;
}

inline unsigned long long Bytes::to_gibibytes()
{
  return m_bytes / 1024 / 1024 / 1024;
}

inline Bytes from_bytes(unsigned long long bytes)
{
  return bytes;
}

inline Bytes from_kibibytes(unsigned long long kib)
{
  return kib * 1024;
}

inline Bytes from_mebibytes(unsigned long long mib)
{
  return mib * 1024 * 1024;
}

inline Bytes from_gibibytes(unsigned long long gib)
{
  return gib * 1024 * 1024 * 1024;
}


} // namespace ns_units

/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
