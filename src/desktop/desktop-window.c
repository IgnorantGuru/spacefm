/*
 *      desktop-window.c
 *
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#include <glib/gi18n.h>

#include "desktop-window.h"
#include "vfs-file-info.h"
#include "vfs-mime-type.h"
#include "vfs-thumbnail-loader.h"
#include "vfs-app-desktop.h"

#include "glib-mem.h"
#include "working-area.h"

#include "ptk-file-misc.h"
#include "ptk-file-menu.h"
#include "ptk-file-task.h"
#include "ptk-utils.h"

#include "settings.h"
#include "main-window.h"
#include "pref-dialog.h"
#include "ptk-file-browser.h"
#include "ptk-clipboard.h"
#include "ptk-file-archiver.h"
#include "ptk-location-view.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>

#include <string.h>

/* for stat */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

struct _DesktopItem
{
    VFSFileInfo* fi;
    guint order;
    GdkRectangle box;   /* bounding rect */
    GdkRectangle icon_rect;
    GdkRectangle text_rect;

    /* Since pango doesn't support using "wrap" and "ellipsize" at the same time,
         let's  do some dirty hack here.  We draw these two lines separately, and
         we can ellipsize the second line. */
    int len1;   /* length for the first line of label text */
    int line_h1;    /* height of the first line */

    gboolean is_selected : 1;
    gboolean is_prelight : 1;
};

static void desktop_window_class_init           (DesktopWindowClass *klass);
static void desktop_window_init             (DesktopWindow *self);
static void desktop_window_finalize         (GObject *object);

static gboolean on_expose( GtkWidget* w, GdkEventExpose* evt );
static void on_size_allocate( GtkWidget* w, GtkAllocation* alloc );
static void on_size_request( GtkWidget* w, GtkRequisition* req );
static gboolean on_button_press( GtkWidget* w, GdkEventButton* evt );
static gboolean on_button_release( GtkWidget* w, GdkEventButton* evt );
static gboolean on_mouse_move( GtkWidget* w, GdkEventMotion* evt );
static gboolean on_key_press( GtkWidget* w, GdkEventKey* evt );
static void on_style_set( GtkWidget* w, GtkStyle* prev );
static void on_realize( GtkWidget* w );
static gboolean on_focus_in( GtkWidget* w, GdkEventFocus* evt );
static gboolean on_focus_out( GtkWidget* w, GdkEventFocus* evt );
/* static gboolean on_scroll( GtkWidget *w, GdkEventScroll *evt, gpointer user_data ); */

static void on_drag_begin( GtkWidget* w, GdkDragContext* ctx );
static gboolean on_drag_motion( GtkWidget* w, GdkDragContext* ctx, gint x, gint y, guint time );
static gboolean on_drag_drop( GtkWidget* w, GdkDragContext* ctx, gint x, gint y, guint time );
static void on_drag_data_get( GtkWidget* w, GdkDragContext* ctx, GtkSelectionData* data, guint info, guint time );
static void on_drag_data_received( GtkWidget* w, GdkDragContext* ctx, gint x, gint y, GtkSelectionData* data, guint info, guint time );
static void on_drag_leave( GtkWidget* w, GdkDragContext* ctx, guint time );
static void on_drag_end( GtkWidget* w, GdkDragContext* ctx );

static void on_file_listed( VFSDir* dir,  gboolean is_cancelled, DesktopWindow* self );
static void on_file_created( VFSDir* dir, VFSFileInfo* file, gpointer user_data );
static void on_file_deleted( VFSDir* dir, VFSFileInfo* file, gpointer user_data );
static void on_file_changed( VFSDir* dir, VFSFileInfo* file, gpointer user_data );
static void on_thumbnail_loaded( VFSDir* dir,  VFSFileInfo* fi, DesktopWindow* self );

static void on_sort_by_name ( GtkMenuItem *menuitem, DesktopWindow* self );
static void on_sort_by_size ( GtkMenuItem *menuitem, DesktopWindow* self );
static void on_sort_by_mtime ( GtkMenuItem *menuitem, DesktopWindow* self );
static void on_sort_by_type ( GtkMenuItem *menuitem, DesktopWindow* self );
static void on_sort_custom( GtkMenuItem *menuitem, DesktopWindow* self );
static void on_sort_ascending( GtkMenuItem *menuitem, DesktopWindow* self );
static void on_sort_descending( GtkMenuItem *menuitem, DesktopWindow* self );

static void on_paste( GtkMenuItem *menuitem, DesktopWindow* self );
static void on_settings( GtkMenuItem *menuitem, DesktopWindow* self );

static void on_popup_new_folder_activate ( GtkMenuItem *menuitem, gpointer data );
static void on_popup_new_text_file_activate ( GtkMenuItem *menuitem, gpointer data );

static GdkFilterReturn on_rootwin_event ( GdkXEvent *xevent, GdkEvent *event, gpointer data );
static void forward_event_to_rootwin( GdkScreen *gscreen, GdkEvent *event );

static void calc_item_size( DesktopWindow* self, DesktopItem* item );
static void layout_items( DesktopWindow* self );
static void paint_item( DesktopWindow* self, DesktopItem* item, GdkRectangle* expose_area );
static void move_item( DesktopWindow* self, DesktopItem* item, int x, int y, gboolean is_offset );
static void paint_rubber_banding_rect( DesktopWindow* self );

/* FIXME: this is too dirty and here is some redundant code.
 *  We really need better and cleaner APIs for this */
static void open_folders( GList* folders );

static GList* sort_items( GList* items, DesktopWindow* win );
static GCompareDataFunc get_sort_func( DesktopWindow* win );
static int comp_item_by_name( DesktopItem* item1, DesktopItem* item2, DesktopWindow* win );
static int comp_item_by_size( DesktopItem* item1, DesktopItem* item2, DesktopWindow* win );
static int comp_item_by_mtime( DesktopItem* item1, DesktopItem* item2, DesktopWindow* win );
static int comp_item_by_type( DesktopItem* item1, DesktopItem* item2, DesktopWindow* win );
static int comp_item_custom( DesktopItem* item1, DesktopItem* item2, DesktopWindow* win );

static void redraw_item( DesktopWindow* win, DesktopItem* item );
static void desktop_item_free( DesktopItem* item );

/*
static GdkPixmap* get_root_pixmap( GdkWindow* root );
static gboolean set_root_pixmap(  GdkWindow* root , GdkPixmap* pix );
*/

static DesktopItem* hit_test( DesktopWindow* self, int x, int y );

/* static Atom ATOM_XROOTMAP_ID = 0; */
static Atom ATOM_NET_WORKAREA = 0;

/* Local data */
static GtkWindowClass *parent_class = NULL;

static GdkPixmap* pix = NULL;

enum {
    DRAG_TARGET_URI_LIST,
    DRAG_TARGET_DESKTOP_ICON
};

/*  Drag & Drop/Clipboard targets  */
static GtkTargetEntry drag_targets[] = {
   { "text/uri-list", 0 , DRAG_TARGET_URI_LIST },
   { "DESKTOP_ICON", GTK_TARGET_SAME_WIDGET, DRAG_TARGET_DESKTOP_ICON }
};

static GdkAtom text_uri_list_atom = 0;
static GdkAtom desktop_icon_atom = 0;

static PtkMenuItemEntry icon_menu[] =
{
    PTK_RADIO_MENU_ITEM( N_( "Sort by _Name" ), on_sort_by_name, 0, 0 ),
    PTK_RADIO_MENU_ITEM( N_( "Sort by _Size" ), on_sort_by_size, 0, 0 ),
    PTK_RADIO_MENU_ITEM( N_( "Sort by _Type" ), on_sort_by_type, 0, 0 ),
    PTK_RADIO_MENU_ITEM( N_( "Sort by _Modification Time" ), on_sort_by_mtime, 0, 0 ),
    /* PTK_RADIO_MENU_ITEM( N_( "Custom" ), on_sort_custom, 0, 0 ), */
    PTK_SEPARATOR_MENU_ITEM,
    PTK_RADIO_MENU_ITEM( N_( "Ascending" ), on_sort_ascending, 0, 0 ),
    PTK_RADIO_MENU_ITEM( N_( "Descending" ), on_sort_descending, 0, 0 ),
    PTK_MENU_END
};

static PtkMenuItemEntry create_new_menu[] =
{
    PTK_IMG_MENU_ITEM( N_( "_Folder" ), "gtk-directory", on_popup_new_folder_activate, 0, 0 ),
    PTK_IMG_MENU_ITEM( N_( "_Text File" ), "gtk-edit", on_popup_new_text_file_activate, 0, 0 ),
    PTK_MENU_END
};

static PtkMenuItemEntry desktop_menu[] =
{
    PTK_POPUP_MENU( N_( "_Icons" ), icon_menu ),
    PTK_STOCK_MENU_ITEM( GTK_STOCK_PASTE, on_paste ),
    PTK_SEPARATOR_MENU_ITEM,
    PTK_POPUP_IMG_MENU( N_( "_Create New" ), "gtk-new", create_new_menu ),
    PTK_SEPARATOR_MENU_ITEM,
    PTK_IMG_MENU_ITEM( N_( "_Desktop Settings" ), GTK_STOCK_PREFERENCES, on_settings, GDK_Return, GDK_MOD1_MASK ),
    PTK_MENU_END
};

GType desktop_window_get_type(void)
{
    static GType self_type = 0;
    if (! self_type)
    {
        static const GTypeInfo self_info =
        {
            sizeof(DesktopWindowClass),
            NULL, /* base_init */
            NULL, /* base_finalize */
            (GClassInitFunc)desktop_window_class_init,
            NULL, /* class_finalize */
            NULL, /* class_data */
            sizeof(DesktopWindow),
            0,
            (GInstanceInitFunc)desktop_window_init,
            NULL /* value_table */
        };

        self_type = g_type_register_static(GTK_TYPE_WINDOW, "DesktopWindow", &self_info, 0);    }

    return self_type;
}

static void desktop_window_class_init(DesktopWindowClass *klass)
{
    GObjectClass *g_object_class;
    GtkWidgetClass* wc;
	typedef gboolean (*DeleteEvtHandler) (GtkWidget*, GdkEvent*);

    g_object_class = G_OBJECT_CLASS(klass);
    wc = GTK_WIDGET_CLASS(klass);

    g_object_class->finalize = desktop_window_finalize;

    wc->expose_event = on_expose;
    wc->size_allocate = on_size_allocate;
    wc->size_request = on_size_request;
    wc->button_press_event = on_button_press;
    wc->button_release_event = on_button_release;
    wc->motion_notify_event = on_mouse_move;
    wc->key_press_event = on_key_press;
    wc->style_set = on_style_set;
    wc->realize = on_realize;
    wc->focus_in_event = on_focus_in;
    wc->focus_out_event = on_focus_out;
    /* wc->scroll_event = on_scroll; */
    wc->delete_event = (gpointer)gtk_true;

    wc->drag_begin = on_drag_begin;
    wc->drag_motion = on_drag_motion;
    wc->drag_drop = on_drag_drop;
    wc->drag_data_get = on_drag_data_get;
    wc->drag_data_received = on_drag_data_received;
    wc->drag_leave = on_drag_leave;
    wc->drag_end = on_drag_end;

    parent_class = (GtkWindowClass*)g_type_class_peek(GTK_TYPE_WINDOW);

    /* ATOM_XROOTMAP_ID = XInternAtom( GDK_DISPLAY(),"_XROOTMAP_ID", False ); */
    ATOM_NET_WORKAREA = XInternAtom( GDK_DISPLAY(),"_NET_WORKAREA", False );

    text_uri_list_atom = gdk_atom_intern_static_string( drag_targets[DRAG_TARGET_URI_LIST].target );
    desktop_icon_atom = gdk_atom_intern_static_string( drag_targets[DRAG_TARGET_DESKTOP_ICON].target );
}

