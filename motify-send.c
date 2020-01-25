#include "notify.h"
#include "storage.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static
void
usage(void)
{
    fprintf(stderr, "USAGE: %s [options] appname summary body\n"
                    "    where appname can only contain ASCII letters, digits, underscores and \n"
                    "      dashes, can not be empty or start with a dash.\n"
                    "\n"
                    "OPTIONS:\n"
                    "    -n: show new notification instead of possibly re-using old one.\n"
                    "    -i icon: path to notification icon.\n"
                    "    -u urgency: either 'low' ('0'), 'normal' ('1'), or 'critical' ('2').\n"
                    "    -t timeout: timeout in milliseconds (0 for no timeout, -1 for default).\n"
                    ,
            PROGRAM_NAME);
    exit(EXIT_FAILURE);
}

static
bool
parse_urgency(unsigned char *urgency, const char *arg)
{
    if (strcmp(arg, "low") == 0 || strcmp(arg, "0") == 0) {
        *urgency = 0;
    } else if (strcmp(arg, "normal") == 0 || strcmp(arg, "1") == 0) {
        *urgency = 1;
    } else if (strcmp(arg, "critical") == 0 || strcmp(arg, "2") == 0) {
        *urgency = 2;
    } else {
        return false;
    }
    return true;
}

static
bool
parse_timeout(int *timeout, const char *arg)
{
    // who cares?
    return sscanf(arg, "%d", timeout) == 1;
}

static
bool
valid_appname(const char *appname)
{
    if (appname[0] == '\0' || appname[0] == '-') {
        return false;
    }
    for (size_t i = 0; appname[i]; ++i) {
        char c = appname[i];
        if (!(('a' <= c && c <= 'z') ||
              ('A' <= c && c <= 'Z') ||
              ('0' <= c && c <= '9') ||
              c == '_' ||
              c == '-'))
        {
            return false;
        }
    }
    return true;
}

int
main(int argc, char **argv)
{
    bool show_new = false;
    const char *icon_path = "";
    unsigned char urgency = 1;
    int timeout = -1;

    for (int c; (c = getopt(argc, argv, "ni:u:t:")) != -1;) {
        switch (c) {
        case 'n':
            show_new = true;
            break;
        case 'i':
            icon_path = optarg;
            break;
        case 'u':
            if (!parse_urgency(&urgency, optarg)) {
                fprintf(stderr, "Invalid -u argument: '%s'\n", optarg);
                usage();
            }
            break;
        case 't':
            if (!parse_timeout(&timeout, optarg)) {
                fprintf(stderr, "Invalid -t argument: '%s'\n", optarg);
                usage();
            }
            break;
        case '?':
            usage();
        }
    }

    if (argc - optind != 3) {
        usage();
    }
    const char *appname = argv[optind++];
    const char *summary = argv[optind++];
    const char *body    = argv[optind++];

    if (!valid_appname(appname)) {
        fprintf(stderr, "Invalid appname.\n");
        usage();
    }

    if (!notify_init()) {
        return EXIT_FAILURE;
    }

    int fd = storage_open(appname);
    if (fd < 0) {
        return EXIT_FAILURE;
    }
    if (!storage_lock(fd)) {
        return EXIT_FAILURE;
    }
    unsigned replaces_id = show_new ? 0 : storage_read(fd);
    unsigned id = notify(appname, replaces_id, icon_path, summary, body, urgency, timeout);
    if (id) {
        storage_write(fd, id);
    }

    return EXIT_SUCCESS;
}
