#pragma once

#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <utility>

namespace logloom {

/// legend notion:
/// X is black node, [Y] is red node, {Z} is double black node

class RBTree {
public:
  void insert(uint64_t key, uint64_t value) {
    auto [new_root, inserted] = Node::insert_into(root_, key, value);
    root_ = new_root;
    if (inserted) {
      root_->color_ = Node::Color::Black;
      count_++;
    }
  }

  bool remove(uint64_t key) {
    auto [new_root, removed] = Node::remove_from(root_, key);
    root_ = new_root;
    if (removed) {
      root_->color_ = Node::Color::Black;
      count_--;
    }
    return removed;
  }

  std::optional<uint64_t> get(uint64_t key) const {
    return Node::get(root_, key);
  }

  bool empty() const {
    return count_ == 0;
  }

  bool is_valid() const {
    if (root_ && root_->color_ != Node::Color::Black) {
      return false;
    }
    return Node::check_invariant(root_);
  }

  void print() const {
    Node::print_tree(root_, Node::Direction::Self, 0);
  }

private:
  struct Node : std::enable_shared_from_this<Node> {
    using Ptr = std::shared_ptr<Node>;

    enum class Color : uint8_t {
      Red,
      Black,
      DoubleBlack,
      DoubleBlackNil,
    };

    enum class Direction : uint8_t {
      Self,
      Left,
      Right,
    };

    Node(Ptr left, Ptr right, uint64_t weight, uint64_t value, Color color)
        : children_{.left_ = std::move(left), .right_ = std::move(right)},
          weight_(weight),
          value_(value),
          color_(color) {}

    struct Children {
      Ptr left_;
      Ptr right_;
    } children_{};
    uint64_t weight_;
    uint64_t value_;
    Color color_;

    static std::pair<Ptr, Ptr> children(const Ptr &node) {
      if (!node || node->color_ == Color::DoubleBlackNil) {
        return {nullptr, nullptr};
      }
      return {node->children_.left_, node->children_.right_};
    }

    bool no_children() const {
      return !children_.left_ && !children_.right_;
    }

    bool single_child() const {
      return !!children_.left_ != !!children_.right_;
    }

    bool full_children() const {
      return children_.left_ && children_.right_;
    }

    bool is_black(Direction d = Direction::Self) const {
      auto _is_black = [](Color c) {
        return c == Color::Black || c == Color::DoubleBlack ||
               c == Color::DoubleBlackNil;
      };
      switch (d) {
      case Direction::Self:
        return _is_black(color_);
      case Direction::Left:
        return !children_.left_ || _is_black(children_.left_->color_);
      case Direction::Right:
        return !children_.right_ || _is_black(children_.right_->color_);
      default:
        __builtin_unreachable();
      }
    }

    bool is_single_black(Direction d = Direction::Self) const {
      auto _is_n = [](Color c) { return c == Color::Black; };
      switch (d) {
      case Direction::Self:
        return _is_n(color_);
      case Direction::Left:
        return !children_.left_ || _is_n(children_.left_->color_);
      case Direction::Right:
        return !children_.right_ || _is_n(children_.right_->color_);
      default:
        __builtin_unreachable();
      }
    }

    bool is_red(Direction d = Direction::Self) const {
      auto _is_red = [](Color c) { return c == Color::Red; };
      switch (d) {
      case Direction::Self:
        return _is_red(color_);
      case Direction::Left:
        return children_.left_ && _is_red(children_.left_->color_);
      case Direction::Right:
        return children_.right_ && _is_red(children_.right_->color_);
      default:
        __builtin_unreachable();
      }
    }

    bool is_double_black(Direction d = Direction::Self) const {
      auto _is_bb = [](Color c) {
        return c == Color::DoubleBlack || c == Color::DoubleBlackNil;
      };
      switch (d) {
      case Direction::Self:
        return _is_bb(color_);
      case Direction::Left:
        return children_.left_ && _is_bb(children_.left_->color_);
      case Direction::Right:
        return children_.right_ && _is_bb(children_.right_->color_);
      default:
        __builtin_unreachable();
      }
    }

