/*
* SpaceFM desktop-window.c
*
* Copyright (C) 2014 IgnorantGuru <ignorantguru@gmx.com>
* Copyright (C) 2012 BwackNinja <bwackninja@gmail.com>
* Copyright (C) 2008 Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>
*
* License: See COPYING file
*
*/

#include <stdlib.h>
#include <glib/gi18n.h>
#include <math.h>  // sqrt

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
#include "item-prop.h"
#include "main-window.h"
#include "pref-dialog.h"
#include "ptk-file-browser.h"
#include "ptk-clipboard.h"
#include "ptk-file-archiver.h"
#include "ptk-location-view.h"
#include "ptk-app-chooser.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <fnmatch.h>

#if GTK_CHECK_VERSION (3, 0, 0)
#include <cairo-xlib.h>
#endif

#include <string.h>

/* for stat */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "gtk2-compat.h"

struct _DesktopItem
{
    VFSFileInfo* fi;    // empty box if NULL
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

#if GTK_CHECK_VERSION (3, 0, 0)
static gboolean on_draw( GtkWidget* w, cairo_t* cr );
static void desktop_window_get_preferred_width (GtkWidget *widget, gint *minimal_width, gint *natural_width);
static void desktop_window_get_preferred_height (GtkWidget *widget, gint *minimal_height, gint *natural_height);
#else
static gboolean on_expose( GtkWidget* w, GdkEventExpose* evt );
#endif
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

static void on_settings( GtkMenuItem *menuitem, DesktopWindow* self );

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

static void redraw_item( DesktopWindow* win, DesktopItem* item );
static void desktop_item_free( DesktopItem* item );

GList* desktop_window_get_selected_items( DesktopWindow* win,
                                          gboolean current_first );

/*
static GdkPixmap* get_root_pixmap( GdkWindow* root );
static gboolean set_root_pixmap(  GdkWindow* root , GdkPixmap* pix );
*/

static DesktopItem* hit_test( DesktopWindow* self, int x, int y );
static DesktopItem* hit_test_icon( DesktopWindow* self, int x, int y );
static gboolean hit_test_text( DesktopWindow* self, int x, int y,
                                                    DesktopItem** next_item );
static DesktopItem* hit_test_box( DesktopWindow* self, int x, int y );

static void custom_order_write( DesktopWindow* self );
static GHashTable* custom_order_read( DesktopWindow* self );
//static gboolean on_configure_event( GtkWidget* w, GdkEventConfigure *event );

/* static Atom ATOM_XROOTMAP_ID = 0; */
static Atom ATOM_NET_WORKAREA = 0;

/* Local data */
static GtkWindowClass *parent_class = NULL;

/*static GdkPixmap* pix = NULL;*/

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

//sfm from ptk-file-icon-renderer.c
static GdkPixbuf* link_icon = NULL;
/* GdkPixbuf RGBA C-Source image dump */
#ifdef __SUNPRO_C
#pragma align 4 (link_icon_data)
#endif
#ifdef __GNUC__
static const guint8 link_icon_data[] __attribute__ ((__aligned__ (4))) =
#else
static const guint8 link_icon_data[] =
#endif
    { ""
      /* Pixbuf magic (0x47646b50) */
      "GdkP"
      /* length: header (24) + pixel_data (400) */
      "\0\0\1\250"
      /* pixdata_type (0x1010002) */
      "\1\1\0\2"
      /* rowstride (40) */
      "\0\0\0("
      /* width (10) */
      "\0\0\0\12"
      /* height (10) */
      "\0\0\0\12"
      /* pixel_data: */
      "\200\200\200\377\200\200\200\377\200\200\200\377\200\200\200\377\200"
      "\200\200\377\200\200\200\377\200\200\200\377\200\200\200\377\200\200"
      "\200\377\0\0\0\377\200\200\200\377\377\377\377\377\377\377\377\377\377"
      "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
      "\377\377\377\377\377\377\0\0\0\377\200\200\200\377\377\377\377\377\0"
      "\0\0\377\0\0\0\377\0\0\0\377\0\0\0\377\0\0\0\377\377\377\377\377\377"
      "\377\377\377\0\0\0\377\200\200\200\377\377\377\377\377\0\0\0\377\0\0"
      "\0\377\0\0\0\377\0\0\0\377\377\377\377\377\377\377\377\377\377\377\377"
      "\377\0\0\0\377\200\200\200\377\377\377\377\377\0\0\0\377\0\0\0\377\0"
      "\0\0\377\0\0\0\377\0\0\0\377\377\377\377\377\377\377\377\377\0\0\0\377"
      "\200\200\200\377\377\377\377\377\0\0\0\377\0\0\0\377\0\0\0\377\0\0\0"
      "\377\0\0\0\377\0\0\0\377\377\377\377\377\0\0\0\377\200\200\200\377\377"
      "\377\377\377\0\0\0\377\377\377\377\377\377\377\377\377\0\0\0\377\0\0"
      "\0\377\0\0\0\377\377\377\377\377\0\0\0\377\200\200\200\377\377\377\377"
      "\377\377\377\377\377\377\377\377\377\377\377\377\377\0\0\0\377\0\0\0"
      "\377\0\0\0\377\377\377\377\377\0\0\0\377\200\200\200\377\377\377\377"
      "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
      "\377\377\377\377\377\377\377\377\377\377\377\377\0\0\0\377\0\0\0\377"
      "\0\0\0\377\0\0\0\377\0\0\0\377\0\0\0\377\0\0\0\377\0\0\0\377\0\0\0\377"
      "\0\0\0\377\0\0\0\377"
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

#if GTK_CHECK_VERSION (3, 0, 0)
    wc->draw = on_draw;
    wc->get_preferred_width = desktop_window_get_preferred_width;
    wc->get_preferred_height = desktop_window_get_preferred_height;
#else
    wc->expose_event = on_expose;
    wc->size_request = on_size_request;
#endif
    wc->size_allocate = on_size_allocate;
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
    //wc->configure_event = on_configure_event;

    parent_class = (GtkWindowClass*)g_type_class_peek(GTK_TYPE_WINDOW);

    /* ATOM_XROOTMAP_ID = XInternAtom( GDK_DISPLAY(),"_XROOTMAP_ID", False ); */
    ATOM_NET_WORKAREA = XInternAtom( gdk_x11_get_default_xdisplay(),"_NET_WORKAREA", False );

    text_uri_list_atom = gdk_atom_intern_static_string( drag_targets[DRAG_TARGET_URI_LIST].target );
    desktop_icon_atom = gdk_atom_intern_static_string( drag_targets[DRAG_TARGET_DESKTOP_ICON].target );

    /*  on emit, desktop window is not an object so this doesn't work
    g_signal_new ( "task-notify",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       0,
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1, G_TYPE_POINTER );
    */
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

    self->label_w = 100;
    self->icon_size = 48;
    self->spacing = 0;
    self->x_pad = app_settings.margin_pad;
    self->y_pad = app_settings.margin_pad;
    self->margin_top = app_settings.margin_top;
    self->margin_left = app_settings.margin_left;
    self->margin_right = app_settings.margin_right;
    self->margin_bottom = app_settings.margin_bottom;
    self->insert_item = NULL;
    self->renamed_item = self->renaming_item = NULL;
    self->file_listed = FALSE;

    self->icon_render = gtk_cell_renderer_pixbuf_new();
    g_object_set( self->icon_render, "follow-state", TRUE, NULL);
    g_object_ref_sink(self->icon_render);
    pc = gtk_widget_get_pango_context( (GtkWidget*)self );
    self->pl = gtk_widget_create_pango_layout( (GtkWidget*)self, NULL );
    pango_layout_set_alignment( self->pl, PANGO_ALIGN_CENTER );
    pango_layout_set_wrap( self->pl, PANGO_WRAP_WORD_CHAR );
    pango_layout_set_width( self->pl, self->label_w * PANGO_SCALE );

    metrics = pango_context_get_metrics(
                            pc, gtk_widget_get_style( ((GtkWidget*)self) )->font_desc,
                            pango_context_get_language(pc));

    font_h = pango_font_metrics_get_ascent(metrics) + pango_font_metrics_get_descent (metrics);
    font_h /= PANGO_SCALE;

    if ( !link_icon )
    {
        link_icon = gdk_pixbuf_new_from_inline(
                sizeof(link_icon_data),
                link_icon_data,
                FALSE, NULL );
        g_object_add_weak_pointer( G_OBJECT(link_icon), (gpointer)&link_icon  );
    }
    else
        g_object_ref( (link_icon) );

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
    gtk_widget_set_can_focus ( (GtkWidget*)self, TRUE );

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

    //g_signal_connect( G_OBJECT( self ), "task-notify",
    //                            G_CALLBACK( ptk_file_task_notify_handler ), NULL );
}


GtkWidget* desktop_window_new(void)
{
    GtkWindow* w = (GtkWindow*)g_object_new(DESKTOP_WINDOW_TYPE, NULL);
    return (GtkWidget*)w;
}

void desktop_item_free( DesktopItem* item )
{
    if ( item->fi )
        vfs_file_info_unref( item->fi );
    g_slice_free( DesktopItem, item );
}

void desktop_window_finalize(GObject *object)
{
    DesktopWindow *self = (DesktopWindow*)object;
    Display *xdisplay = GDK_DISPLAY_XDISPLAY( gtk_widget_get_display( (GtkWidget*)object) );

    g_return_if_fail(object != NULL);
    g_return_if_fail(IS_DESKTOP_WINDOW(object));

    gdk_window_remove_filter(
                    gdk_screen_get_root_window( gtk_widget_get_screen( (GtkWidget*)object) ),
                    on_rootwin_event, self );

#if GTK_CHECK_VERSION (3, 0, 0)
    if( self->background )
        XFreePixmap ( xdisplay, self->background );

    if( self->surface )
        cairo_surface_destroy ( self->surface );
#else
    if( self->background )
        g_object_unref( self->background );
#endif

    if( self->hand_cursor )
        gdk_cursor_unref( self->hand_cursor );

    if ( link_icon )
        g_object_unref( link_icon );

    self = DESKTOP_WINDOW(object);
    if ( self->dir )
        g_object_unref( self->dir );

    g_list_foreach( self->items, (GFunc)desktop_item_free, NULL );
    g_list_free( self->items );

    if (G_OBJECT_CLASS(parent_class)->finalize)
        (* G_OBJECT_CLASS(parent_class)->finalize)(object);
}

/*--------------- Signal handlers --------------*/

#if GTK_CHECK_VERSION (3, 0, 0)
gboolean on_draw( GtkWidget* w, cairo_t* cr)
#else
gboolean on_expose( GtkWidget* w, GdkEventExpose* evt )
#endif
{
    DesktopWindow* self = (DesktopWindow*)w;
    GList* l;
    GdkRectangle intersect;
#if GTK_CHECK_VERSION (3, 0, 0)
    GtkAllocation allocation;
    gtk_widget_get_allocation (w, &allocation);
#endif

    if( G_UNLIKELY( ! gtk_widget_get_visible (w) || ! gtk_widget_get_mapped (w) ) )
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
#if GTK_CHECK_VERSION (3, 0, 0)
        if( gdk_rectangle_intersect( &allocation, &item->box, &intersect ) )
            paint_item( self, item, &intersect );
#else
        if( gdk_rectangle_intersect( &evt->area, &item->box, &intersect ) )
            paint_item( self, item, &intersect );
#endif
    }
    return TRUE;
}

void on_size_allocate( GtkWidget* w, GtkAllocation* alloc )
{
    //printf("on_size_allocate  %p  x,y=%d, %d    w,h=%d, %d\n", w, alloc->x,
    //                                alloc->y, alloc->width, alloc->height);
    GdkPixbuf* pix;
    DesktopWindow* self = (DesktopWindow*)w;

    get_working_area( gtk_widget_get_screen(w), &self->wa );
    if ( self->sort_by == DW_SORT_CUSTOM )
        self->order_rows = self->row_count; // possible change of row count in new layout
    layout_items( DESKTOP_WINDOW(w) );

    GTK_WIDGET_CLASS(parent_class)->size_allocate( w, alloc );
}

