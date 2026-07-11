#include <errno.h>
#include <unistd.h>

#include "sandbox.h"

namespace {

constexpr long kMaxPayload = 1 << 20;

bool ReadFull(int fd, void* buf, size_t n) {
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

bool WriteFull(int fd, const void* buf, size_t n) {
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

struct cjail_result SandboxExec(const SandboxOptions& opt) {
  CJailCtxClass ctx = opt.ToCJailCtx();
  struct cjail_result ret = {};
  if (cjail_exec(&ctx.GetCtx(), &ret) < 0) {
    ret.oomkill = errno;
    ret.timekill = -1;
  }
  return ret;
}

} // namespace

int main() {
  long sz = 0;
  if (!ReadFull(0, &sz, sizeof(sz))) return 1;
  if (sz <= 0 || sz > kMaxPayload) return 1;
  std::vector<uint8_t> buf(sz);
  if (!ReadFull(0, buf.data(), static_cast<size_t>(sz))) return 1;
  struct cjail_result res = SandboxExec(SandboxOptions(buf));
  if (!WriteFull(1, &res, sizeof(res))) return 1;
}
