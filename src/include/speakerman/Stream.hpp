#ifndef SPEAKERMAN_M_STREAM_HPP
#define SPEAKERMAN_M_STREAM_HPP
/*
 * speakerman/Stream.hpp
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

#include <cstddef>
#include <type_traits>

namespace speakerman {
struct stream_result {
  static constexpr int END_OF_STREAM = -1;
  static constexpr int INTERRUPTED = END_OF_STREAM - 1;
  static constexpr int INVALID_HANDLE = END_OF_STREAM - 2;
  static constexpr int RESET_BY_PEER = END_OF_STREAM - 3;
  static constexpr int INVALID_ARGUMENT = END_OF_STREAM - 4;
  static constexpr int DATA_TRUNCATED = END_OF_STREAM - 5;
};

int last_stream_result();

void set_stream_result(int result);

size_t last_operation_count();

void set_last_operation_count(size_t count);

class closeable {
public:
  virtual void close() = 0;

  virtual ~closeable() = default;
};

class input_stream : public closeable {
public:
  virtual int read() = 0;

  virtual signed long read(void *buff, size_t offs, size_t length);

  virtual signed long read_line(char *line, size_t line_size = 1024);

  virtual ~input_stream() = default;
};

signed long read_from_stream(input_stream &stream, void *buff, size_t offs,
                             size_t length);

signed long read_line_from_stream(input_stream &stream, char *line,
                                  size_t line_size);

class output_stream : public closeable {
public:
  virtual int write(char c) = 0;

  virtual signed long write(const void *buff, size_t offs, size_t length);

  virtual signed long write_string(const char *string, size_t length);

  virtual signed long write_string(const char *string);

  virtual void flush() = 0;

  virtual ~output_stream() = default;
  long write_json_string(const char *string);
};

signed long write_to_stream(output_stream &stream, const void *buff,
                            size_t offs, size_t length);

signed long write_string_to_stream(output_stream &stream, const char *string,
                                   size_t length);

template <typename T> class managable_input_stream : public input_stream {
public:
  virtual void set_config(const T &config) = 0;
};



} // namespace speakerman

#endif // SPEAKERMAN_M_STREAM_HPP
