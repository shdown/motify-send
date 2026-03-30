// (c) 2026 shdown
// This code is licensed under MIT license (see LICENSE.MIT for details)

#include "hint_list.h"
#include "notify.h"
#include "common.h"
#include <stdlib.h>

void hint_list_add(HintList *x, NotifyHint h)
{
    if (x->size == x->capacity) {
        x->data = x2realloc(x->data, &x->capacity, sizeof(NotifyHint));
    }
    x->data[x->size++] = h;
}

NotifyHint *hint_list_finalize(HintList *x)
{
    hint_list_add(x, (NotifyHint) {0});
    return x->data;
}

static void destroy_item(NotifyHint h)
{
    if (!h.spelling) {
        // Don't mess with h.data of the sentinel entry
        return;
    }
    free(h.spelling);
    if (h.type == NHT_STR) {
        free(h.data.s);
    }
}

void hint_list_destroy(HintList *x)
{
    for (size_t i = 0; i < x->size; ++i) {
        destroy_item(x->data[i]);
    }
    free(x->data);
}
