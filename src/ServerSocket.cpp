/*
 * SpeakermanConfig.cpp
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

#include <cerrno>
#include <chrono>
#include <cstring>
#include <ctime>
#include <iostream>
#include <netdb.h>
#include <unistd.h>

#include <speakerman/ServerSocket.hpp>
#include <speakerman/jack/SignalHandler.hpp>
#include <tdap/MemoryFence.hpp>
#include <tdap/Value.hpp>

namespace speakerman {

using namespace tdap;

static int closeSocket(int socket) { return close(socket); }

/**
 * Manages a addrinfo structure as returned by getaddrinfo and
 * provides some convenience for obtaining it. Takes care of
 * freeing the structure (also on reassign).
 */
class addrinfo_owner {
  struct addrinfo *server_info;

public:
  /**
   * Returns address information structure based on parameters.
   * On success, the method returns a non-null structure, on error
   * the method places errno in the location specified by errorCode
   * or throws a runtime_exception if that location is nullptr.
   * @see getaddrinfo
   * @param name name
   * @param service service
   * @param hints hints
   * @param errorCode error location
   * @return struct addrinfo
   */
  static struct addrinfo *get(const char *name, const char *service,
                              const struct addrinfo &hints, int *errorCode);

  addrinfo_owner(struct addrinfo *info);

  addrinfo_owner();

  addrinfo_owner(const char *name, const char *service,
                 const struct addrinfo &hints, int *errorCode);

  addrinfo_owner(addrinfo_owner &&moved);

  void operator=(addrinfo_owner &source);

  void set_info(struct addrinfo *info);

  void set_info(const char *name, const char *service,
                const struct addrinfo &hints, int &errorCode);

  void set_info_throw(const char *name, const char *service,
                      const struct addrinfo &hints);

  struct addrinfo *info() const;

  struct addrinfo *operator->() const;

  ~addrinfo_owner();
};

struct socket_owner {
  int sd_ = -1;

public:
  socket_owner(int fd) : sd_(fd) {}

  socket_owner() : sd_(-1) {}

  void set(int fd) {
    if (sd_ != -1) {
      closeSocket(sd_);
    }
    sd_ = fd;
  }

  void operator=(int fd) { set(fd); }

  void disown() { sd_ = -1; }

  void close() { set(-1); }

  int get() const { return sd_; }

  int getAndOwn() {
    int r = sd_;
    sd_ = -1;
    return r;
  }

  operator int() const { return sd_; }

  operator int() { return sd_; }

  ~socket_owner() { close(); }
};

static bool ensure_bind(const addrinfo_owner &info, int sockfd_,
                        int timeoutSeconds, int *errorCode) {
  time_t end, now;
  time(&end);
  now = end;
  end += timeoutSeconds > 0 ? timeoutSeconds : 1000;

  int yes;
  if (setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
    std::cerr << "Couldn't set socket to reuse address mode: "
              << strerror(errno) << std::endl;
  }
  if (setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes)) == -1) {
    std::cerr << "Couldn't set socket to reuse address mode: "
              << strerror(errno) << std::endl;
  }
  int sleep_time = 1;
  while (true) {
    SignalHandler::check_raised();
    int result = bind(sockfd_, info->ai_addr, info->ai_addrlen);
    if (result != -1) {
      return true;
    }
    time(&now);
    int sleep_suggestion = 1; //(sleep_time + 1) * 3 / 2;
    int sleep_end = now + sleep_suggestion;
    int actual_end = end < sleep_end ? end : sleep_end;
    sleep_time = actual_end - now;
    if (sleep_time > 0) {
      sleep(sleep_time);
    } else {
      *errorCode = result;
      return false;
    }
  }
}

static struct addrinfo create_hints() {
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  return hints;
}

template <typename R>
static R returnFalseWithCode(int *resultVariable, R falseValue) {
  if (resultVariable) {
    *resultVariable = errno;
    return falseValue;
  }
  throw std::runtime_error(strerror(errno));
}

template <typename R> static R returnFalse(int *resultVariable, R falseValue) {
  return returnFalseWithCode(resultVariable, falseValue);
}

static bool returnFalse(int *resultVariable) {
  return returnFalseWithCode(resultVariable, false);
}

int open_server_socket(const char *service, int timeoutSeconds, int backLog,
                       int *errorCode) {
  socket_owner sock;
  struct addrinfo hints = create_hints();

  addrinfo_owner info(nullptr, service, hints, errorCode);

  sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
  if (sock == -1) {
    return returnFalse(errorCode, -1);
  }
  // try to set reuse studd
  set_reuse(sock, true);
  set_recv_timeout_millis(sock, 2000);
  set_linger_seconds(sock, 1);

  if (!ensure_bind(info, sock, timeoutSeconds, errorCode)) {
    return -1;
  }
  if (listen(sock, backLog) == -1) {
    return returnFalse(errorCode, -1);
  }

  return sock.getAndOwn();
}

bool set_recv_timeout_millis(int sock, long timeout_millis) {
  if (sock < 0) {
    return false;
  }
  timeval tv = get_time_in_millis(timeout_millis);
  return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0;
}

