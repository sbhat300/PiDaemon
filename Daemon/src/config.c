#include "daemon/config.h"

#include <stdlib.h>

const char *daemon_state_path(void) {
    return DAEMON_STATE_PATH_DEFAULT;
}

const char *daemon_socket_path(void) {
    return DAEMON_SOCKET_PATH_DEFAULT;
}

const char *daemon_pid_path(void) {
    return DAEMON_PID_PATH_DEFAULT;
}
