#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>

#include <gtk/gtk.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <gdk/gdkkeysyms.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <fcntl.h>

#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#include "ptk-file-browser.h"

#include "exo-icon-view.h"
#include "exo-tree-view.h"

#include "mime-type/mime-type.h"

#include "ptk-app-chooser.h"

#include "ptk-file-icon-renderer.h"
#include "ptk-utils.h"
#include "ptk-input-dialog.h"
#include "ptk-file-task.h"
#include "ptk-file-misc.h"
#include "ptk-bookmarks.h"

#include "ptk-location-view.h"
#include "ptk-dir-tree-view.h"
#include "ptk-dir-tree.h"

#include "vfs-dir.h"
#include "vfs-utils.h"  //MOD
#include "vfs-file-info.h"
#include "vfs-file-monitor.h"
#include "vfs-app-desktop.h"
#include "ptk-file-list.h"
#include "ptk-text-renderer.h"

#include "ptk-file-archiver.h"
#include "ptk-clipboard.h"

#include "ptk-file-menu.h"
#include "ptk-path-entry.h"  //MOD
#include "find-files.h"  //MOD
#include "main-window.h"

//extern gboolean startup_mode; //MOD

extern char* run_cmd;  //MOD

static void ptk_file_browser_class_init( PtkFileBrowserClass* klass );
static void ptk_file_browser_init( PtkFileBrowser* file_browser );
static void ptk_file_browser_finalize( GObject *obj );
static void ptk_file_browser_get_property ( GObject *obj,
                                            guint prop_id,
                                            GValue *value,
                                            GParamSpec *pspec );
static void ptk_file_browser_set_property ( GObject *obj,
                                            guint prop_id,
                                            const GValue *value,
                                            GParamSpec *pspec );

/* Utility functions */
static GtkWidget* create_folder_view( PtkFileBrowser* file_browser,
                                      PtkFBViewMode view_mode );

static void init_list_view( PtkFileBrowser* file_browser, GtkTreeView* list_view );

static GtkTreeView* ptk_file_browser_create_dir_tree( PtkFileBrowser* file_browser );

static void on_dir_file_listed( VFSDir* dir,
                                             gboolean is_cancelled,
                                             PtkFileBrowser* file_browser );

void ptk_file_browser_open_selected_files_with_app( PtkFileBrowser* file_browser,
                                                    char* app_desktop );

static void
ptk_file_browser_cut_or_copy( PtkFileBrowser* file_browser,
                              gboolean copy );

static void
ptk_file_browser_update_model( PtkFileBrowser* file_browser );

static gboolean
is_latin_shortcut_key( guint keyval );

/* Get GtkTreePath of the item at coordinate x, y */
static GtkTreePath*
folder_view_get_tree_path_at_pos( PtkFileBrowser* file_browser, int x, int y );

#if 0
/* sort functions of folder view */
static gint sort_files_by_name ( GtkTreeModel *model,
                                 GtkTreeIter *a,
                                 GtkTreeIter *b,
                                 gpointer user_data );

static gint sort_files_by_size ( GtkTreeModel *model,
                                 GtkTreeIter *a,
                                 GtkTreeIter *b,
                                 gpointer user_data );

static gint sort_files_by_time ( GtkTreeModel *model,
                                 GtkTreeIter *a,
                                 GtkTreeIter *b,
                                 gpointer user_data );

static gboolean filter_files ( GtkTreeModel *model,
                               GtkTreeIter *it,
                               gpointer user_data );

/* sort functions of dir tree */
static gint sort_dir_tree_by_name ( GtkTreeModel *model,
                                    GtkTreeIter *a,
                                    GtkTreeIter *b,
                                    gpointer user_data );
#endif

/* signal handlers */
static void
on_folder_view_item_activated ( ExoIconView *iconview,
                                GtkTreePath *path,
                                PtkFileBrowser* file_browser );
static void
on_folder_view_row_activated ( GtkTreeView *tree_view,
                               GtkTreePath *path,
                               GtkTreeViewColumn* col,
                               PtkFileBrowser* file_browser );
static void
on_folder_view_item_sel_change ( ExoIconView *iconview,
                                 PtkFileBrowser* file_browser );

/*
static gboolean
on_folder_view_key_press_event ( GtkWidget *widget,
                                 GdkEventKey *event,
                                 PtkFileBrowser* file_browser );
*/
static gboolean
on_folder_view_button_press_event ( GtkWidget *widget,
                                    GdkEventButton *event,
                                    PtkFileBrowser* file_browser );
static gboolean
on_folder_view_button_release_event ( GtkWidget *widget,
                                      GdkEventButton *event,
                                      PtkFileBrowser* file_browser );
static gboolean
on_folder_view_popup_menu ( GtkWidget *widget,
                            PtkFileBrowser* file_browser );
#if 0
static void
on_folder_view_scroll_scrolled ( GtkAdjustment *adjust,
                                 PtkFileBrowser* file_browser );
#endif
static void
on_dir_tree_sel_changed ( GtkTreeSelection *treesel,
                          PtkFileBrowser* file_browser );
/*
static gboolean
on_location_view_button_press_event ( GtkTreeView* view,
                                      GdkEventButton* evt,
                                      PtkFileBrowser* file_browser );
*/
/* Drag & Drop */

static void
on_folder_view_drag_data_received ( GtkWidget *widget,
                                    GdkDragContext *drag_context,
                                    gint x,
                                    gint y,
                                    GtkSelectionData *sel_data,
                                    guint info,
                                    guint time,
                                    gpointer user_data );

static void
on_folder_view_drag_data_get ( GtkWidget *widget,
                               GdkDragContext *drag_context,
                               GtkSelectionData *sel_data,
                               guint info,
                               guint time,
                               PtkFileBrowser *file_browser );

static void
on_folder_view_drag_begin ( GtkWidget *widget,
                            GdkDragContext *drag_context,
                            gpointer user_data );

static gboolean
on_folder_view_drag_motion ( GtkWidget *widget,
                             GdkDragContext *drag_context,
                             gint x,
                             gint y,
                             guint time,
                             PtkFileBrowser* file_browser );

static gboolean
on_folder_view_drag_leave ( GtkWidget *widget,
                            GdkDragContext *drag_context,
                            guint time,
                            PtkFileBrowser* file_browser );

static gboolean
on_folder_view_drag_drop ( GtkWidget *widget,
                           GdkDragContext *drag_context,
                           gint x,
                           gint y,
                           guint time,
                           PtkFileBrowser* file_browser );

static void
on_folder_view_drag_end ( GtkWidget *widget,
                          GdkDragContext *drag_context,
                          gpointer user_data );

/* Default signal handlers */
static void ptk_file_browser_before_chdir( PtkFileBrowser* file_browser,
                                           const char* path,
                                           gboolean* cancel );
static void ptk_file_browser_after_chdir( PtkFileBrowser* file_browser );
static void ptk_file_browser_content_change( PtkFileBrowser* file_browser );
static void ptk_file_browser_sel_change( PtkFileBrowser* file_browser );
static void ptk_file_browser_open_item( PtkFileBrowser* file_browser, const char* path, int action );
static void ptk_file_browser_pane_mode_change( PtkFileBrowser* file_browser );
void ptk_file_browser_update_views( GtkWidget* item, PtkFileBrowser* file_browser );
void focus_folder_view( PtkFileBrowser* file_browser );
void on_shortcut_new_tab_here( GtkMenuItem* item,
                                          PtkFileBrowser* file_browser );
static void on_show_history_menu( GtkMenuToolButton* btn, PtkFileBrowser* file_browser );
void enable_toolbar( PtkFileBrowser* file_browser );

static int
file_list_order_from_sort_order( PtkFBSortOrder order );

static GtkPanedClass *parent_class = NULL;

enum {
    BEFORE_CHDIR_SIGNAL,
    BEGIN_CHDIR_SIGNAL,
    AFTER_CHDIR_SIGNAL,
    OPEN_ITEM_SIGNAL,
    CONTENT_CHANGE_SIGNAL,
    SEL_CHANGE_SIGNAL,
    PANE_MODE_CHANGE_SIGNAL,
    N_SIGNALS
};

enum{  //MOD
    RESPONSE_RUN = 100,
    RESPONSE_RUNTERMINAL = 101,
};

static void enter_callback( GtkEntry* entry, GtkDialog* dlg );  //MOD

static char *replace_str(char *str, char *orig, char *rep); //MOD

void ptk_file_browser_rebuild_toolbox( GtkWidget* widget, PtkFileBrowser* file_browser );
void ptk_file_browser_rebuild_side_toolbox( GtkWidget* widget, PtkFileBrowser* file_browser );

#include "settings.h"    //MOD

static guint signals[ N_SIGNALS ] = { 0 };

static guint folder_view_auto_scroll_timer = 0;
static GtkDirectionType folder_view_auto_scroll_direction = 0;

/*  Drag & Drop/Clipboard targets  */
static GtkTargetEntry drag_targets[] = {
                                           {"text/uri-list", 0 , 0 }
                                       };

#define GDK_ACTION_ALL  (GDK_ACTION_MOVE|GDK_ACTION_COPY|GDK_ACTION_LINK)

GType ptk_file_browser_get_type()
{
    static GType type = G_TYPE_INVALID;
    if ( G_UNLIKELY ( type == G_TYPE_INVALID ) )
    {
        static const GTypeInfo info =
            {
                sizeof ( PtkFileBrowserClass ),
                NULL,
                NULL,
                ( GClassInitFunc ) ptk_file_browser_class_init,
                NULL,
                NULL,
                sizeof ( PtkFileBrowser ),
                0,
                ( GInstanceInitFunc ) ptk_file_browser_init,
                NULL,
            };
        //type = g_type_register_static ( GTK_TYPE_HPANED, "PtkFileBrowser", &info, 0 );
        type = g_type_register_static ( GTK_TYPE_VBOX, "PtkFileBrowser", &info, 0 );
    }
    return type;
}

