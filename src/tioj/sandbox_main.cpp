#include <errno.h>
#include <unistd.h>

#include "sandbox.h"

namespace {

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
  if (read(0, &sz, sizeof(sz)) < 0) return 1;
  std::vector<uint8_t> buf(sz);
  if (read(0, buf.data(), sz) < 0) return 1;
  struct cjail_result res = SandboxExec(SandboxOptions(buf));
  if (write(1, &res, sizeof(res)) < 0) return 1;
}
