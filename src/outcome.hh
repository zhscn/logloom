#pragma once

#include <fmt/core.h>
#include <outcome.hpp>

#include <source_location>

#define TRY(...) OUTCOME_TRY(__VA_ARGS__)
#define TRYV(...) OUTCOME_TRYV(__VA_ARGS__)

#if defined(__GNUC__) || defined(__clang__)
#define TRYX(...) OUTCOME_TRYX(__VA_ARGS__)
#endif

namespace outcome = OUTCOME_V2_NAMESPACE::experimental;

namespace logloom {

template <typename R,
          typename S = outcome::erased_errored_status_code<
              typename outcome::system_code::value_type>,
          typename NoValuePolicy =
              outcome::policy::default_status_result_policy<R, S>>
using Result = outcome::status_result<R, S, NoValuePolicy>;

template <typename R,
          typename S = outcome::erased_errored_status_code<
              typename outcome::system_code::value_type>,
          typename P = std::exception_ptr,
          typename NoValuePolicy =
              outcome::policy::default_status_outcome_policy<R, S, P>>
using Outcome = outcome::status_outcome<R, S, P, NoValuePolicy>;

}  // namespace logloom

template <>
struct fmt::formatter<SYSTEM_ERROR2_NAMESPACE::status_code_domain::string_ref>
    : fmt::formatter<std::string_view> {
  using T = SYSTEM_ERROR2_NAMESPACE::status_code_domain::string_ref;
  template <typename FormatContext>
  auto format(const T &c, FormatContext &ctx) const -> decltype(ctx.out()) {
    auto s = std::string_view{c.data(), c.size()};
    return fmt::formatter<std::string_view>::format(s, ctx);
  }
};

template <typename T>
  requires outcome::is_status_code<T>::value ||
           outcome::is_errored_status_code<T>::value
struct fmt::formatter<T> : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const T &c, FormatContext &ctx) const -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "ErrorDomain={} {}", c.domain().name(),
                          c.message());
  }
};

template <>
struct fmt::formatter<std::source_location> : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const std::source_location &location, FormatContext &ctx) const
      -> decltype(ctx.out()) {
    std::string_view v{location.file_name()};
    return fmt::format_to(ctx.out(), "{}:{}", v.substr(v.find_last_of('/') + 1),
                          location.line());
  }
};

template <>
struct fmt::formatter<std::error_code> : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const std::error_code &ec, FormatContext &ctx) const
      -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "{}", ec.message());
  }
};

SYSTEM_ERROR2_NAMESPACE_BEGIN

namespace detail {
using string_ref = status_code_domain::string_ref;
using atomic_refcounted_string_ref =
    status_code_domain::atomic_refcounted_string_ref;

inline string_ref to_string_ref(const std::string &s) {
  auto p = (char *)malloc(s.size());   // NOLINT
  std::memcpy(p, s.data(), s.size());  // NOLINT
  return atomic_refcounted_string_ref{p, s.size()};
}
}  // namespace detail

template <typename T>
concept AnyhowValue =
    std::is_nothrow_move_constructible_v<T> && fmt::is_formattable<T>::value;

template <typename T>
  requires AnyhowValue<T>
struct AnyhowDomainImpl final : public status_code_domain {
  using Base = status_code_domain;

  struct value_type {
    T value;
    std::source_location location;
  };

  // 0xf1b0a9ec56db8239 ^ 0xc44f7bdeb2cc50e9 = 0x35ffd232e417d2d0
  constexpr AnyhowDomainImpl() : Base(0xf1b0a9ec56db8239) {}
  AnyhowDomainImpl(const AnyhowDomainImpl &) = default;
  AnyhowDomainImpl(AnyhowDomainImpl &&) = default;
  AnyhowDomainImpl &operator=(const AnyhowDomainImpl &) = default;
  AnyhowDomainImpl &operator=(AnyhowDomainImpl &&) = default;
  ~AnyhowDomainImpl() = default;

  string_ref name() const noexcept final {
    return string_ref("Anyhow");
  }

  payload_info_t payload_info() const noexcept final {
    return {sizeof(value_type),
            sizeof(status_code_domain *) + sizeof(value_type),
            (alignof(value_type) > alignof(status_code_domain *))
                ? alignof(value_type)
                : alignof(status_code_domain *)};
  }

  static constexpr const AnyhowDomainImpl &get();

