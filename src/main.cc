#include "log.hh"

#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/flags/usage.h>
#include <absl/flags/usage_config.h>
#include <absl/log/globals.h>
#include <absl/log/initialize.h>
#include <absl/strings/match.h>
#include <fmt/format.h>

#include <fcntl.h>

void setup_logger(std::string_view sv) {
  absl::SetStderrThreshold(absl::LogSeverity::kInfo);
  absl::InitializeLog();

  if (isatty(STDERR_FILENO) == 0) {
    return;
  }

  auto name = fmt::format("{}/oned.{}.log", sv, getpid());
  auto fd = open(name.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd == -1) {
    throw std::runtime_error(
        fmt::format("can not create log file: {}", strerror(errno)));
  }

  close(STDERR_FILENO);
  dup2(fd, STDERR_FILENO);
  close(fd);
}

ABSL_FLAG(std::string, log_dir, "/tmp", "the dir of log file");

void handle_args(int argc, char** argv) {
  absl::FlagsUsageConfig cfg;
  cfg.version_string = []() -> std::string { return ONED_VERSION "\n"; };
  cfg.contains_help_flags = [](absl::string_view sv) -> bool {
    return !absl::StrContains(sv, "absl");
  };
  cfg.normalize_filename = [](absl::string_view sv) -> std::string {
    auto len =
        absl::StrContains(sv, "absl") ? sv.rfind("absl/") : sizeof(ONED_ROOT);
    return {sv.data() + len, sv.length() - len};
  };

  absl::SetFlagsUsageConfig(cfg);

  absl::SetProgramUsageMessage("one editor");
  absl::ParseCommandLine(argc, argv);
}

void check_tty() {
  if (isatty(STDIN_FILENO) == 0) {
    throw std::runtime_error(
        fmt::format("stdin is not a tty: {}", strerror(errno)));
  }
  if (isatty(STDOUT_FILENO) == 0) {
    throw std::runtime_error(
        fmt::format("stdout is not a tty: {}", strerror(errno)));
  }
}

int main(int argc, char** argv) {
  try {
    handle_args(argc, argv);
    check_tty();
    setup_logger(absl::GetFlag(FLAGS_log_dir));
  } catch (const std::exception& ex) {
    fmt::println("{}", ex.what());
    return 1;
  }
  return 0;
}