timeval get_time_in_millis(long timeout_millis) {
  timeval tv;
  tv.tv_sec = timeout_millis / 1000;
  tv.tv_usec = 1000 * (timeout_millis % 1000);
  return tv;
}

bool set_reuse(int sock, bool reuse) {
  if (sock < 0) {
    return false;
  }
  int yes = reuse ? 1 : 0;
  return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == 0;
}

bool set_linger_seconds(int sock, int linger_seconds) {
  if (sock < 0) {
    return false;
  }
  linger l;
  l.l_onoff = linger_seconds > 0;
  l.l_linger = linger_seconds;
  return setsockopt(sock, SOL_SOCKET, SO_LINGER, &l, sizeof(l)) == 0;
}

class state_on_close {
  server_socket_state &state_;
  std::unique_lock<std::mutex> &lock_;
  std::condition_variable &variable_;
  const server_socket_state expected_;
  const server_socket_state set_;

public:
  state_on_close(server_socket_state &state, std::unique_lock<std::mutex> &lock,
                 std::condition_variable &variable,
                 const server_socket_state expectedOnClose,
                 const server_socket_state setOnClose)
      : state_(state), lock_(lock), variable_(variable),
        expected_(expectedOnClose), set_(setOnClose) {}

  ~state_on_close() {
    lock_.lock();
    if (state_ == expected_) {
      state_ = set_;
    }
    variable_.notify_all();
  }
};

server_socket::server_socket(const char *service, int timeoutSeconds,
                             int backLog, int *errorCode) {
  open(service, timeoutSeconds, backLog, errorCode);
}

speakerman::server_socket::State server_socket::state() const {
  tdap::MemoryFence fence;
  return state_;
}

bool server_socket::await_work_done(int timeOutSeconds, Lock &lock,
                                    int *errorCode) {
  if (state_ != State::WORKING) {
    return true;
  }
  state_ = State::SHUTTING_DOWN;
  std::chrono::seconds duration(timeOutSeconds);
  condition_.wait_for(lock, duration);

  switch (state_) {
  case State::LISTENING:
    return true;
  case State::WORKING:
  case State::SHUTTING_DOWN:
    return returnFalseWithCode(errorCode, ETIMEDOUT);
  default:
    return returnFalseWithCode(errorCode, ECANCELED);
  }
}

bool server_socket::open(const char *service, int timeoutSeconds, int backLog,
                         int *errorCode) {
  socket_owner socket =
      open_server_socket(service, timeoutSeconds, backLog, errorCode);
  Lock lock(mutex_);
  if (!await_work_done(timeoutSeconds, lock, errorCode)) {
    return false;
  }
  close(lock);
  sockfd_ = socket.getAndOwn();
  selector_.init(sockfd_);
  service_ = service;
  state_ = State::LISTENING;
  condition_.notify_all();
  return true;
}

bool server_socket::enterWork(int *errorCode) {
  switch (state_) {
  case State::LISTENING:
    state_ = State::WORKING;
    return true;
  case State::SHUTTING_DOWN:
    return false;
  default:
    return returnFalseWithCode(errorCode, EBADFD);
  }
}

bool server_socket::work(int *errorCode, server_socket_worker worker,
                         void *data) {
  if (!worker) {
    throw std::invalid_argument("Worker function cannot be NULL");
  }
  Lock lock(mutex_);

  switch (state_) {
  case State::LISTENING:
    state_ = State::WORKING;
    break;
  case State::WORKING:
    return returnFalseWithCode(errorCode, EBUSY);
  default:
    return returnFalseWithCode(errorCode, EBADFD);
  }
  state_on_close state_guard(state_, lock, condition_, State::WORKING,
                             State::LISTENING);
  State state = state_;
  lock.unlock();

  timeval tv;
  tv.tv_sec = 2;
  tv.tv_usec = 0;

  bool raised = false;
  while (state == State::WORKING && !raised) {
    raised = SignalHandler::is_set();

    socket_selector_iterator iterator = selector_.do_select(tv, raised);
    if (iterator.error_occured()) {
      std::cerr << "Error occured during select" << iterator.error_code()
                << std::endl;
      if (errorCode) {
        *errorCode = iterator.error_code();
      }
      return false;
    }
    while (iterator.has_next()) {
      int i = iterator.get_next();
      if (raised) {
        closeSocket(i);
      } else {
        socket_stream stream(i, true);
        try {
          Result r = worker(stream, *this, data);
          if (r == Result::STOP) {
            return true;
          }
        } catch (const std::exception &e) {
          std::cerr << "Error during work on socket: " << e.what() << std::endl;
        }
      }
    }

    state = this->state();
  }
  return false;
}

void server_socket::close(Lock &lock) {
  if (sockfd_ == -1) {
    return;
  }
  int error = 0;
  if (!await_work_done(1, lock, &error)) {
    std::cerr << "Aborted waiting for work stop: " << strerror(error)
              << std::endl;
  }
  sockfd_ = -1;
  timeval tv;
  tv.tv_sec = 2;
  tv.tv_usec = 0;
  socket_selector_iterator iterator = selector_.do_select(tv, true);
  while (iterator.has_next()) {
    closeSocket(iterator.get_next());
  }
  if (closeSocket(sockfd_) != 0) {
    std::cerr << "Error when closing socket: " << strerror(errno) << std::endl;
  }
  state_ = State::CLOSED;
  condition_.notify_all();
}

