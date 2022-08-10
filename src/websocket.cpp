#include "websocket.h"

WsClient::WsClient(const std::string& url) : url_(url), connected_(false), close_issued_(false) {
  if (url.substr(0, 3) == "wss") {
    client_.emplace<TLSClient>();
    Init_(std::get<TLSClient>(client_));
  } else {
    Init_(std::get<NoTLSClient>(client_));
  }
}

WsClient::~WsClient() {
  if (IsTLS()) {
    Destroy_(std::get<TLSClient>(client_));
  } else {
    Destroy_(std::get<NoTLSClient>(client_));
  }
  thr_->join();
}

bool WsClient::Connect() {
  if (IsTLS()) {
    return Connect_(std::get<TLSClient>(client_));
  } else {
    return Connect_(std::get<NoTLSClient>(client_));
  }
}

bool WsClient::Send(const std::string& str) {
  if (IsTLS()) {
    return Send_(std::get<TLSClient>(client_), str);
  } else {
    return Send_(std::get<NoTLSClient>(client_), str);
  }
}

bool WsClient::Close() {
  if (IsTLS()) {
    return Close_(std::get<TLSClient>(client_));
  } else {
    return Close_(std::get<NoTLSClient>(client_));
  }
}
