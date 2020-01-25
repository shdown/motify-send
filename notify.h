#ifndef notify_h_
#define notify_h_

#include <stdbool.h>

bool
notify_init(void);

unsigned
notify(
    const char *appname,
    unsigned replaces_id,
    const char *icon,
    const char *summary,
    const char *body,
    unsigned char urgency,
    int timeout
);

#endif
