#define _DEFAULT_SOURCE

#include <payload_state.h>

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#define SAT_STATE_MAGIC   0x53415453u /* "SATS" */
#define SAT_STATE_VERSION 2u

// On-disk record. 
// Detects a missing, foreign, stale-version, or torn file and
// falls back to safe defaults 
typedef struct {
    uint32_t           magic;
    uint32_t           version;
    satellite_state_t  data;
    uint32_t           checksum; 
} sat_state_record_t;

static int      g_lock_fd = -1;
static char     g_state_path[PATH_MAX];
static char     g_tmp_path[PATH_MAX];

static uint32_t fnv1a(const void *buf, size_t len) {
    const uint8_t *p = (const uint8_t *)buf;
    uint32_t hash = 0x811C9DC5u;
    for (size_t i = 0; i < len; i++) {
        hash ^= p[i];
        hash *= 0x01000193u;
    }
    return hash;
}

// Creates `dir` and any missing parent directories
static int mkdir_p(const char *dir) {
    char buf[PATH_MAX];
    strncpy(buf, dir, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    for (char *p = buf + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(buf, 0755) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }

    if (mkdir(buf, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

static void state_defaults(satellite_state_t *s) {
    memset(s, 0, sizeof(*s));
    s->op_state             = PAYLOAD_STATE_IDLE;
    s->active_experiment_id = 0;
    s->dce_authority_held   = 0;
    s->last_updated_met     = 0;
}

// Reads and validates the record file. Assumes caller already holds the lock.
static int read_record_locked(sat_state_record_t *rec) {
    int fd = open(g_state_path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    ssize_t total = 0;
    while ((size_t)total < sizeof(*rec)) {
        ssize_t n = read(fd, (uint8_t *)rec + total, sizeof(*rec) - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            close(fd);
            return -1;
        }
        if (n == 0) break; // short/empty file -> treated as corrupt below
        total += n;
    }
    close(fd);

    if ((size_t)total != sizeof(*rec)) {
        errno = EIO;
        return -1;
    }
    if (rec->magic != SAT_STATE_MAGIC || rec->version != SAT_STATE_VERSION) {
        errno = EINVAL;
        return -1;
    }
    if (rec->checksum != fnv1a(&rec->data, sizeof(rec->data))) {
        errno = EIO;
        return -1;
    }

    return 0;
}

// Atomically writes the record file (temp file + fsync + rename). Assumes
// caller already holds the lock.
static int write_record_locked(const sat_state_record_t *rec) {
    int fd = open(g_tmp_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        return -1;
    }

    ssize_t total = 0;
    while ((size_t)total < sizeof(*rec)) {
        ssize_t n = write(fd, (const uint8_t *)rec + total, sizeof(*rec) - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            close(fd);
            unlink(g_tmp_path);
            return -1;
        }
        total += n;
    }

    if (fsync(fd) != 0) {
        close(fd);
        unlink(g_tmp_path);
        return -1;
    }
    close(fd);

    if (rename(g_tmp_path, g_state_path) != 0) {
        unlink(g_tmp_path);
        return -1;
    }

    // fsync the containing directory so the rename itself survives a crash.
    char dir_buf[PATH_MAX];
    strncpy(dir_buf, g_state_path, sizeof(dir_buf) - 1);
    dir_buf[sizeof(dir_buf) - 1] = '\0';
    char *dir = dirname(dir_buf);

    int dir_fd = open(dir, O_RDONLY);
    if (dir_fd >= 0) {
        fsync(dir_fd);
        close(dir_fd);
    }

    return 0;
}

int sat_state_init(const char *path, satellite_state_t *out) {
    if (path == NULL || out == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (strlen(path) >= sizeof(g_state_path) - 5) { // room for ".tmp"/".lock"
        errno = ENAMETOOLONG;
        return -1;
    }
    strcpy(g_state_path, path);
    snprintf(g_tmp_path, sizeof(g_tmp_path), "%s.tmp", path);

    char dir_buf[PATH_MAX];
    strncpy(dir_buf, g_state_path, sizeof(dir_buf) - 1);
    dir_buf[sizeof(dir_buf) - 1] = '\0';
    if (mkdir_p(dirname(dir_buf)) != 0) {
        return -1;
    }

    char lock_path[PATH_MAX];
    snprintf(lock_path, sizeof(lock_path), "%s.lock", path);

    g_lock_fd = open(lock_path, O_CREAT | O_RDWR, 0644);
    if (g_lock_fd < 0) {
        return -1;
    }

    if (flock(g_lock_fd, LOCK_EX) != 0) {
        close(g_lock_fd);
        g_lock_fd = -1;
        return -1;
    }

    sat_state_record_t rec;
    int ok = (read_record_locked(&rec) == 0);

    if (!ok) {
        state_defaults(&rec.data);
        rec.magic    = SAT_STATE_MAGIC;
        rec.version  = SAT_STATE_VERSION;
        rec.checksum = fnv1a(&rec.data, sizeof(rec.data));

        if (write_record_locked(&rec) != 0) {
            flock(g_lock_fd, LOCK_UN);
            close(g_lock_fd);
            g_lock_fd = -1;
            return -1;
        }
    }

    flock(g_lock_fd, LOCK_UN);
    *out = rec.data;
    return 0;
}

int sat_state_load(satellite_state_t *out) {
    if (g_lock_fd < 0 || out == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (flock(g_lock_fd, LOCK_SH) != 0) {
        return -1;
    }

    sat_state_record_t rec;
    int ret = read_record_locked(&rec);

    flock(g_lock_fd, LOCK_UN);

    if (ret != 0) {
        return -1;
    }

    *out = rec.data;
    return 0;
}

int sat_state_store(const satellite_state_t *state) {
    if (g_lock_fd < 0 || state == NULL) {
        errno = EINVAL;
        return -1;
    }

    sat_state_record_t rec;
    rec.magic    = SAT_STATE_MAGIC;
    rec.version  = SAT_STATE_VERSION;
    rec.data     = *state;
    rec.checksum = fnv1a(&rec.data, sizeof(rec.data));

    if (flock(g_lock_fd, LOCK_EX) != 0) {
        return -1;
    }

    int ret = write_record_locked(&rec);

    flock(g_lock_fd, LOCK_UN);
    return ret;
}

void sat_state_close(void) {
    if (g_lock_fd >= 0) {
        close(g_lock_fd);
        g_lock_fd = -1;
    }
}