void ptk_file_browser_class_init( PtkFileBrowserClass* klass )
{
    GObjectClass * object_class;
    GtkWidgetClass *widget_class;

    object_class = ( GObjectClass * ) klass;
    parent_class = g_type_class_peek_parent ( klass );

    object_class->set_property = ptk_file_browser_set_property;
    object_class->get_property = ptk_file_browser_get_property;
    object_class->finalize = ptk_file_browser_finalize;

    widget_class = GTK_WIDGET_CLASS ( klass );

    /* Signals */

    klass->before_chdir = ptk_file_browser_before_chdir;
    klass->after_chdir = ptk_file_browser_after_chdir;
    klass->open_item = ptk_file_browser_open_item;
    klass->content_change = ptk_file_browser_content_change;
    klass->sel_change = ptk_file_browser_sel_change;
    klass->pane_mode_change = ptk_file_browser_pane_mode_change;

    /* before-chdir is emitted when PtkFileBrowser is about to change
    * its working directory. The 1st param is the path of the dir (in UTF-8),
    * and the 2nd param is a gboolean*, which can be filled by the
    * signal handler with TRUE to cancel the operation.
    */
    signals[ BEFORE_CHDIR_SIGNAL ] =
        g_signal_new ( "before-chdir",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET ( PtkFileBrowserClass, before_chdir ),
                       NULL, NULL,
                       gtk_marshal_VOID__POINTER_POINTER,
                       G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER );

    signals[ BEGIN_CHDIR_SIGNAL ] =
        g_signal_new ( "begin-chdir",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET ( PtkFileBrowserClass, begin_chdir ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0 );

    signals[ AFTER_CHDIR_SIGNAL ] =
        g_signal_new ( "after-chdir",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET ( PtkFileBrowserClass, after_chdir ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0 );

    /*
    * This signal is sent when a directory is about to be opened
    * arg1 is the path to be opened, and arg2 is the type of action,
    * ex: open in tab, open in terminal...etc.
    */
    signals[ OPEN_ITEM_SIGNAL ] =
        g_signal_new ( "open-item",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET ( PtkFileBrowserClass, open_item ),
                       NULL, NULL,
                       gtk_marshal_VOID__POINTER_INT, G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_INT );

    signals[ CONTENT_CHANGE_SIGNAL ] =
        g_signal_new ( "content-change",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET ( PtkFileBrowserClass, content_change ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0 );

    signals[ SEL_CHANGE_SIGNAL ] =
        g_signal_new ( "sel-change",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET ( PtkFileBrowserClass, sel_change ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0 );

    signals[ PANE_MODE_CHANGE_SIGNAL ] =
        g_signal_new ( "pane-mode-change",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_LAST,
                       G_STRUCT_OFFSET ( PtkFileBrowserClass, pane_mode_change ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0 );

}

gboolean ptk_file_browser_slider_release( GtkWidget *widget,
                                      GdkEventButton *event,
                                      PtkFileBrowser* file_browser )
{
    int pos;
    char* posa;
//printf("ptk_file_browser_slider_release\n");
    gboolean fullscreen = xset_get_b( "main_full" );

    if ( widget == file_browser->hpane )
    {
        pos = gtk_paned_get_position( file_browser->hpane );
        if ( !fullscreen )
        {
            posa = g_strdup_printf( "%d", pos );
            xset_set_panel( file_browser->mypanel, "slider_positions", "x", posa );
            g_free( posa );
        }
        *file_browser->slide_x = pos;
    }
    else
    {
        pos = gtk_paned_get_position( file_browser->side_vpane_top );
        if ( !fullscreen )
        {
            posa = g_strdup_printf( "%d", pos );
            xset_set_panel( file_browser->mypanel, "slider_positions", "y", posa );
            g_free( posa );
        }
        *file_browser->slide_y = pos;

        pos = gtk_paned_get_position( file_browser->side_vpane_bottom );
        if ( !fullscreen )
        {
            posa = g_strdup_printf( "%d", pos );
            xset_set_panel( file_browser->mypanel, "slider_positions", "s", posa );
            g_free( posa );
        }
        *file_browser->slide_s = pos;
    }
//printf("SAVEPOS %d %d\n", xset_get_int_panel( file_browser->mypanel, "slider_positions", "y" ),
//          xset_get_int_panel( file_browser->mypanel, "slider_positions", "s" )  );

    return FALSE;
}

void on_toolbar_hide( GtkWidget* widget, PtkFileBrowser* file_browser,
                                                            GtkWidget* toolbar2 )
{
    GtkWidget* toolbar;
    int i;

    if ( widget )
        toolbar = (GtkWidget*)g_object_get_data( G_OBJECT(widget), "toolbar" );
    else
        toolbar = toolbar2;
        
    if ( !toolbar )
        return;

    focus_folder_view( file_browser );
    if ( toolbar == file_browser->toolbar )
        xset_set_b_panel( file_browser->mypanel, "show_toolbox", FALSE );
    else
        xset_set_b_panel( file_browser->mypanel, "show_sidebar", FALSE );
    update_views_all_windows( NULL, file_browser );
}

void on_toolbar_help( GtkWidget* widget, PtkFileBrowser* file_browser )
{
    xset_msg_dialog( file_browser, 0, _("Toolbar Config Menu Help"), NULL, 0, _("These toolbar config menus allow you to customize the toolbars.\n\nEnter the Left Toolbar, Right Toolbar, or Side Toolbar submenu and right-click on an item to show or hide it, change the icon, or add a custom tool item.\n\nFor more information, click the Help button below."), NULL, "#designmode-toolbars" );
}

void on_toolbar_config_done( GtkWidget* widget, PtkFileBrowser* file_browser )
{
    if ( !widget || !file_browser )
        return;
        
    if ( !GTK_IS_WIDGET( widget ) )
        return;

    GtkToolbar* toolbar = (GtkToolbar*)g_object_get_data( G_OBJECT(widget), "toolbar" );

    gtk_widget_destroy( widget );
    
    if ( toolbar == file_browser->toolbar )
        rebuild_toolbar_all_windows( 0, file_browser );
    else if ( toolbar == file_browser->side_toolbar )
        rebuild_toolbar_all_windows( 1, file_browser );
}

void on_toolbar_config( GtkWidget* widget, PtkFileBrowser* file_browser )
{
    XSet* set;

    if ( !widget || !file_browser )
        return;
    GtkToolbar* toolbar = (GtkToolbar*)g_object_get_data( G_OBJECT(widget), "toolbar" );
    if ( !toolbar )
        return;

    focus_folder_view( file_browser );
    GtkMenu* popup = gtk_menu_new();
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    xset_context_new();

    // toolbar menus
    if ( toolbar == file_browser->toolbar )
    {
        set = xset_get( "toolbar_left" ); 
        xset_add_menuitem( NULL, file_browser, popup, accel_group, set );
        set = xset_get( "toolbar_right" ); 
        xset_add_menuitem( NULL, file_browser, popup, accel_group, set );
        xset_add_menuitem( NULL, file_browser, popup, accel_group,
                                                            xset_get( "sep_tool1" ) );
    }
    else if ( toolbar == file_browser->side_toolbar )
    {
        set = xset_get( "toolbar_side" ); 
        xset_add_menuitem( NULL, file_browser, popup, accel_group, set );        
        xset_add_menuitem( NULL, file_browser, popup, accel_group,
                                                        xset_get( "sep_tool2" ) );
    }
    else
        return;
                
    // Show/Hide Toolbars
    if ( toolbar == file_browser->toolbar )
    {
        set = xset_set_cb_panel( file_browser->mypanel, "show_sidebar",
                                            update_views_all_windows, file_browser );
        set->disable = ( !file_browser->side_dir && !file_browser->side_book
                                                    && !file_browser->side_dev );
        xset_add_menuitem( NULL, file_browser, popup, accel_group, set );

        set = xset_set_cb( "toolbar_hide", on_toolbar_hide, file_browser );
    }
    else
    {
        set = xset_set_cb_panel( file_browser->mypanel, "show_toolbox",
                                            update_views_all_windows, file_browser );
        xset_add_menuitem( NULL, file_browser, popup, accel_group, set );

        set = xset_set_cb( "toolbar_hide_side", on_toolbar_hide, file_browser );
    }
    xset_set_ob1( set, "toolbar", toolbar );
    xset_add_menuitem( NULL, file_browser, popup, accel_group, set );

    // Help
    if ( toolbar == file_browser->toolbar )
        set = xset_get( "sep_tool3" );
    else
        set = xset_get( "sep_tool4" );    
    xset_add_menuitem( NULL, file_browser, popup, accel_group, set );    
    set = xset_set_cb( "toolbar_help", on_toolbar_help, file_browser );
    xset_add_menuitem( NULL, file_browser, popup, accel_group, set );
    
    // show
    gtk_widget_show_all( GTK_WIDGET( popup ) );
    g_object_set_data( G_OBJECT( popup ), "toolbar", toolbar );
    g_signal_connect( popup, "selection-done",
                      G_CALLBACK( on_toolbar_config_done ), file_browser );
    g_signal_connect( popup, "key-press-event",
                      G_CALLBACK( xset_menu_keypress ), NULL );
    gtk_menu_popup( popup, NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time() );
}

void on_toggle_sideview( GtkMenuItem* item, PtkFileBrowser* file_browser, int job2  )
{
    focus_folder_view( file_browser );
    int job;
    if ( item )
        job = GPOINTER_TO_INT( g_object_get_data( G_OBJECT(item), "job" ) );
    else
        job = job2;
//printf("on_toggle_sideview  %d\n", job);
    
    if ( job == 0 )
        xset_set_b_panel( file_browser->mypanel, "show_dirtree", 
                !xset_get_b_panel( file_browser->mypanel, "show_dirtree" ) );
    else if ( job == 1 )
        xset_set_b_panel( file_browser->mypanel, "show_book", 
                !xset_get_b_panel( file_browser->mypanel, "show_book" ) );
    else if ( job == 2 )
        xset_set_b_panel( file_browser->mypanel, "show_devmon", 
                !xset_get_b_panel( file_browser->mypanel, "show_devmon" ) );
    update_views_all_windows( NULL, file_browser );
}

void ptk_file_browser_rebuild_side_toolbox( GtkWidget* widget,
                                                    PtkFileBrowser* file_browser )
{
    XSet* set;

    // destroy
    if ( file_browser->side_toolbar )
        gtk_widget_destroy( file_browser->side_toolbar );

    // new
    file_browser->side_toolbar = gtk_toolbar_new();
    GtkTooltips* tooltips = gtk_tooltips_new();
    gtk_box_pack_start( file_browser->side_toolbox, file_browser->side_toolbar,
                                                                TRUE, TRUE, 0 );
    gtk_toolbar_set_style( file_browser->side_toolbar, GTK_TOOLBAR_ICONS );
    if ( app_settings.tool_icon_size > 0 
                        && app_settings.tool_icon_size <= GTK_ICON_SIZE_DIALOG )
        gtk_toolbar_set_icon_size( file_browser->side_toolbar,
                                                    app_settings.tool_icon_size );

    // config
    GtkIconSize icon_size = gtk_toolbar_get_icon_size( GTK_TOOLBAR ( 
                                                    file_browser->side_toolbar ) );
    set = xset_set_cb( "toolbar_config", on_toolbar_config, file_browser );
    xset_add_toolitem( file_browser, file_browser, file_browser->side_toolbar,
                                                    tooltips, icon_size, set );

    // callbacks    
    set = xset_set_cb( "stool_dirtree", on_toggle_sideview, file_browser );
        xset_set_b( "stool_dirtree", xset_get_b_panel( file_browser->mypanel,
                                                                "show_dirtree" ) );
        xset_set_ob1_int( set, "job", 0 );
        set->ob2_data = NULL;
    set = xset_set_cb( "stool_book", on_toggle_sideview, file_browser );
        xset_set_b( "stool_book", xset_get_b_panel( file_browser->mypanel,
                                                                    "show_book" ) );
        xset_set_ob1_int( set, "job", 1 );
        set->ob2_data = NULL;
    set = xset_set_cb( "stool_device", on_toggle_sideview, file_browser );
        xset_set_b( "stool_device", xset_get_b_panel( file_browser->mypanel,
                                                                "show_devmon" ) );
        xset_set_ob1_int( set, "job", 2 );
        set->ob2_data = NULL;
    xset_set_cb( "stool_newtab", on_shortcut_new_tab_activate, file_browser );
    xset_set_cb( "stool_newtabhere", on_shortcut_new_tab_here, file_browser );
    set = xset_set_cb( "stool_back", ptk_file_browser_go_back, file_browser );
        set->ob2_data = NULL;
    set = xset_set_cb( "stool_backmenu", ptk_file_browser_go_back, file_browser );
        set->ob2_data = NULL;
    set = xset_set_cb( "stool_forward", ptk_file_browser_go_forward, file_browser );
        set->ob2_data = NULL;
    set = xset_set_cb( "stool_forwardmenu", ptk_file_browser_go_forward, file_browser );
        set->ob2_data = NULL;
    set = xset_set_cb( "stool_up", ptk_file_browser_go_up, file_browser );
        set->ob2_data = NULL;
    xset_set_cb( "stool_refresh", ptk_file_browser_refresh, file_browser );
    xset_set_cb( "stool_default", ptk_file_browser_go_default, file_browser );
    xset_set_cb( "stool_home", ptk_file_browser_go_home, file_browser );

    // side
    set = xset_get( "toolbar_side" );
    xset_add_toolbar( file_browser, file_browser, file_browser->side_toolbar,
                                                            tooltips, set->desc );

    // get side buttons
    set = xset_get( "stool_dirtree" );
    file_browser->toggle_btns_side[0] = set->ob2_data;
    set = xset_get( "stool_book" );
    file_browser->toggle_btns_side[1] = set->ob2_data;
    set = xset_get( "stool_device" );
    file_browser->toggle_btns_side[2] = set->ob2_data;

    set = xset_get( "stool_backmenu" );
    if ( file_browser->back_menu_btn_side = set->ob2_data )   // shown?
        g_signal_connect( G_OBJECT(file_browser->back_menu_btn_side), "show-menu",
                                G_CALLBACK(on_show_history_menu), file_browser );

    set = xset_get( "stool_forwardmenu" );
    if ( file_browser->forward_menu_btn_side = set->ob2_data )
        g_signal_connect( G_OBJECT(file_browser->forward_menu_btn_side), "show-menu",
                                G_CALLBACK(on_show_history_menu), file_browser );

    set = xset_get( "stool_back" );
    file_browser->back_btn[2] = set->ob2_data;
    set = xset_get( "stool_forward" );
    file_browser->forward_btn[2] = set->ob2_data;
    set = xset_get( "stool_up" );
    file_browser->up_btn[2] = set->ob2_data;

    // show
    if ( xset_get_b_panel( file_browser->mypanel, "show_sidebar" ) )
        gtk_widget_show_all( file_browser->side_toolbox );
    enable_toolbar( file_browser );
}

void select_file( PtkFileBrowser* file_browser, char* path )
{
    GtkTreeIter it;
    GtkTreePath* tree_path;
    GtkTreeSelection* tree_sel;
    GtkTreeModel* model = NULL;
    VFSFileInfo* file;
    char* file_name;

    char* name = g_path_get_basename( path );
    
    PtkFileList* list = PTK_FILE_LIST( file_browser->file_list );
    if ( file_browser->view_mode == PTK_FB_ICON_VIEW 
                                || file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        exo_icon_view_unselect_all( EXO_ICON_VIEW( file_browser->folder_view ) );
        model = exo_icon_view_get_model( EXO_ICON_VIEW( file_browser->folder_view ) );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        model = gtk_tree_view_get_model( GTK_TREE_VIEW( file_browser->folder_view ) );
        tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(
                                                        file_browser->folder_view ) );
        gtk_tree_selection_unselect_all( tree_sel );
    }
    if ( !model )
        return;

    if ( gtk_tree_model_get_iter_first( model, &it ) )
    {
        do
        {
            gtk_tree_model_get( model, &it, COL_FILE_INFO, &file, -1 );
            if ( file )
            {
                file_name = vfs_file_info_get_name( file );
                if ( !strcmp( file_name, name ) )
                {
                    tree_path = gtk_tree_model_get_path( GTK_TREE_MODEL(list), &it );
                    if ( file_browser->view_mode == PTK_FB_ICON_VIEW 
                                || file_browser->view_mode == PTK_FB_COMPACT_VIEW )
                    {
                        exo_icon_view_select_path( EXO_ICON_VIEW(
                                            file_browser->folder_view ), tree_path );
                        exo_icon_view_set_cursor( EXO_ICON_VIEW( 
                                file_browser->folder_view ), tree_path, NULL, FALSE );
                        exo_icon_view_scroll_to_path( EXO_ICON_VIEW(
                                file_browser->folder_view ), tree_path, TRUE, .25, 0 );
                    }
                    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
                    {
                        gtk_tree_selection_select_path( tree_sel, tree_path );
                        gtk_tree_view_set_cursor(GTK_TREE_VIEW( file_browser->folder_view ),
                                                                tree_path, NULL, FALSE);
                        gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW( file_browser->folder_view ),
                                                        tree_path, NULL, TRUE, .25, 0 );
                    }
                    vfs_file_info_unref( file );
                    break;
                }
                vfs_file_info_unref( file );
            }
        }
        while ( gtk_tree_model_iter_next( model, &it ) );
    }
    g_free( name );
}

gboolean on_address_bar_focus_in( GtkWidget *entry, GdkEventFocus* evt,
                                                    PtkFileBrowser* file_browser )
{
    ptk_file_browser_focus_me( file_browser );
    return FALSE;
}

void on_address_bar_activate( GtkWidget* entry, PtkFileBrowser* file_browser )
{
    char* text;
    gchar *dir_path, *final_path;
    GList* l;
    char* str;
    
    text = gtk_entry_get_text( GTK_ENTRY( entry ) );
        
    gtk_editable_select_region( (GtkEditable*)entry, 0, 0 );    // clear selection 
    // Convert to on-disk encoding
    dir_path = g_filename_from_utf8( text, -1, NULL, NULL, NULL );
    final_path = vfs_file_resolve_path( ptk_file_browser_get_cwd( file_browser ),
                                                                        dir_path );
    g_free( dir_path );
    
    if ( text[0] == '\0' )
    {
        g_free( final_path );
        return;
    }
    
    if ( !g_file_test( final_path, G_FILE_TEST_EXISTS ) &&
                ( text[0] == '$' || text[0] == '+' || text[0] == '&'
                  || text[0] == '!' || text[0] == '\0' ) )
    {
        // command
        char* command;
        char* trim_command;
        gboolean as_root = FALSE;
        gboolean in_terminal = FALSE;
        gboolean as_task = TRUE;
        char* prefix = g_strdup( "" );
        while ( text[0] == '$' || text[0] == '+' || text[0] == '&' || text[0] == '!' )
        {
            if ( text[0] == '+' )
                in_terminal = TRUE;
            else if ( text[0] == '&' )
                as_task = FALSE;
            else if ( text[0] == '!' )
                as_root = TRUE;
            
            str = prefix;
            prefix = g_strdup_printf( "%s%c", str, text[0] );
            g_free( str );
            text++;
        }
        command = g_strdup( text );
        trim_command = g_strstrip( command );
        if ( trim_command[0] == '\0' )
        {
            g_free( command );
            g_free( prefix );
            ptk_path_entry_help( entry, file_browser );
            gtk_editable_set_position( entry, -1 );
            return;
        }

        // save history
        EntryData* edata = (EntryData*)g_object_get_data( G_OBJECT( entry ), "edata" );
        text = gtk_entry_get_text( GTK_ENTRY( entry ) );
        if ( edata->current )
        {
            if ( !strcmp( edata->current->data, text ) )
            {
                // no change
                //printf( "    same as current\n");
            }
            else
            {
                //printf( "    append (!= current)\n");
                edata->history = g_list_append( edata->history, g_strdup( text ) );
                edata->current = g_list_last( edata->history );
            }
        }
        else //( !edata->current )
        {
            if ( edata->history )
            {
                l = g_list_last( edata->history );
                if ( !strcmp( l->data, text ) )
                {
                    // same as last history
                    //printf( "    same as history last  %s\n", l->data );
                }
                else
                {
                    //printf( "    append (!current history)\n");
                    edata->history = g_list_append( edata->history, g_strdup( text ) );
                    edata->current = g_list_last( edata->history );
                }
            }
            else
            {
                //printf( "    append (!current !history)\n");
                edata->history = g_list_append( edata->history, g_strdup( text ) );
                edata->current = g_list_last( edata->history );
            }
        }
        if ( edata->editing )
        {
            g_free( edata->editing );
            edata->editing = NULL;
        }
        if ( edata->history && g_list_length( edata->history ) > 50 )
            edata->history = g_list_delete_link( edata->history, edata->history );

        // task
        char* task_name = g_strdup( gtk_entry_get_text( GTK_ENTRY( entry ) ) );
        char* cwd = ptk_file_browser_get_cwd( file_browser );
        PtkFileTask* task = ptk_file_exec_new( task_name, cwd, file_browser,
                                                            file_browser->task_view );
        g_free( task_name );
        // don't free cwd!
        task->task->exec_browser = file_browser;
        task->task->exec_command = replace_line_subs( trim_command );
        g_free( command );
        if ( as_root )
            task->task->exec_as_user = g_strdup_printf( "root" );
        if ( !as_task )
            task->task->exec_sync = FALSE;
        else
            task->task->exec_sync = !in_terminal;
        task->task->exec_show_output = TRUE;
        task->task->exec_show_error = TRUE;
        task->task->exec_export = TRUE;
        task->task->exec_terminal = in_terminal;
        task->task->exec_keep_terminal = as_task;
        //task->task->exec_keep_tmp = TRUE;
        ptk_file_task_run( task );
        //gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );

        // reset entry text
        str = prefix;
        prefix = g_strdup_printf( "%s ", str );
        g_free( str );
        gtk_entry_set_text( entry, prefix );
        g_free( prefix );
        gtk_editable_set_position( entry, -1 );
        edata->current = NULL;
    }
    else
    {
        // path?
        // clean double slashes
        while ( strstr( final_path, "//" ) )
        {
            str = final_path;
            final_path = replace_string( str, "//", "/", FALSE );
            g_free( str );
        }
        if ( g_file_test( final_path, G_FILE_TEST_IS_DIR ) )
        {
            // open dir
            if ( strcmp( final_path, ptk_file_browser_get_cwd( file_browser ) ) )
                ptk_file_browser_chdir( file_browser, final_path, PTK_FB_CHDIR_ADD_HISTORY );
            gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );
        }
        else if ( g_file_test( final_path, G_FILE_TEST_EXISTS ) )
        {
            // open dir and select file
            dir_path = g_dirname( final_path );
            if ( strcmp( dir_path, ptk_file_browser_get_cwd( file_browser ) ) )
            {
                file_browser->select_path = strdup( final_path );
                ptk_file_browser_chdir( file_browser, dir_path, PTK_FB_CHDIR_ADD_HISTORY );
            }
            else
                select_file( file_browser, final_path );
            g_free( dir_path );
            gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );
        }
        gtk_editable_set_position( entry, -1 );
    }
    g_free( final_path );
}

void ptk_file_browser_rebuild_toolbox( GtkWidget* widget, PtkFileBrowser* file_browser )
{
//printf(" ptk_file_browser_rebuild_toolbox\n");
    XSet* set;
    
    if ( !file_browser )
        return;

    // destroy
    if ( file_browser->toolbar )
    {
        if ( GTK_IS_WIDGET( file_browser->toolbar ) )
        {
            printf("gtk_widget_destroy( file_browser->toolbar = %#x )\n", file_browser->toolbar );
            // crashing here? http://sourceforge.net/p/spacefm/tickets/88000/?page=0
            gtk_widget_destroy( file_browser->toolbar );  
            printf("    DONE\n" );
        }
        file_browser->toolbar = NULL;
        file_browser->path_bar = NULL;
    }

    if ( !file_browser->path_bar )
    {
        file_browser->path_bar = ( GtkEntry* )ptk_path_entry_new( file_browser );
        g_signal_connect( file_browser->path_bar, "activate",
                            G_CALLBACK(on_address_bar_activate), file_browser );
        g_signal_connect( file_browser->path_bar, "focus-in-event",
                                        G_CALLBACK(on_address_bar_focus_in), file_browser );
    }
    
    // new
    file_browser->toolbar = gtk_toolbar_new();
    GtkTooltips* tooltips = gtk_tooltips_new();
    gtk_box_pack_start( file_browser->toolbox, file_browser->toolbar, TRUE, TRUE, 0 );
    gtk_toolbar_set_style( file_browser->toolbar, GTK_TOOLBAR_ICONS );
    if ( app_settings.tool_icon_size > 0 
                        && app_settings.tool_icon_size <= GTK_ICON_SIZE_DIALOG )
        gtk_toolbar_set_icon_size( file_browser->toolbar, app_settings.tool_icon_size );

    // config
    GtkIconSize icon_size = gtk_toolbar_get_icon_size( GTK_TOOLBAR ( 
                                                            file_browser->toolbar ) );
    set = xset_set_cb( "toolbar_config", on_toolbar_config, file_browser );
    xset_add_toolitem( file_browser, file_browser, file_browser->toolbar, tooltips,
                                                                icon_size, set );

    // callbacks    
    set = xset_set_cb( "tool_dirtree", on_toggle_sideview, file_browser );
        xset_set_b( "tool_dirtree", xset_get_b_panel( file_browser->mypanel,
                                                                "show_dirtree" ) );
        xset_set_ob1_int( set, "job", 0 );
        set->ob2_data = NULL;
    set = xset_set_cb( "tool_book", on_toggle_sideview, file_browser );
        xset_set_b( "tool_book", xset_get_b_panel( file_browser->mypanel,
                                                                "show_book" ) );
        xset_set_ob1_int( set, "job", 1 );
        set->ob2_data = NULL;
    set = xset_set_cb( "tool_device", on_toggle_sideview, file_browser );
        xset_set_b( "tool_device", xset_get_b_panel( file_browser->mypanel,
                                                                "show_devmon" ) );
        xset_set_ob1_int( set, "job", 2 );
        set->ob2_data = NULL;
    xset_set_cb( "tool_newtab", on_shortcut_new_tab_activate, file_browser );
    xset_set_cb( "tool_newtabhere", on_shortcut_new_tab_here, file_browser );
    set = xset_set_cb( "tool_back", ptk_file_browser_go_back, file_browser );
        set->ob2_data = NULL;
    set = xset_set_cb( "tool_backmenu", ptk_file_browser_go_back, file_browser );
        set->ob2_data = NULL;
    set = xset_set_cb( "tool_forward", ptk_file_browser_go_forward, file_browser );
        set->ob2_data = NULL;
    set = xset_set_cb( "tool_forwardmenu", ptk_file_browser_go_forward, file_browser );
        set->ob2_data = NULL;
    set = xset_set_cb( "tool_up", ptk_file_browser_go_up, file_browser );
        set->ob2_data = NULL;
    xset_set_cb( "tool_refresh", ptk_file_browser_refresh, file_browser );
    xset_set_cb( "tool_default", ptk_file_browser_go_default, file_browser );
    xset_set_cb( "tool_home", ptk_file_browser_go_home, file_browser );

    // left
    set = xset_get( "toolbar_left" );
    xset_add_toolbar( file_browser, file_browser, file_browser->toolbar, tooltips,
                                                                        set->desc );

    // get left buttons
    set = xset_get( "tool_dirtree" );
    file_browser->toggle_btns_left[0] = set->ob2_data;
    set = xset_get( "tool_book" );
    file_browser->toggle_btns_left[1] = set->ob2_data;
    set = xset_get( "tool_device" );
    file_browser->toggle_btns_left[2] = set->ob2_data;

    set = xset_get( "tool_backmenu" );
    if ( file_browser->back_menu_btn_left = set->ob2_data )
        g_signal_connect( G_OBJECT(file_browser->back_menu_btn_left), "show-menu",
                                G_CALLBACK(on_show_history_menu), file_browser );

    set = xset_get( "tool_forwardmenu" );
    if ( file_browser->forward_menu_btn_left = set->ob2_data )
        g_signal_connect(  G_OBJECT(file_browser->forward_menu_btn_left), "show-menu",
                                G_CALLBACK(on_show_history_menu), file_browser );

    set = xset_get( "tool_back" );
    file_browser->back_btn[0] = set->ob2_data;
    set = xset_get( "tool_forward" );
    file_browser->forward_btn[0] = set->ob2_data;
    set = xset_get( "tool_up" );
    file_browser->up_btn[0] = set->ob2_data;
    
    // pathbar
    GtkHBox* hbox = gtk_hbox_new( FALSE, 0 );
    GtkToolItem* toolitem = gtk_tool_item_new();
    gtk_tool_item_set_expand ( toolitem, TRUE );
    gtk_toolbar_insert( GTK_TOOLBAR( file_browser->toolbar ), toolitem, -1 );
    gtk_container_add ( GTK_CONTAINER ( toolitem ), hbox );
    gtk_box_pack_start( GTK_BOX ( hbox ), GTK_WIDGET( file_browser->path_bar ),
                                                                    TRUE, TRUE, 5 );

    // callbacks right
    set = xset_set_cb( "rtool_dirtree", on_toggle_sideview, file_browser );
        xset_set_b( "rtool_dirtree", xset_get_b_panel( file_browser->mypanel, "show_dirtree" ) );
        xset_set_ob1_int( set, "job", 0 );
        set->ob2_data = NULL;
   set = xset_set_cb( "rtool_book", on_toggle_sideview, file_browser );
        xset_set_b( "rtool_book", xset_get_b_panel( file_browser->mypanel, "show_book" ) );
        xset_set_ob1_int( set, "job", 1 );
        set->ob2_data = NULL;
    set = xset_set_cb( "rtool_device", on_toggle_sideview, file_browser );
        xset_set_b( "rtool_device", xset_get_b_panel( file_browser->mypanel, "show_devmon" ) );
        xset_set_ob1_int( set, "job", 2 );
        set->ob2_data = NULL;
    xset_set_cb( "rtool_newtab", on_shortcut_new_tab_activate, file_browser );
    xset_set_cb( "rtool_newtabhere", on_shortcut_new_tab_here, file_browser );
    set = xset_set_cb( "rtool_back", ptk_file_browser_go_back, file_browser );
        set->ob2_data = NULL;
    set = xset_set_cb( "rtool_backmenu", ptk_file_browser_go_back, file_browser );
        set->ob2_data = NULL;
    set = xset_set_cb( "rtool_forward", ptk_file_browser_go_forward, file_browser );
        set->ob2_data = NULL;
    set = xset_set_cb( "rtool_forwardmenu", ptk_file_browser_go_forward, file_browser );
        set->ob2_data = NULL;
    set = xset_set_cb( "rtool_up", ptk_file_browser_go_up, file_browser );
        set->ob2_data = NULL;
    xset_set_cb( "rtool_refresh", ptk_file_browser_refresh, file_browser );
    xset_set_cb( "rtool_default", ptk_file_browser_go_default, file_browser );
    xset_set_cb( "rtool_home", ptk_file_browser_go_home, file_browser );

    // right
    set = xset_get( "rtool_backmenu" );
        set->ob2_data = NULL;
    set = xset_get( "rtool_forwardmenu" );
        set->ob2_data = NULL;
        
    set = xset_get( "toolbar_right" );
    xset_add_toolbar( file_browser, file_browser, file_browser->toolbar, tooltips,
                                                                    set->desc );

    // get right buttons
    set = xset_get( "rtool_dirtree" );
    file_browser->toggle_btns_right[0] = set->ob2_data;
    set = xset_get( "rtool_book" );
    file_browser->toggle_btns_right[1] = set->ob2_data;
    set = xset_get( "rtool_device" );
    file_browser->toggle_btns_right[2] = set->ob2_data;

    set = xset_get( "rtool_backmenu" );
    if ( file_browser->back_menu_btn_right = set->ob2_data )
        g_signal_connect(  G_OBJECT(file_browser->back_menu_btn_right), "show-menu",
                                G_CALLBACK(on_show_history_menu), file_browser );

    set = xset_get( "rtool_forwardmenu" );
    if ( file_browser->forward_menu_btn_right = set->ob2_data )
        g_signal_connect(  G_OBJECT(file_browser->forward_menu_btn_right), "show-menu",
                                G_CALLBACK(on_show_history_menu), file_browser );

    set = xset_get( "rtool_back" );
    file_browser->back_btn[1] = set->ob2_data;
    set = xset_get( "rtool_forward" );
    file_browser->forward_btn[1] = set->ob2_data;
    set = xset_get( "rtool_up" );
    file_browser->up_btn[1] = set->ob2_data;

    // show
    if ( xset_get_b_panel( file_browser->mypanel, "show_toolbox" ) )
        gtk_widget_show_all( file_browser->toolbox );
    enable_toolbar( file_browser );
}

void ptk_file_browser_status_change( PtkFileBrowser* file_browser, gboolean panel_focus )
{
    char* scolor;
    GdkColor color;
    
    // image
    gtk_widget_set_sensitive( file_browser->status_image, panel_focus );

    // text color
    if ( panel_focus )
    {
        scolor = xset_get_s( "status_text" );
        if ( scolor && gdk_color_parse( scolor, &color ) )
            gtk_widget_modify_fg( file_browser->status_label, GTK_STATE_NORMAL, &color );
        else
            gtk_widget_modify_fg( file_browser->status_label, GTK_STATE_NORMAL, NULL );
    }
    else
        gtk_widget_modify_fg( file_browser->status_label, GTK_STATE_NORMAL, NULL );

    // frame border color
    if ( panel_focus )
    {
        scolor = xset_get_s( "status_border" );
        if ( scolor && gdk_color_parse( scolor, &color ) )
            gtk_widget_modify_bg( file_browser->status_frame, GTK_STATE_NORMAL, &color );
        else
            gtk_widget_modify_bg( file_browser->status_frame, GTK_STATE_NORMAL, NULL );
            // below caused visibility issues with some themes
            //gtk_widget_modify_bg( file_browser->status_frame, GTK_STATE_NORMAL,
            //                            &GTK_WIDGET( file_browser->status_frame )
            //                            ->style->fg[ GTK_STATE_SELECTED ] );
    }
    else
        gtk_widget_modify_bg( file_browser->status_frame, GTK_STATE_NORMAL, NULL );
}

gboolean on_status_bar_button_press( GtkWidget *widget,
                                    GdkEventButton *event,
                                    PtkFileBrowser* file_browser )
{
    focus_folder_view( file_browser );
    if ( event->type == GDK_BUTTON_PRESS )
    {
        if ( event->button == 2 )
        {
            const char* setname[] =
            {
                "status_name",
                "status_path",
                "status_info",
                "status_hide"
            };
            int i;
            for ( i = 0; i < G_N_ELEMENTS( setname ); i++ )
            {
                if ( xset_get_b( setname[i] ) )
                {
                    if ( i < 2 )
                    {
                        GList* sel_files = ptk_file_browser_get_selected_files(
                                                                    file_browser );
                        if ( !sel_files )
                            return TRUE;
                        if ( i == 0 )
                            ptk_clipboard_copy_name( ptk_file_browser_get_cwd(
                                                    file_browser ), sel_files );                        
                        else
                            ptk_clipboard_copy_as_text( ptk_file_browser_get_cwd(
                                                        file_browser ), sel_files );
                        g_list_foreach( sel_files, ( GFunc ) vfs_file_info_unref, NULL );
                        g_list_free( sel_files );
                    }
                    else if ( i == 2 )
                        ptk_file_browser_file_properties( file_browser, 0 );
                    else if ( i == 3 )
                        focus_panel( NULL, file_browser->main_window, -3 );
                }
            }           
            return TRUE;
        }
    }
    return FALSE;
}

void on_status_effect_change( GtkMenuItem* item, PtkFileBrowser* file_browser )
{
    main_update_fonts( NULL, file_browser );
    set_panel_focus( NULL, file_browser );
}

void on_status_middle_click_config( GtkMenuItem *menuitem, XSet* set )
{
    const char* setname[] =
    {
        "status_name",
        "status_path",
        "status_info",
        "status_hide"
    };
    int i;
    for ( i = 0; i < G_N_ELEMENTS( setname ); i++ )
    {
        if ( !strcmp( set->name, setname[i] ) )
            set->b = XSET_B_TRUE;
        else
            xset_set_b( setname[i], FALSE );
    }
}

void on_status_bar_popup( GtkWidget *widget, GtkMenu *menu,
                                                PtkFileBrowser* file_browser )
{
    GSList* radio_group = NULL;
    XSetContext* context = xset_context_new();
    main_context_fill( file_browser, context );
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    char* desc = g_strdup_printf( "sep_bar1 status_border status_text panel%d_icon_status panel%d_font_status status_middle", file_browser->mypanel, file_browser->mypanel );

    xset_set_cb( "status_border", on_status_effect_change, file_browser );
    xset_set_cb( "status_text", on_status_effect_change, file_browser );
    xset_set_cb_panel( file_browser->mypanel, "icon_status",
                                        on_status_effect_change, file_browser );
    xset_set_cb_panel( file_browser->mypanel, "font_status",
                                        on_status_effect_change, file_browser );
    XSet* set = xset_get( "status_name" );
    xset_set_cb( "status_name", on_status_middle_click_config, set );
    xset_set_ob2( set, NULL, radio_group );
    set = xset_get( "status_path" );
    xset_set_cb( "status_path", on_status_middle_click_config, set );
    xset_set_ob2( set, NULL, radio_group );
    set = xset_get( "status_info" );
    xset_set_cb( "status_info", on_status_middle_click_config, set );
    xset_set_ob2( set, NULL, radio_group );
    set = xset_get( "status_hide" );
    xset_set_cb( "status_hide", on_status_middle_click_config, set );
    xset_set_ob2( set, NULL, radio_group );

    xset_add_menu( NULL, file_browser, menu, accel_group, desc );
    g_free( desc );
    gtk_widget_show_all( menu );
    g_signal_connect( menu, "key-press-event",
                      G_CALLBACK( xset_menu_keypress ), NULL );
}

/*
static gboolean on_status_bar_key_press( GtkWidget* widget, GdkEventKey* event,
                                                                gpointer user_data)
{
    printf( "on_status_bar_key_press\n");
    return FALSE;
}
*/

void ptk_file_browser_init( PtkFileBrowser* file_browser )
{
    // toolbox
    file_browser->path_bar = NULL;
    file_browser->toolbar = NULL;
    file_browser->toolbox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX ( file_browser ), file_browser->toolbox, FALSE, FALSE, 0 );
    ptk_file_browser_rebuild_toolbox( NULL, file_browser );

    // lists area
    file_browser->hpane = gtk_hpaned_new();
    file_browser->side_vbox = gtk_vbox_new( FALSE, 0 );
    file_browser->folder_view_scroll = gtk_scrolled_window_new( NULL, NULL );
    gtk_paned_pack1 ( file_browser->hpane, file_browser->side_vbox, TRUE, TRUE );
    gtk_paned_pack2 ( file_browser->hpane, file_browser->folder_view_scroll, TRUE, TRUE );

    // fill side
    file_browser->side_toolbox = gtk_hbox_new( FALSE, 0 );
    file_browser->side_toolbar = NULL;
    file_browser->side_vpane_top = gtk_vpaned_new();
    file_browser->side_vpane_bottom = gtk_vpaned_new();
    file_browser->side_dir_scroll = gtk_scrolled_window_new( NULL, NULL );
    file_browser->side_book_scroll = gtk_scrolled_window_new( NULL, NULL );
    file_browser->side_dev_scroll = gtk_scrolled_window_new( NULL, NULL );
    gtk_box_pack_start ( file_browser->side_vbox, file_browser->side_toolbox, FALSE, FALSE, 0 );
    gtk_box_pack_start ( file_browser->side_vbox, file_browser->side_vpane_top, TRUE, TRUE, 0 );
    gtk_paned_pack1 ( file_browser->side_vpane_top, file_browser->side_dev_scroll, TRUE, TRUE );
    gtk_paned_pack2 ( file_browser->side_vpane_top, file_browser->side_vpane_bottom, TRUE, TRUE );
    gtk_paned_pack1 ( file_browser->side_vpane_bottom, file_browser->side_book_scroll, TRUE, TRUE );
    gtk_paned_pack2 ( file_browser->side_vpane_bottom, file_browser->side_dir_scroll, TRUE, TRUE );

    // status bar
    file_browser->status_bar = gtk_statusbar_new();
    gtk_statusbar_set_has_resize_grip( file_browser->status_bar, FALSE );
 
    GList* children = gtk_container_get_children( GTK_BOX( file_browser->status_bar ) );
    file_browser->status_frame = GTK_FRAME( children->data );
    g_list_free( children );
    children = gtk_container_get_children( 
                GTK_BOX( gtk_statusbar_get_message_area( file_browser->status_bar ) ) );
    file_browser->status_label = GTK_LABEL( children->data );
    g_list_free( children );
    file_browser->status_image = xset_get_image( "gtk-yes", GTK_ICON_SIZE_MENU ); //don't know panel yet
    gtk_box_pack_start ( file_browser->status_bar, file_browser->status_image,
                                                                FALSE, FALSE, 0 );
    gtk_label_set_selectable( file_browser->status_label, TRUE ); // required for button event
    gtk_widget_set_can_focus( file_browser->status_label, FALSE );
    g_signal_connect( G_OBJECT( file_browser->status_label ), "button-press-event",
                      G_CALLBACK( on_status_bar_button_press ), file_browser );
    g_signal_connect( G_OBJECT( file_browser->status_label ), "populate-popup",
                      G_CALLBACK( on_status_bar_popup ), file_browser );
    //g_signal_connect( G_OBJECT( file_browser->status_label ), "key-press-event",
    //                  G_CALLBACK( on_status_bar_key_press ), file_browser );
    if ( xset_get_s_panel( file_browser->mypanel, "font_status" ) )
    {
        PangoFontDescription* font_desc = pango_font_description_from_string(
                        xset_get_s_panel( file_browser->mypanel, "font_status" ) );
        gtk_widget_modify_font( file_browser->status_label, font_desc );
        pango_font_description_free( font_desc );
    }

    // pack fb vbox
    gtk_box_pack_start( GTK_BOX ( file_browser ), file_browser->hpane, TRUE, TRUE, 0 );
    // TODO pack task frames
    gtk_box_pack_start( GTK_BOX ( file_browser ), file_browser->status_bar, FALSE, FALSE, 0 );

    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( file_browser->folder_view_scroll ),
                                     GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( file_browser->side_dir_scroll ),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( file_browser->side_book_scroll ),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( file_browser->side_dev_scroll ),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );

    g_signal_connect( file_browser->hpane, "button-release-event",
                      G_CALLBACK( ptk_file_browser_slider_release ), file_browser );
    g_signal_connect( file_browser->side_vpane_top, "button-release-event",
                      G_CALLBACK( ptk_file_browser_slider_release ), file_browser );
    g_signal_connect( file_browser->side_vpane_bottom, "button-release-event",
                      G_CALLBACK( ptk_file_browser_slider_release ), file_browser );
/*
    // these work but fire too often
    g_signal_connect( file_browser->hpane, "notify::position",
                      G_CALLBACK( on_slider_change ), file_browser );
    g_signal_connect( file_browser->side_vpane_top, "notify::position",
                      G_CALLBACK( on_slider_change ), file_browser );
    g_signal_connect( file_browser->side_vpane_bottom, "notify::position",
                      G_CALLBACK( on_slider_change ), file_browser );
*/
}

void ptk_file_browser_finalize( GObject *obj )
{
    PtkFileBrowser * file_browser = PTK_FILE_BROWSER( obj );

    if ( file_browser->dir )
    {
        g_signal_handlers_disconnect_matched( file_browser->dir,
                                              G_SIGNAL_MATCH_DATA,
                                              0, 0, NULL, NULL,
                                              file_browser );
        g_object_unref( file_browser->dir );
    }

    /* Remove all idle handlers which are not called yet. */
    do
    {}
    while ( g_source_remove_by_user_data( file_browser ) );

    if ( file_browser->file_list )
    {
        g_signal_handlers_disconnect_matched( file_browser->file_list,
                                              G_SIGNAL_MATCH_DATA,
                                              0, 0, NULL, NULL,
                                              file_browser );
        g_object_unref( G_OBJECT( file_browser->file_list ) );
    }

    G_OBJECT_CLASS( parent_class ) ->finalize( obj );
}

void ptk_file_browser_get_property ( GObject *obj,
                                     guint prop_id,
                                     GValue *value,
                                     GParamSpec *pspec )
{}

void ptk_file_browser_set_property ( GObject *obj,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec )
{}


/* File Browser API */

/*
static gboolean side_pane_chdir( PtkFileBrowser* file_browser,
                                 const char* folder_path )
{
    if ( file_browser->side_dir )
        return ptk_dir_tree_view_chdir( file_browser->side_dir, folder_path );
    
    if ( file_browser->side_dev )
    {
        ptk_location_view_chdir( file_browser->side_dev, folder_path );
        return TRUE;
    }

    if ( file_browser->side_pane_mode == PTK_FB_SIDE_PANE_BOOKMARKS )
    {
        ptk_location_view_chdir( file_browser->side_view, folder_path );
        return TRUE;
    }
    else if ( file_browser->side_pane_mode == PTK_FB_SIDE_PANE_DIR_TREE )
    {
        return ptk_dir_tree_view_chdir( file_browser->side_view, folder_path );
    }

    return FALSE;
}
*/
/*
void create_side_views( PtkFileBrowser* file_browser, int mode )
{
    GtkScrolledWindow* scroll;
    GtkTreeView* view;
    if ( !mode )
    {
        scroll = file_browser->side_dir_scroll;
        view = file_browser->side_dir;
        if ( !view )
            view = ptk_file_browser_create_dir_tree( file_browser );
    }
    else if ( mode == 1 )
    {
        scroll = file_browser->side_book_scroll;
        view = file_browser->side_book;
        //if ( !view )
        //    view = ptk_file_browser_create_bookmarks_view( file_browser );
    }
    else
    {
        scroll = file_browser->side_dev_scroll;
        view = file_browser->side_dev;
        if ( !view )
            view = ptk_file_browser_create_location_view( file_browser );
    }
}
*/

