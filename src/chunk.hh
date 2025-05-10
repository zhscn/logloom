#pragma once

#include "outcome.hh"

#include <boost/intrusive/list.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace oned {

using ChunkID = uint32_t;

struct ChunkView {
  ChunkID id_;
  uint32_t offset_;
  uint32_t length_;

  std::pair<ChunkView, ChunkView> split_at(uint32_t pivot) const;

  std::strong_ordering operator<=>(const ChunkView&) const = default;
};

std::vector<ChunkView> calculate_chunk_views(uint64_t offset, uint64_t length,
                                             uint64_t chunk_size);

struct Chunk : public boost::intrusive::list_base_hook<> {
  bool empty() const {
    return data.empty();
  }

  void reset() {
    std::string s;
    std::swap(data, s);
  }

  std::string data;
};

struct ChunkLoader {  // NOLINT
  using Ptr = std::unique_ptr<ChunkLoader>;

  virtual ~ChunkLoader() = default;
  virtual uint64_t size() const = 0;
  virtual Result<std::string> read_chunk(uint64_t offset, uint32_t length) = 0;

  static Result<Ptr> open(const char* path);
};
using ChunkLoaderPtr = std::unique_ptr<ChunkLoader>;

}  // namespace oned
