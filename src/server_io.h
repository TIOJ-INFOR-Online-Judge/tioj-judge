#ifndef SERVER_IO_H_
#define SERVER_IO_H_

#include <string>
#include "tioj/submission.h"

extern std::string kTIOJUrl;
extern std::string kTIOJKey;
extern size_t kMaxQueue;

// Note that we also need to add some work balancing on webserver in case of multiple clients,
//   because now they will try to greedily fetch submissions to judge them in parallel
//   (instead of miku's behavior of fetching after finishing the running submission),
//   and this may result in inbalanced fetching.

// Send submission query to websocket
void TryFetchSubmission();

// These functions will push the request into queue and return immediately
void SendResult(const Submission&);
void SendFinalResult(const Submission&);
void SendStatus(int submission_id, const std::string&); // "Validating" or "queued"
void SendQueuedSubmissions(bool send_if_empty);

// This function will initialize websocket connection and deal with all server interactions.
// It will not return.
void ServerWorkLoop();

#endif  // SERVER_IO_H_
