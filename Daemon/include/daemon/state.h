#ifndef DAEMON_STATE_H
#define DAEMON_STATE_H

#include <stdint.h>

typedef enum {
    SAT_OP_IDLE       = 0x00,
    SAT_OP_RUNNING    = 0x01,
    SAT_OP_PROCESSING = 0x02,
    SAT_OP_FAULT      = 0xFF
} sat_op_state_t;

// Canonical satellite state, owned exclusively by the daemon. Other
// processes never touch the backing file directly - they go through the
// daemon's IPC interface (DMSG_STATE_GET / DMSG_STATE_SET in proto.h).
// Extend this struct as new fields are needed and bump SAT_STATE_VERSION in
// state.c whenever the layout changes, so a file written by an old build is
// reinitialized instead of misread.
typedef struct {
    sat_op_state_t op_state;             // current payload operating mode
    uint8_t        active_experiment_id; // 0 if idle
    uint8_t        dce_authority_held;   // 1 if the payload currently holds DCE authority
    uint64_t       last_updated_met;     // MET (ms) as of the last sat_state_store()
} satellite_state_t;

// Opens (creating if needed) the state file at `path` and a companion lock
// file next to it, then loads the current state into *out. If the state
// file is missing or fails validation (e.g. a torn write from a power
// loss), *out is reset to safe defaults and persisted immediately. Must be
// called once, from the daemon process only, before sat_state_load/store.
// Returns 0 on success, -1 on failure (errno set).
int sat_state_init(const char *path, satellite_state_t *out);

// Reads the current state under a shared lock. Returns 0 on success, -1 on
// failure. The daemon keeps its own in-memory copy up to date as it
// services requests, so this is mainly useful for out-of-band inspection.
int sat_state_load(satellite_state_t *out);

// Atomically persists *state under an exclusive lock. Returns 0 on success,
// -1 on failure (errno set).
int sat_state_store(const satellite_state_t *state);

// Releases the lock fd. Does not delete the state file.
void sat_state_close(void);

#endif
