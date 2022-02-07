#ifndef SPEAKERMAN_M_WEBSERVER_H
#define SPEAKERMAN_M_WEBSERVER_H
/*
 * speakerman/Webserver.h
 *
 * Added by michel on 2022-02-07
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

#include <atomic>
#include <condition_variable>
#include <mongoose.h>
#include <mutex>

namespace speakerman {

enum class HttpResultHandleResult { Ok, Default, Fail };

class WebServer {
public:
  static const char *eventName(int event);

  const char *documentRoot;
  std::atomic_bool stop = false;
  std::atomic_int count = 0;
  std::condition_variable variable;
  std::mutex mutex;

  static void staticHandle(struct mg_connection *connection, int event,
                           void *eventData, void *webServerInstance);

  void defaultHandle(mg_connection *connection, mg_http_message *httpMessage);

protected:
  virtual HttpResultHandleResult handle(mg_connection *connection,
                                        mg_http_message *httpMessage);

public:
  WebServer(const char *documentRoot);

  void run(const char *listeningAddress, long pollMillis);

  void awaitStop(long awaitMillis);

  virtual ~WebServer() = default;
};
} // namespace speakerman

#endif // SPEAKERMAN_M_WEBSERVER_H
