// (c) 2026 shdown
// This code is licensed under MIT license (see LICENSE.MIT for details)

#pragma once

#include <stdint.h>

int storage_open(const char *appname);

uint32_t storage_read(int fd);

void storage_write(int fd, uint32_t x);