static void desktop_window_init(DesktopWindow *self)
{
    PangoContext* pc;
    PangoFontMetrics *metrics;
    int font_h;
    GdkWindow* root;

    /* sort by name by default */
//    self->sort_by = DW_SORT_BY_NAME;
//    self->sort_type = GTK_SORT_ASCENDING;
    self->sort_by = app_settings.desktop_sort_by;
    self->sort_type = app_settings.desktop_sort_type;

    self->icon_render = gtk_cell_renderer_pixbuf_new();
    g_object_set( self->icon_render, "follow-state", TRUE, NULL);
#if GTK_CHECK_VERSION( 2, 10, 0 )
    g_object_ref_sink(self->icon_render);
#else
    g_object_ref( self->icon_render );
    gtk_object_sink(self->icon_render);
#endif
    pc = gtk_widget_get_pango_context( (GtkWidget*)self );
    self->pl = gtk_widget_create_pango_layout( (GtkWidget*)self, NULL );
    pango_layout_set_alignment( self->pl, PANGO_ALIGN_CENTER );
    pango_layout_set_wrap( self->pl, PANGO_WRAP_WORD_CHAR );
    pango_layout_set_width( self->pl, 100 * PANGO_SCALE );

    metrics = pango_context_get_metrics(
                            pc, ((GtkWidget*)self)->style->font_desc,
                            pango_context_get_language(pc));

    font_h = pango_font_metrics_get_ascent(metrics) + pango_font_metrics_get_descent (metrics);
    font_h /= PANGO_SCALE;

    self->label_w = 100;
    self->icon_size = 48;
    self->spacing = 0;
    self->x_pad = 6;
    self->y_pad = 6;
    self->y_margin = 6;
    self->x_margin = 6;

    const char* desk_dir = vfs_get_desktop_dir();
    if ( desk_dir )
    {
        if ( !g_file_test( desk_dir, G_FILE_TEST_EXISTS ) )
        {
            g_mkdir_with_parents( desk_dir, 0700 );
            desk_dir = vfs_get_desktop_dir();
        }
        if ( g_file_test( desk_dir, G_FILE_TEST_EXISTS ) )
            self->dir = vfs_dir_get_by_path( vfs_get_desktop_dir() );
        else
            self->dir = NULL;  // FIXME this is not handled well 
    }

    if ( self->dir && vfs_dir_is_file_listed( self->dir ) )
        on_file_listed( self->dir, FALSE, self );
    if ( self->dir )
    {
        g_signal_connect( self->dir, "file-listed", G_CALLBACK( on_file_listed ), self );
        g_signal_connect( self->dir, "file-created", G_CALLBACK( on_file_created ), self );
        g_signal_connect( self->dir, "file-deleted", G_CALLBACK( on_file_deleted ), self );
        g_signal_connect( self->dir, "file-changed", G_CALLBACK( on_file_changed ), self );
        g_signal_connect( self->dir, "thumbnail-loaded", G_CALLBACK( on_thumbnail_loaded ), self );
    }
    GTK_WIDGET_SET_FLAGS( (GtkWidget*)self, GTK_CAN_FOCUS );

    gtk_widget_set_app_paintable(  (GtkWidget*)self, TRUE );
/*    gtk_widget_set_double_buffered(  (GtkWidget*)self, FALSE ); */
    gtk_widget_add_events( (GtkWidget*)self,
                                            GDK_POINTER_MOTION_MASK |
                                            GDK_BUTTON_PRESS_MASK |
                                            GDK_BUTTON_RELEASE_MASK |
                                            GDK_KEY_PRESS_MASK|
                                            GDK_PROPERTY_CHANGE_MASK );

    gtk_drag_dest_set( (GtkWidget*)self, 0, NULL, 0,
                       GDK_ACTION_COPY|GDK_ACTION_MOVE|GDK_ACTION_LINK );

    root = gdk_screen_get_root_window( gtk_widget_get_screen( (GtkWidget*)self ) );
    gdk_window_set_events( root, gdk_window_get_events( root )
                           | GDK_PROPERTY_CHANGE_MASK );
    gdk_window_add_filter( root, on_rootwin_event, self );
}


GtkWidget* desktop_window_new(void)
{
    GtkWindow* w = (GtkWindow*)g_object_new(DESKTOP_WINDOW_TYPE, NULL);
    w->type = GTK_WINDOW_TOPLEVEL;
    return (GtkWidget*)w;
}

void desktop_item_free( DesktopItem* item )
{
	vfs_file_info_unref( item->fi );
	g_slice_free( DesktopItem, item );
}

void desktop_window_finalize(GObject *object)
{
    DesktopWindow *self = (DesktopWindow*)object;

    g_return_if_fail(object != NULL);
    g_return_if_fail(IS_DESKTOP_WINDOW(object));

    gdk_window_remove_filter(
                    gdk_screen_get_root_window( gtk_widget_get_screen( (GtkWidget*)object) ),
                    on_rootwin_event, self );

    if( self->background )
        g_object_unref( self->background );

    if( self->hand_cursor )
        gdk_cursor_unref( self->hand_cursor );

    self = DESKTOP_WINDOW(object);
    if ( self->dir )
        g_object_unref( self->dir );

	g_list_foreach( self->items, (GFunc)desktop_item_free, NULL );
	g_list_free( self->items );

    if (G_OBJECT_CLASS(parent_class)->finalize)
        (* G_OBJECT_CLASS(parent_class)->finalize)(object);
}

/*--------------- Signal handlers --------------*/

gboolean on_expose( GtkWidget* w, GdkEventExpose* evt )
{
    DesktopWindow* self = (DesktopWindow*)w;
    GList* l;
    GdkRectangle intersect;

    if( G_UNLIKELY( ! GTK_WIDGET_VISIBLE (w) || ! GTK_WIDGET_MAPPED (w) ) )
        return TRUE;
/*
    gdk_draw_drawable( w->window, self->gc,
                        self->background,
                        evt->area.x, evt->area.y,
                        evt->area.x, evt->area.y,
                        evt->area.width, evt->area.height );
*/
/*
    gdk_gc_set_tile( self->gc, self->background );
    gdk_gc_set_fill( self->gc,  GDK_TILED );/*
    gdk_draw_rectangle( w->window, self->gc, TRUE,
                        evt->area.x, evt->area.y,
                        evt->area.width, evt->area.height );
*/

    if( self->rubber_bending )
        paint_rubber_banding_rect( self );

    for( l = self->items; l; l = l->next )
    {
        DesktopItem* item = (DesktopItem*)l->data;
        if( gdk_rectangle_intersect( &evt->area, &item->box, &intersect ) )
            paint_item( self, item, &intersect );
    }
    return TRUE;
}

void on_size_allocate( GtkWidget* w, GtkAllocation* alloc )
{
    GdkPixbuf* pix;
    DesktopWindow* self = (DesktopWindow*)w;
    GdkRectangle wa;

    get_working_area( gtk_widget_get_screen(w), &self->wa );
    layout_items( DESKTOP_WINDOW(w) );

    GTK_WIDGET_CLASS(parent_class)->size_allocate( w, alloc );
}

void desktop_window_set_pixmap( DesktopWindow* win, GdkPixmap* pix )
{
    if( win->background )
        g_object_unref( win->background );
    win->background = pix ? g_object_ref( pix ) : NULL;

    if( GTK_WIDGET_REALIZED( win ) )
        gdk_window_set_back_pixmap( ((GtkWidget*)win)->window, win->background, FALSE );
}

void desktop_window_set_bg_color( DesktopWindow* win, GdkColor* clr )
{
    if( clr )
    {
        win->bg = *clr;
        gdk_rgb_find_color( gtk_widget_get_colormap( (GtkWidget*)win ),
                            &win->bg );
        if( GTK_WIDGET_VISIBLE(win) )
            gtk_widget_queue_draw(  (GtkWidget*)win );
    }
}

void desktop_window_set_text_color( DesktopWindow* win, GdkColor* clr, GdkColor* shadow )
{
    if( clr || shadow )
    {
        if( clr )
        {
            win->fg = *clr;
            gdk_rgb_find_color( gtk_widget_get_colormap( (GtkWidget*)win ),
                                &win->fg );
        }
        if( shadow )
        {
            win->shadow = *shadow;
            gdk_rgb_find_color( gtk_widget_get_colormap( (GtkWidget*)win ),
                                &win->shadow );
        }
        if( GTK_WIDGET_VISIBLE(win) )
            gtk_widget_queue_draw(  (GtkWidget*)win );
    }
}

/*
 *  Set background of the desktop window.
 *  src_pix is the source pixbuf in original size (no scaling)
 *  This function will stretch or add border to this pixbuf accordiong to 'type'.
 *  If type = DW_BG_COLOR and src_pix = NULL, the background color is used to fill the window.
 */
void desktop_window_set_background( DesktopWindow* win, GdkPixbuf* src_pix, DWBgType type )
{
    GdkPixmap* pixmap = NULL;
    Display* xdisplay;
    Pixmap xpixmap = 0;
    Window xroot;

    win->bg_type = type;

    if( src_pix )
    {
        int src_w = gdk_pixbuf_get_width(src_pix);
        int src_h = gdk_pixbuf_get_height(src_pix);
        int dest_w = gdk_screen_get_width( gtk_widget_get_screen((GtkWidget*)win) );
        int dest_h = gdk_screen_get_height( gtk_widget_get_screen((GtkWidget*)win) );
        GdkPixbuf* scaled = NULL;

        if( type == DW_BG_TILE )
        {
            pixmap = gdk_pixmap_new( ((GtkWidget*)win)->window, src_w, src_h, -1 );
            gdk_draw_pixbuf( pixmap, NULL, src_pix, 0, 0, 0, 0, src_w, src_h, GDK_RGB_DITHER_NORMAL, 0, 0 );
        }
        else
        {
            int src_x = 0, src_y = 0;
            int dest_x = 0, dest_y = 0;
            int w = 0, h = 0;

            pixmap = gdk_pixmap_new( ((GtkWidget*)win)->window, dest_w, dest_h, -1 );
            switch( type )
            {
            case DW_BG_STRETCH:
                if( src_w == dest_w && src_h == dest_h ) /* the same size, no scale is needed */
                    scaled = (GdkPixbuf*)g_object_ref( src_pix );
                else
                    scaled = gdk_pixbuf_scale_simple( src_pix, dest_w, dest_h, GDK_INTERP_BILINEAR );
                w = dest_w;
                h = dest_h;
                break;
            case DW_BG_FULL:
                if( src_w == dest_w && src_h == dest_h )
                    scaled = (GdkPixbuf*)g_object_ref( src_pix );
                else
                {
                    gdouble w_ratio = (float)dest_w / src_w;
                    gdouble h_ratio = (float)dest_h / src_h;
                    gdouble ratio = MIN( w_ratio, h_ratio );
                    if( ratio == 1.0 )
                        scaled = (GdkPixbuf*)g_object_ref( src_pix );
                    else
                        scaled = gdk_pixbuf_scale_simple( src_pix, (src_w * ratio), (src_h * ratio), GDK_INTERP_BILINEAR );
                }
                w = gdk_pixbuf_get_width( scaled );
                h = gdk_pixbuf_get_height( scaled );

                if( w > dest_w )
                    src_x = (w - dest_w) / 2;
                else if( w < dest_w )
                    dest_x = (dest_w - w) / 2;

                if( h > dest_h )
                    src_y = (h - dest_h) / 2;
                else if( h < dest_h )
                    dest_y = (dest_h - h) / 2;
                break;
            case DW_BG_CENTER:  /* no scale is needed */
                scaled = (GdkPixbuf*)g_object_ref( src_pix );

                if( src_w > dest_w )
                {
                    w = dest_w;
                    src_x = (src_w - dest_w) / 2;
                }
                else
                {
                    w = src_w;
                    dest_x = (dest_w - src_w) / 2;
                }
                if( src_h > dest_h )
                {
                    h = dest_h;
                    src_y = (src_h - dest_h) / 2;
                }
                else
                {
                    h = src_h;
                    dest_y = (dest_h - src_h) / 2;
                }
                break;
            }

            if( scaled )
            {
                if( w != dest_w || h != dest_h )
                {
                    GdkGC *gc = gdk_gc_new( ((GtkWidget*)win)->window );
                    gdk_gc_set_fill(gc, GDK_SOLID);
                    gdk_gc_set_foreground( gc, &win->bg);
                    gdk_draw_rectangle( pixmap, gc, TRUE, 0, 0, dest_w, dest_h );
                    g_object_unref(gc);
                }
                gdk_draw_pixbuf( pixmap, NULL, scaled, src_x, src_y, dest_x, dest_y, w, h,
                                                GDK_RGB_DITHER_NORMAL, 0, 0 );
                g_object_unref( scaled );
            }
            else
            {
                g_object_unref( pixmap );
                pixmap = NULL;
            }
        }
    }

    if( win->background )
        g_object_unref( win->background );
    win->background = pixmap;

    if( pixmap )
        gdk_window_set_back_pixmap( ((GtkWidget*)win)->window, pixmap, FALSE );
    else
        gdk_window_set_background( ((GtkWidget*)win)->window, &win->bg );

    gdk_window_clear( ((GtkWidget*)win)->window );
    gtk_widget_queue_draw( (GtkWidget*)win );

    /* set root map here */
    xdisplay = GDK_DISPLAY_XDISPLAY( gtk_widget_get_display( (GtkWidget*)win) );
    xroot = GDK_WINDOW_XID( gtk_widget_get_root_window( (GtkWidget*)win ) );

    XGrabServer (xdisplay);

    if( pixmap )
    {
        xpixmap = GDK_WINDOW_XWINDOW(pixmap);

        XChangeProperty( xdisplay,
                    xroot,
                    gdk_x11_get_xatom_by_name("_XROOTPMAP_ID"), XA_PIXMAP,
                    32, PropModeReplace,
                    (guchar *) &xpixmap, 1);

        XSetWindowBackgroundPixmap( xdisplay, xroot, xpixmap );
    }
    else
    {
        /* FIXME: Anyone knows how to handle this correctly??? */
    }
    XClearWindow( xdisplay, xroot );

    XUngrabServer( xdisplay );
    XFlush( xdisplay );
}