void server_socket::close() {
  Lock lock(mutex_);
  close(lock);
}

server_socket::~server_socket() { close(); }

struct addrinfo *addrinfo_owner::get(const char *name, const char *service,
                                     const struct addrinfo &hints,
                                     int *errorCode) {
  struct addrinfo *result;
  int code = getaddrinfo(name, service, &hints, &result);
  if (code == 0) {
    return result;
  }
  if (errorCode) {
    *errorCode = errno;
    return nullptr;
  }
  throw std::runtime_error(strerror(errno));
}

addrinfo_owner::addrinfo_owner(addrinfo_owner &&moved)
    : addrinfo_owner(moved.server_info) {
  moved.server_info = nullptr;
}

addrinfo_owner::addrinfo_owner(const char *name, const char *service,
                               const struct addrinfo &hints, int *errorCode)
    : addrinfo_owner(get(name, service, hints, errorCode)) {}

addrinfo_owner::addrinfo_owner() : addrinfo_owner(nullptr) {}

addrinfo_owner::addrinfo_owner(struct addrinfo *info) : server_info(info) {}

void addrinfo_owner::operator=(addrinfo_owner &source) {
  server_info = source.server_info;
  source.server_info = nullptr;
}

void addrinfo_owner::set_info(const char *name, const char *service,
                              const struct addrinfo &hints, int &errorCode) {
  set_info(get(name, service, hints, &errorCode));
}

void addrinfo_owner::set_info(struct addrinfo *info) {
  if (server_info) {
    freeaddrinfo(server_info);
  }
  server_info = info;
}

void addrinfo_owner::set_info_throw(const char *name, const char *service,
                                    const struct addrinfo &hints) {
  set_info(get(name, service, hints, nullptr));
}

addrinfo_owner::~addrinfo_owner() { set_info(nullptr); }

struct addrinfo *addrinfo_owner::info() const {
  return server_info;
}

struct addrinfo *addrinfo_owner::operator->() const {
  if (server_info) {
    return server_info;
  }
  throw std::runtime_error("NULL dereference");
}

socket_selector_iterator::socket_selector_iterator(int error_code)
    : selector_(nullptr), error_code_(error_code) {}

socket_selector_iterator::socket_selector_iterator(socket_selector *created_by)
    : selector_(created_by), error_code_(0) {}

bool socket_selector_iterator::error_occured() const {
  return selector_ == nullptr;
}

int socket_selector_iterator::error_code() const { return error_code_; }

int socket_selector_iterator::fetch_next() {
  for (; position_ <= selector_->max_fd_; position_++) {
    if (FD_ISSET(position_, &selector_->readers) &&
        position_ != selector_->sdf_) {
      next_descriptor_ = position_++;
      return next_descriptor_;
    }
  }
  return -1;
}

bool socket_selector_iterator::has_next() {
  return selector_ && (next_descriptor_ != -1 || fetch_next() != -1);
}

int socket_selector_iterator::get_next() {
  if (has_next()) {
    int result = next_descriptor_;
    next_descriptor_ = -1;
    FD_CLR(result, &selector_->master);
    return result;
  }
  return -1;
}

void socket_selector::init(int socket_file_descriptor) {
  sdf_ = socket_file_descriptor >= 0 ? socket_file_descriptor : -1;
  max_fd_ = sdf_;
  FD_ZERO(&master);
  FD_ZERO(&readers);
  if (sdf_ > 0) {
    FD_SET(sdf_, &master);
  }
}

socket_selector_iterator socket_selector::do_select(timeval tv,
                                                    bool for_flush) {
  if (sdf_ < 0) {
    return socket_selector_iterator(EBADF);
  }
  readers = master;
  int error_code = 0;
  if (select(max_fd_ + 1, &readers, nullptr, nullptr, &tv) == -1) {
    returnFalse(&error_code);
    return socket_selector_iterator(error_code);
  }
  if (for_flush) {
    socket_selector_iterator(this);
  }
  int acceptDescriptor = -1;
  sockaddr address;
  socklen_t length = sizeof(sockaddr_storage);
  for (int i = 0; i <= max_fd_; i++) {
    if (FD_ISSET(i, &readers) && i == sdf_) {
      acceptDescriptor = accept(sdf_, &address, &length);
      if (acceptDescriptor == -1) {
        returnFalse(&error_code);
        std::cerr << "Could not accept new connection" << std::endl;
      } else {
        FD_SET(acceptDescriptor, &master);
        if (acceptDescriptor > max_fd_) {
          max_fd_ = acceptDescriptor;
        }
        set_linger_seconds(acceptDescriptor, tv.tv_sec > 0 ? tv.tv_sec : 0);
      }
    }
  }
  return socket_selector_iterator(this);
}

} /* End of namespace speakerman */
