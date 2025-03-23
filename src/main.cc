#include <fmt/base.h>
#include <fmt/format.h>
#include <cxxopts.hpp>

int main(int argc, char** argv) {
  auto desc = "Log index and search tool";
  cxxopts::Options opt("logloom", desc);

  // clang-format off
  opt.add_options()
    ("h,help", "Print help")
    ("v,version", "Print version")
    ("f,file", "Path to the log file", cxxopts::value<std::string>())
    ("o,output", "Path to the output file", cxxopts::value<std::string>())
    ;
  // clang-format on

  opt.parse_positional({"file", "output"});
  opt.positional_help("FILE OUTPUT");
  auto result = opt.parse(argc, argv);

  if (result.count("version") != 0) {
    fmt::println("Logloom version {}", LOGLOOM_VERSION);
    return 0;
  }

  if (result.count("help") != 0 || result.count("file") == 0) {
    fmt::println("{}", opt.help());
    return 0;
  }

  auto file = result["file"].as<std::string>();
  auto output = result["output"].as<std::string>();
  fmt::println("file: {}", file);
  fmt::println("output: {}", output);

  return 0;
}