void desktop_window_set_bg_color( DesktopWindow* win, GdkColor* clr )
{
    if( clr )
    {
        win->bg = *clr;
// Allocating colors is unnecessary with gtk3
#if !GTK_CHECK_VERSION (3, 0, 0)
        gdk_colormap_alloc_color ( gtk_widget_get_colormap( (GtkWidget*)win ),
                            &win->bg, FALSE, TRUE );
#endif
        if( gtk_widget_get_visible( (GtkWidget*)win ) )
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
// Allocating colors is unnecessary with gtk3
#if !GTK_CHECK_VERSION (3, 0, 0)
            gdk_colormap_alloc_color ( gtk_widget_get_colormap( (GtkWidget*)win ),
                                &win->fg, FALSE, TRUE );
#endif
        }
        if( shadow )
        {
            win->shadow = *shadow;
// Allocating colors is unnecessary with gtk3
#if !GTK_CHECK_VERSION (3, 0, 0)
            gdk_colormap_alloc_color ( gtk_widget_get_colormap( (GtkWidget*)win ),
                                &win->shadow, FALSE, TRUE );
#endif
        }
        if( gtk_widget_get_visible( (GtkWidget*)win ) )
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
#if GTK_CHECK_VERSION (3, 0, 0)
    Pixmap pixmap = 0;
    cairo_surface_t *surface = NULL;
    cairo_pattern_t *pattern = NULL;
#else
    GdkPixmap* pixmap = NULL;
#endif
    Display* xdisplay;
    Pixmap xpixmap = 0;
    Visual *xvisual;
    Window xroot;
    cairo_t *cr;
    int dummy;
    unsigned int udummy, depth;

    /* set root map here */
    xdisplay = GDK_DISPLAY_XDISPLAY( gtk_widget_get_display( (GtkWidget*)win) );
    XGetGeometry (xdisplay, GDK_WINDOW_XID( gtk_widget_get_window( (GtkWidget*)win ) ),
                  &xroot, &dummy, &dummy, &dummy, &dummy, &udummy, &depth);
    xvisual = GDK_VISUAL_XVISUAL (gdk_screen_get_system_visual ( gtk_widget_get_screen ( (GtkWidget*)win) ) );

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
#if GTK_CHECK_VERSION (3, 0, 0)
            pixmap = XCreatePixmap(xdisplay, xroot, src_w, src_h, depth);
            surface = cairo_xlib_surface_create (xdisplay, pixmap, xvisual,
                                                 src_w, src_h);
            cr = cairo_create ( surface );
#else
            pixmap = gdk_pixmap_new( gtk_widget_get_window( ((GtkWidget*)win) ), src_w, src_h, -1 );
            cr = gdk_cairo_create ( pixmap );
#endif
            gdk_cairo_set_source_pixbuf ( cr, src_pix, 0, 0 );
            cairo_paint ( cr );
        }
        else
        {
            int src_x = 0, src_y = 0;
            int dest_x = 0, dest_y = 0;
            int w = 0, h = 0;

#if GTK_CHECK_VERSION (3, 0, 0)
            pixmap = XCreatePixmap(xdisplay, xroot, dest_w, dest_h, depth);
            surface = cairo_xlib_surface_create (xdisplay, pixmap, xvisual,
                                                 dest_w, dest_h);
            cr = cairo_create ( surface );
#else
            pixmap = gdk_pixmap_new( gtk_widget_get_window( ((GtkWidget*)win) ), dest_w, dest_h, -1 );
            cr = gdk_cairo_create ( pixmap );
#endif
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
            case DW_BG_ZOOM:
                if( src_w == dest_w && src_h == dest_h )
                    scaled = (GdkPixbuf*)g_object_ref( src_pix );
                else
                {
                    gdouble w_ratio = (float)dest_w / src_w;
                    gdouble h_ratio = (float)dest_h / src_h;

                    gdouble ratio;
                    if (type == DW_BG_FULL)
                        ratio = MIN( w_ratio, h_ratio );
                    else
                        ratio = MAX( w_ratio, h_ratio );

                    if( ratio == 1.0 )
                        scaled = (GdkPixbuf*)g_object_ref( src_pix );
                    else
                        scaled = gdk_pixbuf_scale_simple( src_pix, (src_w * ratio), (src_h * ratio), GDK_INTERP_BILINEAR );
                }
                w = gdk_pixbuf_get_width( scaled );
                h = gdk_pixbuf_get_height( scaled );

                dest_x = (dest_w - w) / 2;
                dest_y = (dest_h - h) / 2;
                break;
            case DW_BG_CENTER:
                scaled = (GdkPixbuf*)g_object_ref( src_pix );
                if( src_w > dest_w )
                    w = dest_w;
                else
                    w = src_w;
                if( src_h > dest_h )
                    h = dest_h;
                else
                    h = src_h;
                dest_x = (dest_w - src_w) / 2;
                dest_y = (dest_h - src_h) / 2;
                break;
            }

            if( scaled )
            {
                if( w != dest_w || h != dest_h )
                {
                    gdk_cairo_set_source_color ( cr, &win->bg );
                    cairo_rectangle ( cr, 0, 0, dest_w, dest_h );
                    cairo_paint ( cr );
                }
                gdk_cairo_set_source_pixbuf ( cr, scaled, dest_x, dest_y );
                cairo_move_to ( cr, src_x, src_y );
                cairo_paint ( cr );
                g_object_unref( scaled );
            }
            else
            {
#if GTK_CHECK_VERSION (3, 0, 0)
                XFreePixmap ( xdisplay, pixmap );
                pixmap = 0;
#else
                g_object_unref( pixmap );
                pixmap = NULL;
#endif
            }
        }
        cairo_destroy ( cr );
    }

#if GTK_CHECK_VERSION (3, 0, 0)
    if( win->background )
        XFreePixmap ( xdisplay, win->background );

    if( win->surface )
        cairo_surface_destroy ( win->surface );

    win->surface = surface;        
#else
    if( win->background )
        g_object_unref( win->background );
#endif
    win->background = pixmap;


    if( pixmap )
    {
#if GTK_CHECK_VERSION (3, 0, 0)
        pattern = cairo_pattern_create_for_surface( surface );
        cairo_pattern_set_extend( pattern, CAIRO_EXTEND_REPEAT );
        gdk_window_set_background_pattern( gtk_widget_get_window( ((GtkWidget*)win) ), pattern );
        cairo_pattern_destroy( pattern );
#else
        gdk_window_set_back_pixmap( gtk_widget_get_window( ((GtkWidget*)win) ), pixmap, FALSE );
#endif
    }
    else
        gdk_window_set_background( gtk_widget_get_window( ((GtkWidget*)win) ), &win->bg );

#if !GTK_CHECK_VERSION (3, 0, 0)
    gdk_window_clear( gtk_widget_get_window( ((GtkWidget*)win) ) );
#else
/*    cairo_t *cr2 = gdk_cairo_create( gtk_widget_get_window ((GtkWidget*)win) );
    gdk_cairo_set_source_color( cr2, &win->bg );
    cairo_paint( cr2 );
    if ( surface )
    {
        cairo_set_source_surface( cr2, surface, 0, 0 );
        cairo_paint( cr2 );
    }
    cairo_destroy(cr2);*/
#endif
    gtk_widget_queue_draw( (GtkWidget*)win );

    XGrabServer (xdisplay);

    if( pixmap )
    {
#if GTK_CHECK_VERSION (3, 0, 0)
        xpixmap = pixmap;
#else
        xpixmap = GDK_WINDOW_XID(pixmap);
#endif

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
    win->x_pad = app_settings.margin_pad;
    win->y_pad = app_settings.margin_pad;
    win->margin_top = app_settings.margin_top;
    win->margin_left = app_settings.margin_left;
    win->margin_right = app_settings.margin_right;
    win->margin_bottom = app_settings.margin_bottom;
    if ( win->sort_by == DW_SORT_CUSTOM )
        win->order_rows = win->row_count; // possible change of row count in new layout
    
    layout_items( win );

    for( l = win->items; l; l = l->next )
    {
        VFSFileInfo* fi = ((DesktopItem*)l->data)->fi;
        if ( !fi )
            continue;
        char* path;
        /* reload the icons for special items if needed */
        if ( (fi->flags & VFS_FILE_INFO_DESKTOP_ENTRY) && !fi->big_thumbnail )
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
            fi->big_thumbnail = gtk_icon_theme_load_icon(
                                            gtk_icon_theme_get_default(),
                                            "gnome-fs-home", size, 0, NULL );
            if( ! fi->big_thumbnail )
                fi->big_thumbnail = gtk_icon_theme_load_icon(
                                            gtk_icon_theme_get_default(),
                                            "folder-home", size, 0, NULL );
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

    if ( win->sort_by == DW_SORT_CUSTOM )
        win->order_rows = win->row_count; // possible change of row count in new layout
     layout_items( win );
}



void on_size_request( GtkWidget* w, GtkRequisition* req )
{
    GdkScreen* scr = gtk_widget_get_screen( w );
    req->width = gdk_screen_get_width( scr );
    req->height = gdk_screen_get_height( scr );
    //printf("on_size_request  %p  w,h=%d, %d\n", w, req->width, req->height );
}

#if GTK_CHECK_VERSION (3, 0, 0)
static void
desktop_window_get_preferred_width (GtkWidget *widget,
                                    gint      *minimal_width,
                                    gint      *natural_width)
{
  GtkRequisition requisition;

  on_size_request (widget, &requisition);

  *minimal_width = *natural_width = requisition.width;
}

static void
desktop_window_get_preferred_height (GtkWidget *widget,
                                     gint      *minimal_height,
                                     gint      *natural_height)
{
  GtkRequisition requisition;

  on_size_request (widget, &requisition);

  *minimal_height = *natural_height = requisition.height;
}
#endif

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
    cairo_t *cr;

    calc_rubber_banding_rect( self, self->rubber_bending_x, self->rubber_bending_y, &rect );

    if( rect.width <= 0 || rect.height <= 0 )
        return;
/*
    gtk_widget_style_get( GTK_WIDGET(self),
                        "selection-box-color", &clr,
                        "selection-box-alpha", &alpha,
                        NULL);
*/

    cr = gdk_cairo_create ( gtk_widget_get_window( ((GtkWidget*)self) ) );
    clr = gdk_color_copy (&gtk_widget_get_style( GTK_WIDGET (self) )->base[GTK_STATE_SELECTED]);
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
#if GTK_CHECK_VERSION (3, 0, 0)
            pix = gdk_pixbuf_get_from_surface( self->surface,
                                                rect.x, rect.y, rect.width, rect.height );
#else
            pix = gdk_pixbuf_get_from_drawable( NULL, self->background, gdk_drawable_get_colormap(self->background),
                                                rect.x, rect.y, 0, 0, rect.width, rect.height );
#endif
    }

    if( pix )
    {
        colorize_pixbuf( pix, clr, alpha );
        if( self->bg_type == DW_BG_TILE ) /* this is currently unreachable */
        {
            /*GdkPixmap* pattern;*/
            /* FIXME: This is damn slow!! */
            /*pattern = gdk_pixmap_new( gtk_widget_get_window( ((GtkWidget*)self) ), pattern_w, pattern_h, -1 );
            if( pattern )
            {
                gdk_draw_pixbuf( pattern, gc, pix, 0, 0,
                                 0, 0, pattern_w, pattern_h, GDK_RGB_DITHER_NONE, 0, 0 );
                gdk_gc_set_tile( gc, pattern );
                gdk_gc_set_fill( gc, GDK_TILED );
        gdk_draw_rectangle( gtk_widget_get_window( ((GtkWidget*)self) ), gc, TRUE,
                            rect.x, rect.y, rect.width-1, rect.height-1 );
                g_object_unref( pattern );
                gdk_gc_set_fill( gc, GDK_SOLID );
            } */
        }
        else
        {
            gdk_cairo_set_source_pixbuf( cr, pix, rect.x, rect.y );
            cairo_rectangle( cr, rect.x, rect.y, rect.width, rect.height );
            cairo_fill( cr );
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
        gdk_cairo_set_source_color( cr, &clr2 );
        cairo_rectangle( cr, rect.x, rect.y, rect.width - 1, rect.height - 1 );
        cairo_fill( cr );
    }

    /* draw the border */
    gdk_cairo_set_source_color( cr, clr );
    cairo_rectangle( cr, rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2 );
    cairo_stroke( cr );

    gdk_color_free (clr);
    cairo_destroy( cr );
}

static void update_rubberbanding( DesktopWindow* self, int newx, int newy, gboolean add )
{
    GList* l;
    GdkRectangle old_rect, new_rect;
/*
#if GTK_CHECK_VERSION (3, 0, 0)
    cairo_region_t *region;
#else
    GdkRegion *region;
#endif
*/
    // Calc new region
    calc_rubber_banding_rect(self, self->rubber_bending_x, self->rubber_bending_y, &old_rect );
    calc_rubber_banding_rect(self, newx, newy, &new_rect );

    // Trigger redraw of region
    gdk_window_invalidate_rect(gtk_widget_get_window( ((GtkWidget*)self) ), &old_rect, FALSE );
    gdk_window_invalidate_rect(gtk_widget_get_window( ((GtkWidget*)self) ), &new_rect, FALSE );

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
        if( item->fi &&
            ( gdk_rectangle_intersect( &new_rect, &item->icon_rect, NULL ) ||
              gdk_rectangle_intersect( &new_rect, &item->text_rect, NULL ) ) )
            selected = TRUE;
        else
            selected = FALSE;

        if( ( item->is_selected != selected ) && ( !add || !item->is_selected ) )
        {
            item->is_selected = selected;
            redraw_item( self, item );
        }
    }
}

