// IWYU pragma: always_keep

#pragma once

#include <absl/log/log.h>
#include <fmt/base.h>

#define LOG_FNAME(name) static constexpr const char *FNAME = #name

#define LOG_IMPLF(_log, _serverity, _spec, ...)                   \
  do {                                                            \
    char buf[4096];                                               \
    fmt::format_to_result res;                                    \
    _log(_serverity) << [&buf, &res]() -> std::string_view {      \
      res = fmt::format_to(buf, "{} " _spec, FNAME, __VA_ARGS__); \
      return std::string_view(buf, res.out - buf);                \
    }();                                                          \
    if (res.truncated) [[unlikely]] {                             \
      LOG(WARNING) << "the last log entry is truncated";          \
    }                                                             \
  } while (false);

#define FATALF(...) LOG_IMPLF(LOG, FATAL, __VA_ARGS__)
#define ERRORF(...) LOG_IMPLF(LOG, ERROR, __VA_ARGS__)
#define WARNF(...) LOG_IMPLF(LOG, WARNING, __VA_ARGS__)
#define INFOF(...) LOG_IMPLF(VLOG, 0, __VA_ARGS__)
#define DEBUGF(...) LOG_IMPLF(VLOG, 1, __VA_ARGS__)
#define TRACEF(...) LOG_IMPLF(VLOG, 2, __VA_ARGS__)