void desktop_window_set_icon_size( DesktopWindow* win, int size )
{
    GList* l;
    win->icon_size = size;
    layout_items( win );
    for( l = win->items; l; l = l->next )
    {
        VFSFileInfo* fi = ((DesktopItem*)l->data)->fi;
        char* path;
        /* reload the icons for special items if needed */
        if( (fi->flags & VFS_FILE_INFO_DESKTOP_ENTRY)  && ! fi->big_thumbnail)
        {
            path = g_build_filename( vfs_get_desktop_dir(), fi->name, NULL );
            vfs_file_info_load_special_info( fi, path );
            g_free( path );
        }
        else if(fi->flags & VFS_FILE_INFO_VIRTUAL)
        {
            /* Currently only "My Documents" is supported */
            if( fi->big_thumbnail )
                g_object_unref( fi->big_thumbnail );
            fi->big_thumbnail = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "gnome-fs-home", size, 0, NULL );
            if( ! fi->big_thumbnail )
                fi->big_thumbnail = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "folder-home", size, 0, NULL );
        }
        else if( vfs_file_info_is_image( fi ) && ! fi->big_thumbnail )
        {
            vfs_thumbnail_loader_request( win->dir, fi, TRUE );
        }
    }
}

void desktop_window_reload_icons( DesktopWindow* win )
{
/*
	GList* l;
	for( l = win->items; l; l = l->next )
	{
		DesktopItem* item = (DesktopItem*)l->data;

		if( item->icon )
			g_object_unref( item->icon );

		if( item->fi )
			item->icon =  vfs_file_info_get_big_icon( item->fi );
		else
			item->icon = NULL;
	}
*/
	layout_items( win );
}



void on_size_request( GtkWidget* w, GtkRequisition* req )
{
    GdkScreen* scr = gtk_widget_get_screen( w );
    req->width = gdk_screen_get_width( scr );
    req->height = gdk_screen_get_height( scr );
}

static void calc_rubber_banding_rect( DesktopWindow* self, int x, int y, GdkRectangle* rect )
{
    int x1, x2, y1, y2, w, h;
    if( self->drag_start_x < x )
    {
        x1 = self->drag_start_x;
        x2 = x;
    }
    else
    {
        x1 = x;
        x2 = self->drag_start_x;
    }

    if( self->drag_start_y < y )
    {
        y1 = self->drag_start_y;
        y2 = y;
    }
    else
    {
        y1 = y;
        y2 = self->drag_start_y;
    }

    rect->x = x1;
    rect->y = y1;
    rect->width = x2 - x1;
    rect->height = y2 - y1;
}

/*
 * Reference: xfdesktop source code
 * http://svn.xfce.org/index.cgi/xfce/view/xfdesktop/trunk/src/xfdesktop-icon-view.c
 * xfdesktop_multiply_pixbuf_rgba()
 * Originally copied from Nautilus, Copyright (C) 2000 Eazel, Inc.
 * Multiplies each pixel in a pixbuf by the specified color
 */
static void colorize_pixbuf( GdkPixbuf* pix, GdkColor* clr, guint alpha )
{
    guchar *pixels, *p;
    int x, y, width, height, rowstride;
    gboolean has_alpha;
    int r = clr->red * 255 / 65535;
    int g = clr->green * 255 / 65535;
    int b = clr->blue * 255 / 65535;
    int a = alpha * 255 / 255;

    pixels = gdk_pixbuf_get_pixels(pix);
    width = gdk_pixbuf_get_width(pix);
    height = gdk_pixbuf_get_height(pix);
    has_alpha = gdk_pixbuf_get_has_alpha(pix);
    rowstride = gdk_pixbuf_get_rowstride(pix);

    for (y = 0; y < height; y++)
    {
        p = pixels;
        for (x = 0; x < width; x++)
        {
            p[0] = p[0] * r / 255;
            p[1] = p[1] * g / 255;
            p[2] = p[2] * b / 255;
            if( has_alpha )
            {
                p[3] = p[3] * a / 255;
                p += 4;
            }
            else
                p += 3;
        }
        pixels += rowstride;
    }
}

void paint_rubber_banding_rect( DesktopWindow* self )
{
    int x1, x2, y1, y2, w, h, pattern_w, pattern_h;
    GdkRectangle rect;
    GdkColor *clr;
    guchar alpha;
    GdkPixbuf* pix;
    GdkGC* gc;

    calc_rubber_banding_rect( self, self->rubber_bending_x, self->rubber_bending_y, &rect );

    if( rect.width <= 0 || rect.height <= 0 )
        return;
/*
    gtk_widget_style_get( GTK_WIDGET(self),
                        "selection-box-color", &clr,
                        "selection-box-alpha", &alpha,
                        NULL);
*/

    gc = gdk_gc_new( ((GtkWidget*)self)->window);
    clr = gdk_color_copy (&GTK_WIDGET (self)->style->base[GTK_STATE_SELECTED]);
    alpha = 64;  /* FIXME: should be themable in the future */

    pix = NULL;
    if( self->bg_type == DW_BG_TILE )
    {
        /* FIXME: disable background in tile mode because current implementation is too slow */
        /*
        gdk_drawable_get_size( self->background, &pattern_w, &pattern_h );
        pix = gdk_pixbuf_get_from_drawable( NULL, self->background, gdk_drawable_get_colormap(self->background),
                                                  0, 0, 0, 0, pattern_w, pattern_h );
        */
    }
    else if( self->bg_type != DW_BG_COLOR )
    {
        if( self->background )
            pix = gdk_pixbuf_get_from_drawable( NULL, self->background, gdk_drawable_get_colormap(self->background),
                                                rect.x, rect.y, 0, 0, rect.width, rect.height );
    }

    if( pix )
    {
        colorize_pixbuf( pix, clr, alpha );
        if( self->bg_type == DW_BG_TILE )
        {
            GdkPixmap* pattern;
            /* FIXME: This is damn slow!! */
            pattern = gdk_pixmap_new( ((GtkWidget*)self)->window, pattern_w, pattern_h, -1 );
            if( pattern )
            {
                gdk_draw_pixbuf( pattern, gc, pix, 0, 0,
                                 0, 0, pattern_w, pattern_h, GDK_RGB_DITHER_NONE, 0, 0 );
                gdk_gc_set_tile( gc, pattern );
                gdk_gc_set_fill( gc, GDK_TILED );
        gdk_draw_rectangle( ((GtkWidget*)self)->window, gc, TRUE,
                            rect.x, rect.y, rect.width-1, rect.height-1 );
                g_object_unref( pattern );
                gdk_gc_set_fill( gc, GDK_SOLID );
            }
        }
        else
        {
            gdk_draw_pixbuf( ((GtkWidget*)self)->window, gc, pix, 0, 0,
                            rect.x, rect.y, rect.width, rect.height, GDK_RGB_DITHER_NONE, 0, 0 );
        }
        g_object_unref( pix );
    }
    else if( self->bg_type == DW_BG_COLOR ) /* draw background color */
    {
        GdkColor clr2 = self->bg;
        clr2.pixel = 0;
        clr2.red = clr2.red * clr->red / 65535;
        clr2.green = clr2.green * clr->green / 65535;
        clr2.blue = clr2.blue * clr->blue / 65535;
        gdk_gc_set_rgb_fg_color( gc, &clr2 );
        gdk_gc_set_fill( gc, GDK_SOLID );
        gdk_draw_rectangle( ((GtkWidget*)self)->window, gc, TRUE,
                            rect.x, rect.y, rect.width-1, rect.height-1 );
    }

    /* draw the border */
    gdk_gc_set_foreground( gc, clr );
    gdk_draw_rectangle( ((GtkWidget*)self)->window, gc, FALSE,
                        rect.x, rect.y, rect.width-1, rect.height-1 );

    gdk_color_free (clr);
    gdk_gc_destroy( gc );
}

static void update_rubberbanding( DesktopWindow* self, int newx, int newy )
{
    GList* l;
    GdkRectangle old_rect, new_rect;
    GdkRegion *region;

    calc_rubber_banding_rect(self, self->rubber_bending_x, self->rubber_bending_y, &old_rect );
    calc_rubber_banding_rect(self, newx, newy, &new_rect );

    gdk_window_invalidate_rect(((GtkWidget*)self)->window, &old_rect, FALSE );
    gdk_window_invalidate_rect(((GtkWidget*)self)->window, &new_rect, FALSE );
//    gdk_window_clear_area(((GtkWidget*)self)->window, new_rect.x, new_rect.y, new_rect.width, new_rect.height );
/*
    region = gdk_region_rectangle( &old_rect );
    gdk_region_union_with_rect( region, &new_rect );

//    gdk_window_invalidate_region( ((GtkWidget*)self)->window, &region, TRUE );

    gdk_region_destroy( region );
*/
    self->rubber_bending_x = newx;
    self->rubber_bending_y = newy;

    /* update selection */
    for( l = self->items; l; l = l->next )
    {
        DesktopItem* item = (DesktopItem*)l->data;
        gboolean selected;
        if( gdk_rectangle_intersect( &new_rect, &item->icon_rect, NULL ) ||
            gdk_rectangle_intersect( &new_rect, &item->text_rect, NULL ) )
            selected = TRUE;
        else
            selected = FALSE;

        if( item->is_selected != selected )
        {
            item->is_selected = selected;
            redraw_item( self, item );
        }
    }
}

static void open_clicked_item( DesktopItem* clicked_item )
{
    char* path = NULL;

    if( vfs_file_info_is_dir( clicked_item->fi ) )  /* this is a folder */
    {
        GList* sel_files = NULL;
        sel_files = g_list_prepend( sel_files, clicked_item->fi );
        open_folders( sel_files );
        g_list_free( sel_files );
    }
    else /* regular files */
    {
        GList* sel_files = NULL;
        sel_files = g_list_prepend( sel_files, clicked_item->fi );
        
        // archive?
        if( sel_files && !xset_get_b( "arc_def_open" ) )
        {
            VFSFileInfo* file = vfs_file_info_ref( (VFSFileInfo*)sel_files->data );
            VFSMimeType* mime_type = vfs_file_info_get_mime_type( file );
            path = g_build_filename( vfs_get_desktop_dir(),
                                            vfs_file_info_get_name( file ), NULL );
            vfs_file_info_unref( file );    
            if ( ptk_file_archiver_is_format_supported( mime_type, TRUE ) )
            {
                int no_write_access = ptk_file_browser_no_access ( 
                                                    vfs_get_desktop_dir(), NULL );

                // first file is archive - use default archive action
                if ( xset_get_b( "arc_def_ex" ) && !no_write_access )
                {
                    ptk_file_archiver_extract( NULL, sel_files,
                                    vfs_get_desktop_dir(), vfs_get_desktop_dir() );
                    goto _done;
                }
                else if ( xset_get_b( "arc_def_exto" ) || 
                        ( xset_get_b( "arc_def_ex" ) && no_write_access &&
                                            !( g_str_has_suffix( path, ".gz" ) && 
                                            !g_str_has_suffix( path, ".tar.gz" ) ) ) )
                {
                    ptk_file_archiver_extract( NULL, sel_files, 
                                                    vfs_get_desktop_dir(), NULL );
                    goto _done;
                }
                else if ( xset_get_b( "arc_def_list" ) && 
                            !( g_str_has_suffix( path, ".gz" ) && 
                                            !g_str_has_suffix( path, ".tar.gz" ) ) )
                {
                    ptk_file_archiver_extract( NULL, sel_files,
                                                vfs_get_desktop_dir(), "////LIST" );
                    goto _done;
                }
            }
        }

        if ( sel_files && xset_get_b( "iso_auto" ) )
        {
            VFSFileInfo* file = vfs_file_info_ref( (VFSFileInfo*)sel_files->data );
            VFSMimeType* mime_type = vfs_file_info_get_mime_type( file );
            if ( mime_type && ( 
                    !strcmp( vfs_mime_type_get_type( mime_type ), "application/x-cd-image" ) ||
                    !strcmp( vfs_mime_type_get_type( mime_type ), "application/x-iso9660-image" ) ) )
            {
                char* str = g_find_program_in_path( "udevil" );
                if ( str )
                {
                    g_free( str );
                    str = g_build_filename( vfs_get_desktop_dir(),
                                                vfs_file_info_get_name( file ), NULL );
                    mount_iso( NULL, str );
                    g_free( str );
                    vfs_file_info_unref( file );
                    goto _done;
                }
            }
            vfs_file_info_unref( file );
        }

        ptk_open_files_with_app( vfs_get_desktop_dir(), sel_files, NULL, NULL,
                                                            TRUE, FALSE ); //MOD
_done:
        g_free( path );
        g_list_free( sel_files );
    }
}

