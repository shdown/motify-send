// (c) 2026 shdown
// This code is licensed under MIT license (see LICENSE.MIT for details)

#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    NHT_BOOL,
    NHT_BYTE,
    NHT_INT16, NHT_UINT16,
    NHT_INT32, NHT_UINT32,
    NHT_INT64, NHT_UINT64,
    NHT_DOUBLE,
    NHT_STR,
} NotifyHintType;

typedef union {
    char c;
    uint64_t i;
    double d;
    char *s;
} NotifyHintData;

typedef struct {
    char *spelling;
    NotifyHintType type;
    NotifyHintData data;
} NotifyHint;

bool notify_is_valid_utf8_str(const char *s);

typedef struct {
    const char *appname;
    uint32_t replaces_id;
    const char *icon;
    const char *summary;
    const char *body;
    int32_t timeout;
} NotifyParams;

void notify_global_init(void);

uint32_t notify_send_notification(
        const NotifyParams *params,
        const NotifyHint *hints);

void notify_close_notification(uint32_t id);

void notify_global_deinit(void);
