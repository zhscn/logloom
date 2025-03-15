#pragma once

#include <cassert>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class PieceTableTest;

namespace logloom {

class PieceTable {
public:
  PieceTable() = default;
  explicit PieceTable(std::string text)
      : original_(std::move(text)),
        pieces_({{
            .offset_ = 0,
            .length_ = original_.size(),
            .type_ = Piece::Type::Original,
        }}) {}

  PieceTable(const PieceTable &other) = delete;
  PieceTable(PieceTable &&other) noexcept = default;
  PieceTable &operator=(const PieceTable &other) = delete;
  PieceTable &operator=(PieceTable &&other) noexcept = default;
  ~PieceTable() = default;

  void insert(uint64_t offset, std::string_view str) {
    auto piece = Piece{
        .offset_ = added_.size(),
        .length_ = str.length(),
        .type_ = Piece::Type::Added,
    };
    added_.append(str);

    auto iter = maybe_split_at(offset);
    pieces_.insert(iter, piece);
  }

  void remove(uint64_t offset, uint64_t length) {
    maybe_split_at(offset);
    auto iter = maybe_split_at(offset + length);
    auto res = find_piece(offset);
    assert(res.first == offset);
    pieces_.erase(res.second, iter);
  }

  std::string dump() const {
    std::string ret;
    for (auto &piece : pieces_) {
      if (piece.type_ == Piece::Type::Original) {
        ret.append(original_.substr(piece.offset_, piece.length_));
      } else {
        ret.append(added_.substr(piece.offset_, piece.length_));
      }
    }
    return ret;
  }

private:
  struct Piece {
    enum class Type : uint8_t { Original, Added };

    uint64_t offset_{};
    uint64_t length_{};
    Type type_ = Type::Original;

    std::pair<Piece, Piece> split(uint64_t pivot) {
      assert(pivot > 0);
      assert(pivot < length_);
      auto left = Piece{
          .offset_ = offset_,
          .length_ = pivot,
          .type_ = type_,
      };
      auto right = Piece{
          .offset_ = offset_ + pivot,
          .length_ = length_ - pivot,
          .type_ = type_,
      };
      return std::make_pair(left, right);
    };

    std::strong_ordering operator<=>(const Piece &other) const = default;
  };

  auto maybe_split_at(uint64_t offset) -> std::vector<Piece>::iterator {
    auto [piece_start, iter] = find_piece(offset);
    if (iter == pieces_.end() || piece_start == offset) {
      return iter;
    }

    assert(piece_start < offset);
    assert(piece_start + iter->length_ > offset);
    // |<---------original piece------------>|
    //                 ^
    //                 |
    //                 +---- insert offset
    //                 |
    //                 v
    // |<-----left---->|<--------right------>|
    auto [left_piece, right_piece] = iter->split(offset - piece_start);
    assert(offset == piece_start + left_piece.length_);
    // remap original piece to right, then insert left before right
    *iter = right_piece;
    iter = pieces_.insert(iter, left_piece);
    // return the right piece
    return ++iter;
  }

  auto find_piece(uint64_t offset)
      -> std::pair<uint64_t, std::vector<Piece>::iterator> {
    auto piece_start = 0UL;
    for (auto iter = pieces_.begin(); iter != pieces_.end(); iter++) {
      if (piece_start + iter->length_ > offset) {
        return std::make_pair(piece_start, iter);
      }
      piece_start += iter->length_;
    }
    return std::make_pair(piece_start, pieces_.end());
  }

  std::string original_;
  std::string added_;
  std::vector<Piece> pieces_;

  friend class ::PieceTableTest;
};
}  // namespace logloom