gboolean on_button_press( GtkWidget* w, GdkEventButton* evt )
{
    DesktopWindow* self = (DesktopWindow*)w;
    DesktopItem *item, *clicked_item = NULL;
    GList* l;

    clicked_item = hit_test( DESKTOP_WINDOW(w), (int)evt->x, (int)evt->y );

    if( evt->type == GDK_BUTTON_PRESS )
    {
        if( evt->button == 1 )  /* left button */
        {
            self->button_pressed = TRUE;    /* store button state for drag & drop */
            self->drag_start_x = evt->x;
            self->drag_start_y = evt->y;
        }

        /* if ctrl / shift is not pressed, deselect all. */
        if( ! (evt->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) )
        {
            /* don't cancel selection if clicking on selected items */
            if( !( (evt->button == 1 || evt->button == 3) && clicked_item && clicked_item->is_selected) )
            {
                for( l = self->items; l ;l = l->next )
                {
                    item = (DesktopItem*) l->data;
                    if( item->is_selected )
                    {
                        item->is_selected = FALSE;
                        redraw_item( self, item );
                    }
                }
            }
        }

        if( clicked_item )
        {
            if( evt->state & ( GDK_CONTROL_MASK) )
                clicked_item->is_selected = ! clicked_item->is_selected;
            else if ( evt->state & ( GDK_SHIFT_MASK ) )
            {
                // select range from last focus
                if ( self->focus )
                {
                    int i;
                    l = g_list_find( self->items, clicked_item );
                    int pos_clicked = g_list_position( self->items, l );
                    l = g_list_find( self->items, self->focus );
                    int pos_focus = g_list_position( self->items, l );
                    if ( pos_focus >= 0 && pos_clicked >= 0 )
                    {
                        if ( pos_clicked < pos_focus )
                        {
                            i = pos_focus;
                            pos_focus = pos_clicked;
                            pos_clicked = i;
                        }
                        for ( i = pos_focus; i <= pos_clicked; i++ )
                        {
                            l = g_list_nth( self->items, i );
                            if ( l )
                            {
                                item = (DesktopItem*) l->data;
                                item->is_selected = TRUE;
                                redraw_item( DESKTOP_WINDOW(w), item );
                            }
                        }
                    }
                }
                clicked_item->is_selected = TRUE;
            }
            else
                clicked_item->is_selected = TRUE;

            if( self->focus && self->focus != clicked_item )
            {
                DesktopItem* old_focus = self->focus;
                if( old_focus )
                    redraw_item( DESKTOP_WINDOW(w), old_focus );
            }
            self->focus = clicked_item;
            redraw_item( self, clicked_item );

            if( evt->button == 3 )  /* right click */
            {
                GList* sel = desktop_window_get_selected_items( self );
                if( sel )
                {
                    item = (DesktopItem*)sel->data;
                    GtkMenu* popup;
                    GList* l;
                    char* file_path = g_build_filename( vfs_get_desktop_dir(), item->fi->name, NULL );
                    /* FIXME: show popup menu for files */
                    for( l = sel; l; l = l->next )
                        l->data = vfs_file_info_ref( ((DesktopItem*)l->data)->fi );
                    popup = GTK_MENU(ptk_file_menu_new( self, NULL, file_path, item->fi, vfs_get_desktop_dir(), sel ));
                    g_free( file_path );

                    gtk_menu_popup( popup, NULL, NULL, NULL, NULL, evt->button, evt->time );
                }
            }
            goto out;
        }
        else /* no item is clicked */
        {
            if( evt->button == 3 )  /* right click on the blank area */
            {
                if( ! app_settings.show_wm_menu ) /* if our desktop menu is used */
                {
                    GtkWidget *popup, *sort_by_items[ 4 ], *sort_type_items[ 2 ];
                    int i;
                    /* show the desktop menu */
                    for( i = 0; i < 4; ++i )
                        icon_menu[ i ].ret = &sort_by_items[ i ];
                    for( i = 0; i < 2; ++i )
                        icon_menu[ 5 + i ].ret = &sort_type_items[ i ];
                    popup = ptk_menu_new_from_data( desktop_menu, self, NULL );
                    gtk_check_menu_item_set_active( (GtkCheckMenuItem*)sort_by_items[ self->sort_by ], TRUE );
                    gtk_check_menu_item_set_active( (GtkCheckMenuItem*)sort_type_items[ self->sort_type ], TRUE );
                    gtk_widget_show_all(popup);
                    g_signal_connect( popup, "selection-done", G_CALLBACK(gtk_widget_destroy), NULL );

                    gtk_menu_popup( GTK_MENU(popup), NULL, NULL, NULL, NULL, evt->button, evt->time );
                    goto out;   /* don't forward the event to root win */
                }
            }
            else if( evt->button == 1 )
            {
                self->rubber_bending = TRUE;

                /* FIXME: if you foward the event here, this will break rubber bending... */
                /* forward the event to root window */
                /* forward_event_to_rootwin( gtk_widget_get_screen(w), evt ); */

                gtk_grab_add( w );
                self->rubber_bending_x = evt->x;
                self->rubber_bending_y = evt->y;
                goto out;
            }
        }
    }
    else if( evt->type == GDK_2BUTTON_PRESS )
    {
        if( clicked_item && evt->button == 1)   /* left double click */
        {
            open_clicked_item( clicked_item );
            goto out;
        }
    }
    /* forward the event to root window */
    forward_event_to_rootwin( gtk_widget_get_screen(w), (GdkEvent*)evt );

out:
    if( ! GTK_WIDGET_HAS_FOCUS(w) )
    {
        /* g_debug( "we don't have the focus, grab it!" ); */
        gtk_widget_grab_focus( w );
    }
    return TRUE;
}

gboolean on_button_release( GtkWidget* w, GdkEventButton* evt )
{
    DesktopWindow* self = (DesktopWindow*)w;
    DesktopItem* clicked_item = hit_test( self, evt->x, evt->y );

    self->button_pressed = FALSE;

    if( self->rubber_bending )
    {
        update_rubberbanding( self, evt->x, evt->y );
        gtk_grab_remove( w );
        self->rubber_bending = FALSE;
    }
    else if( self->dragging )
    {
        self->dragging = FALSE;
    }
    else if( self->single_click && evt->button == 1 && (GDK_BUTTON1_MASK == evt->state) )
    {
        if( clicked_item )
        {
            open_clicked_item( clicked_item );
            return TRUE;
        }
    }

    /* forward the event to root window */
    if( ! clicked_item )
        forward_event_to_rootwin( gtk_widget_get_screen(w), (GdkEvent*)evt );

    return TRUE;
}

static gboolean on_single_click_timeout( DesktopWindow* self )
{
    GtkWidget* w = (GtkWidget*)self;
    GdkEventButton evt;
    int x, y;

    /* generate a fake button press */
    /* FIXME: will this cause any problem? */
    evt.type = GDK_BUTTON_PRESS;
    evt.window = w->window;
    gdk_window_get_pointer( w->window, &x, &y, &evt.state );
    evt.x = x;
    evt.y = y;
    evt.state |= GDK_BUTTON_PRESS_MASK;
    evt.state &= ~GDK_BUTTON_MOTION_MASK;
    on_button_press( GTK_WIDGET(self), &evt );

    return FALSE;
}

gboolean on_mouse_move( GtkWidget* w, GdkEventMotion* evt )
{
    DesktopWindow* self = (DesktopWindow*)w;

    if( ! self->button_pressed )
    {
        if( self->single_click )
        {
            DesktopItem* item = hit_test( self, evt->x, evt->y );
            if( item != self->hover_item )
            {
                if( 0 != self->single_click_timeout_handler )
                {
                    g_source_remove( self->single_click_timeout_handler );
                    self->single_click_timeout_handler = 0;
                }
            }
            if( item )
            {
                gdk_window_set_cursor( w->window, self->hand_cursor );
                /* FIXME: timeout should be customizable */
                if( 0 == self->single_click_timeout_handler )
                    self->single_click_timeout_handler = g_timeout_add( 400,
                                        (GSourceFunc)on_single_click_timeout, self );
            }
            else
            {
                gdk_window_set_cursor( w->window, NULL );
            }
            self->hover_item = item;
        }

        return TRUE;
    }

    if( self->dragging )
    {
    }
    else if( self->rubber_bending )
    {
        update_rubberbanding( self, evt->x, evt->y );
    }
    else
    {
        if ( gtk_drag_check_threshold( w,
                                    self->drag_start_x,
                                    self->drag_start_y,
                                    evt->x, evt->y))
        {
            GtkTargetList* target_list;
            gboolean virtual_item = FALSE;
            GList* sels = desktop_window_get_selected_items(self);

            self->dragging = TRUE;
            if( sels && sels->next == NULL ) /* only one item selected */
            {
                DesktopItem* item = (DesktopItem*)sels->data;
                if( item->fi->flags & VFS_FILE_INFO_VIRTUAL )
                    virtual_item = TRUE;
            }
            g_list_free( sels );
            if( virtual_item )
                target_list = gtk_target_list_new( drag_targets + 1, G_N_ELEMENTS(drag_targets) - 1 );
            else
                target_list = gtk_target_list_new( drag_targets, G_N_ELEMENTS(drag_targets) );
            gtk_drag_begin( w, target_list,
                         GDK_ACTION_COPY|GDK_ACTION_MOVE|GDK_ACTION_LINK,
                         1, (GdkEvent*)evt );
            gtk_target_list_unref( target_list );
        }
    }

    return TRUE;
}

void on_drag_begin( GtkWidget* w, GdkDragContext* ctx )
{
    DesktopWindow* self = (DesktopWindow*)w;
}

static GdkAtom get_best_target_at_dest( DesktopWindow* self, GdkDragContext* ctx, gint x, gint y )
{
    DesktopItem* item;
    GdkAtom expected_target = 0;

    if( G_LIKELY(ctx->targets) )
    {
        if( ctx->action != GDK_ACTION_MOVE )
            expected_target = text_uri_list_atom;
        else
        {
            item = hit_test( self, x, y );
            if( item )  /* drag over a desktpo item */
            {
                GList* sels;
                sels = desktop_window_get_selected_items( self );
                /* drag over the selected items themselves */
                if( g_list_find( sels, item ) )
                    expected_target = desktop_icon_atom;
                else
                    expected_target = text_uri_list_atom;
                g_list_free( sels );
            }
            else    /* drag over blank area, check if it's a desktop icon first. */
            {
                if( g_list_find( ctx->targets, GUINT_TO_POINTER(desktop_icon_atom) ) )
                    return desktop_icon_atom;
                expected_target = text_uri_list_atom;
            }
        }
        if( g_list_find( ctx->targets, GUINT_TO_POINTER(expected_target) ) )
            return expected_target;
    }
    return GDK_NONE;
}

#define GDK_ACTION_ALL  (GDK_ACTION_MOVE|GDK_ACTION_COPY|GDK_ACTION_LINK)

