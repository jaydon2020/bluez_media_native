// Test-only decoder for trusted buffers produced by the native encoder.
// Production receives no binary payloads and must not expose unchecked decoding.
#pragma once
#include "glaze_meta.h"
namespace glz {
namespace detail {
template <typename T>
  requires std::is_integral_v<T>
size_t read_integer(const uint8_t* buf, size_t offset, T& value) {
  using Unsigned = std::make_unsigned_t<T>;
  Unsigned bits{};
  for (size_t i = 0; i < sizeof(T); ++i) {
    bits |= static_cast<Unsigned>(buf[offset + i]) << (i * 8);
  }
  value = std::bit_cast<T>(bits);
  return offset + sizeof(T);
}

// ── Decode primitives ────────────────────────────────────────────────────

inline size_t decode_field(const uint8_t* buf, size_t offset, uint8_t& v) {
  v = buf[offset];
  return offset + 1;
}

inline size_t decode_field(const uint8_t* buf, size_t offset, bool& v) {
  v = buf[offset] != 0;
  return offset + 1;
}

inline size_t decode_field(const uint8_t* buf, size_t offset, int16_t& v) {
  return read_integer(buf, offset, v);
}

inline size_t decode_field(const uint8_t* buf, size_t offset, int32_t& v) {
  return read_integer(buf, offset, v);
}

inline size_t decode_field(const uint8_t* buf, size_t offset, uint16_t& v) {
  return read_integer(buf, offset, v);
}

inline size_t decode_field(const uint8_t* buf, size_t offset, uint32_t& v) {
  return read_integer(buf, offset, v);
}

inline size_t decode_field(const uint8_t* buf, size_t offset, uint64_t& v) {
  return read_integer(buf, offset, v);
}

inline size_t decode_field(const uint8_t* buf, size_t offset, std::string& s) {
  uint32_t len{};
  offset = read_integer(buf, offset, len);
  s.assign(reinterpret_cast<const char*>(buf + offset), len);
  return offset + len;
}

inline size_t decode_field(const uint8_t* buf,
                           size_t offset,
                           std::vector<std::string>& v) {
  uint32_t count{};
  offset = read_integer(buf, offset, count);
  v.resize(count);
  for (uint32_t i = 0; i < count; ++i) {
    offset = decode_field(buf, offset, v[i]);
  }
  return offset;
}

inline size_t decode_field(const uint8_t* buf,
                           size_t offset,
                           std::vector<uint8_t>& v) {
  uint32_t count{};
  offset = read_integer(buf, offset, count);
  v.assign(buf + offset, buf + offset + count);
  return offset + count;
}

// Forward declaration for struct decoding (used by vector<T> below).
template <typename T>
size_t decode_struct(const uint8_t* buf, size_t offset, T& obj);

// Decode a vector of structs that have glz::meta<T> specializations.
template <typename T>
  requires(std::tuple_size_v<decltype(meta<T>::fields)> > 0)
size_t decode_field(const uint8_t* buf, size_t offset, std::vector<T>& v) {
  uint32_t count{};
  offset = read_integer(buf, offset, count);
  v.resize(count);
  for (uint32_t i = 0; i < count; ++i) {
    offset = decode_struct(buf, offset, v[i]);
  }
  return offset;
}

// Decode a map<string, vector<uint8_t>>.
inline size_t decode_field(const uint8_t* buf,
                           size_t offset,
                           std::map<std::string, std::vector<uint8_t>>& m) {
  uint32_t count{};
  offset = read_integer(buf, offset, count);
  m.clear();
  for (uint32_t i = 0; i < count; ++i) {
    std::string key;
    offset = decode_field(buf, offset, key);
    std::vector<uint8_t> val;
    offset = decode_field(buf, offset, val);
    m[std::move(key)] = std::move(val);
  }
  return offset;
}

template <typename T, typename Tuple, std::size_t... I>
size_t decode_impl(const uint8_t* buf,
                   size_t offset,
                   T& obj,
                   const Tuple& fields,
                   std::index_sequence<I...>) {
  ((offset = decode_field(buf, offset, obj.*(std::get<I>(fields).ptr))), ...);
  return offset;
}

template <typename T>
size_t decode_struct(const uint8_t* buf, size_t offset, T& obj) {
  constexpr auto fields = meta<T>::fields;
  constexpr auto N = std::tuple_size_v<decltype(fields)>;
  return decode_impl(buf, offset, obj, fields, std::make_index_sequence<N>{});
}

} // namespace detail
// Decode a struct from a byte buffer using its meta<T>::fields.
// Returns the offset past the consumed bytes.
template <typename T>
size_t decode(const uint8_t* buf, size_t offset, T& obj) {
  return detail::decode_struct(buf, offset, obj);
}

} // namespace glz