void ptk_file_browser_update_views( GtkWidget* item, PtkFileBrowser* file_browser )
{
    int i;
//printf("ptk_file_browser_update_views\n");

    // hide/show browser widgets based on user settings
    int p = file_browser->mypanel;

    if ( xset_get_b_panel( p, "show_toolbox" ) )
    {
        if ( !file_browser->toolbox )
            ptk_file_browser_rebuild_toolbox( NULL, file_browser );
        gtk_widget_show_all( file_browser->toolbox );
    }
    else
        gtk_widget_hide( file_browser->toolbox );
    
    if ( xset_get_b_panel( p, "show_sidebar" ) )
    {
        if ( !file_browser->side_toolbar )
            ptk_file_browser_rebuild_side_toolbox( NULL, file_browser );
        gtk_widget_show_all( file_browser->side_toolbox );
    }
    else
    {
        if ( file_browser->side_toolbar )
        {
            gtk_widget_destroy( file_browser->side_toolbar );
            file_browser->side_toolbar = NULL;
            for ( i = 0; i < 3; i++ )
                file_browser->toggle_btns_side[i] = NULL;
        }
        gtk_widget_hide( file_browser->side_toolbox );
    }
    
    if ( xset_get_b_panel( p, "show_dirtree" ) )
    {
        if ( !file_browser->side_dir )
        {
            file_browser->side_dir = ptk_file_browser_create_dir_tree( file_browser );
            gtk_container_add( file_browser->side_dir_scroll, file_browser->side_dir );
        }
        gtk_widget_show_all( file_browser->side_dir_scroll );
        if ( file_browser->side_dir && file_browser->file_list )
            ptk_dir_tree_view_chdir( file_browser->side_dir,
                                    ptk_file_browser_get_cwd( file_browser ) );
    }
    else
    {
        gtk_widget_hide( file_browser->side_dir_scroll );
        if ( file_browser->side_dir )
            gtk_widget_destroy( file_browser->side_dir );
        file_browser->side_dir = NULL;
    }
    
    if ( xset_get_b_panel( p, "show_book" ) )
    {
        if ( !file_browser->side_book )
        {
            file_browser->side_book = ptk_bookmark_view_new( file_browser );
            gtk_container_add( file_browser->side_book_scroll, file_browser->side_book );
        }
        gtk_widget_show_all( file_browser->side_book_scroll );
    }
    else
    {
        gtk_widget_hide( file_browser->side_book_scroll );
        if ( file_browser->side_book )
            gtk_widget_destroy( file_browser->side_book );
        file_browser->side_book = NULL;
    }

    if ( xset_get_b_panel( p, "show_devmon" ) )
    {
        if ( !file_browser->side_dev )
        {
            file_browser->side_dev = ptk_location_view_new( file_browser );
            gtk_container_add( file_browser->side_dev_scroll, file_browser->side_dev );
        }
        gtk_widget_show_all( file_browser->side_dev_scroll );
    }
    else
    {
        gtk_widget_hide( file_browser->side_dev_scroll );
        if ( file_browser->side_dev )
            gtk_widget_destroy( file_browser->side_dev );
        file_browser->side_dev = NULL;
    }

    if ( xset_get_b_panel( p, "show_book" ) || xset_get_b_panel( p, "show_dirtree" ) )
        gtk_widget_show( file_browser->side_vpane_bottom );
    else
        gtk_widget_hide( file_browser->side_vpane_bottom );
    
    if ( xset_get_b_panel( p, "show_devmon" ) || xset_get_b_panel( p, "show_dirtree" )
                                        || xset_get_b_panel( p, "show_book" ) )
        gtk_widget_show( file_browser->side_vbox );
    else
        gtk_widget_hide( file_browser->side_vbox );

    // toggle dirtree toolbar buttons
    gboolean b;
    for ( i = 0; i < 3; i++ )
    {
        if ( i == 0 )
            b = xset_get_b_panel( p, "show_dirtree" );
        else if ( i == 1 )
            b = xset_get_b_panel( p, "show_book" );
        else
            b = xset_get_b_panel( p, "show_devmon" );
        if ( file_browser->toggle_btns_left[i] )
        {
            g_signal_handlers_block_matched( file_browser->toggle_btns_left[i],
                                                G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                                on_toggle_sideview, NULL );
            gtk_toggle_tool_button_set_active( file_browser->toggle_btns_left[i], b );
            g_signal_handlers_unblock_matched( file_browser->toggle_btns_left[i],
                                                G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                                on_toggle_sideview, NULL );
        }
        if ( file_browser->toggle_btns_right[i] )
        {
            g_signal_handlers_block_matched( file_browser->toggle_btns_right[i],
                                                G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                                on_toggle_sideview, NULL );
            gtk_toggle_tool_button_set_active( file_browser->toggle_btns_right[i], b );
            g_signal_handlers_unblock_matched( file_browser->toggle_btns_right[i],
                                                G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                                on_toggle_sideview, NULL );
        }
        if ( file_browser->toggle_btns_side[i] )
        {
            g_signal_handlers_block_matched( file_browser->toggle_btns_side[i],
                                                G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                                on_toggle_sideview, NULL );
            gtk_toggle_tool_button_set_active( file_browser->toggle_btns_side[i], b );
            g_signal_handlers_unblock_matched( file_browser->toggle_btns_side[i],
                                                G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                                on_toggle_sideview, NULL );
        }
    }
    
    // set slider positions
/*
 * // don't need to block signals for release event method
    g_signal_handlers_block_matched( file_browser->hpane, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                     on_slider_change, NULL );
    g_signal_handlers_block_matched( file_browser->side_vpane_top, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                     on_slider_change, NULL );
    g_signal_handlers_block_matched( file_browser->side_vpane_bottom, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                     on_slider_change, NULL );
*/
    //int pos = xset_get_int_panel( file_browser->mypanel, "slider_positions", "x" );
    // read each slider's pos from dynamic
    int pos = *file_browser->slide_x;
    if ( pos < 100 ) pos = -1;
    gtk_paned_set_position( file_browser->hpane, pos );

    //pos = xset_get_int_panel( file_browser->mypanel, "slider_positions", "y" );
    pos = *file_browser->slide_y;
    if ( pos < 20 ) pos = -1;
    gtk_paned_set_position( file_browser->side_vpane_top, pos );
    while (gtk_events_pending ()) // let other sliders adjust
        gtk_main_iteration ();
        
    //pos = xset_get_int_panel( file_browser->mypanel, "slider_positions", "s" );
    pos = *file_browser->slide_s;
    if ( pos < 20 ) pos = -1;
    gtk_paned_set_position( file_browser->side_vpane_bottom, pos );
    
//printf("SETPOS %d %d\n", xset_get_int_panel( file_browser->mypanel, "slider_positions", "y" ),
//          xset_get_int_panel( file_browser->mypanel, "slider_positions", "s" )  );
    
    // save slider positions (they change when set)
    ptk_file_browser_slider_release( NULL, NULL, file_browser );
    
/*
    g_signal_handlers_unblock_matched( file_browser->hpane, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                       on_slider_change, NULL );
    g_signal_handlers_unblock_matched( file_browser->side_vpane_top, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                       on_slider_change, NULL );
    g_signal_handlers_unblock_matched( file_browser->side_vpane_bottom, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                       on_slider_change, NULL );
*/

    // List Styles
    if ( xset_get_b_panel( p, "list_detailed" ) )
        ptk_file_browser_view_as_list( file_browser );
    else if ( xset_get_b_panel( p, "list_icons" ) )
        ptk_file_browser_view_as_icons( file_browser );
    else if ( xset_get_b_panel( p, "list_compact" ) )
        ptk_file_browser_view_as_compact_list( file_browser );
    else
    {
        xset_set_panel( p, "list_detailed", "b", "1" );
        ptk_file_browser_view_as_list( file_browser );
    }

    // Show Hidden
    ptk_file_browser_show_hidden_files( file_browser,
                            xset_get_b_panel( p, "show_hidden" ) );

    // Set column visibility, save widths
    if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
        on_folder_view_columns_changed( file_browser->folder_view,
                                                        file_browser );

//printf("ptk_file_browser_update_views DONE\n");
}

GtkWidget* ptk_file_browser_new( int curpanel, GtkNotebook* notebook,
                                                    GtkTreeView* task_view,
                                                    gpointer main_window,
                                                    int* slide_x,
                                                    int* slide_y,
                                                    int* slide_s )
{
    PtkFileBrowser * file_browser;
    PtkFBViewMode view_mode;
    file_browser = ( PtkFileBrowser* ) g_object_new( PTK_TYPE_FILE_BROWSER, NULL );
    
    file_browser->mypanel = curpanel;
    file_browser->mynotebook = notebook;
    file_browser->main_window = main_window;
    file_browser->task_view = task_view;
    file_browser->slide_x = slide_x;
    file_browser->slide_y = slide_y;
    file_browser->slide_s = slide_s;

    if ( xset_get_b_panel( curpanel, "list_detailed" ) )
        view_mode = PTK_FB_LIST_VIEW;
    else if ( xset_get_b_panel( curpanel, "list_icons" ) )
        view_mode = PTK_FB_ICON_VIEW;
    else if ( xset_get_b_panel( curpanel, "list_compact" ) )
        view_mode = PTK_FB_COMPACT_VIEW;
    else
    {
        xset_set_panel( curpanel, "list_detailed", "b", "1" );
        view_mode = PTK_FB_LIST_VIEW;
    }

    file_browser->view_mode = view_mode;  //sfm was after next line
    file_browser->folder_view = create_folder_view( file_browser, view_mode );

    gtk_container_add ( GTK_CONTAINER ( file_browser->folder_view_scroll ),
                        file_browser->folder_view );

    file_browser->side_dir = NULL;
    file_browser->side_book = NULL;
    file_browser->side_dev = NULL;

    file_browser->select_path = NULL;

    //gtk_widget_show_all( file_browser->folder_view_scroll );

    // set status bar icon
    char* icon_name;
    XSet* set = xset_get_panel( curpanel, "icon_status" );
    if ( set->icon && set->icon[0] != '\0' )
        icon_name = set->icon;
    else
        icon_name = "gtk-yes";
    gtk_image_set_from_icon_name( file_browser->status_image, icon_name,
                                                            GTK_ICON_SIZE_MENU );
    // set status bar font
    char* fontname = xset_get_s_panel( curpanel, "font_status" );
    if ( fontname )
    {
        PangoFontDescription* font_desc = pango_font_description_from_string( fontname );
        gtk_widget_modify_font( file_browser->status_label, font_desc );
        pango_font_description_free( font_desc );
    }

    gtk_widget_show_all( file_browser );
    
    ptk_file_browser_update_views( NULL, file_browser );
   
    return ( GtkWidget* ) file_browser;
}

gboolean ptk_file_restrict_homedir( const char* folder_path ) {
    const char *homedir = NULL;
    int ret=(1==0);
    
    homedir = g_getenv("HOME");
    if (!homedir) {
      homedir = g_get_home_dir();
    }
    if (g_str_has_prefix(folder_path,homedir)) {
      ret=(1==1);
    }
    if (g_str_has_prefix(folder_path,"/media")) {
      ret=(1==1);
    }
    return ret;
}

void ptk_file_browser_update_tab_label( PtkFileBrowser* file_browser )
{
    GtkWidget * label;
    GtkContainer* hbox;
    GtkImage* icon;
    GtkLabel* text;
    GList* children;
    gchar* name;

    label = gtk_notebook_get_tab_label ( file_browser->mynotebook,
                                         GTK_WIDGET( file_browser ) );
    hbox = GTK_CONTAINER( gtk_bin_get_child ( GTK_BIN( label ) ) );
    children = gtk_container_get_children( hbox );
    icon = GTK_IMAGE( children->data );
    text = GTK_LABEL( children->next->data );
    g_list_free( children );

    /* TODO: Change the icon */

    name = g_path_get_basename( ptk_file_browser_get_cwd( file_browser ) );
    gtk_label_set_text( text, name );
    g_free( name );
}

void ptk_file_browser_select_last( PtkFileBrowser* file_browser ) //MOD added
{
//printf("ptk_file_browser_select_last\n");
    // select one file?
    if ( file_browser->select_path )
    {
        select_file( file_browser, file_browser->select_path );
        g_free( file_browser->select_path );
        file_browser->select_path = NULL;
        return;
    }

    // select previously selected files
    gint elementn = -1;
    GList* l;
    GList* element = NULL;
    //printf("    search for %s\n", (char*)file_browser->curHistory->data );
    
    if ( file_browser->history && file_browser->histsel && file_browser->curHistory &&
                                        ( l = g_list_last( file_browser->history ) ) )
    {
        if ( l->data && !strcmp( (char*)l->data, (char*)file_browser->curHistory->data ) )
        {
            elementn = g_list_position( file_browser->history, l );
            if ( elementn != -1 )
            {
                element = g_list_nth( file_browser->histsel, elementn );
                // skip the current history item if sellist empty since it was just created
                if ( !element->data )
                {
                    //printf( "        found current empty\n");
                    element = NULL;
                }
                //else printf( "        found current NON-empty\n");
            }
        }
        if ( !element )
        {
            while ( l = l->prev )
            {
                if ( l->data && !strcmp( (char*)l->data, (char*)file_browser->curHistory->data ) )
                {
                    elementn = g_list_position( file_browser->history, l );
                    //printf ("        found elementn=%d\n", elementn );
                    if ( elementn != -1 )
                        element = g_list_nth( file_browser->histsel, elementn );
                    break;
                }
            }
        }
    }
    
/*    
    if ( element )
    {
        g_debug ("element OK" );
        if ( element->data )
            g_debug ("element->data OK" );
        else
            g_debug ("element->data NULL" );
    }
    else
        g_debug ("element NULL" );
    g_debug ("histsellen=%d", g_list_length( file_browser->histsel ) );
*/
    if ( element && element->data )
    {
        //printf("    select files\n");
        PtkFileList* list = PTK_FILE_LIST( file_browser->file_list );
        GtkTreeIter it;
        GtkTreePath* tp;
        GtkTreeSelection* tree_sel;
        gboolean firstsel = TRUE;
        if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
            tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( file_browser->folder_view ) );
        for ( l = element->data; l; l = l->next )
        {
            if ( l->data )
            {
                //g_debug ("find a file");
                VFSFileInfo* file = l->data;
                if( ptk_file_list_find_iter( list, &it, file ) )
                {
                    //g_debug ("found file");
                    tp = gtk_tree_model_get_path( GTK_TREE_MODEL(list), &it );
                    if ( file_browser->view_mode == PTK_FB_ICON_VIEW || file_browser->view_mode == PTK_FB_COMPACT_VIEW )
                    {
                        exo_icon_view_select_path( EXO_ICON_VIEW( file_browser->folder_view ), tp );
                        if ( firstsel )
                        {
                            exo_icon_view_set_cursor( EXO_ICON_VIEW( file_browser->folder_view ), tp, NULL, FALSE );
                            exo_icon_view_scroll_to_path( EXO_ICON_VIEW( file_browser->folder_view ),
                                                                    tp, TRUE, .25, 0 );
                            firstsel = FALSE;
                        }
                    }
                    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
                    {
                        gtk_tree_selection_select_path( tree_sel, tp );
                        if ( firstsel )
                        {
                            gtk_tree_view_set_cursor(GTK_TREE_VIEW( file_browser->folder_view ), tp, NULL, FALSE);
                            gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW( file_browser->folder_view ),
                                                                tp, NULL, TRUE, .25, 0 );
                            firstsel = FALSE;
                        }
                    }
                    gtk_tree_path_free( tp );
                }
            }
        }
    }
}

void enable_toolbar( PtkFileBrowser* file_browser )
{
    char* cwd = ptk_file_browser_get_cwd( file_browser );  // may be NULL
    int i;
    for ( i = 0; i < 3; i++ )
    {
        if ( i < 2 && !file_browser->toolbar )
            continue;
        else if ( i == 2 && !file_browser->side_toolbar )
            continue;
        if ( file_browser->back_btn[i] )
            gtk_widget_set_sensitive( file_browser->back_btn[i],
                                                file_browser->curHistory &&
                                                file_browser->curHistory->prev );
        if ( file_browser->forward_btn[i] )
            gtk_widget_set_sensitive( file_browser->forward_btn[i],
                                                file_browser->curHistory &&
                                                file_browser->curHistory->next );
        if ( file_browser->up_btn[i] )
            gtk_widget_set_sensitive( file_browser->up_btn[i],
                                        !cwd || ( cwd && strcmp( cwd, "/" ) ) );
    }
    if ( file_browser->toolbar && file_browser->back_menu_btn_left )
        gtk_widget_set_sensitive( file_browser->back_menu_btn_left,
                                                file_browser->curHistory &&
                                                file_browser->curHistory->prev );
    if ( file_browser->toolbar && file_browser->forward_menu_btn_left )
        gtk_widget_set_sensitive( file_browser->forward_menu_btn_left,
                                                file_browser->curHistory &&
                                                file_browser->curHistory->next );
    if ( file_browser->toolbar && file_browser->back_menu_btn_right )
        gtk_widget_set_sensitive( file_browser->back_menu_btn_right,
                                                file_browser->curHistory &&
                                                file_browser->curHistory->prev );
    if ( file_browser->toolbar && file_browser->forward_menu_btn_right )
        gtk_widget_set_sensitive( file_browser->forward_menu_btn_right,
                                                file_browser->curHistory &&
                                                file_browser->curHistory->next );
    if ( file_browser->side_toolbar && file_browser->back_menu_btn_side )
        gtk_widget_set_sensitive( file_browser->back_menu_btn_side,
                                                file_browser->curHistory &&
                                                file_browser->curHistory->prev );
    if ( file_browser->side_toolbar && file_browser->forward_menu_btn_side )
        gtk_widget_set_sensitive( file_browser->forward_menu_btn_side,
                                                file_browser->curHistory &&
                                                file_browser->curHistory->next );
}

gboolean ptk_file_browser_chdir( PtkFileBrowser* file_browser,
                                 const char* folder_path,
                                 PtkFBChdirMode mode )
{
    gboolean cancel = FALSE;
    GtkWidget* folder_view = file_browser->folder_view;
//printf("ptk_file_browser_chdir\n");
    char* path_end;
    int test_access;
    char* path;
    char* msg;

    file_browser->button_press = FALSE;
    if ( ! folder_path )
        return FALSE;

    if ( folder_path )
    {
        path = strdup( folder_path );
        /* remove redundent '/' */
        if ( strcmp( path, "/" ) )
        {
            path_end = path + strlen( path ) - 1;
            for ( ; path_end > path; --path_end )
            {
                if ( *path_end != '/' )
                    break;
                else
                    *path_end = '\0';
            }
        }
    }
    else
        path = NULL;

    if ( ! path || ! g_file_test( path, ( G_FILE_TEST_IS_DIR ) ) )
    {
        msg = g_strdup_printf( _("Directory doesn't exist\n\n%s"), path );
        ptk_show_error( GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ) ),
                        _("Error"),
                        msg );
        if ( path )
            g_free( path );
        g_free( msg );
        return FALSE;
    }

    /* FIXME: check access */
#if defined(HAVE_EUIDACCESS)
    test_access = euidaccess( path, R_OK | X_OK );
#elif defined(HAVE_EACCESS)
    test_access = eaccess( path, R_OK | X_OK );
#else   /* No check */
    test_access = 0;
#endif

    if ( test_access == -1 )
    {
        msg = g_strdup_printf( _("Unable to access %s\n\n%s"), path, 
                                    g_markup_escape_text(g_strerror( errno ), -1) );
        ptk_show_error( GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ) ),
                        _("Error"),
                        msg );
        g_free(msg);
        return FALSE;
    }

    g_signal_emit( file_browser, signals[ BEFORE_CHDIR_SIGNAL ], 0, path, &cancel );

    if( cancel )
        return FALSE;

    //MOD remember selected files
    //g_debug ("@@@@@@@@@@@ remember: %s", ptk_file_browser_get_cwd( file_browser ) );
    if ( file_browser->curhistsel && file_browser->curhistsel->data )
    {
        //g_debug ("free curhistsel");
        g_list_foreach ( file_browser->curhistsel->data, ( GFunc ) vfs_file_info_unref, NULL );
        g_list_free( file_browser->curhistsel->data );
    }
    if ( file_browser->curhistsel )
    {
        file_browser->curhistsel->data = ptk_file_browser_get_selected_files( file_browser );
         
        //g_debug("set curhistsel %d", g_list_position( file_browser->histsel, file_browser->curhistsel ) );
        //if ( file_browser->curhistsel->data )
        //    g_debug ("curhistsel->data OK" );
        //else
        //    g_debug ("curhistsel->data NULL" );
        
    }

    if ( mode == PTK_FB_CHDIR_ADD_HISTORY )
    {
        if ( ! file_browser->curHistory || strcmp( (char*)file_browser->curHistory->data, path ) )
        {
            /* Has forward history */
            if ( file_browser->curHistory && file_browser->curHistory->next )
            {
                /* clear old forward history */
                g_list_foreach ( file_browser->curHistory->next, ( GFunc ) g_free, NULL );
                g_list_free( file_browser->curHistory->next );
                file_browser->curHistory->next = NULL;
            }
            //MOD added - make histsel shadow file_browser->history
            if ( file_browser->curhistsel && file_browser->curhistsel->next )
            {
                //g_debug("@@@@@@@@@@@ free forward");
                GList* l;
                for ( l = file_browser->curhistsel->next; l; l = l->next )
                {
                    if ( l->data )
                    {
                        //g_debug("free forward item");
                        g_list_foreach ( l->data, ( GFunc ) vfs_file_info_unref, NULL );
                        g_list_free( l->data );
                    }
                }
                g_list_free( file_browser->curhistsel->next );
                file_browser->curhistsel->next = NULL;
            }
            /* Add path to history if there is no forward history */
            file_browser->history = g_list_append( file_browser->history, path );
            file_browser->curHistory = g_list_last( file_browser->history );
            //MOD added - make histsel shadow file_browser->history
            GList* sellist = NULL;
            file_browser->histsel = g_list_append( file_browser->histsel, sellist );
            file_browser->curhistsel = g_list_last( file_browser->histsel );
        }
    }
    else if( mode == PTK_FB_CHDIR_BACK )
    {
        file_browser->curHistory = file_browser->curHistory->prev;
        file_browser->curhistsel = file_browser->curhistsel->prev;  //MOD
    }
    else if( mode == PTK_FB_CHDIR_FORWARD )
    {
        file_browser->curHistory = file_browser->curHistory->next;
        file_browser->curhistsel = file_browser->curhistsel->next;  //MOD
    }

    // remove old dir object
    if ( file_browser->dir )
    {
        g_signal_handlers_disconnect_matched( file_browser->dir,
                                              G_SIGNAL_MATCH_DATA,
                                              0, 0, NULL, NULL,
                                              file_browser );
        g_object_unref( file_browser->dir );
    }

    if ( file_browser->view_mode == PTK_FB_ICON_VIEW || file_browser->view_mode == PTK_FB_COMPACT_VIEW )
        exo_icon_view_set_model( EXO_ICON_VIEW( folder_view ), NULL );
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
        gtk_tree_view_set_model( GTK_TREE_VIEW( folder_view ), NULL );

    file_browser->dir = vfs_dir_get_by_path( path );

    if( ! file_browser->curHistory || path != (char*)file_browser->curHistory->data )
        g_free( path );

    g_signal_emit( file_browser, signals[ BEGIN_CHDIR_SIGNAL ], 0 );

    if( vfs_dir_is_file_listed( file_browser->dir ) )
    {
        on_dir_file_listed( file_browser->dir, FALSE, file_browser );
    }
    else
        file_browser->busy = TRUE;
    g_signal_connect( file_browser->dir, "file-listed",
                                    G_CALLBACK(on_dir_file_listed), file_browser );

    ptk_file_browser_update_tab_label( file_browser );

    char* disp_path = g_filename_display_name( ptk_file_browser_get_cwd( file_browser ) );
    gtk_entry_set_text( file_browser->path_bar, disp_path );
    EntryData* edata = (EntryData*)g_object_get_data(
                                    G_OBJECT( file_browser->path_bar ), "edata" );
    if ( edata )
        edata->current = NULL;
    g_free( disp_path );

    enable_toolbar( file_browser );
    return TRUE;
}

static void on_history_menu_item_activate( GtkWidget* menu_item,
                                                    PtkFileBrowser* file_browser )
{
    GList* l = (GList*)g_object_get_data( G_OBJECT(menu_item), "path"), *tmp;
    tmp = file_browser->curHistory;
    file_browser->curHistory = l;

    if( !  ptk_file_browser_chdir( file_browser, (char*)l->data,
                                                    PTK_FB_CHDIR_NO_HISTORY ) )
        file_browser->curHistory = tmp;
    else
    {
        //MOD sync curhistsel
        gint elementn = -1;
        elementn = g_list_position( file_browser->history, file_browser->curHistory );
        if ( elementn != -1 )
            file_browser->curhistsel = g_list_nth( file_browser->histsel, elementn );
        else
            g_debug("missing history item - ptk-file-browser.c");
    }
}

static GtkWidget* add_history_menu_item( PtkFileBrowser* file_browser,
                                                    GtkWidget* menu, GList* l )
{
    GtkWidget* menu_item, *folder_image;
    char *disp_name;
    disp_name = g_filename_display_basename( (char*)l->data );
    menu_item = gtk_image_menu_item_new_with_label( disp_name );
    g_object_set_data( G_OBJECT( menu_item ), "path", l );
    folder_image = gtk_image_new_from_icon_name( "gnome-fs-directory",
                                                 GTK_ICON_SIZE_MENU );
    gtk_image_menu_item_set_image ( GTK_IMAGE_MENU_ITEM ( menu_item ),
                                    folder_image );
    g_signal_connect( menu_item, "activate",
                      G_CALLBACK( on_history_menu_item_activate ), file_browser );

    gtk_menu_shell_append( GTK_MENU_SHELL(menu), menu_item );
    return menu_item;
}

static void on_show_history_menu( GtkMenuToolButton* btn, PtkFileBrowser* file_browser )
{
    //GtkMenuShell* menu = (GtkMenuShell*)gtk_menu_tool_button_get_menu(btn);
    GtkMenu* menu = gtk_menu_new();
    GList *l;

    if ( btn == (GtkMenuToolButton*)file_browser->back_menu_btn_left 
        || btn == (GtkMenuToolButton*)file_browser->back_menu_btn_right
        || btn == (GtkMenuToolButton*)file_browser->back_menu_btn_side )
    {
        // back history
        for( l = file_browser->curHistory->prev; l != NULL; l = l->prev )
            add_history_menu_item( file_browser, GTK_WIDGET(menu), l );
    }
    else
    {
        // forward history
        for( l = file_browser->curHistory->next; l != NULL; l = l->next )
            add_history_menu_item( file_browser, GTK_WIDGET(menu), l );
    }
    gtk_menu_tool_button_set_menu( btn, menu );
    gtk_widget_show_all( GTK_WIDGET(menu) );
}

#if 0
static gboolean
ptk_file_browser_delayed_content_change( PtkFileBrowser* file_browser )
{
    GTimeVal t;
    g_get_current_time( &t );
    file_browser->prev_update_time = t.tv_sec;
    g_signal_emit( file_browser, signals[ CONTENT_CHANGE_SIGNAL ], 0 );
    file_browser->update_timeout = 0;
    return FALSE;
}
#endif

#if 0
void on_folder_content_update ( FolderContent* content,
                                PtkFileBrowser* file_browser )
{
    /*  FIXME: Newly added or deleted files should not be delayed.
        This must be fixed before 0.2.0 release.  */
    GTimeVal t;
    g_get_current_time( &t );
    /*
      Previous update is < 5 seconds before.
      Queue the update, and don't update the view too often
    */
    if ( ( t.tv_sec - file_browser->prev_update_time ) < 5 )
    {
        /*
          If the update timeout has been set, wait until the timeout happens, and
          don't do anything here.
        */
        if ( 0 == file_browser->update_timeout )
        { /* No timeout callback. Add one */
            /* Delay the update */
            file_browser->update_timeout = g_timeout_add( 5000,
                                                          ( GSourceFunc ) ptk_file_browser_delayed_content_change, file_browser );
        }
    }
    else if ( 0 == file_browser->update_timeout )
    { /* No timeout callback. Add one */
        file_browser->prev_update_time = t.tv_sec;
        g_signal_emit( file_browser, signals[ CONTENT_CHANGE_SIGNAL ], 0 );
    }
}
#endif

#if 0   //MOD  these were used to signal main_window
static gboolean ptk_file_browser_content_changed( PtkFileBrowser* file_browser )
{
    gdk_threads_enter();
    g_signal_emit( file_browser, signals[ CONTENT_CHANGE_SIGNAL ], 0 );
    gdk_threads_leave();
    return FALSE;
}

static void on_folder_content_changed( VFSDir* dir, VFSFileInfo* file,
                                       PtkFileBrowser* file_browser )
{
    g_idle_add( ( GSourceFunc ) ptk_file_browser_content_changed,
                file_browser );
}
#endif

static void on_file_deleted( VFSDir* dir, VFSFileInfo* file,
                                        PtkFileBrowser* file_browser )
{
    /* The folder itself was deleted */
    if( file == NULL )
    {
        on_close_notebook_page( NULL, file_browser );
        //ptk_file_browser_chdir( file_browser, g_get_home_dir(), PTK_FB_CHDIR_ADD_HISTORY);
    }
    //else
    //    on_folder_content_changed( dir, file, file_browser );
}

