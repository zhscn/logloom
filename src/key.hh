#pragma once

#include <fmt/format.h>

#include <stdint.h>

namespace oned {

struct Key {
  enum Modifer : unsigned char {
    None = 0,
    Shift = 1 << 0,
    Alt = 1 << 1,
    Ctrl = 1 << 2,
  };

  enum FunctionalKey {
    Backspace = 0x110000,
    Delete,
    Escape,
    Return,
    Up,
    Down,
    Left,
    Right,
    PageUp,
    PageDown,
    Home,
    End,
    Insert,
    Tab,
    Space,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    FocusIn,
    FocusOut,
  };

  uint32_t key;
  Modifer modifier;
};

}  // namespace oned

template <>
struct fmt::formatter<oned::Key> : public fmt::formatter<std::string_view> {
  auto format(const oned::Key &key, fmt::format_context &ctx) const
      -> decltype(ctx.out()) {
    if (key.modifier & oned::Key::Ctrl) {
      fmt::format_to(ctx.out(), "C-");
    }
    if (key.modifier & oned::Key::Alt) {
      fmt::format_to(ctx.out(), "M-");
    }
    switch (key.key) {
    case oned::Key::Backspace:
      return fmt::format_to(ctx.out(), "Backspace");
    case oned::Key::Delete:
      return fmt::format_to(ctx.out(), "Delete");
    case oned::Key::Escape:
      return fmt::format_to(ctx.out(), "Escape");
    case oned::Key::Return:
      return fmt::format_to(ctx.out(), "Return");
    case oned::Key::Up:
      return fmt::format_to(ctx.out(), "Up");
    case oned::Key::Down:
      return fmt::format_to(ctx.out(), "Down");
    case oned::Key::Left:
      return fmt::format_to(ctx.out(), "Left");
    case oned::Key::Right:
      return fmt::format_to(ctx.out(), "Right");
    case oned::Key::PageUp:
      return fmt::format_to(ctx.out(), "PageUp");
    case oned::Key::PageDown:
      return fmt::format_to(ctx.out(), "PageDown");
    case oned::Key::Home:
      return fmt::format_to(ctx.out(), "Home");
    case oned::Key::End:
      return fmt::format_to(ctx.out(), "End");
    case oned::Key::Insert:
      return fmt::format_to(ctx.out(), "Insert");
    case oned::Key::Tab:
      return fmt::format_to(ctx.out(), "Tab");
    case oned::Key::Space:
      return fmt::format_to(ctx.out(), "Space");
    case oned::Key::F1:
      return fmt::format_to(ctx.out(), "F1");
    case oned::Key::F2:
      return fmt::format_to(ctx.out(), "F2");
    case oned::Key::F3:
      return fmt::format_to(ctx.out(), "F3");
    case oned::Key::F4:
      return fmt::format_to(ctx.out(), "F4");
    case oned::Key::F5:
      return fmt::format_to(ctx.out(), "F5");
    case oned::Key::F6:
      return fmt::format_to(ctx.out(), "F6");
    case oned::Key::F7:
      return fmt::format_to(ctx.out(), "F7");
    case oned::Key::F8:
      return fmt::format_to(ctx.out(), "F8");
    case oned::Key::F9:
      return fmt::format_to(ctx.out(), "F9");
    case oned::Key::F10:
      return fmt::format_to(ctx.out(), "F10");
    case oned::Key::F11:
      return fmt::format_to(ctx.out(), "F11");
    case oned::Key::F12:
      return fmt::format_to(ctx.out(), "F12");
    case oned::Key::FocusIn:
      return fmt::format_to(ctx.out(), "FocusIn");
    case oned::Key::FocusOut:
      return fmt::format_to(ctx.out(), "FocusOut");
    default:
      if (key.key <= std::numeric_limits<char>::max()) {
        return fmt::format_to(ctx.out(), "{}", (char)key.key);
      } else {
        return fmt::format_to(ctx.out(), "{}", key.key);
      }
    }
  }
};
