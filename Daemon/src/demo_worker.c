// Example managed process. Spawned twice by daemond (as "worker_a" and
// "worker_b") to exercise every piece of the daemon: process supervision,
// the locked state store, and message routing between two unrelated
// processes via the daemon as intermediary.
//
// usage: demo_worker <name> [peer_name]
//
// If a peer name is given, the worker kicks off a ping/pong: it sends a
// counter to the peer, the peer bumps it and sends it back, forever. Any
// worker also responds to whoever last messaged it, so only one side needs
// to know the peer's name up front.

#include "daemon/client.h"
#include "daemon/proto.h"

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void sleep_ms(int ms) {
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000 };
    nanosleep(&ts, NULL);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <name> [peer_name]\n", argv[0]);
        return 1;
    }
    const char *name = argv[1];
    const char *peer = argc > 2 ? argv[2] : NULL;

    daemon_client_t c;
    int connected = 0;
    for (int attempt = 0; attempt < 10 && !connected; attempt++) {
        if (daemon_client_connect(name, &c) == 0) {
            connected = 1;
            break;
        }
        sleep_ms(300);
    }
    if (!connected) {
        fprintf(stderr, "[%s] could not connect to daemon: %s\n", name, strerror(errno));
        return 1;
    }
    fprintf(stderr, "[%s] connected and registered\n", name);

    // Exercise state get/set once, up front, before any peer traffic can
    // arrive - see the caveat on daemon_client_get_state/set_state in
    // client.h about why that ordering matters.
    satellite_state_t state;
    if (daemon_client_get_state(&c, &state) == 0) {
        fprintf(stderr, "[%s] state: op_state=0x%02X active_experiment_id=%u\n", name, state.op_state,
                state.active_experiment_id);
        if (peer != NULL) {
            // Arbitrarily, whichever worker initiates the ping/pong also
            // claims the experiment slot, to show a write taking effect.
            state.op_state             = SAT_OP_RUNNING;
            state.active_experiment_id = 7;
            if (daemon_client_set_state(&c, &state) == 0) {
                fprintf(stderr, "[%s] updated state: op_state=0x%02X active_experiment_id=%u\n", name,
                        state.op_state, state.active_experiment_id);
            }
        }
    }

    if (peer != NULL) {
        uint32_t counter = 0;
        for (int attempt = 0; attempt < 10; attempt++) {
            if (daemon_client_send(&c, peer, &counter, sizeof(counter)) == 0) {
                fprintf(stderr, "[%s] -> %s: %u\n", name, peer, counter);
                break;
            }
            sleep_ms(300);
        }
    }

    struct pollfd pfd = { .fd = daemon_client_fd(&c), .events = POLLIN };
    for (;;) {
        int ret = poll(&pfd, 1, -1);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (!(pfd.revents & POLLIN)) {
            continue;
        }

        uint16_t type;
        char src[DAEMON_NAME_MAX];
        uint8_t buf[DAEMON_MAX_MSG_PAYLOAD];
        uint32_t len;
        if (daemon_client_recv(&c, &type, src, buf, sizeof(buf), &len) != 0) {
            fprintf(stderr, "[%s] connection to daemon lost\n", name);
            break;
        }

        if (type == DMSG_DELIVER && len == sizeof(uint32_t)) {
            uint32_t counter;
            memcpy(&counter, buf, sizeof(counter));
            fprintf(stderr, "[%s] <- %s: %u\n", name, src, counter);

            sleep_ms(500);
            counter++;
            daemon_client_send(&c, src, &counter, sizeof(counter));
        } else if (type == DMSG_ERROR) {
            fprintf(stderr, "[%s] daemon reported an error\n", name);
        }
    }

    daemon_client_close(&c);
    return 0;
}
