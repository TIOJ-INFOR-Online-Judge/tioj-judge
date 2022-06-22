#include "sandbox_exec.h"

#include <unistd.h>
#include <sys/wait.h>

#include <fmt/ranges.h>
#include <spdlog/spdlog.h>
#include <tioj/paths.h>

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
    auto cmd = kDataDir / "sandbox-exec";
    if (execl(cmd.c_str(), cmd.c_str(), nullptr) < 0) _exit(1);
  }
  {
    spdlog::debug("cjail_exec pid={} childpid={} boxdir={} command={}",
        getpid(), pid, opt.boxdir, fmt::format("{}", opt.command));
    close(inpipe[1]);
    close(outpipe[0]);
    auto vec = opt.Serialize();
    long size = vec.size();
    if (write(outpipe[1], &size, sizeof(size)) < 0 ||
        write(outpipe[1], vec.data(), vec.size()) < 0 ||
        read(inpipe[0], &ret, sizeof(ret)) < 0) {
      kill(SIGKILL, pid);
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
