#ifndef SERVER_IO_H_
#define SERVER_IO_H_

#include <string>
#include "tioj/submission.h"

extern std::string kTIOJUrl;
extern std::string kTIOJKey;
extern double kFetchInterval;

// TODO FEATURE(web-update-2): currently the fetching process is queued -> validating (atomically set when fetched by any client)
//   we need to change it to queued -> fetched (the original validating) -> validating (once start compiling)
//   this involves changes on web server
// Note that we also need to add some work balancing on webserver in case of multiple clients,
//   because now they will try to greedily fetch submissions to judge them in parallel
//   (instead of miku's behavior of fetching after finishing the running submission),
//   and this may result in inbalanced fetching.
// We can implement it by putting in use the currently-unused judge_servers table

// This function is blocking
bool FetchOneSubmission();

// These functions will push the request into queue and return immediately
void SendResult(const Submission&, bool done);
void SendCEMessage(const Submission&);
void SendValidating(int submission_id); // unused until TODO(web-update-2) is finished

// This function will call FetchOneSubmission() periodically, and also deal with
//   the requests sent from the above functions
void ServerWorkLoop();

#endif  // SERVER_IO_H_