static void on_sort_col_changed( GtkTreeSortable* sortable,
                                 PtkFileBrowser* file_browser )
{
    int col;

    gtk_tree_sortable_get_sort_column_id( sortable,
                                          &col, &file_browser->sort_type );

    switch ( col )
    {
    case COL_FILE_NAME:
        col = PTK_FB_SORT_BY_NAME;
        break;
    case COL_FILE_SIZE:
        col = PTK_FB_SORT_BY_SIZE;
        break;
    case COL_FILE_MTIME:
        col = PTK_FB_SORT_BY_MTIME;
        break;
    case COL_FILE_DESC:
        col = PTK_FB_SORT_BY_TYPE;
        break;
    case COL_FILE_PERM:
        col = PTK_FB_SORT_BY_PERM;
        break;
    case COL_FILE_OWNER:
        col = PTK_FB_SORT_BY_OWNER;
        break;
    }
    file_browser->sort_order = col;
    //MOD enable following to make column click permanent sort
//    app_settings.sort_order = col;
//    if ( file_browser )
//        ptk_file_browser_set_sort_order( PTK_FILE_BROWSER( file_browser ), app_settings.sort_order );

    char* val = g_strdup_printf( "%d", col );
    xset_set_panel( file_browser->mypanel, "list_detailed", "x", val );
    g_free( val );
    val = g_strdup_printf( "%d", file_browser->sort_type );
    xset_set_panel( file_browser->mypanel, "list_detailed", "y", val );
    g_free( val );
}

void ptk_file_browser_update_model( PtkFileBrowser* file_browser )
{
    PtkFileList * list;
    GtkTreeModel *old_list;

    list = ptk_file_list_new( file_browser->dir,
                              file_browser->show_hidden_files );
    old_list = file_browser->file_list;
    file_browser->file_list = GTK_TREE_MODEL( list );
    if ( old_list )
        g_object_unref( G_OBJECT( old_list ) );

    gtk_tree_sortable_set_sort_column_id(
        GTK_TREE_SORTABLE( list ),
        file_list_order_from_sort_order( file_browser->sort_order ),
        file_browser->sort_type );

    ptk_file_list_show_thumbnails( list,
                                   ( file_browser->view_mode == PTK_FB_ICON_VIEW ),
                                   file_browser->max_thumbnail );
    g_signal_connect( list, "sort-column-changed",
                      G_CALLBACK( on_sort_col_changed ), file_browser );

    if ( file_browser->view_mode == PTK_FB_ICON_VIEW || file_browser->view_mode == PTK_FB_COMPACT_VIEW )
        exo_icon_view_set_model( EXO_ICON_VIEW( file_browser->folder_view ),
                                 GTK_TREE_MODEL( list ) );
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
        gtk_tree_view_set_model( GTK_TREE_VIEW( file_browser->folder_view ),
                                 GTK_TREE_MODEL( list ) );

// try to smooth list bounce created by delayed re-appearance of column headers 
//while( gtk_events_pending() )
//    gtk_main_iteration();

}

void on_dir_file_listed( VFSDir* dir,
                                             gboolean is_cancelled,
                                             PtkFileBrowser* file_browser )
{
    file_browser->n_sel_files = 0;

    if ( G_LIKELY( ! is_cancelled ) )
    {
        //g_signal_connect( dir, "file-created",
        //                  G_CALLBACK( on_folder_content_changed ), file_browser );
        g_signal_connect( dir, "file-deleted",
                          G_CALLBACK( on_file_deleted ), file_browser );
        //g_signal_connect( dir, "file-changed",
        //                  G_CALLBACK( on_folder_content_changed ), file_browser );
    }

    ptk_file_browser_update_model( file_browser );

    file_browser->busy = FALSE;

    g_signal_emit( file_browser, signals[ AFTER_CHDIR_SIGNAL ], 0 );
    //g_signal_emit( file_browser, signals[ CONTENT_CHANGE_SIGNAL ], 0 );
    g_signal_emit( file_browser, signals[ SEL_CHANGE_SIGNAL ], 0 );

    if ( file_browser->side_dir )
        ptk_dir_tree_view_chdir( file_browser->side_dir,
                                ptk_file_browser_get_cwd( file_browser ) );

/*
    if ( file_browser->side_pane )
    if ( ptk_file_browser_is_side_pane_visible( file_browser ) )
    {
        side_pane_chdir( file_browser,
                         ptk_file_browser_get_cwd( file_browser ) );
    }
*/


    //FIXME:  This is already done in update_model, but is there any better way to
    //            reduce unnecessary code?
    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {   //sfm why is this needed for compact view???
        if ( G_LIKELY(! is_cancelled) && file_browser->file_list )
        {
            ptk_file_list_show_thumbnails( PTK_FILE_LIST( file_browser->file_list ),
                                           file_browser->view_mode == PTK_FB_ICON_VIEW,
                                           file_browser->max_thumbnail );
        }
    }
}

const char* ptk_file_browser_get_cwd( PtkFileBrowser* file_browser )
{
    if ( ! file_browser->curHistory )
        return NULL;
    return ( const char* ) file_browser->curHistory->data;
}

gboolean ptk_file_browser_is_busy( PtkFileBrowser* file_browser )
{
    return file_browser->busy;
}

gboolean ptk_file_browser_can_back( PtkFileBrowser* file_browser )
{
    /* there is back history */
    return ( file_browser->curHistory && file_browser->curHistory->prev );
}

void ptk_file_browser_go_back( GtkWidget* item, PtkFileBrowser* file_browser )
{
    const char * path;

    focus_folder_view( file_browser );
    /* there is no back history */
    if ( ! file_browser->curHistory || ! file_browser->curHistory->prev )
        return;
    path = ( const char* ) file_browser->curHistory->prev->data;
    ptk_file_browser_chdir( file_browser, path, PTK_FB_CHDIR_BACK );
}

gboolean ptk_file_browser_can_forward( PtkFileBrowser* file_browser )
{
    /* If there is forward history */
    return ( file_browser->curHistory && file_browser->curHistory->next );
}

void ptk_file_browser_go_forward( GtkWidget* item, PtkFileBrowser* file_browser )
{
    const char * path;

    focus_folder_view( file_browser );
    /* If there is no forward history */
    if ( ! file_browser->curHistory || ! file_browser->curHistory->next )
        return ;
    path = ( const char* ) file_browser->curHistory->next->data;
    ptk_file_browser_chdir( file_browser, path, PTK_FB_CHDIR_FORWARD );
}

void ptk_file_browser_go_up( GtkWidget* item, PtkFileBrowser* file_browser )
{
    char * parent_dir;

    focus_folder_view( file_browser );
    parent_dir = g_path_get_dirname( ptk_file_browser_get_cwd( file_browser ) );
    if( strcmp( parent_dir, ptk_file_browser_get_cwd( file_browser ) ) )
        ptk_file_browser_chdir( file_browser, parent_dir, PTK_FB_CHDIR_ADD_HISTORY);
    g_free( parent_dir );
}

void ptk_file_browser_go_home( GtkWidget* item, PtkFileBrowser* file_browser )
{
//    if ( app_settings.home_folder )
//        ptk_file_browser_chdir( PTK_FILE_BROWSER( file_browser ), app_settings.home_folder, PTK_FB_CHDIR_ADD_HISTORY );
//    else
    focus_folder_view( file_browser );
        ptk_file_browser_chdir( PTK_FILE_BROWSER( file_browser ), g_get_home_dir(), PTK_FB_CHDIR_ADD_HISTORY );
}

void ptk_file_browser_go_default( GtkWidget* item, PtkFileBrowser* file_browser )
{
    focus_folder_view( file_browser );
    char* path = xset_get_s( "go_set_default" );
    if ( path && path[0] != '\0' )
        ptk_file_browser_chdir( PTK_FILE_BROWSER( file_browser ), path,
                                                    PTK_FB_CHDIR_ADD_HISTORY );
    else
        ptk_file_browser_chdir( PTK_FILE_BROWSER( file_browser ), g_get_home_dir(),
                                                    PTK_FB_CHDIR_ADD_HISTORY );
}

void ptk_file_browser_set_default_folder( GtkWidget* item, PtkFileBrowser* file_browser )
{
    xset_set( "go_set_default", "s", ptk_file_browser_get_cwd( file_browser ) );
}

GtkWidget* ptk_file_browser_get_folder_view( PtkFileBrowser* file_browser )
{
    return file_browser->folder_view;
}

/* FIXME: unused function */
GtkTreeView* ptk_file_browser_get_dir_tree( PtkFileBrowser* file_browser )
{
    return NULL;
}

void ptk_file_browser_select_all( GtkWidget* item, PtkFileBrowser* file_browser )
{
    GtkTreeSelection * tree_sel;
    if ( file_browser->view_mode == PTK_FB_ICON_VIEW || file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        exo_icon_view_select_all( EXO_ICON_VIEW( file_browser->folder_view ) );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( file_browser->folder_view ) );
        gtk_tree_selection_select_all( tree_sel );
    }
}

void ptk_file_browser_unselect_all( GtkWidget* item, PtkFileBrowser* file_browser )
{
    GtkTreeSelection * tree_sel;
    if ( file_browser->view_mode == PTK_FB_ICON_VIEW || file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        exo_icon_view_unselect_all( EXO_ICON_VIEW( file_browser->folder_view ) );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( file_browser->folder_view ) );
        gtk_tree_selection_unselect_all( tree_sel );
    }
}

static gboolean
invert_selection ( GtkTreeModel* model,
                   GtkTreePath *path,
                   GtkTreeIter* it,
                   PtkFileBrowser* file_browser )
{
    GtkTreeSelection * tree_sel;
    if ( file_browser->view_mode == PTK_FB_ICON_VIEW || file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        if ( exo_icon_view_path_is_selected ( EXO_ICON_VIEW( file_browser->folder_view ), path ) )
            exo_icon_view_unselect_path ( EXO_ICON_VIEW( file_browser->folder_view ), path );
        else
            exo_icon_view_select_path ( EXO_ICON_VIEW( file_browser->folder_view ), path );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( file_browser->folder_view ) );
        if ( gtk_tree_selection_path_is_selected ( tree_sel, path ) )
            gtk_tree_selection_unselect_path ( tree_sel, path );
        else
            gtk_tree_selection_select_path ( tree_sel, path );
    }
    return FALSE;
}

void ptk_file_browser_invert_selection( GtkWidget* item, PtkFileBrowser* file_browser )
{
    GtkTreeModel * model;
    if ( file_browser->view_mode == PTK_FB_ICON_VIEW || file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        model = exo_icon_view_get_model( EXO_ICON_VIEW( file_browser->folder_view ) );
        g_signal_handlers_block_matched( file_browser->folder_view,
                                         G_SIGNAL_MATCH_FUNC,
                                         0, 0, NULL,
                                         on_folder_view_item_sel_change, NULL );
        gtk_tree_model_foreach ( model,
                                 ( GtkTreeModelForeachFunc ) invert_selection, file_browser );
        g_signal_handlers_unblock_matched( file_browser->folder_view,
                                           G_SIGNAL_MATCH_FUNC,
                                           0, 0, NULL,
                                           on_folder_view_item_sel_change, NULL );
        on_folder_view_item_sel_change( EXO_ICON_VIEW( file_browser->folder_view ),
                                        file_browser );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        GtkTreeSelection* tree_sel;
        tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW( file_browser->folder_view ));
        g_signal_handlers_block_matched( tree_sel,
                                         G_SIGNAL_MATCH_FUNC,
                                         0, 0, NULL,
                                         on_folder_view_item_sel_change, NULL );
        model = gtk_tree_view_get_model( GTK_TREE_VIEW( file_browser->folder_view ) );
        gtk_tree_model_foreach ( model,
                                 ( GtkTreeModelForeachFunc ) invert_selection, file_browser );
        g_signal_handlers_unblock_matched( tree_sel,
                                           G_SIGNAL_MATCH_FUNC,
                                           0, 0, NULL,
                                           on_folder_view_item_sel_change, NULL );
        on_folder_view_item_sel_change( (ExoIconView*)tree_sel,
                                        file_browser );
    }

}

/* signal handlers */

void
on_folder_view_item_activated ( ExoIconView *iconview,
                                GtkTreePath *path,
                                PtkFileBrowser* file_browser )
{
    ptk_file_browser_open_selected_files_with_app( file_browser, NULL );
}

void
on_folder_view_row_activated ( GtkTreeView *tree_view,
                               GtkTreePath *path,
                               GtkTreeViewColumn* col,
                               PtkFileBrowser* file_browser )
{
    file_browser->button_press = FALSE;
    ptk_file_browser_open_selected_files_with_app( file_browser, NULL );
}

void on_folder_view_item_sel_change ( ExoIconView *iconview,
                                      PtkFileBrowser* file_browser )
{
    GList * sel_files;
    GList* sel;
    GtkTreeIter it;
    GtkTreeModel* model;
    VFSFileInfo* file;

    file_browser->n_sel_files = 0;
    file_browser->sel_size = 0;

    sel_files = folder_view_get_selected_items( file_browser, &model );

    for ( sel = sel_files; sel; sel = g_list_next( sel ) )
    {
        if ( gtk_tree_model_get_iter( model, &it, ( GtkTreePath* ) sel->data ) )
        {
            gtk_tree_model_get( model, &it, COL_FILE_INFO, &file, -1 );
            if ( file )
            {
                file_browser->sel_size += vfs_file_info_get_size( file );
                vfs_file_info_unref( file );
            }
            ++file_browser->n_sel_files;
        }
    }

    g_list_foreach( sel_files,
                    ( GFunc ) gtk_tree_path_free,
                    NULL );
    g_list_free( sel_files );

    g_signal_emit( file_browser, signals[ SEL_CHANGE_SIGNAL ], 0 );
}

#if 0
static gboolean
is_latin_shortcut_key ( guint keyval )
{
    return ((GDK_0 <= keyval && keyval <= GDK_9) ||
            (GDK_A <= keyval && keyval <= GDK_Z) ||
            (GDK_a <= keyval && keyval <= GDK_z));
}

gboolean
on_folder_view_key_press_event ( GtkWidget *widget,
                                 GdkEventKey *event,
                                 PtkFileBrowser* file_browser )
{
    int modifier = ( event->state & ( GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK ) );
    if ( ! gtk_widget_is_focus( widget ) )
        return FALSE;
//printf("on_folder_view_key_press_event\n");

    // Make key bindings work when the current keyboard layout is not latin
    if ( modifier != 0 && !is_latin_shortcut_key( event->keyval ) ) {
        // We have a non-latin char, try other keyboard groups
        GdkKeymapKey *keys;
        guint *keyvals;
        gint n_entries;
        gint level;

        if ( gdk_keymap_translate_keyboard_state( NULL,
                                                  event->hardware_keycode,
                                                  (GdkModifierType)event->state,
                                                  event->group,
                                                  NULL, NULL, &level, NULL )
            && gdk_keymap_get_entries_for_keycode( NULL,
                                                   event->hardware_keycode,
                                                   &keys, &keyvals,
                                                   &n_entries ) ) {
            gint n;
            for ( n=0; n<n_entries; n++ ) {
                if ( keys[n].group == event->group ) {
                    // Skip keys from the same group
                    continue;
                }
                if ( keys[n].level != level ) {
                    // Allow only same level keys
                    continue;
                }
                if ( is_latin_shortcut_key( keyvals[n] ) ) {
                    // Latin character found
                    event->keyval = keyvals[n];
                    break;
                }
            }
            g_free( keys );
            g_free( keyvals );
        }
    }

    if ( modifier == GDK_CONTROL_MASK )
    {
        switch ( event->keyval )
        {
        case GDK_x:
            ptk_file_browser_cut( file_browser );
            break;
        case GDK_c:
            ptk_file_browser_copy( file_browser );
            break;
        case GDK_v:
            ptk_file_browser_paste( file_browser );
            return TRUE;
            break;
        case GDK_i:
            //ptk_file_browser_invert_selection( file_browser );
            break;
        case GDK_a:
            //ptk_file_browser_select_all( file_browser );
            break;
        case GDK_h:
            ptk_file_browser_show_hidden_files(
                file_browser,
                ! file_browser->show_hidden_files );
            break;
        default:
            return FALSE;
        }
    }
    else if ( modifier == GDK_MOD1_MASK )
    {
        switch ( event->keyval )
        {
        case GDK_Return:
            ptk_file_browser_file_properties( file_browser, 0 );
            break;
        default:
            return FALSE;
        }
    }
    else if ( modifier == GDK_SHIFT_MASK )
    {
        switch ( event->keyval )
        {
        case GDK_Delete:
            ptk_file_browser_delete( file_browser );
            break;
        default:
            return FALSE;
        }
    }
    else if ( modifier == 0 )
    {
        switch ( event->keyval )
        {
        case GDK_F2:
            //ptk_file_browser_rename_selected_files( file_browser );
            break;
        case GDK_Delete:
            ptk_file_browser_delete( file_browser );
            break;
        case GDK_BackSpace:
            ptk_file_browser_go_up( NULL, file_browser );
            break;
        default:
            return FALSE;
        }
    }
/*    else if ( modifier == ( GDK_SHIFT_MASK | GDK_MOD1_MASK ) )  //MOD added
    {
        switch ( event->keyval )
        {
        case GDK_C:
            ptk_file_browser_copy_name( file_browser );
            return TRUE;
            break;      
        case GDK_V:
            ptk_file_browser_paste_target( file_browser );
            return TRUE;
            break;      
        default:
            return FALSE;
        }
    }
    else if ( modifier == ( GDK_SHIFT_MASK | GDK_CONTROL_MASK ) )  //MOD added
    {
        switch ( event->keyval )
        {
        case GDK_C:
            ptk_file_browser_copy_text( file_browser );
            return TRUE;
            break;      
        case GDK_V:
            ptk_file_browser_paste_link( file_browser );
            return TRUE;
            break;      
        default:
            return FALSE;
        }
    }
*/
    else
    {
        return FALSE;
    }
    return TRUE;
}

gboolean
on_folder_view_button_release_event ( GtkWidget *widget,
                                      GdkEventButton *event,
                                      PtkFileBrowser* file_browser )
{
/*
    if( file_browser->single_click && file_browser->view_mode == PTK_FB_LIST_VIEW )
    {

    }
*/
    return FALSE;
}
#endif


static void show_popup_menu( PtkFileBrowser* file_browser,
                             GdkEventButton *event )
{
    const char * cwd;
    char* dir_name = NULL;
    guint32 time;
    gint button;
    GtkWidget* popup;
    char* file_path = NULL;
    VFSFileInfo* file;
    GList* sel_files;

    cwd = ptk_file_browser_get_cwd( file_browser );
    sel_files = ptk_file_browser_get_selected_files( file_browser );
    if( ! sel_files )
    {
        file = NULL;
/*
        file = vfs_file_info_new();
        vfs_file_info_get( file, cwd, NULL );
        sel_files = g_list_prepend( NULL, vfs_file_info_ref( file ) );
        file_path = g_strdup( cwd );
*/        /* dir_name = g_path_get_dirname( cwd ); */
    }
    else
    {
        file = vfs_file_info_ref( (VFSFileInfo*)sel_files->data );
        file_path = g_build_filename( cwd, vfs_file_info_get_name( file ), NULL );
    }
    //MOD added G_FILE_TEST_IS_SYMLINK for dangling symlink popup menu
//    if ( g_file_test( file_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_SYMLINK ) )
//    {
        if ( event )
        {
            button = event->button;
            time = event->time;
        }
        else
        {
            button = 0;
            time = gtk_get_current_event_time();
        }
            popup = ptk_file_menu_new( NULL, file_browser,
                    file_path, file,
                    dir_name ? dir_name : cwd,
                    sel_files );
            gtk_menu_popup( GTK_MENU( popup ), NULL, NULL,
                    NULL, NULL, button, time );
//    }
//    else if ( sel_files )
//    {
//        vfs_file_info_list_free( sel_files );
//    }
    if ( file )
        vfs_file_info_unref( file );

    if ( file_path )
        g_free( file_path );
    if ( dir_name )
        g_free( dir_name );
}

/* invoke popup menu via shortcut key */
gboolean
on_folder_view_popup_menu ( GtkWidget* widget, PtkFileBrowser* file_browser )
{
    show_popup_menu( file_browser, NULL );
    return TRUE;
}

gboolean
on_folder_view_button_press_event ( GtkWidget *widget,
                                    GdkEventButton *event,
                                    PtkFileBrowser* file_browser )
{
    VFSFileInfo * file;
    GtkTreeModel * model = NULL;
    GtkTreePath *tree_path;
    GtkTreeViewColumn* col = NULL;
    GtkTreeIter it;
    gchar *file_path;
    GtkTreeSelection* tree_sel;
    gboolean ret = FALSE;

    if ( event->type == GDK_BUTTON_PRESS )
    {
        focus_folder_view( file_browser );
        file_browser->button_press = TRUE;
        
        if ( event->button == 4 || event->button == 5 )     //sfm
        {
            if ( event->button == 4 )
                ptk_file_browser_go_back( NULL, file_browser );
            else
                ptk_file_browser_go_forward( NULL, file_browser );
            return TRUE;
        }

        // Alt - Left/Right Click
        if ( ( ( event->state & ( GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK ) )
                                    == GDK_MOD1_MASK ) &&
                                    ( event->button == 1 || event->button == 3 ) ) //sfm
        {
            if ( event->button == 1 )
                ptk_file_browser_go_back( NULL, file_browser );
            else
                ptk_file_browser_go_forward( NULL, file_browser );
            return TRUE;
        }
        
        if ( file_browser->view_mode == PTK_FB_ICON_VIEW || file_browser->view_mode == PTK_FB_COMPACT_VIEW )
        {
            tree_path = exo_icon_view_get_path_at_pos( EXO_ICON_VIEW( widget ),
                                                       event->x, event->y );
            model = exo_icon_view_get_model( EXO_ICON_VIEW( widget ) );

            if ( tree_path && app_settings.single_click && !event->state
                                                    && event->button == 1 ) //sfm
            {
                // unselect all but one file
                exo_icon_view_unselect_all( EXO_ICON_VIEW( widget ) );
                exo_icon_view_select_path( EXO_ICON_VIEW( widget ), tree_path );
            }

            /* deselect selected files when right click on blank area */
            if ( !tree_path && event->button == 3 )
                exo_icon_view_unselect_all ( EXO_ICON_VIEW( widget ) );
        }
        else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
        {
            model = gtk_tree_view_get_model( GTK_TREE_VIEW( widget ) );
            gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW( widget ),
                                           event->x, event->y, &tree_path, &col, NULL, NULL );
            tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( widget ) );

            if( col && gtk_tree_view_column_get_sort_column_id(col) != COL_FILE_NAME && tree_path ) //MOD
            {
                gtk_tree_path_free( tree_path );
                tree_path = NULL;
            }
/* this was an attempt to prevent opening of multiple files with single click
 * but it causes drag and rubberband dysfunction - leave multiple open ok?
            if ( tree_path && tree_sel && app_settings.single_click && !event->state
                                                        && event->button == 1) //sfm
            {
                // unselect all but one file
                gtk_tree_selection_unselect_all( tree_sel );
                gtk_tree_selection_select_path( tree_sel, tree_path );
            }
*/
        }

        /* an item is clicked, get its file path */
        if ( tree_path && gtk_tree_model_get_iter( model, &it, tree_path ) )
        {
            gtk_tree_model_get( model, &it, COL_FILE_INFO, &file, -1 );
            file_path = g_build_filename( ptk_file_browser_get_cwd( file_browser ),
                                          vfs_file_info_get_name( file ), NULL );
        }
        else /* no item is clicked */
        {
            file = NULL;
            file_path = NULL;
        }

        /* middle button */
        if ( event->button == 2 && file_path ) /* middle click on a item */
        {
            /* open in new tab if its a folder */
            if ( G_LIKELY( file_path ) )
            {
                if ( g_file_test( file_path, G_FILE_TEST_IS_DIR ) )
                {
                    g_signal_emit( file_browser, signals[ OPEN_ITEM_SIGNAL ], 0,
                                   file_path, PTK_OPEN_NEW_TAB );
                }
            }
            ret = TRUE;
        }
        else if ( event->button == 3 ) /* right click */
        {
            /* cancel all selection, and select the item if it's not selected */
            if ( file_browser->view_mode == PTK_FB_ICON_VIEW || file_browser->view_mode == PTK_FB_COMPACT_VIEW )
            {
                if ( tree_path &&
                    !exo_icon_view_path_is_selected ( EXO_ICON_VIEW( widget ),
                                                          tree_path ) )
                {
                    exo_icon_view_unselect_all ( EXO_ICON_VIEW( widget ) );
                    exo_icon_view_select_path( EXO_ICON_VIEW( widget ), tree_path );
                }
            }
            else if( file_browser->view_mode == PTK_FB_LIST_VIEW )
            {
                if ( tree_path &&
                    !gtk_tree_selection_path_is_selected( tree_sel, tree_path ) )
                {
                    gtk_tree_selection_unselect_all( tree_sel );
                    gtk_tree_selection_select_path( tree_sel, tree_path );
                }
            }
            show_popup_menu( file_browser, event );
            ret = TRUE;
        }
        if ( file )
            vfs_file_info_unref( file );
        g_free( file_path );
        gtk_tree_path_free( tree_path );
    }
/*  go up if double-click in blank area of file list - this was disabled due
 * to complaints about accidental clicking
    else if ( file_browser->button_press && event->type == GDK_2BUTTON_PRESS
                                                        && event->button == 1 )
    {
        if ( file_browser->view_mode == PTK_FB_ICON_VIEW || file_browser->view_mode == PTK_FB_COMPACT_VIEW )
        {
            tree_path = exo_icon_view_get_path_at_pos( EXO_ICON_VIEW( widget ),
                                           event->x, event->y );
            if ( !tree_path )
            {
                ptk_file_browser_go_up( NULL, file_browser );
                ret = TRUE;
            }
            else
                gtk_tree_path_free( tree_path );
        }
        else if( file_browser->view_mode == PTK_FB_LIST_VIEW )
        {
            // row_activated occurs before GDK_2BUTTON_PRESS so use
            // file_browser->button_press to determine if row was already activated
            // or user clicked on non-row
            ptk_file_browser_go_up( NULL, file_browser );
            ret = TRUE;
        }
    }
*/
    return ret;
}

static gboolean on_dir_tree_update_sel ( PtkFileBrowser* file_browser )
{
    char * dir_path;

    if ( !file_browser->side_dir )
        return FALSE;
    gdk_threads_enter();
    dir_path = ptk_dir_tree_view_get_selected_dir( file_browser->side_dir );
        
    if ( dir_path )
    {
        if ( strcmp( dir_path, ptk_file_browser_get_cwd( file_browser ) ) )
        {
            //if ( startup_mode == FALSE ) //MOD
            //{
                ptk_file_browser_chdir( file_browser, dir_path, PTK_FB_CHDIR_ADD_HISTORY);
                //dir_path = g_strdup_printf( _( "change path:\nX\%sX" ), dir_path );
                //ptk_show_error( NULL, _("Path"), dir_path );
            //}
        }
        g_free( dir_path );
    }
    gdk_threads_leave();
    return FALSE;
}

void
on_dir_tree_sel_changed ( GtkTreeSelection *treesel,
                          PtkFileBrowser* file_browser )
{
    g_idle_add( ( GSourceFunc ) on_dir_tree_update_sel, file_browser );
}

void on_shortcut_new_tab_activate( GtkMenuItem* item,
                                          PtkFileBrowser* file_browser )
{
    char* dir_path;

    focus_folder_view( file_browser );
    if ( xset_get_s( "go_set_default" ) )
        dir_path = g_strdup( xset_get_s( "go_set_default" ) );
    else
        dir_path = g_get_home_dir();

    if ( !g_file_test( dir_path, G_FILE_TEST_IS_DIR ) )
        g_signal_emit( file_browser,
                       signals[ OPEN_ITEM_SIGNAL ],
                       0, "/", PTK_OPEN_NEW_TAB );
    else
    {
        g_signal_emit( file_browser,
                       signals[ OPEN_ITEM_SIGNAL ],
                       0, dir_path, PTK_OPEN_NEW_TAB );
    }
}

