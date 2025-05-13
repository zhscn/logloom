#include "chunk_manager.hh"

#include <gtest/gtest.h>

using namespace oned;

class TestChunkLoader final : public ChunkLoader, NonCopyable {
public:
  explicit TestChunkLoader(std::string data) : data_(std::move(data)) {}

  uint64_t size() const final {
    return data_.size();
  }

  Result<std::string> read_chunk(uint64_t offset, uint32_t length) final {
    if (offset > data_.size()) {
      return GenericErrc::invalid_argument;
    }
    return data_.substr(offset, length);
  }

private:
  std::string data_;
};

// NOLINTBEGIN
const char* file;
int line;
const char* message;
// NOLINTEND

#define SET_SCOPE(msg)  \
  file = __FILE_NAME__; \
  line = __LINE__;      \
  message = #msg;

void test_views(uint64_t offset, uint64_t length, uint64_t size,
                std::vector<ChunkView> vs) {
  static constexpr int chunk_size = 10;
  ::testing::ScopedTrace trace(file, line, message);
  auto ret = calculate_chunk_views(offset, length, chunk_size);
  ASSERT_EQ(ret.size(), size);
  for (size_t i = 0; i < vs.size(); i++) {
    ASSERT_EQ(vs[i], ret[i]);
  }
}

TEST(get_chunk_views, aligned) {
  SET_SCOPE(1);
  test_views(0, 10, 1, {{0, 0, 10}});

  SET_SCOPE(2);
  test_views(0, 20, 2, {{0, 0, 10}, {1, 0, 10}});

  SET_SCOPE(3);
  test_views(10, 10, 1, {{1, 0, 10}});

  SET_SCOPE(4);
  test_views(10, 20, 2, {{1, 0, 10}, {2, 0, 10}});
}

TEST(get_chunk_views, not_alinged_end) {
  SET_SCOPE(1);
  test_views(0, 7, 1, {{0, 0, 7}});

  SET_SCOPE(2);
  test_views(0, 18, 2, {{0, 0, 10}, {1, 0, 8}});

  SET_SCOPE(3);
  test_views(10, 7, 1, {{1, 0, 7}});

  SET_SCOPE(4);
  test_views(10, 18, 2, {{1, 0, 10}, {2, 0, 8}});
}

TEST(get_chunk_views, not_aligned_offset) {
  SET_SCOPE(1);
  test_views(2, 8, 1, {{0, 2, 8}});

  SET_SCOPE(2);
  test_views(2, 18, 2, {{0, 2, 8}, {1, 0, 10}});

  SET_SCOPE(3);
  test_views(12, 8, 1, {{1, 2, 8}});

  SET_SCOPE(4);
  test_views(22, 18, 2, {{2, 2, 8}, {3, 0, 10}});
}

TEST(get_chunk_views, non_aligned_offset_end) {
  SET_SCOPE(1);
  test_views(2, 5, 1, {{0, 2, 5}});

  SET_SCOPE(2);
  test_views(2, 12, 2, {{0, 2, 8}, {1, 0, 4}});

  SET_SCOPE(3);
  test_views(12, 35, 4, {{1, 2, 8}, {2, 0, 10}, {3, 0, 10}, {4, 0, 7}});
}

class ChunkManagerTest : public ::testing::Test {
protected:
  void SetUp() override {
    std::string test_data;
    for (int i = 0; i < 5; i++) {
      test_data += std::string(10, 'A' + i);  // NOLINT
    }
    test_data += std::string(5, 'F');
    auto chunk_file = std::make_unique<TestChunkLoader>(std::move(test_data));
    mgr_ = std::make_unique<ChunkManager>(std::move(chunk_file), chunk_size,
                                          memory_limit);
  }

