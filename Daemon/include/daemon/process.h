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

// Starts any process that is not running, has not given up, and whose
// backoff window has elapsed. Call this periodically (e.g. once per event
// loop tick) so restarts scheduled by pm_reap actually happen.
void pm_tick(process_manager_t *pm);

// Reaps every exited child (non-blocking waitpid loop). For each one,
// applies the restart policy: schedules a future restart via `next_start`
// (picked up by pm_tick), or marks the slot given_up after too many
// attempts within DAEMON_RESTART_WINDOW_SEC. Returns the number reaped.
int pm_reap(process_manager_t *pm);

managed_process_t *pm_find_by_pid(process_manager_t *pm, pid_t pid);

// Sends SIGTERM to every running child, waits up to timeout_ms for them to
// exit (reaping as they do), then SIGKILLs any stragglers.
void pm_shutdown_all(process_manager_t *pm, int timeout_ms);

#endif
