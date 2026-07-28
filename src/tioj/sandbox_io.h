#ifndef TIOJ_SANDBOX_IO_H_
#define TIOJ_SANDBOX_IO_H_

#include <cstdlib>
#include <errno.h>
#include <unistd.h>

constexpr long kSandboxMaxPayload = 1 << 20;

inline bool ReadFull(int fd, void* buf, size_t n) {
  size_t got = 0;
  while (got < n) {
    ssize_t r = read(fd, static_cast<char*>(buf) + got, n - got);
    if (r < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    if (r == 0) return false;
    got += static_cast<size_t>(r);
  }
  return true;
}

inline bool WriteFull(int fd, const void* buf, size_t n) {
  size_t put = 0;
  while (put < n) {
    ssize_t w = write(fd, static_cast<const char*>(buf) + put, n - put);
    if (w < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    if (w == 0) return false;
    put += static_cast<size_t>(w);
  }
  return true;
}

#endif // TIOJ_SANDBOX_IO_H_
