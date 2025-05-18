#pragma once

#include "noncopyable.hh"

#include <boost/container/static_vector.hpp>

#include <termios.h>
#include <optional>
#include <span>

namespace oned {

class TerminalHangler : public NonCopyable {
public:
  TerminalHangler();
  TerminalHangler(TerminalHangler&&) noexcept;
  TerminalHangler& operator=(TerminalHangler&&) noexcept;
  ~TerminalHangler();

  void enable_raw_mode();
  void disable_raw_mode();
  bool readable() const;
  std::optional<char> read_char();
  void write(std::span<char> buf);

private:
  std::optional<termios> orig_termios_;
  boost::container::static_vector<char, 128> read_buf_;
  size_t read_pos_ = 0;
  int fd_;
};

}  // namespace oned