static void open_clicked_item( DesktopWindow* self, DesktopItem* clicked_item )
{
    char* path = NULL;

    if ( !clicked_item->fi )
        return;
    /* this won't work yet because desktop context_fill doesn't do selected 
     * else if ( xset_opener( self, NULL, 1 ) )
        return;
    */
    else if ( vfs_file_info_is_dir( clicked_item->fi ) &&
                                                !app_settings.desk_open_mime )
    {
        // a folder - open in SpaceFM browser by default
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
            if ( mime_type && !vfs_file_info_is_dir( file ) && (
                    !strcmp( vfs_mime_type_get_type( mime_type ), "application/x-cd-image" ) ||
                    !strcmp( vfs_mime_type_get_type( mime_type ), "application/x-iso9660-image" ) ||
                    g_str_has_suffix( vfs_file_info_get_name( file ), ".iso" ) ||
                    g_str_has_suffix( vfs_file_info_get_name( file ), ".img" ) ) )
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

void show_desktop_menu( DesktopWindow* self, guint event_button,
                                                guint32 event_time )
{
    GtkMenu* popup;
    DesktopItem *item;
    GList* l;

    GList* sel = desktop_window_get_selected_items( self, TRUE );
    if ( sel )
    {
        item = (DesktopItem*)sel->data;
        char* file_path = g_build_filename( vfs_get_desktop_dir(),
                                            item->fi->name, NULL );
        for ( l = sel; l; l = l->next )
            l->data = vfs_file_info_ref( ((DesktopItem*)l->data)->fi );
        popup = GTK_MENU( ptk_file_menu_new( self, NULL, file_path,
                            item->fi, vfs_get_desktop_dir(), sel ) );
        g_free( file_path );
    }
    else
        popup = GTK_MENU( ptk_file_menu_new( self, NULL, NULL, NULL,
                                    vfs_get_desktop_dir(), NULL ) );
    gtk_menu_popup( popup, NULL, NULL, NULL, NULL, event_button,
                                                   event_time );
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
            /* don't cancel selection if clicking on selected items OR
             * clicking on desktop with right button 
            if( !( (evt->button == 1 || evt->button == 3 || evt->button == 0)
                                && clicked_item && clicked_item->is_selected)
                        && !( !clicked_item && evt->button == 3 ) ) */
            if ( !( ( evt->button == 1 || evt->button == 3 || evt->button == 0 )
                                && clicked_item && clicked_item->is_selected ) )
                desktop_window_select( self, DW_SELECT_NONE );
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
                GList* sel = desktop_window_get_selected_items( self, TRUE );
                if( sel )
                {
                    item = (DesktopItem*)sel->data;
                    GtkMenu* popup;
                    GList* l;
                    char* file_path = g_build_filename( vfs_get_desktop_dir(),
                                                        item->fi->name, NULL );
                    for( l = sel; l; l = l->next )
                        l->data = vfs_file_info_ref( ((DesktopItem*)l->data)->fi );
                    popup = GTK_MENU(ptk_file_menu_new( self, NULL, file_path,
                                        item->fi, vfs_get_desktop_dir(), sel ));
                    g_free( file_path );

                    gtk_menu_popup( popup, NULL, NULL, NULL, NULL, evt->button,
                                                                   evt->time );
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
                    show_desktop_menu( self, evt->button, evt->time );
                    goto out;   // don't forward the event to root win
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
            open_clicked_item( self, clicked_item );
            goto out;
        }
    }
    /* forward the event to root window */
    forward_event_to_rootwin( gtk_widget_get_screen(w), (GdkEvent*)evt );

out:
    if( ! gtk_widget_has_focus(w) )
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
        update_rubberbanding( self, evt->x, evt->y,
                                        !!(evt->state & GDK_CONTROL_MASK) );
        gtk_grab_remove( w );
        self->rubber_bending = FALSE;
    }
    else if( self->dragging )
    {
        self->dragging = FALSE;
    }
    else if ( evt->button == 1 && !( evt->state & 
                      ( GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK ) ) )
    {
        // unmodified left click release
        if ( self->single_click )
        {
            if ( clicked_item )
            {
                open_clicked_item( self, clicked_item );
                return TRUE;
            }
        }
        else
        {
            desktop_window_select( self, DW_SELECT_NONE );
            if ( clicked_item )
            {
                clicked_item->is_selected = TRUE;
                redraw_item( self, clicked_item );
            }
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
    evt.window = gtk_widget_get_window(w);
    gdk_window_get_pointer( gtk_widget_get_window(w), &x, &y, &evt.state );
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
                gdk_window_set_cursor( gtk_widget_get_window(w), self->hand_cursor );
                /* FIXME: timeout should be customizable */
                if( !app_settings.desk_no_single_hover &&
                                    0 == self->single_click_timeout_handler )
                    self->single_click_timeout_handler = 
                                    g_timeout_add( SINGLE_CLICK_TIMEOUT,
                                    (GSourceFunc)on_single_click_timeout, self );
            }
            else
            {
                gdk_window_set_cursor( gtk_widget_get_window(w), NULL );
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
        update_rubberbanding( self, evt->x, evt->y, !!(evt->state & GDK_CONTROL_MASK) );
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
            GList* sels = desktop_window_get_selected_items(self, FALSE);

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

    if( G_LIKELY(gdk_drag_context_list_targets( ctx ) ) )
    {
        if( gdk_drag_context_get_selected_action( ctx ) != GDK_ACTION_MOVE )
            expected_target = text_uri_list_atom;
        else
        {
            if ( self->sort_by == DW_SORT_CUSTOM )
                item = hit_test_icon( self, x, y );
            else
                item = hit_test( self, x, y );
            if( item )  /* drag over a desktpo item */
            {
                GList* sels;
                sels = desktop_window_get_selected_items( self, FALSE );
                /* drag over the selected items themselves */
                if( g_list_find( sels, item ) )
                    expected_target = desktop_icon_atom;
                else
                    expected_target = text_uri_list_atom;
                g_list_free( sels );
            }
            else    /* drag over blank area, check if it's a desktop icon first. */
            {
                if( g_list_find( gdk_drag_context_list_targets( ctx ), GUINT_TO_POINTER(desktop_icon_atom) ) )
                    return desktop_icon_atom;
                expected_target = text_uri_list_atom;
            }
        }
        if( g_list_find( gdk_drag_context_list_targets( ctx ), GUINT_TO_POINTER(expected_target) ) )
            return expected_target;
    }
    return GDK_NONE;
}

#define GDK_ACTION_ALL  (GDK_ACTION_MOVE|GDK_ACTION_COPY|GDK_ACTION_LINK)

gboolean on_drag_motion( GtkWidget* w, GdkDragContext* ctx, gint x, gint y, guint time )
{
    DesktopWindow* self = (DesktopWindow*)w;
    //DesktopItem* item;
    //GdkAtom target;

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

    if( g_list_find( gdk_drag_context_list_targets( ctx ), GUINT_TO_POINTER(text_uri_list_atom) ) )
    {
        GdkDragAction suggested_action = 0;
        /* Only 'move' is available. The user force move action by pressing Shift key */
        if( (gdk_drag_context_get_actions( ctx ) & GDK_ACTION_ALL) == GDK_ACTION_MOVE )
            suggested_action = GDK_ACTION_MOVE;
        /* Only 'copy' is available. The user force copy action by pressing Ctrl key */
        else if( (gdk_drag_context_get_actions( ctx ) & GDK_ACTION_ALL) == GDK_ACTION_COPY )
            suggested_action = GDK_ACTION_COPY;
        /* Only 'link' is available. The user force link action by pressing Shift+Ctrl key */
        else if( (gdk_drag_context_get_actions( ctx ) & GDK_ACTION_ALL) == GDK_ACTION_LINK )
            suggested_action = GDK_ACTION_LINK;
        /* Several different actions are available. We have to figure out a good default action. */
        else
        {
            if( get_best_target_at_dest(self, ctx, x, y ) == text_uri_list_atom )
            {
                // move to icon - check the status of drop site
                self->pending_drop_action = TRUE;
                self->drag_pending_x = x;
                self->drag_pending_y = y;
                gtk_drag_get_data( w, ctx, text_uri_list_atom, time );
                return TRUE;
            }
            else
            {
                // move to desktop
                suggested_action = GDK_ACTION_MOVE;
            }
        }
        gdk_drag_status( ctx, suggested_action, time );
    }
    else if( g_list_find( gdk_drag_context_list_targets( ctx ), GUINT_TO_POINTER(desktop_icon_atom) ) ) /* moving desktop icon */
    {
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
    //printf("DROP: %s\n", gdk_atom_name(target) );
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

void move_desktop_items( DesktopWindow* self, GdkDragContext* ctx, 
                                                    DesktopItem* target_item )
{
    DesktopItem* item;
    GList* l, *ll, *target_l = NULL;

    if ( target_item && !( target_l = g_list_find( self->items, target_item ) ) )
        return;

    GList* sel_items = desktop_window_get_selected_items( self, FALSE );
    if ( !sel_items )
        return;

    if ( target_item && g_list_find( sel_items, target_item ) )
    {
        // dropped selection onto self
        g_list_free( sel_items );
        return;
    }
    
    // sort selected items by name - don't change user's order of icons ?
    //sel_items = g_list_sort_with_data( sel_items,
    //                                (GCompareDataFunc)comp_item_by_name, NULL );   

    for ( l = sel_items; l; l = l->next )
    {
        if ( !( ll = g_list_find( self->items, l->data ) ) )
            continue;  // not in list failsafe
        if ( !target_l )
        {
            // no target - add to end
            //printf( "NO TARGET\n" );
            ll->data = g_slice_new0( DesktopItem );  // new empty
            ((DesktopItem*)ll->data)->fi = NULL;
            self->items = g_list_append( self->items, l->data );
        }
        else if ( !((DesktopItem*)target_l->data)->fi )
        {
            // target is empty, swap them
            //printf( "SWAP %p -> %p\n", l->data, target_l->data );
            ll->data = target_l->data;
            target_l->data = l->data;
            target_l = target_l->next;
        }
        else
        {
            // target is not empty, insert before
            //printf( "INSERT %p before %p\n", l->data, target_l->data );
            ll->data = g_slice_new0( DesktopItem );  // new empty
            ((DesktopItem*)ll->data)->fi = NULL;
            self->items = g_list_insert_before( self->items, target_l, l->data );
            // scan downward and remove first empty
            for ( ll = target_l->next; ll; ll = ll->next )
            {
                if ( !((DesktopItem*)ll->data)->fi )
                {
                    desktop_item_free( (DesktopItem*)ll->data );
                    self->items = g_list_remove( self->items, ll->data );
                    break;
                }
            }
        }
    }
    g_list_free( sel_items );
    layout_items( self );
}


gboolean on_insert_item_invalidate( DesktopWindow* self )
{
    self->insert_item = NULL;
    return FALSE;
}

void desktop_window_insert_task_complete( VFSFileTask* task, DesktopWindow* self )
{
    // invalidate insert_item 2 seconds after task completion
    g_timeout_add_seconds( 2, ( GSourceFunc ) on_insert_item_invalidate, self );
}

void desktop_window_set_insert_item( DesktopWindow* self )
{
    GList* sel_items = desktop_window_get_selected_items( self, FALSE );
    if ( !sel_items )
        self->insert_item = NULL;
    else
    {
        self->insert_item = sel_items->data;
        g_list_free( sel_items );
    }
}

void on_drag_data_received( GtkWidget* w, GdkDragContext* ctx, gint x, gint y, 
                            GtkSelectionData* data, guint info, guint time )
{
    DesktopWindow* self = (DesktopWindow*)w;
    DesktopItem* item;

    if( gtk_selection_data_get_target( data ) == text_uri_list_atom )
    {
        gboolean text_hit = FALSE;
        if ( self->pending_drop_action )
        {
            x = self->drag_pending_x;
            y = self->drag_pending_y;
        }
        if ( self->sort_by == DW_SORT_CUSTOM )
        {
            item = hit_test_icon( self, x, y );
            if ( !item )
                text_hit = hit_test_text( self, x, y, &item );
        }
        else
            item = hit_test( self, x, y );
        char* dest_dir = NULL;
        VFSFileTaskType file_action = VFS_FILE_TASK_MOVE;
        PtkFileTask* task = NULL;
        char** files;
        int n, i;
        GList* file_list;
        struct stat statbuf;    // skip stat64

        if( (gtk_selection_data_get_length( data ) < 0) || 
                                (gtk_selection_data_get_format( data ) != 8) )
        {
            gtk_drag_finish( ctx, FALSE, FALSE, time );
            return;
        }

        if ( item && item->fi && !text_hit && vfs_file_info_is_dir( item->fi ) )
            // drop into a dir item on the desktop
            dest_dir = g_build_filename( vfs_get_desktop_dir(), item->fi->name, NULL );

        // We are just checking the suggested actions for the drop site, not really drop
        if( self->pending_drop_action )
        {
            GdkDragAction suggested_action = 0;
            dev_t dest_dev;

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
                            if( lstat( *pfile, &statbuf ) == 0 
                                                && statbuf.st_dev != dest_dev )
                            {
                                self->drag_src_dev = statbuf.st_dev;
                                break;
                            }
                        }
                    }
                    g_strfreev( files );
                }

                if( self->drag_src_dev != dest_dev )
                    // src and dest are on different devices
                    suggested_action = GDK_ACTION_COPY;
                else
                    suggested_action = GDK_ACTION_MOVE;
            }
            g_free( dest_dir );
            self->pending_drop_action = FALSE;
            gdk_drag_status( ctx, suggested_action, time );
            return;
        }
        //printf("on_drag_data_received  text_uri_list_atom  %s\n", text_hit ? "text_hit" : item ? "item" : "no-item" );

        switch ( gdk_drag_context_get_selected_action( ctx ) )
        {
        case GDK_ACTION_COPY:
            file_action = VFS_FILE_TASK_COPY;
            break;
        case GDK_ACTION_LINK:
            file_action = VFS_FILE_TASK_LINK;
            break;
            // FIXME: GDK_ACTION_DEFAULT, GDK_ACTION_PRIVATE, and GDK_ACTION_ASK are not handled
        default:
            break;
        }

        files = get_files_from_selection_data( data );
        if ( files )
        {
            if ( file_action == VFS_FILE_TASK_MOVE && item && item->fi && !dest_dir )
            {
                // moving onto non-dir item - are source files on desktop?
                ino_t src_dir_inode;
                dev_t src_dir_dev;
                char* src_dir = g_path_get_dirname( files[0] );
                if ( stat( src_dir, &statbuf ) == 0 )
                {
                    src_dir_inode = statbuf.st_ino;
                    src_dir_dev = statbuf.st_dev;
                    if ( stat( vfs_get_desktop_dir(), &statbuf ) == 0 &&
                                            statbuf.st_ino == src_dir_inode &&
                                            statbuf.st_dev == src_dir_dev )
                    {
                        // source files are on desktop, move items only
                        g_strfreev( files );
                        g_free( src_dir );
                        if ( self->sort_by == DW_SORT_CUSTOM )
                            move_desktop_items( self, ctx, item );
                        gtk_drag_finish( ctx, TRUE, FALSE, time );
                        return;
                    }
                }
                g_free( src_dir );
            }

            file_list = NULL;
            n = g_strv_length( files );
            for( i = 0; i < n; ++i )
                file_list = g_list_prepend( file_list, files[i] );
            g_free( files );
            file_list = g_list_reverse( file_list );
            
            // do not pass desktop parent - some WMs won't bring desktop dlg to top
            task = ptk_file_task_new( file_action,
                                      file_list,
                                      dest_dir ? dest_dir : vfs_get_desktop_dir(),
                                      NULL, NULL );
            // get insertion box
            if ( self->sort_by == DW_SORT_CUSTOM )
            {
                if ( !item )
                    item = hit_test_box( self, x, y );
                self->insert_item = item;
                ptk_file_task_set_complete_notify( task,
                                    (GFunc)desktop_window_insert_task_complete,
                                    self );
            }
            ptk_file_task_run( task );
        }
        g_free( dest_dir );

        gtk_drag_finish( ctx, files != NULL, FALSE, time );
    }
    else if ( gtk_selection_data_get_target( data ) == desktop_icon_atom )
    {   // moving desktop icon to desktop
        if ( self->sort_by == DW_SORT_CUSTOM )
        {
            //printf("on_drag_data_received - desktop_icon_atom\n");
            if ( !hit_test_text( self, x, y, &item ) )
                item = hit_test_box( self, x, y );
            move_desktop_items( self, ctx, item );

            /*
            GList* sels = desktop_window_get_selected_items(self), *l;
            int x_off = x - self->drag_start_x;
            int y_off = y - self->drag_start_y;
            for( l = sels; l; l = l->next )
            {
                DesktopItem* item = l->data;
                #if 0   // temporarily turn off
                move_item( self, item, x_off, y_off, TRUE );
                #endif
                DesktopItem* hit_item = hit_test_box( self, x, y );
                printf( "    move: %s, %d (%d), %d (%d) -> %s\n", item->fi ? vfs_file_info_get_name( item->fi ) : "Empty", x, x_off, y, y_off, hit_item ? hit_item->fi ? vfs_file_info_get_name( hit_item->fi ) : "EMPTY" : "MISS" );
            }
            g_list_free( sels );
            */
        }
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

gboolean on_rename_item_invalidate( DesktopWindow* self )
{
    self->renaming_item = self->renamed_item = NULL;
    return FALSE;
}

void desktop_window_rename_selected_files( DesktopWindow* self,
                                           GList* files, const char* cwd )
{
    GList* l;
    GList* ll;
    VFSFileInfo* file;
    DesktopItem* item;
    char* msg;
    
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
                if ( !have_rw_access( target ) )
                {
                    msg = g_strdup_printf( _("You do not have permission to edit the contents of file %s\n\nConsider copying the file to the desktop, or link to a copy placed in ~/.local/share/applications/"), target );
                    xset_msg_dialog( GTK_WIDGET( self ), GTK_MESSAGE_INFO, _("Unable To Rename"), NULL, 0, msg, NULL, NULL );                    
                    g_free( msg );
                    g_free( path );
                    g_free( filename );
                    g_free( target );
                    break;
                }
                else
                {
                    msg = g_strdup_printf( _("Enter new desktop item name:\n\nChanging the name of this desktop item requires modifying the contents of desktop file %s"), target );
                    g_free( target );
                    if ( !xset_text_dialog( GTK_WIDGET( self ), _("Change Desktop Item Name"), NULL, FALSE, msg,
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
                        if ( self->dir )
                            // in case file is a link
                            vfs_dir_emit_file_changed( self->dir, filename, file, FALSE );
                        g_free( path );
                        g_free( filename );
                        xset_msg_dialog( GTK_WIDGET( self ), GTK_MESSAGE_ERROR, _("Rename Error"), NULL, 0, _("An error occured renaming this desktop item."), NULL, NULL );
                        break;
                    }
                    if ( self->dir )
                        // in case file is a link
                        vfs_dir_emit_file_changed( self->dir, filename, file, FALSE );
                }
                g_free( path );
                g_free( filename );
            }
        }
        else
        {
            if ( !ptk_rename_file( self, NULL, cwd, file, NULL, FALSE, 0, NULL ) )
                break;
            if ( self->sort_by == DW_SORT_CUSTOM )
            {
                /* Track the item being renamed so its position can be retained.
                 * This is a bit hackish, but the file monitor currently emits
                 * two events when a file is renamed (delete and create).
                 * This method will not preserve location if file is renamed
                 * from outside SpaceFM.
                 * Would be better to employ inotify MOVED_FROM and MOVED_TO
                 * with cookie, but file monitor doesn't currently provide this
                 * data to callbacks. */
                self->renaming_item = NULL;
                self->renamed_item = NULL;
                for ( ll = self->items; ll; ll = ll->next )
                {
                    item = (DesktopItem*) ll->data;
                    if ( item->fi == file )
                    {
                        self->renaming_item = item;
                        break;
                    }
                }
            }
        }
    }
    if ( self->sort_by == DW_SORT_CUSTOM )
    {
        // invalidate renaming_item 2 seconds after last rename
        g_timeout_add_seconds( 2, ( GSourceFunc ) on_rename_item_invalidate,
                                                                        self );
    }
}

