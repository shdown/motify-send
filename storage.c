// (c) 2026 shdown
// This code is licensed under MIT license (see LICENSE.MIT for details)

#include "storage.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <inttypes.h>
#include "common.h"
#include "io_utils.h"

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

static char *alloc_dir_prefix(bool *out_may_be_unsafe)
{
    const char *base = getenv("XDG_RUNTIME_DIR");
    if (base && base[0]) {
        *out_may_be_unsafe = false;
    } else {
        base = "/tmp";
        *out_may_be_unsafe = true;
    }
    return xasprintf("%s/motify-send_", base);
}

static int try_open(const char *d_path, const char *f_name, bool d_may_be_unsafe)
{
    int file_fd = -1;
    int dir_fd = -1;

    dir_fd = open(d_path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_DIRECTORY);
    if (dir_fd < 0) {
        goto done;
    }

    if (d_may_be_unsafe) {
        struct stat d_info;
        if (fstat(dir_fd, &d_info) < 0) {
            die_with_errno("fstat");
        }
        if (d_info.st_uid != geteuid()) {
            die("directory is owned by someone else");
        }
        if ((d_info.st_mode & ~S_IFMT) != S_IRWXU) {
            die("directory has wrong mode (expected 0777)");
        }
    }

    file_fd = openat(dir_fd, f_name, O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, (mode_t) 0600);

done:
    if (dir_fd >= 0) {
        int saved_errno = errno;
        close(dir_fd);
        errno = saved_errno;
    }
    return file_fd;
}

static inline int try_mkdir(const char *path)
{
    return mkdir(path, 0700);
}

static const char *fetch_login(void)
{
    struct passwd *entry = getpwuid(geteuid());
    if (!entry) {
        die_with_errno("getpwuid");
    }
    if (!entry->pw_name) {
        die("result of getpwuid() has null pw_name (this should never happen)");
    }
    return entry->pw_name;
}

int storage_open(const char *appname)
{
    assert(appname != NULL);

    const char *login = fetch_login();
    if ((strchr(login, '/'))) {
        die("login contains prohibited character");
    }

    bool d_may_be_unsafe;
    char *d_prefix = alloc_dir_prefix(&d_may_be_unsafe);
    char *d_path = xasprintf("%s%s", d_prefix, login);

    int fd = try_open(d_path, appname, d_may_be_unsafe);
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
    fd = try_open(d_path, appname, d_may_be_unsafe);
    if (fd < 0) {
        die_with_errno("open");
    }

ok:
    free(d_path);
    free(d_prefix);
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

static inline uint32_t do_parse_u32(const char *s)
{
    errno = 0;
    char *endptr;
    uint64_t res = strtoull(s, &endptr, 10);
    if (errno || endptr == s || *endptr != '\0') {
        goto fail;
    }
    if (res > UINT32_MAX) {
        goto fail;
    }
    return res;

fail:
    return 0;
}

uint32_t storage_read(int fd)
{
    uint32_t res = 0;

    // lock the file
    do_lock_or_die(fd);

    // read its content
    char buf[16];
    int nread = full_read(fd, buf, sizeof(buf));
    if (nread < 0) {
        die_with_errno("read");
    } else if (nread == 0) {
        goto unlock;
    } else if (nread == (int) sizeof(buf)) {
        die("file is too large");
    }

    if (buf[nread - 1] != '\n') {
        goto unlock;
    }
    buf[nread - 1] = '\0';

    res = do_parse_u32(buf);

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

    // seek to the beginning
    if (lseek(fd, 0, SEEK_SET) == (off_t) -1) {
        die_with_errno("lseek (SEEK_SET)");
    }

    // write the prepared data
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
