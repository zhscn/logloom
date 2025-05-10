#pragma once

#include <boost/endian/conversion.hpp>
#include <boost/pfr.hpp>
#include <concepts>
#include <numeric>  // IWYU pragma: keep
#include <optional>
#include <string>
#include <type_traits>

namespace oned {

namespace detail {
uint64_t zig_zag_encode(int64_t value) {
  return (value << 1) ^ (value >> 63);
}
int64_t zig_zag_decode(uint64_t value) {
  return int64_t(value >> 1 ^ -(value & 1));
}
}  // namespace detail

struct Serializer {
  std::string buffer;

  std::string take() {
    return std::move(buffer);
  }

  void write_int(int64_t value) {
    write_uint(detail::zig_zag_encode(value));
  }
  void write_uint(uint64_t value) {
    if (value < 0xFF - 2) {
      pack_int<uint8_t>(value);
    } else if (value <= std::numeric_limits<uint16_t>::max()) {
      pack_int<uint8_t>(0xFF - 2);
      pack_int<uint16_t>(value);
    } else if (value <= std::numeric_limits<uint32_t>::max()) {
      pack_int<uint8_t>(0xFF - 1);
      pack_int<uint32_t>(value);
    } else {
      pack_int<uint8_t>(0xFF);
      pack_int<uint64_t>(value);
    }
  }
  void write_str(const char *p) {
    pack_str(p, std::strlen(p));
  }
  void write_str(const std::string &str) {
    pack_str(str.data(), str.size());
  }
  void write_str(std::string_view str) {
    pack_str(str.data(), str.size());
  }

private:
  template <typename T>
  void pack_int(T value) {
    T v = boost::endian::native_to_little(value);
    auto size = buffer.size();
    buffer.resize(size + sizeof(value));
    std::memcpy(buffer.data() + size, &v, sizeof(value));  // NOLINT
  }
  void pack_str(const char *p, std::size_t length) {
    write_uint(length);
    auto size = buffer.size();
    buffer.resize(size + length);
    std::memcpy(buffer.data() + size, p, length);  // NOLINT
  }
};

struct Deserializer {
  std::string_view buffer;
  std::size_t pos = 0;

  int64_t read_int() {
    auto value = read_uint();
    return detail::zig_zag_decode(value);
  }
  uint64_t read_uint() {
    auto i = pick_int<uint8_t>();
    if (i < 0xFF - 2) {
      return i;
    }
    if (i == 0xFF - 2) {
      return pick_int<uint16_t>();
    }
    if (i == 0xFF - 1) {
      return pick_int<uint32_t>();
    }
    return pick_int<uint64_t>();
  }

