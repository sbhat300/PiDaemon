#ifndef DAEMON_IPC_H
#define DAEMON_IPC_H

#include <stddef.h>
#include <stdint.h>

// Blocking, EINTR-safe full read/write. Return 0 on success, -1 on error
// (errno set) or -2 if the peer closed the connection before `len` bytes
// were transferred.
int daemon_write_all(int fd, const void *buf, size_t len);
int daemon_read_all(int fd, void *buf, size_t len);

// Frames `payload` (len bytes, may be NULL/0) behind a daemon_hdr_t and
// blocking-writes it to fd. Returns 0 on success, -1 on error.
int daemon_send(int fd, uint16_t type, const void *payload, uint32_t len);

// Blocking-reads one framed message from fd into buf (bufsize capacity).
// On success returns 0 and sets *type and *out_len. Returns -1 on I/O
// error/EOF (errno set), or -2 if the message body is larger than bufsize
// (the connection should be treated as desynchronized and closed).
int daemon_recv(int fd, uint16_t *type, void *buf, uint32_t bufsize, uint32_t *out_len);

#endif
