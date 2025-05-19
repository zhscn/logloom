#include "terminal_handler.hh"
#include "log.hh"

#include <fmt/format.h>

#include <fcntl.h>
#include <sys/select.h>
#include <unistd.h>
#include <array>

namespace oned {

TerminalHangler::TerminalHangler() : fd_(open("/dev/tty", O_RDWR)) {
  if (fd_ < 0) {
    throw std::runtime_error(
        fmt::format("can not open /dev/tty: {}", strerror(errno)));
  }
  INFOF("open /dev/tty: {}", fd_);
  enable_raw_mode();
}

TerminalHangler::TerminalHangler(TerminalHangler&& other) noexcept
    : orig_termios_(other.orig_termios_),
      read_buf_(std::move(other.read_buf_)),
      read_pos_(other.read_pos_),
      fd_(other.fd_) {
  other.fd_ = -1;
}

TerminalHangler& TerminalHangler::operator=(TerminalHangler&& other) noexcept {
  if (this != &other) {
    orig_termios_ = other.orig_termios_;
    read_buf_ = std::move(other.read_buf_);
    read_pos_ = other.read_pos_;
    fd_ = other.fd_;
    other.fd_ = -1;
  }
  return *this;
}

TerminalHangler::~TerminalHangler() {
  if (fd_ != -1) {
    disable_raw_mode();
    close(fd_);
    fd_ = -1;
  }
}

void TerminalHangler::enable_raw_mode() {
  if (orig_termios_) {
    return;
  }

  INFOF("enable raw mode");

  termios t{};
  tcgetattr(fd_, &t);
  orig_termios_ = t;
  t.c_iflag &=
      ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  t.c_oflag &= ~OPOST;
  t.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  t.c_lflag |= NOFLSH;
  t.c_cflag &= ~(CSIZE | PARENB);
  t.c_cflag |= CS8;
  t.c_cc[VMIN] = t.c_cc[VTIME] = 0;
  tcsetattr(fd_, TCSANOW, &t);
}

void TerminalHangler::disable_raw_mode() {
  if (!orig_termios_) {
    return;
  }
  INFOF("disable raw mode");
  tcsetattr(fd_, TCSAFLUSH, &*orig_termios_);
  orig_termios_.reset();
}

bool TerminalHangler::readable() const {
  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(STDIN_FILENO, &read_fds);
  struct timeval timeout = {.tv_sec = 0, .tv_usec = 0};
  return select(fd_ + 1, &read_fds, nullptr, nullptr, &timeout) > 0;
}

std::optional<char> TerminalHangler::read_char() {
  if (read_pos_ < read_buf_.size()) {
    return read_buf_[read_pos_++];
  }
  read_buf_.resize(0);
  read_pos_ = 0;
  if (!readable()) {
    return std::nullopt;
  }
  std::array<char, 128> buf{};
  ssize_t n = read(fd_, buf.data(), buf.size());
  if (n <= 0) {
    throw std::runtime_error(
        fmt::format("Failed to read from terminal: {}", strerror(errno)));
  }
  INFOF("read {} bytes from terminal", n);
  read_buf_.insert(read_buf_.end(), buf.data(), buf.data() + n);  // NOLINT
  return read_buf_[read_pos_++];
}

void TerminalHangler::write(std::span<char> buf) {  // NOLINT
  auto n = ::write(fd_, buf.data(), buf.size());
  if (n == -1) {
    throw std::runtime_error(
        fmt::format("Failed to write to terminal: {}", strerror(errno)));
  }
  ::write(fd_, "\r\n", 2);
}

}  // namespace oned
