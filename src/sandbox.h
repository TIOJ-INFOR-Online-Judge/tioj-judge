#ifndef SANDBOX_H_
#define SANDBOX_H_

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
 public:
  std::string boxdir;
  std::vector<std::string> command;
  bool preserve_env; // override envs
  std::vector<std::string> envs;
  // inside box (relative to boxdir but start with /)
  std::string workdir, input, output, error;
  int input_fd, output_fd, error_fd; // -1 for not dup; overrides input/output
  std::vector<int> cpu_set;
  int uid, gid;
  long wall_time; // us
  long rss, vss; // KiB
  int proc_num;
  int file_num;
  long fsize; // KiB
  std::vector<std::string> dirs;
  std::string tmpfs;

  SandboxOptions() :
      input_fd(-1), output_fd(-1), error_fd(-1),
      uid(65534), gid(65534),
      wall_time(0),
      rss(0), vss(0),
      proc_num(1),
      file_num(0),
      fsize(0) {}
  ~SandboxOptions();
  // the result is invalidated after reassignment/reallocation of any string/vector member
  CJailCtxClass ToCJailCtx() const;
};

// before SandboxExec:
// 1. create temp dir
// 2. generate a unique uid&gid (and possibly unique CPU for cpuset)
// assign uid,gid,cpu_set,boxdir according to 1.2.
// 3. create workdir & input file and assign proper permission
//   - first mount a tmpfs with proper size (output limit) onto workdir
//   - for old-style compatility, first mount a tmpfs with proper size (output limit) onto workdir,
//     set workdir to be writable by uid, and set input/output/error file all inside workdir and writable by uid
//   - for strict style:
//     - don't mount workdir; make nothing inside jail writable by uid,
//       pre-open input/output file inside a directory not openable by uid
//       and set input_fd/output_fd to deliver output (this prevents user from reopening it)
//       and set error to /dev/null
//     - if pin is needed, mount a tmpfs with a small size (fits only pin output) onto workdir,
//       set workdir to be non-writable by uid and pre-create a file writable by uid for pin output
struct cjail_result SandboxExec(const SandboxOptions&);

#endif  // SANDBOX_H_
