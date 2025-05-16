// IWYU pragma: always_keep

#pragma once

#include <absl/log/log.h>
#include <fmt/base.h>

#define LOG_FNAME(name) static constexpr const char *FNAME = #name

#define IMPL_LOGF_(_log, _serverity, _spec, ...)                       \
  /* NOLINTBEGIN */                                                    \
  do {                                                                 \
    char LOG_IMPL__buf[4096];                                          \
    _log(_serverity) << [&] {                                          \
      auto res = fmt::format_to(LOG_IMPL__buf, "{} " _spec,            \
                                FNAME __VA_OPT__(, ) __VA_ARGS__);     \
      if (res.truncated) [[unlikely]] {                                \
        LOG(WARNING) << "the next log is truncated";                   \
      }                                                                \
      return std::string_view(LOG_IMPL__buf, res.out - LOG_IMPL__buf); \
    }();                                                               \
  } while (false); /* NOLINTEND */

#define FATALF(...) IMPL_LOGF_(LOG, FATAL, __VA_ARGS__)
#define ERRORF(...) IMPL_LOGF_(LOG, ERROR, __VA_ARGS__)
#define WARNF(...) IMPL_LOGF_(LOG, WARNING, __VA_ARGS__)
#define INFOF(...) IMPL_LOGF_(VLOG, 0, __VA_ARGS__)
#define DEBUGF(...) IMPL_LOGF_(VLOG, 1, __VA_ARGS__)
#define TRACEF(...) IMPL_LOGF_(VLOG, 2, __VA_ARGS__)
