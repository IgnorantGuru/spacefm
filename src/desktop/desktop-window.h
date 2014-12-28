/*
* SpaceFM desktop-window.h
*
* Copyright (C) 2014 IgnorantGuru <ignorantguru@gmx.com>
* Copyright (C) 2012 BwackNinja <bwackninja@gmail.com>
* Copyright (C) 2008 Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>
*
* License: See COPYING file
*
*/

#ifndef __DESKTOP_WINDOW_H__
#define __DESKTOP_WINDOW_H__

#include <gtk/gtk.h>
#include <sys/types.h>  /* for dev_t */
#include <sys/stat.h>
#include <X11/Xlib.h>

#include "vfs-dir.h"
#include "vfs-file-task.h"

G_BEGIN_DECLS

#define DESKTOP_WINDOW_TYPE             (desktop_window_get_type())
#define DESKTOP_WINDOW(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),\
        DESKTOP_WINDOW_TYPE, DesktopWindow))
#define DESKTOP_WINDOW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),\
        DESKTOP_WINDOW_TYPE, DesktopWindowClass))
#define IS_DESKTOP_WINDOW(obj)              (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
        DESKTOP_WINDOW_TYPE))
#define IS_DESKTOP_WINDOW_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE((klass),\
        DESKTOP_WINDOW_TYPE))

typedef struct _DesktopWindow               DesktopWindow;
typedef struct _DesktopWindowClass          DesktopWindowClass;

typedef struct _DesktopItem     DesktopItem;

typedef enum {
    DW_SORT_BY_NAME,
    DW_SORT_BY_SIZE,
    DW_SORT_BY_TYPE,
    DW_SORT_BY_MTIME,
    DW_SORT_CUSTOM
}DWSortType;

typedef enum {
    DW_BG_COLOR,
    DW_BG_TILE,
    DW_BG_FULL,
    DW_BG_STRETCH,
    DW_BG_CENTER,
    DW_BG_ZOOM,
}DWBgType;

typedef enum {
    DW_SELECT_ALL,
    DW_SELECT_NONE,
    DW_SELECT_INVERSE,
    DW_SELECT_PATTERN
}DWSelectMode;

struct _DesktopWindow
{
    GtkWindow parent;

    /* all items on the desktop window */
    GList* items;

    /* margins of the whole desktop window */
    int margin_top;
    int margin_left;
    int margin_right;
    int margin_bottom;

    /* padding oudside the bounding box of the whole item */
    int x_pad;
    int y_pad;

    /* space between icon and text label */
    int spacing;

    /* size of icons */
    int icon_size;

    /* width of label */
    int label_w;

    /* size of the items */
    int item_w;

    /* focused item */
    DesktopItem* focus;

    /* sort types */
    DWSortType sort_by : 4;
    GtkSortType sort_type : 2;
    gboolean dir_first : 1;
    gboolean file_first : 1;
    gboolean show_thumbnails : 1;
    gboolean single_click : 1;

    /* <private> */

    gboolean button_pressed : 1;
    gboolean rubber_bending : 1;
    gboolean dragging : 1;
    gboolean drag_entered : 1;
    gboolean pending_drop_action : 1;
    dev_t drag_src_dev;

    /* single click */
    guint single_click_timeout_handler;
    GdkCursor* hand_cursor;
    DesktopItem* hover_item;

    gint drag_start_x;  /* for drag & drop */
    gint drag_start_y;
    gint drag_pending_x;
    gint drag_pending_y;
    guint rubber_bending_x;
    guint rubber_bending_y;

    /* the directory content */
    VFSDir* dir;

    /* renderers for the items */
    PangoLayout* pl;

    GtkCellRenderer* icon_render;

    /* background image */
#if GTK_CHECK_VERSION (3, 0, 0)
    Pixmap background;
    cairo_surface_t *surface;
#else
    GdkPixmap* background;
#endif
    DWBgType bg_type;

    GdkColor fg;
    GdkColor bg;
    GdkColor shadow;

    GdkRectangle wa;    /* working area */
    
    guint screen_index;     // screen index of this desktop window
    guint box_count;        // number of boxes visible on the desktop
    guint row_count;        // number of rows visible on the desktop
    guint order_rows;       // number of rows from saved desktop layout
    gpointer insert_item;   // item at which to insert dragged files
    gpointer renaming_item; // item is being renamed
    gpointer renamed_item;  // item was renamed
    gboolean file_listed;   // TRUE once files are read
};

struct _DesktopWindowClass
{
    GtkWindowClass parent_class;
};

GType       desktop_window_get_type (void);
GtkWidget* desktop_window_new          (void);

/*
 *  Set background of the desktop window.
 *  src_pix is the source pixbuf in original size (no scaling)
 *  This function will stretch or add border to this pixbuf accordiong to 'type'.
 *  If type = DW_BG_COLOR and src_pix = NULL, the background color is used to fill the window.
 */
void desktop_window_set_background( DesktopWindow* win, GdkPixbuf* src_pix, DWBgType type );
void desktop_window_set_bg_color( DesktopWindow* win, GdkColor* clr );
void desktop_window_set_text_color( DesktopWindow* win, GdkColor* clr, GdkColor* shadow );

void desktop_window_set_icon_size( DesktopWindow* win, int size );
void desktop_window_set_show_thumbnails( DesktopWindow* win, gboolean show );

void desktop_window_set_single_click( DesktopWindow* win, gboolean single_click );

void desktop_window_reload_icons( DesktopWindow* win );

void desktop_window_sort_items( DesktopWindow* win, DWSortType sort_by, GtkSortType sort_type );

GList* desktop_window_get_selected_files( DesktopWindow* win );

gboolean desktop_write_exports( VFSFileTask* vtask, const char* value, FILE* file );
void desktop_context_fill( DesktopWindow* win, gpointer context );
void desktop_window_rename_selected_files( DesktopWindow* win,
                                                GList* files, const char* cwd );
void desktop_window_on_autoopen_cb( gpointer task, gpointer aop );
void desktop_window_select( DesktopWindow* self, DWSelectMode mode );
void desktop_window_copycmd( DesktopWindow* desktop, GList* sel_files,
                                                char* cwd, char* setname );
void desktop_window_add_application( DesktopWindow* desktop );
void desktop_window_insert_task_complete( VFSFileTask* task, DesktopWindow* self );
void desktop_window_set_insert_item( DesktopWindow* self );
void desktop_window_open_desktop_dir( GtkMenuItem *menuitem,
                                                    DesktopWindow* desktop );


G_END_DECLS

#endif /* __DESKTOP_WINDOW_H__ */