gboolean on_drag_motion( GtkWidget* w, GdkDragContext* ctx, gint x, gint y, guint time )
{
    DesktopWindow* self = (DesktopWindow*)w;
    DesktopItem* item;
    GdkAtom target;

    if( ! self->drag_entered )
    {
        self->drag_entered = TRUE;
    }

    if( self->rubber_bending )
    {
        /* g_debug("rubber banding!"); */
        return TRUE;
    }

    /* g_debug( "suggest: %d, action = %d", ctx->suggested_action, ctx->action ); */

    if( g_list_find( ctx->targets, GUINT_TO_POINTER(text_uri_list_atom) ) )
    {
        GdkDragAction suggested_action = 0;
        /* Only 'move' is available. The user force move action by pressing Shift key */
        if( (ctx->actions & GDK_ACTION_ALL) == GDK_ACTION_MOVE )
            suggested_action = GDK_ACTION_MOVE;
        /* Only 'copy' is available. The user force copy action by pressing Ctrl key */
        else if( (ctx->actions & GDK_ACTION_ALL) == GDK_ACTION_COPY )
            suggested_action = GDK_ACTION_COPY;
        /* Only 'link' is available. The user force link action by pressing Shift+Ctrl key */
        else if( (ctx->actions & GDK_ACTION_ALL) == GDK_ACTION_LINK )
            suggested_action = GDK_ACTION_LINK;
        /* Several different actions are available. We have to figure out a good default action. */
        else
        {
            if( get_best_target_at_dest(self, ctx, x, y ) == text_uri_list_atom )
            {
                self->pending_drop_action = TRUE;
                /* check the status of drop site */
                gtk_drag_get_data( w, ctx, text_uri_list_atom, time );
                return TRUE;
            }
            else /* move desktop icon */
            {
                suggested_action = GDK_ACTION_MOVE;
            }
        }
        ctx->action = suggested_action;
        gdk_drag_status( ctx, suggested_action, time );
    }
    else if( g_list_find( ctx->targets, GUINT_TO_POINTER(desktop_icon_atom) ) ) /* moving desktop icon */
    {
        ctx->action = GDK_ACTION_MOVE;
        gdk_drag_status( ctx, GDK_ACTION_MOVE, time );
    }
    else
    {
        gdk_drag_status (ctx, 0, time);
    }
    return TRUE;
}

gboolean on_drag_drop( GtkWidget* w, GdkDragContext* ctx, gint x, gint y, guint time )
{
    DesktopWindow* self = (DesktopWindow*)w;
    GdkAtom target = get_best_target_at_dest( self, ctx, x, y );
    /* g_debug("DROP: %s!", gdk_atom_name(target) ); */
    if( target == GDK_NONE )
        return FALSE;
    if( target == text_uri_list_atom || target == desktop_icon_atom )
        gtk_drag_get_data( w, ctx, target, time );
    return TRUE;
}

void on_drag_data_get( GtkWidget* w, GdkDragContext* ctx, GtkSelectionData* data, guint info, guint time )
{
    DesktopWindow* self = (DesktopWindow*)w;
    GList *sels, *l;
    char* uri_list;
    gsize len;

    if( info == DRAG_TARGET_URI_LIST )
    {
        GString *buf = g_string_sized_new( 4096 );

        sels = desktop_window_get_selected_files( self );

        for( l = sels; l; l = l->next )
        {
            VFSFileInfo* fi = (VFSFileInfo*)l->data;
            char* path, *uri;

            if( fi->flags & VFS_FILE_INFO_VIRTUAL )
                continue;

            path = g_build_filename( vfs_get_desktop_dir(), fi->name, NULL );
            uri = g_filename_to_uri( path, NULL, NULL );
            g_free( path );
            g_string_append( buf, uri );
            g_string_append( buf, "\r\n" );
            g_free( uri );
        }

        g_list_foreach( sels, (GFunc)vfs_file_info_unref, NULL );
        g_list_free( sels );

        uri_list = g_convert( buf->str, buf->len, "ASCII", "UTF-8", NULL, &len, NULL);
        g_string_free( buf, TRUE);

        if( uri_list )
        {
            gtk_selection_data_set( data,
                      text_uri_list_atom,
                      8, (guchar *)uri_list, len );
            g_free (uri_list);
        }
    }
    else if( info == DRAG_TARGET_DESKTOP_ICON )
    {
    }
}

static char** get_files_from_selection_data(GtkSelectionData* data)
{
    char** files = gtk_selection_data_get_uris(data), **pfile;
    if( files )
    {
        /* convert uris to filenames */
        for( pfile = files; *pfile; ++pfile )
        {
            char* file = g_filename_from_uri( *pfile, NULL, NULL );
            g_free( *pfile );
            if( file )
                *pfile = file;
            else
            {
                /* FIXME: This is very inefficient, but it's a rare case. */
                int n = g_strv_length( pfile + 1 );
                if( n > 0 )
                {
                    memmove( pfile, file + sizeof(char*), sizeof(char*) * (n + 1) );    /* omit the path if conversion fails */
                    --pfile;
                }
                else
                {
                    *pfile = NULL;
                    break;
                }
            }
        }
    }
    if( files && ! *files )
    {
        g_strfreev( files );
        files = NULL;
    }
    return files;
}

void on_drag_data_received( GtkWidget* w, GdkDragContext* ctx, gint x, gint y, GtkSelectionData* data, guint info, guint time )
{
    DesktopWindow* self = (DesktopWindow*)w;

    if( data->target == text_uri_list_atom )
    {
        DesktopItem* item = hit_test( self, x, y );
        char* dest_dir = NULL;
        VFSFileTaskType file_action = VFS_FILE_TASK_MOVE;
        PtkFileTask* task = NULL;
        char** files;
        int n, i;
        GList* file_list;

        if( (data->length < 0) || (data->format != 8) )
        {
            gtk_drag_finish( ctx, FALSE, FALSE, time );
            return;
        }

        if( item )
        {
            if( (item->fi->flags & VFS_FILE_INFO_VIRTUAL) && vfs_file_info_is_dir( item->fi ) )
                dest_dir = g_build_filename( vfs_get_desktop_dir(), item->fi->name, NULL );
        }

        /* We are just checking the suggested actions for the drop site, not really drop */
        if( self->pending_drop_action )
        {
            GdkDragAction suggested_action = 0;
            dev_t dest_dev;
            struct stat statbuf;    // skip stat64

            if( stat( dest_dir ? dest_dir : vfs_get_desktop_dir(), &statbuf ) == 0 )
            {
                dest_dev = statbuf.st_dev;
                if( 0 == self->drag_src_dev )
                {
                    char** pfile;
                    files = get_files_from_selection_data(data);
                    self->drag_src_dev = dest_dev;
                    if( files )
                    {
                        for( pfile = files; *pfile; ++pfile )
                        {
                            if( stat( *pfile, &statbuf ) == 0 && statbuf.st_dev != dest_dev )
                            {
                                self->drag_src_dev = statbuf.st_dev;
                                break;
                            }
                        }
                    }
                    g_strfreev( files );
                }

                if( self->drag_src_dev != dest_dev )     /* src and dest are on different devices */
                    suggested_action = GDK_ACTION_COPY;
                else
                    suggested_action = GDK_ACTION_MOVE;
            }
            g_free( dest_dir );
            self->pending_drop_action = FALSE;
            gdk_drag_status( ctx, suggested_action, time );
            return;
        }

        switch ( ctx->action )
        {
        case GDK_ACTION_COPY:
            file_action = VFS_FILE_TASK_COPY;
            break;
        case GDK_ACTION_LINK:
            file_action = VFS_FILE_TASK_LINK;
            break;
            /* FIXME:
              GDK_ACTION_DEFAULT, GDK_ACTION_PRIVATE, and GDK_ACTION_ASK are not handled */
        default:
            break;
        }

        files = get_files_from_selection_data( data );
        if( files )
        {
            /* g_debug("file_atcion: %d", file_action); */
            file_list = NULL;
            n = g_strv_length( files );
            for( i = 0; i < n; ++i )
                file_list = g_list_prepend( file_list, files[i] );
            g_free( files );

            task = ptk_file_task_new( file_action,
                                      file_list,
                                      dest_dir ? dest_dir : vfs_get_desktop_dir(),
                                      GTK_WINDOW( self ), NULL );
            ptk_file_task_run( task );
        }
        g_free( dest_dir );

        gtk_drag_finish( ctx, files != NULL, FALSE, time );
    }
    else if( data->target == desktop_icon_atom ) /* moving desktop icon */
    {
        GList* sels = desktop_window_get_selected_items(self), *l;
        int x_off = x - self->drag_start_x;
        int y_off = y - self->drag_start_y;
        for( l = sels; l; l = l->next )
        {
            DesktopItem* item = l->data;
            #if 0   /* temporarily turn off */
            move_item( self, item, x_off, y_off, TRUE );
            #endif
            /* g_debug( "move: %d, %d", x_off, y_off ); */
        }
        g_list_free( sels );
        gtk_drag_finish( ctx, TRUE, FALSE, time );
    }
}

void on_drag_leave( GtkWidget* w, GdkDragContext* ctx, guint time )
{
    DesktopWindow* self = (DesktopWindow*)w;
    self->drag_entered = FALSE;
    self->drag_src_dev = 0;
}

void on_drag_end( GtkWidget* w, GdkDragContext* ctx )
{
    DesktopWindow* self = (DesktopWindow*)w;
}

void desktop_window_rename_selected_files( DesktopWindow* win,
                                                        GList* files, const char* cwd )
{
    GList* l;
    VFSFileInfo* file;
    for ( l = files; l; l = l->next )
    {
        file = (VFSFileInfo*)l->data;
        if ( vfs_file_info_is_desktop_entry( file ) )
        {
            char* filename = g_filename_display_name( file->name );
            if ( filename )
            {
                char* path = g_build_filename( cwd, filename, NULL );
                const char* name = vfs_file_info_get_disp_name( file );
                char* new_name = NULL;
                char* target = g_file_read_link( path, NULL );
                if ( !target )
                    target = g_strdup( path );
                char* msg = g_strdup_printf( _("Enter new desktop item name:\n\nChanging the name of this desktop item requires modifying the contents of desktop file %s"), target );
                g_free( target );
                if ( !xset_text_dialog( GTK_WIDGET( win ), _("Change Desktop Item Name"), NULL, FALSE, msg,
                                    NULL, name, &new_name, NULL, FALSE, NULL ) || !new_name )
                {
                    g_free( msg );
                    g_free( path );
                    g_free( filename );
                    break;
                }
                g_free( msg );
                if ( new_name[0] == '\0' )
                {
                    g_free( path );
                    g_free( filename );
                    break;
                }
                if ( !vfs_app_desktop_rename( path, new_name ) )
                {
                    if ( win->dir )
                        // in case file is a link
                        vfs_dir_emit_file_changed( win->dir, filename, file );
                    g_free( path );
                    g_free( filename );
                    xset_msg_dialog( GTK_WIDGET( win ), GTK_MESSAGE_ERROR, _("Rename Error"), NULL, 0, _("An error occured renaming this desktop item."), NULL, NULL );
                    break;
                }
                if ( win->dir )
                    // in case file is a link
                    vfs_dir_emit_file_changed( win->dir, filename, file );
                g_free( path );
                g_free( filename );
            }
        }
        else
        {
            if ( !ptk_rename_file( win, NULL, cwd, file, NULL, FALSE, 0 ) )
                break;
        }
    }
 
}

gboolean on_key_press( GtkWidget* w, GdkEventKey* evt )
{
    GList* sels;
    DesktopWindow* self = (DesktopWindow*)w;
    int modifier = ( evt->state & ( GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK ) );

    sels = desktop_window_get_selected_files( self );

    if ( modifier == GDK_CONTROL_MASK )
    {
        switch ( evt->keyval )
        {
        case GDK_x:
            if( sels )
                ptk_clipboard_cut_or_copy_files( vfs_get_desktop_dir(), sels, FALSE );
            break;
        case GDK_c:
            if( sels )
                ptk_clipboard_cut_or_copy_files( vfs_get_desktop_dir(), sels, TRUE );
            break;
        case GDK_v:
            on_paste( NULL, self );
            break;
/*
        case GDK_i:
            ptk_file_browser_invert_selection( file_browser );
            break;
        case GDK_a:
            ptk_file_browser_select_all( file_browser );
            break;
*/
        }
    }
    else if ( modifier == GDK_MOD1_MASK )
    {
        switch ( evt->keyval )
        {
        case GDK_Return:
            if( sels )
                ptk_show_file_properties( NULL, vfs_get_desktop_dir(), sels, 0 );
            break;
        }
    }
    else if ( modifier == GDK_SHIFT_MASK )
    {
        switch ( evt->keyval )
        {
        case GDK_Delete:
            if( sels )
                ptk_delete_files( NULL, vfs_get_desktop_dir(), sels, NULL );
            break;
        }
    }
    else if ( modifier == 0 )
    {
        switch ( evt->keyval )
        {
        case GDK_F2:
            if ( sels )
                desktop_window_rename_selected_files( self, sels, vfs_get_desktop_dir() );
                //ptk_rename_file( NULL, vfs_get_desktop_dir(), (VFSFileInfo*)sels->data );
            break;
        case GDK_Delete:
            if( sels )
                ptk_delete_files( NULL, vfs_get_desktop_dir(), sels, NULL );
            break;
        }
    }

    if( sels )
        vfs_file_info_list_free( sels );

    return TRUE;
}

