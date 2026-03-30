// (c) 2026 shdown
// This code is licensed under MIT license (see LICENSE.MIT for details)

#include "common.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

void out_of_memory(void)
{
    fputs("Out of memory.\n", stderr);
    abort();
}

static inline size_t mul_or_oom(size_t a, size_t b)
{
    if (b && a > SIZE_MAX / b) {
        out_of_memory();
    }
    return a * b;
}

void *xmalloc(size_t n)
{
    void *r = malloc(n);
    if (n && !r) {
        out_of_memory();
    }
    return r;
}

void *xrealloc(void *p, size_t n)
{
    if (!n) {
        free(p);
        return NULL;
    }
    void *r = realloc(p, n);
    if (!r) {
        out_of_memory();
    }
    return r;
}

void *x2realloc(void *p, size_t *capacity, size_t elemsz)
{
    if (*capacity) {
        *capacity = mul_or_oom(*capacity, 2);
    } else {
        *capacity = 1;
    }
    return xrealloc(p, mul_or_oom(*capacity, elemsz));
}

char *xstrdup(const char *s)
{
    assert(s != NULL);

    size_t ns = strlen(s);
    char *r = xmalloc(ns + 1);
    memcpy(r, s, ns + 1);
    return r;
}

char *xasprintf_vl(const char *fmt, va_list vl)
{
    va_list vl2;
    va_copy(vl2, vl);

    int n = vsnprintf(NULL, 0, fmt, vl);
    if (n < 0) {
        goto fail;
    }

    const size_t nr = ((size_t) n) + 1;
    char *r = xmalloc(nr);
    if (vsnprintf(r, nr, fmt, vl2) < 0) {
        goto fail;
    }

    va_end(vl2);
    return r;

fail:
    fputs("FATAL: xasprintf_vl: vsnprintf() failed.\n", stderr);
    abort();
}

char *xasprintf(const char *fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    char *r = xasprintf_vl(fmt, vl);
    va_end(vl);
    return r;
}
