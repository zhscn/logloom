#include "chunk_manager.hh"

#include <gtest/gtest.h>

using namespace logloom;

const char* file;
int line;
const char* message;

#define SET_SCOPE(msg)  \
  file = __FILE_NAME__; \
  line = __LINE__;      \
  message = #msg;

void test_views(uint64_t offset, uint64_t length, uint64_t size,
                std::vector<ChunkView> vs) {
  static constexpr int chunk_size = 10;
  ::testing::ScopedTrace trace(file, line, message);
  auto ret = cal_chunk_views(offset, length, chunk_size);
  ASSERT_EQ(ret.size(), size);
  for (auto i = 0; i < vs.size(); i++) {
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