  bool _do_failure(const status_code<void> &code) const noexcept final {
    assert(code.domain() == *this);
    (void)code;
    if constexpr (std::is_same_v<T, std::error_code>) {
      return static_cast<const status_code<AnyhowDomainImpl<T>> &>(code)
          .value()
          .value;
    }
    return true;
  }

  bool _do_equivalent(
      const status_code<void> &code1,
      const status_code<void> & /*code2*/) const noexcept final {
    assert(code1.domain() == *this);
    (void)code1;
    return false;
  }

  generic_code _generic_code(
      const status_code<void> &code) const noexcept final {
    assert(code.domain() == *this);
    (void)code;
    return errc::unknown;
  }

  string_ref _do_message(const status_code<void> &code) const noexcept final {
    assert(code.domain() == *this);
    using Self = status_code<AnyhowDomainImpl<T>>;
    const auto &value = static_cast<const Self &>(code).value();  // NOLINT
    return detail::to_string_ref(
        fmt::format("{} {}", value.location, value.value));
  }

  void _do_throw_exception(const status_code<void> &code) const final {
    assert(code.domain() == *this);
    using Self = status_code<AnyhowDomainImpl>;
    const auto &c = static_cast<const Self &>(code);  // NOLINT
    throw status_error<AnyhowDomainImpl>(c);
  }
};

template <typename T>
  requires AnyhowValue<T>
constexpr const AnyhowDomainImpl<T> AnyhowDomain = {};

template <typename T>
  requires AnyhowValue<T>
constexpr const AnyhowDomainImpl<T> &AnyhowDomainImpl<T>::get() {
  return AnyhowDomain<T>;
}

template <typename T>
  requires AnyhowValue<T>
inline system_code make_status_code(status_code<AnyhowDomainImpl<T>> value) {
  return make_nested_status_code(std::move(value));
}

template <typename Enum, typename = void>
struct HasQuickEnum : std::false_type {};

template <typename Enum>
struct HasQuickEnum<Enum, std::void_t<quick_status_code_from_enum<Enum>>>
    : std::true_type {};

template <typename Enum, typename Payload>
concept EnumPayload = std::is_enum_v<Enum> && HasQuickEnum<Enum>::value &&
                      requires {
                        { quick_status_code_from_enum<Enum>::payload_uuid };
                        {
                          quick_status_code_from_enum<Enum>::all_failures
                        } -> std::convertible_to<bool>;
                      } && fmt::is_formattable<Payload>::value &&
                      std::is_nothrow_move_constructible_v<Payload>;

template <typename Enum, typename Payload = void>
struct EnumPayloadDomainImpl;

template <typename Enum, typename Payload = void>
using EnumPayloadError = status_code<EnumPayloadDomainImpl<Enum, Payload>>;

template <typename Enum>
  requires std::is_enum_v<Enum> && HasQuickEnum<Enum>::value
struct EnumValueBase {
  using QuickEnum = quick_status_code_from_enum<Enum>;
  using QuickEnumMapping = typename QuickEnum::mapping;

  static const QuickEnumMapping *find_mapping(Enum v) {
    for (const auto &i : QuickEnum::value_mappings()) {
      if (i.value == v) {
        return &i;
      }
    }
    return nullptr;
  }
};

template <typename Enum, typename Payload = void>
struct EnumValue : EnumValueBase<Enum> {
  struct value_type {
    Enum value{};
    std::source_location loc = std::source_location::current();
  };

  static std::string to_string(const value_type &v) {
    auto m = EnumValueBase<Enum>::find_mapping(v.value);
    assert(m);
    return fmt::format("{} {}", v.loc, m->message);
  }
};

template <typename Enum, typename Payload>
  requires EnumPayload<Enum, Payload>
struct EnumValue<Enum, Payload> : EnumValueBase<Enum> {
  struct value_type {
    Enum value{};
    Payload payload{};
    std::source_location loc = std::source_location::current();
  };
  static std::string to_string(const value_type &v) {
    auto m = EnumValueBase<Enum>::find_mapping(v.value);
    assert(m);
    return fmt::format("{} {} {}", v.loc, m->message, v.payload);
  }
};

template <typename Enum, typename Payload>
struct EnumPayloadDomainImpl final : public status_code_domain {
  using Base = status_code_domain;
  using QuickEnum = quick_status_code_from_enum<Enum>;
  using EnumPayloadErrorSelf = EnumPayloadError<Enum, Payload>;

