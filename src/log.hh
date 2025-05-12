// IWYU pragma: always_keep

#pragma once

#include <absl/log/log.h>
#include <fmt/base.h>

#define LOG_FNAME(name) static constexpr const char *FNAME = #name

#define LOG_IMPLF(_log, _serverity, _spec, ...)                           \
  /* NOLINTBEGIN */                                                       \
  do {                                                                    \
    char LOG_IMPL__buf[4096];                                             \
    _log(_serverity) << [&] {                                             \
      auto res =                                                          \
          fmt::format_to(LOG_IMPL__buf, "{} " _spec, FNAME, __VA_ARGS__); \
      if (res.truncated) [[unlikely]] {                                   \
        LOG(WARNING) << "the next log is truncated";                      \
      }                                                                   \
      return std::string_view(LOG_IMPL__buf, res.out - LOG_IMPL__buf);    \
    }();                                                                  \
  } while (false); /* NOLINTEND */

#define FATALF(...) LOG_IMPLF(LOG, FATAL, __VA_ARGS__)
#define ERRORF(...) LOG_IMPLF(LOG, ERROR, __VA_ARGS__)
#define WARNF(...) LOG_IMPLF(LOG, WARNING, __VA_ARGS__)
#define INFOF(...) LOG_IMPLF(VLOG, 0, __VA_ARGS__)
#define DEBUGF(...) LOG_IMPLF(VLOG, 1, __VA_ARGS__)
#define TRACEF(...) LOG_IMPLF(VLOG, 2, __VA_ARGS__)
