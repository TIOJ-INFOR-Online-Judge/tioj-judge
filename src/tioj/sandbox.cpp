#include "sandbox.h"

#include <unistd.h>
#include <cstring>

SandboxOptions::SandboxOptions(const std::vector<uint8_t>& vec) {
  size_t cur = 0;
  auto ReadInt = [&]() {
    Int r = *(Int*)(vec.data() + cur);
    cur += sizeof(Int);
    return r;
  };
  auto ReadString = [&]() {
    Int size = ReadInt();
    std::string str(size, '\0');
    memcpy(str.data(), vec.data() + cur, size);
    cur += size;
    return str;
  };
  boxdir = ReadString();
  command.resize(ReadInt());
  for (auto& i : command) i = ReadString();
  preserve_env = ReadInt();
  envs.resize(ReadInt());
  for (auto& i : envs) i = ReadString();
  workdir = ReadString();
  input = ReadString();
  output = ReadString();
  error = ReadString();
  fd_input = ReadInt();
  fd_output = ReadInt();
  fd_error = ReadInt();
  cpu_set.resize(ReadInt());
  for (auto& i : cpu_set) i = ReadInt();
  uid = ReadInt();
  gid = ReadInt();
  wall_time = ReadInt();
  cpu_time = ReadInt();
  rss = ReadInt();
  vss = ReadInt();
  proc_num = ReadInt();
  file_num = ReadInt();
  fsize = ReadInt();
  dirs.resize(ReadInt());
  for (auto& i : dirs) i = ReadString();
}

std::vector<uint8_t> SandboxOptions::Serialize() const {
  std::vector<uint8_t> ret;
  auto AddLenWrite = [&](size_t len, Int r){
    Int cur = ret.size();
    ret.resize(cur + len);
    *(Int*)(ret.data() + cur) = r;
  };
  auto PushInt = [&](Int r) { AddLenWrite(sizeof(Int), r); };
  auto PushString = [&](const std::string& str){
    Int cur = ret.size();
    AddLenWrite(str.size() + sizeof(Int), str.size());
    memcpy(ret.data() + (cur + sizeof(Int)), str.c_str(), str.size());
  };
  PushString(boxdir);
  PushInt(command.size());
  for (auto& i : command) PushString(i);
  PushInt(preserve_env);
  PushInt(envs.size());
  for (auto& i : envs) PushString(i);
  PushString(workdir);
  PushString(input);
  PushString(output);
  PushString(error);
  PushInt(fd_input);
  PushInt(fd_output);
  PushInt(fd_error);
  PushInt(cpu_set.size());
  for (auto& i : cpu_set) PushInt(i);
  PushInt(uid);
  PushInt(gid);
  PushInt(wall_time);
  PushInt(cpu_time);
  PushInt(rss);
  PushInt(vss);
  PushInt(proc_num);
  PushInt(file_num);
  PushInt(fsize);
  PushInt(dirs.size());
  for (auto& i : dirs) PushString(i);
  return ret;
}

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
  ret.argv_buf_.push_back(nullptr);
  ctx.argv = const_cast<char* const*>(ret.argv_buf_.data());
  if (preserve_env) {
    ctx.environ = environ;
  } else {
    for (auto& i : envs) ret.env_buf_.emplace_back(i.data());
    ret.env_buf_.push_back(nullptr);
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
  ctx.lim_cputime.tv_sec = cpu_time / 1'000'000;
  ctx.lim_cputime.tv_usec = cpu_time % 1'000'000;
  // default: cputime_poll_interval
  // default: seccomp_cfg
  // bind mounts
  // reallocation of str_buf_ invalidate str.data(), thus we need to reserve it first
  ret.str_buf_.reserve(dirs.size());
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
