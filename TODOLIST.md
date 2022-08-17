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
- [ ] Multistage problems & update README
- [x] Linear time calibration

## Future

- [ ] Add option to stop on first non-AC testdata of each task
- [ ] Pin judge (platform-independent judge result)
- [ ] I/O Interactive & output-only
    - I/O interactive should also support multistage (similar to that of CMS Communication)
    - Output-only needs a lot of modifications on web server and API
- [ ] Periodically clear testdata for recently unused problems
- [ ] Add more languages
- [ ] Add non-root judge (use libfakechroot for isolation; no RSS limiting & VSS reporting support)
- [ ] Add command-line utility for judging
- [ ] Cache special judge compile results
- [ ] Reference program for time calibration
