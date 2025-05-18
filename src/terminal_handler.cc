#include "terminal_handler.hh"
#include "log.hh"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>
#include <array>

namespace oned {

TerminalHangler::TerminalHangler() : fd_(open("/dev/tty", O_RDWR)) {
  if (fd_ == -1) {
    throw std::runtime_error(
        fmt::format("can not open /dev/tty: {}", strerror(errno)));
  }
  enable_raw_mode();
}

TerminalHangler::~TerminalHangler() {
  if (fd_ != -1) {
    disable_raw_mode();
    close(fd_);
  }
}

void TerminalHangler::enable_raw_mode() {
  if (orig_termios_) {
    return;
  }

  char buf[] =
      "\x1b[?1049h"  // alternative screen
      "\x1b[?1004h"  // enable focus notify
      "\x1b[22t"     // push window title
      "\x1b[>5u"     // enable kitty protocol
      "\x1b[?25l"    // hide cursor
      "\x1b[?2004h"  // brackted-paste events
      ;

  ::write(fd_, buf, sizeof(buf));

  termios t{};
  tcgetattr(fd_, &t);
  orig_termios_ = t;
  verse_key_ = t.c_cc[VERASE];
  t.c_iflag &=
      ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  t.c_oflag &= ~OPOST;
  t.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  t.c_lflag |= NOFLSH;
  t.c_cflag &= ~(CSIZE | PARENB);
  t.c_cflag |= CS8;
  t.c_cc[VMIN] = 0;
  t.c_cc[VTIME] = 0;
  tcsetattr(fd_, TCSANOW, &t);
}

void TerminalHangler::disable_raw_mode() {
  if (!orig_termios_) {
    return;
  }
  char buf[] =
      "\x1b[?2004l"  // brackted-paste events
      "\x1b[?25h"    // show cursor
      "\x1b[<u"      // disable kitty protocol
      "\x1b[23t"     // pop window title
      "\x1b[?1004l"  // disable focus notify
      "\x1b[?1049l"  // exit alternative screen
      "\x1b[m"       // reset terminal
      ;
  ::write(fd_, buf, sizeof(buf));
  tcsetattr(fd_, TCSAFLUSH, &*orig_termios_);
  orig_termios_.reset();
}

bool TerminalHangler::readable() const {
  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(fd_, &read_fds);
  struct timeval timeout = {.tv_sec = 0, .tv_usec = 0};
  return select(fd_ + 1, &read_fds, nullptr, nullptr, &timeout) > 0;
}

template <typename ReadChar>
struct KeyParser {
  ReadChar read_char_;
  termios &raw;

  std::optional<char> read_char() {
    return read_char_();
  }

