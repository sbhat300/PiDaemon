#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <fcntl.h>
#include <arpa/inet.h> 

#include <spicenet/config.h>
#include <spicenet/snp.h>

// Entries for bulk transfer
struct bulk_calc_entry {
    uint64_t mission_time;
    float    angular_accel[3];
    float    commanded_torque[4];
};

struct bulk_ft_entry {
    uint64_t mission_time;
    float    force_vector[3];
    float    torque_vector[3];
};

// APID Definitions 
// OBC -> Pi
#define APID_CMD_RX            0x010
#define APID_ADCS_TELEM_RX     0x020
#define APID_ADCS_RESP_RX      0x022
#define APID_BULK_EXP_RX       0x033
#define APID_BULK_CTRL_RX      0x034

// Pi -> OBC
#define APID_TELEM_TX          0x011
#define APID_CMD_RESP_TX       0x012
#define APID_ADCS_REQ_TX       0x021
#define APID_BULK_CALC_TX      0x030
#define APID_BULK_FT_TX        0x031
#define APID_BULK_VID_TX       0x032

// Pi <-> OBC
#define APID_BULK_ACK_BIDI     0x035

// APIDs to poll/listen on
const int NUM_LISTEN_APIDS = 6;
int listen_apids[NUM_LISTEN_APIDS] = {
    APID_CMD_RX, 
    APID_ADCS_TELEM_RX, 
    APID_ADCS_RESP_RX, 
    APID_BULK_EXP_RX, 
    APID_BULK_CTRL_RX, 
    APID_BULK_ACK_BIDI
};

// Input handler functions
// All multi-byte fields arrive in Big-Endian, handle this here also
// APID 0x010
void handle_payload_command(uint8_t *buf, int len) {
}

// APID 0x020
void handle_adcs_telemetry(uint8_t *buf, int len) {
}

// APID 0x022
void handle_adcs_response(uint8_t *buf, int len) {
}

// APID 0x033
void handle_bulk_experiment_table(uint8_t *buf, int len) {
}

// APID 0x034
void handle_bulk_control_algo(uint8_t *buf, int len) {
}

// APID 0x035
void handle_bulk_ack(uint8_t *buf, int len) {
}

// Output handler functions
// These are non blocking so they can snp_write can directly be called here

// APID 0x011 
int send_health_telemetry(uint64_t met, uint8_t state, uint8_t exp_id, float temp, uint32_t storage) {

}

// APID 0x012 
int send_command_response(uint8_t opcode, uint8_t status) {

}

// APID 0x021 
int send_adcs_request(uint8_t request_type, float target_quat[4], float target_rates[3], uint32_t duration) {

}

// APID 0x030 
int send_bulk_calculation_data(uint32_t transfer_id, struct bulk_calc_entry *entries, uint32_t num_entries) {

}

// APID 0x031 
int send_bulk_force_torque_data(uint32_t transfer_id, struct bulk_ft_entry *entries, uint32_t num_entries) {

}

// APID 0x032
int send_bulk_video_data(uint32_t transfer_id, uint8_t *video_buffer, uint32_t total_bytes) {

}

// APID 0x035
int send_bulk_ack(uint32_t transfer_id, uint8_t status, uint32_t *missing_chunks, uint32_t num_missing_chunks) {

}

int main(int argc, char **argv) {
    int fd;
    const char *portname = "/dev/ttyS0"; 
    uint8_t rx_buffer[512]; // Max payload MTU 512 bytes

    if (snp_open(&fd, (char *)portname) != 0) {
        perror("[Failed to open serial connection]");
        return EXIT_FAILURE;
    }
    printf("[Opened Serial Connection] %s\n", portname);

    if (snp_listen(fd) != 0) {
        printf("[Serial Connection Invalid]\n");
        return EXIT_FAILURE;
    }
    printf("[Serial Connection Confirmed]\n");

    // Connect to all listening APIDs
    snp_app_t *apps[NUM_LISTEN_APIDS];
    struct pollfd pollfds[NUM_LISTEN_APIDS];

    for (int i = 0; i < NUM_LISTEN_APIDS; i++) {
        if (snp_connect(listen_apids[i], &apps[i]) != 0) {
            fprintf(stderr, "[Failed to connect to apid %d]\n", listen_apids[i]);
            return EXIT_FAILURE;
        }

        printf("[Connected to apid] %d\n", listen_apids[i]);
        pollfds[i].fd = apps[i]->read[0]; 
        pollfds[i].events = POLLIN;
    }

    // Event Loop
    while (1) {
        int ret = poll(pollfds, NUM_LISTEN_APIDS, -1); 

        if (ret > 0) {
            for (int i = 0; i < NUM_LISTEN_APIDS; i++) {
                if (pollfds[i].revents & POLLIN) {
                    
                    int bytes_read = snp_read(apps[i], rx_buffer, sizeof(rx_buffer));
                    
                    if (bytes_read > 0) {
                        // Route to appropriate handler based on APID
                        switch(apps[i]->apid) {
                            case APID_CMD_RX:
                                handle_payload_command(rx_buffer, bytes_read);
                                break;
                            case APID_ADCS_TELEM_RX:
                                handle_adcs_telemetry(rx_buffer, bytes_read);
                                break;
                            case APID_ADCS_RESP_RX:
                                handle_adcs_response(rx_buffer, bytes_read);
                                break;
                            case APID_BULK_EXP_RX:
                                handle_bulk_experiment_table(rx_buffer, bytes_read);
                                break;
                            case APID_BULK_CTRL_RX:
                                handle_bulk_control_algo(rx_buffer, bytes_read);
                                break;
                            case APID_BULK_ACK_BIDI:
                                handle_bulk_ack(rx_buffer, bytes_read);
                                break;
                            default:
                                break;
                        }
                    }
                }
            }
        }
    }

    return EXIT_SUCCESS;
}