void on_shortcut_new_tab_here( GtkMenuItem* item,
                                          PtkFileBrowser* file_browser )
{
    char* dir_path;

    focus_folder_view( file_browser );
    dir_path = ptk_file_browser_get_cwd( file_browser );
    if ( !g_file_test( dir_path, G_FILE_TEST_IS_DIR ) )
    {
        if ( xset_get_s( "go_set_default" ) )
            dir_path = g_strdup( xset_get_s( "go_set_default" ) );
        else
            dir_path = g_get_home_dir();
    }
    if ( !g_file_test( dir_path, G_FILE_TEST_IS_DIR ) )
        g_signal_emit( file_browser,
                       signals[ OPEN_ITEM_SIGNAL ],
                       0, "/", PTK_OPEN_NEW_TAB );
    else
    {
        g_signal_emit( file_browser,
                       signals[ OPEN_ITEM_SIGNAL ],
                       0, dir_path, PTK_OPEN_NEW_TAB );
    }
}
/*
void on_shortcut_new_window_activate( GtkMenuItem* item,
                                             PtkFileBrowser* file_browser )
{
    char * dir_path;
    dir_path = ptk_location_view_get_selected_dir( file_browser->side_dir );
    if ( dir_path )
    {
        g_signal_emit( file_browser,
                       signals[ OPEN_ITEM_SIGNAL ],
                       0, dir_path, PTK_OPEN_NEW_WINDOW );
        g_free( dir_path );
    }
}

static void on_shortcut_remove_activate( GtkMenuItem* item,
                                         PtkFileBrowser* file_browser )
{
    char * dir_path;
    dir_path = ptk_location_view_get_selected_dir( file_browser->side_dir );
    if ( dir_path )
    {
        ptk_bookmarks_remove( dir_path );
        g_free( dir_path );
    }
}

static void on_shortcut_rename_activate( GtkMenuItem* item,
                                         PtkFileBrowser* file_browser )
{
    ptk_location_view_rename_selected_bookmark( file_browser->side_dir );
}

static PtkMenuItemEntry shortcut_popup_menu[] =
    {
        PTK_MENU_ITEM( N_( "Open in New _Tab" ), on_shortcut_new_tab_activate, 0, 0 ),
        PTK_MENU_ITEM( N_( "Open in New _Window" ), on_shortcut_new_window_activate, 0, 0 ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_STOCK_MENU_ITEM( GTK_STOCK_REMOVE, on_shortcut_remove_activate ),
        PTK_IMG_MENU_ITEM( N_( "_Rename" ), "gtk-edit", on_shortcut_rename_activate, GDK_F2, 0 ),
        PTK_MENU_END
    };
*/

void on_folder_view_columns_changed( GtkTreeView *view,
                                                PtkFileBrowser* file_browser )
{
    char* title;
    char* pos;
    XSet* set = NULL;
    int i, j, width;
    GtkTreeViewColumn* col;
    
    if ( file_browser->view_mode != PTK_FB_LIST_VIEW )
        return;
    gboolean fullscreen = xset_get_b( "main_full" );

    const char* titles[] =
        {
            N_( "Name" ), N_( "Size" ), N_( "Type" ),
            N_( "Permission" ), N_( "Owner" ), N_( "Modified" )
        };

    const char* set_names[] =
        {
            "detcol_name", "detcol_size", "detcol_type",
            "detcol_perm", "detcol_owner", "detcol_date"
        };

    for ( i = 0; i < 6; i++ )
    {
        col = gtk_tree_view_get_column( view, i );
        if ( !col )
            return;
        title = gtk_tree_view_column_get_title( col );
        for ( j = 0; j < 6; j++ )
        {
            if ( !strcmp( title, _(titles[j]) ) )
                break;
        }
        if ( j != 6 )
        {
            set = xset_get_panel( file_browser->mypanel, set_names[j] );
            // save column position
            pos = g_strdup_printf( "%d", i );
            xset_set_set( set, "x", pos );
            g_free( pos );
            // save column width
            if ( !fullscreen && ( width = gtk_tree_view_column_get_width( col ) ) )
            {
                pos = g_strdup_printf( "%d", width );
                xset_set_set( set, "y", pos );
                g_free( pos );
            }
            // set column visibility
            gtk_tree_view_column_set_visible( col, 
                        xset_get_b_panel( file_browser->mypanel, set_names[j] ) );
        }
    }
}

void on_folder_view_destroy( GtkTreeView *view, PtkFileBrowser* file_browser )
{
    guint id = g_signal_lookup ("columns-changed", G_TYPE_FROM_INSTANCE(view) );
    if ( id )
    {
        gulong hand = g_signal_handler_find( ( gpointer ) view, G_SIGNAL_MATCH_ID,
                                        id, NULL, NULL, NULL, NULL );
        if ( hand )
            g_signal_handler_disconnect( ( gpointer ) view, hand );
    }
    //on_folder_view_columns_changed( view, file_browser ); // save widths
}

gboolean folder_view_search_equal( GtkTreeModel* model, gint col,
                                                         const gchar* key,
                                                         GtkTreeIter* it,
                                                         gpointer search_data )
{   
    if ( col != COL_FILE_NAME )
        return TRUE;

    char* name;
    char* lower_name;
    char* lower_key;
    gboolean no_match;
    
    gtk_tree_model_get( model, it, col, &name, -1 );

    lower_name = g_ascii_strdown( name, -1 );
    lower_key = g_ascii_strdown( key, -1 );
    
    if ( !g_ascii_strcasecmp( lower_name, name )
                                        && !strcmp( lower_key, key ) )
        // key is all lowercase and name is ascii-like so do icase search
        no_match = !strstr( lower_name, key );
    else
        // do case sensitive search
        no_match = !strstr( name, key );
        
    g_free( lower_name );
    g_free( lower_key );
    return no_match;  //return FALSE for match
}

static GtkWidget* create_folder_view( PtkFileBrowser* file_browser,
                                      PtkFBViewMode view_mode )
{
    GtkWidget * folder_view = NULL;
    GtkTreeSelection* tree_sel;
    GtkCellRenderer* renderer;
    int big_icon_size, small_icon_size, icon_size = 0;

    vfs_mime_type_get_icon_size( &big_icon_size, &small_icon_size );

    switch ( view_mode )
    {
    case PTK_FB_ICON_VIEW:
    case PTK_FB_COMPACT_VIEW:
        folder_view = exo_icon_view_new();

        if( view_mode == PTK_FB_COMPACT_VIEW )
        {
            exo_icon_view_set_layout_mode( (ExoIconView*)folder_view, EXO_ICON_VIEW_LAYOUT_COLS );
            exo_icon_view_set_orientation( (ExoIconView*)folder_view, GTK_ORIENTATION_HORIZONTAL );
        }
        else
        {
            exo_icon_view_set_column_spacing( (ExoIconView*)folder_view, 4 );
            exo_icon_view_set_item_width ( (ExoIconView*)folder_view, 110 );
        }

        exo_icon_view_set_selection_mode ( (ExoIconView*)folder_view,
                                           GTK_SELECTION_MULTIPLE );

        exo_icon_view_set_pixbuf_column ( (ExoIconView*)folder_view, COL_FILE_BIG_ICON );
        exo_icon_view_set_text_column ( (ExoIconView*)folder_view, COL_FILE_NAME );

        // search
        exo_icon_view_set_enable_search( (ExoIconView*)folder_view, TRUE );
        exo_icon_view_set_search_column( (ExoIconView*)folder_view, COL_FILE_NAME );
        exo_icon_view_set_search_equal_func( (ExoIconView*)folder_view,
                                            folder_view_search_equal, NULL, NULL );

        exo_icon_view_set_single_click( (ExoIconView*)folder_view, file_browser->single_click );
        exo_icon_view_set_single_click_timeout( (ExoIconView*)folder_view, 400 );

        gtk_cell_layout_clear ( GTK_CELL_LAYOUT ( folder_view ) );

        /* renderer = gtk_cell_renderer_pixbuf_new (); */
        file_browser->icon_render = renderer = ptk_file_icon_renderer_new();

        /* add the icon renderer */
        g_object_set ( G_OBJECT ( renderer ),
                       "follow_state", TRUE,
                       NULL );
        gtk_cell_layout_pack_start ( GTK_CELL_LAYOUT ( folder_view ), renderer, FALSE );
        gtk_cell_layout_add_attribute ( GTK_CELL_LAYOUT ( folder_view ), renderer,
                                        "pixbuf", view_mode == PTK_FB_COMPACT_VIEW ? COL_FILE_SMALL_ICON : COL_FILE_BIG_ICON );
        gtk_cell_layout_add_attribute ( GTK_CELL_LAYOUT ( folder_view ), renderer,
                                        "info", COL_FILE_INFO );
        /* add the name renderer */
        renderer = ptk_text_renderer_new ();

        if( view_mode == PTK_FB_COMPACT_VIEW )
        {
            g_object_set ( G_OBJECT ( renderer ),
                           "xalign", 0.0,
                           "yalign", 0.5,
                           NULL );
            icon_size = small_icon_size;
        }
        else
        {
            g_object_set ( G_OBJECT ( renderer ),
                           "wrap-mode", PANGO_WRAP_WORD_CHAR,
                           "wrap-width", 110,
                           "xalign", 0.5,
                           "yalign", 0.0,
                           NULL );
            icon_size = big_icon_size;
        }
        gtk_cell_layout_pack_start ( GTK_CELL_LAYOUT ( folder_view ), renderer, TRUE );
        gtk_cell_layout_add_attribute ( GTK_CELL_LAYOUT ( folder_view ), renderer,
                                        "text", COL_FILE_NAME );

        exo_icon_view_enable_model_drag_source (
            EXO_ICON_VIEW( folder_view ),
            ( GDK_CONTROL_MASK | GDK_BUTTON1_MASK | GDK_BUTTON3_MASK ),
            drag_targets, G_N_ELEMENTS( drag_targets ), GDK_ACTION_ALL );

        exo_icon_view_enable_model_drag_dest (
            EXO_ICON_VIEW( folder_view ),
            drag_targets, G_N_ELEMENTS( drag_targets ), GDK_ACTION_ALL );

        g_signal_connect ( ( gpointer ) folder_view,
                           "item_activated",
                           G_CALLBACK ( on_folder_view_item_activated ),
                           file_browser );

        g_signal_connect_after ( ( gpointer ) folder_view,
                                 "selection-changed",
                                 G_CALLBACK ( on_folder_view_item_sel_change ),
                                 file_browser );

        break;
    case PTK_FB_LIST_VIEW:
        folder_view = exo_tree_view_new ();

        init_list_view( file_browser, GTK_TREE_VIEW( folder_view ) );

        tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( folder_view ) );
        gtk_tree_selection_set_mode( tree_sel, GTK_SELECTION_MULTIPLE );

#if GTK_CHECK_VERSION(2, 10, 0)
        if ( xset_get_b( "rubberband" ) )
            gtk_tree_view_set_rubber_banding( (GtkTreeView*)folder_view, TRUE );
#endif

        // Search
        gtk_tree_view_set_enable_search( (GtkTreeView*)folder_view, TRUE );
        gtk_tree_view_set_search_column( (GtkTreeView*)folder_view, COL_FILE_NAME );
        gtk_tree_view_set_search_equal_func( (GtkTreeView*)folder_view,
                                        folder_view_search_equal, NULL, NULL );

        exo_tree_view_set_single_click( (ExoTreeView*)folder_view, file_browser->single_click );
        exo_tree_view_set_single_click_timeout( (ExoTreeView*)folder_view, 400 );

        icon_size = small_icon_size;

        gtk_tree_view_enable_model_drag_source (
            GTK_TREE_VIEW( folder_view ),
            ( GDK_CONTROL_MASK | GDK_BUTTON1_MASK | GDK_BUTTON3_MASK ),
            drag_targets, G_N_ELEMENTS( drag_targets ), GDK_ACTION_ALL );

        gtk_tree_view_enable_model_drag_dest (
            GTK_TREE_VIEW( folder_view ),
            drag_targets, G_N_ELEMENTS( drag_targets ), GDK_ACTION_ALL );

        g_signal_connect ( ( gpointer ) folder_view,
                           "row_activated",
                           G_CALLBACK ( on_folder_view_row_activated ),
                           file_browser );

        g_signal_connect_after ( ( gpointer ) tree_sel,
                                 "changed",
                                 G_CALLBACK ( on_folder_view_item_sel_change ),
                                 file_browser );
        //MOD
        g_signal_connect ( ( gpointer ) folder_view, "columns-changed",
                           G_CALLBACK ( on_folder_view_columns_changed ),
                           file_browser );
        g_signal_connect ( ( gpointer ) folder_view, "destroy",
                           G_CALLBACK ( on_folder_view_destroy ),
                           file_browser );
        break;
    }

    gtk_cell_renderer_set_fixed_size( file_browser->icon_render, icon_size, icon_size );

    g_signal_connect ( ( gpointer ) folder_view,
                       "button-press-event",
                       G_CALLBACK ( on_folder_view_button_press_event ),
                       file_browser );
//    g_signal_connect ( ( gpointer ) folder_view,
//                       "button-release-event",
//                       G_CALLBACK ( on_folder_view_button_release_event ),
//                       file_browser );

    //g_signal_connect ( ( gpointer ) folder_view,
    //                   "key_press_event",
    //                   G_CALLBACK ( on_folder_view_key_press_event ),
    //                   file_browser );

    g_signal_connect ( ( gpointer ) folder_view,
                       "popup-menu",
                       G_CALLBACK ( on_folder_view_popup_menu ),
                       file_browser );

    /* init drag & drop support */

    g_signal_connect ( ( gpointer ) folder_view, "drag-data-received",
                       G_CALLBACK ( on_folder_view_drag_data_received ),
                       file_browser );

    g_signal_connect ( ( gpointer ) folder_view, "drag-data-get",
                       G_CALLBACK ( on_folder_view_drag_data_get ),
                       file_browser );

    g_signal_connect ( ( gpointer ) folder_view, "drag-begin",
                       G_CALLBACK ( on_folder_view_drag_begin ),
                       file_browser );

    g_signal_connect ( ( gpointer ) folder_view, "drag-motion",
                       G_CALLBACK ( on_folder_view_drag_motion ),
                       file_browser );

    g_signal_connect ( ( gpointer ) folder_view, "drag-leave",
                       G_CALLBACK ( on_folder_view_drag_leave ),
                       file_browser );

    g_signal_connect ( ( gpointer ) folder_view, "drag-drop",
                       G_CALLBACK ( on_folder_view_drag_drop ),
                       file_browser );

    g_signal_connect ( ( gpointer ) folder_view, "drag-end",
                       G_CALLBACK ( on_folder_view_drag_end ),
                       file_browser );

    // set font
    if ( xset_get_s_panel( file_browser->mypanel, "font_file" ) )
    {
        PangoFontDescription* font_desc = pango_font_description_from_string(
                        xset_get_s_panel( file_browser->mypanel, "font_file" ) );
        gtk_widget_modify_font( folder_view, font_desc );
        pango_font_description_free( font_desc );
    }

    return folder_view;
}


void init_list_view( PtkFileBrowser* file_browser, GtkTreeView* list_view )
{
    GtkTreeViewColumn * col;
    GtkCellRenderer *renderer;
    GtkCellRenderer *pix_renderer;
    int i, j, width;

    int cols[] = { COL_FILE_NAME, COL_FILE_SIZE, COL_FILE_DESC,
                   COL_FILE_PERM, COL_FILE_OWNER, COL_FILE_MTIME };

    const char* titles[] =
        {
            N_( "Name" ), N_( "Size" ), N_( "Type" ),
            N_( "Permission" ), N_( "Owner" ), N_( "Modified" )
        };
    const char* set_names[] =
        {
            "detcol_name", "detcol_size", "detcol_type",
            "detcol_perm", "detcol_owner", "detcol_date"
        };

    for ( i = 0; i < G_N_ELEMENTS( cols ); i++ )
    {
        col = gtk_tree_view_column_new ();
        gtk_tree_view_column_set_resizable ( col, TRUE );

        renderer = gtk_cell_renderer_text_new();

        for ( j = 0; j < G_N_ELEMENTS( cols ); j++ )
        {
            if ( xset_get_int_panel( file_browser->mypanel, set_names[j], "x" ) == i )
                break;
        }
        if ( j == G_N_ELEMENTS( cols ) )
            j = i; // failsafe
        else
        {
            width = xset_get_int_panel( file_browser->mypanel, set_names[j], "y" );
            if ( width )
            {
                gtk_tree_view_column_set_sizing( col, GTK_TREE_VIEW_COLUMN_FIXED );
                gtk_tree_view_column_set_min_width( col, 50 );
                if ( cols[j] == COL_FILE_NAME && !app_settings.always_show_tabs
                                && file_browser->view_mode == PTK_FB_LIST_VIEW
                                && gtk_notebook_get_n_pages(
                                            file_browser->mynotebook ) == 1 )
                {
                    // when tabs are added, the width of the notebook decreases
                    // by a few pixels, meaning there is not enough space for
                    // all columns - this causes a horizontal scrollbar to 
                    // appear on new and sometimes first tab
                    // so shave some pixels off first columns
                    gtk_tree_view_column_set_fixed_width ( col, width - 6 );

                    // below causes increasing reduction of column every time new tab is 
                    // added and closed - undesirable
                    PtkFileBrowser* first_fb = (PtkFileBrowser*)
                            gtk_notebook_get_nth_page( file_browser->mynotebook, 0 );
                    
                    if ( first_fb && first_fb->view_mode == PTK_FB_LIST_VIEW &&
                                            GTK_IS_TREE_VIEW( first_fb->folder_view ) )
                    {
                        GtkTreeViewColumn* first_col = gtk_tree_view_get_column(
                                                            first_fb->folder_view, 0 );
                        if ( first_col )
                        {
                            int first_width = gtk_tree_view_column_get_width( first_col );
                            if ( first_width > 10 )
                                gtk_tree_view_column_set_fixed_width( first_col,
                                                                    first_width - 6 );
                        }
                    }
                }
                else
                    gtk_tree_view_column_set_fixed_width ( col, width );
            }
        }

        if ( cols[j] == COL_FILE_NAME )
        {
            gtk_object_set( GTK_OBJECT( renderer ),
                            /* "editable", TRUE, */
                            "ellipsize", PANGO_ELLIPSIZE_END, NULL );
            /*
            g_signal_connect( renderer, "editing-started",
                              G_CALLBACK( on_filename_editing_started ), NULL );
            */
            file_browser->icon_render = pix_renderer = ptk_file_icon_renderer_new();

            gtk_tree_view_column_pack_start( col, pix_renderer, FALSE );
            gtk_tree_view_column_set_attributes( col, pix_renderer,
                                                 "pixbuf", COL_FILE_SMALL_ICON,
                                                 "info", COL_FILE_INFO, NULL );
            gtk_tree_view_column_set_expand ( col, TRUE );
            gtk_tree_view_column_set_sizing( col, GTK_TREE_VIEW_COLUMN_FIXED );
            gtk_tree_view_column_set_min_width( col, 150 );
            gtk_tree_view_column_set_reorderable( col, FALSE );
            exo_tree_view_set_activable_column( (ExoTreeView*)list_view, col );
        }
        else
        {
            gtk_tree_view_column_set_reorderable( col, TRUE );
            gtk_tree_view_column_set_visible( col, 
                    xset_get_b_panel( file_browser->mypanel, set_names[j] ) );
        }

        if ( cols[j] == COL_FILE_SIZE )
            gtk_cell_renderer_set_alignment( renderer, 1, 0 );
        
        gtk_tree_view_column_pack_start( col, renderer, TRUE );       
        gtk_tree_view_column_set_attributes( col, renderer, "text", cols[ j ], NULL );
        gtk_tree_view_append_column ( list_view, col );
        gtk_tree_view_column_set_title( col, _( titles[ j ] ) );
        gtk_tree_view_column_set_sort_indicator( col, TRUE );
        gtk_tree_view_column_set_sort_column_id( col, cols[ j ] );
        gtk_tree_view_column_set_sort_order( col, GTK_SORT_DESCENDING );
    }
    gtk_tree_view_set_rules_hint ( list_view, TRUE );
}

void ptk_file_browser_refresh( GtkWidget* item, PtkFileBrowser* file_browser )
{
    char* tmpcwd;  //MOD
    /*
    * FIXME:
    * Do nothing when there is unfinished task running in the
    * working thread.
    * This should be fixed with a better way in the future.
    */
//    if ( file_browser->busy )    //MOD
//        return ;

    //MOD   try to trigger real reload
    tmpcwd = ptk_file_browser_get_cwd( file_browser );

    ptk_file_browser_chdir( file_browser,
                            "/",
                            PTK_FB_CHDIR_NO_HISTORY );
    ptk_file_browser_update_model( file_browser );

    if ( !ptk_file_browser_chdir( file_browser,
                            tmpcwd,
                            PTK_FB_CHDIR_NO_HISTORY ) )
    {
        char* path = xset_get_s( "go_set_default" );
        if ( path && path[0] != '\0' )
            ptk_file_browser_chdir( PTK_FILE_BROWSER( file_browser ), path,
                                                        PTK_FB_CHDIR_ADD_HISTORY );
        else
            ptk_file_browser_chdir( PTK_FILE_BROWSER( file_browser ), g_get_home_dir(),
                                                            PTK_FB_CHDIR_ADD_HISTORY );
    }
    else if ( file_browser->max_thumbnail )
    {
        // clear thumbnails
        ptk_file_list_show_thumbnails( PTK_FILE_LIST( file_browser->file_list ),
                                        file_browser->view_mode == PTK_FB_ICON_VIEW,
                                        0 );
        while( gtk_events_pending() )
            gtk_main_iteration();
    }

    ptk_file_browser_update_model( file_browser );
}

guint ptk_file_browser_get_n_all_files( PtkFileBrowser* file_browser )
{
    return file_browser->dir ? file_browser->dir->n_files : 0;
}

guint ptk_file_browser_get_n_visible_files( PtkFileBrowser* file_browser )
{
    return file_browser->file_list ?
           gtk_tree_model_iter_n_children( file_browser->file_list, NULL ) : 0;
}

GList* folder_view_get_selected_items( PtkFileBrowser* file_browser,
                                       GtkTreeModel** model )
{
    GtkTreeSelection * tree_sel;
    if ( file_browser->view_mode == PTK_FB_ICON_VIEW || file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        *model = exo_icon_view_get_model( EXO_ICON_VIEW( file_browser->folder_view ) );
        return exo_icon_view_get_selected_items( EXO_ICON_VIEW( file_browser->folder_view ) );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( file_browser->folder_view ) );
        return gtk_tree_selection_get_selected_rows( tree_sel, model );
    }
    return NULL;
}

static char* folder_view_get_drop_dir( PtkFileBrowser* file_browser, int x, int y )
{
    GtkTreePath * tree_path = NULL;
    GtkTreeModel *model = NULL;
    GtkTreeViewColumn* col;
    GtkTreeIter it;
    VFSFileInfo* file;
    char* dest_path = NULL;

    if ( file_browser->view_mode == PTK_FB_ICON_VIEW || file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        exo_icon_view_widget_to_icon_coords ( EXO_ICON_VIEW( file_browser->folder_view ),
                                              x, y, &x, &y );
        tree_path = folder_view_get_tree_path_at_pos( file_browser, x, y );
        model = exo_icon_view_get_model( EXO_ICON_VIEW( file_browser->folder_view ) );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW( file_browser->folder_view ), x, y,
                                       NULL, &col, NULL, NULL );
        if ( col == gtk_tree_view_get_column( GTK_TREE_VIEW( file_browser->folder_view ), 0 ) )
        {
            gtk_tree_view_get_dest_row_at_pos ( GTK_TREE_VIEW( file_browser->folder_view ), x, y,
                                                &tree_path, NULL );
            model = gtk_tree_view_get_model( GTK_TREE_VIEW( file_browser->folder_view ) );
        }
    }
    if ( tree_path )
    {
        if ( G_UNLIKELY( ! gtk_tree_model_get_iter( model, &it, tree_path ) ) )
            return NULL;

        gtk_tree_model_get( model, &it, COL_FILE_INFO, &file, -1 );
        if ( file )
        {
            if ( vfs_file_info_is_dir( file ) )
            {
                dest_path = g_build_filename( ptk_file_browser_get_cwd( file_browser ),
                                              vfs_file_info_get_name( file ), NULL );
            }
            else  /* Drop on a file, not folder */
            {
                /* Return current directory */
                dest_path = g_strdup( ptk_file_browser_get_cwd( file_browser ) );
            }
            vfs_file_info_unref( file );
        }
        gtk_tree_path_free( tree_path );
    }
    else
    {
        dest_path = g_strdup( ptk_file_browser_get_cwd( file_browser ) );
    }
    return dest_path;
}

void on_folder_view_drag_data_received ( GtkWidget *widget,
                                         GdkDragContext *drag_context,
                                         gint x,
                                         gint y,
                                         GtkSelectionData *sel_data,
                                         guint info,
                                         guint time,
                                         gpointer user_data )
{
    gchar **list, **puri;
    GList* files = NULL;
    PtkFileTask* task;
    VFSFileTaskType file_action = VFS_FILE_TASK_MOVE;
    PtkFileBrowser* file_browser = ( PtkFileBrowser* ) user_data;
    char* dest_dir;
    char* file_path;
    GtkWidget* parent_win;
    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name( widget, "drag-data-received" );

    if ( ( sel_data->length >= 0 ) && ( sel_data->format == 8 ) )
    {
        dest_dir = folder_view_get_drop_dir( file_browser, x, y );
        puri = list = gtk_selection_data_get_uris( sel_data );
        /* We only want to update drag status, not really want to drop */
        if( file_browser->pending_drag_status )
        {
            dev_t dest_dev;
            struct stat statbuf;
            if( stat( dest_dir, &statbuf ) == 0 )
            {
                dest_dev = statbuf.st_dev;
                if( 0 == file_browser->drag_source_dev )
                {
                    file_browser->drag_source_dev = dest_dev;
                    for( ; *puri; ++puri )
                    {
                        file_path = g_filename_from_uri( *puri, NULL, NULL );
                        if( stat( file_path, &statbuf ) == 0 && statbuf.st_dev != dest_dev )
                        {
                            file_browser->drag_source_dev = statbuf.st_dev;
                            g_free( file_path );
                            break;
                        }
                        g_free( file_path );
                    }
                }
                //MOD always suggest move
                //if( file_browser->drag_source_dev != dest_dev )     /* src and dest are on different devices */
                //    drag_context->suggested_action = GDK_ACTION_COPY;
                //else
                    drag_context->suggested_action = GDK_ACTION_MOVE;
            }
            g_free( dest_dir );
            g_strfreev( list );
            file_browser->pending_drag_status = 0;
            return;
        }
        if ( puri )
        {
            if ( 0 == ( drag_context->action &
                        ( GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK ) ) )
            {
                drag_context->action = GDK_ACTION_MOVE;
            }
            gtk_drag_finish ( drag_context, TRUE, FALSE, time );

            while ( *puri )
            {
                if ( **puri == '/' )
                    file_path = g_strdup( *puri );
                else
                    file_path = g_filename_from_uri( *puri, NULL, NULL );

                if ( file_path )
                    files = g_list_prepend( files, file_path );
                ++puri;
            }
            g_strfreev( list );

            switch ( drag_context->action )
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

            if ( files )
            {
                /* g_print( "dest_dir = %s\n", dest_dir ); */

                /* We only want to update drag status, not really want to drop */
                if( file_browser->pending_drag_status )
                {
                    struct stat statbuf;
                    if( stat( dest_dir, &statbuf ) == 0 )
                    {
                        file_browser->pending_drag_status = 0;

                    }
                    g_list_foreach( files, (GFunc)g_free, NULL );
                    g_list_free( files );
                    g_free( dest_dir );
                    return;
                }
                else /* Accept the drop and perform file actions */
                {
                    parent_win = gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) );
                    task = ptk_file_task_new( file_action,
                                              files,
                                              dest_dir,
                                              GTK_WINDOW( parent_win ),
                                              file_browser->task_view );
                    ptk_file_task_run( task );
                    g_free( dest_dir );
                }
            }
            gtk_drag_finish ( drag_context, TRUE, FALSE, time );
            return ;
        }
    }

    /* If we are only getting drag status, not finished. */
    if( file_browser->pending_drag_status )
    {
        file_browser->pending_drag_status = 0;
        return;
    }
    gtk_drag_finish ( drag_context, FALSE, FALSE, time );
}

