#include "outcome.hh"

#include <fmt/format.h>

namespace oned {
Result<void> foo() {
  return make_error("foo");
}
}  // namespace oned

int main(int argc, const char** argv) {
  try {
    auto res = oned::foo();
    if (!res) {
      fmt::println("{}", res.error());
    }
  } catch (const std::exception& ex) {
    return 1;
  }
  return 0;
}