  static constexpr size_t uuid_size = detail::cstrlen(QuickEnum::payload_uuid);
  static constexpr uint64_t payload_uuid =
      detail::parse_uuid_from_pointer<uuid_size>(QuickEnum::payload_uuid);

  constexpr EnumPayloadDomainImpl() : Base(payload_uuid) {}
  EnumPayloadDomainImpl(const EnumPayloadDomainImpl &) = default;
  EnumPayloadDomainImpl(EnumPayloadDomainImpl &&) = default;
  EnumPayloadDomainImpl &operator=(const EnumPayloadDomainImpl &) = default;
  EnumPayloadDomainImpl &operator=(EnumPayloadDomainImpl &&) = default;
  ~EnumPayloadDomainImpl() = default;

  using EnumValueType = EnumValue<Enum, Payload>;
  using value_type = typename EnumValueType::value_type;

  string_ref name() const noexcept final {
    return string_ref{QuickEnum::domain_name};
  }

  payload_info_t payload_info() const noexcept final {
    return {sizeof(value_type),
            sizeof(status_code_domain *) + sizeof(value_type),
            (alignof(value_type) > alignof(status_code_domain *))
                ? alignof(value_type)
                : alignof(status_code_domain *)};
  }

  static constexpr const EnumPayloadDomainImpl &get();

  bool _do_failure(const status_code<void> &code) const noexcept final {
    assert(code.domain() == *this);
    if constexpr (QuickEnum::all_failures) {
      return true;
    } else {
      const auto &c = static_cast<const EnumPayloadErrorSelf &>(code);
      const auto *_mapping = EnumValueType::find_mapping(c.value().value);
      assert(_mapping != nullptr);
      for (auto ec : _mapping->code_mappings) {
        if (ec == errc::success) {
          return false;
        }
      }
      return true;
    }
  }

  bool _do_equivalent(const status_code<void> &code1,
                      const status_code<void> &code2) const noexcept final {
    assert(code1.domain() == *this);
    auto code1_id = code1.domain().id();
    auto code2_id = code2.domain().id();

    const auto &c1 = static_cast<const EnumPayloadErrorSelf &>(code1);
    if (code2.domain() == *this) {
      const auto &c2 = static_cast<const EnumPayloadErrorSelf &>(code2);
      return c1.value().value == c2.value().value;
    }

    assert(code1_id == payload_uuid);
    if (code1_id == (code2_id ^ 0xc44f7bdeb2cc50e9)) {
      using IndirectCode = status_code<detail::indirecting_domain<
          EnumPayloadErrorSelf, std::allocator<EnumPayloadErrorSelf>>>;
      const auto &c2 = static_cast<const IndirectCode &>(code2);
      return c1.value().value == c2.value()->sc.value().value;
    }

    if (code2.domain() == quick_status_code_from_enum_domain<Enum>) {
      using QuickCode = quick_status_code_from_enum_code<Enum>;
      const auto &c2 = static_cast<const QuickCode &>(code2);
      return c1.value().value == c2.value();
    }

    if (code2.domain() == generic_code_domain) {
      const auto &c2 = static_cast<const generic_code &>(code2);  // NOLINT
      const auto *_mapping = EnumValueType::find_mapping(c1.value().value);
      assert(_mapping != nullptr);
      for (auto ec : _mapping->code_mappings) {
        if (ec == c2.value()) {
          return true;
        }
      }
    }
    return false;
  }

  generic_code _generic_code(
      const status_code<void> &code) const noexcept final {
    assert(code.domain() == *this);
    auto value = static_cast<const EnumPayloadErrorSelf &>(code).value().value;
    const auto *_mapping = EnumValueType::find_mapping(value);
    assert(_mapping != nullptr);
    if (_mapping->code_mappings.size() > 0) {
      return *_mapping->code_mappings.begin();
    }
    return errc::unknown;
  }

  string_ref _do_message(const status_code<void> &code) const noexcept final {
    assert(code.domain() == *this);
    const auto &c = static_cast<const EnumPayloadErrorSelf &>(code);
    return detail::to_string_ref(EnumValueType::to_string(c.value()));
  }

  void _do_throw_exception(const status_code<void> &code) const final;
};

template <typename Enum, typename Payload>
constexpr EnumPayloadDomainImpl<Enum, Payload> EnumPayloadDomain = {};