    bool is_double_black_nil(Direction d = Direction::Self) const {
      auto _is_bb_nil = [](Color c) { return c == Color::DoubleBlackNil; };
      switch (d) {
      case Direction::Self:
        return _is_bb_nil(color_);
      case Direction::Left:
        return children_.left_ && _is_bb_nil(children_.left_->color_);
      case Direction::Right:
        return children_.right_ && _is_bb_nil(children_.right_->color_);
      default:
        __builtin_unreachable();
      }
    }

    Ptr dup_with_left(Ptr new_left) const {
      return std::make_shared<Node>(std::move(new_left), children_.right_,
                                    weight_, value_, color_);
    }

    Ptr dup_with_right(Ptr new_right) const {
      return std::make_shared<Node>(children_.left_, std::move(new_right),
                                    weight_, value_, color_);
    }

    Ptr dup_with_child(Ptr new_left, Ptr new_right) const {
      return std::make_shared<Node>(std::move(new_left), std::move(new_right),
                                    weight_, value_, color_);
    }

    Ptr dup_with_child_and_color(Ptr new_left, Ptr new_right,
                                 Color color) const {
      return std::make_shared<Node>(std::move(new_left), std::move(new_right),
                                    weight_, value_, color);
    }

    Ptr dup_with_color(Color color) const {
      return std::make_shared<Node>(children_.left_, children_.right_, weight_,
                                    value_, color);
    }

    Ptr dup_with_value(uint64_t value) const {
      return std::make_shared<Node>(children_.left_, children_.right_, weight_,
                                    value, color_);
    }

    Ptr to_single_black() {
      // this function is only called on double black nodes, at which point
      // the new tree has not been rebalanced yet, so we can just turn the
      // current double black node into a black node without allocating a new
      // node
      assert(is_double_black());
      if (is_double_black_nil()) {
        return nullptr;
      }
      assert(color_ == Color::DoubleBlack);
      color_ = Color::Black;
      return shared_from_this();
    }

    static Ptr new_leaf(uint64_t key, uint64_t value, Color c = Color::Red) {
      return std::make_shared<Node>(nullptr, nullptr, key, value, c);
    }

    static std::optional<uint64_t> get(const Ptr &node, uint64_t key) {
      if (!node) {
        return std::nullopt;
      }
      if (node->weight_ == key) {
        return node->value_;
      }
      if (node->weight_ < key) {
        return get(node->children_.right_, key);
      }
      return get(node->children_.left_, key);
    }

