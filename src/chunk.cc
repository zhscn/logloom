#include "chunk.hh"
#include "noncopyable.hh"

#include <cassert>
#include <cerrno>

namespace oned {

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

std::vector<ChunkView> calculate_chunk_views(uint64_t offset, uint64_t length,
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

class FileChunkLoader final : public ChunkLoader, NonCopyable {
public:
  FileChunkLoader(std::FILE* file, uint64_t file_size)
      : file_(file), file_size_(file_size) {}

  FileChunkLoader(FileChunkLoader&&) noexcept = default;

  FileChunkLoader& operator=(FileChunkLoader&&) noexcept = default;

  ~FileChunkLoader() final {
    if (file_ != nullptr) {
      fclose(file_);  // NOLINT
    }
  }

  uint64_t size() const final {
    return file_size_;
  }

  Result<std::string> read_chunk(uint64_t offset, uint32_t length) final {
    if (auto ret = fseek(file_, static_cast<long>(offset), SEEK_SET);
        ret == -1L) {
      return errno_to_errc(errno);
    }

    std::string data;
    data.resize(length);
    auto n = fread(data.data(), sizeof(char), length, file_);
    if (n != length) {
      if (feof(file_) != 0) {
        return make_error(GenericErrc::io_error,
                          fmt::format("unexpected EOF when reading at {}~{}",
                                      offset, length));
      }
      return GenericErrc::io_error;
    }

    return data;
  }

private:
  std::FILE* file_;
  uint64_t file_size_;
};

Result<ChunkLoaderPtr> ChunkLoader::open(const char* path) {
  auto file = fopen(path, "rb");  // NOLINT
  if (file == nullptr) {
    return errno_to_errc(errno);
  }

  auto get_size = [](std::FILE* f) -> Result<uint64_t> {
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

    return file_size;
  };

  auto file_size = get_size(file);
  if (!file_size) {
    fclose(file);
    return std::move(file_size).error();
  }

  return std::make_unique<FileChunkLoader>(file, file_size.value());
}

}  // namespace oned
