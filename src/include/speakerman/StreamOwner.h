#ifndef SPEAKERMAN_M_STREAM_OWNER_H
#define SPEAKERMAN_M_STREAM_OWNER_H
/*
 * speakerman/StreamOwner.h
 *
 * Added by michel on 2022-01-09
 * Copyright (C) 2015-2022 Michel Fleur.
 * Source https://github.com/emmef/speakerman
 * Email speakerman@emmef.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <istream>

namespace speakerman {

class StreamOwner {
  std::ifstream &stream_;
  bool owns_;

  void operator=(const StreamOwner &) {}

  void operator=(StreamOwner &&) noexcept {}

public:
  explicit StreamOwner(std::ifstream &owned);
  StreamOwner(const StreamOwner &source);
  StreamOwner(StreamOwner &&source) noexcept;
  static StreamOwner open(const char *file_name);
  bool is_open() const;
  std::ifstream &stream() const { return stream_; };
  ~StreamOwner();
};

} // namespace speakerman

#endif // SPEAKERMAN_M_STREAM_OWNER_H
