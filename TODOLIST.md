## Current

- [x] Use new API
- [x] README: Add libtioj
- [x] Only allow a fixed number of submissions in queue
- [x] Set a maximum filesize / RSS limit
- [x] Option to ignore some testdata in overall verdict
- [x] Provide Dockerfile
- [x] Move DB operations from `server_io.cpp` to a separate file
- [x] Fix job counting bug
- [x] ActionCable for server IO
- [x] Use pipe output in strict mode
- [x] Multistage problems & update README
- [x] Linear time calibration
- [x] Different comparison methods
- [x] Additional message
- [ ] Optionally invoke special judge between stages in multistage

## Future

- [ ] Add option to stop on first non-AC testdata of each task (TODO FEATURE(group))
- [ ] Pin judge (platform-independent judge result)
- [ ] I/O Interactive (TODO FEATURE(io-interactive))
    - I/O interactive should also support multistage (similar to that of CMS Communication)
    - Modify cjail to allow specify cgroup name for CPU time / memory accounting (set a hard limit for sum of all tasks to avoid attacks)
        - Add cgroup_settings & cgroup_base_name to struct cjail_ctx; implement runtime detection of cgroups (for cgroup_settings = NULL)
    - Treat all processes as one task
- [ ] Output-only
    - Needs a lot of modifications on web server and API
- [ ] Optimize task result sending (not send every task result every time)
- [ ] Refactor: add testdata path into `class Submission` and move related paths out of libtioj
- [ ] Periodically clear testdata for recently unused problems
- [ ] Add more languages
- [ ] Add non-root judge (use libfakechroot for isolation; no RSS limiting & VSS reporting support)
- [ ] Add command-line utility for judging
- [ ] Cache special judge compile results
- [ ] Reference program for time calibration