void on_folder_view_drag_data_get ( GtkWidget *widget,
                                    GdkDragContext *drag_context,
                                    GtkSelectionData *sel_data,
                                    guint info,
                                    guint time,
                                    PtkFileBrowser *file_browser )
{
    GdkAtom type = gdk_atom_intern( "text/uri-list", FALSE );
    gchar* uri;
    GString* uri_list = g_string_sized_new( 8192 );
    GList* sels = ptk_file_browser_get_selected_files( file_browser );
    GList* sel;
    VFSFileInfo* file;
    char* full_path;

    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name( widget, "drag-data-get" );

    drag_context->actions = GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK;
    // drag_context->suggested_action = GDK_ACTION_MOVE;

    for ( sel = sels; sel; sel = g_list_next( sel ) )
    {
        file = ( VFSFileInfo* ) sel->data;
        full_path = g_build_filename( ptk_file_browser_get_cwd( file_browser ),
                                      vfs_file_info_get_name( file ), NULL );
        uri = g_filename_to_uri( full_path, NULL, NULL );
        g_free( full_path );
        g_string_append( uri_list, uri );
        g_free( uri );

        g_string_append( uri_list, "\r\n" );
    }
    g_list_foreach( sels, ( GFunc ) vfs_file_info_unref, NULL );
    g_list_free( sels );
    gtk_selection_data_set ( sel_data, type, 8,
                             ( guchar* ) uri_list->str, uri_list->len + 1 );
    g_string_free( uri_list, TRUE );
}


void on_folder_view_drag_begin ( GtkWidget *widget,
                                 GdkDragContext *drag_context,
                                 gpointer user_data )
{
    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name ( widget, "drag-begin" );
    /* gtk_drag_set_icon_stock ( drag_context, GTK_STOCK_DND_MULTIPLE, 1, 1 ); */
    gtk_drag_set_icon_default( drag_context );
}

static GtkTreePath*
folder_view_get_tree_path_at_pos( PtkFileBrowser* file_browser, int x, int y )
{
    GtkTreePath *tree_path;

    if ( file_browser->view_mode == PTK_FB_ICON_VIEW || file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        tree_path = exo_icon_view_get_path_at_pos( EXO_ICON_VIEW( file_browser->folder_view ), x, y );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW( file_browser->folder_view ), x, y,
                                       &tree_path, NULL, NULL, NULL );
    }
    return tree_path;
}

gboolean on_folder_view_auto_scroll( GtkScrolledWindow* scroll )
{
    GtkAdjustment * vadj;
    gdouble vpos;

    gdk_threads_enter();

    vadj = gtk_scrolled_window_get_vadjustment( scroll ) ;
    vpos = gtk_adjustment_get_value( vadj );

    if ( folder_view_auto_scroll_direction == GTK_DIR_UP )
    {
        vpos -= vadj->step_increment;
        if ( vpos > vadj->lower )
            gtk_adjustment_set_value ( vadj, vpos );
        else
            gtk_adjustment_set_value ( vadj, vadj->lower );
    }
    else
    {
        vpos += vadj->step_increment;
        if ( ( vpos + vadj->page_size ) < vadj->upper )
            gtk_adjustment_set_value ( vadj, vpos );
        else
            gtk_adjustment_set_value ( vadj, ( vadj->upper - vadj->page_size ) );
    }

    gdk_threads_leave();
    return TRUE;
}

gboolean on_folder_view_drag_motion ( GtkWidget *widget,
                                      GdkDragContext *drag_context,
                                      gint x,
                                      gint y,
                                      guint time,
                                      PtkFileBrowser* file_browser )
{
    GtkScrolledWindow * scroll;
    GtkAdjustment *vadj;
    gdouble vpos;
    GtkTreeModel* model = NULL;
    GtkTreePath *tree_path;
    GtkTreeViewColumn* col;
    GtkTreeIter it;
    VFSFileInfo* file;
    GdkDragAction suggested_action;
    GdkAtom target;
    GtkTargetList* target_list;

    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name ( widget, "drag-motion" );

    scroll = GTK_SCROLLED_WINDOW( gtk_widget_get_parent ( widget ) );

    vadj = gtk_scrolled_window_get_vadjustment( scroll ) ;
    vpos = gtk_adjustment_get_value( vadj );

    if ( y < 32 )
    {
        /* Auto scroll up */
        if ( ! folder_view_auto_scroll_timer )
        {
            folder_view_auto_scroll_direction = GTK_DIR_UP;
            folder_view_auto_scroll_timer = g_timeout_add(
                                                150,
                                                ( GSourceFunc ) on_folder_view_auto_scroll,
                                                scroll );
        }
    }
    else if ( y > ( widget->allocation.height - 32 ) )
    {
        if ( ! folder_view_auto_scroll_timer )
        {
            folder_view_auto_scroll_direction = GTK_DIR_DOWN;
            folder_view_auto_scroll_timer = g_timeout_add(
                                                150,
                                                ( GSourceFunc ) on_folder_view_auto_scroll,
                                                scroll );
        }
    }
    else if ( folder_view_auto_scroll_timer )
    {
        g_source_remove( folder_view_auto_scroll_timer );
        folder_view_auto_scroll_timer = 0;
    }

    tree_path = NULL;
    if ( file_browser->view_mode == PTK_FB_ICON_VIEW || file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        exo_icon_view_widget_to_icon_coords( EXO_ICON_VIEW( widget ), x, y, &x, &y );
        tree_path = exo_icon_view_get_path_at_pos( EXO_ICON_VIEW( widget ), x, y );
        model = exo_icon_view_get_model( EXO_ICON_VIEW( widget ) );
    }
    else
    {
        if ( gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW( widget ), x, y,
                                            NULL, &col, NULL, NULL ) )
        {
            if ( gtk_tree_view_get_column ( GTK_TREE_VIEW( widget ), 0 ) == col )
            {
                gtk_tree_view_get_dest_row_at_pos ( GTK_TREE_VIEW( widget ), x, y,
                                                    &tree_path, NULL );
                model = gtk_tree_view_get_model( GTK_TREE_VIEW( widget ) );
            }
        }
    }

    if ( tree_path )
    {
        if ( gtk_tree_model_get_iter( model, &it, tree_path ) )
        {
            gtk_tree_model_get( model, &it, COL_FILE_INFO, &file, -1 );
            if ( ! file || ! vfs_file_info_is_dir( file ) )
            {
                gtk_tree_path_free( tree_path );
                tree_path = NULL;
            }
            vfs_file_info_unref( file );
        }
    }

    if ( file_browser->view_mode == PTK_FB_ICON_VIEW || file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        exo_icon_view_set_drag_dest_item ( EXO_ICON_VIEW( widget ),
                                           tree_path, EXO_ICON_VIEW_DROP_INTO );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        gtk_tree_view_set_drag_dest_row( GTK_TREE_VIEW( widget ),
                                         tree_path,
                                         GTK_TREE_VIEW_DROP_INTO_OR_AFTER );
    }

    if ( tree_path )
        gtk_tree_path_free( tree_path );

    /* FIXME: Creating a new target list everytime is very inefficient,
         but currently gtk_drag_dest_get_target_list always returns NULL
         due to some strange reason, and cannot be used currently.  */
    target_list = gtk_target_list_new( drag_targets, G_N_ELEMENTS(drag_targets) );
    target = gtk_drag_dest_find_target( widget, drag_context, target_list );
    gtk_target_list_unref( target_list );

    if (target == GDK_NONE)
        gdk_drag_status( drag_context, 0, time);
    else
    {
        /* Only 'move' is available. The user force move action by pressing Shift key */
        if( (drag_context->actions & GDK_ACTION_ALL) == GDK_ACTION_MOVE )
            suggested_action = GDK_ACTION_MOVE;
        /* Only 'copy' is available. The user force copy action by pressing Ctrl key */
        else if( (drag_context->actions & GDK_ACTION_ALL) == GDK_ACTION_COPY )
            suggested_action = GDK_ACTION_COPY;
        /* Only 'link' is available. The user force link action by pressing Shift+Ctrl key */
        else if( (drag_context->actions & GDK_ACTION_ALL) == GDK_ACTION_LINK )
            suggested_action = GDK_ACTION_LINK;
        /* Several different actions are available. We have to figure out a good default action. */
        else
        {
            file_browser->pending_drag_status = 1;
            gtk_drag_get_data (widget, drag_context, target, time);
            suggested_action = drag_context->suggested_action;
        }
        gdk_drag_status( drag_context, suggested_action, time );
    }
    return TRUE;
}

gboolean on_folder_view_drag_leave ( GtkWidget *widget,
                                     GdkDragContext *drag_context,
                                     guint time,
                                     PtkFileBrowser* file_browser )
{
    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name( widget, "drag-leave" );

    file_browser->drag_source_dev = 0;

    if ( folder_view_auto_scroll_timer )
    {
        g_source_remove( folder_view_auto_scroll_timer );
        folder_view_auto_scroll_timer = 0;
    }
    return TRUE;
}


gboolean on_folder_view_drag_drop ( GtkWidget *widget,
                                    GdkDragContext *drag_context,
                                    gint x,
                                    gint y,
                                    guint time,
                                    PtkFileBrowser* file_browser )
{
    GdkAtom target = gdk_atom_intern( "text/uri-list", FALSE );
    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name( widget, "drag-drop" );

    gtk_drag_get_data ( widget, drag_context, target, time );
    return TRUE;
}


void on_folder_view_drag_end ( GtkWidget *widget,
                               GdkDragContext *drag_context,
                               gpointer user_data )
{
    PtkFileBrowser * file_browser = ( PtkFileBrowser* ) user_data;
    if ( folder_view_auto_scroll_timer )
    {
        g_source_remove( folder_view_auto_scroll_timer );
        folder_view_auto_scroll_timer = 0;
    }
    if ( file_browser->view_mode == PTK_FB_ICON_VIEW || file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        exo_icon_view_set_drag_dest_item( EXO_ICON_VIEW( widget ), NULL, 0 );
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        gtk_tree_view_set_drag_dest_row( GTK_TREE_VIEW( widget ), NULL, 0 );
    }
}

void ptk_file_browser_rename_selected_files( PtkFileBrowser* file_browser,
                                                        GList* files, char* cwd )
{
    GtkWidget * parent;
    VFSFileInfo* file;
    GList* l;
    
    if ( !file_browser )
        return;
    gtk_widget_grab_focus( file_browser->folder_view );
    parent = gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) );

    if ( ! files )
        return;

    for ( l = files; l; l = l->next )
    {
        file = (VFSFileInfo*)l->data;
        if ( !ptk_rename_file( NULL, file_browser, cwd, file, NULL, FALSE ) )
            break;
    }
}

#if 0
void ptk_file_browser_rename_selected_file( PtkFileBrowser* file_browser )
{
    GtkWidget * parent;
    GList* files;
    VFSFileInfo* file;

    gtk_widget_grab_focus( file_browser->folder_view );
    files = ptk_file_browser_get_selected_files( file_browser );
    if ( ! files )
        return ;
    file = vfs_file_info_ref( ( VFSFileInfo* ) files->data );
    g_list_foreach( files, ( GFunc ) vfs_file_info_unref, NULL );
    g_list_free( files );

    parent = gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) );
    ptk_rename_file( NULL, file_browser, ptk_file_browser_get_cwd( file_browser ),
                     file, NULL, FALSE );
    vfs_file_info_unref( file );

//#if 0
    // In place editing causes problems. So, I disable it.
    if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        gtk_tree_view_get_cursor( GTK_TREE_VIEW( file_browser->folder_view ),
                                  &tree_path, NULL );
        if ( ! tree_path )
            return ;
        col = gtk_tree_view_get_column( GTK_TREE_VIEW( file_browser->folder_view ), 0 );
        renderers = gtk_tree_view_column_get_cell_renderers( col );
        for ( l = renderers; l; l = l->next )
        {
            if ( GTK_IS_CELL_RENDERER_TEXT( l->data ) )
            {
                renderer = GTK_CELL_RENDERER( l->data );
                gtk_tree_view_set_cursor_on_cell( GTK_TREE_VIEW( file_browser->folder_view ),
                                                  tree_path,
                                                  col, renderer,
                                                  TRUE );
                break;
            }
        }
        g_list_free( renderers );
        gtk_tree_path_free( tree_path );
    }
    else if ( file_browser->view_mode == PTK_FB_ICON_VIEW || file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        exo_icon_view_get_cursor( EXO_ICON_VIEW( file_browser->folder_view ),
                                  &tree_path, &renderer );
        if ( ! tree_path || !renderer )
            return ;
        g_object_set( G_OBJECT( renderer ), "editable", TRUE, NULL );
        exo_icon_view_set_cursor( EXO_ICON_VIEW( file_browser->folder_view ),
                                  tree_path,
                                  renderer,
                                  TRUE );
        gtk_tree_path_free( tree_path );
    }
//#endif
}
#endif

gboolean ptk_file_browser_can_paste( PtkFileBrowser* file_browser )
{
    /* FIXME: return FALSE when we don't have write permission */
    return FALSE;
}

void ptk_file_browser_paste( PtkFileBrowser* file_browser )
{
    GList * sel_files;
    VFSFileInfo* file;
    gchar* dest_dir = NULL;

    sel_files = ptk_file_browser_get_selected_files( file_browser );
//MOD removed - if you want this then at least make sure src != dest
/*    if ( sel_files && sel_files->next == NULL &&
            ( file = ( VFSFileInfo* ) sel_files->data ) &&
            vfs_file_info_is_dir( ( VFSFileInfo* ) sel_files->data ) )
    {
        dest_dir = g_build_filename( ptk_file_browser_get_cwd( file_browser ),
                                     vfs_file_info_get_name( file ), NULL );
    }
*/
    ptk_clipboard_paste_files(
        GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ) ),
        dest_dir ? dest_dir : ptk_file_browser_get_cwd( file_browser ),
        file_browser->task_view );
    if ( dest_dir )
        g_free( dest_dir );
    if ( sel_files )
    {
        g_list_foreach( sel_files, ( GFunc ) vfs_file_info_unref, NULL );
        g_list_free( sel_files );
    }
}

void ptk_file_browser_paste_link( PtkFileBrowser* file_browser )  //MOD added
{
    ptk_clipboard_paste_links(
        GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ) ),
        ptk_file_browser_get_cwd( file_browser ),
        file_browser->task_view );
}

void ptk_file_browser_paste_target( PtkFileBrowser* file_browser )  //MOD added
{
    ptk_clipboard_paste_targets(
        GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ) ),
        ptk_file_browser_get_cwd( file_browser ),
        file_browser->task_view );
}

gboolean ptk_file_browser_can_cut_or_copy( PtkFileBrowser* file_browser )
{
    return FALSE;
}

void ptk_file_browser_cut( PtkFileBrowser* file_browser )
{
    /* What "cut" and "copy" do are the same.
    *  The only difference is clipboard_action = GDK_ACTION_MOVE.
    */
    ptk_file_browser_cut_or_copy( file_browser, FALSE );
}

void ptk_file_browser_cut_or_copy( PtkFileBrowser* file_browser,
                                   gboolean copy )
{
    GList * sels;

    sels = ptk_file_browser_get_selected_files( file_browser );
    if ( ! sels )
        return ;
    ptk_clipboard_cut_or_copy_files( ptk_file_browser_get_cwd( file_browser ),
                                     sels, copy );
    vfs_file_info_list_free( sels );
}

void ptk_file_browser_copy( PtkFileBrowser* file_browser )
{
    ptk_file_browser_cut_or_copy( file_browser, TRUE );
}

gboolean ptk_file_browser_can_delete( PtkFileBrowser* file_browser )
{
    /* FIXME: return FALSE when we don't have write permission. */
    return TRUE;
}

void ptk_file_browser_delete( PtkFileBrowser* file_browser )
{
    GList * sel_files;
    GtkWidget* parent_win;

    if ( ! file_browser->n_sel_files )
        return ;
    sel_files = ptk_file_browser_get_selected_files( file_browser );
    parent_win = gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) );
    ptk_delete_files( GTK_WINDOW( parent_win ),
                      ptk_file_browser_get_cwd( file_browser ),
                      sel_files, file_browser->task_view );
    vfs_file_info_list_free( sel_files );
}

GList* ptk_file_browser_get_selected_files( PtkFileBrowser* file_browser )
{
    GList * sel_files;
    GList* sel;
    GList* file_list = NULL;
    GtkTreeModel* model;
    GtkTreeIter it;
    VFSFileInfo* file;

    sel_files = folder_view_get_selected_items( file_browser, &model );
    if ( !sel_files )
        return NULL;

    for ( sel = sel_files; sel; sel = g_list_next( sel ) )
    {
        gtk_tree_model_get_iter( model, &it, ( GtkTreePath* ) sel->data );
        gtk_tree_model_get( model, &it, COL_FILE_INFO, &file, -1 );
        file_list = g_list_append( file_list, file );
    }
    g_list_foreach( sel_files,
                    ( GFunc ) gtk_tree_path_free,
                    NULL );
    g_list_free( sel_files );
    return file_list;
}

void ptk_file_browser_open_selected_files( PtkFileBrowser* file_browser )
{
    ptk_file_browser_open_selected_files_with_app( file_browser, NULL );
}

void ptk_file_browser_open_selected_files_with_app( PtkFileBrowser* file_browser,
                                                    char* app_desktop )

{
    GList * sel_files;
    sel_files = ptk_file_browser_get_selected_files( file_browser );

    // archive?
    if( sel_files && !xset_get_b( "arc_def_open" ) )
    {
        VFSFileInfo* file = vfs_file_info_ref( (VFSFileInfo*)sel_files->data );
        VFSMimeType* mime_type = vfs_file_info_get_mime_type( file );
        vfs_file_info_unref( file );    
        if ( ptk_file_archiver_is_format_supported( mime_type, TRUE ) )
        {
            
int no_write_access = 0;
#if defined(HAVE_EUIDACCESS)
    no_write_access = euidaccess( ptk_file_browser_get_cwd( file_browser ), W_OK );
#elif defined(HAVE_EACCESS)
    no_write_access = eaccess( ptk_file_browser_get_cwd( file_browser ), W_OK );
#endif

            // first file is archive - use default archive action
            if ( xset_get_b( "arc_def_ex" ) && !no_write_access )
            {
                ptk_file_archiver_extract( file_browser, sel_files,
                                        ptk_file_browser_get_cwd( file_browser ) );
                goto _done;
            }
            else if ( xset_get_b( "arc_def_exto" ) || 
                        ( xset_get_b( "arc_def_ex" ) && no_write_access ) )
            {
                ptk_file_archiver_extract( file_browser, sel_files, NULL );
                goto _done;
            }
            else if ( xset_get_b( "arc_def_list" ) )
            {
                ptk_file_archiver_extract( file_browser, sel_files, "////LIST" );
                goto _done;
            }
        }
    }

    ptk_open_files_with_app( ptk_file_browser_get_cwd( file_browser ),
                         sel_files, app_desktop, file_browser, FALSE, FALSE );
_done:
    vfs_file_info_list_free( sel_files );
}

void ptk_file_browser_paste_as( GtkMenuItem* item, PtkFileBrowser* file_browser )
{
    GtkClipboard * clip = gtk_clipboard_get( GDK_SELECTION_CLIPBOARD );
    GdkAtom gnome_target;
    GdkAtom uri_list_target;
    gchar **uri_list, **puri;
    GtkSelectionData* sel_data = NULL;
    GList* files = NULL;
    gchar* file_path;
    gint missing_targets = 0;
    char* str;
    gboolean is_cut = FALSE;
    char* uri_list_str;
    VFSFileInfo* file;
    char* file_dir;

    char* cwd = ptk_file_browser_get_cwd( file_browser );
    
    // get files from clip
    gnome_target = gdk_atom_intern( "x-special/gnome-copied-files", FALSE );
    sel_data = gtk_clipboard_wait_for_contents( clip, gnome_target );
    if ( sel_data )
    {
        if ( sel_data->length <= 0 || sel_data->format != 8 )
            return ;

        uri_list_str = ( char* ) sel_data->data;
        is_cut = ( 0 == strncmp( ( char* ) sel_data->data, "cut", 3 ) );

        if ( uri_list_str )
        {
            while ( *uri_list_str && *uri_list_str != '\n' )
                ++uri_list_str;
        }
    }
    else
    {
        uri_list_target = gdk_atom_intern( "text/uri-list", FALSE );
        sel_data = gtk_clipboard_wait_for_contents( clip, uri_list_target );
        if ( ! sel_data )
            return ;
        if ( sel_data->length <= 0 || sel_data->format != 8 )
            return ;
        uri_list_str = ( char* ) sel_data->data;
    }

    if ( !uri_list_str )
        return;

    // create file list
    puri = uri_list = g_uri_list_extract_uris( uri_list_str );
    while ( *puri )
    {
        file_path = g_filename_from_uri( *puri, NULL, NULL );
        if ( file_path )
        {
            if ( g_file_test( file_path, G_FILE_TEST_EXISTS ) )             
            {
                files = g_list_prepend( files, file_path );
            }
            else
                missing_targets++;
        }
        ++puri;
    }
    g_strfreev( uri_list );
    gtk_selection_data_free( sel_data );

    GList* l;
    GtkWidget* parent = gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) );
    
    for ( l = files; l; l = l->next )
    {
        file_path = (char*)l->data;
        file = vfs_file_info_new();
        vfs_file_info_get( file, file_path, NULL );
        file_dir = g_path_get_dirname( file_path );
        if ( !ptk_rename_file( NULL, file_browser, file_dir, file, cwd, !is_cut ) )
        {
            vfs_file_info_unref( file );
            g_free( file_dir );
            missing_targets = 0;
            break;
        }
        vfs_file_info_unref( file );
        g_free( file_dir );
    }
    g_list_foreach( files, ( GFunc ) g_free, NULL );
    g_list_free( files );

    if ( missing_targets > 0 )
        ptk_show_error( GTK_WINDOW( parent ),
                        g_strdup_printf ( _("Error") ),
                        g_strdup_printf ( "%i target%s missing",
                        missing_targets, 
                        missing_targets > 1 ? g_strdup_printf ( "s are" ) : 
                        g_strdup_printf ( " is" ) ) );
}

/*
void ptk_file_browser_paste_move( PtkFileBrowser* file_browser, char* setname )
{
    GtkClipboard * clip = gtk_clipboard_get( GDK_SELECTION_CLIPBOARD );
    GdkAtom gnome_target;
    GdkAtom uri_list_target;
    gchar **uri_list, **puri;
    GtkSelectionData* sel_data = NULL;
    GList* files = NULL;
    gchar* file_path;
    gint missing_targets = 0;
    char* str;
    gboolean is_cut = FALSE;
    char* uri_list_str;
    VFSFileInfo* file;
    char* file_dir;

    char* cwd = ptk_file_browser_get_cwd( file_browser );
    
    // get files from clip
    gnome_target = gdk_atom_intern( "x-special/gnome-copied-files", FALSE );
    sel_data = gtk_clipboard_wait_for_contents( clip, gnome_target );
    if ( sel_data )
    {
        if ( sel_data->length <= 0 || sel_data->format != 8 )
            return ;

        uri_list_str = ( char* ) sel_data->data;
        is_cut = ( 0 == strncmp( ( char* ) sel_data->data, "cut", 3 ) );

        if ( uri_list_str )
        {
            while ( *uri_list_str && *uri_list_str != '\n' )
                ++uri_list_str;
        }
    }
    else
    {
        uri_list_target = gdk_atom_intern( "text/uri-list", FALSE );
        sel_data = gtk_clipboard_wait_for_contents( clip, uri_list_target );
        if ( ! sel_data )
            return ;
        if ( sel_data->length <= 0 || sel_data->format != 8 )
            return ;
        uri_list_str = ( char* ) sel_data->data;
    }

    if ( !uri_list_str )
        return;

    // create file list
    puri = uri_list = g_uri_list_extract_uris( uri_list_str );
    while ( *puri )
    {
        file_path = g_filename_from_uri( *puri, NULL, NULL );
        if ( file_path )
        {
            if ( strstr( setname, "_2target" ) &&
                        g_file_test( file_path, G_FILE_TEST_IS_SYMLINK ) )
            {
                str = file_path;
                file_path = g_file_read_link( file_path, NULL );
                g_free( str );
            }
            if ( file_path )
            {
                if ( g_file_test( file_path, G_FILE_TEST_EXISTS ) )             
                {
                    files = g_list_prepend( files, file_path );
                }
                else
                    missing_targets++;
            }
        }
        ++puri;
    }
    g_strfreev( uri_list );
    gtk_selection_data_free( sel_data );

    // rename/move/copy/link
    GList* l;
    int job;
    gboolean as_root = FALSE;
    GtkWidget* parent = gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) );
    
    // set job
    if ( strstr( setname, "_2link" ) )
        job = 2;  // link ( ignore is_cut )
    else if ( strstr( setname, "_2target" ) )
        job = 1;  // copy ( ignore is_cut )
    else if ( strstr( setname, "_2file" ) )
    {
        if ( is_cut )
            job = 0;  // move
        else
            job = 1;  // copy
    }
    else
        job = 1;  // failsafe copy

    // as root
    if ( !strncmp( setname, "root_", 5 ) )
        as_root = TRUE;

    for ( l = files; l; l = l->next )
    {
        file_path = (char*)l->data;
        file = vfs_file_info_new();
        vfs_file_info_get( file, file_path, NULL );
        file_dir = g_path_get_dirname( file_path );
        if ( !ptk_rename_file( NULL, file_browser,
                         job, file_dir, file, cwd, as_root ) )
        {
            vfs_file_info_unref( file );
            g_free( file_dir );
            missing_targets = 0;
            break;
        }
        vfs_file_info_unref( file );
        g_free( file_dir );
    }
    g_list_foreach( files, ( GFunc ) g_free, NULL );
    g_list_free( files );

    if ( missing_targets > 0 )
        ptk_show_error( GTK_WINDOW( parent ),
                        g_strdup_printf ( "Error" ),
                        g_strdup_printf ( "%i target%s missing",
                        missing_targets, 
                        missing_targets > 1 ? g_strdup_printf ( "s are" ) : 
                        g_strdup_printf ( " is" ) ) );
}
*/

/*
void ptk_file_browser_copy_rename( PtkFileBrowser* file_browser, GList* sel_files,
                                                    char* cwd, char* setname )
{
    //
    //copy_rename          copy to name
    //copy_link            copy to link
    //root_copy_rename     root copy to name
    //root_copy_link       root copy to link
    //root_move            root move
    //
    GList* l;
    VFSFileInfo* file;
    GtkWidget* parent = gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) );

    if ( !sel_files )
        return;

    gboolean as_root = FALSE;
    int job;
    if ( strstr( setname, "copy_rename" ) )
        job = 1;  //copy
    else if ( strstr( setname, "copy_link" ) )
        job = 2;  //link
    else
        job = 0;  //move
    
    if ( !strncmp( setname, "root_", 5 ) )
        as_root = TRUE;

    for ( l = sel_files; l; l = l->next )
    {
        file = (VFSFileInfo*)l->data;
        if ( !ptk_rename_file( NULL, file_browser,
                         job, cwd, file, NULL, as_root ) )
            break;
    }    
}
*/

void ptk_file_browser_copycmd( PtkFileBrowser* file_browser, GList* sel_files,
                                                char* cwd, char* setname )
{
    if ( !setname || !file_browser || !sel_files )
        return;
    XSet* set2;
    char* copy_dest = NULL;
    char* move_dest = NULL;
    char* path;
    
    if ( !strcmp( setname, "copy_tab_prev" ) )
        copy_dest = main_window_get_tab_cwd( file_browser, -1 );
    else if ( !strcmp( setname, "copy_tab_next" ) )
        copy_dest = main_window_get_tab_cwd( file_browser, -2 );
    else if ( !strncmp( setname, "copy_tab_", 9 ) )
        copy_dest = main_window_get_tab_cwd( file_browser, atoi( setname + 9 ) );
    else if ( !strcmp( setname, "copy_panel_prev" ) )
        copy_dest = main_window_get_panel_cwd( file_browser, -1 );
    else if ( !strcmp( setname, "copy_panel_next" ) )
        copy_dest = main_window_get_panel_cwd( file_browser, -2 );
    else if ( !strncmp( setname, "copy_panel_", 11 ) )
        copy_dest = main_window_get_panel_cwd( file_browser, atoi( setname + 11 ) );
    else if ( !strcmp( setname, "copy_loc_last" ) )
    {
        set2 = xset_get( "copy_loc_last" );
        copy_dest = g_strdup( set2->desc );
    }
    else if ( !strcmp( setname, "move_tab_prev" ) )
        move_dest = main_window_get_tab_cwd( file_browser, -1 );
    else if ( !strcmp( setname, "move_tab_next" ) )
        move_dest = main_window_get_tab_cwd( file_browser, -2 );
    else if ( !strncmp( setname, "move_tab_", 9 ) )
        move_dest = main_window_get_tab_cwd( file_browser, atoi( setname + 9 ) );
    else if ( !strcmp( setname, "move_panel_prev" ) )
        move_dest = main_window_get_panel_cwd( file_browser, -1 );
    else if ( !strcmp( setname, "move_panel_next" ) )
        move_dest = main_window_get_panel_cwd( file_browser, -2 );
    else if ( !strncmp( setname, "move_panel_", 11 ) )
        move_dest = main_window_get_panel_cwd( file_browser, atoi( setname + 11 ) );
    else if ( !strcmp( setname, "move_loc_last" ) )
    {
        set2 = xset_get( "copy_loc_last" );
        move_dest = g_strdup( set2->desc );
    }
    else if ( !strcmp( setname, "copy_loc" ) || !strcmp( setname, "move_loc" ) )
    {
        char* folder;
        set2 = xset_get( "copy_loc_last" );
        if ( set2->desc )
            folder = set2->desc;
        else
            folder = cwd;
        path = xset_file_dialog( file_browser, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                                _("Choose Location"), folder, NULL );
        if ( path && g_file_test( path, G_FILE_TEST_IS_DIR ) )
        {
            if ( !strcmp( setname, "copy_loc" ) )
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
            xset_msg_dialog( file_browser, GTK_MESSAGE_ERROR, _("Invalid Destination"), NULL, 0, _("Destination same as source"), NULL, NULL );
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
                                GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET(
                                                                file_browser ) ) ),
                                file_browser->task_view );
        ptk_file_task_run( task );
        g_free( dest_dir );
    }
    else
    {
        xset_msg_dialog( file_browser, GTK_MESSAGE_ERROR, _("Invalid Destination"), NULL, 0, _("Invalid destination"), NULL, NULL );
    }
}

