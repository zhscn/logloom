#pragma once

namespace logloom {
struct NonCopyable {
  NonCopyable() = default;
  NonCopyable(const NonCopyable &) = delete;
  NonCopyable(NonCopyable &&) = default;
  NonCopyable &operator=(const NonCopyable &) = delete;
  NonCopyable &operator=(NonCopyable &&) = default;
  ~NonCopyable() = default;
};
}  // namespace logloom