  std::string_view read_str() {
    auto length = read_uint();
    auto str = buffer.substr(pos, length);
    pos += length;
    return str;
  }

private:
  template <typename T>
  T pick_int() {
    T value;
    std::memcpy(&value, buffer.data() + pos, sizeof(value));  // NOLINT
    pos += sizeof(value);
    return boost::endian::little_to_native(value);
  }
};

template <std::unsigned_integral T>
void serialize(Serializer &serializer, T value) {
  serializer.write_uint(uint64_t(value));
}

template <std::signed_integral T>
void serialize(Serializer &serializer, T value) {
  serializer.write_int(int64_t(value));
}

template <std::floating_point T>
void serialize(Serializer &serializer, T value) {
  union {
    T f;
    uint64_t u;
  } v;
  v.f = value;
  serializer.write_uint(v.u);
}

void serialize(Serializer &serializer, const char *str) {
  serializer.write_str(str);
}

void serialize(Serializer &serializer, const std::string &str) {
  serializer.write_str(str);
}

void serialize(Serializer &serializer, std::string_view str) {
  serializer.write_str(str);
}

template <typename T>
void serialize(Serializer &serializer, const std::optional<T> &value) {
  if (value) {
    serializer.write_uint(1);
    serialize(serializer, *value);
  } else {
    serializer.write_uint(0);
  }
}

template <std::unsigned_integral T>
void deserialize(Deserializer &deserializer, T &value) {
  value = deserializer.read_uint();
}

template <std::signed_integral T>
void deserialize(Deserializer &deserializer, T &value) {
  value = deserializer.read_int();
}

template <std::floating_point T>
void deserialize(Deserializer &deserializer, T &value) {
  union {
    T f;
    uint64_t u;
  } v;
  v.u = deserializer.read_uint();
  value = v.f;
}

void deserialize(Deserializer &deserializer, std::string &value) {
  value = std::string(deserializer.read_str());
}

void deserialize(Deserializer &deserializer, std::string_view &value) {
  value = deserializer.read_str();
}

template <typename T>
void deserialize(Deserializer &deserializer, std::vector<T> &value) {
  value.resize(deserializer.read_uint());
  for (auto &v : value) {
    deserialize(deserializer, v);
  }
}

template <typename T>
void deserialize(Deserializer &deserializer, std::optional<T> &value) {
  if (deserializer.read_uint()) {
    T v;
    deserialize(deserializer, v);
    value = std::move(v);
  } else {
    value = std::nullopt;
  }
}

template <typename T>
  requires std::is_aggregate_v<T>
void serialize(Serializer &serializer, const T &value) {
  boost::pfr::for_each_field(
      value, [&](const auto &field) { serialize(serializer, field); });
}

template <typename T>
  requires std::is_aggregate_v<T>
void deserialize(Deserializer &deserializer, T &value) {
  boost::pfr::for_each_field(
      value, [&](auto &field) { deserialize(deserializer, field); });
}

template <typename C>
concept AssociativeContainer = requires(const C &c) {
  typename C::key_type;
  typename C::mapped_type;

  { c.begin() } -> std::same_as<typename C::const_iterator>;
  { c.end() } -> std::same_as<typename C::const_iterator>;

  requires requires(typename C::const_iterator it) {
    { it->first } -> std::convertible_to<const typename C::key_type &>;
    { it->second } -> std::convertible_to<const typename C::mapped_type &>;
  };
};

template <typename T>
concept NoKeyType = !requires { typename T::key_type; };

template <typename C>
concept SequenceContainer = NoKeyType<C> && requires(const C &c) {
  typename C::value_type;
  typename C::const_iterator;
  { c.begin() } -> std::same_as<typename C::const_iterator>;
  { c.end() } -> std::same_as<typename C::const_iterator>;

  requires requires(typename C::const_iterator it) {
    { *it } -> std::convertible_to<const typename C::value_type &>;
  };
};

template <typename C>
concept SetContainer = requires(const C &c) {
  typename C::key_type;
  typename C::value_type;
  typename C::const_iterator;

  requires std::is_same_v<typename C::key_type, typename C::value_type>;

  { c.begin() } -> std::same_as<typename C::const_iterator>;
  { c.end() } -> std::same_as<typename C::const_iterator>;

  requires requires(typename C::const_iterator it) {
    { *it } -> std::convertible_to<const typename C::value_type &>;
  };
};

template <AssociativeContainer C>
void serialize(Serializer &serializer, const C &value) {
  serializer.write_uint(value.size());
  for (const auto &[key, value] : value) {
    serialize(serializer, key);
    serialize(serializer, value);
  }
}

template <AssociativeContainer C>
void deserialize(Deserializer &deserializer, C &container) {
  auto size = deserializer.read_uint();
  container.clear();
  for (std::size_t i = 0; i < size; ++i) {
    typename C::key_type key;
    typename C::mapped_type value;
    deserialize(deserializer, key);
    deserialize(deserializer, value);
    container.insert({std::move(key), std::move(value)});
  }
}

template <SequenceContainer C>
void serialize(Serializer &serializer, const C &container) {
  serializer.write_uint(container.size());
  for (const auto &v : container) {
    serialize(serializer, v);
  }
}

template <SequenceContainer C>
void deserialize(Deserializer &deserializer, C &container) {
  auto size = deserializer.read_uint();
  container.clear();
  for (std::size_t i = 0; i < size; ++i) {
    typename C::value_type v;
    deserialize(deserializer, v);
    container.push_back(std::move(v));
  }
}

template <SetContainer C>
void serialize(Serializer &serializer, const C &container) {
  serializer.write_uint(container.size());
  for (const auto &v : container) {
    serialize(serializer, v);
  }
}

template <SetContainer C>
void deserialize(Deserializer &deserializer, C &container) {
  auto size = deserializer.read_uint();
  container.clear();
  for (std::size_t i = 0; i < size; ++i) {
    typename C::value_type v;
    deserialize(deserializer, v);
    container.insert(std::move(v));
  }
}

}  // namespace oned
