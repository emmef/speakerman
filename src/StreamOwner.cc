//
// Created by michel on 09-01-22.
//

#include <speakerman/StreamOwner.h>
#include <fstream>

namespace speakerman {
StreamOwner::StreamOwner(std::ifstream &owned) : stream_(owned), owns_(true) {}

StreamOwner::StreamOwner(const StreamOwner &source)
    : stream_(source.stream_), owns_(false) {}

StreamOwner::StreamOwner(StreamOwner &&source) noexcept
    : stream_(source.stream_), owns_(true) {
  source.owns_ = false;
}
StreamOwner StreamOwner::open(const char *file_name) {
  std::ifstream stream;
  stream.open(file_name);
  return StreamOwner(stream);
}
StreamOwner::~StreamOwner() {
  if (owns_ && stream_.is_open()) {
    stream_.close();
  }
}
bool StreamOwner::is_open() const { return stream_.is_open(); }

} // namespace speakerman
