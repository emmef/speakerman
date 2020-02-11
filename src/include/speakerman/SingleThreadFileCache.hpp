/*
 * SingleThreadFileCache.hpp
 *
 * Part of 'Speaker management system'
 *
 * Copyright (C) 2013-2014 Michel Fleur.
 * https://github.com/emmef/simpledsp
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

#ifndef SMS_SPEAKERMAN_SINGLE_THREAD_FILE_CACHE_GUARD_H_
#define SMS_SPEAKERMAN_SINGLE_THREAD_FILE_CACHE_GUARD_H_

#include <speakerman/Stream.hpp>
#include <string>

namespace speakerman {

class file_entry : public input_stream {
  std::string name_;
  char *data_ = nullptr;
  size_t size_ = 0;
  size_t capacity_ = 0;
  size_t readPos_ = 0;
  long long fileStamp_;
  long long lastChecked_;

public:
  file_entry(const char *name);

  void reset();

  virtual int read();

  virtual signed long read(void *buff, size_t offs, size_t length);

  virtual void close();

  size_t size() const { return size_; }

  ~file_entry();
};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_SINGLE_THREAD_FILE_CACHE_GUARD_H_ */
