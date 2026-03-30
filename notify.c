// (c) 2026 shdown
// This code is licensed under MIT license (see LICENSE.MIT for details)

#include "notify.h"
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include "common.h"

bool notify_is_valid_utf8_str(const char *s)
{
    return g_utf8_validate(s, -1, NULL);
}

ATTR_NORETURN
static void die_with_gerror(const char *where, GError *err)
{
    fprintf(stderr, "%s: %s\n", where, err->message);
    exit(1);
}

static GDBusConnection *cnx;

void notify_global_init(void)
{
    assert(cnx == NULL);

    GError *err = NULL;
    cnx = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
    if (err) {
        die_with_gerror("g_dbus_get_sync", err);
    }
}

static GVariant *hint2gv(const NotifyHint *h)
{
    switch (h->type) {
    case NHT_BYTE:   return g_variant_new_byte(h->data.c);
    case NHT_BOOL:   return g_variant_new_boolean(h->data.c != '\0');
    case NHT_DOUBLE: return g_variant_new_double(h->data.d);
    case NHT_STR:    return g_variant_new_string(h->data.s);

    case NHT_INT16:  return g_variant_new_int16(h->data.i);
    case NHT_UINT16: return g_variant_new_uint16(h->data.i);

    case NHT_INT32:  return g_variant_new_int32(h->data.i);
    case NHT_UINT32: return g_variant_new_uint32(h->data.i);

    case NHT_INT64:  return g_variant_new_int64(h->data.i);
    case NHT_UINT64: return g_variant_new_uint64(h->data.i);
    }

    // Shoul be unreachable.
    abort();
}

uint32_t notify_send_notification(
        const NotifyParams *params, const NotifyHint *hints)
{
    assert(params != NULL);

    assert(cnx != NULL);

    GVariantBuilder *gv_hints = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

    if (hints) {
        for (const NotifyHint *h = hints; h->spelling; ++h) {
            GVariant *gv_val = hint2gv(h);
            g_variant_builder_add(gv_hints, "{sv}", h->spelling, gv_val);
        }
    }

    GError *err = NULL;
    GVariant *var_reply = g_dbus_connection_call_sync(
        cnx,
        "org.freedesktop.Notifications",
        "/org/freedesktop/Notifications",
        "org.freedesktop.Notifications",
        "Notify",
        g_variant_new(
            "(susssasa{sv}i)",
            params->appname,
            (guint32) params->replaces_id,
            params->icon,
            params->summary,
            params->body,
            (GVariantBuilder *) NULL,
            gv_hints,
            (gint32) params->timeout
        ),
        G_VARIANT_TYPE("(u)"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &err
    );

    g_variant_builder_unref(gv_hints);

    if (err) {
        die_with_gerror("g_dbus_connection_call_sync", err);
    }
    guint32 reply;
    g_variant_get(var_reply, "(u)", &reply);
    g_variant_unref(var_reply);
    return reply;
}

void notify_close_notification(uint32_t id)
{
    assert(cnx != NULL);

    GError *err = NULL;
    GVariant *var_reply = g_dbus_connection_call_sync(
        cnx,
        "org.freedesktop.Notifications",
        "/org/freedesktop/Notifications",
        "org.freedesktop.Notifications",
        "CloseNotification",
        g_variant_new("(u)", (guint32) id),
        G_VARIANT_TYPE("()"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &err
    );
    if (err) {
        die_with_gerror("g_dbus_connection_call_sync", err);
    }
    g_variant_unref(var_reply);
}

void notify_global_deinit(void)
{
    assert(cnx != NULL);

    (void) g_dbus_connection_close_sync(cnx, NULL, NULL);

    cnx = NULL;
}