void on_style_set( GtkWidget* w, GtkStyle* prev )
{
    DesktopWindow* self = (DesktopWindow*)w;

    PangoContext* pc;
    PangoFontMetrics *metrics;
    int font_h;
    pc = gtk_widget_get_pango_context( (GtkWidget*)self );

    metrics = pango_context_get_metrics(
                            pc, ((GtkWidget*)self)->style->font_desc,
                            pango_context_get_language(pc));

    font_h = pango_font_metrics_get_ascent(metrics) + pango_font_metrics_get_descent (metrics);
    font_h /= PANGO_SCALE;
}

void on_realize( GtkWidget* w )
{
    guint32 val;
    DesktopWindow* self = (DesktopWindow*)w;

    GTK_WIDGET_CLASS(parent_class)->realize( w );
    gdk_window_set_type_hint( w->window,
                              GDK_WINDOW_TYPE_HINT_DESKTOP );

    gtk_window_set_skip_pager_hint( GTK_WINDOW(w), TRUE );
    gtk_window_set_skip_taskbar_hint( GTK_WINDOW(w), TRUE );
    gtk_window_set_resizable( (GtkWindow*)w, FALSE );

    /* This is borrowed from fbpanel */
#define WIN_HINTS_SKIP_FOCUS      (1<<0)    /* skip "alt-tab" */
    val = WIN_HINTS_SKIP_FOCUS;
    XChangeProperty(GDK_DISPLAY(), GDK_WINDOW_XWINDOW(w->window),
          XInternAtom(GDK_DISPLAY(), "_WIN_HINTS", False), XA_CARDINAL, 32,
          PropModeReplace, (unsigned char *) &val, 1);

    if( ! self->gc )
        self->gc = gdk_gc_new( w->window );

//    if( self->background )
//        gdk_window_set_back_pixmap( w->window, self->background, FALSE );
}

gboolean on_focus_in( GtkWidget* w, GdkEventFocus* evt )
{
    DesktopWindow* self = (DesktopWindow*) w;
    GTK_WIDGET_SET_FLAGS( w, GTK_HAS_FOCUS );
    if( self->focus )
        redraw_item( self, self->focus );
    return FALSE;
}

gboolean on_focus_out( GtkWidget* w, GdkEventFocus* evt )
{
    DesktopWindow* self = (DesktopWindow*) w;
    if( self->focus )
    {
        GTK_WIDGET_UNSET_FLAGS( w, GTK_HAS_FOCUS );
        redraw_item( self, self->focus );
    }
    return FALSE;
}

/*
gboolean on_scroll( GtkWidget *w, GdkEventScroll *evt, gpointer user_data )
{
    forward_event_to_rootwin( gtk_widget_get_screen( w ), ( GdkEvent* ) evt );
    return TRUE;
}
*/


void on_sort_by_name ( GtkMenuItem *menuitem, DesktopWindow* self )
{
    desktop_window_sort_items( self, DW_SORT_BY_NAME, self->sort_type );
}

void on_sort_by_size ( GtkMenuItem *menuitem, DesktopWindow* self )
{
    desktop_window_sort_items( self, DW_SORT_BY_SIZE, self->sort_type );
}

void on_sort_by_mtime ( GtkMenuItem *menuitem, DesktopWindow* self )
{
    desktop_window_sort_items( self, DW_SORT_BY_MTIME, self->sort_type );
}

void on_sort_by_type ( GtkMenuItem *menuitem, DesktopWindow* self )
{
    desktop_window_sort_items( self, DW_SORT_BY_TYPE, self->sort_type );
}

void on_sort_custom( GtkMenuItem *menuitem, DesktopWindow* self )
{
    desktop_window_sort_items( self, DW_SORT_CUSTOM, self->sort_type );
}

void on_sort_ascending( GtkMenuItem *menuitem, DesktopWindow* self )
{
    desktop_window_sort_items( self, self->sort_by, GTK_SORT_ASCENDING );
}

void on_sort_descending( GtkMenuItem *menuitem, DesktopWindow* self )
{
    desktop_window_sort_items( self, self->sort_by, GTK_SORT_DESCENDING );
}

void on_paste( GtkMenuItem *menuitem, DesktopWindow* self )
{
    const gchar* dest_dir = vfs_get_desktop_dir();
    ptk_clipboard_paste_files( NULL, dest_dir, NULL );
}

void
on_popup_new_folder_activate ( GtkMenuItem *menuitem,
                               gpointer data )
{
    ptk_create_new_file( NULL,  vfs_get_desktop_dir(), TRUE, NULL );
}

void
on_popup_new_text_file_activate ( GtkMenuItem *menuitem,
                                  gpointer data )
{
    ptk_create_new_file( NULL,  vfs_get_desktop_dir(), FALSE, NULL );
}

void on_settings( GtkMenuItem *menuitem, DesktopWindow* self )
{
    fm_edit_preference( NULL, PREF_DESKTOP );
}


/* private methods */

void calc_item_size( DesktopWindow* self, DesktopItem* item )
{
    PangoLayoutLine* line;
    int line_h;

    item->box.width = self->item_w;
    item->box.height = self->y_pad * 2;
    item->box.height += self->icon_size;
    item->box.height += self->spacing;

    pango_layout_set_text( self->pl, item->fi->disp_name, -1 );
    pango_layout_set_wrap( self->pl, PANGO_WRAP_WORD_CHAR );    /* wrap the text */
    pango_layout_set_ellipsize( self->pl, PANGO_ELLIPSIZE_NONE );

    if( pango_layout_get_line_count(self->pl) >= 2 ) /* there are more than 2 lines */
    {
        /* we only allow displaying two lines, so let's get the second line */
/* Pango only provide version check macros in the latest versions...
 *  So there is no point in making this check.
 *  FIXME: this check should be done ourselves in configure.
  */
#if defined (PANGO_VERSION_CHECK)
#if PANGO_VERSION_CHECK( 1, 16, 0 )
        line = pango_layout_get_line_readonly( self->pl, 1 );
#else
        line = pango_layout_get_line( self->pl, 1 );
#endif
#else
        line = pango_layout_get_line( self->pl, 1 );
#endif
        item->len1 = line->start_index; /* this the position where the first line wraps */

        /* OK, now we layout these 2 lines separately */
        pango_layout_set_text( self->pl, item->fi->disp_name, item->len1 );
        pango_layout_get_pixel_size( self->pl, NULL, &line_h );
        item->text_rect.height = line_h;
    }
    else
    {
        item->text_rect.height = 0;
    }

    pango_layout_set_wrap( self->pl, 0 );    /* wrap the text */
    pango_layout_set_ellipsize( self->pl, PANGO_ELLIPSIZE_END );

    pango_layout_set_text( self->pl, item->fi->disp_name + item->len1, -1 );
    pango_layout_get_pixel_size( self->pl, NULL, &line_h );
    item->text_rect.height += line_h;

    item->text_rect.width = 100;
    item->box.height += item->text_rect.height;

    item->icon_rect.width = item->icon_rect.height = self->icon_size;
}

void layout_items( DesktopWindow* self )
{
    GList* l;
    DesktopItem* item;
    GtkWidget* widget = (GtkWidget*)self;
    int x, y, w, y2;

    self->item_w = MAX( self->label_w, self->icon_size ) + self->x_pad * 2;

    x = self->wa.x + self->x_margin;
    y = self->wa.y + self->y_margin;

    pango_layout_set_width( self->pl, 100 * PANGO_SCALE );

    for( l = self->items; l; l = l->next )
    {
        item = (DesktopItem*)l->data;
        item->box.x = x;
        item->box.y = y;

        item->box.width = self->item_w;
        calc_item_size( self, item );

        y2 = self->wa.y + self->wa.height - self->y_margin; /* bottom */
        if( y + item->box.height > y2 ) /* bottom is reached */
        {
            y = self->wa.y + self->y_margin;
            item->box.y = y;
            y += item->box.height;
            x += self->item_w;  /* go to the next column */
            item->box.x = x;
        }
        else /* bottom is not reached */
        {
            y += item->box.height;  /* move to the next row */
        }

        item->icon_rect.x = item->box.x + (item->box.width - self->icon_size) / 2;
        item->icon_rect.y = item->box.y + self->y_pad;

        item->text_rect.x = item->box.x + self->x_pad;
        item->text_rect.y = item->box.y + self->y_pad + self->icon_size + self->spacing;
    }
    gtk_widget_queue_draw( GTK_WIDGET(self) );
}

void on_file_listed( VFSDir* dir, gboolean is_cancelled, DesktopWindow* self )
{
    GList* l, *items = NULL;
    VFSFileInfo* fi;
    DesktopItem* item;

    g_mutex_lock( dir->mutex );
    for( l = dir->file_list; l; l = l->next )
    {
        fi = (VFSFileInfo*)l->data;
        if( fi->name[0] == '.' )    /* skip the hidden files */
            continue;
        item = g_slice_new0( DesktopItem );
        item->fi = vfs_file_info_ref( fi );
        items = g_list_prepend( items, item );
		/* item->icon = vfs_file_info_get_big_icon( fi ); */

        if( vfs_file_info_is_image( fi ) )
            vfs_thumbnail_loader_request( dir, fi, TRUE );
    }
    g_mutex_unlock( dir->mutex );

    self->items = NULL;
    self->items = g_list_sort_with_data( items, get_sort_func(self), self );

/*
    // Make an item for Home dir
    fi = vfs_file_info_new();
    fi->disp_name = g_strdup( _("My Documents") );
    fi->mime_type = vfs_mime_type_get_from_type( XDG_MIME_TYPE_DIRECTORY );
    fi->name = g_strdup( g_get_home_dir() );
    fi->mode |= S_IFDIR;
    fi->flags |= VFS_FILE_INFO_VIRTUAL; // this is a virtual file which doesn't exist on the file system
    fi->big_thumbnail = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "gnome-fs-home", self->icon_size, 0, NULL );
    if( ! fi->big_thumbnail )
        fi->big_thumbnail = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "folder-home", self->icon_size, 0, NULL );

    item = g_slice_new0( DesktopItem );
    item->fi = fi;
	// item->icon = vfs_file_info_get_big_thumbnail( fi );
    self->items = g_list_prepend( self->items, item );
*/

    layout_items( self );
}

void on_thumbnail_loaded( VFSDir* dir,  VFSFileInfo* fi, DesktopWindow* self )
{
    GList* l;
    GtkWidget* w = (GtkWidget*)self;

    for( l = self->items; l; l = l->next )
    {
        DesktopItem* item = (DesktopItem*) l->data;
        if( item->fi == fi )
        {
/*
        	if( item->icon )
        		g_object_unref( item->icon );
        	item->icon = vfs_file_info_get_big_thumbnail( fi );
*/
            redraw_item( self, item );
            return;
        }
    }
}

void on_file_created( VFSDir* dir, VFSFileInfo* file, gpointer user_data )
{
    GList *l;
    DesktopWindow* self = (DesktopWindow*)user_data;
    DesktopItem* item;

    /* don't show hidden files */
    if( file->name[0] == '.' )
        return;

    /* prevent duplicated items */
    for( l = self->items; l; l = l->next )
    {
        item = (DesktopItem*)l->data;
        if( strcmp( file->name, item->fi->name ) == 0 )
            return;
    }

    item = g_slice_new0( DesktopItem );
    item->fi = vfs_file_info_ref( file );
    /* item->icon = vfs_file_info_get_big_icon( file ); */

    self->items = g_list_insert_sorted_with_data( self->items, item,
                                                                            get_sort_func(self), self );

    /* FIXME: we shouldn't update the whole screen */
    /* FIXME: put this in idle handler with priority higher than redraw but lower than resize */
    layout_items( self );
}

void on_file_deleted( VFSDir* dir, VFSFileInfo* file, gpointer user_data )
{
    GList *l;
    DesktopWindow* self = (DesktopWindow*)user_data;
    DesktopItem* item;

    /* FIXME: special handling is needed here */
    if( ! file )
        return;

    /* don't deal with hidden files */
    if( file->name[0] == '.' )
        return;

    /* find items */
    for( l = self->items; l; l = l->next )
    {
        item = (DesktopItem*)l->data;
        if( item->fi == file )
            break;
    }

    if( l ) /* found */
    {
        item = (DesktopItem*)l->data;
        self->items = g_list_delete_link( self->items, l );
        desktop_item_free( item );
        /* FIXME: we shouldn't update the whole screen */
        /* FIXME: put this in idle handler with priority higher than redraw but lower than resize */
        layout_items( self );
    }
}

