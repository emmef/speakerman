/*
 * WebServer.hpp
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

#ifndef SMS_SPEAKERMAN_SERVER_SOCKET_GUARD_H_
#define SMS_SPEAKERMAN_SERVER_SOCKET_GUARD_H_

#include <condition_variable>
#include <iostream>
#include <speakerman/SocketStream.hpp>
#include <sys/socket.h>

namespace speakerman {
enum class server_socket_state { CLOSED, LISTENING, WORKING, SHUTTING_DOWN };

enum class server_worker_result { CONTINUE, STOP };

int open_server_socket(const char *service, int timeoutSeconds, int backLog,
                       int *errorCode);
bool set_linger_seconds(int sock, int linger_seconds);
bool set_reuse(int sock, bool reuse);
timeval get_time_in_millis(long timeout_millis);
bool set_recv_timeout_millis(int sock, long timeout_millis);

class socket_selector;
class socket_selector_iterator {
  socket_selector *selector_;
  int position_ = 0;
  int next_descriptor_ = -1;
  int error_code_;

  int fetch_next();
  friend class socket_selector;

  socket_selector_iterator(socket_selector *created_by);
  socket_selector_iterator(int error_code);

public:
  bool error_occured() const;
  int error_code() const;
  bool has_next();
  int get_next();
};

class socket_selector {
  fd_set master;
  fd_set readers;
  int sdf_ = -1;
  int max_fd_ = -1;
  friend class socket_selector_iterator;

public:
  socket_selector(int socket_file_descriptor) { init(socket_file_descriptor); }

  socket_selector() : socket_selector(-1) {}

  void reset() { init(-1); }

  void init(int socket_file_descriptor);

  socket_selector_iterator do_select(timeval tv, bool for_flush);
};

class server_socket {
public:
  using State = server_socket_state;
  using Result = server_worker_result;
  using Lock = std::unique_lock<std::mutex>;
  using Stream = socket_stream;

  typedef server_worker_result (*server_socket_worker)(
      socket_stream &stream, const server_socket &server, void *data);

public:
  server_socket() {}

  server_socket(const char *service, int timeoutSeconds, int backLog,
                int *errorCode);

  bool open(const char *service, int timeoutSeconds, int backLog,
            int *errorCode);

  const char *service() const { return service_; }

  State state() const;

  bool isOpen() const { return state() != State::CLOSED; }

  bool isWorking() const { return state() == State::WORKING; }

  bool work(int *errorCode, server_socket_worker worker, void *data);

  void close();

  ~server_socket();

private:
  std::mutex mutex_;
  std::condition_variable condition_;
  int sockfd_ = -1;
  const char *service_ = nullptr;
  State state_ = State::CLOSED;
  socket_selector selector_;

  bool await_work_done(int timeOutSeconds, Lock &lock, int *errorCode);

  void close(Lock &lock);

  bool enterWork(int *errorCode);
};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_SERVER_SOCKET_GUARD_H_ */
