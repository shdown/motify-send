// (c) 2026 shdown
// This code is licensed under MIT license (see LICENSE.MIT for details)

#pragma once

#include <stddef.h>
#include <stdarg.h>

#if __GNUC__ >= 2
#   define ATTR_PRINTF(N_, M_) __attribute__((format(printf, N_, M_)))
#   define ATTR_NORETURN       __attribute__((noreturn))
#else
#   define ATTR_PRINTF(N_, M_) /*nothing*/
#   define ATTR_NORETURN       /*nothing*/
#endif

ATTR_NORETURN
void out_of_memory(void);

void *xmalloc(size_t n);

void *xrealloc(void *p, size_t n);

void *x2realloc(void *p, size_t *capacity, size_t elemsz);

char *xstrdup(const char *s);

ATTR_PRINTF(1, 2)
char *xasprintf(const char *fmt, ...);

ATTR_PRINTF(1, 0)
char *xasprintf_vl(const char *fmt, va_list vl);
