#ifndef TIOJ_SANDBOX_H_
#define TIOJ_SANDBOX_H_

#include <string>
#include <vector>

#include <cjail/cjail.h>

class SandboxOptions;
class CJailCtxClass {
 private:
  std::vector<const char*> argv_buf_;
  std::vector<const char*> env_buf_;
  std::vector<std::string> str_buf_;
  std::vector<struct jail_mount_ctx> mnt_buf_;
  struct jail_mount_list* mnt_list_;
  cpu_set_t cpu_set_;
  struct cjail_ctx ctx_;
 public:
  CJailCtxClass() : mnt_list_(mnt_list_new()) {}
  ~CJailCtxClass() {
    mnt_list_free(mnt_list_);
  }
  struct cjail_ctx& GetCtx() { return ctx_; }
  const struct cjail_ctx& GetCtx() const { return ctx_; }

  friend class SandboxOptions;
};

class SandboxOptions {
  using Int = long; // serialize
 public:
  std::string boxdir;
  std::vector<std::string> command;
  bool preserve_env; // override envs
  std::vector<std::string> envs;
  // inside box (relative to boxdir but start with /)
  std::string workdir, input, output, error;
  int fd_input, fd_output, fd_error; // -1 for not dup; overrides input/output
  std::vector<int> cpu_set;
  int uid, gid;
  long wall_time, cpu_time; // us
  long rss, vss; // KiB
  int proc_num;
  int file_num;
  long fsize; // KiB
  std::vector<std::string> dirs;

  SandboxOptions() :
      preserve_env(false),
      fd_input(-1), fd_output(-1), fd_error(-1),
      uid(65534), gid(65534),
      wall_time(0), cpu_time(0),
      rss(0), vss(0),
      proc_num(0),
      file_num(0),
      fsize(0) {}
  SandboxOptions(const std::vector<uint8_t>& serial);

  void FilterDirs();
  // platform dependent, only intended for same machine
  std::vector<uint8_t> Serialize() const;
  // the result is invalidated after reassignment/reallocation of any string/vector member
  CJailCtxClass ToCJailCtx() const;
};

#endif  // TIOJ_SANDBOX_H_
