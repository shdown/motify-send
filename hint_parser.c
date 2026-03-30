// (c) 2026 shdown
// This code is licensed under MIT license (see LICENSE.MIT for details)

#include "hint_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include "common.h"
#include "notify.h"

#define die_fmt(arg_descr, ...) \
    do { \
        fprintf(stderr, "Cannot parse %s: ", (arg_descr)); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        exit(2); \
    } while (0)

static char *fptr_error;

#define clear_fptr_error() (free(fptr_error), fptr_error = NULL)

#define set_fptr_error(...) (free(fptr_error), fptr_error = xasprintf(__VA_ARGS__))

// So, we have a catalogue of known D-Bus types, along with
// functions to parse a string into that type.
//
// Also, all functions take a "spec" argument. It is only
// used for functions that parse intger types; it signifies
// the signedness and bit width of an integer type.
// E.g. -16 means int16_t, 16 means uint16_t.

typedef struct {
    const char *name;
    NotifyHintType type;
    int (*f)(NotifyHintData *dst, const char *s, int spec);
    int f_spec;
} KnownType;

static int do_bool(NotifyHintData *dst, const char *s, int spec)
{
    (void) spec;

    if (strcmp(s, "1") == 0 || strcmp(s, "true") == 0) {
        *dst = (NotifyHintData) {.c = 1};
        return 0;

    } else if (strcmp(s, "0") == 0 || strcmp(s, "false") == 0) {
        *dst = (NotifyHintData) {.c = 0};
        return 0;

    } else {
        set_fptr_error("expected either of: '0'/'1'/'true'/'false'");
        return -1;
    }
}

static inline int decode_hex_digit(char c)
{
    if ('0' <= c && c <= '9') {
        return c - '0';
    }
    if ('a' <= c && c <= 'f') {
        return 10 + (c - 'a');
    }
    if ('A' <= c && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

static int do_byte(NotifyHintData *dst, const char *s, int spec)
{
    (void) spec;

    if (s[0] == '\0' || s[1] == '\0') {
        *dst = (NotifyHintData) {.c = s[0]};
        return 0;

    } else if (s[0] == '#') {
        if (strlen(s) != 3) {
            set_fptr_error("invalid #XX escape (wrong length)");
        }
        int hi = decode_hex_digit(s[1]);
        int lo = decode_hex_digit(s[2]);
        if (hi < 0 || lo < 0) {
            set_fptr_error("invalid #XX escape (bad hex digit)");
            return -1;
        }
        uint8_t res = (hi << 4) | lo;
        *dst = (NotifyHintData) {.c = res};
        return 0;

    } else {
        set_fptr_error("expected either of: empty string/single byte/#XX escape");
        return -1;
    }
}

static int do_str(NotifyHintData *dst, const char *s, int spec)
{
    (void) spec;

    if (!notify_is_valid_utf8_str(s)) {
        set_fptr_error("string contains invalid UTF-8");
        return -1;
    }

    *dst = (NotifyHintData) {.s = xstrdup(s)};
    return 0;
}

static int do_double(NotifyHintData *dst, const char *s, int spec)
{
    (void) spec;

    char *endptr;
    errno = 0;
    double d = strtod(s, &endptr);

    if (errno != 0 || s[0] == '\0' || *endptr != '\0') {
        set_fptr_error("cannot parse into double");
        return -1;
    }

    *dst = (NotifyHintData) {.d = d};
    return 0;
}

static inline int64_t get_int_min(int spec)
{
    switch (spec) {
    case -16: return INT16_MIN;
    case -32: return INT32_MIN;
    case -64: return INT64_MIN;
    }
    // Must be unreachable.
    abort();
}

static inline int64_t get_int_max(int spec)
{
    switch (spec) {
    case -16: return INT16_MAX;
    case -32: return INT32_MAX;
    case -64: return INT64_MAX;
    }
    // Must be unreachable.
    abort();
}

static inline uint64_t get_uint_max(int spec)
{
    switch (spec) {
    case 16: return UINT16_MAX;
    case 32: return UINT32_MAX;
    case 64: return UINT64_MAX;
    }
    // Must be unreachable.
    abort();
}

static int do_int(NotifyHintData *dst, const char *s, int spec)
{
    uint64_t ures;
    char *endptr;
    errno = 0;

    if (spec < 0) {
        ures = strtoll(s, &endptr, 10);
    } else {
        ures = strtoull(s, &endptr, 10);
    }

    if (errno != 0 || s[0] == '\0' || *endptr != '\0') {
        set_fptr_error("cannot parse into integer");
        return -1;
    }

    if (spec < 0) {
        int64_t ires = ures;
        if (ires < get_int_min(spec) || ires > get_int_max(spec)) {
            goto overflow;
        }
    } else {
        if (ures > get_uint_max(spec)) {
            goto overflow;
        }
    }

    dst->i = ures;
    return 0;

overflow:
    set_fptr_error("overflow or underflow");
    return -1;
}

static const KnownType KNOWN_TYPES[] = {
    {"bool",    NHT_BOOL,   do_bool, 0},
    {"byte",    NHT_BYTE,   do_byte, 0},
    {"double",  NHT_DOUBLE, do_double, 0},
    {"str",     NHT_STR,    do_str, 0},

    {"i16",     NHT_INT16,  do_int, -16},
    {"u16",     NHT_UINT16, do_int, 16},

    {"i32",     NHT_INT32,  do_int, -32},
    {"u32",     NHT_UINT32, do_int, 32},

    {"i64",     NHT_INT64,  do_int, -64},
    {"u64",     NHT_UINT64, do_int, 64},

    {0},
};

static const KnownType *find_known_type(const char *type_name)
{
    for (const KnownType *kt = KNOWN_TYPES; kt->name; ++kt) {
        if (strcmp(kt->name, type_name) == 0) {
            return kt;
        }
    }
    return NULL;
}

// Finally, we get to the function that parses the whole thing.
// But first a small helper function.

static inline char *next(char **pos, char delim)
{
    char *found = strchr(*pos, delim);
    if (!found) {
        return NULL;
    }
    *found = '\0';
    char *old = *pos;
    *pos = found + 1;
    return old;
}

void parse_hint(const char *arg, NotifyHint *out, const char *arg_descr)
{
    assert(arg != NULL);
    assert(out != NULL);
    assert(arg_descr != NULL);

    char *s = xstrdup(arg);

    char *pos = s;
    char *spelling = next(&pos, '=');
    if (!spelling) {
        die_fmt(arg_descr, "no '=' found");
    }
    char *type_name = next(&pos, ':');
    if (!type_name) {
        die_fmt(arg_descr, "no ':' found in value part");
    }
    char *data = next(&pos, '\0');
    assert(data != NULL);

    const KnownType *kt = find_known_type(type_name);
    if (!kt) {
        die_fmt(arg_descr, "unknown type '%s'", type_name);
    }

    *out = (NotifyHint) {0};

    if (kt->f(&out->data, data, kt->f_spec) < 0) {
        assert(fptr_error != NULL);
        die_fmt(arg_descr, "error parsing into %s: %s", type_name, fptr_error);
    }
    out->spelling = xstrdup(spelling);
    out->type = kt->type;

    clear_fptr_error();

    free(s);
}