template <typename Enum, typename Payload>
constexpr const EnumPayloadDomainImpl<Enum, Payload> &
EnumPayloadDomainImpl<Enum, Payload>::get() {
  return EnumPayloadDomain<Enum, Payload>;
}

template <typename Enum, typename Payload>
inline system_code make_status_code(EnumPayloadError<Enum, Payload> e) {
  return make_nested_status_code(std::move(e));
}

template <typename Enum, typename Payload>
void EnumPayloadDomainImpl<Enum, Payload>::_do_throw_exception(
    const status_code<void> &code) const {
  assert(code.domain() == *this);
  const auto &c = static_cast<const EnumPayloadErrorSelf &>(code);
  throw status_error<EnumPayloadDomainImpl<Enum, Payload>>(c);
}

/// make_error factory functions

template <typename Enum>
  requires std::is_enum_v<Enum>
EnumPayloadError<Enum> make_error(
    Enum e, std::source_location loc = std::source_location::current()) {
  return EnumPayloadError<Enum>({e, loc});
}

template <typename Enum, typename Payload>
  requires EnumPayload<Enum, Payload> &&
           std::convertible_to<Payload, std::string>
EnumPayloadError<Enum, std::string> make_error(
    Enum e, Payload &&payload,
    std::source_location loc = std::source_location::current()) {
  return EnumPayloadError<Enum, std::string>(
      {e, std::string(std::forward<Payload>(payload)), loc});
}

template <typename Enum, typename Payload>
  requires EnumPayload<Enum, Payload>
EnumPayloadError<Enum, Payload> make_error(
    Enum e, Payload &&payload,
    std::source_location loc = std::source_location::current()) {
  return EnumPayloadError<Enum, Payload>(
      {e, std::forward<Payload>(payload), loc});
}

template <typename T>
  requires fmt::is_formattable<T>::value
inline auto make_error(
    T &&value, std::source_location location = std::source_location::current())
    -> std::conditional_t<std::convertible_to<T, std::string>,
                          status_code<AnyhowDomainImpl<std::string>>,
                          status_code<AnyhowDomainImpl<T>>> {
  if constexpr (std::convertible_to<T, std::string>) {
    using StatusCode = status_code<AnyhowDomainImpl<std::string>>;
    return StatusCode({std::string(std::forward<T>(value)), location});
  } else {
    using StatusCode = status_code<AnyhowDomainImpl<T>>;
    return StatusCode({std::forward<T>(value), location});
  }
}

SYSTEM_ERROR2_NAMESPACE_END

