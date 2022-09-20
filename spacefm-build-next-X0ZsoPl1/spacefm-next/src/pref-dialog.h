#ifndef _PREF_DLG_H_
#define _PREF_DLG_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum {
    PREF_GENERAL,
    PREF_INTERFACE,
    PREF_DESKTOP,
    PREF_VOLMAN,
    PREF_ADVANCED
}PrefDlgPage;

gboolean fm_edit_preference( GtkWindow* parent, int page );

G_END_DECLS

#endif

