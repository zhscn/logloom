#pragma once

#include "chunk.hh"
#include "noncopyable.hh"

class ChunkManagerTest;

namespace oned {

class ChunkManager : NonCopyable {
public:
  ChunkManager(ChunkLoaderPtr loader, uint32_t chunk_size,
               uint64_t chunk_memory_limit);

  Result<std::string_view> get_chunk(ChunkView view);

  Result<std::vector<std::string_view>> get_chunks(
      const std::vector<ChunkView> &views);

  std::size_t chunk_count() const {
    return lru_.size();
  }

private:
  Result<void> touch_chunk(ChunkID id);

  using ChunkLRU = boost::intrusive::list<Chunk>;

  ChunkLoaderPtr loader_;
  std::vector<Chunk> chunks_;
  ChunkLRU lru_;

  uint32_t chunk_size_;
  uint64_t chunk_memory_limit_;

  friend class ::ChunkManagerTest;
};

}  // namespace oned