namespace logloom {
using SYSTEM_ERROR2_NAMESPACE::make_error;

enum class GenericErrc : int8_t {
  unknown = -1,
  address_family_not_supported = EAFNOSUPPORT,
  address_in_use = EADDRINUSE,
  address_not_available = EADDRNOTAVAIL,
  already_connected = EISCONN,
  argument_list_too_long = E2BIG,
  argument_out_of_domain = EDOM,
  bad_address = EFAULT,
  bad_file_descriptor = EBADF,
  bad_message = EBADMSG,
  broken_pipe = EPIPE,
  connection_aborted = ECONNABORTED,
  connection_already_in_progress = EALREADY,
  connection_refused = ECONNREFUSED,
  connection_reset = ECONNRESET,
  cross_device_link = EXDEV,
  destination_address_required = EDESTADDRREQ,
  device_or_resource_busy = EBUSY,
  directory_not_empty = ENOTEMPTY,
  executable_format_error = ENOEXEC,
  file_exists = EEXIST,
  file_too_large = EFBIG,
  filename_too_long = ENAMETOOLONG,
  function_not_supported = ENOSYS,
  host_unreachable = EHOSTUNREACH,
  identifier_removed = EIDRM,
  illegal_byte_sequence = EILSEQ,
  inappropriate_io_control_operation = ENOTTY,
  interrupted = EINTR,
  invalid_argument = EINVAL,
  invalid_seek = ESPIPE,
  io_error = EIO,
  is_a_directory = EISDIR,
  message_size = EMSGSIZE,
  network_down = ENETDOWN,
  network_reset = ENETRESET,
  network_unreachable = ENETUNREACH,
  no_buffer_space = ENOBUFS,
  no_child_process = ECHILD,
  no_link = ENOLINK,
  no_lock_available = ENOLCK,
  no_message = ENOMSG,
  no_protocol_option = ENOPROTOOPT,
  no_space_on_device = ENOSPC,
  no_stream_resources = ENOSR,
  no_such_device_or_address = ENXIO,
  no_such_device = ENODEV,
  no_such_file_or_directory = ENOENT,
  no_such_process = ESRCH,
  not_a_directory = ENOTDIR,
  not_a_socket = ENOTSOCK,
  not_a_stream = ENOSTR,
  not_connected = ENOTCONN,
  not_enough_memory = ENOMEM,
  not_supported = ENOTSUP,
  operation_canceled = ECANCELED,
  operation_in_progress = EINPROGRESS,
  operation_not_permitted = EPERM,
  operation_not_supported = EOPNOTSUPP,
  operation_would_block = EWOULDBLOCK,
  owner_dead = EOWNERDEAD,
  permission_denied = EACCES,
  protocol_error = EPROTO,
  protocol_not_supported = EPROTONOSUPPORT,
  read_only_file_system = EROFS,
  resource_deadlock_would_occur = EDEADLK,
  resource_unavailable_try_again = EAGAIN,
  result_out_of_range = ERANGE,
  state_not_recoverable = ENOTRECOVERABLE,
  stream_timeout = ETIME,
  text_file_busy = ETXTBSY,
  timed_out = ETIMEDOUT,
  too_many_files_open_in_system = ENFILE,
  too_many_files_open = EMFILE,
  too_many_links = EMLINK,
  too_many_symbolic_link_levels = ELOOP,
  value_too_large = EOVERFLOW,
  wrong_protocol_type = EPROTOTYPE,
};

inline GenericErrc errno_to_errc(int e) {  // NOLINT
  if (e == EAFNOSUPPORT) {
    return GenericErrc::address_family_not_supported;
  }
  if (e == EADDRINUSE) {
    return GenericErrc::address_in_use;
  }
  if (e == EADDRNOTAVAIL) {
    return GenericErrc::address_not_available;
  }
  if (e == EISCONN) {
    return GenericErrc::already_connected;
  }
  if (e == E2BIG) {
    return GenericErrc::argument_list_too_long;
  }
  if (e == EDOM) {
    return GenericErrc::argument_out_of_domain;
  }
  if (e == EFAULT) {
    return GenericErrc::bad_address;
  }
  if (e == EBADF) {
    return GenericErrc::bad_file_descriptor;
  }
  if (e == EBADMSG) {
    return GenericErrc::bad_message;
  }
  if (e == EPIPE) {
    return GenericErrc::broken_pipe;
  }
  if (e == ECONNABORTED) {
    return GenericErrc::connection_aborted;
  }
  if (e == EALREADY) {
    return GenericErrc::connection_already_in_progress;
  }
  if (e == ECONNREFUSED) {
    return GenericErrc::connection_refused;
  }
  if (e == ECONNRESET) {
    return GenericErrc::connection_reset;
  }
  if (e == EXDEV) {
    return GenericErrc::cross_device_link;
  }
  if (e == EDESTADDRREQ) {
    return GenericErrc::destination_address_required;
  }
  if (e == EBUSY) {
    return GenericErrc::device_or_resource_busy;
  }
  if (e == ENOTEMPTY) {
    return GenericErrc::directory_not_empty;
  }
  if (e == ENOEXEC) {
    return GenericErrc::executable_format_error;
  }
  if (e == EEXIST) {
    return GenericErrc::file_exists;
  }
  if (e == EFBIG) {
    return GenericErrc::file_too_large;
  }
  if (e == ENAMETOOLONG) {
    return GenericErrc::filename_too_long;
  }
  if (e == ENOSYS) {
    return GenericErrc::function_not_supported;
  }
  if (e == EHOSTUNREACH) {
    return GenericErrc::host_unreachable;
  }
  if (e == EIDRM) {
    return GenericErrc::identifier_removed;
  }
  if (e == EILSEQ) {
    return GenericErrc::illegal_byte_sequence;
  }
  if (e == ENOTTY) {
    return GenericErrc::inappropriate_io_control_operation;
  }
  if (e == EINTR) {
    return GenericErrc::interrupted;
  }
  if (e == EINVAL) {
    return GenericErrc::invalid_argument;
  }
  if (e == ESPIPE) {
    return GenericErrc::invalid_seek;
  }
  if (e == EIO) {
    return GenericErrc::io_error;
  }
  if (e == EISDIR) {
    return GenericErrc::is_a_directory;
  }
  if (e == EMSGSIZE) {
    return GenericErrc::message_size;
  }
  if (e == ENETDOWN) {
    return GenericErrc::network_down;
  }
  if (e == ENETRESET) {
    return GenericErrc::network_reset;
  }
  if (e == ENETUNREACH) {
    return GenericErrc::network_unreachable;
  }
  if (e == ENOBUFS) {
    return GenericErrc::no_buffer_space;
  }
  if (e == ECHILD) {
    return GenericErrc::no_child_process;
  }
  if (e == ENOLINK) {
    return GenericErrc::no_link;
  }
  if (e == ENOLCK) {
    return GenericErrc::no_lock_available;
  }
  if (e == ENOMSG) {
    return GenericErrc::no_message;
  }
  if (e == ENOPROTOOPT) {
    return GenericErrc::no_protocol_option;
  }
  if (e == ENOSPC) {
    return GenericErrc::no_space_on_device;
  }
  if (e == ENOSR) {
    return GenericErrc::no_stream_resources;
  }
  if (e == ENXIO) {
    return GenericErrc::no_such_device_or_address;
  }
  if (e == ENODEV) {
    return GenericErrc::no_such_device;
  }
  if (e == ENOENT) {
    return GenericErrc::no_such_file_or_directory;
  }
  if (e == ESRCH) {
    return GenericErrc::no_such_process;
  }
  if (e == ENOTDIR) {
    return GenericErrc::not_a_directory;
  }
  if (e == ENOTSOCK) {
    return GenericErrc::not_a_socket;
  }
  if (e == ENOSTR) {
    return GenericErrc::not_a_stream;
  }
  if (e == ENOTCONN) {
    return GenericErrc::not_connected;
  }
  if (e == ENOMEM) {
    return GenericErrc::not_enough_memory;
  }
  if (e == ENOTSUP) {
    return GenericErrc::not_supported;
  }
  if (e == ECANCELED) {
    return GenericErrc::operation_canceled;
  }
  if (e == EINPROGRESS) {
    return GenericErrc::operation_in_progress;
  }
  if (e == EPERM) {
    return GenericErrc::operation_not_permitted;
  }
  if (e == EOPNOTSUPP) {
    return GenericErrc::operation_not_supported;
  }
  if (e == EWOULDBLOCK) {
    return GenericErrc::operation_would_block;
  }
  if (e == EOWNERDEAD) {
    return GenericErrc::owner_dead;
  }
  if (e == EACCES) {
    return GenericErrc::permission_denied;
  }
  if (e == EPROTO) {
    return GenericErrc::protocol_error;
  }
  if (e == EPROTONOSUPPORT) {
    return GenericErrc::protocol_not_supported;
  }
  if (e == EROFS) {
    return GenericErrc::read_only_file_system;
  }
  if (e == EDEADLK) {
    return GenericErrc::resource_deadlock_would_occur;
  }
  if (e == EAGAIN) {
    return GenericErrc::resource_unavailable_try_again;
  }
  if (e == ERANGE) {
    return GenericErrc::result_out_of_range;
  }
  if (e == ENOTRECOVERABLE) {
    return GenericErrc::state_not_recoverable;
  }
  if (e == ETIME) {
    return GenericErrc::stream_timeout;
  }
  if (e == ETXTBSY) {
    return GenericErrc::text_file_busy;
  }
  if (e == ETIMEDOUT) {
    return GenericErrc::timed_out;
  }
  if (e == ENFILE) {
    return GenericErrc::too_many_files_open_in_system;
  }
  if (e == EMFILE) {
    return GenericErrc::too_many_files_open;
  }
  if (e == EMLINK) {
    return GenericErrc::too_many_links;
  }
  if (e == ELOOP) {
    return GenericErrc::too_many_symbolic_link_levels;
  }
  if (e == EOVERFLOW) {
    return GenericErrc::value_too_large;
  }
  if (e == EPROTOTYPE) {
    return GenericErrc::wrong_protocol_type;
  }
  return GenericErrc::unknown;
}
}  // namespace logloom

