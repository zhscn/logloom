#pragma once

#include "noncopyable.hh"
#include "outcome.hh"

#include <boost/intrusive/list.hpp>

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace logloom {

using ChunkID = uint32_t;

struct ChunkView {
  ChunkID id_;
  uint32_t offset_;
  uint32_t length_;

  std::pair<ChunkView, ChunkView> split_at(uint32_t pivot) const;

  std::strong_ordering operator<=>(const ChunkView &) const = default;
};

std::vector<ChunkView> cal_chunk_views(uint64_t offset, uint64_t length,
                                       uint64_t chunk_size);

struct Chunk : public boost::intrusive::list_base_hook<> {
  bool empty() const {
    return data.empty();
  }

  void release() {
    std::string s;
    std::swap(data, s);
  }

  std::string data;
};

class ChunkManager : NonCopyable {
public:
  static Result<ChunkManager> open(const char *path, uint32_t chunk_size,
                                   uint64_t chunk_memory_limit);

  ChunkManager(std::FILE *file, uint64_t file_size, uint32_t chunk_size,
               uint64_t chunk_memory_limit);

  ChunkManager(ChunkManager &&) noexcept = default;

  ChunkManager &operator=(ChunkManager &&) noexcept = default;

  ~ChunkManager();

  Result<std::string_view> get_chunk(ChunkView view);

  Result<std::vector<std::string_view>> get_chunk(
      const std::vector<ChunkView> &views);

  Result<uint64_t> size() const;

private:
  Result<void> touch_chunk(ChunkID id);

  using ChunkLRU = boost::intrusive::list<Chunk>;

  std::FILE *file_;
  std::vector<Chunk> chunks_;
  ChunkLRU lru_;

  uint64_t file_size_;
  uint32_t chunk_size_;
  uint32_t chunk_count_;
  uint64_t chunk_memory_usage_;
  uint64_t chunk_memory_limit_;
};

}  // namespace logloom
