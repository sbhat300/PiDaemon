#ifndef DAEMON_UTIL_H
#define DAEMON_UTIL_H

// Creates `dir` if it doesn't already exist. Paths are fixed compile-time
// constants (see config.h) one level below a directory that's always
// present on a standard Linux system (/var/lib, /run), so unlike a general
// `mkdir -p` this only ever needs to create a single level - no parent-walk
// required. Returns 0 on success (including if `dir` already existed), -1
// on failure (errno set).
int daemon_ensure_dir(const char *dir);

#endif
