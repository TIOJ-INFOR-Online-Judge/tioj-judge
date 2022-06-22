#ifndef SANDBOX_EXEC_H_
#define SANDBOX_EXEC_H_

#include "sandbox.h"

// We separate this from sandbox.h because this function needs libtioj and other logging functions,
//   while we need to keep sandbox.h as small as possible

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

#endif  // SANDBOX_EXEC_H_
