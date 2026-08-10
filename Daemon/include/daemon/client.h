#ifndef DAEMON_CLIENT_H
#define DAEMON_CLIENT_H

#include <stdint.h>

#include "daemon/state.h"

// Handle for a process's connection to the central daemon.
typedef struct {
    int fd;
} daemon_client_t;

// Connects to the daemon's IPC socket and registers under `name` (must be
// unique among currently-connected clients, truncated to
// DAEMON_NAME_MAX - 1 chars). Blocks until the daemon confirms
// registration. Returns 0 on success, -1 on failure (errno set; on a
// protocol-level rejection such as a duplicate name, errno is set to
// EEXIST).
int daemon_client_connect(const char *name, daemon_client_t *out);

// The underlying socket fd, for callers that want to multiplex it into
// their own poll()/select() loop instead of using the blocking helpers
// below.
int daemon_client_fd(const daemon_client_t *c);

// Convenience request/reply helpers. These block until the daemon replies
// and assume the reply to a request is the very next frame read off the
// wire. That assumption breaks if another client has DMSG_SEND-routed a
// DMSG_DELIVER to you concurrently - it would be read here and misparsed
// as the expected reply. Only use get_state/set_state on a connection that
// never appears as a DMSG_SEND destination (i.e. never calls
// daemon_client_recv itself). A process that both exchanges messages with
// peers and needs state should send DMSG_STATE_GET/SET itself via
// daemon_send() and read the reply back through its own daemon_client_recv
// loop, matching on message type.
int daemon_client_get_state(daemon_client_t *c, satellite_state_t *out);
int daemon_client_set_state(daemon_client_t *c, const satellite_state_t *state);

// Asks the daemon to relay `data` (len bytes) to the client registered as
// `dest`. This only queues the DMSG_SEND request; it does not wait for a
// reply, so a bad destination name is reported asynchronously as a
// DMSG_ERROR the next time the caller reads from the connection. Returns 0
// if the request was written successfully, -1 on I/O failure.
int daemon_client_send(daemon_client_t *c, const char *dest, const void *data, uint32_t len);

// Blocking read of the next frame from the daemon: a DMSG_DELIVER (with
// the sender's name copied into src_out, if non-NULL), a DMSG_STATE_VALUE,
// a DMSG_ERROR, a DMSG_PONG, etc. Returns 0 on success with *type/*out_len
// set and the payload copied into buf, -1 on I/O error, -2 if the payload
// was larger than bufsize.
int daemon_client_recv(daemon_client_t *c, uint16_t *type, char *src_out, void *buf, uint32_t bufsize,
                        uint32_t *out_len);

void daemon_client_close(daemon_client_t *c);

#endif
