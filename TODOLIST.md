## Current

[x] Use new API
[x] README: Add libtioj
[x] Only allow a fixed number of submissions in queue
[x] Set a maximum filesize / RSS limit
[x] Option to ignore some testdata in overall verdict
[x] Provide Dockerfile
[ ] Move DB operations from `server_io.cpp` to a separate file

## Future

[ ] Periodically clear testdata for recently unused problems
[ ] Add option to stop on first non-AC testdata of each task
[ ] Linear time calibration by a reference program (for multiple judge servers)
[ ] Add more languages
[ ] Add non-root judge (use libfakechroot for isolation; no RSS limiting & VSS reporting support)
[ ] Add command-line utility for judging
[ ] Cache special judge compile results