void ptk_file_browser_rootcmd( PtkFileBrowser* file_browser, GList* sel_files,
                                                char* cwd, char* setname )
{
    /*
     * root_copy_loc    copy to location
     * root_move2       move to
     * root_delete      delete
     */
    if ( !setname || !file_browser || !sel_files )
        return;
    XSet* set;
    char* path;
    char* cmd;
    char* task_name;
    
    char* file_paths = g_strdup( "" );
    GList* sel;
    char* file_path;
    char* file_path_q;
    char* str;
    int item_count = 0;
    for ( sel = sel_files; sel; sel = sel->next )
    {
        file_path = g_build_filename( cwd,
                    vfs_file_info_get_name( ( VFSFileInfo* ) sel->data ), NULL );
        file_path_q = bash_quote( file_path );
        str = file_paths;
        file_paths = g_strdup_printf( "%s %s", file_paths, file_path_q );
        g_free( str );
        g_free( file_path );
        g_free( file_path_q );
        item_count++;
    }
    
    if ( !strcmp( setname, "root_delete" ) )
    {
        if ( !app_settings.no_confirm )
        {
            str = g_strdup_printf( _("Delete %d selected item%s as root ?"),
                                            item_count, item_count > 1 ? "s" : "" );
            if ( xset_msg_dialog( file_browser, GTK_MESSAGE_WARNING, _("Confirm Delete As Root"), NULL, GTK_BUTTONS_YES_NO, _("DELETE AS ROOT"), str, NULL ) != GTK_RESPONSE_YES )
            {
                g_free( str );
                return;
            }
            g_free( str );
        }
        cmd = g_strdup_printf( "rm -r %s", file_paths );
        task_name = g_strdup( _("Delete As Root") );
    }
    else
    {
        char* folder;
        set = xset_get( setname );
        if ( set->desc )
            folder = set->desc;
        else
            folder = cwd;
        path = xset_file_dialog( file_browser, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                                _("Choose Location"), folder, NULL );
        if ( path && g_file_test( path, G_FILE_TEST_IS_DIR ) )
        {
            xset_set_set( set, "desc", path );
            char* quote_path = bash_quote( path );
            
            if ( !strcmp( setname, "root_move2" ) )
            {
                task_name = g_strdup( _("Move As Root") );
                // problem: no warning if already exists
                cmd = g_strdup_printf( "mv -f %s %s", file_paths, quote_path );
            }
            else
            {
                task_name = g_strdup( _("Copy As Root") );
                // problem: no warning if already exists
                cmd = g_strdup_printf( "cp -r %s %s", file_paths, quote_path );
            }
            
            g_free( quote_path );
            g_free( path );
        }
        else
            return;
    }
    g_free( file_paths );

    // root task
    PtkFileTask* task = ptk_file_exec_new( task_name, cwd, file_browser,
                                            file_browser->task_view );
    g_free( task_name );
    task->task->exec_command = cmd;
    task->task->exec_sync = TRUE;
    task->task->exec_popup = FALSE;
    task->task->exec_show_output = FALSE;
    task->task->exec_show_error = TRUE;
    task->task->exec_export = FALSE;
    task->task->exec_as_user = g_strdup( "root" );
    ptk_file_task_run( task );          
}

void ptk_file_browser_open_terminal( GtkWidget* item, PtkFileBrowser* file_browser )
{
//MOD just open a single terminal for the current folder
/*
    GList * sel;
    GList* sel_files = ptk_file_browser_get_selected_files( file_browser );
    VFSFileInfo* file;
    char* full_path;
    char* dir;

    if ( sel_files )
    {
        for ( sel = sel_files; sel; sel = sel->next )
        {
            file = ( VFSFileInfo* ) sel->data;
            full_path = g_build_filename( ptk_file_browser_get_cwd( file_browser ),
                                          vfs_file_info_get_name( file ), NULL );
            if ( g_file_test( full_path, G_FILE_TEST_IS_DIR ) )
            {
                g_signal_emit( file_browser, signals[ OPEN_ITEM_SIGNAL ], 0, full_path, PTK_OPEN_TERMINAL );
            }
            else
            {
                dir = g_path_get_dirname( full_path );
                g_signal_emit( file_browser, signals[ OPEN_ITEM_SIGNAL ], 0, dir, PTK_OPEN_TERMINAL );
                g_free( dir );
            }
            g_free( full_path );
        }
        g_list_foreach( sel_files, ( GFunc ) vfs_file_info_unref, NULL );
        g_list_free( sel_files );
    }
    else
    {
*/
        g_signal_emit( file_browser, signals[ OPEN_ITEM_SIGNAL ], 0,
                       ptk_file_browser_get_cwd( file_browser ), PTK_OPEN_TERMINAL );
//    }
}

void ptk_file_browser_hide_selected( PtkFileBrowser* file_browser,
                                                    GList* files, char* cwd )
{
    if ( xset_msg_dialog( file_browser, 0, _("Hide File"), NULL, GTK_BUTTONS_OK_CANCEL, _("The names of the selected files will be added to the '.hidden' file located in this folder, which will hide them from view in SpaceFM.  You may need to refresh the view or restart SpaceFM for the files to disappear.\n\nTo unhide a file, open the .hidden file in your text editor, remove the name of the file, and refresh."), NULL, NULL ) != GTK_RESPONSE_OK )
        return;
    
    VFSFileInfo* file;
    GList *l;

    if ( files )
    {
        for ( l = files; l; l = l->next )
        {
            file = ( VFSFileInfo* ) l->data;
            if ( !vfs_dir_add_hidden( cwd, vfs_file_info_get_name( file ) ) )
                ptk_show_error( GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET  ( file_browser ) ) ),
                            _("Error"), _("Error hiding files") );
        }
        // refresh from here causes a segfault occasionally
        //ptk_file_browser_refresh( NULL, file_browser );
    }
    else 
        ptk_show_error( GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET  ( file_browser ) ) ),
                           _("Error"), _( "No files are selected" ) );
}

#if 0
static void on_properties_dlg_destroy( GtkObject* dlg, GList* sel_files )
{
    g_list_foreach( sel_files, ( GFunc ) vfs_file_info_unref, NULL );
    g_list_free( sel_files );
}
#endif

void ptk_file_browser_file_properties( PtkFileBrowser* file_browser, int page )
{
    GtkWidget * parent;
    GList* sel_files = NULL;
    char* dir_name = NULL;
    const char* cwd;

    if ( ! file_browser )
        return ;
    sel_files = ptk_file_browser_get_selected_files( file_browser );
    cwd = ptk_file_browser_get_cwd( file_browser );
    if ( !sel_files )
    {
        VFSFileInfo * file = vfs_file_info_new();
        vfs_file_info_get( file, ptk_file_browser_get_cwd( file_browser ), NULL );
        sel_files = g_list_prepend( NULL, file );
        dir_name = g_path_get_dirname( cwd );
    }
    parent = gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) );
    ptk_show_file_properties( GTK_WINDOW( parent ),
                              dir_name ? dir_name : cwd,
                              sel_files, page );
    vfs_file_info_list_free( sel_files );
    g_free( dir_name );
}

void
on_popup_file_properties_activate ( GtkMenuItem *menuitem,
                                    gpointer user_data )
{
    GObject * popup = G_OBJECT( user_data );
    PtkFileBrowser* file_browser = ( PtkFileBrowser* ) g_object_get_data( popup,
                                                                          "PtkFileBrowser" );
    ptk_file_browser_file_properties( file_browser, 0 );
}

void ptk_file_browser_show_hidden_files( PtkFileBrowser* file_browser,
                                         gboolean show )
{
    if ( !!file_browser->show_hidden_files == show )
        return;
    file_browser->show_hidden_files = show;

    if ( file_browser->file_list )
    {
        ptk_file_browser_update_model( file_browser );
        g_signal_emit( file_browser, signals[ SEL_CHANGE_SIGNAL ], 0 );
    }

    if ( file_browser->side_dir )
    {
        ptk_dir_tree_view_show_hidden_files( file_browser->side_dir,
                                             file_browser->show_hidden_files );
    }
}

static gboolean on_dir_tree_button_press( GtkWidget* view,
                                        GdkEventButton* evt,
                                        PtkFileBrowser* file_browser )
{
    ptk_file_browser_focus_me( file_browser );

    //MOD Added left click
/*    if ( evt->type == GDK_BUTTON_PRESS && evt->button == 1 )    // left click
    {
        //startup_mode = FALSE;
        return FALSE;
    }       
*/
    if ( evt->type == GDK_BUTTON_PRESS && evt->button == 2 )    /* middle click */
    {
        GtkTreeModel * model;
        GtkTreePath* tree_path;
        GtkTreeIter it;
        
        model = gtk_tree_view_get_model( GTK_TREE_VIEW( view ) );
        if ( gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW( view ),
                                            evt->x, evt->y, &tree_path, NULL, NULL, NULL ) )
        {
            if ( gtk_tree_model_get_iter( model, &it, tree_path ) )
            {
                VFSFileInfo * file;
                gtk_tree_model_get( model, &it,
                                    COL_DIR_TREE_INFO,
                                    &file, -1 );
                if ( file )
                {
                    char* file_path;
                    file_path = ptk_dir_view_get_dir_path( model, &it );
                    g_signal_emit( file_browser, signals[ OPEN_ITEM_SIGNAL ], 0,
                                   file_path, PTK_OPEN_NEW_TAB );
                    g_free( file_path );
                    vfs_file_info_unref( file );
                }
            }
            gtk_tree_path_free( tree_path );
        }
        return TRUE;
    }
    return FALSE;
}


GtkTreeView* ptk_file_browser_create_dir_tree( PtkFileBrowser* file_browser )
{
    GtkWidget * dir_tree;
    GtkTreeSelection* dir_tree_sel;
    dir_tree = ptk_dir_tree_view_new( file_browser,
                                      file_browser->show_hidden_files );
    dir_tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( dir_tree ) );
    g_signal_connect ( dir_tree_sel, "changed",
                       G_CALLBACK ( on_dir_tree_sel_changed ),
                       file_browser );
    g_signal_connect ( dir_tree, "button-press-event",
                       G_CALLBACK ( on_dir_tree_button_press ),
                       file_browser );
                       
    // set font
    if ( xset_get_s_panel( file_browser->mypanel, "font_file" ) )
    {
        PangoFontDescription* font_desc = pango_font_description_from_string(
                        xset_get_s_panel( file_browser->mypanel, "font_file" ) );
        gtk_widget_modify_font( dir_tree, font_desc );
        pango_font_description_free( font_desc );
    }

    return GTK_TREE_VIEW ( dir_tree );
}

int file_list_order_from_sort_order( PtkFBSortOrder order )
{
    int col;
    switch ( order )
    {
    case PTK_FB_SORT_BY_NAME:
        col = COL_FILE_NAME;
        break;
    case PTK_FB_SORT_BY_SIZE:
        col = COL_FILE_SIZE;
        break;
    case PTK_FB_SORT_BY_MTIME:
        col = COL_FILE_MTIME;
        break;
    case PTK_FB_SORT_BY_TYPE:
        col = COL_FILE_DESC;
        break;
    case PTK_FB_SORT_BY_PERM:
        col = COL_FILE_PERM;
        break;
    case PTK_FB_SORT_BY_OWNER:
        col = COL_FILE_OWNER;
        break;
    default:
        col = COL_FILE_NAME;
    }
    return col;
}

void ptk_file_browser_set_sort_order( PtkFileBrowser* file_browser,
                                      PtkFBSortOrder order )
{
    int col;
    if ( order == file_browser->sort_order )
        return ;

    file_browser->sort_order = order;
    col = file_list_order_from_sort_order( order );

    if ( file_browser->file_list )
    {
        gtk_tree_sortable_set_sort_column_id(
            GTK_TREE_SORTABLE( file_browser->file_list ),
            col,
            file_browser->sort_type );
    }
}

void ptk_file_browser_set_sort_type( PtkFileBrowser* file_browser,
                                     GtkSortType order )
{
    int col;
    GtkSortType old_order;

    if ( order != file_browser->sort_type )
    {
        file_browser->sort_type = order;
        if ( file_browser->file_list )
        {
            gtk_tree_sortable_get_sort_column_id(
                GTK_TREE_SORTABLE( file_browser->file_list ),
                &col, &old_order );
            gtk_tree_sortable_set_sort_column_id(
                GTK_TREE_SORTABLE( file_browser->file_list ),
                col, order );
        }
    }
}

PtkFBSortOrder ptk_file_browser_get_sort_order( PtkFileBrowser* file_browser )
{
    return file_browser->sort_order;
}

GtkSortType ptk_file_browser_get_sort_type( PtkFileBrowser* file_browser )
{
    return file_browser->sort_type;
}

/* FIXME: Don't recreate the view if previous view is compact view */
void ptk_file_browser_view_as_icons( PtkFileBrowser* file_browser )
{
    if ( file_browser->view_mode == PTK_FB_ICON_VIEW )
        return ;

    ptk_file_list_show_thumbnails( PTK_FILE_LIST( file_browser->file_list ), TRUE,
                                   file_browser->max_thumbnail );

    file_browser->view_mode = PTK_FB_ICON_VIEW;
    gtk_widget_destroy( file_browser->folder_view );
    file_browser->folder_view = create_folder_view( file_browser, PTK_FB_ICON_VIEW );
    exo_icon_view_set_model( EXO_ICON_VIEW( file_browser->folder_view ),
                             file_browser->file_list );
    gtk_widget_show( file_browser->folder_view );
    gtk_container_add( GTK_CONTAINER( file_browser->folder_view_scroll ), file_browser->folder_view );
}

/* FIXME: Don't recreate the view if previous view is icon view */
void ptk_file_browser_view_as_compact_list( PtkFileBrowser* file_browser )
{
    if ( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
        return ;

    ptk_file_list_show_thumbnails( PTK_FILE_LIST( file_browser->file_list ), FALSE,
                                   file_browser->max_thumbnail );

    file_browser->view_mode = PTK_FB_COMPACT_VIEW;
    gtk_widget_destroy( file_browser->folder_view );
    file_browser->folder_view = create_folder_view( file_browser, PTK_FB_COMPACT_VIEW );
    exo_icon_view_set_model( EXO_ICON_VIEW( file_browser->folder_view ),
                             file_browser->file_list );
    gtk_widget_show( file_browser->folder_view );
    gtk_container_add( GTK_CONTAINER( file_browser->folder_view_scroll ), file_browser->folder_view );
}

void ptk_file_browser_view_as_list ( PtkFileBrowser* file_browser )
{
    if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
        return ;

    ptk_file_list_show_thumbnails( PTK_FILE_LIST( file_browser->file_list ), FALSE,
                                   file_browser->max_thumbnail );

    file_browser->view_mode = PTK_FB_LIST_VIEW;
    gtk_widget_destroy( file_browser->folder_view );
    file_browser->folder_view = create_folder_view( file_browser, PTK_FB_LIST_VIEW );
    gtk_tree_view_set_model( GTK_TREE_VIEW( file_browser->folder_view ),
                             file_browser->file_list );
    gtk_widget_show( file_browser->folder_view );
    gtk_container_add( GTK_CONTAINER( file_browser->folder_view_scroll ), file_browser->folder_view );

}

void ptk_file_browser_create_new_file( PtkFileBrowser* file_browser,
                                       gboolean create_folder )
{
    VFSFileInfo *file = NULL;
    if( ptk_create_new_file( GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ) ),
                         ptk_file_browser_get_cwd( file_browser ), create_folder, &file ) )
    {
        PtkFileList* list = PTK_FILE_LIST( file_browser->file_list );
        GtkTreeIter it;
        /* generate created event before FAM to enhance responsiveness. */
        vfs_dir_emit_file_created( file_browser->dir, vfs_file_info_get_name(file), file );

        /* select the created file */
        if( ptk_file_list_find_iter( list, &it, file ) )
        {
            GtkTreePath* tp = gtk_tree_model_get_path( GTK_TREE_MODEL(list), &it );
            if ( file_browser->view_mode == PTK_FB_ICON_VIEW || file_browser->view_mode == PTK_FB_COMPACT_VIEW )
            {
                exo_icon_view_select_path( EXO_ICON_VIEW( file_browser->folder_view ), tp );
                exo_icon_view_set_cursor( EXO_ICON_VIEW( file_browser->folder_view ), tp, NULL, FALSE );

                /* NOTE for dirty hack:
                 *  Layout of icon view is done in idle handler,
                 *  so we have to let it re-layout after the insertion of new item.
                  * or we cannot scroll to the specified path correctly.  */
                while( gtk_events_pending() )
                    gtk_main_iteration();
                exo_icon_view_scroll_to_path( EXO_ICON_VIEW( file_browser->folder_view ),
                                                        tp, FALSE, 0, 0 );
            }
            else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
            {
                //MOD  give new folder/file focus
                
                //GtkTreeSelection * tree_sel;
                //tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( file_browser->folder_view ) );
                //gtk_tree_selection_select_iter( tree_sel, &it );

                GtkTreeSelection *selection;
                GList            *selected_paths = NULL;
                GList            *lp;            
                /* save selected paths */
                selection = gtk_tree_view_get_selection (GTK_TREE_VIEW( file_browser->folder_view ));
                selected_paths = gtk_tree_selection_get_selected_rows (selection, NULL);
                
                gtk_tree_view_set_cursor(GTK_TREE_VIEW( file_browser->folder_view ), tp, NULL, FALSE);
                /* select all previously selected paths */
                for (lp = selected_paths; lp != NULL; lp = lp->next)
                gtk_tree_selection_select_path (selection, lp->data);

                /*
                while( gtk_events_pending() )
                    gtk_main_iteration();

                */
                gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW( file_browser->folder_view ),
                                                        tp, NULL, FALSE, 0, 0 );
            }
            gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );  //MOD
            gtk_tree_path_free( tp );
        }
        vfs_file_info_unref( file );
    }
}

guint ptk_file_browser_get_n_sel( PtkFileBrowser* file_browser,
                                  guint64* sel_size )
{
    if ( sel_size )
        *sel_size = file_browser->sel_size;
    return file_browser->n_sel_files;
}

void ptk_file_browser_before_chdir( PtkFileBrowser* file_browser,
                                    const char* path,
                                    gboolean* cancel )
{}

void ptk_file_browser_after_chdir( PtkFileBrowser* file_browser )
{}

void ptk_file_browser_content_change( PtkFileBrowser* file_browser )
{}

void ptk_file_browser_sel_change( PtkFileBrowser* file_browser )
{}

void ptk_file_browser_pane_mode_change( PtkFileBrowser* file_browser )
{}

void ptk_file_browser_open_item( PtkFileBrowser* file_browser, const char* path, int action )
{}

/* Side pane */

/*
void ptk_file_browser_set_side_pane_mode( PtkFileBrowser* file_browser,
                                          PtkFBSidePaneMode mode )
{
    GtkAdjustment * adj;
    if ( file_browser->side_pane_mode == mode )
        return ;
    file_browser->side_pane_mode = mode;

//    if ( ! file_browser->side_pane )
    if ( !ptk_file_browser_is_side_pane_visible( file_browser ) )
        return ;

    gtk_widget_destroy( GTK_WIDGET( file_browser->side_view ) );
    adj = gtk_scrolled_window_get_hadjustment(
              GTK_SCROLLED_WINDOW( file_browser->side_view_scroll ) );
    gtk_adjustment_set_value( adj, 0 );
    switch ( file_browser->side_pane_mode )
    {
    case PTK_FB_SIDE_PANE_DIR_TREE:
        gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( file_browser->side_view_scroll ),
                                        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
        file_browser->side_view = ptk_file_browser_create_dir_tree( file_browser );
        gtk_toggle_tool_button_set_active ( file_browser->dir_tree_btn, TRUE );
        break;
    case PTK_FB_SIDE_PANE_BOOKMARKS:
        gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( file_browser->side_view_scroll ),
                                        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
        file_browser->side_view = ptk_file_browser_create_location_view( file_browser );
        gtk_toggle_tool_button_set_active ( file_browser->location_btn, TRUE );
        break;
    }
    gtk_container_add( GTK_CONTAINER( file_browser->side_view_scroll ),
                       GTK_WIDGET( file_browser->side_view ) );
    gtk_widget_show( GTK_WIDGET( file_browser->side_view ) );

//    if ( file_browser->side_pane && file_browser->file_list )
    if ( ptk_file_browser_is_side_pane_visible( file_browser ) && file_browser->file_list )
    {
        side_pane_chdir( file_browser, ptk_file_browser_get_cwd( file_browser ) );
    }

    g_signal_emit( file_browser, signals[ PANE_MODE_CHANGE_SIGNAL ], 0 );
}

PtkFBSidePaneMode
ptk_file_browser_get_side_pane_mode( PtkFileBrowser* file_browser )
{
    return file_browser->side_pane_mode;
}

static void on_show_location_view( GtkWidget* item, PtkFileBrowser* file_browser )
{
    if ( gtk_toggle_tool_button_get_active( GTK_TOGGLE_TOOL_BUTTON( item ) ) )
        ptk_file_browser_set_side_pane_mode( file_browser, PTK_FB_SIDE_PANE_BOOKMARKS );
}

static void on_show_dir_tree( GtkWidget* item, PtkFileBrowser* file_browser )
{
    if ( gtk_toggle_tool_button_get_active( GTK_TOGGLE_TOOL_BUTTON( item ) ) )
        ptk_file_browser_set_side_pane_mode( file_browser, PTK_FB_SIDE_PANE_DIR_TREE );
}

static PtkToolItemEntry side_pane_bar[] = {
                                              PTK_RADIO_TOOL_ITEM( NULL, "gtk-harddisk", N_( "Location" ), on_show_location_view ),
                                              PTK_RADIO_TOOL_ITEM( NULL, "gtk-open", N_( "Directory Tree" ), on_show_dir_tree ),
                                              PTK_TOOL_END
                                          };

static void ptk_file_browser_create_side_pane( PtkFileBrowser* file_browser )
{
    GtkTooltips* tooltips = gtk_tooltips_new();
    file_browser->side_pane_buttons = gtk_toolbar_new();
    
    file_browser->side_pane = gtk_vbox_new( FALSE, 0 );
    file_browser->side_view_scroll = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW( file_browser->side_view_scroll ),
                                         GTK_SHADOW_IN );

    switch ( file_browser->side_pane_mode )
    {
    case PTK_FB_SIDE_PANE_DIR_TREE:
        gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( file_browser->side_view_scroll ),
                                        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
        file_browser->side_view = ptk_file_browser_create_dir_tree( file_browser );
        break;
    case PTK_FB_SIDE_PANE_BOOKMARKS:
    default:
        gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( file_browser->side_view_scroll ),
                                        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
        file_browser->side_view = ptk_file_browser_create_location_view( file_browser );
        break;
    }
    gtk_container_add( GTK_CONTAINER( file_browser->side_view_scroll ),
                       GTK_WIDGET( file_browser->side_view ) );
    gtk_box_pack_start( GTK_BOX( file_browser->side_pane ),
                        GTK_WIDGET( file_browser->side_view_scroll ),
                        TRUE, TRUE, 0 );
    
    gtk_toolbar_set_style( GTK_TOOLBAR( file_browser->side_pane_buttons ), GTK_TOOLBAR_ICONS );
    side_pane_bar[ 0 ].ret = ( GtkWidget** ) ( GtkWidget * ) & file_browser->location_btn;
    side_pane_bar[ 1 ].ret = ( GtkWidget** ) ( GtkWidget * ) & file_browser->dir_tree_btn;
    ptk_toolbar_add_items_from_data( file_browser->side_pane_buttons,
                                     side_pane_bar, file_browser, tooltips );
    gtk_box_pack_start( GTK_BOX( file_browser->side_pane ),
                        file_browser->side_pane_buttons, FALSE, FALSE, 0 );
                        
    gtk_widget_show_all( file_browser->side_pane );
    if ( !file_browser->show_side_pane_buttons )
    {
        gtk_widget_hide( file_browser->side_pane_buttons );
    }
    
    gtk_paned_pack1( GTK_PANED( file_browser ),
                     file_browser->side_pane, FALSE, TRUE );
}

void ptk_file_browser_show_side_pane( PtkFileBrowser* file_browser,
                                      PtkFBSidePaneMode mode )
{
    file_browser->side_pane_mode = mode;

    if ( ! file_browser->side_pane )
    {
        ptk_file_browser_create_side_pane( file_browser );

        if ( file_browser->file_list )
        {
            side_pane_chdir( file_browser, ptk_file_browser_get_cwd( file_browser ) );
        }

        switch ( mode )
        {
        case PTK_FB_SIDE_PANE_BOOKMARKS:
            gtk_toggle_tool_button_set_active( file_browser->location_btn, TRUE );
            break;
        case PTK_FB_SIDE_PANE_DIR_TREE:
            gtk_toggle_tool_button_set_active( file_browser->dir_tree_btn, TRUE );
            break;
        }
    }
    //gtk_widget_show( file_browser->side_pane );
    gtk_widget_show( file_browser->side_vbox );
    file_browser->show_side_pane = TRUE;
}

void ptk_file_browser_hide_side_pane( PtkFileBrowser* file_browser )
{
    //if ( file_browser->side_pane )
//    {
        file_browser->show_side_pane = FALSE;
        //gtk_widget_destroy( file_browser->side_pane );
        gtk_widget_destroy( file_browser->side_view );
        //file_browser->side_pane = NULL;
        file_browser->side_view = NULL;
        //file_browser->side_view_scroll = NULL;
        
        //file_browser->side_pane_buttons = NULL;
        //file_browser->location_btn = NULL;
        //file_browser->dir_tree_btn = NULL;
//    }
}

gboolean ptk_file_browser_is_side_pane_visible( PtkFileBrowser* file_browser )
{
    return file_browser->show_side_pane;
}

void ptk_file_browser_show_side_pane_buttons( PtkFileBrowser* file_browser )
{
    file_browser->show_side_pane_buttons = TRUE;
    if ( file_browser->side_pane )
    {
        gtk_widget_show( file_browser->side_pane_buttons );
    }
}

void ptk_file_browser_hide_side_pane_buttons( PtkFileBrowser* file_browser )
{
    file_browser->show_side_pane_buttons = FALSE;
    if ( file_browser->side_pane )
    {
        gtk_widget_hide( file_browser->side_pane_buttons );
    }
}
*/

void ptk_file_browser_show_shadow( PtkFileBrowser* file_browser )
{
    gtk_scrolled_window_set_shadow_type ( GTK_SCROLLED_WINDOW ( file_browser->folder_view_scroll ),
                                          GTK_SHADOW_IN );
}

void ptk_file_browser_hide_shadow( PtkFileBrowser* file_browser )
{
    gtk_scrolled_window_set_shadow_type ( GTK_SCROLLED_WINDOW ( file_browser->folder_view_scroll ),
                                          GTK_SHADOW_NONE );
}

void ptk_file_browser_show_thumbnails( PtkFileBrowser* file_browser,
                                       int max_file_size )
{
    file_browser->max_thumbnail = max_file_size;
    if ( file_browser->file_list )
    {
        ptk_file_list_show_thumbnails( PTK_FILE_LIST( file_browser->file_list ),
                                       file_browser->view_mode == PTK_FB_ICON_VIEW,
                                       max_file_size );
    }
}