    static Ptr balance(Ptr node) {  // NOLINT
      assert(node);
      if (node->is_red()) {
        // Okasaki insertion balance does not apply to red nodes directly
        return node;
      }

      auto &Z = node;

      if (Z->is_single_black()) {
        // Okasaki insertion balance procedure

        if (Z->is_red(Direction::Left)) {
          auto [Y, d] = children(Z);

          if (Y->is_red(Direction::Left)) {
            /**
             * LL
             *
             *         Z                           |
             *        / \                  [Y]     |
             *      [Y]  d                 / \     |
             *      / \       =====>      X   Z    |
             *    [X]  c                 / \ / \   |
             *    / \                   a  b c  d  |
             *   a   b                             |
             */
            auto [X, c] = children(Y);
            auto [a, b] = children(X);
            return Y->dup_with_child(X->dup_with_color(Color::Black),
                                     Z->dup_with_left(c));
          }  // Y->is_red(Direction::Left)

          if (Y->is_red(Direction::Right)) {
            /**
             * LR
             *
             *       Z                           |
             *      / \                  [X]     |
             *    [Y]  d                 / \     |
             *    / \       =====>      Y   Z    |
             *   a  [X]                / \ / \   |
             *      / \               a  b c  d  |
             *     b   c                         |
             */
            auto [a, X] = children(Y);
            auto [b, c] = children(X);
            return X->dup_with_child(
                Y->dup_with_child_and_color(a, b, Color::Black),
                Z->dup_with_left(c));
          }  // Y->is_red(Direction::Right)
        }  // Z->is_red(Direction::Left)

        if (Z->is_red(Direction::Right)) {
          auto [a, Y] = children(Z);

          if (Y->is_red(Direction::Left)) {
            /**
             * RL
             *
             *      Z                             |
             *     / \                    [X]     |
             *    a  [Y]                  / \     |
             *       / \     =====>      Z   Y    |
             *     [X]  d               / \ / \   |
             *     / \                 a  b c  d  |
             *    b   c                           |
             */
            auto [X, d] = children(Y);
            auto [b, c] = children(X);
            return X->dup_with_child(
                Z->dup_with_right(b),
                Y->dup_with_child_and_color(c, d, Color::Black));
          }  // Y->is_red(Direction::Left)

          if (Y->is_red(Direction::Right)) {
            /**
             * RR
             *
             *      Z                               |
             *     / \                      [Y]     |
             *    a  [Y]                    / \     |
             *       / \       =====>      Z   X    |
             *      b  [X]                / \ / \   |
             *         / \               a  b c  d  |
             *        c   d                         |
             */
            auto [b, X] = children(Y);
            auto [c, d] = children(X);
            return Y->dup_with_child(Z->dup_with_right(b),
                                     X->dup_with_color(Color::Black));
          }  // Y->is_red(Direction::Right)
        }  // Z->is_red(Direction::Right)
      }  // Z->color_ == Color::Black

      if (Z->is_double_black()) {
        // double black nodes elimination procedure

        assert(!Z->is_double_black_nil());
        if (Z->is_red(Direction::Left) &&
            Z->children_.left_->is_red(Direction::Right)) {
          /**
           * LR
           *
           *      {Z}                          |
           *      / \                   X      |
           *    [Y]  d                 / \     |
           *    / \       =====>      Y   Z    |
           *   a  [X]                / \ / \   |
           *      / \               a  b c  d  |
           *     b   c                         |
           */
          auto [Y, d] = children(Z);
          auto [a, X] = children(Y);
          auto [b, c] = children(X);
          assert(Y->is_black(Direction::Left));
          return X->dup_with_child_and_color(
              Y->dup_with_child_and_color(a, b, Color::Black),
              Z->dup_with_child_and_color(c, d, Color::Black), Color::Black);
        }  // Z->is_red(Direction::Left) &&
           // Z->children_.left_->is_red(Direction::Right)

        if (Z->is_red(Direction::Right) &&
            Z->children_.right_->is_red(Direction::Left)) {
          /**
           * RL
           *
           *     {Z}                            |
           *     / \                     X      |
           *    a  [Y]                  / \     |
           *       / \     =====>      Z   Y    |
           *     [X]  d               / \ / \   |
           *     / \                 a  b c  d  |
           *    b   c                           |
           */
          auto [a, Y] = children(Z);
          auto [X, d] = children(Y);
          auto [b, c] = children(X);
          assert(!d->is_red());
          return X->dup_with_child_and_color(
              Z->dup_with_child_and_color(a, b, Color::Black),
              Y->dup_with_child_and_color(c, d, Color::Black), Color::Black);
        }  // Z->is_red(Direction::Right) &&
           // Z->children_.right_->is_red(Direction::Left)
      }  // Z->is_double_black()

      return node;
    }

    static std::pair<Ptr, bool> insert_into(Ptr &node, uint64_t key,
                                            uint64_t value) {
      if (!node) {
        return {new_leaf(key, value), true};
      }
      if (node->weight_ == key) {
        return {node->dup_with_value(value), false};
      }
      auto [left, right] = children(node);
      if (node->weight_ < key) {
        auto [new_right, inserted] = insert_into(right, key, value);
        return {balance(node->dup_with_right(std::move(new_right))), inserted};
      }
      auto [new_left, inserted] = insert_into(left, key, value);
      return {balance(node->dup_with_left(std::move(new_left))), inserted};
    }

