#include "chunk_manager.hh"

namespace oned {

ChunkManager::ChunkManager(ChunkLoaderPtr loader, uint32_t chunk_size,
                           uint64_t chunk_memory_limit)
    : loader_(std::move(loader)),
      chunks_(loader_->size() / chunk_size + 1),
      chunk_size_(chunk_size),
      chunk_memory_limit_(chunk_memory_limit) {}

Result<std::string_view> ChunkManager::get_chunk(ChunkView view) {
  auto &[id, off, len] = view;
  assert(id < chunks_.size());
  assert(id * chunk_size_ + off + len <= loader_->size());
  TRYV(touch_chunk(id));
  return std::string_view(chunks_[id].data).substr(off, len);
}

Result<std::vector<std::string_view>> ChunkManager::get_chunks(
    const std::vector<ChunkView> &views) {
  std::vector<std::string_view> ret;
  for (auto &v : views) {
    auto str = TRYX(get_chunk(v));
    ret.push_back(str);
  }
  return ret;
}

Result<void> ChunkManager::touch_chunk(ChunkID id) {
  auto &c = chunks_[id];

  // move to front of LRU
  if (c.is_linked()) {
    lru_.erase(lru_.iterator_to(c));
  }
  lru_.push_front(c);

  if (!c.empty()) {
    return outcome::success();
  }

  auto s = chunk_size_;
  auto offset = id * s;
  auto length = std::min(s, static_cast<uint32_t>(loader_->size() - offset));
  c.data = TRYX(loader_->read_chunk(offset, length));

  // trim LRU if necessary
  while (lru_.size() * chunk_size_ > chunk_memory_limit_) {
    auto &chunk = lru_.back();
    chunk.reset();
    lru_.pop_back();
  }

  return outcome::success();
}

}  // namespace oned
