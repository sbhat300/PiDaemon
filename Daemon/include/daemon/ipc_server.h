#ifndef DAEMON_IPC_SERVER_H
#define DAEMON_IPC_SERVER_H

#include <poll.h>

#include "daemon/state.h"

typedef struct ipc_server ipc_server_t;

// Creates the listening Unix-domain socket at `socket_path`, unlinking any
// stale socket file left behind by an unclean shutdown. Caller must have
// already established (e.g. via a pidfile lock) that no other daemon
// instance is running, since removing a live peer's socket would break it.
ipc_server_t *ipc_server_create(const char *socket_path);
void ipc_server_destroy(ipc_server_t *srv);

// Fills `fds` (capacity `max_fds`) with every fd the server wants
// monitored this iteration - the listener plus one entry per connected
// client, POLLIN always and POLLOUT added when a client has queued output
// - and returns the count written. Call this immediately before poll(),
// and pass the same array back into ipc_server_service() once poll()
// returns, since indices must line up between the two calls.
int ipc_server_collect_pollfds(ipc_server_t *srv, struct pollfd *fds, int max_fds);

// Services whatever ipc_server_collect_pollfds() marked ready: accepts new
// connections, reads and dispatches complete messages (registration,
// state get/set against *state, and DMSG_SEND routing between clients),
// and flushes queued output. `fds`/`n` must be the exact array/count
// returned by the preceding ipc_server_collect_pollfds() call, now
// annotated with revents by poll().
void ipc_server_service(ipc_server_t *srv, struct pollfd *fds, int n, satellite_state_t *state);

#endif
