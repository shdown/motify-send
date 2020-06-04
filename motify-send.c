#include "common.h"
#include "notify.h"
#include "storage.h"
#include <unistd.h>

static
void
print_usage_and_exit(void)
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
    exit(2);
}

static
bool
parse_urgency(const char *arg, unsigned char *out)
{
    if (strcmp(arg, "low") == 0 || strcmp(arg, "0") == 0) {
        *out = 0;
    } else if (strcmp(arg, "normal") == 0 || strcmp(arg, "1") == 0) {
        *out = 1;
    } else if (strcmp(arg, "critical") == 0 || strcmp(arg, "2") == 0) {
        *out = 2;
    } else {
        return false;
    }
    return true;
}

static
bool
parse_timeout(const char *arg, int *out)
{
    return sscanf(arg, "%d", out) == 1;
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
            if (!parse_urgency(optarg, &urgency)) {
                fprintf(stderr, "Invalid -u argument: '%s'\n", optarg);
                print_usage_and_exit();
            }
            break;
        case 't':
            if (!parse_timeout(optarg, &timeout)) {
                fprintf(stderr, "Invalid -t argument: '%s'\n", optarg);
                print_usage_and_exit();
            }
            break;
        case '?':
            print_usage_and_exit();
        }
    }

    if (argc - optind != 3) {
        fprintf(stderr, "Expected exactly 3 positional arguments.\n");
        print_usage_and_exit();
    }
    const char *appname = argv[optind++];
    const char *summary = argv[optind++];
    const char *body    = argv[optind++];

    if (!valid_appname(appname)) {
        fprintf(stderr, "Invalid appname.\n");
        print_usage_and_exit();
    }

    if (!notify_init()) {
        goto fail;
    }

    int fd = storage_open(appname);
    if (fd < 0) {
        goto fail;
    }

    if (!storage_lock(fd)) {
        goto fail;
    }

    unsigned replaces_id = show_new ? 0 : storage_read(fd);
    unsigned id = notify(appname, replaces_id, icon_path, summary, body, urgency, timeout);
    if (id) {
        if (!storage_write(fd, id)) {
            goto fail;
        }
    }

    return 0;

fail:
    return 1;
}