    static Ptr rotate(const Ptr &node) {
      /**
       *      Y      |
       *     / \     |
       *    X   Z    |
       *   / \ / \   |
       *  a  b c  d  |
       */
      auto &Y = node;
      auto [X, Z] = children(Y);
      auto [a, b] = children(X);
      auto [c, d] = children(Z);

      if (Y->is_red()) {
        if (Y->is_double_black(Direction::Left)) {
          /**
           *                           Z    |
           *     [Y]                  / \   |
           *     / \      ====>     [Y]  d  |
           *   {X}  Z               / \     |
           *   / \ / \             X   c    |
           *  a  b c  d           / \       |
           *                     a   b      |
           */
          assert(Z->is_black());
          auto new_X = X->to_single_black();
          auto new_Y = Y->dup_with_child(std::move(new_X), c);
          return balance(Z->dup_with_left(std::move(new_Y)));
        }  // Y->is_double_black(Direction::Left)

        if (Y->is_double_black(Direction::Right)) {
          /**
           *                         X        |
           *     [Y]                / \       |
           *     / \       ====>   a  [Y]     |
           *    X  {Z}                / \     |
           *   / \ / \               b   Z    |
           *  a  b c  d                 / \   |
           *                           c   d  |
           */
          assert(X->is_black());
          auto new_Z = Z->to_single_black();
          auto new_Y = Y->dup_with_child(b, std::move(new_Z));
          auto new_X = X->dup_with_right(std::move(new_Y));
          return balance(std::move(new_X));
        }  // Y->is_double_black(Direction::Right)
      }  // Y->is_red()

      if (Y->is_double_black(Direction::Left) &&
          Y->is_black(Direction::Right)) {
        /**
         *                            {Z}   |
         *      Y                     / \   |
         *     / \       ====>      [Y]  d  |
         *   {X}  Z                 / \     |
         *   / \ / \               X   c    |
         *  a  b c  d             / \       |
         *                       a   b      |
         */
        auto new_X = X->to_single_black();
        auto new_Y =
            Y->dup_with_child_and_color(std::move(new_X), c, Color::Red);
        auto new_Z = Z->dup_with_child_and_color(std::move(new_Y), d,
                                                 Color::DoubleBlack);
        return balance(std::move(new_Z));
      }  // Y->is_double_black(Direction::Left) && Y->is_black(Direction::Right)

      if (Y->is_black(Direction::Left) &&
          Y->is_double_black(Direction::Right)) {
        /**
         *                        {X}       |
         *      Y                 / \       |
         *     / \        ====>  a  [Y]     |
         *    X  {Z}                / \     |
         *   / \ / \               b   Z    |
         *  a  b c  d                 / \   |
         *                           c   d  |
         */
        auto new_Z = Z->to_single_black();
        auto new_Y =
            Y->dup_with_child_and_color(b, std::move(new_Z), Color::Red);
        auto new_X = X->dup_with_child_and_color(a, std::move(new_Y),
                                                 Color::DoubleBlack);
        return balance(std::move(new_X));
      }  // Y->is_black(Direction::Left) && Y->is_double_black(Direction::Right)

      if (Y->is_double_black(Direction::Left) && Y->is_red(Direction::Right)) {
        /**
         *                             Z    |
         *      Y                     / \   |
         *     / \                   C   d  |
         *   {X} [Z]                / \     |
         *   / \ / \      ====>   [Y]  f    |
         *  a  b C  d             / \       |
         *      / \              X   e      |
         *     e   f            / \         |
         *                     a   b        |
         */
        auto [e, f] = children(c);
        auto new_X = X->to_single_black();
        auto new_Y =
            Y->dup_with_child_and_color(std::move(new_X), e, Color::Red);
        auto new_C =
            c->dup_with_child_and_color(std::move(new_Y), f, Color::Black);
        return Z->dup_with_child_and_color(balance(std::move(new_C)), d,
                                           Color::Black);
      }  // Y->is_double_black(Direction::Left) && Y->is_red(Direction::Right)

      if (Y->is_red(Direction::Left) && Y->is_double_black(Direction::Right)) {
        /**
         *                          X          |
         *      Y                  / \         |
         *     / \                a   B        |
         *   [X] {Z}                 / \       |
         *   / \ / \      ====>     e  [Y]     |
         *  a  B c  d                  / \     |
         *    / \                     f   Z    |
         *   e   f                       / \   |
         *                              c   d  |
         */
        auto [e, f] = children(b);
        auto new_Z = Z->to_single_black();
        auto new_Y =
            Y->dup_with_child_and_color(f, std::move(new_Z), Color::Red);
        auto new_B = balance(
            b->dup_with_child_and_color(e, std::move(new_Y), Color::Black));
        return X->dup_with_child_and_color(a, std::move(new_B), Color::Black);
      }  // Y->is_red(Direction::Left) && Y->is_double_black(Direction::Right)

      return node;
    }

    struct MinimalDeleteResult {
      uint64_t key;
      uint64_t value;
      Ptr node;
    };

