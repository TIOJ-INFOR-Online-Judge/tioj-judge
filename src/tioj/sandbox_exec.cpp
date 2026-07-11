#include "sandbox_exec.h"

#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>

#include <spdlog/fmt/bundled/ranges.h>
#include <spdlog/spdlog.h>
#include <tioj/paths.h>

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

} // namespace

struct cjail_result SandboxExec(const SandboxOptions& opt) {
  struct cjail_result ret = {};
  int inpipe[2], outpipe[2];
  pid_t pid;
  if (pipe(inpipe) < 0 || pipe(outpipe) < 0) goto err;
  pid = fork();
  if (pid < 0) goto err;
  if (pid == 0) {
    dup2(inpipe[1], 1);
    dup2(outpipe[0], 0);
    close(inpipe[0]);
    close(inpipe[1]);
    close(outpipe[0]);
    close(outpipe[1]);
    auto cmd = internal::kDataDir / "sandbox-exec";
    if (execl(cmd.c_str(), cmd.c_str(), nullptr) < 0) _exit(1);
  }
  {
    spdlog::debug("cjail_exec pid={} childpid={} boxdir={} command={}", getpid(), pid, opt.boxdir,
                  fmt::format("{}", opt.command));
    close(inpipe[1]);
    close(outpipe[0]);
    auto vec = opt.Serialize();
    long size = vec.size();
    if (vec.empty() || vec.size() > static_cast<size_t>(kMaxPayload) ||
        !WriteFull(outpipe[1], &size, sizeof(size)) || !WriteFull(outpipe[1], vec.data(), vec.size()) ||
        !ReadFull(inpipe[0], &ret, sizeof(ret))) {
      kill(pid, SIGKILL);
      waitpid(pid, nullptr, 0);
      goto err;
    }
  }
  waitpid(pid, nullptr, 0);
  if (ret.timekill == -1) {
    spdlog::warn("cjail_exec error: errno={} {}", ret.oomkill, strerror(ret.oomkill));
  }
  return ret;
err:
  spdlog::warn("SandboxExec error: errno={} {}", errno, strerror(errno));
  ret.oomkill = errno;
  ret.timekill = -1;
  return ret;
}
