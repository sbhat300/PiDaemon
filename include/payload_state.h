#ifndef PAYLOAD_STATE_H
#define PAYLOAD_STATE_H

#include <stdint.h>

typedef enum {
    PAYLOAD_STATE_IDLE       = 0x00,
    PAYLOAD_STATE_RUNNING    = 0x01,
    PAYLOAD_STATE_PROCESSING = 0x02,
    PAYLOAD_STATE_FAULT      = 0xFF
} payload_op_state_t;

// Persisted payload state. Extend this struct as new fields are
// needed and change SAT_STATE_VERSION in payload_state.c whenever the layout
// changes, so a file written by an old build is reinitialized instead of
// misread.
typedef struct {
    payload_op_state_t  op_state;             // payload_op_state_t
    uint8_t             active_experiment_id; // matches OpCode 0x20 argument, 0 if idle
    uint8_t             dce_authority_held;   // 1 if payOBC currently holds DCE authority (ICD 4.6)
    uint64_t            last_updated_met;     // MET (ms) as of the last sat_state_store()
} satellite_state_t;

// Opens (creating if needed) the state file at `path` and a companion lock
// file next to it, then loads the current state into *out. If the state
// file is missing or fails validation (e.g. torn write from a power loss),
// *out is reset to safe defaults (idle, no experiment, no DCE authority) and
// persisted immediately. Must be called once before sat_state_load/store.
// Returns 0 on success, -1 on failure (errno set).
int sat_state_init(const char *path, satellite_state_t *out);

// Reads the current state under a shared lock. Safe to call concurrently
// with other readers; blocks only while a sat_state_store() processing.
// Returns 0 on success, -1 on failure.
int sat_state_load(satellite_state_t *out);

// Atomically persists *state under an exclusive lock:
// Returns 0 on success, -1 on failure (errno set).
int sat_state_store(const satellite_state_t *state);

// Releases the lock fd. Does not delete the state file.
void sat_state_close(void);

#endif