    static MinimalDeleteResult minimal_delete(const Ptr &node) {
      assert(node);
      static Ptr double_nil = new_leaf(0, 0, Color::DoubleBlackNil);

      if (node->no_children()) {
        if (node->is_red()) {
          return MinimalDeleteResult(node->weight_, node->value_, nullptr);
        }
        if (node->is_black()) {
          return MinimalDeleteResult(node->weight_, node->value_, double_nil);
        }
      }
      if (node->single_child() && node->children_.right_) {
        assert(node->is_black());
        assert(node->children_.right_->is_red());
        return MinimalDeleteResult(
            node->weight_, node->value_,
            node->children_.right_->dup_with_color(Color::Red));
      }
      assert(node->children_.left_);
      auto res = minimal_delete(node->children_.left_);
      res.node = rotate(node->dup_with_left(res.node));
      return res;
    }

    static std::pair<Ptr, bool> remove_from(Ptr &node, uint64_t key) {
      if (!node) {
        return {nullptr, false};
      }

      if (key == node->weight_) {
        /**
         * red node without children
         *
         *     [N]                |
         *     / \     ===>  nil  |
         *    /   \               |
         *   nil  nil             |
         */
        if (node->is_red() && node->no_children()) {
          return {nullptr, true};
        }

        if (node->is_black() && node->single_child()) {
          /**
           * black node with single red child
           *
           *      P                    |
           *     / \                   |
           *   [C] nil                 |
           *                     C     |
           *      or    ===>    / \    |
           *                  nil nil  |
           *      P                    |
           *     / \                   |
           *   nil [C]                 |
           */
          if (node->is_red(Direction::Left)) {
            return {node->children_.left_->dup_with_color(Color::Black), true};
          }
          if (node->is_red(Direction::Right)) {
            return {node->children_.right_->dup_with_color(Color::Black), true};
          }
        }

        if (node->is_black() && node->no_children()) {
          // single black node, return a double black node
          return {new_leaf(0, 0, Color::DoubleBlackNil), true};
        }
        // fallback to recursive procedure
      }

      if (node->weight_ < key) {
        auto [new_right, removed] = remove_from(node->children_.right_, key);
        if (removed) {
          return {rotate(node->dup_with_right(new_right)), true};
        }
        return {node, false};
      }
      if (node->weight_ > key) {
        auto res = remove_from(node->children_.left_, key);
        auto &[new_left, removed] = res;
        if (removed) {
          return {rotate(node->dup_with_left(new_left)), true};
        }
        return {node, false};
      }

      // node->weight_ == key, find the minimal successor

      auto res = minimal_delete(node->children_.right_);
      auto new_node = node->dup_with_right(res.node);
      new_node->weight_ = res.key;
      new_node->value_ = res.value;
      return {rotate(new_node), true};
    }

    static bool check_invariant(const Ptr &node) {
      if (!node) {
        return true;
      }

      if (node->is_red()) {
        if (node->is_red(Direction::Left)) {
          return false;
        }
        if (node->is_red(Direction::Right)) {
          return false;
        }
      }

      auto [left, right] = children(node);

      if (!check_invariant(left)) {
        return false;
      }

      if (!check_invariant(right)) {
        return false;
      }

      int left_black_height = get_black_height(left);
      int right_black_height = get_black_height(right);

      return left_black_height == right_black_height;
    }

    static int get_black_height(const Ptr &node) {
      if (!node) {
        return 1;
      }

      auto [left, right] = children(node);

      int height = get_black_height(left);
      if (height == -1) {
        return -1;
      }

      int right_height = get_black_height(right);
      if (right_height == -1) {
        return -1;
      }

      if (height != right_height) {
        return -1;
      }

      return height + (node->color_ == Color::Black ? 1 : 0);
    }
    static void print_tree(const Ptr &node, Direction d, int indent) {
      if (!node) {
        return;
      }
      for (int i = 0; i < indent; i++) {
        std::cout << " ";
      }
      auto dstr = "X";
      if (d == Direction::Left) {
        dstr = "L";
      }
      if (d == Direction::Right) {
        dstr = "R";
      }
      std::cout << node->weight_ << "(" << dstr << " "
                << (node->color_ == Color::Black ? "B" : "R") << ")\n";
      print_tree(node->children_.left_, Direction::Left, indent + 2);
      print_tree(node->children_.right_, Direction::Right, indent + 2);
    }
  };

  std::shared_ptr<Node> root_;
  uint64_t count_{};
};
}  // namespace logloom