void on_file_changed( VFSDir* dir, VFSFileInfo* file, gpointer user_data )
{
    GList *l;
    DesktopWindow* self = (DesktopWindow*)user_data;
    DesktopItem* item;
    GtkWidget* w = (GtkWidget*)self;

    /* don't touch hidden files */
    if( file->name[0] == '.' )
        return;

    /* find items */
    for( l = self->items; l; l = l->next )
    {
        item = (DesktopItem*)l->data;
        if( item->fi == file )
            break;
    }

    if( l ) /* found */
    {
        item = (DesktopItem*)l->data;
        /*
   if( item->icon )
        	g_object_unref( item->icon );
		item->icon = vfs_file_info_get_big_icon( file );
		*/
        if( GTK_WIDGET_VISIBLE( w ) )
        {
            /* redraw the item */
            redraw_item( self, item );
        }
    }
}


/*-------------- Private methods -------------------*/

void paint_item( DesktopWindow* self, DesktopItem* item, GdkRectangle* expose_area )
{
    /* GdkPixbuf* icon = item->icon ? gdk_pixbuf_ref(item->icon) : NULL; */
    GdkPixbuf* icon;
    const char* text = item->fi->disp_name;
    GtkWidget* widget = (GtkWidget*)self;
    GdkDrawable* drawable = widget->window;
    GdkGC* tmp;
    GtkCellRendererState state = 0;
    GdkRectangle text_rect;
    int w, h;

    if( item->fi->big_thumbnail )
        icon = g_object_ref( item->fi->big_thumbnail );
    else
        icon = vfs_file_info_get_big_icon( item->fi );

    if( item->is_selected )
        state = GTK_CELL_RENDERER_SELECTED;

    g_object_set( self->icon_render, "pixbuf", icon, NULL );
    gtk_cell_renderer_render( self->icon_render, drawable, widget,
                                                       &item->icon_rect, &item->icon_rect, expose_area, state );

	if( icon )
		g_object_unref( icon );

    tmp = widget->style->fg_gc[0];

    text_rect = item->text_rect;

    pango_layout_set_wrap( self->pl, 0 );

    if( item->is_selected )
    {
        GdkRectangle intersect={0};

        if( gdk_rectangle_intersect( expose_area, &item->text_rect, &intersect ) )
            gdk_draw_rectangle( widget->window,
                        widget->style->bg_gc[GTK_STATE_SELECTED],
                        TRUE, intersect.x, intersect.y, intersect.width, intersect.height );
    }
    else
    {
        /* Do the drop shadow stuff...  This is a little bit dirty... */
        ++text_rect.x;
        ++text_rect.y;

        gdk_gc_set_foreground( self->gc, &self->shadow );

        if( item->len1 > 0 )
        {
            pango_layout_set_text( self->pl, text, item->len1 );
            pango_layout_get_pixel_size( self->pl, &w, &h );
            gdk_draw_layout( widget->window, self->gc, text_rect.x, text_rect.y, self->pl );
            text_rect.y += h;
        }
        pango_layout_set_text( self->pl, text + item->len1, -1 );
        pango_layout_set_ellipsize( self->pl, PANGO_ELLIPSIZE_END );
        gdk_draw_layout( widget->window, self->gc, text_rect.x, text_rect.y, self->pl );

        --text_rect.x;
        --text_rect.y;
    }

    if( self->focus == item && GTK_WIDGET_HAS_FOCUS(widget) )
    {
        gtk_paint_focus( widget->style, widget->window,
                        GTK_STATE_NORMAL,/*item->is_selected ? GTK_STATE_SELECTED : GTK_STATE_NORMAL,*/
                        &item->text_rect, widget, "icon_view",
                        item->text_rect.x, item->text_rect.y,
                        item->text_rect.width, item->text_rect.height);
    }

    text_rect = item->text_rect;

    gdk_gc_set_foreground( self->gc, &self->fg );

    if( item->len1 > 0 )
    {
        pango_layout_set_text( self->pl, text, item->len1 );
        pango_layout_get_pixel_size( self->pl, &w, &h );
        gdk_draw_layout( widget->window, self->gc, text_rect.x, text_rect.y, self->pl );
        text_rect.y += h;
    }
    pango_layout_set_text( self->pl, text + item->len1, -1 );
    pango_layout_set_ellipsize( self->pl, PANGO_ELLIPSIZE_END );
    gdk_draw_layout( widget->window, self->gc, text_rect.x, text_rect.y, self->pl );

    widget->style->fg_gc[0] = tmp;
}

void move_item( DesktopWindow* self, DesktopItem* item, int x, int y, gboolean is_offset )
{
    GdkRectangle old = item->box;

    if( ! is_offset )
    {
        x -= item->box.x;
        y -= item->box.y;
    }
    item->box.x += x;
    item->box.y += y;
    item->icon_rect.x += x;
    item->icon_rect.y += y;
    item->text_rect.x += x;
    item->text_rect.y += y;

    gtk_widget_queue_draw_area( (GtkWidget*)self, old.x, old.y, old.width, old.height );
    gtk_widget_queue_draw_area( (GtkWidget*)self, item->box.x, item->box.y, item->box.width, item->box.height );
}

static gboolean is_point_in_rect( GdkRectangle* rect, int x, int y )
{
    return rect->x < x && x < (rect->x + rect->width) && y > rect->y && y < (rect->y + rect->height);
}

DesktopItem* hit_test( DesktopWindow* self, int x, int y )
{
    DesktopItem* item;
    GList* l;
    for( l = self->items; l; l = l->next )
    {
        item = (DesktopItem*) l->data;
        if( is_point_in_rect( &item->icon_rect, x, y )
         || is_point_in_rect( &item->text_rect, x, y ) )
            return item;
    }
    return NULL;
}

/* FIXME: this is too dirty and here is some redundant code.
 *  We really need better and cleaner APIs for this */
void open_folders( GList* folders )
{
    FMMainWindow* main_window;
    
    main_window = fm_main_window_get_last_active();
    
    if ( !main_window )
    {
        main_window = FM_MAIN_WINDOW(fm_main_window_new());
        //FM_MAIN_WINDOW( main_window ) ->splitter_pos = app_settings.splitter_pos;
        gtk_window_set_default_size( GTK_WINDOW( main_window ),
                                     app_settings.width,
                                     app_settings.height );
        gtk_widget_show( GTK_WIDGET(main_window) );
    }
    
    while( folders )
    {
        VFSFileInfo* fi = (VFSFileInfo*)folders->data;
        char* path;
        if( fi->flags & VFS_FILE_INFO_VIRTUAL )
        {
            /* if this is a special item, not a real file in desktop dir */
            if( fi->name[0] == '/' )   /* it's a real path */
            {
                path = g_strdup( fi->name );
            }
            else
            {
                folders = folders->next;
                continue;
            }
            /* FIXME/TODO: In the future we should handle mounting here. */
        }
        else
        {
            path = g_build_filename( vfs_get_desktop_dir(), fi->name, NULL );
        }
        fm_main_window_add_new_tab( FM_MAIN_WINDOW( main_window ), path );

        g_free( path );
        folders = folders->next;
    }
    gtk_window_present( GTK_WINDOW( main_window ) );
}

GCompareDataFunc get_sort_func( DesktopWindow* win )
{
    GCompareDataFunc comp;

    switch( win->sort_by )
    {
        case DW_SORT_BY_NAME:
            comp = (GCompareDataFunc)comp_item_by_name;
            break;
        case DW_SORT_BY_SIZE:
            comp = (GCompareDataFunc)comp_item_by_size;
            break;
        case DW_SORT_BY_TYPE:
            comp = (GCompareDataFunc)comp_item_by_type;
            break;
        case DW_SORT_BY_MTIME:
            comp = (GCompareDataFunc)comp_item_by_mtime;
            break;
        case DW_SORT_CUSTOM:
            comp = (GCompareDataFunc)comp_item_custom;
            break;
        default:
            comp = (GCompareDataFunc)comp_item_by_name;
    }
    return comp;
}

/* return -1 if item1 is virtual, and item2 is not, and vice versa. return 0 if both are, or both aren't. */
#define COMP_VIRTUAL( item1, item2 )  \
  ( ( ((item2->fi->flags & VFS_FILE_INFO_VIRTUAL) ? 1 : 0) - ((item1->fi->flags & VFS_FILE_INFO_VIRTUAL) ? 1 : 0) ) )

int comp_item_by_name( DesktopItem* item1, DesktopItem* item2, DesktopWindow* win )
{
    int ret;
    if( ret = COMP_VIRTUAL( item1, item2 ) )
        return ret;
    ret =g_utf8_collate( item1->fi->disp_name, item2->fi->disp_name );
    if( win->sort_type == GTK_SORT_DESCENDING )
        ret = -ret;
    return ret;
}

int comp_item_by_size( DesktopItem* item1, DesktopItem* item2, DesktopWindow* win  )
{
    int ret;
    if( ret = COMP_VIRTUAL( item1, item2 ) )
        return ret;
    ret =item1->fi->size - item2->fi->size;
    if( win->sort_type == GTK_SORT_DESCENDING )
        ret = -ret;
    return ret;
}

int comp_item_by_mtime( DesktopItem* item1, DesktopItem* item2, DesktopWindow* win  )
{
    int ret;
    if( ret = COMP_VIRTUAL( item1, item2 ) )
        return ret;
    ret =item1->fi->mtime - item2->fi->mtime;
    if( win->sort_type == GTK_SORT_DESCENDING )
        ret = -ret;
    return ret;
}

int comp_item_by_type( DesktopItem* item1, DesktopItem* item2, DesktopWindow* win  )
{
    int ret;
    if( ret = COMP_VIRTUAL( item1, item2 ) )
        return ret;
    ret = strcmp( item1->fi->mime_type->type, item2->fi->mime_type->type );

    if( win->sort_type == GTK_SORT_DESCENDING )
        ret = -ret;
    return ret;
}

int comp_item_custom( DesktopItem* item1, DesktopItem* item2, DesktopWindow* win )
{
    return (item1->order - item2->order);
}

void redraw_item( DesktopWindow* win, DesktopItem* item )
{
    GdkRectangle rect = item->box;
    --rect.x;
    --rect.y;
    rect.width += 2;
    rect.height += 2;
    gdk_window_invalidate_rect( ((GtkWidget*)win)->window, &rect, FALSE );
}


/* ----------------- public APIs ------------------*/

void desktop_window_sort_items( DesktopWindow* win, DWSortType sort_by,
                                                            GtkSortType sort_type )
{
    GList* items = NULL;
    GList* special_items;
    
    if( win->sort_type == sort_type && win->sort_by == sort_by )
        return;

    app_settings.desktop_sort_by = win->sort_by = sort_by;
    app_settings.desktop_sort_type = win->sort_type = sort_type;
    save_settings( NULL );  //MOD

    /* skip the special items since they always appears first */
    gboolean special = FALSE;    //MOD added - otherwise caused infinite loop in layout items once My Documents was removed
    special_items = win->items;
    for( items = special_items; items; items = items->next )
    {
        DesktopItem* item = (DesktopItem*)items->data;
        if( ! (item->fi->flags & VFS_FILE_INFO_VIRTUAL) )
            break;
        else
            special = TRUE;
    }

    if( ! items )
        return;

    /* the previous item of the first non-special item is the last special item */
    if( special && items->prev )
    {
        items->prev->next = NULL;
        items->prev = NULL;
    }

    items = g_list_sort_with_data( items, get_sort_func(win), win );
    if ( special )
        win->items = g_list_concat( special_items, items );
    else
        win->items = items;
        
    layout_items( win );
}

GList* desktop_window_get_selected_items( DesktopWindow* win )
{
    GList* sel = NULL;
    GList* l;

    for( l = win->items; l; l = l->next )
    {
        DesktopItem* item = (DesktopItem*) l->data;
        if( item->is_selected )
        {
            if( G_UNLIKELY( item == win->focus ) )
                sel = g_list_prepend( sel, item );
            else
                sel = g_list_append( sel, item );
        }
    }

    return sel;
}

GList* desktop_window_get_selected_files( DesktopWindow* win )
{
    GList* sel = desktop_window_get_selected_items( win );
    GList* l;

    l = sel;
    while( l )
    {
        DesktopItem* item = (DesktopItem*) l->data;
        if( item->fi->flags & VFS_FILE_INFO_VIRTUAL )
        {
            /* don't include virtual items */
            GList* tmp = l;
            l = tmp->next;
            sel = g_list_remove_link( sel, tmp );
            g_list_free1( tmp );
        }
        else
        {
            l->data = vfs_file_info_ref( item->fi );
            l = l->next;
        }
    }
    return sel;
}


