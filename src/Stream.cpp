/*
 * Stream.cpp
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

#include <speakerman/Stream.hpp>
#include <stdexcept>

namespace speakerman {
using namespace std;
static thread_local int last_result;
static thread_local size_t last_count;

int last_stream_result() { return last_result; }

void set_stream_result(int result) { last_result = result; }

size_t last_operation_count() { return last_count; }

void set_last_operation_count(size_t count) { last_count = count; }

signed long read_from_stream(input_stream &stream, void *data, size_t offs,
                             size_t length) {
  char *buff = static_cast<char *>(data);
  size_t end = offs + length;
  size_t i;
  int r;
  for (i = offs; i < end; i++) {
    r = stream.read();
    if (r < 0) {
      break;
    }
    buff[i] = r;
  }
  size_t l = i - offs;
  set_last_operation_count(l);
  if (r >= 0 || r == stream_result::END_OF_STREAM) {
    return l;
  }
  set_stream_result(r);
  return r;
}

signed long read_line_from_stream(input_stream &stream, char *line,
                                  size_t buff_size) {
  if (buff_size < 1) {
    return 0;
  }
  size_t end = buff_size - 1;
  size_t i;
  int r;
  for (i = 0; i < end; i++) {
    r = stream.read();
    if (r < 0) {
      break;
    }
    if (r == '\r' || r == '\n') {
      break;
    }
    line[i] = r;
  }
  line[i] = 0;
  set_last_operation_count(i);
  if (r == '\r' || r == '\n') {
    return i;
  }
  if (r < 0) {
    return r;
  }
  return stream_result::DATA_TRUNCATED;
}

signed long write_to_stream(output_stream &stream, const void *data,
                            size_t offs, size_t length) {
  const char *buff = static_cast<const char *>(data);
  size_t end = offs + length;
  size_t i;
  int r;
  for (i = offs; i < end; i++) {
    r = stream.write(buff[i]);
    if (r < 0) {
      break;
    }
  }
  size_t l = i - offs;
  set_last_operation_count(l);
  if (r >= 0 || r == stream_result::END_OF_STREAM) {
    return l;
  }
  set_stream_result(r);
  return r;
}

signed long write_string_to_stream(output_stream &stream, const char *buff,
                                   size_t length) {
  size_t end = length;
  size_t i;
  int r;
  for (i = 0; i < end; i++) {
    char c = buff[i];
    if (c == 0) {
      set_last_operation_count(i);
      return i;
    }
    r = stream.write(buff[i]);
    if (r < 0) {
      set_last_operation_count(i);
      return r;
    }
  }

  return buff[i] == 0 ? i : stream_result::DATA_TRUNCATED;
}

signed long write_json_string_to_stream(output_stream &stream, const char *buff,
                                        size_t length) {
  static constexpr const char digits[] = "0123456789abcdef";
  size_t end = length;
  size_t i;
  int r;
  int extraOutputChars = 0;
  for (i = 0; i < end; i++) {
    char c = buff[i];
    if (c == 0) {
      set_last_operation_count(i);
      return i;
    }
    int xChars;
    if (c == '\"' || c == '\\' || c == '/') {
      r = stream.write('\\');
      r = r >= 0 ? stream.write(c) : r;
      xChars = 1;
    } else if (c == '\b') {
      r = stream.write('\\');
      r = r >= 0 ? stream.write('b') : r;
      xChars = 1;
    } else if (c == '\f') {
      r = stream.write('\\');
      r = r >= 0 ? stream.write('f') : r;
      xChars = 1;
    } else if (c == '\n') {
      r = stream.write('\\');
      r = r >= 0 ? stream.write('n') : r;
      xChars = 1;
    } else if (c == '\r') {
      r = stream.write('\\');
      r = r >= 0 ? stream.write('r') : r;
      xChars = 1;
    } else if (c == '\t') {
      r = stream.write('\\');
      r = r >= 0 ? stream.write('t') : r;
      xChars = 1;
    } else if (c < ' ') {
      r = stream.write('\\');
      r = r >= 0 ? stream.write('u') : r;
      r = r >= 0 ? stream.write('0') : r;
      r = r >= 0 ? stream.write('0') : r;
      r = r >= 0 ? stream.write(digits[c / 16]) : r;
      r = r >= 0 ? stream.write(digits[c % 16]) : r;
      xChars = 5;
    } else {
      r = stream.write(c);
      xChars = 0;
    }
    if (r < 0) {
      set_last_operation_count(i);
      return r;
    }
    extraOutputChars += xChars;
  }

  return buff[i] == 0 ? i : stream_result::DATA_TRUNCATED;
}

signed long input_stream::read(void *buff, size_t offs, size_t length) {
  return read_from_stream(*this, buff, offs, length);
}

signed long input_stream::read_line(char *line, size_t line_size) {
  return read_line_from_stream(*this, line, line_size);
}

signed long output_stream::write(const void *buff, size_t offs, size_t length) {
  return write_to_stream(*this, buff, offs, length);
}

signed long output_stream::write_string(const char *string, size_t length) {
  return write_string_to_stream(*this, string, length);
}
signed long output_stream::write_string(const char *string) {
  if (!string) {
    return 0;
  }
  auto p = string;
  size_t len;
  for (len = 0; len < 1048576 && *p; len++, p++)
    ;
  return write_string(string, len);
}

signed long output_stream::write_json_string(const char *string) {
  if (!string) {
    return 0;
  }
  auto p = string;
  size_t len;
  for (len = 0; len < 1048576 && *p; len++, p++)
    ;
  return write_json_string_to_stream(*this, string, len);
}


} // namespace speakerman