  std::optional<Key> next_key() {
    auto c = read_char();
    if (!c) {
      return std::nullopt;
    }
    if (*c == '\x1b') {
      if (auto next = read_char(); next) {
        if (*next == '[') {
          return parse_csi();
        }
        if (*next == 'O') {
          return parse_ss3();
        }
        return Key{(uint32_t)*next, Key::Alt};
      }
      return Key{Key::Escape, Key::None};
    }
    if (*c == 9) {
      return Key{Key::Tab, Key::None};
    }
    if (*c == 13) {
      return Key{Key::Return, Key::None};
    }
    if (*c == ' ') {
      return Key{Key::Space, Key::None};
    }
    if (*c == raw.c_cc[VERASE]) {
      return Key{Key::Backspace, Key::None};
    }
    return Key{(uint32_t)*c, Key::None};
  }

private:
  std::optional<Key> parse_ss3() {
    return std::nullopt;
  }
  std::optional<Key> parse_csi() {
    auto next_char = [this] { return read_char().value_or(0xff); };
    int params[16][4]{};
    auto c = next_char();
    [[maybe_unused]] char private_mode = 0;
    if (c == '?' || c == '<' || c == '=' || c == '>') {
      private_mode = c;
      c = next_char();
    }
    for (int count = 0, subcount = 0; count < 16 && c >= 0x30 && c <= 0x3f;
         c = next_char()) {
      if (isdigit(c)) {
        auto &pos = params[count][subcount];
        pos = pos * 10 + c - '0';
      } else if (c == ':' && subcount < 3) {
        subcount++;
      } else if (c == ';') {
        count++;
        subcount = 0;
      } else {
        return std::nullopt;
      }
    }
    if (c != '$' && (c < 0x40 || c > 0x7e)) {
      return std::nullopt;
    }
    auto masked_key = [&](uint32_t key, uint32_t shifted_key = 0) {
      int mask = std::max(params[1][0] - 1, 0);
      unsigned char mod = Key::None;
      if (mask & 1) {
        mod |= Key::Shift;
      }
      if (mask & 2) {
        mod |= Key::Alt;
      }
      if (mask & 4) {
        mod |= Key::Ctrl;
      }
      if (shifted_key != 0 && (mod & Key::Shift)) {
        mod &= ~Key::Shift;
        key = shifted_key;
      }
      return Key{key, static_cast<Key::Modifer>(mod)};
    };
    switch (c) {
    case 'A':
      return masked_key(Key::Up);
    case 'B':
      return masked_key(Key::Down);
    case 'C':
      return masked_key(Key::Right);
    case 'D':
      return masked_key(Key::Left);
    case 'E':
      return masked_key('5');  // Numeric keypad 5
    case 'F':
      return masked_key(Key::End);  // PC/xterm style
    case 'H':
      return masked_key(Key::Home);  // PC/xterm style
    case 'P':
      return masked_key(Key::F1);
    case 'Q':
      return masked_key(Key::F2);
    case 'R':
      return masked_key(Key::F3);
    case 'S':
      return masked_key(Key::F4);
    case '~': {
      if (params[0][0] == 200) {
        INFOF("start paste");
      } else if (params[0][0] == 201) {
        INFOF("end paste");
      }
      return std::nullopt;
    }
    case 'u': {
      uint32_t key;
      auto convert = [this](uint32_t c) -> uint32_t {
        if (c == (0x1f & 'm')) {
          return Key::Return;
        }
        if (c == (0x1f & 'i')) {
          return Key::Tab;
        }
        if (c == ' ') {
          return Key::Space;
        }
        if (c == raw.c_cc[VERASE]) {
          return Key::Backspace;
        }
        if (c == 127) {
          return Key::Delete;
        }
        if (c == 27) {
          return Key::Escape;
        }
        return c;
      };
      INFOF("{}", params[0]);
      key = convert(params[0][0]);
      return masked_key(key, convert(params[0][1]));
    }
    case 'I':
      return Key{Key::FocusIn, Key::None};
    case 'O':
      return Key{Key::FocusOut, Key::None};
    }
    return std::nullopt;
  }
};

std::optional<char> TerminalHangler::read_char() {
  if (read_pos_ < read_buf_.size()) {
    return read_buf_[read_pos_++];
  }

  if (!readable()) {
    read_buf_.resize(0);
    read_pos_ = 0;
    return std::nullopt;
  }

  std::array<char, 128> buf{};
  ssize_t n = read(fd_, buf.data(), buf.size());
  if (n <= 0) {
    throw std::runtime_error(
        fmt::format("Failed to read from terminal: {}", strerror(errno)));
  }
  if (n == 1) {
    return buf[0];
  }
  INFOF("read {} bytes", n);
  read_buf_.insert(read_buf_.end(), buf.data(), buf.data() + n);  // NOLINT
  return read_buf_[read_pos_++];
}

std::optional<Key> TerminalHangler::get_next_key() {
  KeyParser parser{[this] {
                     auto c = read_char();
                     if (c) {
                       INFOF("char int {}", int(*c));
                     }
                     return c;
                   },
                   *orig_termios_};
  return parser.next_key();
}

void TerminalHangler::write(std::span<char> buf) {  // NOLINT
  auto n = ::write(fd_, buf.data(), buf.size());
  if (n == -1) {
    throw std::runtime_error(
        fmt::format("Failed to write to terminal: {}", strerror(errno)));
  }
  ::write(fd_, "\r\n", 2);
}

std::pair<int, int> TerminalHangler::get_size() const {
  struct winsize ws;
  if (ioctl(fd_, TIOCGWINSZ, &ws) != 0) {
    throw std::runtime_error("Failed to get window size");
  }
  return std::make_pair<int, int>(ws.ws_row, ws.ws_col);
}

}  // namespace oned
