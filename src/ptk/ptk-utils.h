/*
*  C Interface: ptkutils
*
* Description: Some GUI utilities
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

/*
  I don't like GtkUIManager provided by gtk+, so I implement my own. ;-)
*/

#ifndef _PTK_UTILS_H_
#define _PTK_UTILS_H_

#include <gtk/gtk.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

#define PTK_STOCK_MENU_ITEM( id, cb ) { id, NULL, G_CALLBACK(cb), 0, 0, NULL, NULL }
#define PTK_MENU_ITEM( label, cb, key, mod ) { label, NULL, G_CALLBACK(cb), key, mod, NULL, NULL }
#define PTK_CHECK_MENU_ITEM( label, cb, key, mod ) { label, (char*)1, G_CALLBACK(cb), key, mod, NULL, NULL }
#define PTK_RADIO_MENU_ITEM( label, cb, key, mod ) { label, (char*)2, G_CALLBACK(cb), key, mod, NULL, NULL }
#define PTK_IMG_MENU_ITEM( label, icon, cb, key, mod ) { label, icon, G_CALLBACK(cb), key, mod, NULL, NULL }
#define PTK_POPUP_MENU( label, sub ) { label, NULL, NULL, 0, 0, sub, NULL }
#define PTK_POPUP_IMG_MENU( label, icon, sub ) { label, icon, NULL, 0, 0, sub, NULL }
#define PTK_SEPARATOR_MENU_ITEM { NULL, (char *)(-1), NULL, 0, 0, NULL, 0}
#define PTK_MENU_END  {0}
#define PTK_IS_STOCK_ITEM( ent )  ( ent->label && (*(guint32*)ent->label) == *(guint32*)"gtk-" )
#define PTK_IS_CHECK_MENU_ITEM( ent )  ( ent->stock_icon == (char*)1 )
#define PTK_IS_RADIO_MENU_ITEM( ent )  ( ent->stock_icon == (char*)2 )

struct _PtkMenuItemEntry
{
  const char* label; /* or stock id */
  const char* stock_icon; /* or menu type  1: check, 2: radio */
  GCallback callback;
  guint key;
  GdkModifierType mod;
  struct _PtkMenuItemEntry* sub_menu;
  GtkWidget** ret;
  GtkWidget* menu;  //MOD
};
typedef struct _PtkMenuItemEntry PtkMenuItemEntry;

#define PTK_STOCK_TOOL_ITEM( id, cb ) { id, NULL, NULL, G_CALLBACK(cb), NULL, NULL }
#define PTK_TOOL_ITEM( label, icon, tooltip, cb ) { label, icon, tooltip, G_CALLBACK(cb), NULL, NULL }
#define PTK_CHECK_TOOL_ITEM( label, icon, tooltip, cb ) { label, icon, tooltip, G_CALLBACK(cb), (PtkMenuItemEntry*)1, NULL }
#define PTK_RADIO_TOOL_ITEM( label, icon, tooltip, cb ) { label, icon, tooltip, G_CALLBACK(cb), (PtkMenuItemEntry*)2, NULL }
#define PTK_EMPTY_MENU  ((gpointer)3)
#define PTK_MENU_TOOL_ITEM( label, icon, tooltip, cb, menu ) { label, icon, tooltip, cb, menu, NULL }
#define PTK_SEPARATOR_TOOL_ITEM { NULL, NULL, NULL, -1, NULL, NULL}
#define PTK_TOOL_END  {0}
#define PTK_IS_CHECK_TOOL_ITEM( ent )  ( ent->menu == (PtkMenuItemEntry*)1 )
#define PTK_IS_RADIO_TOOL_ITEM( ent )  ( ent->menu == (PtkMenuItemEntry*)2 )
#define PTK_IS_SEPARATOR_TOOL_ITEM( ent )  ( ent->callback == G_CALLBACK(-1) )

struct _PtkToolItemEntry
{
  const char* label; /* or stock id */
  const char* stock_icon; /* or menu type  1: check, 2: radio */
  const char* tooltip;
  GCallback callback;
  struct _PtkMenuItemEntry* menu; /* NULL: normal, 1: check, 2: radio, 3: empty menu, > 3: menu */
  GtkWidget** ret;
};
typedef struct _PtkToolItemEntry PtkToolItemEntry;

GtkWidget* ptk_menu_new_from_data( PtkMenuItemEntry* entries,
                                   gpointer cb_data,
                                   GtkAccelGroup* accel_group );

void ptk_menu_add_items_from_data( GtkWidget* menu,
                                   PtkMenuItemEntry* entries,
                                   gpointer cb_data,
                                   GtkAccelGroup* accel_group );

GtkWidget* ptk_toolbar_add_items_from_data( GtkWidget* toolbar,
                                            PtkToolItemEntry* entries,
                                            gpointer cb_data );

/* The string 'message' can contain pango markups.
  * If special characters like < and > are used in the string,
  * they should be escaped with g_markup_escape_text().
  */
void ptk_show_error(GtkWindow* parent, const char* title, const char* message );

/* Because GNOME HIG causes some usability problems under limited screen size,
 * this API is provided to adjust the dialogs, and try to fit them into
 * small screens via totally breaking GNOME HIG and compress spacings.
 */
void ptk_dialog_fit_small_screen( GtkDialog* dlg );

/* gtk_dialog_run will disable all parent windows of the dialog.
 * However, sometimes we need modaless dialogs. So here it is.
 */
int ptk_dialog_run_modaless( GtkDialog* dlg );

GtkBuilder* _gtk_builder_new_from_file( const char* file, GError** err );

G_END_DECLS

#endif