/*----------------- X11-related sutff ----------------*/

static
GdkFilterReturn on_rootwin_event ( GdkXEvent *xevent,
                                   GdkEvent *event,
                                   gpointer data )
{
    XPropertyEvent * evt = ( XPropertyEvent* ) xevent;
    DesktopWindow* self = (DesktopWindow*)data;

    if ( evt->type == PropertyNotify )
    {
        if( evt->atom == ATOM_NET_WORKAREA )
        {
            /* working area is resized */
            get_working_area( gtk_widget_get_screen((GtkWidget*)self), &self->wa );
            layout_items( self );
        }
#if 0
        else if( evt->atom == ATOM_XROOTMAP_ID )
        {
            /* wallpaper was changed by other programs */
        }
#endif
    }
    return GDK_FILTER_TRANSLATE;
}

/* This function is taken from xfdesktop */
void forward_event_to_rootwin( GdkScreen *gscreen, GdkEvent *event )
{
    XButtonEvent xev, xev2;
    Display *dpy = GDK_DISPLAY_XDISPLAY( gdk_screen_get_display( gscreen ) );

    if ( event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE )
    {
        if ( event->type == GDK_BUTTON_PRESS )
        {
            xev.type = ButtonPress;
            /*
             * rox has an option to disable the next
             * instruction. it is called "blackbox_hack". Does
             * anyone know why exactly it is needed?
             */
            XUngrabPointer( dpy, event->button.time );
        }
        else
            xev.type = ButtonRelease;

        xev.button = event->button.button;
        xev.x = event->button.x;    /* Needed for icewm */
        xev.y = event->button.y;
        xev.x_root = event->button.x_root;
        xev.y_root = event->button.y_root;
        xev.state = event->button.state;

        xev2.type = 0;
    }
    else if ( event->type == GDK_SCROLL )
    {
        xev.type = ButtonPress;
        xev.button = event->scroll.direction + 4;
        xev.x = event->scroll.x;    /* Needed for icewm */
        xev.y = event->scroll.y;
        xev.x_root = event->scroll.x_root;
        xev.y_root = event->scroll.y_root;
        xev.state = event->scroll.state;

        xev2.type = ButtonRelease;
        xev2.button = xev.button;
    }
    else
        return ;
    xev.window = GDK_WINDOW_XWINDOW( gdk_screen_get_root_window( gscreen ) );
    xev.root = xev.window;
    xev.subwindow = None;
    xev.time = event->button.time;
    xev.same_screen = True;

    XSendEvent( dpy, xev.window, False, ButtonPressMask | ButtonReleaseMask,
                ( XEvent * ) & xev );
    if ( xev2.type == 0 )
        return ;

    /* send button release for scroll event */
    xev2.window = xev.window;
    xev2.root = xev.root;
    xev2.subwindow = xev.subwindow;
    xev2.time = xev.time;
    xev2.x = xev.x;
    xev2.y = xev.y;
    xev2.x_root = xev.x_root;
    xev2.y_root = xev.y_root;
    xev2.state = xev.state;
    xev2.same_screen = xev.same_screen;

    XSendEvent( dpy, xev2.window, False, ButtonPressMask | ButtonReleaseMask,
                ( XEvent * ) & xev2 );
}

/* FIXME: set single click timeout */
void desktop_window_set_single_click( DesktopWindow* win, gboolean single_click )
{
    if( single_click == win->single_click )
        return;
    win->single_click = single_click;
    if( single_click )
    {
      win->hand_cursor = gdk_cursor_new_for_display( gtk_widget_get_display(GTK_WIDGET(win)), GDK_HAND2 );
    }
    else
    {
        gdk_cursor_unref( win->hand_cursor );
        win->hand_cursor = NULL;
        if( GTK_WIDGET_REALIZED(win) )
            gdk_window_set_cursor( ((GtkWidget*)win)->window, NULL );
    }
}

#if 0
GdkPixmap* get_root_pixmap( GdkWindow* root )
{
    Pixmap root_pix = None;

    Atom type;
    int format;
    long bytes_after;
    Pixmap *data = NULL;
    long n_items;
    int result;

    result =  XGetWindowProperty(
                            GDK_WINDOW_XDISPLAY( root ),
                            GDK_WINDOW_XID( root ),
                            ATOM_XROOTMAP_ID,
                            0, 16L,
                            False, XA_PIXMAP,
                            &type, &format, &n_items,
                            &bytes_after, (unsigned char **)&data);

    if (result == Success && n_items)
        root_pix = *data;

    if (data)
        XFree(data);

    return root_pix ? gdk_pixmap_foreign_new( root_pix ) : NULL;
}

gboolean set_root_pixmap(  GdkWindow* root, GdkPixmap* pix )
{
    return TRUE;
}

#endif

void desktop_context_fill( DesktopWindow* win, gpointer context )
{
    GtkClipboard* clip = NULL;

    if ( !win )
        return;
    XSetContext* c = (XSetContext*)context;
    c->valid = FALSE;
    // assume we don't need all selected files info
    
    if ( !c->var[CONTEXT_IS_ROOT] )
        c->var[CONTEXT_IS_ROOT] = geteuid() == 0 ?
                                        g_strdup( "true" ) : g_strdup( "false" );

    if ( !c->var[CONTEXT_CLIP_FILES] )
    {
        clip = gtk_clipboard_get( GDK_SELECTION_CLIPBOARD );
        if ( ! gtk_clipboard_wait_is_target_available ( clip,
                    gdk_atom_intern( "x-special/gnome-copied-files", FALSE ) ) &&
             ! gtk_clipboard_wait_is_target_available ( clip,
                        gdk_atom_intern( "text/uri-list", FALSE ) ) )
            c->var[CONTEXT_CLIP_FILES] = g_strdup( "false" );
        else
            c->var[CONTEXT_CLIP_FILES] = g_strdup( "true" );
    }
    
    if ( !c->var[CONTEXT_CLIP_TEXT] )
    {
        if ( !clip )
            clip = gtk_clipboard_get( GDK_SELECTION_CLIPBOARD );
        c->var[CONTEXT_CLIP_TEXT] = gtk_clipboard_wait_is_text_available( clip ) ?
                                    g_strdup( "true" ) : g_strdup( "false" );
    }
    
    c->var[CONTEXT_BOOKMARK] = g_strdup( "" );
    c->var[CONTEXT_DEVICE] = g_strdup( "" );
    c->var[CONTEXT_DEVICE_LABEL] = g_strdup( "" );
    c->var[CONTEXT_DEVICE_MOUNT_POINT] = g_strdup( "" );
    c->var[CONTEXT_DEVICE_UDI] = g_strdup( "" );
    c->var[CONTEXT_DEVICE_FSTYPE] = g_strdup( "" );
    c->var[CONTEXT_DEVICE_PROP] = g_strdup( "" );
    
    c->var[CONTEXT_PANEL_COUNT] = g_strdup( "" );
    c->var[CONTEXT_PANEL] = g_strdup( "" );
    c->var[CONTEXT_TAB] = g_strdup( "" );
    c->var[CONTEXT_TAB_COUNT] = g_strdup( "" );
    int p;
    for ( p = 1; p < 5; p++ )
    {
        c->var[CONTEXT_PANEL1_DIR + p - 1] = g_strdup( "" );
        c->var[CONTEXT_PANEL1_SEL + p - 1] = g_strdup( "false" );
        c->var[CONTEXT_PANEL1_DEVICE + p - 1] = g_strdup( "" );
    }

    c->var[CONTEXT_TASK_TYPE] =  g_strdup( "" );
    c->var[CONTEXT_TASK_NAME] =  g_strdup( "" );
    c->var[CONTEXT_TASK_DIR] =  g_strdup( "" );
    c->var[CONTEXT_TASK_COUNT] = g_strdup( "0" );

    c->valid = TRUE;
}

gboolean desktop_write_exports( VFSFileTask* vtask, const char* value, FILE* file )
{
    int result;
    const char* cwd = vfs_get_desktop_dir();
    char* path;
    char* esc_path;
    GList* sel_files;
    GList* l;
    VFSFileInfo* fi;

    if ( !vtask->exec_desktop )
        return FALSE;
    DesktopWindow* win = (DesktopWindow*)vtask->exec_desktop;
    XSet* set = (XSet*)vtask->exec_set;

    if ( !file )
        return FALSE;
    result = fputs( "# source\n\n", file );
    if ( result < 0 ) return FALSE;
 
    write_src_functions( file );

    // selected files
    sel_files = desktop_window_get_selected_files( win );
    if ( sel_files )
    {
        fprintf( file, "fm_desktop_files=(\n" );
        for ( l = sel_files; l; l = l->next )
        {
            fi = (VFSFileInfo*)l->data;
            if ( fi->flags & VFS_FILE_INFO_VIRTUAL )
            {
                continue;
            }
            path = g_build_filename( cwd, fi->name, NULL );
            esc_path = bash_quote( path );
            fprintf( file, "%s\n", esc_path );
            g_free( esc_path );
            g_free( path );
        }
        fputs( ")\n", file );

        fprintf( file, "fm_filenames=(\n" );
        for ( l = sel_files; l; l = l->next )
        {
            fi = (VFSFileInfo*)l->data;
            if ( fi->flags & VFS_FILE_INFO_VIRTUAL )
            {
                continue;
            }
            esc_path = bash_quote( fi->name );
            fprintf( file, "%s\n", esc_path );
            g_free( esc_path );
        }
        fputs( ")\n", file );

        g_list_foreach( sel_files, (GFunc)vfs_file_info_unref, NULL );
        g_list_free( sel_files );
    }

    // my selected files 
    esc_path = bash_quote( cwd );
    fprintf( file, "fm_pwd=%s\n", esc_path );
    fprintf( file, "fm_desktop_pwd=%s\n", esc_path );
    g_free( esc_path );
    fprintf( file, "\nfm_files=(\"${fm_desktop_files[@]}\")\n" );
    fprintf( file, "fm_file=\"${fm_files[0]}\"\n" );
    fprintf( file, "fm_filename=\"${fm_filenames[0]}\"\n" );

    // command
    if ( vtask->exec_command )
    {
        esc_path = bash_quote( vtask->exec_command );
        fprintf( file, "fm_command=%s\n", esc_path );
        g_free( esc_path );
    }
    // user
    const char* this_user = g_get_user_name();
    if ( this_user )
    {
        esc_path = bash_quote( this_user );
        fprintf( file, "fm_user=%s\n", esc_path );
        g_free( esc_path );
        //g_free( this_user );  DON'T
    }
    // variable value
    if ( value )
    {
        esc_path = bash_quote( value );
        fprintf( file, "fm_value=%s\n", esc_path );
        g_free( esc_path );
    }

    // set
    if ( set )
    {
        // cmd_dir
        if ( set->plugin )
        {
            path = g_build_filename( set->plug_dir, "files", NULL );
            if ( !g_file_test( path, G_FILE_TEST_EXISTS ) )
            {
                g_free( path );
                path = g_build_filename( set->plug_dir, set->plug_name, NULL );
            }
        }
        else
        {
            path = g_build_filename( xset_get_config_dir(), "scripts",
                                                            set->name, NULL );
        }
        esc_path = bash_quote( path );
        fprintf( file, "fm_cmd_dir=%s\n", esc_path );
        g_free( esc_path );
        g_free( path );

        // cmd_data
        if ( set->plugin )
        {
            XSet* mset = xset_get_plugin_mirror( set );
            path = g_build_filename( xset_get_config_dir(), "plugin-data",
                                                        mset->name, NULL );
        }
        else
            path = g_build_filename( xset_get_config_dir(), "plugin-data",
                                                        set->name, NULL );
        esc_path = bash_quote( path );
        fprintf( file, "fm_cmd_data=%s\n", esc_path );
        g_free( esc_path );
        g_free( path );
        
        // plugin_dir
        if ( set->plugin )
        {
            esc_path = bash_quote( set->plug_dir );
            fprintf( file, "fm_plugin_dir=%s\n", esc_path );
            g_free( esc_path );
        }
        
        // cmd_name
        if ( set->menu_label )
        {
            esc_path = bash_quote( set->menu_label );
            fprintf( file, "fm_cmd_name=%s\n", esc_path );
            g_free( esc_path );
        }
    }

    // tmp
    if ( geteuid() != 0 && vtask->exec_as_user
                                    && !strcmp( vtask->exec_as_user, "root" ) )
        fprintf( file, "fm_tmp_dir=%s\n", xset_get_shared_tmp_dir() );
    else
        fprintf( file, "fm_tmp_dir=%s\n", xset_get_user_tmp_dir() );


    result = fputs( "\n", file );
    return result >= 0;
}

