// glaze_meta.h — lightweight compile-time struct reflection for the binary
// event payloads posted to Dart. Provides glz::meta<T> and glz::field(), used
// by bluez_media_types.h to describe struct fields for serialization.
//
// All variable-length values use uint32_t prefixes. The Dart decoder must read
// the same four-byte little-endian prefix width; using uint64_t for a length or
// count breaks the wire ABI by consuming payload bytes as part of the prefix.

#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace glz {

// Field descriptor: a name + member pointer pair.
template <typename T, typename MemberPtr>
struct FieldDescriptor {
  const char* name;
  MemberPtr ptr;
};

template <typename T, typename MemberPtr>
constexpr auto field(const char* name, MemberPtr ptr) {
  return FieldDescriptor<T, MemberPtr>{name, ptr};
}

// Overload for deduced class type from member pointer.
template <typename C, typename M>
constexpr auto field(const char* name, M C::* ptr) {
  return FieldDescriptor<C, M C::*>{name, ptr};
}

// meta<T> — specialize for each struct to list its fields.
// Default: empty (no fields).
template <typename T>
struct meta {
  static constexpr auto fields = std::make_tuple();
};

// ── Binary encode/decode helpers ──────────────────────────────────────────

namespace detail {

inline void write_bytes(std::vector<uint8_t>& buf, const void* data, size_t n) {
  const auto* p = static_cast<const uint8_t*>(data);
  buf.insert(buf.end(), p, p + n);
}

template <typename T>
  requires std::is_integral_v<T>
void write_integer(std::vector<uint8_t>& buf, T value) {
  using Unsigned = std::make_unsigned_t<T>;
  const auto bits = std::bit_cast<Unsigned>(value);
  for (size_t i = 0; i < sizeof(T); ++i) {
    buf.push_back(static_cast<uint8_t>(bits >> (i * 8)));
  }
}

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

// ── Encode primitives ────────────────────────────────────────────────────

inline void encode_field(std::vector<uint8_t>& buf, uint8_t v) {
  buf.push_back(v);
}

inline void encode_field(std::vector<uint8_t>& buf, bool v) {
  buf.push_back(v ? 1 : 0);
}

inline void encode_field(std::vector<uint8_t>& buf, int16_t v) {
  write_integer(buf, v);
}

inline void encode_field(std::vector<uint8_t>& buf, int32_t v) {
  write_integer(buf, v);
}

inline void encode_field(std::vector<uint8_t>& buf, uint16_t v) {
  write_integer(buf, v);
}

inline void encode_field(std::vector<uint8_t>& buf, uint32_t v) {
  write_integer(buf, v);
}

inline void encode_field(std::vector<uint8_t>& buf, uint64_t v) {
  write_integer(buf, v);
}

inline void encode_field(std::vector<uint8_t>& buf, const std::string& s) {
  auto len = static_cast<uint32_t>(s.size());
  write_integer(buf, len);
  write_bytes(buf, s.data(), s.size());
}

inline void encode_field(std::vector<uint8_t>& buf,
                         const std::vector<std::string>& v) {
  auto count = static_cast<uint32_t>(v.size());
  write_integer(buf, count);
  for (const auto& s : v) {
    encode_field(buf, s);
  }
}

inline void encode_field(std::vector<uint8_t>& buf,
                         const std::vector<uint8_t>& v) {
  auto count = static_cast<uint32_t>(v.size());
  write_integer(buf, count);
  write_bytes(buf, v.data(), v.size());
}

// Forward declaration for struct encoding (used by vector<T> below).
template <typename T>
void encode_struct(std::vector<uint8_t>& buf, const T& obj);

// Encode a vector of structs that have glz::meta<T> specializations.
template <typename T>
  requires(std::tuple_size_v<decltype(meta<T>::fields)> > 0)
void encode_field(std::vector<uint8_t>& buf, const std::vector<T>& v) {
  auto count = static_cast<uint32_t>(v.size());
  write_integer(buf, count);
  for (const auto& item : v) {
    encode_struct(buf, item);
  }
}

// Encode a map<string, vector<uint8_t>>.
inline void encode_field(std::vector<uint8_t>& buf,
                         const std::map<std::string, std::vector<uint8_t>>& m) {
  auto count = static_cast<uint32_t>(m.size());
  write_integer(buf, count);
  for (const auto& [key, val] : m) {
    encode_field(buf, key);
    encode_field(buf, val);
  }
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

// ── Struct encode/decode via meta<T>::fields ─────────────────────────────

template <typename T, typename Tuple, std::size_t... I>
void encode_impl(std::vector<uint8_t>& buf,
                 const T& obj,
                 const Tuple& fields,
                 std::index_sequence<I...>) {
  (encode_field(buf, obj.*(std::get<I>(fields).ptr)), ...);
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
void encode_struct(std::vector<uint8_t>& buf, const T& obj) {
  constexpr auto fields = meta<T>::fields;
  constexpr auto N = std::tuple_size_v<decltype(fields)>;
  encode_impl(buf, obj, fields, std::make_index_sequence<N>{});
}

template <typename T>
size_t decode_struct(const uint8_t* buf, size_t offset, T& obj) {
  constexpr auto fields = meta<T>::fields;
  constexpr auto N = std::tuple_size_v<decltype(fields)>;
  return decode_impl(buf, offset, obj, fields, std::make_index_sequence<N>{});
}

}  // namespace detail

// Encode a struct to a byte buffer using its meta<T>::fields.
template <typename T>
std::vector<uint8_t> encode(const T& obj) {
  std::vector<uint8_t> buf;
  detail::encode_struct(buf, obj);
  return buf;
}

// Decode a struct from a byte buffer using its meta<T>::fields.
// Returns the offset past the consumed bytes.
template <typename T>
size_t decode(const uint8_t* buf, size_t offset, T& obj) {
  return detail::decode_struct(buf, offset, obj);
}

}  // namespace glz
