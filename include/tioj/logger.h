#ifndef INCLUDE_LOGGER_H_
#define INCLUDE_LOGGER_H_

// Call this before any call to judge submissions. Otherwise, the logger may deadlock.
void InitLogger();

#endif  // INCLUDE_LOGGER_H_
