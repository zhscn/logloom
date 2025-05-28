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

/**
 * setup_logger
 *
 * log level is handled by the flags defined in absl::log_flags library.
 *
 * ERROR: --stderrthreshold=0 (default level)
 * WARNING: --stderrthreshold=1
 * INFO: --stderrthreshold=2
 * DEBUG: --stderrthreshold=2 --v=1
 * TRACE: --stderrthreshold=2 --v=2
 */
void setup_logger(std::string_view sv) {
  absl::InitializeLog();

  if (isatty(STDERR_FILENO) == 0) {
    return;
  }

  // redirect stderr to a file when it is attached to a terminal

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

ABSL_FLAG(std::string, log_dir, "/tmp",
          "The directory to store log files when stderr is not redirected");

void handle_args(int argc, char** argv) {
  absl::FlagsUsageConfig cfg;
  cfg.version_string = []() -> std::string { return ONED_VERSION "\n"; };
  cfg.contains_help_flags = [](absl::string_view sv) -> bool {
    return !absl::StrContains(sv, "absl");
  };
  cfg.normalize_filename = [](absl::string_view sv) -> std::string {
    auto len = sizeof(ONED_ROOT);
    if (auto n = sv.rfind("oned/"); n != absl::string_view::npos) {
      len = n;
    }
    return {sv.data() + len, sv.length() - len};
  };

  absl::SetFlagsUsageConfig(cfg);
  absl::SetProgramUsageMessage("one editor");
  absl::ParseCommandLine(argc, argv);
}

int main(int argc, char** argv) {
  try {
    handle_args(argc, argv);
    setup_logger(absl::GetFlag(FLAGS_log_dir));
  } catch (const std::exception& ex) {
    ERRORF("{}", ex.what());
    return 1;
  }
  return 0;
}
