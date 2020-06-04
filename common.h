#ifndef common_h_
#define common_h_

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#define panic(Msg_) \
    do { \
        fprintf(stderr, "panic() at %s:%d: %s.\n", __FILE__, __LINE__, Msg_); \
        abort(); \
    } while (0)

ssize_t full_read(int fd, char *buf, size_t nbuf);

void *xmalloc(size_t n);

char *xa_vsprintf(const char *fmt, va_list vl);

char *xa_sprintf(const char *fmt, ...);

#endif
