#ifndef DAEMON_PROTO_H
#define DAEMON_PROTO_H

// Wire protocol for the daemon's Unix-domain-socket IPC bus. This is local
// IPC only (a single machine, a single build): payloads use host byte
// order and native struct layout. Do not reuse this framing for anything
// that crosses a network or talks between different architectures/builds.

#include <stdint.h>

#include "daemon/config.h"
#include "daemon/state.h"

typedef enum {
    DMSG_REGISTER      = 1, // client -> daemon: daemon_register_body_t, must be the first message
    DMSG_REGISTER_OK   = 2, // daemon -> client: empty payload
    DMSG_STATE_GET     = 3, // client -> daemon: empty payload
    DMSG_STATE_VALUE   = 4, // daemon -> client: satellite_state_t (reply to GET or SET)
    DMSG_STATE_SET     = 5, // client -> daemon: satellite_state_t (full replace)
    DMSG_SEND          = 6, // client -> daemon: daemon_send_body_t + raw bytes
    DMSG_DELIVER       = 7, // daemon -> client: daemon_deliver_body_t + raw bytes
    DMSG_ERROR         = 8, // daemon -> client: daemon_error_body_t
    DMSG_PING          = 9, // client -> daemon: empty payload
    DMSG_PONG          = 10 // daemon -> client: empty payload
} daemon_msg_type_t;

// Precedes every message on the wire.
typedef struct {
    uint32_t length;   // bytes of payload following this header
    uint16_t type;     // daemon_msg_type_t
    uint16_t reserved;
} daemon_hdr_t;

typedef struct {
    char name[DAEMON_NAME_MAX]; // not necessarily NUL-terminated if it fills the array
} daemon_register_body_t;

typedef struct {
    char dest[DAEMON_NAME_MAX];
    // raw application bytes follow, up to
    // DAEMON_MAX_MSG_PAYLOAD - sizeof(daemon_send_body_t)
} daemon_send_body_t;

typedef struct {
    char src[DAEMON_NAME_MAX];
    // raw application bytes follow
} daemon_deliver_body_t;

typedef enum {
    DAEMON_ERR_BAD_MESSAGE    = 1, // malformed frame or message not valid in current state
    DAEMON_ERR_NOT_REGISTERED = 2, // first message on a connection was not DMSG_REGISTER
    DAEMON_ERR_NAME_TAKEN     = 3, // DMSG_REGISTER name collides with an already-connected client
    DAEMON_ERR_UNKNOWN_DEST   = 4  // DMSG_SEND targeted a name with no registered client
} daemon_err_t;

typedef struct {
    uint32_t code; // daemon_err_t
} daemon_error_body_t;

#endif