DesktopItem* get_next_item( DesktopWindow* self, int direction )
{
    DesktopItem *item, *current_item;
    DesktopItem* next_item = NULL;
    GList* l;

    if ( self->focus )
        current_item = self->focus;
    else if ( self->items )
    {
        current_item = (DesktopItem*) self->items->data;
        self->focus = current_item;
    }
    else 
        return NULL; //No items!

    for ( l = self->items; l; l = l->next )
    {
        item = (DesktopItem*) l->data;
        if ( item != current_item )
        {
            next_item = item;
            break;
        }
    }

    if ( next_item )  //If there are other items
    {
        int sign = ( direction==GDK_KEY_Down||direction==GDK_KEY_Right)? 1 : -1;
        int keep_x = (direction==GDK_KEY_Down||direction==GDK_KEY_Up)? 1 : 0;
        int test_x = 1 - keep_x;
        int keep_y = (direction==GDK_KEY_Left||direction==GDK_KEY_Right)? 1 : 0;
        int test_y = 1 - keep_y;

        int diff = 32000;
        int nearest_diff = diff;
        int line_diff;

        gboolean done = FALSE;

        int myline_x = current_item->icon_rect.x;
        int myline_y = current_item->icon_rect.y;

        for ( l = self->items; l; l = l->next )
        {
            item = (DesktopItem*) l->data;
            if ( item == current_item )
                continue;
            diff = item->icon_rect.x*test_x + item->icon_rect.y*test_y;
            diff -= current_item->icon_rect.x*test_x + current_item->icon_rect.y*test_y;
            diff = diff*sign; //positive diff for the valid items;

            //so we have icons with variable height, let's get dirty...
            line_diff = item->icon_rect.x*keep_x + item->icon_rect.y*keep_y;
            line_diff -= current_item->icon_rect.x*keep_x + current_item->icon_rect.y*keep_y;
            if ( line_diff < 0 )
                line_diff = -line_diff; //positive line diff for adding;
            diff += 2*line_diff*(diff>0?1:-1); //line_diff is more important than diff

            if ( ( !line_diff || test_x ) && diff > 0 && diff < nearest_diff )
            {
                next_item = item;
                nearest_diff = diff;
                done = TRUE;
            }
        }

        //Support for jumping through the borders to the next/prev row or column
        /* self->items is sorted by columns by default, so for now let's just support up-down looping */
        if ( !done && test_y )
        {
            GList* m;
            for ( l = self->items; l; l = l->next )
            {
                item = (DesktopItem*) l->data;
                if ( item == current_item )
                {
                    m = sign == 1 ? l->next : l->prev;
                    if ( m )
                    {
                        next_item = (DesktopItem*) m->data;
                        done = TRUE;
                    }
                    break;
                }
            }
        }

        if ( !done )
            return current_item;
        else
            return next_item;
    }
    return current_item;
}

void focus_item( DesktopWindow* self, DesktopItem* item )
{
    if ( !item )
        return;
    DesktopItem* current = self->focus;
    if ( current )
        redraw_item( self, current );
    self->focus = item;
    redraw_item( self, item );
}

void desktop_window_select( DesktopWindow* self, DWSelectMode mode )
{
    char* key;
    char* name;
    gboolean icase = FALSE;
    
    if ( mode < DW_SELECT_ALL || mode > DW_SELECT_PATTERN )
        return;

    if ( mode == DW_SELECT_PATTERN )
    {
        // get pattern from user  (store in ob1 so it's not saved)
        XSet* set = xset_get( "select_patt" );
        if ( !xset_text_dialog( GTK_WIDGET( self ), _("Select By Pattern"), NULL, FALSE, _("Enter pattern to select files and folders:\n\nIf your pattern contains any uppercase characters, the matching will be case sensitive.\n\nExample:  *sp*e?m*\n\nTIP: You can also enter '%% PATTERN' in the path bar."),
                                            NULL, set->ob1, &set->ob1,
                                            NULL, FALSE, NULL ) || !set->ob1 )
            return;
        key = set->ob1;

        // case insensitive search ?
        char* lower_key = g_utf8_strdown( key, -1 );
        if ( !strcmp( lower_key, key ) )
        {
            // key is all lowercase so do icase search
            icase = TRUE;
        }
        g_free( lower_key );
    }
    else
        key = NULL;

    GList* l;
    DesktopItem* item;
    gboolean sel;
    for( l = self->items; l; l = l->next )
    {
        item = (DesktopItem*) l->data;
        if ( !item->fi )
            sel = FALSE;  // empty
        else if ( mode == DW_SELECT_ALL )
            sel = TRUE;
        else if ( mode == DW_SELECT_NONE )
            sel = FALSE;
        else if ( mode == DW_SELECT_INVERSE )
            sel = !item->is_selected;
        else
        {
            // DW_SELECT_PATTERN - test name
            name = (char*)vfs_file_info_get_disp_name( item->fi );
            if ( icase )
                name = g_utf8_strdown( name, -1 );

            sel = fnmatch( key, name, 0 ) == 0;

            if ( icase )
                g_free( name );
        }
        if ( sel != item->is_selected )
        {
            item->is_selected = sel;
            redraw_item( self, item );
        }
    }
}

void select_item( DesktopWindow* self, DesktopItem* item, gboolean val )
{
    if ( !item )
        return;
    item->is_selected = item->fi ? val : FALSE;
    redraw_item( self, item );
}