void ptk_file_browser_update_display( PtkFileBrowser* file_browser )
{
    GtkTreeSelection * tree_sel;
    GList *sel = NULL, *l;
    GtkTreePath* tree_path;
    int big_icon_size, small_icon_size;

    if ( ! file_browser->file_list )
        return ;
    g_object_ref( G_OBJECT( file_browser->file_list ) );

    if ( file_browser->max_thumbnail )
        ptk_file_list_show_thumbnails( PTK_FILE_LIST( file_browser->file_list ),
                                       file_browser->view_mode == PTK_FB_ICON_VIEW,
                                       file_browser->max_thumbnail );

    vfs_mime_type_get_icon_size( &big_icon_size, &small_icon_size );

    if ( file_browser->view_mode == PTK_FB_ICON_VIEW || file_browser->view_mode == PTK_FB_COMPACT_VIEW )
    {
        sel = exo_icon_view_get_selected_items( EXO_ICON_VIEW( file_browser->folder_view ) );

        exo_icon_view_set_model( EXO_ICON_VIEW( file_browser->folder_view ), NULL );
        if( file_browser->view_mode == PTK_FB_ICON_VIEW )
            gtk_cell_renderer_set_fixed_size( file_browser->icon_render, big_icon_size, big_icon_size );
        else if( file_browser->view_mode == PTK_FB_COMPACT_VIEW )
            gtk_cell_renderer_set_fixed_size( file_browser->icon_render, small_icon_size, small_icon_size );
        exo_icon_view_set_model( EXO_ICON_VIEW( file_browser->folder_view ),
                                 GTK_TREE_MODEL( file_browser->file_list ) );

        for ( l = sel; l; l = l->next )
        {
            tree_path = ( GtkTreePath* ) l->data;
            exo_icon_view_select_path( EXO_ICON_VIEW( file_browser->folder_view ),
                                       tree_path );
            gtk_tree_path_free( tree_path );
        }
    }
    else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( file_browser->folder_view ) );
        sel = gtk_tree_selection_get_selected_rows( tree_sel, NULL );

        gtk_tree_view_set_model( GTK_TREE_VIEW( file_browser->folder_view ), NULL );
        gtk_cell_renderer_set_fixed_size( file_browser->icon_render, small_icon_size, small_icon_size );
        gtk_tree_view_set_model( GTK_TREE_VIEW( file_browser->folder_view ),
                                 GTK_TREE_MODEL( file_browser->file_list ) );

        for ( l = sel; l; l = l->next )
        {
            tree_path = ( GtkTreePath* ) l->data;
            gtk_tree_selection_select_path( tree_sel, tree_path );
            gtk_tree_path_free( tree_path );
        }
    }
    g_list_free( sel );
    g_object_unref( G_OBJECT( file_browser->file_list ) );
}

void ptk_file_browser_emit_open( PtkFileBrowser* file_browser,
                                 const char* path,
                                 PtkOpenAction action )
{
    g_signal_emit( file_browser, signals[ OPEN_ITEM_SIGNAL ], 0, path, action );
}

void ptk_file_browser_set_single_click( PtkFileBrowser* file_browser, gboolean single_click )
{
    if( single_click == file_browser->single_click )
        return;
    if( file_browser->view_mode == PTK_FB_LIST_VIEW )
        exo_tree_view_set_single_click( (ExoTreeView*)file_browser->folder_view, single_click );
    else if( file_browser->view_mode == PTK_FB_ICON_VIEW || file_browser->view_mode == PTK_FB_COMPACT_VIEW )
        exo_icon_view_set_single_click( (ExoIconView*)file_browser->folder_view, single_click );
    file_browser->single_click = single_click;
}

void ptk_file_browser_set_single_click_timeout( PtkFileBrowser* file_browser, guint timeout )
{
    if( timeout == file_browser->single_click_timeout )
        return;
    if( file_browser->view_mode == PTK_FB_LIST_VIEW )
        exo_tree_view_set_single_click_timeout( (ExoTreeView*)file_browser->folder_view, timeout );
    else if( file_browser->view_mode == PTK_FB_ICON_VIEW || file_browser->view_mode == PTK_FB_COMPACT_VIEW )
        exo_icon_view_set_single_click_timeout( (ExoIconView*)file_browser->folder_view, timeout );
    file_browser->single_click_timeout = timeout;
}

////////////////////////////////////////////////////////////////////////////

int ptk_file_browser_no_access( char* cwd, char* smode )
{
    int mode;
    if ( !smode )
        mode = W_OK;
    else if ( !strcmp( smode, "R_OK" ) )
        mode = R_OK;
    else
        mode = W_OK;
        
    int no_access = 0;
    #if defined(HAVE_EUIDACCESS)
        no_access = euidaccess( cwd, mode );
    #elif defined(HAVE_EACCESS)
        no_access = eaccess( cwd, mode );
    #endif
    return no_access;
}

int bookmark_item_comp( const char* item, const char* path )
{
    return strcmp( ptk_bookmarks_item_get_path( item ), path );
}

void ptk_file_browser_add_bookmark( GtkMenuItem *menuitem, PtkFileBrowser* file_browser )
{
    const char* path = ptk_file_browser_get_cwd( PTK_FILE_BROWSER( file_browser ) );
    gchar* name;

    if ( ! g_list_find_custom( app_settings.bookmarks->list,
                               path,
                               ( GCompareFunc ) bookmark_item_comp ) )
    {
        name = g_path_get_basename( path );
        ptk_bookmarks_append( name, path );
        g_free( name );
    }
    else
        ptk_show_error( gtk_widget_get_toplevel( file_browser ), _("Error"),
                                                _("Bookmark already exists") );

}

void ptk_file_browser_find_file( GtkMenuItem *menuitem, PtkFileBrowser* file_browser )
{
    const char* cwd;
    const char* dirs[2];
    cwd = ptk_file_browser_get_cwd( file_browser );

    dirs[0] = cwd;
    dirs[1] = NULL;
    fm_find_files( dirs );
}

/*
void ptk_file_browser_open_folder_as_root( GtkMenuItem *menuitem,
                                                PtkFileBrowser* file_browser )
{
    const char* cwd;
    //char* cmd_line;
    GError *err = NULL;
    char* argv[5];  //MOD
    
    cwd = ptk_file_browser_get_cwd( file_browser );

    //MOD separate arguments for ktsuss compatibility
    //cmd_line = g_strdup_printf( "%s --no-desktop '%s'", g_get_prgname(), cwd );
    argv[1] = g_get_prgname();
    argv[2] = g_strdup_printf ( "--no-desktop" );
    argv[3] = cwd;
    argv[4] = NULL;
    if( ! vfs_sudo_cmd_async( cwd, argv, &err ) )  //MOD
    {
        ptk_show_error( gtk_widget_get_toplevel( file_browser ), _("Error"), err->message );
        g_error_free( err );
    }
    //g_free( cmd_line );
}
*/

void ptk_file_browser_focus( GtkMenuItem *item, PtkFileBrowser* file_browser, int job2 )
{
    GtkWidget* widget;
    int job;
    if ( item )
        job = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( item ), "job" ) );
    else
        job = job2;

    switch ( job )
    {
        case 0:
            // path bar
            if ( !xset_get_b_panel( file_browser->mypanel, "show_pathbar" ) )
            {
                xset_set_panel( file_browser->mypanel, "show_pathbar", "b", "1" );
                update_views_all_windows( NULL, file_browser );
            }
            widget = file_browser->path_bar;
            break;
        case 1:
            if ( !xset_get_b_panel( file_browser->mypanel, "show_dirtree" ) )
            {
                xset_set_panel( file_browser->mypanel, "show_dirtree", "b", "1" );
                update_views_all_windows( NULL, file_browser );
            }
            widget = file_browser->side_dir;
            break;
        case 2:
            if ( !xset_get_b_panel( file_browser->mypanel, "show_book" ) )
            {
                xset_set_panel( file_browser->mypanel, "show_book", "b", "1" );
                update_views_all_windows( NULL, file_browser );
            }
            widget = file_browser->side_book;
            break;
        case 3:
            if ( !xset_get_b_panel( file_browser->mypanel, "show_devmon" ) )
            {
                xset_set_panel( file_browser->mypanel, "show_devmon", "b", "1" );
                update_views_all_windows( NULL, file_browser );
            }
            widget = file_browser->side_dev;
            break;
        case 4:
            widget = file_browser->folder_view;
            break;
        default:
            return;
    }
    if ( GTK_WIDGET_VISIBLE( widget ) )
        gtk_widget_grab_focus( GTK_WIDGET( widget ) );
}

void focus_folder_view( PtkFileBrowser* file_browser )
{
    gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );
    g_signal_emit( file_browser, signals[ PANE_MODE_CHANGE_SIGNAL ], 0 );
}

void ptk_file_browser_focus_me( PtkFileBrowser* file_browser )
{
    g_signal_emit( file_browser, signals[ PANE_MODE_CHANGE_SIGNAL ], 0 );
}

void ptk_file_browser_go_tab( GtkMenuItem *item, PtkFileBrowser* file_browser,
                                                                        int t )
{
    GtkNotebook* notebook = file_browser->mynotebook;
    int tab_num;
    if ( item )
        tab_num = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( item ), "tab_num" ) );
    else
        tab_num = t;
    
    if ( tab_num == -1 )  // prev
    {
        if ( gtk_notebook_get_current_page( notebook ) == 0 )
            gtk_notebook_set_current_page( notebook, 
                            gtk_notebook_get_n_pages( notebook ) - 1 );
        else
            gtk_notebook_prev_page( notebook );
    }
    else if ( tab_num == -2 )  // next
    {
        if ( gtk_notebook_get_current_page( notebook ) + 1 ==
                                gtk_notebook_get_n_pages( notebook ) )
            gtk_notebook_set_current_page( notebook, 0 );
        else
            gtk_notebook_next_page( notebook );
    }
    else if ( tab_num == -3 )  // close
    {
        on_close_notebook_page( NULL, file_browser );
/*        // must switch page before close current or seg fault unless only
        int page = gtk_notebook_get_current_page( notebook );
        if ( gtk_notebook_get_current_page( notebook ) + 1
                            == gtk_notebook_get_n_pages( notebook ) )
            gtk_notebook_prev_page( notebook );
        else
            gtk_notebook_next_page( notebook );
        gtk_notebook_remove_page( notebook, page );
        if ( gtk_notebook_get_n_pages ( notebook ) == 0 )
            on_shortcut_new_tab_activate( NULL, file_browser );
*/    }
    else
    {
        if ( tab_num <= gtk_notebook_get_n_pages( notebook ) && tab_num > 0 )
            gtk_notebook_set_current_page( notebook, tab_num - 1 );
    }
}

void ptk_file_browser_open_in_tab( PtkFileBrowser* file_browser, int tab_num,
                                                        char* file_path )
{
    int page_x;
    GtkNotebook* notebook = file_browser->mynotebook;
    int cur_page = gtk_notebook_get_current_page( notebook );
    int pages = gtk_notebook_get_n_pages( notebook );
    
    if ( tab_num == -1 )  // prev
        page_x = cur_page - 1;
    else if ( tab_num == -2 )  // next
        page_x = cur_page + 1;
    else
        page_x = tab_num - 1;
    
    if ( page_x > -1 && page_x < pages && page_x != cur_page )
    {
        PtkFileBrowser* a_browser = (PtkFileBrowser*)gtk_notebook_get_nth_page(
                                                            notebook, page_x );
        ptk_file_browser_chdir( a_browser, file_path, PTK_FB_CHDIR_ADD_HISTORY );     
    }
}

void ptk_file_browser_on_permission( GtkMenuItem* item, PtkFileBrowser* file_browser,
                                                    GList* sel_files, char* cwd )
{
    char* name;
    char* cmd;
    const char* prog;
    gboolean as_root = FALSE;
    char* user1 = "1000";
    char* user2 = "1001";
    char* myuser = g_strdup_printf( "%d", geteuid() );
    
    if ( !sel_files )
        return;
        
    XSet* set = (XSet*)g_object_get_data( G_OBJECT( item ), "set" );
    if ( !set || !file_browser )
        return;
        
    if ( !strncmp( set->name, "perm_", 5 ) )
    {
        name = set->name + 5;
        if ( !strncmp( name, "go", 2 ) || !strncmp( name, "ugo", 3 ) )
            prog = "chmod -R";
        else
            prog = "chmod";
    }
    else if ( !strncmp( set->name, "rperm_", 6 ) )
    {
        name = set->name + 6;
        if ( !strncmp( name, "go", 2 ) || !strncmp( name, "ugo", 3 ) )
            prog = "chmod -R";
        else
            prog = "chmod";
        as_root = TRUE;
    }
    else if ( !strncmp( set->name, "own_", 4 ) )
    {
        name = set->name + 4;
        prog = "chown";
        as_root = TRUE;
    }
    else if ( !strncmp( set->name, "rown_", 5 ) )
    {
        name = set->name + 5;
        prog = "chown -R";
        as_root = TRUE;
    }
    else
        return;
    
    if ( !strcmp( name, "r" ) )
        cmd = g_strdup_printf( "u+r-wx,go-rwx" );
    else if ( !strcmp( name, "rw" ) )
        cmd = g_strdup_printf( "u+rw-x,go-rwx" );
    else if ( !strcmp( name, "rwx" ) )
        cmd = g_strdup_printf( "u+rwx,go-rwx" );
    else if ( !strcmp( name, "r_r" ) )
        cmd = g_strdup_printf( "u+r-wx,g+r-wx,o-rwx" );
    else if ( !strcmp( name, "rw_r" ) )
        cmd = g_strdup_printf( "u+rw-x,g+r-wx,o-rwx" );
    else if ( !strcmp( name, "rw_rw" ) )
        cmd = g_strdup_printf( "u+rw-x,g+rw-x,o-rwx" );
    else if ( !strcmp( name, "rwxr_x" ) )
        cmd = g_strdup_printf( "u+rwx,g+rx-w,o-rwx" );
    else if ( !strcmp( name, "rwxrwx" ) )
        cmd = g_strdup_printf( "u+rwx,g+rwx,o-rwx" );
    else if ( !strcmp( name, "r_r_r" ) )
        cmd = g_strdup_printf( "ugo+r,ugo-wx" );
    else if ( !strcmp( name, "rw_r_r" ) )
        cmd = g_strdup_printf( "u+rw-x,go+r-wx" );
    else if ( !strcmp( name, "rw_rw_rw" ) )
        cmd = g_strdup_printf( "ugo+rw-x" );
    else if ( !strcmp( name, "rwxr_r" ) )
        cmd = g_strdup_printf( "u+rwx,go+r-wx" );
    else if ( !strcmp( name, "rwxr_xr_x" ) )
        cmd = g_strdup_printf( "u+rwx,go+rx-w" );
    else if ( !strcmp( name, "rwxrwxrwx" ) )
        cmd = g_strdup_printf( "ugo+rwx,-t" );
    else if ( !strcmp( name, "rwxrwxrwt" ) )
        cmd = g_strdup_printf( "ugo+rwx,+t" );
    else if ( !strcmp( name, "unstick" ) )
        cmd = g_strdup_printf( "-t" );
    else if ( !strcmp( name, "stick" ) )
        cmd = g_strdup_printf( "+t" );
    else if ( !strcmp( name, "go_w" ) )
        cmd = g_strdup_printf( "go-w" );
    else if ( !strcmp( name, "go_rwx" ) )
        cmd = g_strdup_printf( "go-rwx" );
    else if ( !strcmp( name, "ugo_w" ) )
        cmd = g_strdup_printf( "ugo+w" );
    else if ( !strcmp( name, "ugo_rx" ) )
        cmd = g_strdup_printf( "ugo+rX" );
    else if ( !strcmp( name, "ugo_rwx" ) )
        cmd = g_strdup_printf( "ugo+rwX" );
    else if ( !strcmp( name, "myuser" ) )
        cmd = g_strdup_printf( "%s:%s", myuser, myuser );
    else if ( !strcmp( name, "myuser_users" ) )
        cmd = g_strdup_printf( "%s:users", myuser );
    else if ( !strcmp( name, "user1" ) )
        cmd = g_strdup_printf( "%s:%s", user1, user1 );
    else if ( !strcmp( name, "user1_users" ) )
        cmd = g_strdup_printf( "%s:users", user1 );
    else if ( !strcmp( name, "user2" ) )
        cmd = g_strdup_printf( "%s:%s", user2, user2 );
    else if ( !strcmp( name, "user2_users" ) )
        cmd = g_strdup_printf( "%s:users", user2 );
    else if ( !strcmp( name, "root" ) )
        cmd = g_strdup_printf( "root:root" );
    else if ( !strcmp( name, "root_users" ) )
        cmd = g_strdup_printf( "root:users" );
    else if ( !strcmp( name, "root_myuser" ) )
        cmd = g_strdup_printf( "root:%s", myuser );
    else if ( !strcmp( name, "root_user1" ) )
        cmd = g_strdup_printf( "root:%s", user1 );
    else if ( !strcmp( name, "root_user2" ) )
        cmd = g_strdup_printf( "root:%s", user2 );
    else
        return;
    
    char* file_paths = g_strdup( "" );
    GList* sel;
    char* file_path;
    char* str;
    for ( sel = sel_files; sel; sel = sel->next )
    {
        file_path = bash_quote( vfs_file_info_get_name( ( VFSFileInfo* ) sel->data ) );
        str = file_paths;
        file_paths = g_strdup_printf( "%s %s", file_paths, file_path );
        g_free( str );
        g_free( file_path );
    }

    // task
    PtkFileTask* task = ptk_file_exec_new( set->menu_label,
                                        cwd,
                                        file_browser,
                                        file_browser->task_view );
    task->task->exec_command = g_strdup_printf( "%s %s %s", prog, cmd, file_paths );
    g_free( cmd );
    g_free( file_paths );
    task->task->exec_browser = file_browser;
    task->task->exec_sync = TRUE;
    task->task->exec_show_error = TRUE;
    task->task->exec_show_output = FALSE;
    task->task->exec_export = FALSE;
    if ( as_root )
        task->task->exec_as_user = g_strdup_printf( "root" );
    ptk_file_task_run( task );
}

void ptk_file_browser_on_action( PtkFileBrowser* browser, char* setname )
{
    char* xname;
    int i;
    XSet* set = xset_get( setname );
    
//printf("ptk_file_browser_on_action\n");

    if ( g_str_has_prefix( set->name, "book_" ) )
    {
        xname = set->name + 5;
        if ( !strcmp( xname, "icon" ) )
            update_bookmark_icons();
        else if ( !strcmp( xname, "new" ) )
            ptk_file_browser_add_bookmark( NULL, browser );
        else if ( !strcmp( xname, "rename" ) )
            on_bookmark_rename( NULL, browser );
        else if ( !strcmp( xname, "edit" ) )
            on_bookmark_edit( NULL, browser );
        else if ( !strcmp( xname, "remove" ) )
            on_bookmark_remove( NULL, browser );
        else if ( !strcmp( xname, "open" ) )
            on_bookmark_open( NULL, browser );
        else if ( !strcmp( xname, "tab" ) )
            on_bookmark_open_tab( NULL, browser );
    }
    else if ( g_str_has_prefix( set->name, "tool_" )
            || g_str_has_prefix( set->name, "stool_" )
            || g_str_has_prefix( set->name, "rtool_" ) )
    {
        if ( g_str_has_prefix( set->name, "tool_" ) )
            xname = set->name + 5;
        else
            xname = set->name + 6;
        
        if ( !strcmp( xname, "dirtree" ) )
        {
            xset_set_b( set->name, xset_get_b_panel( browser->mypanel,
                                                                "show_dirtree" ) );
            on_toggle_sideview( NULL, browser, 0 );
        }
        else if ( !strcmp( xname, "book" ) )
        {
            xset_set_b( set->name, xset_get_b_panel( browser->mypanel,
                                                                "show_book" ) );
            on_toggle_sideview( NULL, browser, 1 );
        }
        else if ( !strcmp( xname, "device" ) )
        {
            xset_set_b( set->name, xset_get_b_panel( browser->mypanel,
                                                                "show_devmon" ) );
            on_toggle_sideview( NULL, browser, 2 );
        }
        else if ( !strcmp( xname, "newtab" ) )
            on_shortcut_new_tab_activate( NULL, browser );
        else if ( !strcmp( xname, "newtabhere" ) )
            on_shortcut_new_tab_here( NULL, browser );
        else if ( !strcmp( xname, "back" ) )
            ptk_file_browser_go_back( NULL, browser );
        else if ( !strcmp( xname, "backmenu" ) )
            ptk_file_browser_go_back( NULL, browser );
        else if ( !strcmp( xname, "forward" ) )
            ptk_file_browser_go_forward( NULL, browser );
        else if ( !strcmp( xname, "forwardmenu" ) )
            ptk_file_browser_go_forward( NULL, browser );
        else if ( !strcmp( xname, "up" ) )
            ptk_file_browser_go_up( NULL, browser );
        else if ( !strcmp( xname, "default" ) )
            ptk_file_browser_go_default( NULL, browser );
        else if ( !strcmp( xname, "home" ) )
            ptk_file_browser_go_home( NULL, browser );
        else if ( !strcmp( xname, "refresh" ) )
            ptk_file_browser_refresh( NULL, browser );
    }
    else if ( !strcmp( set->name, "toolbar_hide" ) )
        on_toolbar_hide( NULL, browser, browser->toolbar );
    else if ( !strcmp( set->name, "toolbar_hide_side" ) )
        on_toolbar_hide( NULL, browser, browser->side_toolbar );
    else if ( !strcmp( set->name, "toolbar_help" ) )
        on_toolbar_help( NULL, browser );
    else if ( g_str_has_prefix( set->name, "go_" ) )
    {
        xname = set->name + 3;
        if ( !strcmp( xname, "back" ) )
            ptk_file_browser_go_back( NULL, browser );
        else if ( !strcmp( xname, "forward" ) )
            ptk_file_browser_go_forward( NULL, browser );
        else if ( !strcmp( xname, "up" ) )
            ptk_file_browser_go_up( NULL, browser );
        else if ( !strcmp( xname, "home" ) )
            ptk_file_browser_go_home( NULL, browser );
        else if ( !strcmp( xname, "default" ) )
            ptk_file_browser_go_default( NULL, browser );
        else if ( !strcmp( xname, "set_default" ) )
            ptk_file_browser_set_default_folder( NULL, browser );
    }
    else if ( g_str_has_prefix( set->name, "tab_" ) )
    {
        xname = set->name + 4;
        if ( !strcmp( xname, "new" ) )
            on_shortcut_new_tab_activate( NULL, browser );
        else if ( !strcmp( xname, "new_here" ) )
            on_shortcut_new_tab_here( NULL, browser );
        else
        {
            if ( !strcmp( xname, "prev" ) )
                i = -1;
            else if ( !strcmp( xname, "next" ) )
                i = -2;
            else if ( !strcmp( xname, "close" ) )
                i = -3;
            else
                i = atoi( xname );
            ptk_file_browser_go_tab( NULL, browser, i );
        }
    }
    else if ( g_str_has_prefix( set->name, "focus_" ) )
    {
        xname = set->name + 6;
        if ( !strcmp( xname, "path_bar" ) )
            i = 0;
        else if ( !strcmp( xname, "filelist" ) )
            i = 4;
        else if ( !strcmp( xname, "dirtree" ) )
            i = 1;
        else if ( !strcmp( xname, "book" ) )
            i = 2;
        else if ( !strcmp( xname, "device" ) )
            i = 3;
        ptk_file_browser_focus( NULL, browser, i );
    }
    else if ( !strcmp( set->name, "view_reorder_col" ) )
        on_reorder( NULL, browser );
    else if ( !strcmp( set->name, "view_refresh" ) )
        ptk_file_browser_refresh( NULL, browser );
    else if ( g_str_has_prefix( set->name, "sortby_" ) )
    {
        xname = set->name + 7;
        i = -3;
        if ( !strcmp( xname, "name" ) )
            i = PTK_FB_SORT_BY_NAME;
        else if ( !strcmp( xname, "size" ) )
            i = PTK_FB_SORT_BY_SIZE;
        else if ( !strcmp( xname, "type" ) )
            i = PTK_FB_SORT_BY_TYPE;
        else if ( !strcmp( xname, "perm" ) )
            i = PTK_FB_SORT_BY_PERM;
        else if ( !strcmp( xname, "owner" ) )
            i = PTK_FB_SORT_BY_OWNER;
        else if ( !strcmp( xname, "date" ) )
            i = PTK_FB_SORT_BY_MTIME;
        else if ( !strcmp( xname, "ascend" ) )
        {
            i = -1;
            set->b = browser->sort_type == GTK_SORT_ASCENDING ? XSET_B_TRUE : XSET_B_FALSE;
        }
        else if ( !strcmp( xname, "descend" ) )
        {
            i = -2;
            set->b = browser->sort_type == GTK_SORT_DESCENDING ? XSET_B_TRUE : XSET_B_FALSE;
        }
        if ( i > 0 )
            set->b = browser->sort_order == i ? XSET_B_TRUE : XSET_B_FALSE;
        on_popup_sortby( NULL, browser, i );
    }
    else if ( !strcmp( set->name, "path_help" ) )
        ptk_path_entry_help( NULL, browser );
    else if ( g_str_has_prefix( set->name, "panel" ) )
    {
        i = 0;
        if ( strlen( set->name ) > 6 )
        {
            xname = g_strdup( set->name + 5 );
            xname[1] = '\0';
            i = atoi( xname );
            xname[1] = '_';
            g_free( xname );
        }
        //printf( "ACTION panelN=%d  %c\n", i, set->name[5] );
        if ( i > 0 && i < 5 )
        {
            xname = set->name + 7;
            if ( !strcmp( xname, "show_hidden" ) )  // shared key
            {
                ptk_file_browser_show_hidden_files( browser,
                            xset_get_b_panel( browser->mypanel, "show_hidden" ) );
            }
            else if ( !strcmp( xname, "show" ) ) // main View|Panel N
                show_panels_all_windows( NULL, (FMMainWindow*)browser->main_window );
            else if ( g_str_has_prefix( xname, "show_" ) )  // shared key
                update_views_all_windows( NULL, browser );
            else if ( !strcmp( xname, "list_detailed" ) )  // shared key
                on_popup_list_detailed( NULL, browser );
            else if ( !strcmp( xname, "list_icons" ) )  // shared key
                on_popup_list_icons( NULL, browser );
            else if ( !strcmp( xname, "list_compact" ) )  // shared key
                on_popup_list_compact( NULL, browser );
            else if ( !strcmp( xname, "icon_tab" )
                                        || g_str_has_prefix( xname, "font_" ) )
                main_update_fonts( NULL, browser );
            else if ( g_str_has_prefix( xname, "detcol_" )  // shared key
                                && browser->view_mode == PTK_FB_LIST_VIEW )
                on_folder_view_columns_changed( browser->folder_view,
                                                            browser );
            else if ( !strcmp( xname, "icon_status" ) )  // shared key
                on_status_effect_change( NULL, browser );
            else if ( !strcmp( xname, "font_status" ) )  // shared key
                on_status_effect_change( NULL, browser );
        }
    }
    else if ( g_str_has_prefix( set->name, "status_" ) )
    {
        xname = set->name + 7;
        if ( !strcmp( xname, "border" ) 
                || !strcmp( xname, "text" ) )
            on_status_effect_change( NULL, browser );
        else if ( !strcmp( xname, "name" ) 
                    || !strcmp( xname, "path" )
                    || !strcmp( xname, "info" )
                    || !strcmp( xname, "hide" ) )
            on_status_middle_click_config( NULL, set );
    }
    else if ( g_str_has_prefix( set->name, "paste_" ) )
    {
        xname = set->name + 6;
        if ( !strcmp( xname, "link" ) )
            ptk_file_browser_paste_link( browser );
        else if ( !strcmp( xname, "target" ) )
            ptk_file_browser_paste_target( browser );
        else if ( !strcmp( xname, "as" ) )
            ptk_file_browser_paste_as( NULL, browser );
    }
    else if ( g_str_has_prefix( set->name, "select_" ) )
    {
        xname = set->name + 7;
        if ( !strcmp( xname, "all" ) )
            ptk_file_browser_select_all( NULL, browser );
        else if ( !strcmp( xname, "un" ) )
            ptk_file_browser_unselect_all( NULL, browser );
        else if ( !strcmp( xname, "invert" ) )
            ptk_file_browser_invert_selection( NULL, browser );
    }
    else  // all the rest require ptkfilemenu data
        ptk_file_menu_action( browser, set->name );
}


