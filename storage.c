#include "storage.h"
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>

static const char *DIR_PREFIX = "/tmp/motify-send_";

int storage_open(const char *appname)
{
    int fd = -1;
    char *dpath = NULL;
    char *fpath = NULL;

    const char *login = getlogin();
    if (!login) {
        fprintf(stderr, "getlogin: %s\n", strerror(errno));
        goto done;
    }
    dpath = xa_sprintf("%s%s", DIR_PREFIX, login);
    fpath = xa_sprintf("%s/%s", dpath, appname);

    enum {
        OPEN_FLAGS = O_RDWR | O_CREAT | O_CLOEXEC,
        OPEN_MODE = 0600,
        MKDIR_MODE = 0700,
    };

    fd = open(fpath, OPEN_FLAGS, (mode_t) OPEN_MODE);

    if (fd < 0) {
        if (errno != ENOENT) {
            fprintf(stderr, "open: %s: %s", fpath, strerror(errno));
            goto done;
        }

        if (mkdir(dpath, MKDIR_MODE) < 0) {
            if (errno != EEXIST) {
                fprintf(stderr, "mkdir: %s: %s", dpath, strerror(errno));
                goto done;
            }
            // We ignore EEXIST error because it is likely has been created by another copy of this
            // program running in parallel.
        }

        fd = open(fpath, OPEN_FLAGS, (mode_t) OPEN_MODE);
        if (fd < 0) {
            fprintf(stderr, "open: %s: %s", fpath, strerror(errno));
            goto done;
        }
    }

done:
    free(dpath);
    free(fpath);
    return fd;
}

bool storage_lock(int fd)
{
    if (flock(fd, LOCK_EX) < 0) {
        fprintf(stderr, "flock (LOCK_EX): %s\n", strerror(errno));
        return false;
    }

    return true;
}

unsigned storage_read(int fd)
{
    char buf[16];
    ssize_t r = full_read(fd, buf, sizeof(buf) - 1);
    if (r < 0) {
        fprintf(stderr, "read: %s\n", strerror(errno));
        return 0;
    }
    buf[r] = '\0';
    unsigned val;
    if (sscanf(buf, "%u", &val) != 1) {
        return 0;
    }
    return val;
}

bool storage_write(int fd, unsigned value)
{
    if (ftruncate(fd, 0) < 0) {
        fprintf(stderr, "ftruncate: %s\n", strerror(errno));
        return false;
    }

    if (lseek(fd, 0, SEEK_SET) == (off_t) -1) {
        fprintf(stderr, "lseek: %s\n", strerror(errno));
        return false;
    }

    if (dprintf(fd, "%u\n", value) < 0) {
        fprintf(stderr, "dprintf: %s\n", strerror(errno));
        return false;
    }

    return true;
}

bool storage_unlock(int fd)
{
    if (flock(fd, LOCK_UN) < 0) {
        fprintf(stderr, "flock (LOCK_UN): %s\n", strerror(errno));
        return false;
    }

    return true;
}
