#include "piece_table.hh"

#include <gtest/gtest.h>
#include <random>

using logloom::PieceTable;

class PieceTableTest : public ::testing::Test {
protected:
  void SetUp() override {
    table = std::make_unique<logloom::PieceTable>(8);
    // Initialize chunks with "00001111" and "2222"
    table->chunks_.resize(2);
    table->chunks_[0] = "00001111";
    table->chunks_[1] = "2222";
    table->total_size_ = 12;  // Total size of all chunks

    auto &pieces = table->pieces_;
    piece0 = {
        .offset_ = 0,
        .length_ = 4,
        .chunk_idx_ = 0,
    };
    piece1 = {
        .offset_ = 4,
        .length_ = 4,
        .chunk_idx_ = 0,
    };
    piece2 = {
        .offset_ = 0,
        .length_ = 4,
        .chunk_idx_ = 1,
    };
    pieces.push_back(piece0);
    pieces.push_back(piece1);
    pieces.push_back(piece2);
  }

  std::unique_ptr<logloom::PieceTable> table;
  logloom::PieceTable::Piece piece0;
  logloom::PieceTable::Piece piece1;
  logloom::PieceTable::Piece piece2;

  void test_find_piece_aligned() {
    {
      SCOPED_TRACE("offset 0");
      auto [piece_start, iter] = table->find_piece(0);
      EXPECT_EQ(piece_start, 0);
      EXPECT_EQ(*iter, piece0);
    }
    {
      SCOPED_TRACE("offset 4");
      auto [piece_start, iter] = table->find_piece(4);
      EXPECT_EQ(piece_start, 4);
      EXPECT_EQ(*iter, piece1);
    }
    {
      SCOPED_TRACE("offset 8");
      auto [piece_start, iter] = table->find_piece(8);
      EXPECT_EQ(piece_start, 8);
      EXPECT_EQ(*iter, piece2);
    }
    {
      SCOPED_TRACE("offset 12");
      auto [piece_start, iter] = table->find_piece(12);
      EXPECT_EQ(piece_start, 12);
      EXPECT_EQ(iter, table->pieces_.end());
    }
    {
      SCOPED_TRACE("offset 16");
      auto [piece_start, iter] = table->find_piece(16);
      EXPECT_EQ(piece_start, 12);
      EXPECT_EQ(iter, table->pieces_.end());
    }
  }

  void test_find_piece_unaligned() {
    {
      SCOPED_TRACE("offset 1");
      auto [piece_start, iter] = table->find_piece(1);
      EXPECT_EQ(piece_start, 0);
      EXPECT_EQ(*iter, piece0);
    }
    {
      SCOPED_TRACE("offset 5");
      auto [piece_start, iter] = table->find_piece(5);
      EXPECT_EQ(piece_start, 4);
      EXPECT_EQ(*iter, piece1);
    }
    {
      SCOPED_TRACE("offset 9");
      auto [piece_start, iter] = table->find_piece(9);
      EXPECT_EQ(piece_start, 8);
      EXPECT_EQ(*iter, piece2);
    }
    {
      SCOPED_TRACE("offset 13");
      auto [piece_start, iter] = table->find_piece(13);
      EXPECT_EQ(piece_start, 12);
      EXPECT_EQ(iter, table->pieces_.end());
    }
  }

  void test_piece_split() {
    auto [left, right] = piece0.split(2);
    EXPECT_EQ(left.offset_, 0);
    EXPECT_EQ(left.length_, 2);
    EXPECT_EQ(left.chunk_idx_, 0);
    EXPECT_EQ(right.offset_, 2);
    EXPECT_EQ(right.length_, 2);
    EXPECT_EQ(right.chunk_idx_, 0);
  }

  void test_maybe_split_at() {
    auto iter = table->maybe_split_at(2);
    EXPECT_EQ(iter->offset_, 2);
    EXPECT_EQ(iter->length_, 2);
    EXPECT_EQ(iter->chunk_idx_, 0);
    EXPECT_EQ(table->pieces_.size(), 4);
    EXPECT_EQ(table->pieces_[0].offset_, 0);
    EXPECT_EQ(table->pieces_[0].length_, 2);
    EXPECT_EQ(table->pieces_[0].chunk_idx_, 0);
    EXPECT_EQ(table->pieces_[1], *iter);
  }

