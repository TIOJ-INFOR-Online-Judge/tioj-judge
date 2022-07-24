#ifndef SERVER_IO_H_
#define SERVER_IO_H_

#include <string>
#include "tioj/submission.h"

extern std::string kTIOJUrl;
extern std::string kTIOJKey;
extern double kFetchInterval;
extern int kMaxQueue;

// Note that we also need to add some work balancing on webserver in case of multiple clients,
//   because now they will try to greedily fetch submissions to judge them in parallel
//   (instead of miku's behavior of fetching after finishing the running submission),
//   and this may result in inbalanced fetching.
// We can implement it by putting in use the currently-unused judge_servers table

// This function is blocking
bool FetchOneSubmission();

// These functions will push the request into queue and return immediately
void SendResult(const Submission&);
void SendFinalResult(const Submission&);
void SendStatus(int submission_id, const std::string&); // "Validating" or "queued"

// This function will call FetchOneSubmission() periodically, and also deal with
//   the requests sent from the above functions
void ServerWorkLoop();

#endif  // SERVER_IO_H_
