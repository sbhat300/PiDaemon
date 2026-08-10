#include "daemon/util.h"

#include <errno.h>
#include <sys/stat.h>

int daemon_ensure_dir(const char *dir) {
    if (mkdir(dir, 0755) == 0 || errno == EEXIST) {
        return 0;
    }
    return -1;
}
