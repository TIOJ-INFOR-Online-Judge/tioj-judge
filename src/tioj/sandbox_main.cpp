#include <errno.h>
#include <unistd.h>

#include "sandbox.h"
#include "sandbox_io.h"

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
  if (!ReadFull(0, &sz, sizeof(sz))) return 1;
  if (sz <= 0 || sz > kSandboxMaxPayload) return 1;
  std::vector<uint8_t> buf(sz);
  if (!ReadFull(0, buf.data(), static_cast<size_t>(sz))) return 1;
  struct cjail_result res = SandboxExec(SandboxOptions(buf));
  if (!WriteFull(1, &res, sizeof(res))) return 1;
}
