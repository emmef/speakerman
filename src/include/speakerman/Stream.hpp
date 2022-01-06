/*
 * Stream.hpp
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

#ifndef SMS_SPEAKERMAN_STREAM_GUARD_H_
#define SMS_SPEAKERMAN_STREAM_GUARD_H_

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
};

signed long write_to_stream(output_stream &stream, const void *buff,
                            size_t offs, size_t length);

signed long write_string_to_stream(output_stream &stream, const char *string,
                                   size_t length);

template <typename T> class managable_input_stream : public input_stream {
public:
  virtual void set_config(const T &config) = 0;
};

template <typename T, class Y>
class managed_input_stream : public input_stream {
  using Stream = managable_input_stream<T>;
  static_assert(std::is_base_of<Stream, Y>::value,
                "Type parameter must be subclass of manageable_input_stream of "
                "same type");
  Stream stream_;

  void cleanup() { stream_.close(); }

public:
  virtual int read() { return stream_.read(); }

  virtual signed long read(void *buff, size_t offs, size_t length) {
    return stream_.read(buff, offs, length);
  }

  virtual signed long read_line(char *line, size_t line_size) {
    return stream_.read_line(line, line_size);
  }

  void set_config(const T &config) {
    cleanup();
    stream_.set_config(config);
  }

  virtual ~managed_input_stream() { cleanup(); }
};

template <typename T> class managable_output_stream : public output_stream {
public:
  virtual void set_config(const T &config) = 0;
};

class file_owner {
  int file_descriptor_;
  bool owns_file_;

protected:
  int file_descriptor() const { return file_descriptor_; }

  virtual void close_file() = 0;

  virtual void before_close_file() = 0;

  virtual void on_file_set() = 0;

  void cleanup_file();

public:
  file_owner();

  void set_file(int file_descriptor, bool owns_file);

  virtual ~file_owner();
};

class buffer_stream : public output_stream, public input_stream {
  size_t capacity_;
  char *data_;
  size_t read_;
  size_t write_;

protected:
  char *data() const;

public:
  buffer_stream(size_t capacity);

  virtual int read() override final;

  virtual int write(char c) override final;

  virtual void flush() override final;

  virtual void close() override final;

  virtual ~buffer_stream();

  size_t readable_size() const;

  size_t maximum_size() const;

  size_t allocated_size() const;
};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_STREAM_GUARD_H_ */
