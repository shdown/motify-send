// (c) 2026 shdown
// This code is licensed under MIT license (see LICENSE.MIT for details)

#include "io_utils.h"
#include <unistd.h>

int full_read(int fd, char *buf, int nbuf)
{
    int nread = 0;
    while (nread != nbuf) {
        int r = read(fd, buf + nread, nbuf - nread);
        if (r < 0) {
            return -1;
        } else if (r == 0) {
            break;
        }
        nread += r;
    }
    return nread;
}

int full_write(int fd, const char *buf, int nbuf)
{
    int nwritten = 0;
    while (nwritten != nbuf) {
        int w = write(fd, buf + nwritten, nbuf - nwritten);
        if (w < 0) {
            return -1;
        }
        nwritten += w;
    }
    return 0;
}
