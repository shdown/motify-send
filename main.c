// (c) 2026 shdown
// This code is licensed under MIT license (see LICENSE.MIT for details)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include "storage.h"
#include "notify.h"
#include "common.h"
#include "hint_list.h"
#include "hint_parser.h"

static HintList hint_list = HINT_LIST_STATIC_INIT;

static bool parse_urgency(const char *s, unsigned char *out)
{
    if (strcmp(s, "0") == 0 || strcmp(s, "low") == 0) {
        *out = 0;
        return true;
    }
    if (strcmp(s, "1") == 0 || strcmp(s, "normal") == 0) {
        *out = 1;
        return true;
    }
    if (strcmp(s, "2") == 0 || strcmp(s, "critical") == 0) {
        *out = 2;
        return true;
    }
    return false;
}

static bool parse_timeout(const char *s, int32_t *out)
{
    errno = 0;
    char *endptr;
    long res = strtol(s, &endptr, 10);
    if (errno || endptr == s || *endptr != '\0') {
        return false;
    }
#if LONG_MAX > INT32_MAX
    if (res < INT32_MIN || res > INT32_MAX) {
        return false;
    }
#endif
    *out = res;
    return true;
}

static bool is_valid_appname(const char *appname)
{
    if (appname[0] == '\0' || appname[0] == '-') {
        return false;
    }
    for (size_t i = 0; appname[i]; ++i) {
        char c = appname[i];
        if ('a' <= c && c <= 'z') {
            continue;
        }
        if ('A' <= c && c <= 'Z') {
            continue;
        }
        if ('0' <= c && c <= '9') {
            continue;
        }
        if (c == '_' || c == '-') {
            continue;
        }
        return false;
    }
    return true;
}

static void add_hint(const char *s)
{
    size_t hint_idx = hint_list_size(&hint_list);
    char descr[20 + 24];
    snprintf(descr, sizeof(descr), "hint with index %zu", hint_idx);

    NotifyHint hint;
    parse_hint(s, &hint, descr);
    hint_list_add(&hint_list, hint);
}

static void add_urgency_hint(unsigned char urgency)
{
    NotifyHint hint = {
        .spelling = xstrdup("urgency"),
        .type = NHT_BYTE,
        .data = {.c = urgency},
    };
    hint_list_add(&hint_list, hint);
}

static void print_usage_and_exit(const char *extra_msg)
{
    if (extra_msg) {
        fprintf(stderr, "%s\n", extra_msg);
    }
    fputs(
        "USAGE:\n"
        "    1. Create or replace notification:\n"
        "        motify-send [OPTION...] APPNAME SUMMARY BODY\n"
        "    2. Close existing notification:\n"
        "        motify-send -c APPNAME\n"
        "    3. Show usage information (this message):\n"
        "        motify-send -h\n"
        "\n"
        "Supported OPTIONs (all of them only make sense in the usage #1 above):\n"
        "    -i PATH: set path to the icon file.\n"
        "    -u URGENCY: set urgency; URGENCY is either '0'/'low', '1'/'normal' or '2'/'critical'.\n"
        "    -t SECONDS: set notification timeout in seconds; negative value means no timeout.\n"
        "    -n: force creation of a new notification; never replace.\n"
        "    -H KEY=TYPE:VALUE: add a hint.\n"
        "\n"
        "Supported hint types:\n"
        " * bool: either 'true'/'false' or '0'/'1'\n"
        " * byte: either of:\n"
        "     - empty string: NUL byte\n"
        "     - single character: byte with this value\n"
        "     - #XX, where X is a hex digit: hex notation (e.g. #ff)\n"
        " * double: parsed from a string representation of floating-point value\n"
        " * str: as-is\n"
        " * i16, u16, i32, u32, i64, u64: parsed from a string representation of integer value\n"
        ""
        , stderr);
    exit(2);
}

typedef struct {
    const char *icon_path;
    unsigned char urgency;
    int32_t timeout;
    bool n_flag;
    bool c_flag;

    const char *appname;
    const char *summary;
    const char *body;
} Args;

static void die_bad_utf8(const char *what)
{
    fprintf(stderr, "FATAL: %s contains invalid UTF-8.\n", what);
    exit(1);
}

static void validate_utf8(Args *args)
{
    if (args->icon_path && !notify_is_valid_utf8_str(args->icon_path)) {
        die_bad_utf8("icon path (-i argument)");
    }
    if (args->appname && !notify_is_valid_utf8_str(args->appname)) {
        die_bad_utf8("appname");
    }
    if (args->summary && !notify_is_valid_utf8_str(args->summary)) {
        die_bad_utf8("summary");
    }
    if (args->body && !notify_is_valid_utf8_str(args->body)) {
        die_bad_utf8("body");
    }
}

static void run(Args *args)
{
    notify_global_init();

    int fd = storage_open(args->appname);

    uint32_t replaces_id;
    if (args->n_flag) {
        replaces_id = 0;
    } else {
        replaces_id = storage_read(fd);
    }

    if (args->c_flag) {
        if (replaces_id) {
            notify_close_notification(replaces_id);
            storage_write(fd, 0);
        } else {
            fprintf(stderr, "There is no notification to close.\n");
        }
    } else {
        NotifyParams params = {
            .appname = args->appname,
            .replaces_id = replaces_id,
            .icon = args->icon_path,
            .summary = args->summary,
            .body = args->body,
            .timeout = args->timeout,
        };

        add_urgency_hint(args->urgency);

        NotifyHint *hints = hint_list_finalize(&hint_list);
        uint32_t new_id = notify_send_notification(&params, hints);
        storage_write(fd, new_id);
    }

    close(fd);
    hint_list_destroy(&hint_list);
    notify_global_deinit();
}

int main(int argc, char **argv)
{
    Args args = {
        .icon_path = "",
        .urgency = 1,
        .timeout = -1,
        .n_flag = false,
        .c_flag = false,
        .appname = NULL,
        .summary = NULL,
        .body = NULL,
    };

    for (int c; (c = getopt(argc, argv, "i:u:t:H:nch")) != -1;) {
        switch (c) {
        case 'i':
            args.icon_path = optarg;
            break;
        case 'u':
            if (!parse_urgency(optarg, &args.urgency)) {
                fprintf(stderr, "Cannot parse urgency (-u argument).\n");
                exit(2);
            }
            break;
        case 't':
            if (!parse_timeout(optarg, &args.timeout)) {
                fprintf(stderr, "Cannot parse timeout (-t argument).\n");
                exit(2);
            }
            break;
        case 'H':
            add_hint(optarg);
            break;
        case 'n':
            args.n_flag = true;
            break;
        case 'c':
            args.c_flag = true;
            break;
        case '?':
        case 'h':
            print_usage_and_exit(NULL);
            break;
        }
    }

    if (args.c_flag) {
        if (argc - optind != 1) {
            print_usage_and_exit("Expected exactly one positional argument.");
        }
        args.appname = argv[optind++];
    } else {
        if (argc - optind != 3) {
            print_usage_and_exit("Expected exactly 3 positional arguments.");
        }
        args.appname = argv[optind++];
        args.summary = argv[optind++];
        args.body    = argv[optind++];
    }

    if (!is_valid_appname(args.appname)) {
        print_usage_and_exit("Invalid appname.");
    }

    validate_utf8(&args);

    run(&args);

    return 0;
}
