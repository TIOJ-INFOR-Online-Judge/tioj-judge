#include "sandbox.h"

#include <unistd.h>
#include <string>

#include <fmt/ranges.h>
#include <spdlog/spdlog.h>
#include "utils.h"

CJailCtxClass SandboxOptions::ToCJailCtx() const {
  CJailCtxClass ret;
  struct cjail_ctx& ctx = ret.ctx_;
  cjail_ctx_init(&ctx);
  // default: preservefd, sharenet
  if (fd_input != -1) ctx.fd_input = fd_input;
  if (fd_output != -1) ctx.fd_output = fd_output;
  if (fd_error != -1) ctx.fd_error = fd_error;
  if (!input.empty()) ctx.redir_input = input.data();
  if (!output.empty()) ctx.redir_output = output.data();
  if (!error.empty()) ctx.redir_error = error.data();
  for (auto& i : command) ret.argv_buf_.push_back(i.data());
  ctx.argv = const_cast<char* const*>(ret.argv_buf_.data());
  if (preserve_env) {
    ctx.environ = environ;
  } else {
    for (auto& i : envs) ret.env_buf_.emplace_back(i.data());
    ctx.environ = const_cast<char* const*>(ret.env_buf_.data());
  }
  ctx.chroot = boxdir.data();
  ctx.working_dir = workdir.data();
  // default: cgroup_root
  if (cpu_set.empty()) {
    ctx.cpuset = nullptr;
  } else {
    CPU_ZERO(&ret.cpu_set_);
    for (auto& i : cpu_set) CPU_SET(i, &ret.cpu_set_);
  }
  ctx.cpuset = &ret.cpu_set_;
  ctx.uid = uid;
  ctx.gid = gid;
  ctx.rlim_as = vss;
  ctx.rlim_core = 0; // no core dump
  ctx.rlim_nofile = file_num;
  ctx.rlim_fsize = fsize;
  ctx.rlim_proc = proc_num;
  // default: rlim_stack (no limit)
  ctx.cg_rss = rss;
  ctx.lim_time.tv_sec = wall_time / 1'000'000;
  ctx.lim_time.tv_usec = wall_time % 1'000'000;
  // default: seccomp_cfg
  // bind mounts
  for (auto& i : dirs) {
    ret.mnt_buf_.emplace_back();
    struct jail_mount_ctx& mnt_ctx = ret.mnt_buf_.back();
    ret.str_buf_.push_back("bind");
    mnt_ctx.type = ret.str_buf_.back().data();
    mnt_ctx.source = mnt_ctx.target = i.data();
    mnt_ctx.fstype = mnt_ctx.data = nullptr;
    mnt_ctx.flags = 0;
    mnt_list_add(ret.mnt_list_, &mnt_ctx);
  }
  ctx.mount_cfg = ret.mnt_list_;
  return ret;
}

struct cjail_result SandboxExec(const SandboxOptions& opt) {
  CJailCtxClass ctx = opt.ToCJailCtx();
  struct cjail_result ret = {};
  spdlog::debug("cjail_exec command: {}", fmt::format("{}", opt.command));
  if (cjail_exec(&ctx.GetCtx(), &ret) < 0) {
    spdlog::warn("cjail_exec error: errno={} {}", errno, strerror(errno));
    ret.timekill = -1;
  }
  return ret;
}
