// (c) 2026 shdown
// This code is licensed under MIT license (see LICENSE.MIT for details)

#include "storage.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include "common.h"
#include "io_utils.h"

static const char *DIR_PREFIX = "/tmp/motify-send_";

ATTR_NORETURN
static void die_with_errno(const char *where)
{
    fprintf(stderr, "storage: %s: %s\n", where, strerror(errno));
    exit(1);
}

ATTR_NORETURN
static void die(const char *what)
{
    fprintf(stderr, "storage: %s\n", what);
    exit(1);
}

static inline int try_open(const char *d_path, const char *f_name)
{
    int dir_fd = open(d_path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_DIRECTORY);
    if (dir_fd < 0) {
        return -1;
    }

    int file_fd = openat(dir_fd, f_name, O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, (mode_t) 0600);

    int saved_errno = errno;
    close(dir_fd);
    errno = saved_errno;

    return file_fd;
}

static inline int try_mkdir(const char *path)
{
    return mkdir(path, 0700);
}

int storage_open(const char *appname)
{
    assert(appname != NULL);

    const char *login = getlogin();
    if (!login) {
        die_with_errno("login");
    }
    if ((strchr(login, '/'))) {
        die("login contains prohibited character");
    }

    char *d_path = xasprintf("%s%s", DIR_PREFIX, login);

    int fd = try_open(d_path, appname);
    if (fd >= 0) {
        goto ok;
    }

    // try_open() failed...
    if (errno != ENOENT) {
        // ...and the reason is not ENOENT
        die_with_errno("open");
    }

    // ...and the reason *is* ENOENT, so, let's try to create the directory.
    // If it fails with EEXIST, it's probably the other copy of ours has
    // managed to create it between our open() and mkdir() calls.
    if (try_mkdir(d_path) < 0 && errno != EEXIST) {
        die_with_errno("mkdir");
    }

    // now, let's try to open the file again.
    fd = try_open(d_path, appname);
    if (fd < 0) {
        die_with_errno("open");
    }

ok:
    free(d_path);
    return fd;
}

static inline void do_lock_or_die(int fd)
{
    if (flock(fd, LOCK_EX) < 0) {
        die_with_errno("flock (LOCK_EX)");
    }
}

static inline void do_unlock_or_die(int fd)
{
    if (flock(fd, LOCK_UN) < 0) {
        die_with_errno("flock (LOCK_UN)");
    }
}

static inline int rstrip_nl(const char *buf, int nbuf)
{
    if (nbuf && buf[nbuf - 1] == '\n') {
        return nbuf - 1;
    }
    return nbuf;
}

static inline uint32_t do_parse_u32_or_die(const char *s)
{
    errno = 0;
    char *endptr;
    unsigned long res = strtoul(s, &endptr, 10);
    if (errno || endptr == s || *endptr != '\0') {
        goto fail;
    }
#if ULONG_MAX > UINT32_MAX
    if (res > UINT32_MAX) {
        goto fail;
    }
#endif
    return res;
fail:
    die("cannot parse file content into uint32_t");
}

uint32_t storage_read(int fd)
{
    uint32_t res;

    // lock the file
    do_lock_or_die(fd);

    // read its content
    char buf[16];
    int nread = full_read(fd, buf, sizeof(buf));
    if (nread < 0) {
        die_with_errno("read");
    } else if (nread == 0) {
        res = 0;
        goto unlock;
    } else if (nread == (int) sizeof(buf)) {
        die("file is too large");
    }

    // strip a newline and zero-terminate the buffer
    int stripped_len = rstrip_nl(buf, nread);
    // stripped_len <= nread < sizeof(buf), so it's OK to do this
    buf[stripped_len] = '\0';

    // parse the content
    res = do_parse_u32_or_die(buf);

unlock:
    // unlock
    do_unlock_or_die(fd);
    return res;
}

void storage_write(int fd, uint32_t x)
{
    // prepare the data to write
    char data[16];
    snprintf(data, sizeof(data), "%" PRIu32 "\n", x);

    // lock the file
    do_lock_or_die(fd);

    // truncate the file
    if (ftruncate(fd, 0) < 0) {
        die_with_errno("ftruncate");
    }

    // seek to the end
    if (lseek(fd, 0, SEEK_SET) == (off_t) -1) {
        die_with_errno("lseek (SEEK_SET)");
    }

    // write the prepaed data
    if (full_write(fd, data, strlen(data)) < 0) {
        // die, but first try to truncate the file to zero size,
        // to "clear" partial data that we could have written.
        int saved_errno = errno;
        (void) ftruncate(fd, 0);
        errno = saved_errno;

        die_with_errno("write");
    }

    // unlock the file
    do_unlock_or_die(fd);
}
