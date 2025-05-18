#pragma once

#include "noncopyable.hh"
#include "key.hh"

#include <boost/container/static_vector.hpp>

#include <termios.h>
#include <optional>
#include <span>

namespace oned {

class TerminalHangler : public NonCopyable {
public:
  TerminalHangler();
  TerminalHangler(TerminalHangler&&) = delete;
  TerminalHangler& operator=(TerminalHangler&&) = delete;
  ~TerminalHangler();

  void enable_raw_mode();
  void disable_raw_mode();
  bool readable() const;
  std::optional<char> read_char();
  std::optional<Key> get_next_key();
  void write(std::span<char> buf);
  std::pair<int, int> get_size() const;

private:
  boost::container::static_vector<char, 64> read_buf_;
  std::optional<termios> orig_termios_;
  size_t read_pos_ = 0;
  int fd_;
  char verse_key_;
};

}  // namespace oned