  void test_insert() {
    table->insert(4, "xxxx");
    EXPECT_EQ(table->chunks_[1], "2222xxxx");
    EXPECT_EQ(table->pieces_.size(), 4);
    EXPECT_EQ(table->pieces_[1].offset_, 4);
    EXPECT_EQ(table->pieces_[1].length_, 4);
    EXPECT_EQ(table->pieces_[1].chunk_idx_, 1);  // New chunk index
    EXPECT_EQ(table->dump(), "0000xxxx11112222");

    table->insert(16, "yyyy");
    EXPECT_EQ(table->chunks_.size(), 3);
    EXPECT_EQ(table->chunks_[2], "yyyy");
    EXPECT_EQ(table->pieces_.size(), 5);
    EXPECT_EQ(table->pieces_[4].offset_, 0);
    EXPECT_EQ(table->pieces_[4].length_, 4);
    EXPECT_EQ(table->pieces_[4].chunk_idx_, 2);  // New chunk index
    EXPECT_EQ(table->dump(), "0000xxxx11112222yyyy");

    table->insert(18, "zzzz");
    EXPECT_EQ(table->pieces_.size(), 7);
    EXPECT_EQ(table->dump(), "0000xxxx11112222yyzzzzyy");
  }

  void test_remove() {
    EXPECT_EQ(table->dump(), "000011112222");
    EXPECT_EQ(table->pieces_.size(), 3);
    table->remove(0, 4);
    EXPECT_EQ(table->pieces_.size(), 2);
    EXPECT_EQ(table->pieces_[0].offset_, 4);
    EXPECT_EQ(table->pieces_[0].length_, 4);
    EXPECT_EQ(table->pieces_[0].chunk_idx_, 0);
    EXPECT_EQ(table->pieces_[1].offset_, 0);
    EXPECT_EQ(table->pieces_[1].length_, 4);
    EXPECT_EQ(table->pieces_[1].chunk_idx_, 1);
    EXPECT_EQ(table->dump(), "11112222");

    table->remove(1, 2);
    EXPECT_EQ(table->pieces_.size(), 3);
    EXPECT_EQ(table->pieces_[0].offset_, 4);
    EXPECT_EQ(table->pieces_[0].length_, 1);
    EXPECT_EQ(table->pieces_[0].chunk_idx_, 0);
    EXPECT_EQ(table->pieces_[1].offset_, 7);
    EXPECT_EQ(table->pieces_[1].length_, 1);
    EXPECT_EQ(table->pieces_[1].chunk_idx_, 0);
    EXPECT_EQ(table->pieces_[2].offset_, 0);
    EXPECT_EQ(table->pieces_[2].length_, 4);
    EXPECT_EQ(table->pieces_[2].chunk_idx_, 1);
    EXPECT_EQ(table->dump(), "112222");

    table->remove(0, 6);
    EXPECT_EQ(table->pieces_.size(), 0);
  }

  static std::string random_string(size_t length) {
    static constexpr std::string_view charset =
        "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<> dist(0, charset.size() - 1);
    std::string str(length, '\0');
    for (size_t i = 0; i < length; ++i) {
      str[i] = charset[dist(rng)];
    }
    return str;
  }

  static void test_fuzzy() {
    auto str = std::string();
    auto t = PieceTable();
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> op_dist(0, 1);
    std::uniform_int_distribution<uint64_t> str_len_dist(0, 1000);

    for (int i = 0; i < 10000; ++i) {
      auto op = op_dist(rng);
      std::uniform_int_distribution<uint64_t> offset_dist(0ULL, str.size());
      auto offset = offset_dist(rng);
      auto length = str_len_dist(rng);
      if (op == 0) {
        // insert
        auto s = random_string(length);
        t.insert(offset, s);
        str.insert(offset, s);
      } else {
        // remove
        length = std::min<uint64_t>(length, str.size() - offset);
        t.remove(offset, length);
        str.erase(offset, length);
      }
      EXPECT_EQ(t.dump(), str);
    }
  }
};

TEST_F(PieceTableTest, FindPieceAligned) {
  test_find_piece_aligned();
}

TEST_F(PieceTableTest, FindPieceUnaligned) {
  test_find_piece_unaligned();
}

TEST_F(PieceTableTest, PieceSplit) {
  test_piece_split();
}

TEST_F(PieceTableTest, MaybeSplitAt) {
  test_maybe_split_at();
}

TEST_F(PieceTableTest, Insert) {
  test_insert();
}

TEST_F(PieceTableTest, Remove) {
  test_remove();
}

TEST_F(PieceTableTest, Fuzzy) {
  test_fuzzy();
}