SYSTEM_ERROR2_NAMESPACE_BEGIN
template <>
struct quick_status_code_from_enum<logloom::GenericErrc>
    : quick_status_code_from_enum_defaults<logloom::GenericErrc> {
  static constexpr auto domain_name = "logloom::GenericErrc";
  // 0xbe3fa47c5bf771af
  static constexpr auto domain_uuid = "60e74dc8-2bb0-4f19-9af0-327decfabcf2";
  // 0x7413569d6f9952c0 ^ 0xc44f7bdeb2cc50e9 = 0xb05c2d43dd550229
  static constexpr auto payload_uuid = "93d3f92b-a755-4c14-9ff6-60dd7e307d53";
  static constexpr bool all_failures = true;
  static const std::initializer_list<mapping> &value_mappings() {
    // NOLINTBEGIN
    // clang-format off
    static const std::initializer_list<mapping> v = {
        {logloom::GenericErrc::address_family_not_supported, "address_family_not_supported", {errc::address_family_not_supported}},
        {logloom::GenericErrc::address_in_use, "address_in_use", {errc::address_in_use}},
        {logloom::GenericErrc::address_not_available, "address_not_available", {errc::address_not_available}},
        {logloom::GenericErrc::already_connected, "already_connected", {errc::already_connected}},
        {logloom::GenericErrc::argument_list_too_long, "argument_list_too_long", {errc::argument_list_too_long}},
        {logloom::GenericErrc::argument_out_of_domain, "argument_out_of_domain", {errc::argument_out_of_domain}},
        {logloom::GenericErrc::bad_address, "bad_address", {errc::bad_address}},
        {logloom::GenericErrc::bad_file_descriptor, "bad_file_descriptor", {errc::bad_file_descriptor}},
        {logloom::GenericErrc::bad_message, "bad_message", {errc::bad_message}},
        {logloom::GenericErrc::broken_pipe, "broken_pipe", {errc::broken_pipe}},
        {logloom::GenericErrc::connection_aborted, "connection_aborted", {errc::connection_aborted}},
        {logloom::GenericErrc::connection_already_in_progress, "connection_already_in_progress", {errc::connection_already_in_progress}},
        {logloom::GenericErrc::connection_refused, "connection_refused", {errc::connection_refused}},
        {logloom::GenericErrc::connection_reset, "connection_reset", {errc::connection_reset}},
        {logloom::GenericErrc::cross_device_link, "cross_device_link", {errc::cross_device_link}},
        {logloom::GenericErrc::destination_address_required, "destination_address_required", {errc::destination_address_required}},
        {logloom::GenericErrc::device_or_resource_busy, "device_or_resource_busy", {errc::device_or_resource_busy}},
        {logloom::GenericErrc::directory_not_empty, "directory_not_empty", {errc::directory_not_empty}},
        {logloom::GenericErrc::executable_format_error, "executable_format_error", {errc::executable_format_error}},
        {logloom::GenericErrc::file_exists, "file_exists", {errc::file_exists}},
        {logloom::GenericErrc::file_too_large, "file_too_large", {errc::file_too_large}},
        {logloom::GenericErrc::filename_too_long, "filename_too_long", {errc::filename_too_long}},
        {logloom::GenericErrc::function_not_supported, "function_not_supported", {errc::function_not_supported}},
        {logloom::GenericErrc::host_unreachable, "host_unreachable", {errc::host_unreachable}},
        {logloom::GenericErrc::identifier_removed, "identifier_removed", {errc::identifier_removed}},
        {logloom::GenericErrc::illegal_byte_sequence, "illegal_byte_sequence", {errc::illegal_byte_sequence}},
        {logloom::GenericErrc::inappropriate_io_control_operation, "inappropriate_io_control_operation", {errc::inappropriate_io_control_operation}},
        {logloom::GenericErrc::interrupted, "interrupted", {errc::interrupted}},
        {logloom::GenericErrc::invalid_argument, "invalid_argument", {errc::invalid_argument}},
        {logloom::GenericErrc::invalid_seek, "invalid_seek", {errc::invalid_seek}},
        {logloom::GenericErrc::io_error, "io_error", {errc::io_error}},
        {logloom::GenericErrc::is_a_directory, "is_a_directory", {errc::is_a_directory}},
        {logloom::GenericErrc::message_size, "message_size", {errc::message_size}},
        {logloom::GenericErrc::network_down, "network_down", {errc::network_down}},
        {logloom::GenericErrc::network_reset, "network_reset", {errc::network_reset}},
        {logloom::GenericErrc::network_unreachable, "network_unreachable", {errc::network_unreachable}},
        {logloom::GenericErrc::no_buffer_space, "no_buffer_space", {errc::no_buffer_space}},
        {logloom::GenericErrc::no_child_process, "no_child_process", {errc::no_child_process}},
        {logloom::GenericErrc::no_link, "no_link", {errc::no_link}},
        {logloom::GenericErrc::no_lock_available, "no_lock_available", {errc::no_lock_available}},
        {logloom::GenericErrc::no_message, "no_message", {errc::no_message}},
        {logloom::GenericErrc::no_protocol_option, "no_protocol_option", {errc::no_protocol_option}},
        {logloom::GenericErrc::no_space_on_device, "no_space_on_device", {errc::no_space_on_device}},
        {logloom::GenericErrc::no_stream_resources, "no_stream_resources", {errc::no_stream_resources}},
        {logloom::GenericErrc::no_such_device_or_address, "no_such_device_or_address", {errc::no_such_device_or_address}},
        {logloom::GenericErrc::no_such_device, "no_such_device", {errc::no_such_device}},
        {logloom::GenericErrc::no_such_file_or_directory, "no_such_file_or_directory", {errc::no_such_file_or_directory}},
        {logloom::GenericErrc::no_such_process, "no_such_process", {errc::no_such_process}},
        {logloom::GenericErrc::not_a_directory, "not_a_directory", {errc::not_a_directory}},
        {logloom::GenericErrc::not_a_socket, "not_a_socket", {errc::not_a_socket}},
        {logloom::GenericErrc::not_a_stream, "not_a_stream", {errc::not_a_stream}},
        {logloom::GenericErrc::not_connected, "not_connected", {errc::not_connected}},
        {logloom::GenericErrc::not_enough_memory, "not_enough_memory", {errc::not_enough_memory}},
        {logloom::GenericErrc::not_supported, "not_supported", {errc::not_supported}},
        {logloom::GenericErrc::operation_canceled, "operation_canceled", {errc::operation_canceled}},
        {logloom::GenericErrc::operation_in_progress, "operation_in_progress", {errc::operation_in_progress}},
        {logloom::GenericErrc::operation_not_permitted, "operation_not_permitted", {errc::operation_not_permitted}},
        {logloom::GenericErrc::operation_not_supported, "operation_not_supported", {errc::operation_not_supported}},
        {logloom::GenericErrc::operation_would_block, "operation_would_block", {errc::operation_would_block}},
        {logloom::GenericErrc::owner_dead, "owner_dead", {errc::owner_dead}},
        {logloom::GenericErrc::permission_denied, "permission_denied", {errc::permission_denied}},
        {logloom::GenericErrc::protocol_error, "protocol_error", {errc::protocol_error}},
        {logloom::GenericErrc::protocol_not_supported, "protocol_not_supported", {errc::protocol_not_supported}},
        {logloom::GenericErrc::read_only_file_system, "read_only_file_system", {errc::read_only_file_system}},
        {logloom::GenericErrc::resource_deadlock_would_occur, "resource_deadlock_would_occur", {errc::resource_deadlock_would_occur}},
        {logloom::GenericErrc::resource_unavailable_try_again, "resource_unavailable_try_again", {errc::resource_unavailable_try_again}},
        {logloom::GenericErrc::result_out_of_range, "result_out_of_range", {errc::result_out_of_range}},
        {logloom::GenericErrc::state_not_recoverable, "state_not_recoverable", {errc::state_not_recoverable}},
        {logloom::GenericErrc::stream_timeout, "stream_timeout", {errc::stream_timeout}},
        {logloom::GenericErrc::text_file_busy, "text_file_busy", {errc::text_file_busy}},
        {logloom::GenericErrc::timed_out, "timed_out", {errc::timed_out}},
        {logloom::GenericErrc::too_many_files_open_in_system, "too_many_files_open_in_system", {errc::too_many_files_open_in_system}},
        {logloom::GenericErrc::too_many_files_open, "too_many_files_open", {errc::too_many_files_open}},
        {logloom::GenericErrc::too_many_links, "too_many_links", {errc::too_many_links}},
        {logloom::GenericErrc::too_many_symbolic_link_levels, "too_many_symbolic_link_levels", {errc::too_many_symbolic_link_levels}},
        {logloom::GenericErrc::value_too_large, "value_too_large", {errc::value_too_large}},
        {logloom::GenericErrc::wrong_protocol_type, "wrong_protocol_type", {errc::wrong_protocol_type}},
    };
    // clang-format on
    // NOLINTEND
    return v;
  }
};
SYSTEM_ERROR2_NAMESPACE_END
