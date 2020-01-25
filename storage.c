#include "storage.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

static const char *DIR_PREFIX = "/tmp/motify-send_";

static inline
void
copy_n_advance(char **dest, const char *src, size_t n)
{
    if (n) {
        memcpy(*dest, src, n);
    }
    *dest += n;
}

int
storage_open(const char *appname)
{
    int fd = -1;
    char *filename = NULL;

    const char *login = getlogin();
    if (!login) {
        fprintf(stderr, "getlogin: %s\n", strerror(errno));
        goto done;
    }

    const size_t nprefix = strlen(DIR_PREFIX);
    const size_t nlogin = strlen(login);
    const size_t nappname = strlen(appname);

    filename = malloc(nprefix + nlogin + 1 + nappname + 1);
    if (!filename) {
        fprintf(stderr, "Out of memory.\n");
        goto done;
    }

    char *ptr = filename;
    copy_n_advance(&ptr, DIR_PREFIX, nprefix);
    copy_n_advance(&ptr, login, nlogin);
    *ptr++ = '/';
    copy_n_advance(&ptr, appname, nappname + 1);

    for (int retry = 0; ; ++retry) {
        do {
            fd = open(filename, O_RDWR | O_CREAT, (mode_t) 0600);
        } while (fd < 0 && errno == EINTR);

        if (fd >= 0) {
            break;
        }

        if (!(retry == 0 && errno == ENOENT)) {
            fprintf(stderr, "open: %s: %s\n", filename, strerror(errno));
            goto done;
        }

        char *dirname_last = filename + nprefix + nlogin;
        assert(*dirname_last == '/');
        *dirname_last = '\0';
        // We ignore EEXIST error because it is likely has been created by another copy of this
        // program running in parallel.
        if (mkdir(filename, 0700) < 0 && errno != EEXIST) {
            fprintf(stderr, "mkdir: %s: %s\n", filename, strerror(errno));
            goto done;
        }
        *dirname_last = '/';
    }

done:
    free(filename);
    return fd;
}

bool
storage_lock(int fd)
{
    int r;
    do {
        r = flock(fd, LOCK_EX);
    } while (r < 0 && errno == EINTR);

    if (r < 0) {
        fprintf(stderr, "flock (LOCK_EX): %s\n", strerror(errno));
        return false;
    }
    return true;
}

static
ssize_t
full_read(int fd, char *buf, size_t nbuf)
{
    size_t nread = 0;
    while (nread != nbuf) {
        ssize_t r = read(fd, buf + nread, nbuf - nread);
        if (r < 0) {
            return -1;
        } else if (r == 0) {
            break;
        }
        nread += r;
    }
    return nread;
}

static
ssize_t
full_write(int fd, const char *buf, size_t nbuf)
{
    for (size_t nwritten = 0; nwritten != nbuf;) {
        ssize_t w = write(fd, buf + nwritten, nbuf - nwritten);
        if (w < 0) {
            return -1;
        }
        nwritten += w;
    }
    return nbuf;
}

unsigned
storage_read(int fd)
{
    char buf[16];
    ssize_t nread = full_read(fd, buf, sizeof(buf) - 1);
    if (nread < 0) {
        fprintf(stderr, "read: %s\n", strerror(errno));
        return 0;
    }
    buf[nread] = '\0';
    unsigned result;
    if (sscanf(buf, "%u", &result) != 1) {
        return 0;
    }
    return result;
}

bool
storage_write(int fd, unsigned value)
{
    int r;
    while ((r = ftruncate(fd, 0)) < 0 && errno == EINTR) {}
    if (r < 0) {
        fprintf(stderr, "ftruncate: %s\n", strerror(errno));
        return false;
    }

    if (lseek(fd, 0, SEEK_SET) == (off_t) -1) {
        fprintf(stderr, "lseek: %s\n", strerror(errno));
        return false;
    }

    char buf[16];
    if (full_write(fd, buf, snprintf(buf, sizeof(buf), "%u\n", value)) < 0) {
        fprintf(stderr, "write: %s\n", strerror(errno));
        return false;
    }
    return true;
}

bool
storage_unlock(int fd)
{
    if (flock(fd, LOCK_UN) < 0) {
        fprintf(stderr, "flock (LOCK_UN): %s\n", strerror(errno));
        return false;
    }
    return true;
}
