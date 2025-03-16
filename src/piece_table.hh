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
  static constexpr uint64_t kDefaultChunkSize = 64;

  explicit PieceTable(uint64_t chunk_size = kDefaultChunkSize)
      : chunk_size_(chunk_size) {}
  explicit PieceTable(std::string_view str,
                      uint64_t chunk_size = kDefaultChunkSize)
      : chunk_size_(chunk_size), pieces_(append_string(str)) {}

  PieceTable(const PieceTable &other) = delete;
  PieceTable(PieceTable &&other) noexcept = default;
  PieceTable &operator=(const PieceTable &other) = delete;
  PieceTable &operator=(PieceTable &&other) noexcept = default;
  ~PieceTable() = default;

  void insert(uint64_t offset, std::string_view str) {
    auto iter = maybe_split_at(offset);
    auto ps = append_string(str);
    pieces_.insert(iter, ps.begin(), ps.end());
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
      auto &chunk = chunks_[piece.chunk_idx_];
      assert(chunk.size() <= chunk_size_);
      ret.append(chunk.substr(piece.offset_, piece.length_));
    }
    return ret;
  }

private:
  struct Piece {
    uint64_t offset_{};
    uint64_t length_{};
    uint64_t chunk_idx_{};

    std::pair<Piece, Piece> split(uint64_t pivot) {
      assert(pivot > 0);
      assert(pivot < length_);
      auto left = Piece{
          .offset_ = offset_,
          .length_ = pivot,
          .chunk_idx_ = chunk_idx_,
      };
      auto right = Piece{
          .offset_ = offset_ + pivot,
          .length_ = length_ - pivot,
          .chunk_idx_ = chunk_idx_,
      };
      return std::make_pair(left, right);
    };

    std::strong_ordering operator<=>(const Piece &other) const = default;
  };

  std::vector<Piece> append_string(std::string_view str) {
    if (str.empty()) {
      return {};
    }
    auto start = total_size_ / chunk_size_;
    auto end = (total_size_ + str.size() + chunk_size_ - 1) / chunk_size_;
    assert(start < end);
    chunks_.resize(end);
    uint64_t str_cursor = 0;
    std::vector<Piece> ps;
    for (auto i = start; i < end; i++) {
      auto &chunk = chunks_[i];
      if (chunk.capacity() != chunk_size_) {
        chunk.reserve(chunk_size_);
      }
      auto rest_str_len = str.size() - str_cursor;
      auto rest_chunk_len = chunk_size_ - chunk.size();
      auto str_len = std::min(rest_str_len, rest_chunk_len);
      ps.emplace_back(chunk.size(), str_len, i);
      chunk.append(str.substr(str_cursor, str_len));
      str_cursor += str_len;
    }
    total_size_ += str.size();
    return ps;
  }

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

  uint64_t chunk_size_;
  uint64_t total_size_{};
  std::vector<std::string> chunks_;
  std::vector<Piece> pieces_;

  friend class ::PieceTableTest;
};
}  // namespace logloom
