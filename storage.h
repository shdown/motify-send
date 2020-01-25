#ifndef store_h_
#define store_h_

#include <stdbool.h>

int
storage_open(const char *appname);

bool
storage_lock(int fd);

unsigned
storage_read(int fd);

bool
storage_write(int fd, unsigned value);

bool
storage_unlock(int fd);

#endif
