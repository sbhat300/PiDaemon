#ifndef DAEMON_PROCESS_H
#define DAEMON_PROCESS_H

#include <sys/types.h>
#include <time.h>

#include "daemon/config.h"

typedef enum {
    RESTART_ALWAYS,      // always relaunch, however it exited
    RESTART_ON_FAILURE,  // relaunch only on nonzero exit / termination by signal
    RESTART_NEVER
} restart_policy_t;

#define PROCESS_MAX_ARGS 8

typedef struct {
    char             name[DAEMON_NAME_MAX]; // supervisor-facing identifier, for logging
    char             path[256];             // path passed to execv
    char            *argv[PROCESS_MAX_ARGS + 1]; // argv[0] is overwritten with `path` by pm_add
    restart_policy_t restart;
} process_spec_t;

typedef struct {
    process_spec_t spec;
    pid_t          pid;           // 0 if not currently running
    int            attempts;      // restart attempts within the current window
    time_t         window_start;
    time_t         next_start;    // earliest time() a restart may occur (backoff)
    int            given_up;      // exceeded DAEMON_RESTART_MAX_ATTEMPTS
    int            shutting_down; // SIGTERM sent by pm_shutdown_one, awaiting exit
    time_t         kill_deadline; // pm_tick SIGKILLs if still running after this time
    int            kill_sent;     // SIGKILL already issued, don't repeat/re-log
} managed_process_t;

typedef struct {
    managed_process_t procs[DAEMON_MAX_PROCESSES];
    int                count;
} process_manager_t;

void pm_init(process_manager_t *pm);

// Registers a process to be supervised. Does not start it. Returns 0 on
// success, -1 if the table is full.
int pm_add(process_manager_t *pm, const process_spec_t *spec);

// fork()s + exec()s every registered process that isn't already running.
// Returns the number successfully started.
int pm_start_all(process_manager_t *pm);

// fork()s + exec()s the registered process with the given name, resetting
// its restart backoff state as pm_start_all does. Returns 0 on success, -1
// if no process with that name is registered, it is already running, or
// start failed.
int pm_start_one(process_manager_t *pm, const char *name);

// Starts any process that is not running, has not given up, and whose
// backoff window has elapsed. Also SIGKILLs any process pm_shutdown_one
// marked shutting_down whose kill_deadline has passed. Call this
// periodically (e.g. once per event loop tick) so restarts scheduled by
// pm_reap, and forced kills scheduled by pm_shutdown_one, actually happen.
void pm_tick(process_manager_t *pm);

// Reaps every exited child (non-blocking waitpid loop). For each one,
// applies the restart policy: schedules a future restart via `next_start`
// (picked up by pm_tick), or marks the slot given_up after too many
// attempts within DAEMON_RESTART_WINDOW_SEC. Returns the number reaped.
int pm_reap(process_manager_t *pm);

managed_process_t *pm_find_by_pid(process_manager_t *pm, pid_t pid);

// Sends SIGTERM to every running child, waits up to timeout_ms for them to
// exit (reaping as they do), then SIGKILLs any stragglers. Marks every
// process given_up so pm_tick won't auto-restart them afterward.
void pm_shutdown_all(process_manager_t *pm, int timeout_ms);

// Sends SIGTERM to the registered process with the given name and returns
// immediately (does not block). Marks it given_up so pm_tick won't
// auto-restart it, and shutting_down so pm_tick will SIGKILL it if it's
// still running after timeout_ms — the actual exit is observed later via
// the normal pm_reap path. Call pm_start_one to bring it back afterward.
// Returns 0 if the process was found (whether or not it was running), -1 if
// no process with that name is registered.
int pm_shutdown_one(process_manager_t *pm, const char *name, int timeout_ms);

#endif
