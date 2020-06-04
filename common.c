#include "common.h"

ssize_t full_read(int fd, char *buf, size_t nbuf)
{
    size_t nread = 0;
    while (nread != nbuf) {
        ssize_t r = read(fd, buf + nread, nbuf - nread);
        if (r < 0)
            return -1;
        if (r == 0)
            break;
        nread += r;
    }
    return nread;
}

void *xmalloc(size_t n)
{
    void *r = malloc(n);
    if (n && !r)
        panic("out of memory");
    return r;
}

char *xa_vsprintf(const char *fmt, va_list vl)
{
    va_list vl2;
    va_copy(vl2, vl);

    int n = vsnprintf(NULL, 0, fmt, vl);
    if (n < 0)
        goto fail;

    const size_t nr = ((size_t) n) + 1;
    char *r = xmalloc(nr);
    if (vsnprintf(r, nr, fmt, vl2) < 0)
        goto fail;

    va_end(vl2);
    return r;

fail:
    panic("vsnprintf() failed");
}

char *xa_sprintf(const char *fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    char *r = xa_vsprintf(fmt, vl);
    va_end(vl);
    return r;
}