  void test_lru() {
    ASSERT_EQ(mgr_->chunk_count(), 0);

    // touch A lru: A
    auto chunk1 = mgr_->get_chunk(ChunkView{0, 0, 10});
    ASSERT_TRUE(chunk1);
    ASSERT_EQ(chunk1.value(), std::string(10, 'A'));
    ASSERT_EQ(mgr_->chunk_count(), 1);

    // touch B lru: B A
    auto chunk2 = mgr_->get_chunk(ChunkView{1, 0, 10});
    ASSERT_TRUE(chunk2);
    ASSERT_EQ(chunk2.value(), std::string(10, 'B'));
    ASSERT_EQ(mgr_->chunk_count(), 2);

    // touch C lru: C B A
    auto chunk3 = mgr_->get_chunk(ChunkView{2, 0, 10});
    ASSERT_TRUE(chunk3);
    ASSERT_EQ(chunk3.value(), std::string(10, 'C'));
    ASSERT_EQ(mgr_->chunk_count(), 3);

    {
      // touch A lru: A C B
      auto chunk = mgr_->get_chunk(ChunkView{0, 0, 10});
      ASSERT_TRUE(chunk);
      ASSERT_EQ(chunk.value(), std::string(10, 'A'));
      ASSERT_EQ(mgr_->chunk_count(), 3);

      // lru: A C B
      auto p = mgr_->lru_.begin();
      ASSERT_EQ(p->data, std::string(10, 'A'));
      ++p;
      ASSERT_EQ(p->data, std::string(10, 'C'));
      ++p;
      ASSERT_EQ(p->data, std::string(10, 'B'));
    }

    // touch D evict B lru: D A C
    auto chunk4 = mgr_->get_chunk(ChunkView{3, 0, 10});
    ASSERT_TRUE(chunk4);
    ASSERT_EQ(chunk4.value(), std::string(10, 'D'));
    ASSERT_EQ(mgr_->chunk_count(), 3);
    for (auto& c : mgr_->lru_) {
      ASSERT_NE(c.data, std::string(10, 'B'));
    }

    // touch A lru: A D C
    chunk1 = mgr_->get_chunk(ChunkView{0, 0, 10});
    ASSERT_TRUE(chunk1);
    ASSERT_EQ(chunk1.value(), std::string(10, 'A'));
    ASSERT_EQ(mgr_->chunk_count(), 3);
    {
      // lru: A D C
      auto p = mgr_->lru_.begin();
      ASSERT_EQ(p->data, std::string(10, 'A'));
      ++p;
      ASSERT_EQ(p->data, std::string(10, 'D'));
      ++p;
      ASSERT_EQ(p->data, std::string(10, 'C'));
    }

    // touch E evict C lru: E A D
    auto chunk5 = mgr_->get_chunk(ChunkView{4, 0, 10});
    ASSERT_TRUE(chunk5);
    ASSERT_EQ(chunk5.value(), std::string(10, 'E'));
    ASSERT_EQ(mgr_->chunk_count(), 3);
    for (auto& c : mgr_->lru_) {
      ASSERT_NE(c.data, std::string(10, 'C'));
    }

    // touch B evict D lru: B E A
    chunk2 = mgr_->get_chunk(ChunkView{1, 0, 10});
    ASSERT_TRUE(chunk2);
    ASSERT_EQ(chunk2.value(), std::string(10, 'B'));
    ASSERT_EQ(mgr_->chunk_count(), 3);
    for (auto& c : mgr_->lru_) {
      ASSERT_NE(c.data, std::string(10, 'D'));
    }

    // touch F evict E lru: F B A
    auto chunk6 = mgr_->get_chunk(ChunkView{5, 0, 5});
    ASSERT_TRUE(chunk6);
    ASSERT_EQ(chunk6.value(), std::string(5, 'F'));
  }

  std::unique_ptr<ChunkManager> mgr_;
  static constexpr uint32_t chunk_size = 10;
  static constexpr uint64_t memory_limit = 30;
};

TEST_F(ChunkManagerTest, memory_limit_and_lru) {
  test_lru();
}
