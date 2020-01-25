#include "notify.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <stdio.h>
#include <stdlib.h>

static GDBusConnection *cnx;

bool
notify_init(void)
{
    GError *err = NULL;
    cnx = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
    if (err) {
        fprintf(stderr, "g_bus_get_sync: %s\n", err->message);
        g_error_free(err);
        return false;
    }
    return true;
}

unsigned
notify(
    const char *appname,
    unsigned replaces_id,
    const char *icon,
    const char *summary,
    const char *body,
    unsigned char urgency,
    int timeout
) {
    GVariantBuilder *hints = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(hints, "{sv}", "urgency", g_variant_new_byte(urgency));

    GError *err = NULL;
    GVariant *var_reply = g_dbus_connection_call_sync(
        cnx,
        "org.freedesktop.Notifications",
        "/org/freedesktop/Notifications",
        "org.freedesktop.Notifications",
        "Notify",
        g_variant_new(
            "(susssasa{sv}i)",
            appname,
            (guint32) replaces_id,
            icon,
            summary,
            body,
            NULL,
            hints,
            (gint32) timeout
        ),
        G_VARIANT_TYPE("(u)"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &err
    );

    g_variant_builder_unref(hints);

    if (err) {
        fprintf(stderr, "g_dbus_connection_call_sync: %s\n", err->message);
        g_error_free(err);
        return 0;
    }
    guint32 reply;
    g_variant_get(var_reply, "(u)", &reply);
    g_variant_unref(var_reply);
    return reply;
}
