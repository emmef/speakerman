//
// Created by michel on 07-02-22.
//

#include <iostream>
#include <speakerman/Webserver.h>
#include <speakerman/jack/SignalHandler.hpp>

namespace speakerman {

namespace {

class WebManagerOwner {
  mg_mgr manager;

public:
  WebManagerOwner(const WebManagerOwner &) = delete;
  WebManagerOwner(WebManagerOwner &&) = delete;
  WebManagerOwner() { mg_mgr_init(&manager); }

  mg_mgr &operator->() { return manager; }

  mg_mgr *operator()() { return &manager; }

  ~WebManagerOwner() { mg_mgr_free(&manager); }
};

} // namespace

void WebServer::staticHandle(struct mg_connection *connection, int event,
                             void *eventData, void *webServerInstance) {
  if (webServerInstance) {
    switch (event) {
    case MG_EV_HTTP_MSG:
      static_cast<WebServer *>(webServerInstance)
          ->defaultHandle(connection,
                          static_cast<mg_http_message *>(eventData));
      break;
    default:
      break;//std::cout << "Got event: " << eventName(event) << std::endl;
    }
  }
}

WebServer::WebServer(const char *staticDocumentRoot)
    : documentRoot(staticDocumentRoot) {}

void WebServer::defaultHandle(mg_connection *connection,
                              mg_http_message *httpMessage) {

  try {
    switch (handle(connection, httpMessage)) {
    case HttpResultHandleResult::Ok:
      return;
    case HttpResultHandleResult::Fail:
      mg_http_reply(connection, 500, nullptr, "No information");
      return;
    default:
      break;
    }
    mg_http_serve_opts opts = {0};
    opts.root_dir = documentRoot;
    opts.ssi_pattern = "#.shtml";
    mg_http_serve_dir(connection, httpMessage, &opts);
  } catch (const std::exception &e) {
    mg_http_reply(connection, 500, nullptr, e.what());
  }
}

void WebServer::run(const char *listeningAddress, long pollMillis) {
  class RunCounter {
    std::atomic_int &count;
    std::condition_variable &variable;

  public:
    RunCounter(std::atomic_int &c, std::condition_variable &v)
        : count(c), variable(v) {
      count.fetch_add(1);
    }

    ~RunCounter() {
      if (count.fetch_sub(1) == 1) {
        variable.notify_all();
      }
    }
  };

  WebManagerOwner manager;
  mg_connection *connection =
      mg_http_listen(manager(), listeningAddress, staticHandle, this);
  if (connection == nullptr) {
    std::cerr << "Failed to start listening on \"" << listeningAddress << "\""
              << std::endl;
    return;
  }
  RunCounter counter(count, variable);
  while (true) {
    if (jack::SignalHandler::is_set()) {
      std::cerr << "Received signal " << jack::SignalHandler::get_signal()
                << std::endl;
      break;
    }
    if (stop) {
      std::cout << "Webserver used awaitStop()" << std::endl;
      break;
    }
    mg_mgr_poll(manager(), pollMillis);
  }
}

void WebServer::awaitStop(long waitMillis) {
  struct StopGuard {
    std::atomic_bool &stop;
    bool didSet;
    StopGuard(std::atomic_bool &s) : stop(s) {
      bool expected = false;
      bool didSet = stop.compare_exchange_strong(expected, true);
    }
    ~StopGuard() {
      if (didSet) {
        stop = false;
      }
    }
  };

  std::unique_lock<std::mutex> lock(mutex);

  if (count > 0) {
    StopGuard stopGuard(stop);
    if (waitMillis > 0) {
      variable.wait_for(lock, std::chrono::microseconds(waitMillis));
    } else {
      variable.wait(lock);
    }
  }
}
const char *WebServer::eventName(int event) {
  switch (event) {
  case MG_EV_ERROR:
    return "Error";
  case MG_EV_OPEN:
    return "Connection created";
  case MG_EV_POLL:
    return "mg_mgr_poll iteration";
  case MG_EV_RESOLVE:
    return "Host name is resolved";
  case MG_EV_CONNECT:
    return "Connection established";
  case MG_EV_ACCEPT:
    return "Connection accepted";
  case MG_EV_READ:
    return "Data received from socket";
  case MG_EV_WRITE:
    return "Data written to socket";
  case MG_EV_CLOSE:
    return "Connection closed";
  case MG_EV_HTTP_MSG:
    return "HTTP request/response";
  case MG_EV_HTTP_CHUNK:
    return "HTTP chunk (partial msg)";
  case MG_EV_WS_OPEN:
    return "Websocket handshake done";
  case MG_EV_WS_MSG:
    return "Websocket msg, text or bin";
  case MG_EV_WS_CTL:
    return "Websocket control msg";
  case MG_EV_MQTT_CMD:
    return "MQTT low-level command";
  case MG_EV_MQTT_MSG:
    return "MQTT PUBLISH received";
  case MG_EV_MQTT_OPEN:
    return "MQTT CONNACK received";
  case MG_EV_SNTP_TIME:
    return "SNTP time received";
  default:
    return "User event";
  }
}

HttpResultHandleResult WebServer::handle(mg_connection *connection,
                                         mg_http_message *httpMessage) {
  return HttpResultHandleResult::Default;
};

} // namespace speakerman