gboolean on_key_press( GtkWidget* w, GdkEventKey* event )
{
    DesktopWindow* desktop = (DesktopWindow*)w;
    int keymod = ( event->state & ( GDK_SHIFT_MASK | GDK_CONTROL_MASK |
                 GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK ) );

    if ( event->keyval == 0 )
        return FALSE;

    GList* l;
    XSet* set;
    char* xname;
    for ( l = xsets; l; l = l->next )
    {
        if ( ((XSet*)l->data)->shared_key )
        {
            // set has shared key
            set = xset_get( ((XSet*)l->data)->shared_key );
            if ( set->key == event->keyval && set->keymod == keymod )
            {
                // shared key match
                if ( g_str_has_prefix( set->name, "panel" ) )
                    return FALSE;
                goto _key_found;  // for speed
            }
            else
                continue;
        }
        if ( ((XSet*)l->data)->key == event->keyval
                                        && ((XSet*)l->data)->keymod == keymod )
        {
            set = (XSet*)l->data;
_key_found:
            // run menu_cb
            if ( set->menu_style < XSET_MENU_SUBMENU )
            {
                set->browser = NULL;
                set->desktop = desktop;
                xset_menu_cb( NULL, set );  // also does custom activate
            }
            if ( !set->lock )
                return TRUE;

            // handlers
            if ( !strcmp( set->name, "new_app" ) )
                desktop_window_add_application( desktop );
            else if ( g_str_has_prefix( set->name, "desk_" ) )
            {
                if ( g_str_has_prefix( set->name, "desk_sort_" ) )
                {
                    int by;
                    char* xname = set->name + 10;
                    if ( !strcmp( xname, "name" ) )
                        by = DW_SORT_BY_NAME;
                    else if ( !strcmp( xname, "size" ) )
                        by = DW_SORT_BY_SIZE;
                    else if ( !strcmp( xname, "type" ) )
                        by = DW_SORT_BY_TYPE;
                    else if ( !strcmp( xname, "date" ) )
                        by = DW_SORT_BY_MTIME;
                    else if ( !strcmp( xname, "cust" ) )
                        by = DW_SORT_CUSTOM;
                    else
                    {
                        if ( !strcmp( xname, "ascend" ) )
                            by = GTK_SORT_ASCENDING;
                        else if ( !strcmp( xname, "descend" ) )
                            by = GTK_SORT_DESCENDING;
                        else
                            return TRUE;
                        desktop_window_sort_items( desktop, desktop->sort_by, by );
                        return TRUE;
                    }
                    desktop_window_sort_items( desktop, by, desktop->sort_type );
                }
                else if ( !strcmp( set->name, "desk_pref" ) )
                    // do not pass desktop parent - some WMs won't bring desktop dlg to top
                    fm_edit_preference( NULL, PREF_DESKTOP );
                else if ( !strcmp( set->name, "desk_open" ) )
                    desktop_window_open_desktop_dir( NULL, desktop );
            }
            else if ( g_str_has_prefix( set->name, "paste_" ) )
            {
                xname = set->name + 6;
                if ( !strcmp( xname, "link" ) )
                {
                    desktop_window_set_insert_item( desktop );
                    // do not pass desktop parent - some WMs won't bring desktop dlg to top
                    // must pass desktop here for callback window
                    ptk_clipboard_paste_links( NULL,
                                    vfs_get_desktop_dir(), NULL,
                                    (GFunc)desktop_window_insert_task_complete,
                                    GTK_WINDOW( gtk_widget_get_toplevel(
                                                GTK_WIDGET( desktop ) ) ) );
                }
                else if ( !strcmp( xname, "target" ) )
                {
                    desktop_window_set_insert_item( desktop );
                    // do not pass desktop parent - some WMs won't bring desktop dlg to top
                    // must pass desktop here for callback window
                    ptk_clipboard_paste_targets( NULL,
                            vfs_get_desktop_dir(),
                            NULL, (GFunc)desktop_window_insert_task_complete,
                            GTK_WINDOW( gtk_widget_get_toplevel(
                                        GTK_WIDGET( desktop ) ) ) );
                }
                else if ( !strcmp( xname, "as" ) )
                {
                    desktop_window_set_insert_item( desktop );
                    // must pass desktop here for callback window
                    ptk_file_misc_paste_as( desktop, NULL, vfs_get_desktop_dir(),
                                    (GFunc)desktop_window_insert_task_complete );
                }
            }
            else if ( g_str_has_prefix( set->name, "select_" ) )
            {
                DWSelectMode mode;
                xname = set->name + 7;
                if ( !strcmp( xname, "all" ) )
                    mode = DW_SELECT_ALL;
                else if ( !strcmp( xname, "un" ) )
                    mode = DW_SELECT_NONE;
                else if ( !strcmp( xname, "invert" ) )
                    mode = DW_SELECT_INVERSE;
                else if ( !strcmp( xname, "patt" ) )
                    mode = DW_SELECT_PATTERN;
                else
                    return TRUE;
                desktop_window_select( desktop, mode );
            }
            else
                // all the rest require ptkfilemenu data
                ptk_file_menu_action( desktop, NULL, set->name );
            return TRUE;
        }
    }

    if ( keymod == GDK_CONTROL_MASK )
    {
        switch ( event->keyval )
        {
        case GDK_KEY_Down:
        case GDK_KEY_Up:
        case GDK_KEY_Left:
        case GDK_KEY_Right:
            focus_item( desktop, get_next_item( desktop, event->keyval ) );
            return TRUE;
        case GDK_KEY_space:
            if ( desktop->focus )
                select_item( desktop, desktop->focus, !desktop->focus->is_selected );
            return TRUE;
        }
    }
    else if ( keymod == 0 )
    {
        switch ( event->keyval )
        {
        case GDK_KEY_Return:
        case GDK_KEY_space:
            if ( desktop->focus )
                open_clicked_item( desktop, desktop->focus );
            return TRUE;
        case GDK_KEY_Down:
        case GDK_KEY_Up:
        case GDK_KEY_Left:
        case GDK_KEY_Right:
            desktop_window_select( desktop, DW_SELECT_NONE );
            focus_item( desktop, get_next_item( desktop, event->keyval ) );
            if ( desktop->focus )
                select_item( desktop, desktop->focus, TRUE );
            return TRUE;
        case GDK_KEY_Menu:
            show_desktop_menu( desktop, 0, event->time );
            return TRUE;
        }
    }
    return FALSE;
}

void on_style_set( GtkWidget* w, GtkStyle* prev )
{
    DesktopWindow* self = (DesktopWindow*)w;

    PangoContext* pc;
    PangoFontMetrics *metrics;
    int font_h;
    pc = gtk_widget_get_pango_context( (GtkWidget*)self );

    metrics = pango_context_get_metrics(
                            pc, gtk_widget_get_style((GtkWidget*)self)->font_desc,
                            pango_context_get_language(pc));

    font_h = pango_font_metrics_get_ascent(metrics) + pango_font_metrics_get_descent (metrics);
    font_h /= PANGO_SCALE;
}

void on_realize( GtkWidget* w )
{
    guint32 val;
    DesktopWindow* self = (DesktopWindow*)w;

    GTK_WIDGET_CLASS(parent_class)->realize( w );
    gdk_window_set_type_hint( gtk_widget_get_window(w),
                              GDK_WINDOW_TYPE_HINT_DESKTOP );

    gtk_window_set_skip_pager_hint( GTK_WINDOW(w), TRUE );
    gtk_window_set_skip_taskbar_hint( GTK_WINDOW(w), TRUE );
    gtk_window_set_resizable( (GtkWindow*)w, FALSE );

    /* This is borrowed from fbpanel */
#define WIN_HINTS_SKIP_FOCUS      (1<<0)    /* skip "alt-tab" */
    val = WIN_HINTS_SKIP_FOCUS;
    XChangeProperty(gdk_x11_get_default_xdisplay(), GDK_WINDOW_XID(gtk_widget_get_window(w)),
          XInternAtom(gdk_x11_get_default_xdisplay(), "_WIN_HINTS", False), XA_CARDINAL, 32,
          PropModeReplace, (unsigned char *) &val, 1);

//    if( self->background )
//        gdk_window_set_back_pixmap( w->window, self->background, FALSE );
}

gboolean on_focus_in( GtkWidget* w, GdkEventFocus* evt )
{
    DesktopWindow* self = (DesktopWindow*) w;
    //sfm gtk3 gtk_widget_grab_focus( w ) not equivalent
    //gtk_widget_grab_focus( w );
#if GTK_CHECK_VERSION (3, 0, 0)
    // TODO
#else
    GTK_WIDGET_SET_FLAGS( w, GTK_HAS_FOCUS );
#endif
    if( self->focus )
        redraw_item( self, self->focus );
    return FALSE;
}

