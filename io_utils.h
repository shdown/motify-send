// (c) 2026 shdown
// This code is licensed under MIT license (see LICENSE.MIT for details)

#pragma once

// Note that 'nbuf' has type 'int', not 'size_t'.
// This makes for simpler code for this project.
// Do not copy without consideration.

int full_read(int fd, char *buf, int nbuf);

int full_write(int fd, const char *buf, int nbuf);
