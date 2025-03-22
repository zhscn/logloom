#include "chunk_manager.hh"

namespace logloom {

std::pair<ChunkView, ChunkView> ChunkView::split_at(uint32_t pivot) const {
  assert(pivot != 0);
  assert(pivot != length_);
  return std::make_pair(
      ChunkView{
          .id_ = id_,
          .offset_ = offset_,
          .length_ = pivot,
      },
      ChunkView{
          .id_ = id_,
          .offset_ = offset_ + pivot,
          .length_ = length_ - pivot,
      });
}

std::vector<ChunkView> cal_chunk_views(uint64_t offset, uint64_t length,
                                       uint64_t chunk_size) {
  assert(chunk_size < std::numeric_limits<uint32_t>::max() / 2);
  std::vector<ChunkView> ret;

  auto start_id = offset / chunk_size;
  auto end_id = (offset + length + chunk_size - 1) / chunk_size;
  assert(end_id < std::numeric_limits<ChunkID>::max());
  for (ChunkID id = start_id; id < end_id; id++) {
    uint32_t chunk_off = offset % chunk_size;
    uint32_t chunk_len = std::min(chunk_size - chunk_off, length);
    offset += chunk_len;
    length -= chunk_len;
    ret.push_back(ChunkView{
        .id_ = id,
        .offset_ = chunk_off,
        .length_ = chunk_len,
    });
  }

  return ret;
}

ChunkManager::ChunkManager(std::FILE *file, uint64_t file_size,
                           uint32_t chunk_size, uint64_t chunk_memory_limit)
    : file_(file),
      chunks_(file_size / chunk_size + 1),
      file_size_(file_size),
      chunk_size_(chunk_size),
      chunk_count_(0),
      chunk_memory_usage_(0),
      chunk_memory_limit_(chunk_memory_limit) {}

ChunkManager::~ChunkManager() {
  if (file_ != nullptr) {
    fclose(file_);  // NOLINT
  }
}

Result<ChunkManager> ChunkManager::open(const char *path, uint32_t chunk_size,
                                        uint64_t chunk_memory_limit) {
  auto file = fopen(path, "rb");  // NOLINT
  if (file == nullptr) {
    return errno_to_errc(errno);
  }

  auto get_size = [](std::FILE *f) -> Result<uint64_t> {
    auto cur_pos = ftell(f);
    if (cur_pos == -1L) {
      return errno_to_errc(errno);
    }

    auto ret = fseek(f, 0, SEEK_END);
    if (ret != 0) {
      return errno_to_errc(errno);
    }

    auto file_size = ftell(f);
    if (file_size == -1L) {
      return errno_to_errc(errno);
    }

    ret = fseek(f, cur_pos, SEEK_SET);
    if (ret != 0) {
      return errno_to_errc(errno);
    }

    return file_size;
  };

  auto file_size_ = get_size(file);
  if (!file_size_) {
    fclose(file);
    return std::move(file_size_).error();
  }
  auto file_size = file_size_.value();

  return ChunkManager(file, file_size, chunk_size, chunk_memory_limit);
}

Result<std::string_view> ChunkManager::get_chunk(ChunkView view) {
  assert(view.offset_ + view.length_ <= file_size_);
  auto &[id, off, len] = view;
  assert(id < chunks_.size());
  auto &c = chunks_[id];
  if (c.empty()) {
    TRYV(touch_chunk(id));
  }
  return std::string_view(c.data).substr(view.offset_, view.length_);
}

Result<std::vector<std::string_view>> ChunkManager::get_chunk(
    const std::vector<ChunkView> &views) {
  std::vector<std::string_view> ret;
  for (auto &v : views) {
    auto str = TRYX(get_chunk(v));
    ret.push_back(str);
  }
  return ret;
}

Result<uint64_t> ChunkManager::size() const {
  return file_size_;
}

Result<void> ChunkManager::touch_chunk(ChunkID id) {
  if (!chunks_[id].empty()) {
    return outcome::success();
  }

  auto s = chunk_size_;
  auto offset = id * s;
  auto length = std::min(s, static_cast<uint32_t>(file_size_ - offset));

  if (auto ret = fseek(file_, offset, SEEK_SET); ret == -1L) {
    return errno_to_errc(errno);
  }

  auto &chunk = chunks_[id];
  chunk.data.resize(length);
  auto n = fread(chunk.data.data(), sizeof(char), length, file_);
  if (n != length) {
    chunk.release();
    if (feof(file_) != 0) {
      return make_error(
          GenericErrc::io_error,
          fmt::format("unexpected EOF when reading chunk {}", id));
    }
    return GenericErrc::io_error;
  }

  lru_.erase(lru_.iterator_to(chunk));
  lru_.push_front(chunk);
  chunk_count_++;
  chunk_memory_usage_ += length;

  if (chunk_memory_usage_ > chunk_memory_limit_) {
    auto &chunk = lru_.back();
    chunk.release();
    chunk_memory_usage_ -= chunk.data.size();
    chunk_count_--;
    lru_.pop_back();
  }

  return outcome::success();
}

}  // namespace logloom