gboolean on_focus_out( GtkWidget* w, GdkEventFocus* evt )
{
    DesktopWindow* self = (DesktopWindow*) w;
    if( self->focus )
    {
        //sfm gtk3 restored - can GTK_WIDGET_UNSET_FLAGS be removed without changes?
#if GTK_CHECK_VERSION (3, 0, 0)
    // TODO
#else
        GTK_WIDGET_UNSET_FLAGS( w, GTK_HAS_FOCUS );
#endif
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

void desktop_window_on_autoopen_cb( gpointer task, gpointer aop )
{
    if ( !aop )
        return;

    AutoOpenCreate* ao = (AutoOpenCreate*)aop;
    DesktopWindow* self = (DesktopWindow*)ao->file_browser;
    
    if ( ao->path && g_file_test( ao->path, G_FILE_TEST_EXISTS ) )
    {
        char* cwd = g_path_get_dirname( ao->path );
        VFSFileInfo* file;

        // select item on desktop
        if ( GTK_IS_WINDOW( self ) && self->dir && 
                                    !g_strcmp0( cwd, vfs_get_desktop_dir() ) )
        {
            char* name = g_path_get_basename( ao->path );
            
            // force file created notify
            vfs_dir_emit_file_created( self->dir, name, TRUE );
            vfs_dir_flush_notify_cache();

            // find item on desktop
            GList* l;
            DesktopItem* item;
            for ( l = self->items; l; l = l->next )
            {
                item = (DesktopItem*)l->data;
                if ( item->fi && 
                            !g_strcmp0( vfs_file_info_get_name( item->fi ), name ) )
                    break;
            }

            if ( l ) // found
            {
                desktop_window_select( self, DW_SELECT_NONE );
                select_item( self, (DesktopItem*)l->data, TRUE );
                focus_item( self, (DesktopItem*)l->data );
            }
            g_free( name );
        }

        // open file/folder
        if ( ao->open_file )
        {
            file = vfs_file_info_new();
            vfs_file_info_get( file, ao->path, NULL );
            GList* sel_files = NULL;
            sel_files = g_list_prepend( sel_files, file );
            if ( g_file_test( ao->path, G_FILE_TEST_IS_DIR ) )
            {
                gdk_threads_enter();
                open_folders( sel_files );
                gdk_threads_leave();
            }
            else
                ptk_open_files_with_app( cwd, sel_files,
                                                    NULL, NULL, FALSE, TRUE );
            vfs_file_info_unref( file );
            g_list_free( sel_files );
        }
        g_free( cwd );
    }
    g_free( ao->path );
    g_slice_free( AutoOpenCreate, ao );
}

void on_settings( GtkMenuItem *menuitem, DesktopWindow* self )
{
    // do not pass desktop parent - some WMs won't bring desktop dlg to top
    fm_edit_preference( NULL, PREF_DESKTOP );
}


/* private methods */

void calc_item_size( DesktopWindow* self, DesktopItem* item )
{
    PangoLayoutLine* line;
    int line_h;
    gboolean fake_line = TRUE;  //self->sort_by == DW_SORT_CUSTOM;
    
    item->box.width = self->item_w;
    item->box.height = self->y_pad * 2;
    item->box.height += self->icon_size;
    item->box.height += self->spacing;

    // get text_rect height 
    item->text_rect.height = 0;
    
    if ( item->fi )  // not empty
    {
        pango_layout_set_text( self->pl, item->fi->disp_name, -1 );
        pango_layout_set_wrap( self->pl, PANGO_WRAP_WORD_CHAR );  // wrap the text
        pango_layout_set_ellipsize( self->pl, PANGO_ELLIPSIZE_NONE );

        if( pango_layout_get_line_count(self->pl) >= 2 ) // there are more than 2 lines
        {
            // we only allow displaying two lines, so let's get the second line
            // Pango only provide version check macros in the latest versions...
            // So there is no point in making this check.
            // FIXME: this check should be done ourselves in configure.
#if defined (PANGO_VERSION_CHECK)
#if PANGO_VERSION_CHECK( 1, 16, 0 )
            line = pango_layout_get_line_readonly( self->pl, 1 );
#else
            line = pango_layout_get_line( self->pl, 1 );
#endif
#else
            line = pango_layout_get_line( self->pl, 1 );
#endif
            item->len1 = line->start_index; // this the position where the first line wraps

            // OK, now we layout these 2 lines separately
            pango_layout_set_text( self->pl, item->fi->disp_name, item->len1 );
            pango_layout_get_pixel_size( self->pl, NULL, &line_h );
            item->text_rect.height = line_h;
            fake_line = FALSE;
        }
    }

    pango_layout_set_wrap( self->pl, 0 );    // wrap the text
    pango_layout_set_ellipsize( self->pl, PANGO_ELLIPSIZE_END );

    if ( item->fi )
        pango_layout_set_text( self->pl, item->fi->disp_name + item->len1, -1 );
    else
        pango_layout_set_text( self->pl, "Empty", -1 );
    pango_layout_get_pixel_size( self->pl, NULL, &line_h );
    item->text_rect.height += line_h;

    item->text_rect.width = MAX( self->label_w, self->icon_size ); //100;

    // add empty text line height to standardize height to two lines for custom sort
    item->box.height += item->text_rect.height * ( fake_line ? 2 : 1 );

    item->icon_rect.width = item->icon_rect.height = self->icon_size;
}

void layout_items( DesktopWindow* self )
{
    GList* l;
    GList* ll;
    DesktopItem* item;
    GtkWidget* widget = (GtkWidget*)self;
    int i, x, y, w, bottom, right, row_count;
    gboolean list_compressed = self->sort_by != DW_SORT_CUSTOM;

    self->item_w = MAX( self->label_w, self->icon_size ) + self->x_pad * 2;
    pango_layout_set_width( self->pl, MAX( self->label_w, self->icon_size ) *
                                                        PANGO_SCALE );  // 100
    pango_layout_set_font_description( self->pl, app_settings.desk_font );

start_layout:
    x = self->wa.x + self->margin_left;
    y = self->wa.y + self->margin_top;
    right = self->wa.x + self->wa.width - self->margin_right;
    bottom = self->wa.y + self->wa.height - self->margin_bottom;
    self->box_count = 0;
    self->row_count = 0;
    row_count = 0;
    
    for ( l = self->items; l; l = l->next )
    {
        item = (DesktopItem*)l->data;
        item->box.width = self->item_w;
        calc_item_size( self, item );

        if ( y + item->box.height > bottom )
        {
            // bottom reached - go to next column
            y = self->wa.y + self->margin_top;
            x += self->item_w;
            // save row count for this layout
            if ( !self->row_count )
                self->row_count = row_count;
            row_count = 1;  // adding first row of new column
            if ( self->sort_by == DW_SORT_CUSTOM && 
                                self->order_rows > self->row_count && l->prev )
            {
                // saved row count was greater than current - eat empties
                GList* l_prev = l->prev;
                ll = l;
                for ( i = self->row_count; i < self->order_rows && ll; i++ )
                {
                    item = (DesktopItem*)ll->data;
                    if ( item->fi )
                    {
                        // non-empty encountered in non-existent row
                        // This will cause subsequent rows to be out of alignment
                        ll = NULL;
                    }
                    else
                    {
                        if ( l_prev )
                        {
                            // rewind to previous item once for l = l->next
                            // before deleting current l
                            l = l_prev;
                            l_prev = NULL;
                        }
                        ll = ll->next;
                        desktop_item_free( item );
                        self->items = g_list_remove( self->items, item );
                    }
                }
                if ( !l_prev )
                    continue;  // was rewound
            }
        }
        else if ( self->sort_by == DW_SORT_CUSTOM )
        {
            row_count++;  // adding new row to current column
            if ( self->order_rows && self->order_rows < row_count && l->prev )
            {
                // saved row count was less than current - insert empty
                item = g_slice_new0( DesktopItem );
                item->fi = NULL;
                item->box.width = self->item_w;
                calc_item_size( self, item );
                self->items = g_list_insert_before( self->items, l, item );
                l = l->prev;
            }
        }
        
        if ( !list_compressed && x + item->box.width > right )
        {
            // right side reached - remove empties and redo layout (custom sort)
            gboolean list_changed = FALSE;
            // scan down below box_count and remove all empties
            for ( ll = g_list_nth( self->items, self->box_count ); ll; 
                                                            ll = ll->next )
            {
                if ( !((DesktopItem*)ll->data)->fi )
                {
                    desktop_item_free( (DesktopItem*)ll->data );
                    self->items = g_list_remove( self->items, ll->data );
                    if ( !list_changed )
                        list_changed = TRUE;
                }
            }
            // scan up above box_count and remove empties until all items fit
            gboolean empty_removed = TRUE;
            while ( empty_removed &&
                                g_list_length( self->items ) > self->box_count )
            {
                empty_removed = FALSE;
                for ( ll = g_list_nth( self->items, self->box_count - 1 ); ll; 
                                                                ll = ll->prev )
                {
                    if ( !((DesktopItem*)ll->data)->fi )
                    {
                        desktop_item_free( (DesktopItem*)ll->data );
                        self->items = g_list_remove( self->items, ll->data );
                        if ( !list_changed )
                            list_changed = TRUE;
                        empty_removed = TRUE;
                        break;
                    }
                }                
            }
            // all items now fit or list is as compressed as possible - redo
            list_compressed = TRUE;
            if ( list_changed )
                goto start_layout;
        }
        else
            self->box_count++;

        item->box.x = x;
        item->box.y = y;
        y += item->box.height; // go to next row

        item->icon_rect.x = item->box.x + (item->box.width - self->icon_size) / 2;
        item->icon_rect.y = item->box.y + self->y_pad;

        item->text_rect.x = item->box.x + self->x_pad;
        item->text_rect.y = item->box.y + self->y_pad + self->icon_size + self->spacing;
        /*
        if ( !item->fi )
            printf( "LAY EMPTY %d, %d  %p\n", x, y, item );            
        else
            printf( "LAY %s %d, %d  %p\n", vfs_file_info_get_name( item->fi ), x, y, item );
        */
    }
    self->order_rows = 0;  // reset
    
    if ( self->sort_by == DW_SORT_CUSTOM )
    {
        // add empty boxes to fill window
        //printf("---------- ADD_EMPTY_BOXES\n");
        do
        {
            item = g_slice_new0( DesktopItem );
            item->fi = NULL;
            item->box.width = self->item_w;
            calc_item_size( self, item );
            
            if( y + item->box.height > bottom )
            {
                // bottom reached - go to next column
                y = self->wa.y + self->margin_top;
                x += self->item_w;
            }
            if ( x + item->box.width > right )
            {
                // right side reached - stop adding empty boxes
                //printf( "RIGHT reached - stop adding empty %d, %d\n", x, y );
                g_slice_free( DesktopItem, item );
                break;
            }
            
            item->box.x = x;
            item->box.y = y;
            //printf( "ADD empty %d, %d  %p\n", x, y, item );
            y += item->box.height; // go to next row
            
            // add to list
            self->items = g_list_append( self->items, item );
            self->box_count++;
        } while ( 1 );
        custom_order_write( self );
    }
    //printf("    box_count = %d\n", self->box_count );
    gtk_widget_queue_draw( GTK_WIDGET(self) );
}

void on_file_listed( VFSDir* dir, gboolean is_cancelled, DesktopWindow* self )
{
    GList* l, *items = NULL;
    VFSFileInfo* fi;
    DesktopItem* item;
    DesktopItem* target_item;
    gpointer ptr;
    int order, i;
    GList* ll;
    GList* unordered = NULL;
    
    self->file_listed = TRUE;
    GHashTable* order_hash = custom_order_read( self );

    g_mutex_lock( dir->mutex );
    for( l = dir->file_list; l; l = l->next )
    {
        fi = (VFSFileInfo*)l->data;
        if( fi->name[0] == '.' )    /* skip the hidden files */
            continue;
        item = g_slice_new0( DesktopItem );
        item->fi = vfs_file_info_ref( fi );
        if ( self->sort_by == DW_SORT_CUSTOM )
        {
            if ( order_hash && ( ptr = g_hash_table_lookup( order_hash,
                                            vfs_file_info_get_name( fi ) ) ) )
            {
                order = GPOINTER_TO_INT( ptr ) - 1;
                ll = g_list_nth( items, order );
                if ( !ll )
                {
                    // position is beyond end of list - add empties
                    for ( i = g_list_length( items ); i < order; i++ )
                    {
                        target_item = g_slice_new0( DesktopItem );
                        target_item->fi = NULL;
                        items = g_list_append( items, target_item );                    
                    }
                    items = g_list_append( items, item );                    
                }
                else if ( ((DesktopItem*)ll->data)->fi )
                {
                    // position already used - insert
                    items = g_list_insert( items, item, order + 1 );
                }
                else
                {
                    // position empty - replace
                    desktop_item_free( (DesktopItem*)ll->data );
                    ll->data = item;
                }
            }
            else
            {
                // order not found
                unordered = g_list_prepend( unordered, item );
            }
        }
        else
            items = g_list_prepend( items, item );
        if ( vfs_file_info_is_image( fi ) )
            vfs_thumbnail_loader_request( dir, fi, TRUE );
    }
    g_mutex_unlock( dir->mutex );

    // sort
    GCompareDataFunc comp_func = get_sort_func( self );
    if ( comp_func )
        self->items = g_list_sort_with_data( items, comp_func, self );
    else
    {
        // custom sort
        if ( unordered )
        {
            unordered = g_list_sort_with_data( unordered,
                                (GCompareDataFunc)comp_item_by_name, self );
            items = g_list_concat( items, unordered );
        }
        self->items = items;
        if ( order_hash )
            g_hash_table_destroy( order_hash );
    }

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
        if ( item->fi && item->fi == fi )
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

    if( !dir || !file )
        // failsafe
        return;

    /* don't show hidden files */
    if( file->name[0] == '.' )
        return;

    /* prevent duplicated items */
    for( l = self->items; l; l = l->next )
    {
        item = (DesktopItem*)l->data;
        if( item->fi && strcmp( file->name, item->fi->name ) == 0 )
            return;
    }

    item = g_slice_new0( DesktopItem );
    item->fi = vfs_file_info_ref( file );
    if ( !item->fi->big_thumbnail && vfs_file_info_is_image( item->fi ) )
        vfs_thumbnail_loader_request( dir, item->fi, TRUE );

    GCompareDataFunc comp_func = get_sort_func( self );
    if ( comp_func )
        self->items = g_list_insert_sorted_with_data( self->items, item,
                                                    get_sort_func(self), self );
    else
    {
        // custom sort
        GList* ll;
        if ( self->insert_item &&
                        ( l = g_list_find( self->items, self->insert_item ) ) )
        {
            // insert where dropped
            if ( ((DesktopItem*)self->insert_item)->fi )
            {
                // dropped onto a non-empty item - insert before
                self->items = g_list_insert_before( self->items, l, item );
                // scan downward and remove first empty
                for ( ll = l->next; ll; ll = ll->next )
                {
                    if ( !((DesktopItem*)ll->data)->fi )
                    {
                        desktop_item_free( (DesktopItem*)ll->data );
                        self->items = g_list_remove( self->items, ll->data );
                        break;
                    }
                }                
            }
            else
            {
                // dropped onto empty item, replace it
                desktop_item_free( (DesktopItem*)self->insert_item );
                l->data = item;
                self->insert_item = l->next->data;
            }
        }
        else if ( self->renamed_item && !((DesktopItem*)self->renamed_item)->fi &&
                        ( l = g_list_find( self->items, self->renamed_item ) ) )
        {
            // insert where item was renamed
            desktop_item_free( (DesktopItem*)self->renamed_item );
            self->renamed_item = NULL;
            l->data = item;
            item->is_selected = TRUE;
        }
        else
        {
            // find empty at end of all icons
            ll = NULL;
            for ( l = g_list_last( self->items ); l; l = l->prev )
            {
                if ( !((DesktopItem*)l->data)->fi )
                    ll = l;  // found empty
                else 
                    break;   // found icon
            }
            if ( ll )
            {
                // replace empty
                desktop_item_free( (DesktopItem*)ll->data );
                ll->data = item;
            }
            else
                // no empties found
                self->items = g_list_append( self->items, item );
        }
    }

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
    if( !dir || !file )
        // file == NULL == the desktop dir itself was deleted
        return;

    /* don't deal with hidden files */
    if( file->name[0] == '.' )
        return;

    /* find items */
    for( l = self->items; l; l = l->next )
    {
        item = (DesktopItem*)l->data;
        if ( item->fi && item->fi == file )
            break;
    }

    if( l ) /* found */
    {
        item = (DesktopItem*)l->data;
        if ( self->sort_by == DW_SORT_CUSTOM )
        {
            // replace item with empty
            DesktopItem* item_new = g_slice_new0( DesktopItem );
            item_new->fi = NULL;
            item_new->box = item->box;
            desktop_item_free( item );
            l->data = item_new;
            if ( self->renaming_item == item && self->renamed_item == NULL )
            {
                // item is being renamed - retain position
                self->renamed_item = item_new;
                self->renaming_item = NULL;
            }

            // layout all since deleting may allow more icons to fit if full
            //redraw_item( self, item_new );
            layout_items( self );
        }
        else
        {
            self->items = g_list_delete_link( self->items, l );
            desktop_item_free( item );
            /* FIXME: we shouldn't update the whole screen */
            /* FIXME: put this in idle handler with priority higher than redraw but lower than resize */
            layout_items( self );
        }
    }
}

void on_file_changed( VFSDir* dir, VFSFileInfo* file, gpointer user_data )
{
    GList *l;
    DesktopWindow* self = (DesktopWindow*)user_data;
    DesktopItem* item;
    GtkWidget* w = (GtkWidget*)self;

    if ( !dir || !file )
        // file == NULL == the desktop dir itself changed
        return;

    /* don't touch hidden files */
    if( file->name[0] == '.' )
        return;

    /* find items */
    for( l = self->items; l; l = l->next )
    {
        item = (DesktopItem*)l->data;
        if ( item->fi && item->fi == file )
            break;
    }

    if( l ) /* found */
    {
        item = (DesktopItem*)l->data;
        if ( !item->fi->big_thumbnail && vfs_file_info_is_image( item->fi ) )
            vfs_thumbnail_loader_request( dir, item->fi, TRUE );

        if( gtk_widget_get_visible( w ) )
        {
            /* redraw the item */
            redraw_item( self, item );
        }
    }
}

void desktop_window_copycmd( DesktopWindow* desktop, GList* sel_files,
                                                char* cwd, char* setname )
{
    if ( !setname || !desktop || !sel_files )
        return;
    XSet* set2;
    char* copy_dest = NULL;
    char* move_dest = NULL;
    char* path;
    
    if ( !strcmp( setname, "copy_loc_last" ) )
    {
        set2 = xset_get( "copy_loc_last" );
        copy_dest = g_strdup( set2->desc );
    }
    else if ( !strcmp( setname, "move_loc_last" ) )
    {
        set2 = xset_get( "copy_loc_last" );
        move_dest = g_strdup( set2->desc );
    }
    else if ( strcmp( setname, "copy_loc" ) && strcmp( setname, "move_loc" ) )
        return;

    if ( !strcmp( setname, "copy_loc" ) || !strcmp( setname, "move_loc" ) ||
                                            ( !copy_dest && !move_dest ) )
    {
        char* folder;
        set2 = xset_get( "copy_loc_last" );
        if ( set2->desc )
            folder = set2->desc;
        else
            folder = cwd;
        // do not pass desktop parent - some WMs won't bring desktop dlg to top
        path = xset_file_dialog( NULL, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                            _("Choose Location"), folder, NULL );
        if ( path && g_file_test( path, G_FILE_TEST_IS_DIR ) )
        {
            if ( g_str_has_prefix( setname, "copy_loc" ) )
                copy_dest = path;
            else
                move_dest = path;
            set2 = xset_get( "copy_loc_last" );
            xset_set_set( set2, "desc", path );
        }
        else
            return;
    }
    
    if ( copy_dest || move_dest )
    {
        int file_action;
        char* dest_dir;
        
        if ( copy_dest )
        {
            file_action = VFS_FILE_TASK_COPY;
            dest_dir = copy_dest;
        }
        else
        {
            file_action = VFS_FILE_TASK_MOVE;
            dest_dir = move_dest;
        }
        
        if ( !strcmp( dest_dir, cwd ) )
        {
            // do not pass desktop parent - some WMs won't bring desktop dlg to top
            xset_msg_dialog( NULL, GTK_MESSAGE_ERROR,
                                        _("Invalid Destination"), NULL, 0,
                                        _("Destination same as source"), NULL, NULL );
            g_free( dest_dir );
            return;
        }

        // rebuild sel_files with full paths
        GList* file_list = NULL;
        GList* sel;
        char* file_path;
        VFSFileInfo* file;
        for ( sel = sel_files; sel; sel = sel->next )
        {
            file = ( VFSFileInfo* ) sel->data;
            file_path = g_build_filename( cwd,
                                          vfs_file_info_get_name( file ), NULL );
            file_list = g_list_prepend( file_list, file_path );
        }

        // task
        PtkFileTask* task = ptk_file_task_new( file_action,
                                file_list,
                                dest_dir,
                                NULL,
                                NULL );
        ptk_file_task_run( task );
        g_free( dest_dir );
    }
    else
    {
        // do not pass desktop parent - some WMs won't bring desktop dlg to top
        xset_msg_dialog( NULL, GTK_MESSAGE_ERROR,
                                    _("Invalid Destination"), NULL, 0,
                                    _("Invalid destination"), NULL, NULL );
    }
}

/*-------------- Private methods -------------------*/

void paint_item( DesktopWindow* self, DesktopItem* item, GdkRectangle* expose_area )
{
    /* GdkPixbuf* icon = item->icon ? gdk_pixbuf_ref(item->icon) : NULL; */
    GdkPixbuf* icon;
    
    if ( !item->fi )
        return;   // empty
    const char* text = item->fi->disp_name;
    GtkWidget* widget = (GtkWidget*)self;
#if !GTK_CHECK_VERSION (3, 0, 0)
    GdkDrawable* drawable = gtk_widget_get_window(widget);
#endif
    GtkCellRendererState state = 0;
    GdkRectangle text_rect;
    int w, h;
    cairo_t *cr;

    cr = gdk_cairo_create( gtk_widget_get_window( widget ) );

    if ( item->fi->big_thumbnail )
        icon = g_object_ref( item->fi->big_thumbnail );
    else
        icon = vfs_file_info_get_big_icon( item->fi );

    if( item->is_selected )
        state = GTK_CELL_RENDERER_SELECTED;

    g_object_set( self->icon_render, "pixbuf", icon, NULL );
#if GTK_CHECK_VERSION (3, 0, 0)
    gtk_cell_renderer_render( self->icon_render, cr, widget,
                                                       &item->icon_rect, &item->icon_rect, state );
#else
    gtk_cell_renderer_render( self->icon_render, drawable, widget,
                                                       &item->icon_rect, &item->icon_rect, expose_area, state );
#endif

    if( icon )
        g_object_unref( icon );

    // add link_icon arrow to links
    if ( vfs_file_info_is_symlink( item->fi ) && link_icon )
    {
        cairo_set_operator ( cr, CAIRO_OPERATOR_OVER );
        gdk_cairo_set_source_pixbuf ( cr, link_icon,
                                      item->icon_rect.x,
                                      item->icon_rect.y );
       gdk_cairo_rectangle ( cr, &item->icon_rect );
        cairo_fill ( cr );
    }

    // text
    text_rect = item->text_rect;

    pango_layout_set_wrap( self->pl, 0 );

    if( item->is_selected )
    {
        GdkRectangle intersect={0};

        if( gdk_rectangle_intersect( expose_area, &item->text_rect, &intersect ) )
        {
            gdk_cairo_set_source_color( cr, &gtk_widget_get_style(widget)->bg[GTK_STATE_SELECTED] );
            cairo_rectangle( cr, intersect.x, intersect.y, intersect.width, intersect.height );
            cairo_fill( cr );
        }
    }
    else
    {
        /* Do the drop shadow stuff...  This is a little bit dirty... */
        ++text_rect.x;
        ++text_rect.y;

        gdk_cairo_set_source_color( cr, &self->shadow );

        if( item->len1 > 0 )
        {
            pango_layout_set_text( self->pl, text, item->len1 );
            pango_layout_get_pixel_size( self->pl, &w, &h );
            pango_cairo_update_layout( cr, self->pl );
            cairo_move_to( cr, text_rect.x, text_rect.y );
            pango_cairo_show_layout( cr, self->pl );
            text_rect.y += h;
        }
        pango_layout_set_text( self->pl, text + item->len1, -1 );
        pango_layout_set_ellipsize( self->pl, PANGO_ELLIPSIZE_END );
        cairo_move_to( cr, text_rect.x, text_rect.y );
        pango_cairo_show_layout( cr, self->pl );

        --text_rect.x;
        --text_rect.y;
    }

    if( self->focus == item && gtk_widget_has_focus(widget) )
    {
#if GTK_CHECK_VERSION (3, 0, 0)
        gtk_paint_focus( gtk_widget_get_style(widget), cr,
                        GTK_STATE_NORMAL,/*item->is_selected ? GTK_STATE_SELECTED : GTK_STATE_NORMAL,*/
                        widget, "icon_view",
                        item->text_rect.x, item->text_rect.y,
                        item->text_rect.width, item->text_rect.height);
#else
        gtk_paint_focus( gtk_widget_get_style(widget), gtk_widget_get_window(widget),
                        GTK_STATE_NORMAL,/*item->is_selected ? GTK_STATE_SELECTED : GTK_STATE_NORMAL,*/
                        &item->text_rect, widget, "icon_view",
                        item->text_rect.x, item->text_rect.y,
                        item->text_rect.width, item->text_rect.height);
#endif
    }

    text_rect = item->text_rect;

    gdk_cairo_set_source_color( cr, &self->fg );

    if( item->len1 > 0 )
    {
        pango_layout_set_text( self->pl, text, item->len1 );
        pango_layout_get_pixel_size( self->pl, &w, &h );
        pango_cairo_update_layout( cr, self->pl );
        cairo_move_to( cr, text_rect.x, text_rect.y );
        pango_cairo_show_layout( cr, self->pl );
        text_rect.y += h;
    }
    pango_layout_set_text( self->pl, text + item->len1, -1 );
    pango_layout_set_ellipsize( self->pl, PANGO_ELLIPSIZE_END );
    cairo_move_to( cr, text_rect.x, text_rect.y );
    pango_cairo_show_layout( cr, self->pl );

    cairo_destroy( cr );
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
{   // hit on icon or text ?
    DesktopItem* item;
    GList* l;
    for ( l = self->items; l; l = l->next )
    {
        item = (DesktopItem*) l->data;
        if ( !item->fi )
            continue;  // empty box
        if ( is_point_in_rect( &item->icon_rect, x, y )
                        || is_point_in_rect( &item->text_rect, x, y ) )
            return item;
    }
    return NULL;
}

DesktopItem* hit_test_icon( DesktopWindow* self, int x, int y )
{   // hit on icon ?
    DesktopItem* item;
    GList* l;
    for ( l = self->items; l; l = l->next )
    {
        item = (DesktopItem*) l->data;
        if ( !item->fi )
            continue;  // empty box
        if ( is_point_in_rect( &item->icon_rect, x, y ) )
            return item;
    }
    return NULL;
}

gboolean hit_test_text( DesktopWindow* self, int x, int y,
                                                    DesktopItem** next_item )
{   // hit on text ?   sets next item
    DesktopItem* item;
    GList* l;
    for ( l = self->items; l; l = l->next )
    {
        item = (DesktopItem*) l->data;
        if ( !item->fi )
            continue;  // empty box
        if ( is_point_in_rect( &item->text_rect, x, y ) )
        {
            // hit text
            if ( l->next )
                item = (DesktopItem*)l->next->data;
            else
                item = NULL;
            if ( next_item )
                *next_item = item;
            return TRUE;
        }
    }
    if ( next_item )
        *next_item = NULL;
    return FALSE;
}

DesktopItem* hit_test_box( DesktopWindow* self, int x, int y )  //sfm
{   // hit on box ?
    DesktopItem* item;
    GList* l;

    for ( l = self->items; l; l = l->next )
    {
        item = (DesktopItem*)l->data;
        if ( is_point_in_rect( &item->box, x, y ) )
        {
            if ( item->fi && l->next &&
                         ((DesktopItem*)l->next->data)->box.x == item->box.x &&
                         y > item->text_rect.y )
                // clicked in lower area of non-empty box,
                // return next box if same column
                return (DesktopItem*)l->next->data;
            return item;
        }
    }
    
    // unlikely - no box was directly hit, so use closest
    GList* closest_l = NULL;
    float dist_min, dist;
    int x2, y2;
    for ( l = self->items; l; l = l->next )
    {
        item = (DesktopItem*)l->data;
        x2 = x - ( item->box.x + item->box.width / 2 );
        y2 = y - ( item->box.y + item->box.height / 2 );
        dist = sqrt( x2 * x2 + y2 * y2  );
        if ( !closest_l || dist < dist_min )
        {
            dist_min = dist;
            closest_l = l;
        }
    }
    //printf( "MISS %p  dist = %f\n", closest_l ? closest_l->data : NULL, dist_min );
    if ( !closest_l )
        return NULL;
    if ( !((DesktopItem*)closest_l->data)->fi )
        return (DesktopItem*)closest_l->data;
    if ( !( closest_l->next && ((DesktopItem*)closest_l->next->data)->box.x == 
                               ((DesktopItem*)closest_l->data)->box.x ) )
        // if closest is non-empty and last item in column, use next
        return closest_l->next ? (DesktopItem*)closest_l->next->data : NULL;
    return (DesktopItem*)closest_l->data;
}

/* FIXME: this is too dirty and here is some redundant code.
 *  We really need better and cleaner APIs for this */
void open_folders( GList* folders )
{
    FMMainWindow* main_window;
    gboolean new_window = FALSE;
    
    main_window = fm_main_window_get_on_current_desktop();
    
    if ( !main_window )
    {
        main_window = FM_MAIN_WINDOW(fm_main_window_new());
        //FM_MAIN_WINDOW( main_window ) ->splitter_pos = app_settings.splitter_pos;
        /*  now done in fm_main_window_new
        gtk_window_set_default_size( GTK_WINDOW( main_window ),
                                     app_settings.width,
                                     app_settings.height );
        gtk_widget_show( GTK_WIDGET(main_window) );
        */
        new_window = !xset_get_b( "main_save_tabs" );
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
        if ( new_window )
        {
            main_window_open_path_in_current_tab( main_window, path );
            new_window = FALSE;
        }
        else
            fm_main_window_add_new_tab( FM_MAIN_WINDOW( main_window ), path );

        g_free( path );
        folders = folders->next;
    }
    gtk_window_present( GTK_WINDOW( main_window ) );
}

void desktop_window_open_desktop_dir( GtkMenuItem *menuitem,
                                                    DesktopWindow* desktop )
{
    FMMainWindow* main_window;
    gboolean new_window = FALSE;
    
    const char* path = vfs_get_desktop_dir();
    if ( !desktop || !path )
        return;
    main_window = fm_main_window_get_on_current_desktop();
    
    if ( !main_window )
    {
        main_window = FM_MAIN_WINDOW( fm_main_window_new() );
        /*  now done in fm_main_window_new
        gtk_window_set_default_size( GTK_WINDOW( main_window ),
                                     app_settings.width,
                                     app_settings.height );
        gtk_widget_show( GTK_WIDGET(main_window) );
        */
        new_window = !xset_get_b( "main_save_tabs" );
    }
    
    if ( new_window )
    {
        main_window_open_path_in_current_tab( main_window, path );
        new_window = FALSE;
    }
    else
        fm_main_window_add_new_tab( FM_MAIN_WINDOW( main_window ), path );

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
            comp = NULL;
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
    //ret =g_utf8_collate( item1->fi->disp_name, item2->fi->disp_name );
    // natural icase
    ret = strcmp( item1->fi->collate_icase_key, item2->fi->collate_icase_key );
    if( win && win->sort_type == GTK_SORT_DESCENDING )
        ret = -ret;
    return ret;
}

int comp_item_by_size( DesktopItem* item1, DesktopItem* item2, DesktopWindow* win  )
{
    int ret;
    if( ret = COMP_VIRTUAL( item1, item2 ) )
        return ret;
    ret =item1->fi->size - item2->fi->size;

    if ( ret == 0 )  //sfm
        ret = g_utf8_collate( item1->fi->disp_name, item2->fi->disp_name );
    else if( win->sort_type == GTK_SORT_DESCENDING )
        ret = -ret;
    return ret;
}

int comp_item_by_mtime( DesktopItem* item1, DesktopItem* item2, DesktopWindow* win  )
{
    int ret;
    if( ret = COMP_VIRTUAL( item1, item2 ) )
        return ret;
    ret =item1->fi->mtime - item2->fi->mtime;

    if ( ret == 0 )  //sfm
        ret = g_utf8_collate( item1->fi->disp_name, item2->fi->disp_name );
    else if( win->sort_type == GTK_SORT_DESCENDING )
        ret = -ret;
    return ret;
}

int comp_item_by_type( DesktopItem* item1, DesktopItem* item2, DesktopWindow* win  )
{
    int ret;
    if( ret = COMP_VIRTUAL( item1, item2 ) )
        return ret;
    ret = strcmp( item1->fi->mime_type->type, item2->fi->mime_type->type );

    if ( ret == 0 )  //sfm
        ret = g_utf8_collate( item1->fi->disp_name, item2->fi->disp_name );
    else if( win->sort_type == GTK_SORT_DESCENDING )
        ret = -ret;
    return ret;
}

void redraw_item( DesktopWindow* win, DesktopItem* item )
{
    GdkRectangle rect = item->box;
    --rect.x;
    --rect.y;
    rect.width += 2;
    rect.height += 2;
    gdk_window_invalidate_rect( gtk_widget_get_window((GtkWidget*)win), &rect, FALSE );
}


/* ----------------- public APIs ------------------*/

void desktop_window_sort_items( DesktopWindow* win, DWSortType sort_by,
                                                            GtkSortType sort_type )
{
    if( win->sort_type == sort_type && win->sort_by == sort_by )
        return;

    app_settings.desktop_sort_by = win->sort_by = sort_by;
    app_settings.desktop_sort_type = win->sort_type = sort_type;
    
    if ( sort_by == DW_SORT_CUSTOM )
    {
        GHashTable* order_hash = custom_order_read( win );
        if ( order_hash )
        {
            // rebuild ordered item list
            GList* items = NULL;
            GList* unordered = NULL;
            GList* l, *ll;
            DesktopItem* item, *target_item;
            gpointer ptr;
            int order, i;
            for ( l = win->items; l; l = l->next )
            {
                item = (DesktopItem*)l->data;
                if ( G_UNLIKELY( !item->fi ) )
                {
                    desktop_item_free( item );
                    continue;
                }
                if ( ptr = g_hash_table_lookup( order_hash,
                                    vfs_file_info_get_name( item->fi ) ) )
                {
                    order = GPOINTER_TO_INT( ptr ) - 1;
                    ll = g_list_nth( items, order );
                    if ( !ll )
                    {
                        // position is beyond end of list - add empties
                        for ( i = g_list_length( items ); i < order; i++ )
                        {
                            target_item = g_slice_new0( DesktopItem );
                            target_item->fi = NULL;
                            items = g_list_append( items, target_item );                    
                        }
                        items = g_list_append( items, item );                    
                    }
                    else if ( ((DesktopItem*)ll->data)->fi )
                    {
                        // position already used - insert
                        items = g_list_insert( items, item, order + 1 );
                    }
                    else
                    {
                        // position empty - replace
                        desktop_item_free( (DesktopItem*)ll->data );
                        ll->data = item;
                    }
                }
                else
                {
                    // order not found
                    unordered = g_list_prepend( unordered, item );
                }
            }
            if ( unordered )
            {
                unordered = g_list_sort_with_data( unordered,
                                    (GCompareDataFunc)comp_item_by_name, win );
                items = g_list_concat( items, unordered );
            }
            g_list_free( win->items );
            win->items = items;
            g_hash_table_destroy( order_hash );
        }        
    }
    else
    {
        // remove empty boxes for non-custom sort
        DesktopItem* item;
        GList* items = win->items;
        //printf("\n-------------------\nREMOVE\n" );
        while ( items )
        {
            if ( ((DesktopItem*)items->data)->fi )
            {
                //item = (DesktopItem*)items->data; printf( "KEEP %s %d, %d  %p\n", vfs_file_info_get_name( item->fi ), item->box.x, item->box.y, item );
                items = items->next;
            }
            else
            {
                // remove
                item = (DesktopItem*)items->data;
                //printf("REMOVE empty %d, %d  %p\n", item->box.x, item->box.y, item );
                items = items->next;                
                win->items = g_list_remove( win->items, item );
                desktop_item_free( item );
            }
        }
        // sort
        GCompareDataFunc comp_func = get_sort_func( win );
        if ( comp_func )
            win->items = g_list_sort_with_data( win->items, comp_func, win );
    }

    // layout
    layout_items( win );
        
    /* //sfm 0.9.0 no longer using special items
    // skip the special items since they always appears first
    GList* items = NULL;
    gboolean special = FALSE;    //MOD added - otherwise caused infinite loop in layout items once My Documents was removed
    GList* special_items = win->items;
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

    // the previous item of the first non-special item is the last special item
    if( special && items->prev )
    {
        items->prev->next = NULL;
        items->prev = NULL;
    }

    GCompareDataFunc comp_func = get_sort_func( win );
    if ( comp_func )
        items = g_list_sort_with_data( items, comp_func, win );
    if ( special )
        win->items = g_list_concat( special_items, items );
    else
        win->items = items;
        
    layout_items( win );
    */
}

GList* desktop_window_get_selected_items( DesktopWindow* win,
                                          gboolean current_first )
{
    GList* sel = NULL;
    GList* l;

    for( l = win->items; l; l = l->next )
    {
        DesktopItem* item = (DesktopItem*) l->data;
        if ( item->is_selected && item->fi )
        {
            if( G_UNLIKELY( item == win->focus && current_first ) )
                sel = g_list_prepend( sel, item );
            else
                sel = g_list_append( sel, item );
        }
    }

    return sel;
}

GList* desktop_window_get_selected_files( DesktopWindow* win )
{
    GList* sel = desktop_window_get_selected_items( win, TRUE );
    GList* l;

    l = sel;
    while( l )
    {
        DesktopItem* item = (DesktopItem*) l->data;
        if ( item->fi->flags & VFS_FILE_INFO_VIRTUAL )
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

void desktop_window_add_application( DesktopWindow* desktop )
{
    char* app = NULL;
    VFSMimeType* mime_type;

    GList* sel_files = desktop_window_get_selected_files( desktop );
    if ( sel_files )
    {
        mime_type = vfs_file_info_get_mime_type( (VFSFileInfo*)sel_files->data );
        if ( G_LIKELY( ! mime_type ) )
            mime_type = vfs_mime_type_get_from_type( XDG_MIME_TYPE_UNKNOWN );
        g_list_foreach( sel_files, (GFunc)vfs_file_info_unref, NULL );
        g_list_free( sel_files );
    }
    else
        mime_type = vfs_mime_type_get_from_type( XDG_MIME_TYPE_DIRECTORY );

    // do not pass desktop parent - some WMs won't bring desktop dlg to top
    app = (char *) ptk_choose_app_for_mime_type( NULL,
                                        mime_type, TRUE, TRUE, FALSE, FALSE );
    if ( app )
    {
        char* path = vfs_mime_type_locate_desktop_file( NULL, app );
        if ( path && g_file_test( path, G_FILE_TEST_IS_REGULAR ) )
        {
            sel_files = g_list_prepend( NULL, path );
            PtkFileTask* task = ptk_file_task_new( VFS_FILE_TASK_LINK,
                                                   sel_files,
                                                   vfs_get_desktop_dir(),
                                                   NULL,
                                                   NULL );
            ptk_file_task_run( task );
        }
        else
            g_free( path );
        g_free( app );
    }
    vfs_mime_type_unref( mime_type );
}

static void custom_order_write( DesktopWindow* self )
{
    if ( self->sort_by != DW_SORT_CUSTOM || !self->file_listed )
        return;

    char* filename = g_strdup_printf( "desktop%d", self->screen_index );
    char* path = g_build_filename( xset_get_config_dir(), filename, NULL );
    g_free( filename );
    FILE* file = fopen( path, "w" );
    if ( file )
    {
        GList* l;
        
        fprintf( file, "~rows=%d\n", self->row_count );
        
        int i = 1; // start from 1 to detect atoi failure
        for ( l = self->items; l; l = l->next )
        {
            if ( ((DesktopItem*)l->data)->fi )
                fprintf( file, "%d=%s\n", i,
                        vfs_file_info_get_name( ((DesktopItem*)l->data)->fi ) );
            i++;
        }
        fclose( file );
    }
    else
        g_warning( "Error writing to file %s\n", path );
    g_free( path );
}

static GHashTable* custom_order_read( DesktopWindow* self )
{
    char line[ 2048 ];
    char* sep;
    int order;
    GHashTable* order_hash = NULL;

    self->order_rows = 0;    
    if ( self->sort_by != DW_SORT_CUSTOM )
        return NULL;
    char* filename = g_strdup_printf( "desktop%d", self->screen_index );
    char* path = g_build_filename( xset_get_config_dir(), filename, NULL );
    g_free( filename );
    
    FILE* file = fopen( path, "r" );
    if ( file )
    {
        order_hash = g_hash_table_new_full( g_str_hash, g_str_equal,
                                            (GDestroyNotify)g_free, NULL );
        while ( fgets( line, sizeof( line ), file ) )
        {
            strtok( line, "\r\n" );
            if ( sep = strchr( line, '=' ) )
            {
                sep[0] = '\0';
                if ( line[0] == '~' )
                {
                    // read setting
                    if ( !strcmp( line + 1, "rows" ) )
                    {
                        // get saved row count
                        order = atoi( sep + 1 );
                        if ( order != self->row_count )
                            self->order_rows = order;
                    }
                }
                else if ( !strchr( line, '_' ) )  // forward compat for attribs
                {
                    // read file order and name
                    order = atoi( line );
                    if ( order < 1 )
                        continue;
                    g_hash_table_insert( order_hash,
                                            (gpointer)g_strdup( sep + 1 ),
                                            GINT_TO_POINTER( order ) );
                }
            }
        }
        fclose( file );
    }
    g_free( path );    
    return order_hash;
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
            //printf("working area is resized   x,y=%d, %d   w,h=%d, %d\n",
            //        self->wa.x, self->wa.y, self->wa.width, self->wa.height);
            
            /* This doesn't seem to have the desired effect, and also
             * desktop window size should be based on screen size not WA
             * https://github.com/IgnorantGuru/spacefm/issues/300
            // resize desktop window
            GdkScreen* screen = gtk_widget_get_screen( GTK_WIDGET( self ) );
            int width = gdk_screen_get_width( screen );
            int height = gdk_screen_get_height( screen );
            if ( width && height )
                printf( "    screen size   w,h=%d, %d\n", width, height );
            gtk_window_resize( GTK_WINDOW( self ), self->wa.width, self->wa.height );
            gtk_window_move( GTK_WINDOW( self ), 0, 0 );
            // update wallpaper
            fm_desktop_update_wallpaper();
            */
            // layout icons
            if ( self->sort_by == DW_SORT_CUSTOM )
                self->order_rows = self->row_count; // possible change of row count in new layout
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
    xev.window = GDK_WINDOW_XID( gdk_screen_get_root_window( gscreen ) );
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
        if( gtk_widget_get_realized( (GtkWidget*)win ) )
            gdk_window_set_cursor( gtk_widget_get_window((GtkWidget*)win), NULL );
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

    // utils
    esc_path = bash_quote( xset_get_s( "editor" ) );
    fprintf( file, "fm_editor=%s\n", esc_path );
    g_free( esc_path );
    fprintf( file, "fm_editor_terminal=%d\n", xset_get_b( "editor" ) ? 1 : 0 );    

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

/* This does not detect screen size changes, only fires initially
 * and when desktop window is manually resized
gboolean on_configure_event( GtkWidget* w, GdkEventConfigure *event )
{
    DesktopWindow* self = (DesktopWindow*) w;
    
    printf("on_configure_event %p  x,y=%d, %d    w,h=%d, %d  file_listed = %s\n", self, event->x, event->y, event->width, event->height, self->file_listed ? "TRUE" : "FALSE" );
    get_working_area( gtk_widget_get_screen((GtkWidget*)self), &self->wa ); //temp
    printf("    working area is   x,y=%d, %d   w,h=%d, %d\n",
                        self->wa.x, self->wa.y, self->wa.width, self->wa.height);

    if ( self->file_listed )  // skip initial configure events
    {
        // possible desktop resize - get working area and redo layout
        printf( "    get_working_area\n");
        get_working_area( gtk_widget_get_screen(w), &self->wa );
        if ( self->sort_by == DW_SORT_CUSTOM )
            self->order_rows = self->row_count; // possible change of row count in new layout
        layout_items( self );
    }
    return FALSE;
}
*/
