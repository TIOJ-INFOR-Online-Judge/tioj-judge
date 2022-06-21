#include <tioj/logger.h>

#include <pthread.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/ansicolor_sink.h>
#include <cstdio>

using ansicolor_sink = spdlog::sinks::ansicolor_sink<spdlog::details::console_mutex>;

void Prepare() {
  for (auto& i : spdlog::default_logger()->sinks()) {
    if (auto ptr = dynamic_cast<ansicolor_sink*>(i.get())) {
      ptr->mutex_.lock();
    }
  }
}

void Child() {
  for (auto& i : spdlog::default_logger()->sinks()) {
    if (auto ptr = dynamic_cast<ansicolor_sink*>(i.get())) {
      ptr->mutex_.unlock();
    }
  }
}

void Parent() {
  for (auto& i : spdlog::default_logger()->sinks()) {
    if (auto ptr = dynamic_cast<ansicolor_sink*>(i.get())) {
      ptr->mutex_.unlock();
    }
  }
}

void InitLogger() {
  spdlog::debug("Setup logger pthread_atfork");
  pthread_atfork(Prepare, Parent, Child);
}
