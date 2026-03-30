// (c) 2026 shdown
// This code is licensed under MIT license (see LICENSE.MIT for details)

#pragma once

#include "notify.h"
#include <stddef.h>

typedef struct {
    NotifyHint *data;
    size_t size;
    size_t capacity;
} HintList;

#define HINT_LIST_STATIC_INIT {0}

#define hint_list_size(List_) ((List_)->size)

void hint_list_add(HintList *x, NotifyHint h);

NotifyHint *hint_list_finalize(HintList *x);

void hint_list_destroy(HintList *x);
