/*
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <libintl.h>

#include "pcmanfm.h"
#include "private.h"

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib-object.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>  // XGetWindowProperty
#include <X11/Xatom.h> // XA_CARDINAL

#include <string.h>
#include <malloc.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include "ptk-location-view.h"
#include "exo-tree-view.h"

#include "ptk-file-browser.h"

#include "main-window.h"
#include "ptk-utils.h"

#include "pref-dialog.h"
#include "ptk-file-properties.h"
#include "ptk-path-entry.h"
#include "ptk-file-menu.h"

#include "settings.h"
#include "item-prop.h"
#include "find-files.h"
#include "desktop.h"

#ifdef HAVE_STATVFS
/* FIXME: statvfs support should be moved to src/vfs */
#include <sys/statvfs.h>
#endif

#include "vfs-app-desktop.h"
#include "vfs-execute.h"
#include "vfs-utils.h"  /* for vfs_sudo() */
#include "go-dialog.h"
#include "vfs-file-task.h"
#include "ptk-location-view.h"
#include "ptk-clipboard.h"
#include "ptk-handler.h"

#include "gtk2-compat.h"

void rebuild_menus( FMMainWindow* main_window );

static void fm_main_window_class_init( FMMainWindowClass* klass );
static void fm_main_window_init( FMMainWindow* main_window );
static void fm_main_window_finalize( GObject *obj );
static void fm_main_window_get_property ( GObject *obj,
                                          guint prop_id,
                                          GValue *value,
                                          GParamSpec *pspec );
static void fm_main_window_set_property ( GObject *obj,
                                          guint prop_id,
                                          const GValue *value,
                                          GParamSpec *pspec );
static gboolean fm_main_window_delete_event ( GtkWidget *widget,
                                              GdkEvent *event );
#if 0
static void fm_main_window_destroy ( GtkObject *object );
#endif

//static gboolean fm_main_window_key_press_event ( GtkWidget *widget,
//                                                 GdkEventKey *event );

static gboolean fm_main_window_window_state_event ( GtkWidget *widget,
                                                    GdkEventWindowState *event );
static void fm_main_window_next_tab ( FMMainWindow* widget );
static void fm_main_window_prev_tab ( FMMainWindow* widget );


static void on_folder_notebook_switch_pape ( GtkNotebook *notebook,
                                             GtkWidget *page,
                                             guint page_num,
                                             gpointer user_data );
//static void on_close_tab_activate ( GtkMenuItem *menuitem,
//                                    gpointer user_data );

//static void on_file_browser_before_chdir( PtkFileBrowser* file_browser,
//                                          const char* path, gboolean* cancel,
//                                          FMMainWindow* main_window );
static void on_file_browser_begin_chdir( PtkFileBrowser* file_browser,
                                         FMMainWindow* main_window );
static void on_file_browser_open_item( PtkFileBrowser* file_browser,
                                       const char* path, PtkOpenAction action,
                                       FMMainWindow* main_window );
static void on_file_browser_after_chdir( PtkFileBrowser* file_browser,
                                         FMMainWindow* main_window );
static void on_file_browser_content_change( PtkFileBrowser* file_browser,
                                            FMMainWindow* main_window );
static void on_file_browser_sel_change( PtkFileBrowser* file_browser,
                                        FMMainWindow* main_window );
//static void on_file_browser_pane_mode_change( PtkFileBrowser* file_browser,
//                                              FMMainWindow* main_window );
void on_file_browser_panel_change( PtkFileBrowser* file_browser,
                                 FMMainWindow* main_window );
static void on_file_browser_splitter_pos_change( PtkFileBrowser* file_browser,
                                                 GParamSpec *param,
                                                 FMMainWindow* main_window );
static gboolean on_tab_drag_motion ( GtkWidget *widget,
                                     GdkDragContext *drag_context,
                                     gint x,
                                     gint y,
                                     guint time,
                                     PtkFileBrowser* file_browser );
static gboolean on_main_window_focus( GtkWidget* main_window,
                                      GdkEventFocus *event,
                                      gpointer user_data );

static gboolean on_main_window_keypress( FMMainWindow* main_window,
                                         GdkEventKey* event, XSet* known_set );
//static gboolean on_main_window_keyrelease( FMMainWindow* widget,
//                                        GdkEventKey* event, gpointer data);
static gboolean on_window_button_press_event( GtkWidget* widget, 
                                       GdkEventButton *event,
                                       FMMainWindow* main_window );     //sfm
static void on_new_window_activate ( GtkMenuItem *menuitem,
                                     gpointer user_data );
static void fm_main_window_close( FMMainWindow* main_window );

GtkWidget* main_task_view_new( FMMainWindow* main_window );
void main_task_add_menu( FMMainWindow* main_window, GtkMenu* menu,
                                                GtkAccelGroup* accel_group );
void on_task_popup_show( GtkMenuItem* item, FMMainWindow* main_window, char* name2 );
gboolean main_tasks_running( FMMainWindow* main_window );
void on_task_stop( GtkMenuItem* item, GtkWidget* view, XSet* set2,
                                                            PtkFileTask* task2 );
void on_preference_activate ( GtkMenuItem *menuitem, gpointer user_data );
void main_task_prepare_menu( FMMainWindow* main_window, GtkWidget* menu,
                                                GtkAccelGroup* accel_group );
void on_task_columns_changed( GtkWidget *view, gpointer user_data );
PtkFileTask* get_selected_task( GtkWidget* view );
static void fm_main_window_update_status_bar( FMMainWindow* main_window,
                                              PtkFileBrowser* file_browser );
void set_window_title( FMMainWindow* main_window, PtkFileBrowser* file_browser );
void on_task_column_selected( GtkMenuItem* item, GtkWidget* view );
void on_task_popup_errset( GtkMenuItem* item, FMMainWindow* main_window, char* name2 );
void show_task_dialog( GtkWidget* widget, GtkWidget* view );
void on_about_activate ( GtkMenuItem *menuitem, gpointer user_data );
void on_main_help_activate ( GtkMenuItem *menuitem, FMMainWindow* main_window );
void on_main_faq ( GtkMenuItem *menuitem, FMMainWindow* main_window );
void on_homepage_activate ( GtkMenuItem *menuitem, FMMainWindow* main_window );
void on_news_activate ( GtkMenuItem *menuitem, FMMainWindow* main_window );
void on_getplug_activate ( GtkMenuItem *menuitem, FMMainWindow* main_window );
void update_window_title( GtkMenuItem* item, FMMainWindow* main_window );
void on_toggle_panelbar( GtkWidget* widget, FMMainWindow* main_window );
void on_fullscreen_activate ( GtkMenuItem *menuitem, FMMainWindow* main_window );
static gboolean delayed_focus( GtkWidget* widget );
static gboolean delayed_focus_file_browser( PtkFileBrowser* file_browser );
static gboolean idle_set_task_height( FMMainWindow* main_window );


static GtkWindowClass *parent_class = NULL;

static int n_windows = 0;
static GList* all_windows = NULL;

static guint theme_change_notify = 0;

//  Drag & Drop/Clipboard targets
static GtkTargetEntry drag_targets[] = {
                                           {"text/uri-list", 0 , 0 }
                                       };

GType fm_main_window_get_type()
{
    static GType type = G_TYPE_INVALID;
    if ( G_UNLIKELY ( type == G_TYPE_INVALID ) )
    {
        static const GTypeInfo info =
            {
                sizeof ( FMMainWindowClass ),
                NULL,
                NULL,
                ( GClassInitFunc ) fm_main_window_class_init,
                NULL,
                NULL,
                sizeof ( FMMainWindow ),
                0,
                ( GInstanceInitFunc ) fm_main_window_init,
                NULL,
            };
        type = g_type_register_static ( GTK_TYPE_WINDOW, "FMMainWindow", &info, 0 );
    }
    return type;
}

void fm_main_window_class_init( FMMainWindowClass* klass )
{
    GObjectClass * object_class;
    GtkWidgetClass *widget_class;

    object_class = ( GObjectClass * ) klass;
    parent_class = g_type_class_peek_parent ( klass );

    object_class->set_property = fm_main_window_set_property;
    object_class->get_property = fm_main_window_get_property;
    object_class->finalize = fm_main_window_finalize;

    widget_class = GTK_WIDGET_CLASS ( klass );
    widget_class->delete_event = (gpointer) fm_main_window_delete_event;
//widget_class->key_press_event = on_main_window_keypress;  //fm_main_window_key_press_event;
    widget_class->window_state_event = fm_main_window_window_state_event;

    /*  this works but desktop_window doesn't
    g_signal_new ( "task-notify",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       0,
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1, G_TYPE_POINTER );
    */
}


gboolean on_configure_evt_timer( FMMainWindow* main_window )
{
    // verify main_window still valid
    GList* l;
    for ( l = all_windows; l; l = l->next )
    {
        if ( (FMMainWindow*)l->data == main_window )
            break;
    }
    if ( !l )
        return FALSE;

    if ( main_window->configure_evt_timer )
    {
        g_source_remove( main_window->configure_evt_timer );
        main_window->configure_evt_timer = 0;
    }
    main_window_event( main_window, evt_win_move, "evt_win_move", 0, 0,
                                                    NULL, 0, 0, 0, TRUE );
    return FALSE;
}

gboolean on_window_configure_event( GtkWindow *window, 
                                    GdkEvent *event, FMMainWindow* main_window )
{
    // use timer to prevent rapid events during resize
    if ( ( evt_win_move->s || evt_win_move->ob2_data ) &&
                                        !main_window->configure_evt_timer )
        main_window->configure_evt_timer = g_timeout_add( 200,
                        ( GSourceFunc ) on_configure_evt_timer, main_window );
    return FALSE;
}

void on_plugin_install( GtkMenuItem* item, FMMainWindow* main_window, XSet* set2 )
{
    XSet* set;
    char* path = NULL;
    char* deffolder;
    char* plug_dir;
    char* msg;
    int type = 0;
    int job = PLUGIN_JOB_INSTALL;
    
    if ( !item )
        set = set2;
    else
        set = (XSet*)g_object_get_data( G_OBJECT(item), "set" );
    if ( !set )
        return;
    
    if ( g_str_has_suffix( set->name, "cfile" ) || g_str_has_suffix( set->name, "curl" ) )
        job = PLUGIN_JOB_COPY;

    if ( g_str_has_suffix( set->name, "file" ) )
    {
        // get file path
        XSet* save = xset_get( "plug_ifile" );
        if ( save->s )  //&& g_file_test( save->s, G_FILE_TEST_IS_DIR )
            deffolder = save->s;
        else
        {
            if ( !( deffolder = xset_get_s( "go_set_default" ) ) )
                deffolder = "/";
        }
        path = xset_file_dialog( GTK_WIDGET( main_window ),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        _("Choose Plugin File"),
                                        deffolder, NULL );
        if ( !path )
            return;
        if ( save->s )
            g_free( save->s );
        save->s = g_path_get_dirname( path );
    }
    else
    {
        // get url
        if ( !xset_text_dialog( GTK_WIDGET( main_window ), _("Enter Plugin URL"), NULL, FALSE, _("Enter SpaceFM Plugin URL:\n\n(wget will be used to download the plugin file)"), NULL, NULL, &path, NULL, FALSE, job == 0 ? "#plugins-install" : "#plugins-import" ) || !path || path[0] == '\0' )
            return;
        type = 1;  //url
    }

    if ( job == PLUGIN_JOB_INSTALL )
    {
        // install job
        char* filename = g_path_get_basename( path );
        char* ext = strstr( filename, ".spacefm-plugin" );
        if ( !ext )
            ext = strstr( filename, ".tar.gz" );
        if ( ext )
            ext[0] = '\0';
        char* plug_dir_name = plain_ascii_name( filename );
        if ( ext )
            ext[0] = '.';
        g_free( filename );
        if ( plug_dir_name[0] == '\0' )
        {
            msg = g_strdup_printf( _("This plugin's filename is invalid.  Please rename it using alpha-numeric ASCII characters and try again.") );
            xset_msg_dialog( GTK_WIDGET( main_window ), GTK_MESSAGE_ERROR,
                                        _("Invalid Plugin Filename"), NULL,
                                        0, msg, NULL, "#plugins-install" );
            {
                g_free( plug_dir_name );
                g_free( path );
                g_free( msg );
                return;
            }
        }

        if ( DATADIR )
            plug_dir = g_build_filename( DATADIR, "spacefm", "plugins",
                                                            plug_dir_name, NULL );
        else if ( !g_file_test( "/usr/share/spacefm/plugins", G_FILE_TEST_IS_DIR ) &&
                    g_file_test( "/usr/local/share/spacefm/plugins", G_FILE_TEST_IS_DIR ) )
            plug_dir = g_build_filename( "/usr/local/share/spacefm/plugins",
                                                        plug_dir_name, NULL );
        else
            plug_dir = g_build_filename( "/usr/share/spacefm/plugins",
                                                    plug_dir_name, NULL );
        
        if ( g_file_test( plug_dir, G_FILE_TEST_EXISTS ) )
        {
            msg = g_strdup_printf( _("There is already a plugin installed as '%s'.  Overwrite ?\n\nTip: You can also rename this plugin file to install it under a different name."), plug_dir_name );
            if ( xset_msg_dialog( GTK_WIDGET( main_window ), GTK_MESSAGE_WARNING,
                                        _("Overwrite Plugin ?"), NULL,
                                        GTK_BUTTONS_YES_NO,
                                        msg, NULL, "#plugins-install" ) != GTK_RESPONSE_YES )
            {
                g_free( plug_dir_name );
                g_free( plug_dir );
                g_free( path );
                g_free( msg );
                return;
            }
            g_free( msg );
        }
        g_free( plug_dir_name );
    }
    else
    {
        // copy job
        const char* user_tmp = xset_get_user_tmp_dir();
        if ( !user_tmp )
        {
            xset_msg_dialog( GTK_WIDGET( main_window ), GTK_MESSAGE_ERROR,
                                _("Error Creating Temp Directory"), NULL, 0, 
                                _("Unable to create temporary directory"), NULL,
                                NULL );
            g_free( path );
            return;
        }
        char* hex8;
        plug_dir = NULL;
        while ( !plug_dir || ( plug_dir && g_file_test( plug_dir, G_FILE_TEST_EXISTS ) ) )
        {
            hex8 = randhex8();
            if ( plug_dir )
                g_free( plug_dir );
            plug_dir = g_build_filename( user_tmp, hex8, NULL );
            g_free( hex8 );
        }
    }

    install_plugin_file( main_window, NULL, path, plug_dir, type, job, NULL );
    g_free( path );
    g_free( plug_dir );
}

GtkWidget* create_plugins_menu( FMMainWindow* main_window )
{
    GtkWidget* plug_menu;
    GtkWidget* item;
    GList* l;
    GList* plugins = NULL;
    XSet* set;
    
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( 
                    fm_main_window_get_current_file_browser( main_window ) );
    GtkAccelGroup* accel_group = gtk_accel_group_new ();
    plug_menu = gtk_menu_new();
    if ( !file_browser )
        return plug_menu;
        
    set = xset_set_cb( "plug_ifile", on_plugin_install, main_window );
        xset_set_ob1( set, "set", set );
    set = xset_set_cb( "plug_iurl", on_plugin_install, main_window );
        xset_set_ob1( set, "set", set );
    set = xset_set_cb( "plug_cfile", on_plugin_install, main_window );
        xset_set_ob1( set, "set", set );
    set = xset_set_cb( "plug_curl", on_plugin_install, main_window );
        xset_set_ob1( set, "set", set );

    set = xset_get( "plug_install" );
    xset_add_menuitem( NULL, file_browser, plug_menu, accel_group, set );
    set = xset_get( "plug_copy" );
    xset_add_menuitem( NULL, file_browser, plug_menu, accel_group, set );

    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append( GTK_MENU_SHELL( plug_menu ), item );

    set = xset_get( "plug_inc" );
    item = xset_add_menuitem( NULL, file_browser, plug_menu, accel_group, set );
    if ( item )
    {
        GtkWidget* inc_menu = gtk_menu_item_get_submenu( GTK_MENU_ITEM ( item ) );

        plugins = xset_get_plugins( TRUE );
        for ( l = plugins; l; l = l->next )
            xset_add_menuitem( NULL, file_browser, inc_menu, accel_group,
                                                                    (XSet*)l->data );
    }
    set->disable = !plugins;
    if ( plugins )
        g_list_free( plugins );

    plugins = xset_get_plugins( FALSE );
    for ( l = plugins; l; l = l->next )
        xset_add_menuitem( NULL, file_browser, plug_menu, accel_group,
                                                                (XSet*)l->data );
    if ( plugins )
        g_list_free( plugins );
        
    gtk_widget_show_all( plug_menu );
    if ( set->disable )
        gtk_widget_hide( item );  // temporary until included available
    return plug_menu;
}

/*  don't use this method because menu must be updated just after user opens it
 * due to file_browser value in xsets
void main_window_on_plugins_change( FMMainWindow* main_window )
{
    if ( main_window )
    {
        main_window->plug_menu = create_plugins_menu( main_window );
        // FIXME: We have to popupdown the menu first, if it's showed on screen.
        // Otherwise, it's rare but possible that we try to replace the menu while it's in use.
        gtk_menu_popdown( (GtkMenu*)gtk_menu_item_get_submenu ( GTK_MENU_ITEM (
                                                main_window->plug_menu_item ) ) );
        gtk_menu_item_set_submenu ( GTK_MENU_ITEM ( main_window->plug_menu_item ),
                                                        main_window->plug_menu );
    }
    else
    {
        // all windows
        FMMainWindow* a_window;
        GList* l;
        
        for ( l = all_windows; l; l = l->next )
        {
            a_window = (FMMainWindow*)l->data;
            a_window->plug_menu = create_plugins_menu( a_window );
            gtk_menu_popdown( (GtkMenu*)gtk_menu_item_get_submenu ( GTK_MENU_ITEM (
                                                    a_window->plug_menu_item ) ) );
            gtk_menu_item_set_submenu ( GTK_MENU_ITEM ( a_window->plug_menu_item ),
                                                            a_window->plug_menu );
        }
    }
}
*/

void import_all_plugins( FMMainWindow* main_window )
{
    GDir* dir;
    const char* name;
    char* plug_dir;
    char* plug_file;
    char* bookmarks_dir;
    int i;
    gboolean found = FALSE;
    gboolean found_plugins = FALSE;

    // get potential locations
    char* path;
    GList* locations = NULL;
    if ( DATADIR )
    {
        locations = g_list_append( locations, g_build_filename( DATADIR,
                                                    "spacefm", "included", NULL ) );
        locations = g_list_append( locations, g_build_filename( DATADIR,
                                                    "spacefm", "plugins", NULL ) );
    }
    const gchar* const * sdir = g_get_system_data_dirs();
    for( ; *sdir; ++sdir )
    {
        path = g_build_filename( *sdir, "spacefm", "included", NULL );
        if ( !g_list_find_custom( locations, path, (GCompareFunc)g_strcmp0 ) )
            locations = g_list_append( locations, path );
        else
            g_free( path );
        path = g_build_filename( *sdir, "spacefm", "plugins", NULL );
        if ( !g_list_find_custom( locations, path, (GCompareFunc)g_strcmp0 ) )
            locations = g_list_append( locations, path );
        else
            g_free( path );
    }
    if ( !g_list_find_custom( locations, "/usr/local/share/spacefm/included", (GCompareFunc)g_strcmp0 ) )
        locations = g_list_append( locations, g_strdup( "/usr/local/share/spacefm/included" ) );
    if ( !g_list_find_custom( locations, "/usr/share/spacefm/included", (GCompareFunc)g_strcmp0 ) )
        locations = g_list_append( locations, g_strdup( "/usr/share/spacefm/included" ) );
    if ( !g_list_find_custom( locations, "/usr/local/share/spacefm/plugins", (GCompareFunc)g_strcmp0 ) )
        locations = g_list_append( locations, g_strdup( "/usr/local/share/spacefm/plugins" ) );
    if ( !g_list_find_custom( locations, "/usr/share/spacefm/plugins", (GCompareFunc)g_strcmp0 ) )
        locations = g_list_append( locations, g_strdup( "/usr/share/spacefm/plugins" ) );

    GList* l;
    for ( l = locations; l; l = l->next )
    {
        dir = g_dir_open( (char*)l->data, 0, NULL );
        if ( dir )
        {
            while ( ( name = g_dir_read_name( dir ) ) )
            {
                bookmarks_dir = g_build_filename( (char*)l->data, name,
                                                        "main_book", NULL );
                plug_file = g_build_filename( (char*)l->data, name,
                                                        "plugin", NULL );
                if ( g_file_test( plug_file, G_FILE_TEST_EXISTS ) &&
                     !g_file_test( bookmarks_dir, G_FILE_TEST_EXISTS ) )
                {
                    plug_dir = g_build_filename( (char*)l->data, name, NULL );
                    if ( xset_import_plugin( plug_dir, NULL ) )
                    {
                        found = TRUE;
                        if ( i == 1 )
                            found_plugins = TRUE;
                    }
                    else
                        printf( "Invalid Plugin Ignored: %s/\n", plug_dir );
                    g_free( plug_dir );
                }
                g_free( plug_file );
                g_free( bookmarks_dir );
            }
            g_dir_close( dir );
        }
    }
    g_list_foreach( locations, (GFunc)g_free, NULL );
    g_list_free( locations );
    
    clean_plugin_mirrors();
}

void on_devices_show( GtkMenuItem* item, FMMainWindow* main_window )
{
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( 
                        fm_main_window_get_current_file_browser( main_window ) );
    if ( !file_browser )
        return;
    int mode = main_window->panel_context[file_browser->mypanel-1];

    xset_set_b_panel_mode( file_browser->mypanel, "show_devmon", mode,
                                                !file_browser->side_dev );
    update_views_all_windows( NULL, file_browser );
    if ( file_browser->side_dev )
        gtk_widget_grab_focus( GTK_WIDGET( file_browser->side_dev ) );
}

GtkWidget* create_devices_menu( FMMainWindow* main_window )
{
    GtkWidget* dev_menu;
    GtkWidget* item;
    XSet* set;
    
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( 
                    fm_main_window_get_current_file_browser( main_window ) );
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    dev_menu = gtk_menu_new();
    if ( !file_browser )
        return dev_menu;

    set = xset_set_cb( "main_dev", on_devices_show, main_window );
    set->b = file_browser->side_dev ? XSET_B_TRUE : XSET_B_UNSET;
    xset_add_menuitem( NULL, file_browser, dev_menu, accel_group, set );

    set = xset_get( "main_dev_sep" );
    xset_add_menuitem( NULL, file_browser, dev_menu, accel_group, set );

    ptk_location_view_dev_menu( GTK_WIDGET( file_browser ), file_browser, dev_menu );
#ifndef HAVE_HAL
    set = xset_get( "sep_dm3" );
    xset_add_menuitem( NULL, file_browser, dev_menu, accel_group, set );

    set = xset_get( "dev_menu_settings" );
    xset_add_menuitem( NULL, file_browser, dev_menu, accel_group, set );

#endif
    // show all
    gtk_widget_show_all( dev_menu );

    return dev_menu;
}

void on_open_url( GtkWidget* widget, FMMainWindow* main_window )
{
    PtkFileBrowser* file_browser = 
                    PTK_FILE_BROWSER( fm_main_window_get_current_file_browser(
                                                                main_window ) );
    char* url = xset_get_s( "main_save_session" );
    if ( file_browser && url && url[0] )
        ptk_location_view_mount_network( file_browser, url, TRUE, TRUE );
#if 0
    /* was on_save_session */
    xset_autosave_cancel();
    char* err_msg = save_settings( main_window );
    if ( err_msg )
    {
        char* msg = g_strdup_printf( _("Error: Unable to save session file\n\n( %s )"), err_msg );
        g_free( err_msg );
        xset_msg_dialog( GTK_WIDGET( main_window ), GTK_MESSAGE_ERROR, _("Save Session Error"),
                                                    NULL, 0, msg, NULL, 
                                                    "#programfiles-home-session" );
        g_free( msg );
    }
#endif
}

void on_find_file_activate ( GtkMenuItem *menuitem, gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    const char* cwd;
    const char* dirs[2];
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( 
                    fm_main_window_get_current_file_browser( main_window ) );
    cwd = ptk_file_browser_get_cwd( file_browser );

    dirs[0] = cwd;
    dirs[1] = NULL;
    fm_find_files( dirs );
}

void on_open_current_folder_as_root ( GtkMenuItem *menuitem,
                                      gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser = 
                    PTK_FILE_BROWSER( fm_main_window_get_current_file_browser(
                                                                main_window ) );
    if ( !file_browser )
        return;
    // root task
    PtkFileTask* task = ptk_file_exec_new( _("Open Root Window"),
                                    ptk_file_browser_get_cwd( file_browser ),
                                    GTK_WIDGET( file_browser ), file_browser->task_view );
    char* prog = g_find_program_in_path( g_get_prgname() );
    if ( !prog )
        prog = g_strdup( g_get_prgname() );
    if ( !prog )
        prog = g_strdup( "spacefm" );
    char* cwd = bash_quote( ptk_file_browser_get_cwd( file_browser ) );
    task->task->exec_command = g_strdup_printf( "HOME=/root %s %s", prog, cwd );
    g_free( prog );
    g_free( cwd );
    task->task->exec_as_user = g_strdup( "root" );
    task->task->exec_sync = FALSE;
    task->task->exec_export = FALSE;
    task->task->exec_browser = NULL;
    ptk_file_task_run( task );
}

void main_window_open_terminal( FMMainWindow* main_window, gboolean as_root )
{
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( 
                        fm_main_window_get_current_file_browser( main_window ) );
    if ( !file_browser )
        return;
    GtkWidget* parent = gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) );
    char* main_term = xset_get_s( "main_terminal" );
    if ( !main_term || main_term[0] == '\0' )
    {
        ptk_show_error( GTK_WINDOW( parent ),
                        _("Terminal Not Available"),
                        _( "Please set your terminal program in View|Preferences|Advanced" ) );
        fm_edit_preference( (GtkWindow*)parent, PREF_ADVANCED );
        main_term = xset_get_s( "main_terminal" );
        if ( !main_term || main_term[0] == '\0' )
            return;
    }
    
    // task
    char* terminal = g_find_program_in_path( main_term );
    if ( !terminal )
        terminal = g_strdup( main_term );
    PtkFileTask* task = ptk_file_exec_new( _("Open Terminal"),
                            ptk_file_browser_get_cwd( file_browser ),
                            GTK_WIDGET( file_browser ), file_browser->task_view );\

    task->task->exec_command = terminal;
    task->task->exec_as_user = as_root ? g_strdup( "root" ) : NULL;
    task->task->exec_sync = FALSE;
    task->task->exec_export = TRUE;
    task->task->exec_browser = file_browser;
    ptk_file_task_run( task );
}

void on_open_terminal_activate ( GtkMenuItem *menuitem,
                                 gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    main_window_open_terminal( main_window, FALSE );
}

void on_open_root_terminal_activate ( GtkMenuItem *menuitem,
                                 gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    main_window_open_terminal( main_window, TRUE );
}

void on_quit_activate( GtkMenuItem *menuitem, gpointer user_data )
{
    fm_main_window_delete_event( user_data, NULL );
    //fm_main_window_close( GTK_WIDGET( user_data ) );
}

void main_window_rubberband_all()
{
    GList* l;
    FMMainWindow* main_window;
    PtkFileBrowser* a_browser;
    int num_pages, i, p;
    GtkWidget* notebook;
    
    for ( l = all_windows; l; l = l->next )
    {
        main_window = (FMMainWindow*)l->data;
        for ( p = 1; p < 5; p++ )
        {
            notebook = main_window->panel[p-1];
            num_pages = gtk_notebook_get_n_pages( GTK_NOTEBOOK( notebook ) );
            for ( i = 0; i < num_pages; i++ )
            {
                a_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                         GTK_NOTEBOOK( notebook ), i ) );
                if ( a_browser->view_mode == PTK_FB_LIST_VIEW )
                {
                    gtk_tree_view_set_rubber_banding( (GtkTreeView*)a_browser->folder_view,
                                                        xset_get_b( "rubberband" ) );
                }
            }
        }
    }
}

void main_window_refresh_all()
{
    GList* l;
    FMMainWindow* main_window;
    PtkFileBrowser* a_browser;
    int num_pages, i, p;
    GtkWidget* notebook;
    
    for ( l = all_windows; l; l = l->next )
    {
        main_window = (FMMainWindow*)l->data;
        for ( p = 1; p < 5; p++ )
        {
            notebook = main_window->panel[p-1];
            num_pages = gtk_notebook_get_n_pages( GTK_NOTEBOOK( notebook ) );
            for ( i = 0; i < num_pages; i++ )
            {
                a_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                         GTK_NOTEBOOK( notebook ), i ) );
                ptk_file_browser_refresh( NULL, a_browser );
            }
        }
    }
}

void main_window_root_bar_all()
{
    if ( geteuid() != 0 )
        return;
    GList* l;
    FMMainWindow* a_window;
    GdkColor color;
    //GdkColor color_white;

    if ( xset_get_b( "root_bar" ) )
    {
        color.pixel = 0;
        color.red = 30000;
        color.blue = 1000;
        color.green = 0;

        //gdk_color_parse( "#ffffff", &color_white );

        for ( l = all_windows; l; l = l->next )
        {
            a_window = (FMMainWindow*)l->data;
            gtk_widget_modify_bg( GTK_WIDGET( a_window ), GTK_STATE_NORMAL, &color );
            gtk_widget_modify_bg( GTK_WIDGET( a_window->menu_bar ),
                                                        GTK_STATE_NORMAL, &color );
            gtk_widget_modify_bg( GTK_WIDGET( a_window->panelbar ),
                                                        GTK_STATE_NORMAL, &color );
            // how to change menu bar text color?
            //gtk_widget_modify_fg( GTK_MENU_ITEM( a_window->file_menu_item ), GTK_STATE_NORMAL, &color_white );
        }
    }
    else
    {
        for ( l = all_windows; l; l = l->next )
        {
            a_window = (FMMainWindow*)l->data;
            gtk_widget_modify_bg( GTK_WIDGET( a_window ), GTK_STATE_NORMAL, NULL );
            gtk_widget_modify_bg( GTK_WIDGET( a_window->menu_bar ),
                                                        GTK_STATE_NORMAL, NULL );
            gtk_widget_modify_bg( GTK_WIDGET( a_window->panelbar ),
                                                        GTK_STATE_NORMAL, NULL );
            //gtk_widget_modify_fg( a_window->menu_bar, GTK_STATE_NORMAL, NULL );
        }        
    }
}

void main_update_fonts( GtkWidget* widget, PtkFileBrowser* file_browser )
{
    PtkFileBrowser* a_browser;
    int i;
    char* fontname;
    PangoFontDescription* font_desc;
    GList* l;
    FMMainWindow* main_window;
    int num_pages;
    GtkWidget* label;
    GtkContainer* hbox;
    GtkLabel* text;
    GList* children;
    GtkImage* icon;
    GtkWidget* tab_label;
    XSet* set;
    char* icon_name;
    
    int p = file_browser->mypanel;
    // all windows/panel p/all browsers
    for ( l = all_windows; l; l = l->next )
    {
        main_window = (FMMainWindow*)l->data;
        set = xset_get_panel( p, "icon_status" );
        if ( set->icon && set->icon[0] != '\0' )
            icon_name = set->icon;
        else
            icon_name = "gtk-yes";
        num_pages = gtk_notebook_get_n_pages( GTK_NOTEBOOK( main_window->panel[p-1] ) );
        for ( i = 0; i < num_pages; i++ )
        {
            a_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page( 
                                GTK_NOTEBOOK( main_window->panel[p-1] ), i ) );
            // file list / tree
            fontname = xset_get_s_panel( p, "font_file" );
            if ( fontname )
            {
                font_desc = pango_font_description_from_string( fontname );
                gtk_widget_modify_font( GTK_WIDGET( a_browser->folder_view ),
                                                                    font_desc );
                
                if ( a_browser->view_mode != PTK_FB_LIST_VIEW )
                {
                    // force rebuild of folder_view for font change in exo_icon_view
                    gtk_widget_destroy( a_browser->folder_view );
                    a_browser->folder_view = NULL;
                    ptk_file_browser_update_views( NULL, a_browser );
                }
                
                if ( a_browser->side_dir )
                    gtk_widget_modify_font( GTK_WIDGET( a_browser->side_dir ),
                                                                    font_desc );
                pango_font_description_free( font_desc );
            }
            else
            {
                gtk_widget_modify_font( GTK_WIDGET( a_browser->folder_view ), NULL );        
                if ( a_browser->side_dir )
                    gtk_widget_modify_font( GTK_WIDGET( a_browser->side_dir ), NULL );        
            }
            // devices
            if ( a_browser->side_dev )
            {
                fontname = xset_get_s_panel( p, "font_dev" );
                if ( fontname )
                {
                    font_desc = pango_font_description_from_string( fontname );
                    gtk_widget_modify_font( GTK_WIDGET( a_browser->side_dev ),
                                                                    font_desc );
                    pango_font_description_free( font_desc );
                }
                else
                    gtk_widget_modify_font( GTK_WIDGET( a_browser->side_dev ), NULL );        
            }
            // bookmarks
            if ( a_browser->side_book )
            {
                fontname = xset_get_s_panel( p, "font_book" );
                if ( fontname )
                {
                    font_desc = pango_font_description_from_string( fontname );
                    gtk_widget_modify_font( GTK_WIDGET( a_browser->side_book ),
                                                                    font_desc );
                    pango_font_description_free( font_desc );
                }
                else
                    gtk_widget_modify_font( GTK_WIDGET( a_browser->side_book ),
                                                                        NULL );
            }
            // pathbar
            if ( a_browser->path_bar )
            {
                fontname = xset_get_s_panel( p, "font_path" );
                if ( fontname )
                {
                    font_desc = pango_font_description_from_string( fontname );
                    gtk_widget_modify_font( GTK_WIDGET( a_browser->path_bar ),
                                                                    font_desc );
                    pango_font_description_free( font_desc );
                }
                else
                    gtk_widget_modify_font( GTK_WIDGET( a_browser->path_bar ),
                                                                        NULL );
            }
            
            // status bar font and icon
            if ( a_browser->status_label )
            {
                fontname = xset_get_s_panel( p, "font_status" );
                if ( fontname )
                {
                    font_desc = pango_font_description_from_string( fontname );
                    gtk_widget_modify_font( GTK_WIDGET( a_browser->status_label ),
                                                                    font_desc );
                    pango_font_description_free( font_desc );
                }
                else
                    gtk_widget_modify_font( GTK_WIDGET( a_browser->status_label ),
                                                                        NULL );

                gtk_image_set_from_icon_name( GTK_IMAGE( a_browser->status_image ),
                                                icon_name, GTK_ICON_SIZE_MENU );
            }
            
            // tabs
            // need to recreate to change icon
            tab_label = fm_main_window_create_tab_label( main_window,
                                                        a_browser );
            gtk_notebook_set_tab_label( GTK_NOTEBOOK( main_window->panel[p-1] ),
                                        GTK_WIDGET(a_browser), tab_label );
            // not needed?
            //fm_main_window_update_tab_label( main_window, a_browser,
            //                            a_browser->dir->disp_path );
        }
        // tasks
        fontname = xset_get_s( "font_task" );
        if ( fontname )
        {
            font_desc = pango_font_description_from_string( fontname );
            gtk_widget_modify_font( GTK_WIDGET( main_window->task_view ), font_desc );
            pango_font_description_free( font_desc );
        }
        else
            gtk_widget_modify_font( GTK_WIDGET( main_window->task_view ), NULL );
            
        // panelbar
        gtk_image_set_from_icon_name( GTK_IMAGE( main_window->panel_image[p-1] ),
                                            icon_name, GTK_ICON_SIZE_MENU );
    }
}

void update_window_icon( GtkWindow* window, GtkIconTheme* theme )
{
    GdkPixbuf* icon;
    char* name;
    GError *error = NULL;

    XSet* set = xset_get( "main_icon" );
    if ( set->icon )
        name = set->icon;
    else if ( geteuid() == 0 )
        name = "spacefm-root";
    else
        name = "spacefm";
    
    icon = gtk_icon_theme_load_icon( theme, name, 48, 0, &error );
    if ( icon )
    {
        gtk_window_set_icon( window, icon );
        g_object_unref( icon );
    }
    else if ( error != NULL )
    {
        // An error occured on loading the icon
        fprintf( stderr, "spacefm: Unable to load the window icon "
        "'%s' in - update_window_icon - %s\n", name, error->message);
        g_error_free( error );
    }
}

static void update_window_icons( GtkIconTheme* theme, GtkWindow* window )
{
    g_list_foreach( all_windows, (GFunc)update_window_icon, theme );
}

void on_main_icon()
{
    GList* l;
    FMMainWindow* a_window;
    for ( l = all_windows; l; l = l->next )
    {
        a_window = (FMMainWindow*)l->data;
        update_window_icon( GTK_WINDOW( a_window ), gtk_icon_theme_get_default() );
    }
}

void main_design_mode( GtkMenuItem *menuitem, FMMainWindow* main_window )
{
    xset_msg_dialog( GTK_WIDGET( main_window ), 0, _("Design Mode Help"), NULL, 0, _("Design Mode allows you to change the name, shortcut key and icon of menu, toolbar and bookmark items, show help for an item, and add your own custom commands and applications.\n\nTo open the Design Menu, simply right-click on a menu item, bookmark, or toolbar item.  To open the Design Menu for a submenu, first close the submenu (by clicking on it).\n\nFor more information, click the Help button below."), NULL, "#designmode" );
}

void main_window_bookmark_changed( const char* changed_set_name )
{
    GList* l;
    FMMainWindow* main_window;
    PtkFileBrowser* a_browser;
    int num_pages, i, p;
    GtkWidget* notebook;
    
    for ( l = all_windows; l; l = l->next )
    {
        main_window = (FMMainWindow*)l->data;
        for ( p = 1; p < 5; p++ )
        {
            notebook = main_window->panel[p-1];
            num_pages = gtk_notebook_get_n_pages( GTK_NOTEBOOK( notebook ) );
            for ( i = 0; i < num_pages; i++ )
            {
                a_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                         GTK_NOTEBOOK( notebook ), i ) );
                if ( a_browser->side_book )
                    ptk_bookmark_view_xset_changed(
                                        GTK_TREE_VIEW( a_browser->side_book ),
                                        a_browser, changed_set_name );
            }
        }
    }
}

void main_window_refresh_all_tabs_matching( const char* path )
{
    // This function actually closes the tabs because refresh doesn't work.
    // dir objects have multiple refs and unreffing them all wouldn't finalize
    // the dir object for unknown reason.
    
    // This breaks auto open of tabs on automount
    return;
#if 0
    GList* l;
    FMMainWindow* a_window;
    PtkFileBrowser* a_browser;
    GtkWidget* notebook;
    int cur_tabx, p;
    int pages;
    char* cwd_canon;
    
//printf("main_window_refresh_all_tabs_matching %s\n", path );
    // canonicalize path
    char buf[ PATH_MAX + 1 ];
    char* canon = g_strdup( realpath( path, buf ) );
    if ( !canon )
        canon = g_strdup( path );

    if ( !g_file_test( canon, G_FILE_TEST_IS_DIR ) )
    {
        g_free( canon );
        return;
    }

    // do all windows all panels all tabs
    for ( l = all_windows; l; l = l->next )
    {
        a_window = (FMMainWindow*)l->data;
        for ( p = 1; p < 5; p++ )
        {
            notebook = a_window->panel[p-1];
            pages = gtk_notebook_get_n_pages( GTK_NOTEBOOK( notebook ) );
            for ( cur_tabx = 0; cur_tabx < pages; cur_tabx++ )
            {
                a_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page( 
                                            GTK_NOTEBOOK( notebook ),
                                                                cur_tabx ) );
                cwd_canon = realpath( ptk_file_browser_get_cwd( a_browser ),
                                                                    buf );
                if ( !g_strcmp0( canon, cwd_canon ) && g_file_test( canon, G_FILE_TEST_IS_DIR ) )
                {
                    on_close_notebook_page( NULL, a_browser );
                    pages = gtk_notebook_get_n_pages( GTK_NOTEBOOK( notebook ) );
                    cur_tabx--;
                    //ptk_file_browser_refresh( NULL, a_browser );
                }
            }
        }
    }
    g_free( canon );
#endif
}

void main_window_rebuild_all_toolbars( PtkFileBrowser* file_browser )
{
    GList* l;
    FMMainWindow* a_window;
    PtkFileBrowser* a_browser;
    GtkWidget* notebook;
    int cur_tabx, p;
    int pages;
//printf("main_window_rebuild_all_toolbars\n");

    // do this browser first
    if ( file_browser )
        ptk_file_browser_rebuild_toolbars( file_browser );
    
    // do all windows all panels all tabs
    for ( l = all_windows; l; l = l->next )
    {
        a_window = (FMMainWindow*)l->data;
        for ( p = 1; p < 5; p++ )
        {
            notebook = a_window->panel[p-1];
            pages = gtk_notebook_get_n_pages( GTK_NOTEBOOK( notebook ) );
            for ( cur_tabx = 0; cur_tabx < pages; cur_tabx++ )
            {
                a_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page( 
                                            GTK_NOTEBOOK( notebook ),
                                            cur_tabx ) );
                if ( a_browser != file_browser )
                    ptk_file_browser_rebuild_toolbars( a_browser );
            }
        }
    }
    xset_autosave( FALSE, FALSE );
}

void main_window_update_all_bookmark_views()
{
    GList* l;
    FMMainWindow* a_window;
    PtkFileBrowser* a_browser;
    GtkWidget* notebook;
    int cur_tabx, p;
    int pages;

    // do all windows all panels all tabs
    for ( l = all_windows; l; l = l->next )
    {
        a_window = (FMMainWindow*)l->data;
        for ( p = 1; p < 5; p++ )
        {
            notebook = a_window->panel[p-1];
            pages = gtk_notebook_get_n_pages( GTK_NOTEBOOK( notebook ) );
            for ( cur_tabx = 0; cur_tabx < pages; cur_tabx++ )
            {
                a_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page( 
                                            GTK_NOTEBOOK( notebook ),
                                                                cur_tabx ) );
                if ( a_browser->side_book )
                    ptk_bookmark_view_update_icons( NULL, a_browser );
            }
        }
    }
    main_window_rebuild_all_toolbars( NULL ); // toolbar uses bookmark icon
}

void update_views_all_windows( GtkWidget* item, PtkFileBrowser* file_browser )
{
    GList* l;
    FMMainWindow* a_window;
    PtkFileBrowser* a_browser;
    GtkWidget* notebook;
    int cur_tabx, p;
    
//printf("update_views_all_windows\n");
    // do this browser first
    if ( !file_browser )
        return;
    p = file_browser->mypanel;

    ptk_file_browser_update_views( NULL, file_browser );

    // do other windows
    for ( l = all_windows; l; l = l->next )
    {
        a_window = (FMMainWindow*)l->data;
        if ( gtk_widget_get_visible( a_window->panel[p-1] ) )
        {
            notebook = a_window->panel[p-1];
            cur_tabx = gtk_notebook_get_current_page( GTK_NOTEBOOK( notebook ) );
            if ( cur_tabx != -1 )
            {
                a_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page( 
                                            GTK_NOTEBOOK( notebook ), cur_tabx ) );
                if ( a_browser != file_browser )
                    ptk_file_browser_update_views( NULL, a_browser );
            }
        }
    }
    xset_autosave( FALSE, FALSE );
}

void main_window_toggle_thumbnails_all_windows()
{
    int p, i, n;
    GtkNotebook* notebook;
    GList* l;
    PtkFileBrowser* file_browser;
    FMMainWindow* a_window;

    // toggle
    app_settings.show_thumbnail = !app_settings.show_thumbnail;

    // update all windows/all panels/all browsers
    for ( l = all_windows; l; l = l->next )
    {
        a_window = FM_MAIN_WINDOW( l->data );
        for ( p = 1; p < 5; p++ )
        {
            notebook = GTK_NOTEBOOK( a_window->panel[p-1] );
            n = gtk_notebook_get_n_pages( notebook );
            for ( i = 0; i < n; ++i )
            {
                file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                                 notebook, i ) );
                ptk_file_browser_show_thumbnails( file_browser,
                              app_settings.show_thumbnail ? 
                              app_settings.max_thumb_size : 0 );
            }
        }
    }

    fm_desktop_update_thumbnails();

    /* Ensuring free space at the end of the heap is freed to the OS,
     * mainly to deal with the possibility thousands of large thumbnails
     * have been freed but the memory not actually released by SpaceFM */
#if defined (__GLIBC__)
    malloc_trim(0);
#endif
}

void focus_panel( GtkMenuItem* item, gpointer mw, int p )
{
    int panel, hidepanel;
    int panel_num;
    
    FMMainWindow* main_window = (FMMainWindow*)mw;
    
    if ( item )
        panel_num = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( item ),
                                                                    "panel_num" ) );
    else
        panel_num = p;

    if ( panel_num == -1 )  // prev
    {
        panel = main_window->curpanel - 1;
        do
        {
            if ( panel < 1 )
                panel = 4;
            if ( xset_get_b_panel( panel, "show" ) )
                break;
            panel--;
        } while ( panel != main_window->curpanel - 1 );
    }
    else if ( panel_num == -2 )  // next
    {
        panel = main_window->curpanel + 1;
        do
        {
            if ( panel > 4 )
                panel = 1;
            if ( xset_get_b_panel( panel, "show" ) )
                break;
            panel++;
        } while ( panel != main_window->curpanel + 1 );
    }
    else if ( panel_num == -3 )  // hide
    {
        hidepanel = main_window->curpanel;
        panel = main_window->curpanel + 1;
        do
        {
            if ( panel > 4 )
                panel = 1;
            if ( xset_get_b_panel( panel, "show" ) )
                break;
            panel++;
        } while ( panel != hidepanel );
        if ( panel == hidepanel )
            panel = 0;
    }
    else
        panel = panel_num;
        
    if ( panel > 0 && panel < 5 )
    {
        if ( gtk_widget_get_visible( main_window->panel[panel-1] ) )
        {
            gtk_widget_grab_focus( GTK_WIDGET( main_window->panel[panel-1] ) );
            main_window->curpanel = panel;
            main_window->notebook = main_window->panel[panel-1];
            PtkFileBrowser* file_browser = PTK_FILE_BROWSER( 
                        fm_main_window_get_current_file_browser( main_window ) );
            if ( file_browser )
            {
                gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );
                set_panel_focus( main_window, file_browser );
            }
        }
        else if ( panel_num != -3 )
        {
            xset_set_b_panel( panel, "show", TRUE );
            show_panels_all_windows( NULL, main_window );
            gtk_widget_grab_focus( GTK_WIDGET( main_window->panel[panel-1] ) );
            main_window->curpanel = panel;
            main_window->notebook = main_window->panel[panel-1];
            PtkFileBrowser* file_browser = PTK_FILE_BROWSER( 
                        fm_main_window_get_current_file_browser( main_window ) );
            if ( file_browser )
            {
                gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );
                set_panel_focus( main_window, file_browser );
            }
        }
        if ( panel_num == -3 )
        {
            xset_set_b_panel( hidepanel, "show", FALSE );
            show_panels_all_windows( NULL, main_window );
        }
    }
}

void show_panels_all_windows( GtkMenuItem* item, FMMainWindow* main_window )
{
    GList* l;
    FMMainWindow* a_window;

    // do this window first
    main_window->panel_change = TRUE;
    show_panels( NULL, main_window );

    // do other windows
    main_window->panel_change = FALSE;  // don't save columns for other windows
    for ( l = all_windows; l; l = l->next )
    {
        a_window = (FMMainWindow*)l->data;
        if ( main_window != a_window )
            show_panels( NULL, a_window );
    }
    
    xset_autosave( FALSE, FALSE );
}

void show_panels( GtkMenuItem* item, FMMainWindow* main_window )
{
    int p, cur_tabx, i;
    const char* folder_path;
    XSet* set;
    char* tabs;
    char* end;
    char* tab_dir;
    char* tabs_add;
    gboolean show[5];  //array starts at 1 for clarity
    gboolean tab_added;
    gboolean horiz, vert;
    PtkFileBrowser* file_browser;

    // save column widths and side sliders of visible panels
    if ( main_window->panel_change )
    {
        for ( p = 1 ; p < 5; p++ )
        {
            if ( gtk_widget_get_visible( GTK_WIDGET( main_window->panel[p-1] ) ) )
            {
                cur_tabx = gtk_notebook_get_current_page( GTK_NOTEBOOK(
                                                    main_window->panel[p-1] ) );
                if ( cur_tabx != -1 )
                {
                    file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                            GTK_NOTEBOOK( main_window->panel[p-1] ),
                                            cur_tabx ) );
                    if ( file_browser )
                    {
                        if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
                            ptk_file_browser_save_column_widths(
                                    GTK_TREE_VIEW( file_browser->folder_view ),
                                    file_browser );
                        //ptk_file_browser_slider_release( NULL, NULL, file_browser );
                    }
                }
            }
        }
    }
    
    // show panelbar
    if ( !!gtk_widget_get_visible( main_window->panelbar ) != 
                !!( !main_window->fullscreen && xset_get_b( "main_pbar" ) ) )
    {
        if ( xset_get_b( "main_pbar" ) )
            gtk_widget_show( GTK_WIDGET( main_window->panelbar ) );
        else
            gtk_widget_hide( GTK_WIDGET( main_window->panelbar ) );
    }

    // all panels hidden?
    for ( p = 1 ; p < 5; p++ )
    {
        if ( xset_get_b_panel( p, "show" ) )
            break;
    }
    if ( p == 5 )
        xset_set_b_panel( 1, "show", TRUE );

    for ( p = 1 ; p < 5; p++ )
        show[p] = xset_get_b_panel( p, "show" );
    
    for ( p = 1 ; p < 5; p++ )
    {
        // panel context - how panels share horiz and vert space with other panels
        if ( p == 1 )
        {
            horiz = show[2];
            vert = show[3] || show[4];
        }
        else if ( p == 2 )
        {
            horiz = show[1];
            vert = show[3] || show[4];
        }
        else if ( p == 3 )
        {
            horiz = show[4];
            vert = show[1] || show[2];
        }
        else
        {
            horiz = show[3];
            vert = show[1] || show[2];
        }
        if ( horiz && vert )
            main_window->panel_context[p-1] = PANEL_BOTH;
        else if ( horiz )
            main_window->panel_context[p-1] = PANEL_HORIZ;
        else if ( vert )
            main_window->panel_context[p-1] = PANEL_VERT;
        else
            main_window->panel_context[p-1] = PANEL_NEITHER;
        
        if ( show[p] )
        {
            // shown
            // test if panel and mode exists
            char* str = g_strdup_printf( "panel%d_slider_positions%d", p,
                                                main_window->panel_context[p-1] );
            if ( !( set = xset_is( str ) ) )
            {
                // no config exists for this panel and mode - copy
                //printf ("no config for %d, %d\n", p, main_window->panel_context[p-1] );
                XSet* set_old;
                int mode = main_window->panel_context[p-1];
                xset_set_b_panel_mode( p, "show_toolbox", mode,
                                        xset_get_b_panel( p, "show_toolbox" ) );
                xset_set_b_panel_mode( p, "show_devmon", mode,
                                        xset_get_b_panel( p, "show_devmon" ) );
                if ( xset_is( "show_dirtree" ) )
                    xset_set_b_panel_mode( p, "show_dirtree", mode,
                                        xset_get_b_panel( p, "show_dirtree" ) );
                xset_set_b_panel_mode( p, "show_book", mode,
                                        xset_get_b_panel( p, "show_book" ) );
                xset_set_b_panel_mode( p, "show_sidebar", mode,
                                        xset_get_b_panel( p, "show_sidebar" ) );
                xset_set_b_panel_mode( p, "detcol_name", mode,
                                        xset_get_b_panel( p, "detcol_name" ) );
                xset_set_b_panel_mode( p, "detcol_size", mode,
                                        xset_get_b_panel( p, "detcol_size" ) );
                xset_set_b_panel_mode( p, "detcol_type", mode,
                                        xset_get_b_panel( p, "detcol_type" ) );
                xset_set_b_panel_mode( p, "detcol_perm", mode,
                                        xset_get_b_panel( p, "detcol_perm" ) );
                xset_set_b_panel_mode( p, "detcol_owner", mode,
                                        xset_get_b_panel( p, "detcol_owner" ) );
                xset_set_b_panel_mode( p, "detcol_date", mode,
                                        xset_get_b_panel( p, "detcol_date" ) );
                set_old = xset_get_panel( p, "slider_positions" );
                set = xset_get_panel_mode( p, "slider_positions", mode );
                set->x = g_strdup( set_old->x ? set_old->x : "0" );
                set->y = g_strdup( set_old->y ? set_old->y : "0" );
                set->s = g_strdup( set_old->s ? set_old->s : "0" );
            }
            g_free( str );
            // load dynamic slider positions for this panel context
            main_window->panel_slide_x[p-1] = set->x ? atoi( set->x ) : 0;
            main_window->panel_slide_y[p-1] = set->y ? atoi( set->y ) : 0;
            main_window->panel_slide_s[p-1] = set->s ? atoi( set->s ) : 0;
//printf( "loaded panel %d: %d, %d, %d\n", p, 
            if ( !gtk_notebook_get_n_pages( GTK_NOTEBOOK( main_window->panel[p-1] ) ) )
            {
                main_window->notebook = main_window->panel[p-1];
                main_window->curpanel = p;
                // load saved tabs
                tab_added = FALSE;
                set = xset_get_panel( p, "show" );
                if ( ( set->s && app_settings.load_saved_tabs ) || set->ob1 )
                {
                    // set->ob1 is preload path
                    tabs_add = g_strdup_printf( "%s%s%s", 
                            set->s && app_settings.load_saved_tabs ? set->s : "",
                                                        set->ob1 ? "///" : "",
                                                        set->ob1 ? set->ob1 : "" );
                    tabs = tabs_add;
                    while ( tabs && !strncmp( tabs, "///", 3 ) )
                    {
                        tabs += 3;
                        if ( !strncmp( tabs, "////", 4 ) )
                        {
                            tab_dir = g_strdup( "/" );
                            tabs++;
                        }
                        else if ( end = strstr( tabs, "///" ) )
                        {
                            end[0] = '\0';
                            tab_dir = g_strdup( tabs );
                            end[0] = '/';
                            tabs = end;
                        }
                        else
                        {
                            tab_dir = g_strdup( tabs );
                            tabs = NULL;
                        }
                        if ( tab_dir[0] != '\0' )
                        {
                            // open saved tab
                            if ( g_str_has_prefix( tab_dir, "~/" ) )
                            {
                                // convert ~ to /home/user for hacked session files
                                str = g_strdup_printf( "%s%s", g_get_home_dir(),
                                                                tab_dir + 1 );
                                g_free( tab_dir );
                                tab_dir = str;
                            }
                            if ( g_file_test( tab_dir, G_FILE_TEST_IS_DIR ) )
                                folder_path = tab_dir;
                            else if ( !( folder_path = xset_get_s( "go_set_default" ) ) )
                            {
                                if ( geteuid() != 0 )
                                    folder_path = g_get_home_dir();
                                else
                                    folder_path = "/";
                            }
                            fm_main_window_add_new_tab( main_window, folder_path );
                            tab_added = TRUE;
                        }
                        g_free( tab_dir );
                    }
                    if ( set->x && !set->ob1 )
                    {
                        // set current tab
                        cur_tabx = atoi( set->x );
                        if ( cur_tabx >= 0 && cur_tabx < gtk_notebook_get_n_pages(
                                            GTK_NOTEBOOK( main_window->panel[p-1] ) ) )
                        {
                            gtk_notebook_set_current_page( GTK_NOTEBOOK( 
                                            main_window->panel[p-1] ), cur_tabx );
                            file_browser = PTK_FILE_BROWSER( 
                                            gtk_notebook_get_nth_page( 
                                            GTK_NOTEBOOK( main_window->panel[p-1] ),
                                            cur_tabx ) );
                            //if ( file_browser->folder_view )
                            //      gtk_widget_grab_focus( file_browser->folder_view );
//printf("call delayed (showpanels) #%d %#x window=%#x\n", cur_tabx, file_browser->folder_view, main_window);
                            g_idle_add( ( GSourceFunc ) delayed_focus, file_browser->folder_view );
                        }
                    }
                    g_free( set->ob1 );
                    set->ob1 = NULL;
                    g_free( tabs_add );
                }
                if ( !tab_added )
                {
                    // open default tab
                    if ( !( folder_path = xset_get_s( "go_set_default" ) ) )
                    {
                        if ( geteuid() != 0 )
                            folder_path = g_get_home_dir();
                        else
                            folder_path = "/";
                    }
                    fm_main_window_add_new_tab( main_window, folder_path );
                }
            }
            if ( ( evt_pnl_show->s || evt_pnl_show->ob2_data ) && 
                    !gtk_widget_get_visible( GTK_WIDGET( main_window->panel[p-1] ) ) )
                main_window_event( main_window, evt_pnl_show, "evt_pnl_show", p,
                                                    0, NULL, 0, 0, 0, TRUE );
            gtk_widget_show( GTK_WIDGET( main_window->panel[p-1] ) );
            if ( !gtk_toggle_tool_button_get_active( 
                                GTK_TOGGLE_TOOL_BUTTON( main_window->panel_btn[p-1] ) ) )
            {
                g_signal_handlers_block_matched( main_window->panel_btn[p-1],
                                                    G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                                    on_toggle_panelbar, NULL );
                gtk_toggle_tool_button_set_active( 
                                GTK_TOGGLE_TOOL_BUTTON( main_window->panel_btn[p-1] ), TRUE );
                g_signal_handlers_unblock_matched( main_window->panel_btn[p-1],
                                                    G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                                    on_toggle_panelbar, NULL );
            }
        }
        else
        {
            // not shown
            if ( ( evt_pnl_show->s || evt_pnl_show->ob2_data ) && 
                    gtk_widget_get_visible( GTK_WIDGET( main_window->panel[p-1] ) ) )
                main_window_event( main_window, evt_pnl_show, "evt_pnl_show", p,
                                                    0, NULL, 0, 0, 0, FALSE );
            gtk_widget_hide( GTK_WIDGET( main_window->panel[p-1] ) );
            if ( gtk_toggle_tool_button_get_active( 
                                GTK_TOGGLE_TOOL_BUTTON( main_window->panel_btn[p-1] ) ) )
            {
                g_signal_handlers_block_matched( main_window->panel_btn[p-1],
                                                    G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                                    on_toggle_panelbar, NULL );
                gtk_toggle_tool_button_set_active( 
                                GTK_TOGGLE_TOOL_BUTTON( main_window->panel_btn[p-1] ), FALSE );
                g_signal_handlers_unblock_matched( main_window->panel_btn[p-1],
                                                    G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                                    on_toggle_panelbar, NULL );
            }
        }
    }
    if ( show[1] || show[2] )
        gtk_widget_show( GTK_WIDGET( main_window->hpane_top ) );
    else
        gtk_widget_hide( GTK_WIDGET( main_window->hpane_top ) );
    if ( show[3] || show[4] )
        gtk_widget_show( GTK_WIDGET( main_window->hpane_bottom ) );
    else
        gtk_widget_hide( GTK_WIDGET( main_window->hpane_bottom ) );

    // current panel hidden?
    if ( !xset_get_b_panel( main_window->curpanel, "show" ) )
    {
        p = main_window->curpanel + 1;
        if ( p > 4 )
            p = 1;
        while ( p != main_window->curpanel )
        {
            if ( xset_get_b_panel( p, "show" ) )
            {
                main_window->curpanel = p;
                main_window->notebook = main_window->panel[p-1];
                cur_tabx = gtk_notebook_get_current_page( GTK_NOTEBOOK( 
                                                        main_window->notebook ) );
                file_browser = (PtkFileBrowser*)
                                gtk_notebook_get_nth_page( GTK_NOTEBOOK( 
                                main_window->notebook ),
                                cur_tabx );
                gtk_widget_grab_focus( file_browser->folder_view );
                break;
            }
            p++;
            if ( p > 4 )
                p = 1;
        }
    }
    set_panel_focus( main_window, NULL );

    // update views all panels
    for ( p = 1 ; p < 5; p++ )
    {
        if ( show[p] )
        {
            cur_tabx = gtk_notebook_get_current_page( GTK_NOTEBOOK(
                                                main_window->panel[p-1] ) );
            if ( cur_tabx != -1 )
            {
                file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                        GTK_NOTEBOOK( main_window->panel[p-1] ),
                                        cur_tabx ) );
                if ( file_browser )
                    ptk_file_browser_update_views( NULL, file_browser );
            }
        }
    }
}

void on_toggle_panelbar( GtkWidget* widget, FMMainWindow* main_window )
{
//printf("on_toggle_panelbar\n" );
    int i;
    for ( i = 0; i < 4; i++ )
    {
        if ( widget == main_window->panel_btn[i] )
        {
            xset_set_b_panel( i+1, "show",
                    gtk_toggle_tool_button_get_active( GTK_TOGGLE_TOOL_BUTTON( widget ) ) );
            break;
        }
    }
    show_panels_all_windows( NULL, main_window );
}

static gboolean on_menu_bar_event( GtkWidget* widget, GdkEvent* event,
                                                    FMMainWindow* main_window )
{
    rebuild_menus( main_window );
    return FALSE;
}

void on_bookmarks_show( GtkMenuItem* item, FMMainWindow* main_window )
{
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( 
                        fm_main_window_get_current_file_browser( main_window ) );
    if ( !file_browser )
        return;
    int mode = main_window->panel_context[file_browser->mypanel-1];

    xset_set_b_panel_mode( file_browser->mypanel, "show_book", mode,
                                                !file_browser->side_book );
    update_views_all_windows( NULL, file_browser );
    if ( file_browser->side_book )
    {
        ptk_bookmark_view_chdir( GTK_TREE_VIEW( file_browser->side_book ),
                                                    file_browser, TRUE );
        gtk_widget_grab_focus( GTK_WIDGET( file_browser->side_book ) );
    }
}

void rebuild_menus( FMMainWindow* main_window )
{
    GtkWidget* newmenu;
    GtkWidget* submenu;
    char* menu_elements;
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    XSet* set;
    XSet* child_set;
    char* str;
    
//printf("rebuild_menus\n");
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( 
                        fm_main_window_get_current_file_browser( main_window ) );
    if ( !file_browser )
        return;
    XSetContext* context = xset_context_new();
    main_context_fill( file_browser, context );

    // File
    newmenu = gtk_menu_new();
    xset_set_cb( "main_new_window", on_new_window_activate, main_window );
    xset_set_cb( "main_root_window", on_open_current_folder_as_root, main_window );
    xset_set_cb( "main_search", on_find_file_activate, main_window );
    xset_set_cb( "main_terminal", on_open_terminal_activate, main_window );
    xset_set_cb( "main_root_terminal", on_open_root_terminal_activate, main_window );
    xset_set_cb( "main_save_session", on_open_url, main_window );
    xset_set_cb( "main_exit", on_quit_activate, main_window );
    menu_elements = g_strdup_printf( "main_save_session main_search sep_f1 main_terminal main_root_terminal main_new_window main_root_window sep_f2 main_save_tabs sep_f3 main_exit" );
    xset_add_menu( NULL, file_browser, newmenu, accel_group, menu_elements );
    g_free( menu_elements );
    gtk_widget_show_all( GTK_WIDGET(newmenu) );
    g_signal_connect( newmenu, "key-press-event",
                      G_CALLBACK( xset_menu_keypress ), NULL );
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( main_window->file_menu_item ), newmenu );
    
    // View
    newmenu = gtk_menu_new();
    xset_set_cb( "main_prefs", on_preference_activate, main_window );
    xset_set_cb( "font_task", main_update_fonts, file_browser );
    xset_set_cb( "main_full", on_fullscreen_activate, main_window );
    xset_set_cb( "main_design_mode", main_design_mode, main_window );
    xset_set_cb( "main_icon", on_main_icon, NULL );
    xset_set_cb( "main_title", update_window_title, main_window );
    
    int p;
    int vis_count = 0;
    for ( p = 1 ; p < 5; p++ )
    {
        if ( xset_get_b_panel( p, "show" ) )
            vis_count++;
    }
    if ( !vis_count )
    {
        xset_set_b_panel( 1, "show", TRUE );
        vis_count++;
    }
    set = xset_set_cb( "panel1_show", show_panels_all_windows, main_window );
        set->disable = ( main_window->curpanel == 1 && vis_count == 1 );
    set = xset_set_cb( "panel2_show", show_panels_all_windows, main_window );
        set->disable = ( main_window->curpanel == 2 && vis_count == 1 );
    set = xset_set_cb( "panel3_show", show_panels_all_windows, main_window );
        set->disable = ( main_window->curpanel == 3 && vis_count == 1 );
    set = xset_set_cb( "panel4_show", show_panels_all_windows, main_window );
        set->disable = ( main_window->curpanel == 4 && vis_count == 1 );
    
    xset_set_cb( "main_pbar", show_panels_all_windows, main_window );
    
    set = xset_set_cb( "panel_prev", focus_panel, main_window );
        xset_set_ob1_int( set, "panel_num", -1 );
        set->disable = ( vis_count == 1 );
    set = xset_set_cb( "panel_next", focus_panel, main_window );
        xset_set_ob1_int( set, "panel_num", -2 );
        set->disable = ( vis_count == 1 );
    set = xset_set_cb( "panel_hide", focus_panel, main_window );
        xset_set_ob1_int( set, "panel_num", -3 );
        set->disable = ( vis_count == 1 );
    set = xset_set_cb( "panel_1", focus_panel, main_window );
        xset_set_ob1_int( set, "panel_num", 1 );
        set->disable = ( main_window->curpanel == 1 );
    set = xset_set_cb( "panel_2", focus_panel, main_window );
        xset_set_ob1_int( set, "panel_num", 2 );
        set->disable = ( main_window->curpanel == 2 );
    set = xset_set_cb( "panel_3", focus_panel, main_window );
        xset_set_ob1_int( set, "panel_num", 3 );
        set->disable = ( main_window->curpanel == 3 );
    set = xset_set_cb( "panel_4", focus_panel, main_window );
        xset_set_ob1_int( set, "panel_num", 4 );
        set->disable = ( main_window->curpanel == 4 );

    menu_elements = g_strdup_printf( "panel1_show panel2_show panel3_show panel4_show main_pbar main_focus_panel" );
    char* menu_elements2 = g_strdup_printf( "sep_v1 main_tasks main_auto sep_v2 main_title main_icon main_full sep_v3 main_design_mode main_prefs" );
    
    main_task_prepare_menu( main_window, newmenu, accel_group );
    xset_add_menu( NULL, file_browser, newmenu, accel_group, menu_elements );

    // Panel View submenu
    set = xset_get( "con_view" );
    str = set->menu_label;
    set->menu_label = g_strdup_printf( "%s %d %s", _("Panel"),
                                main_window->curpanel, set->menu_label );
    ptk_file_menu_add_panel_view_menu( file_browser, newmenu, accel_group );
    g_free( set->menu_label );
    set->menu_label = str;

    xset_add_menu( NULL, file_browser, newmenu, accel_group, menu_elements2 );
    g_free( menu_elements );
    g_free( menu_elements2 );
    gtk_widget_show_all( GTK_WIDGET(newmenu) );
    g_signal_connect( newmenu, "key-press-event",
                      G_CALLBACK( xset_menu_keypress ), NULL );
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( main_window->view_menu_item ), newmenu );

    // Devices
    main_window->dev_menu = create_devices_menu( main_window );
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( main_window->dev_menu_item ),
                                                    main_window->dev_menu );
    g_signal_connect( main_window->dev_menu, "key-press-event",
                      G_CALLBACK( xset_menu_keypress ), NULL );
    
    // Bookmarks
    newmenu = gtk_menu_new();
    set = xset_set_cb( "book_show", on_bookmarks_show, main_window );   
    set->b = file_browser->side_book ? XSET_B_TRUE : XSET_B_UNSET;
    xset_add_menuitem( NULL, file_browser, newmenu, accel_group,
                                set );
    set = xset_set_cb( "book_add", ptk_bookmark_view_add_bookmark, file_browser );
    set->disable = FALSE;
    xset_add_menuitem( NULL, file_browser, newmenu, accel_group,
                                set );
    gtk_menu_shell_append( GTK_MENU_SHELL(newmenu),
                                gtk_separator_menu_item_new() );
    xset_add_menuitem( NULL, file_browser, newmenu, accel_group,
                                ptk_bookmark_view_get_first_bookmark( NULL ) );
    gtk_widget_show_all( GTK_WIDGET(newmenu) );
    g_signal_connect( newmenu, "key-press-event",
                                G_CALLBACK( xset_menu_keypress ), NULL );
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( main_window->book_menu_item ),
                                                            newmenu );
    
    // Plugins
    main_window->plug_menu = create_plugins_menu( main_window );
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( main_window->plug_menu_item ),
                                                    main_window->plug_menu );
    g_signal_connect( main_window->plug_menu, "key-press-event",
                      G_CALLBACK( xset_menu_keypress ), NULL );
    
    // Tool
    newmenu = gtk_menu_new();
    set = xset_get( "main_tool" );
    if ( !set->child )
    {
        child_set = xset_custom_new();
        child_set->menu_label = g_strdup( _("New _Command") );
        child_set->parent = g_strdup( "main_tool" );
        set->child = g_strdup( child_set->name );
    }
    else
        child_set = xset_get( set->child );
    xset_add_menuitem( NULL, file_browser, newmenu, accel_group, child_set );
    gtk_widget_show_all( GTK_WIDGET(newmenu) );
    g_signal_connect( newmenu, "key-press-event",
                      G_CALLBACK( xset_menu_keypress ), NULL );
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( main_window->tool_menu_item ),
                                                            newmenu );
    
    // Help
    newmenu = gtk_menu_new();
    xset_set_cb( "main_faq", on_main_faq, main_window );
    xset_set_cb( "main_about", on_about_activate, main_window );
    xset_set_cb( "main_help", on_main_help_activate, main_window );
    xset_set_cb( "main_homepage", on_homepage_activate, main_window );
    xset_set_cb( "main_news", on_news_activate, main_window );
    xset_set_cb( "main_getplug", on_getplug_activate, main_window );
    menu_elements = g_strdup_printf( "main_faq main_help sep_h1 main_homepage main_news main_getplug sep_h2 main_help_opt sep_h3 main_about" );
    xset_add_menu( NULL, file_browser, newmenu, accel_group, menu_elements );
    g_free( menu_elements );
    gtk_widget_show_all( GTK_WIDGET(newmenu) );
    g_signal_connect( newmenu, "key-press-event",
                      G_CALLBACK( xset_menu_keypress ), NULL );
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( main_window->help_menu_item ), newmenu );
//printf("rebuild_menus  DONE\n");
}

void on_main_window_realize( GtkWidget* widget, FMMainWindow* main_window )
{
    // preset the task manager height for no double-resize on first show
    idle_set_task_height( main_window );
}

void fm_main_window_init( FMMainWindow* main_window )
{
    GtkWidget *bookmark_menu;
    //GtkWidget *view_menu_item;
    GtkWidget *edit_menu_item, *edit_menu, *history_menu;
    GtkToolItem *toolitem;
    GtkWidget *hbox;
    GtkLabel *label;
    GtkAccelGroup *edit_accel_group;
    GClosure* closure;
    int i;
    char* icon_name;
    XSet* set;
    
    main_window->configure_evt_timer = 0;
    main_window->fullscreen = FALSE;
    main_window->opened_maximized = main_window->maximized =
                                                        app_settings.maximized;

    /* this is used to limit the scope of gtk_grab and modal dialogs */
    main_window->wgroup = gtk_window_group_new();
    gtk_window_group_add_window( main_window->wgroup, (GtkWindow*)main_window );

    /* Add to total window count */
    ++n_windows;
    all_windows = g_list_prepend( all_windows, main_window );

    pcmanfm_ref();

    //g_signal_connect( G_OBJECT( main_window ), "task-notify",
    //                            G_CALLBACK( ptk_file_task_notify_handler ), NULL );

    /* Start building GUI */
    /*
    NOTE: gtk_window_set_icon_name doesn't work under some WMs, such as IceWM.
    gtk_window_set_icon_name( GTK_WINDOW( main_window ),
                              "gnome-fs-directory" ); */
    if( 0 == theme_change_notify )
    {
        theme_change_notify = g_signal_connect( gtk_icon_theme_get_default(), "changed",
                                        G_CALLBACK(update_window_icons), NULL );
    }
    update_window_icon( (GtkWindow*)main_window, gtk_icon_theme_get_default() );

    main_window->main_vbox = gtk_vbox_new ( FALSE, 0 );
    gtk_container_add ( GTK_CONTAINER ( main_window ), main_window->main_vbox );

    // Create menu bar
    main_window->accel_group = gtk_accel_group_new ();
    main_window->menu_bar = gtk_menu_bar_new();
    //gtk_box_pack_start ( GTK_BOX ( main_window->main_vbox ),
    //                     main_window->menu_bar, FALSE, FALSE, 0 );
    GtkWidget* menu_hbox = gtk_hbox_new ( FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX ( menu_hbox ),
                         main_window->menu_bar, TRUE, TRUE, 0 );

    // panelbar
    main_window->panelbar = gtk_toolbar_new();
#if GTK_CHECK_VERSION (3, 0, 0)
    GtkStyleContext *style_ctx = gtk_widget_get_style_context( main_window->panelbar );
    gtk_style_context_add_class (style_ctx, GTK_STYLE_CLASS_MENUBAR);
    gtk_style_context_remove_class (style_ctx, GTK_STYLE_CLASS_TOOLBAR);
#endif
    gtk_toolbar_set_show_arrow( GTK_TOOLBAR( main_window->panelbar ), FALSE );
    gtk_toolbar_set_style( GTK_TOOLBAR( main_window->panelbar ), GTK_TOOLBAR_ICONS );
    gtk_toolbar_set_icon_size( GTK_TOOLBAR( main_window->panelbar ), GTK_ICON_SIZE_MENU );
    // set pbar background to menu bar background
    //gtk_widget_modify_bg( main_window->panelbar, GTK_STATE_NORMAL,
    //                                    &GTK_WIDGET( main_window )
    //                                    ->style->bg[ GTK_STATE_NORMAL ] );
    for ( i = 0; i < 4; i++ )
    {
        main_window->panel_btn[i] = GTK_WIDGET( gtk_toggle_tool_button_new() );
        icon_name = g_strdup_printf( _("Show Panel %d"), i + 1 );
        gtk_tool_button_set_label( GTK_TOOL_BUTTON( main_window->panel_btn[i] ),
                                                            icon_name );
        g_free( icon_name );
        set = xset_get_panel( i + 1, "icon_status" );
        if ( set->icon && set->icon[0] != '\0' )
            icon_name = set->icon;
        else
            icon_name = "gtk-yes";
        main_window->panel_image[i] = xset_get_image( icon_name, GTK_ICON_SIZE_MENU );
        gtk_tool_button_set_icon_widget( GTK_TOOL_BUTTON( main_window->panel_btn[i] ),
                                                    main_window->panel_image[i] );
        gtk_toolbar_insert( GTK_TOOLBAR( main_window->panelbar ),
                                GTK_TOOL_ITEM( main_window->panel_btn[i] ), -1 );
        g_signal_connect( main_window->panel_btn[i], "toggled",
                                G_CALLBACK( on_toggle_panelbar ), main_window );
        if ( i == 1 )
        {
            GtkToolItem* sep = gtk_separator_tool_item_new();
            gtk_separator_tool_item_set_draw( GTK_SEPARATOR_TOOL_ITEM( sep ), TRUE );
            gtk_toolbar_insert( GTK_TOOLBAR( main_window->panelbar ), sep, -1 );
        }
    }
    gtk_box_pack_start ( GTK_BOX ( menu_hbox ),
                         main_window->panelbar, TRUE, TRUE, 0 );
    gtk_box_pack_start ( GTK_BOX ( main_window->main_vbox ),
                         menu_hbox, FALSE, FALSE, 0 );

    main_window->file_menu_item = gtk_menu_item_new_with_mnemonic( _("_File") );
    gtk_menu_shell_append( GTK_MENU_SHELL( main_window->menu_bar ), main_window->file_menu_item );
    
    main_window->view_menu_item = gtk_menu_item_new_with_mnemonic( _("_View") );
    gtk_menu_shell_append( GTK_MENU_SHELL( main_window->menu_bar ), main_window->view_menu_item );
    
    main_window->dev_menu_item = gtk_menu_item_new_with_mnemonic( _("_Devices") );
    gtk_menu_shell_append( GTK_MENU_SHELL( main_window->menu_bar ), main_window->dev_menu_item );
    main_window->dev_menu = NULL;

    main_window->book_menu_item = gtk_menu_item_new_with_mnemonic( _("_Bookmarks") );
    gtk_menu_shell_append( GTK_MENU_SHELL( main_window->menu_bar ), main_window->book_menu_item );

    main_window->plug_menu_item = gtk_menu_item_new_with_mnemonic( _("_Plugins") );
    gtk_menu_shell_append( GTK_MENU_SHELL( main_window->menu_bar ), main_window->plug_menu_item );
    main_window->plug_menu = NULL;

    main_window->tool_menu_item = gtk_menu_item_new_with_mnemonic( _("_Tools") );
    gtk_menu_shell_append( GTK_MENU_SHELL( main_window->menu_bar ), main_window->tool_menu_item );
    
    main_window->help_menu_item = gtk_menu_item_new_with_mnemonic( _("_Help") );
    gtk_menu_shell_append( GTK_MENU_SHELL( main_window->menu_bar ), main_window->help_menu_item );

    rebuild_menus( main_window );

/*
#ifdef SUPER_USER_CHECKS
    // Create warning bar for super user
    if ( geteuid() == 0 )                 // Run as super user!
    {
        main_window->status_bar = gtk_event_box_new();
        gtk_widget_modify_bg( main_window->status_bar, GTK_STATE_NORMAL,
                              &main_window->status_bar->style->bg[ GTK_STATE_SELECTED ] );
        label = GTK_LABEL( gtk_label_new ( _( "Warning: You are in super user mode" ) ) );
        gtk_misc_set_padding( GTK_MISC( label ), 0, 2 );
        gtk_widget_modify_fg( GTK_WIDGET( label ), GTK_STATE_NORMAL,
                              &GTK_WIDGET( label ) ->style->fg[ GTK_STATE_SELECTED ] );
        gtk_container_add( GTK_CONTAINER( main_window->status_bar ), GTK_WIDGET( label ) );
        gtk_box_pack_start ( GTK_BOX ( main_window->main_vbox ),
                             main_window->status_bar, FALSE, FALSE, 2 );
    }
#endif
*/

    /* Create client area */
    main_window->task_vpane = gtk_vpaned_new();
    main_window->vpane = gtk_vpaned_new();
    main_window->hpane_top = gtk_hpaned_new();
    main_window->hpane_bottom = gtk_hpaned_new();

    for ( i = 0; i < 4; i++ )
    {
        main_window->panel[i] = gtk_notebook_new();
        gtk_notebook_set_show_border( GTK_NOTEBOOK( main_window->panel[i] ), FALSE );
        gtk_notebook_set_scrollable ( GTK_NOTEBOOK( main_window->panel[i] ), TRUE );
        g_signal_connect ( main_window->panel[i], "switch-page",
                        G_CALLBACK ( on_folder_notebook_switch_pape ), main_window );
    }

    main_window->task_scroll = gtk_scrolled_window_new( NULL, NULL );
    
    gtk_paned_pack1( GTK_PANED(main_window->hpane_top), main_window->panel[0],
                                                                    FALSE, TRUE );
    gtk_paned_pack2( GTK_PANED(main_window->hpane_top), main_window->panel[1],
                                                                    TRUE, TRUE );
    gtk_paned_pack1( GTK_PANED(main_window->hpane_bottom), main_window->panel[2],
                                                                    FALSE, TRUE );
    gtk_paned_pack2( GTK_PANED(main_window->hpane_bottom), main_window->panel[3],
                                                                    TRUE, TRUE );

    gtk_paned_pack1( GTK_PANED(main_window->vpane), main_window->hpane_top,
                                                                    FALSE, TRUE );
    gtk_paned_pack2( GTK_PANED(main_window->vpane), main_window->hpane_bottom,
                                                                    TRUE, TRUE );

    gtk_paned_pack1( GTK_PANED(main_window->task_vpane), main_window->vpane,
                                                                    TRUE, TRUE );
    gtk_paned_pack2( GTK_PANED(main_window->task_vpane), main_window->task_scroll,
                                                                    FALSE, TRUE );

    gtk_box_pack_start ( GTK_BOX ( main_window->main_vbox ),
                               GTK_WIDGET( main_window->task_vpane ), TRUE, TRUE, 0 );

    main_window->notebook = main_window->panel[0];
    main_window->curpanel = 1;

/*    GTK_NOTEBOOK( gtk_notebook_new() );
    gtk_notebook_set_show_border( main_window->notebook, FALSE );
    gtk_notebook_set_scrollable ( main_window->notebook, TRUE );
    gtk_box_pack_start ( GTK_BOX ( main_window->main_vbox ), GTK_WIDGET( main_window->notebook ), TRUE, TRUE, 0 );
*/
/*
    // Create Status bar 
    main_window->status_bar = gtk_statusbar_new ();
    gtk_box_pack_start ( GTK_BOX ( main_window->main_vbox ),
                         main_window->status_bar, FALSE, FALSE, 0 );
*/

    // Task View
    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( main_window->task_scroll ),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    main_window->task_view = main_task_view_new( main_window );
    gtk_container_add( GTK_CONTAINER( main_window->task_scroll ),
                                            GTK_WIDGET( main_window->task_view ) );
    
    gtk_window_set_role( GTK_WINDOW( main_window ), "file_manager" );

    gtk_widget_show_all( main_window->main_vbox );

    g_signal_connect( G_OBJECT( main_window->file_menu_item ), "button-press-event",
                      G_CALLBACK( on_menu_bar_event ), main_window );
    g_signal_connect( G_OBJECT( main_window->view_menu_item ), "button-press-event",
                      G_CALLBACK( on_menu_bar_event ), main_window );
    g_signal_connect( G_OBJECT( main_window->dev_menu_item ), "button-press-event",
                      G_CALLBACK( on_menu_bar_event ), main_window );
    g_signal_connect( G_OBJECT( main_window->book_menu_item ), "button-press-event",
                      G_CALLBACK( on_menu_bar_event ), main_window );
    g_signal_connect( G_OBJECT( main_window->plug_menu_item ), "button-press-event",
                      G_CALLBACK( on_menu_bar_event ), main_window );
    g_signal_connect( G_OBJECT( main_window->tool_menu_item ), "button-press-event",
                      G_CALLBACK( on_menu_bar_event ), main_window );
    g_signal_connect( G_OBJECT( main_window->help_menu_item ), "button-press-event",
                      G_CALLBACK( on_menu_bar_event ), main_window );

    // use this OR widget_class->key_press_event = on_main_window_keypress;  
    g_signal_connect( G_OBJECT( main_window ), "key-press-event",
                      G_CALLBACK( on_main_window_keypress ), NULL );

    g_signal_connect ( main_window, "focus-in-event",
                       G_CALLBACK ( on_main_window_focus ), NULL );

    g_signal_connect( G_OBJECT(main_window), "configure-event",
                      G_CALLBACK(on_window_configure_event), main_window );

    g_signal_connect ( G_OBJECT(main_window), "button-press-event",
                      G_CALLBACK ( on_window_button_press_event ), main_window );

    g_signal_connect ( G_OBJECT(main_window), "realize",
                      G_CALLBACK ( on_main_window_realize ), main_window );

    import_all_plugins( main_window );
    main_window->panel_change = FALSE;
    show_panels( NULL, main_window );
    main_window_root_bar_all();
    
    gtk_widget_hide( GTK_WIDGET( main_window->task_scroll ) );
    on_task_popup_show( NULL, main_window, NULL );

    // show window
    gtk_window_set_default_size( GTK_WINDOW( main_window ),
                                 app_settings.width, app_settings.height );
    if ( app_settings.maximized )
        gtk_window_maximize( GTK_WINDOW( main_window ) );
    gtk_widget_show ( GTK_WIDGET( main_window ) );

    // restore panel sliders
    // do this after maximizing/showing window so slider positions are valid
    // in actual window size
    int pos = xset_get_int( "panel_sliders", "x" );
    if ( pos < 200 ) pos = 200;
    gtk_paned_set_position( GTK_PANED( main_window->hpane_top ), pos );
    pos = xset_get_int( "panel_sliders", "y" );
    if ( pos < 200 ) pos = 200;
    gtk_paned_set_position( GTK_PANED( main_window->hpane_bottom ), pos );
    pos = xset_get_int( "panel_sliders", "s" );
    if ( pos < 200 ) pos = -1;
    gtk_paned_set_position( GTK_PANED( main_window->vpane ), pos );

    // build the main menu initially, eg for F10 - Note: file_list is NULL
    // NOT doing this because it slows down the initial opening of the window
    // and shows a stale menu anyway.
    //rebuild_menus( main_window );

    main_window_event( main_window, NULL, "evt_win_new", 0, 0, NULL, 0, 0, 0, TRUE );
}

void fm_main_window_finalize( GObject *obj )
{
    all_windows = g_list_remove( all_windows, obj );
    --n_windows;

    g_object_unref( ((FMMainWindow*)obj)->wgroup );

    pcmanfm_unref();

    /* Remove the monitor for changes of the bookmarks */
//    ptk_bookmarks_remove_callback( ( GFunc ) on_bookmarks_change, obj );
    if ( 0 == n_windows )
    {
        g_signal_handler_disconnect( gtk_icon_theme_get_default(), theme_change_notify );
        theme_change_notify = 0;
    }
 
    G_OBJECT_CLASS( parent_class ) ->finalize( obj );
}

void fm_main_window_get_property ( GObject *obj,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec )
{}

void fm_main_window_set_property ( GObject *obj,
                                   guint prop_id,
                                   const GValue *value,
                                   GParamSpec *pspec )
{}

static void fm_main_window_close( FMMainWindow* main_window )
{    
    /*
    printf("DISC=%d\n", g_signal_handlers_disconnect_by_func(
                            G_OBJECT( main_window ),
                            G_CALLBACK( ptk_file_task_notify_handler ), NULL ) );
    */
    if ( evt_win_close->s || evt_win_close->ob2_data )
        main_window_event( main_window, evt_win_close, "evt_win_close", 0, 0,
                                                        NULL, 0, 0, 0, FALSE );
    gtk_widget_destroy( GTK_WIDGET( main_window ) );
}

void on_abort_tasks_response( GtkDialog* dlg, int response, GtkWidget* main_window )
{
    fm_main_window_close( (FMMainWindow*)main_window );
}

void fm_main_window_store_positions( FMMainWindow* main_window )
{
    if ( !main_window )
        main_window = fm_main_window_get_last_active();
    if ( !main_window )
        return;
    // if the window is not fullscreen (is normal or maximized) save sliders
    // and columns
    if ( !main_window->fullscreen )
    {
        // store width/height + sliders
        int pos;
        char* posa;
        GtkAllocation allocation;
        gtk_widget_get_allocation ( GTK_WIDGET( main_window ) , &allocation );

        if ( !main_window->maximized && allocation.width > 0 )
        {
            app_settings.width = allocation.width;
            app_settings.height = allocation.height;
        }
        if ( GTK_IS_PANED( main_window->hpane_top ) )
        {
            pos = gtk_paned_get_position( GTK_PANED( main_window->hpane_top ) );
            if ( pos )
            {
                posa = g_strdup_printf( "%d", pos );
                xset_set( "panel_sliders", "x", posa );
                g_free( posa );
            }
            
            pos = gtk_paned_get_position( GTK_PANED( main_window->hpane_bottom ) );
            if ( pos )
            {
                posa = g_strdup_printf( "%d", pos );
                xset_set( "panel_sliders", "y", posa );
                g_free( posa );
            }
            
            pos = gtk_paned_get_position( GTK_PANED( main_window->vpane ) );
            if ( pos )
            {
                posa = g_strdup_printf( "%d", pos );
                xset_set( "panel_sliders", "s", posa );
                g_free( posa );
            }
            if ( gtk_widget_get_visible( main_window->task_scroll ) )
            {
                pos = gtk_paned_get_position( GTK_PANED( main_window->task_vpane ) );
                if ( pos )
                {
                    // save slider pos for version < 0.9.2 (in case of downgrade)
                    posa = g_strdup_printf( "%d", pos );
                    xset_set( "panel_sliders", "z", posa );
                    g_free( posa );
                    // save absolute height introduced v0.9.2
                    posa = g_strdup_printf( "%d", allocation.height - pos );
                    xset_set( "task_show_manager", "x", posa );
                    g_free( posa );
//printf("CLOS  win %dx%d    task height %d   slider %d\n", allocation.width, allocation.height, allocation.height - pos, pos );
                }
            }
        }

        // store task columns
        on_task_columns_changed( main_window->task_view, NULL );

        // store fb columns
        int p, page_x;
        PtkFileBrowser* a_browser;
        if ( main_window->maximized )
            main_window->opened_maximized = TRUE;  // force save of columns
        for ( p = 1; p < 5; p++ )
        {
            page_x = gtk_notebook_get_current_page( GTK_NOTEBOOK( main_window->panel[p-1] ) );
            if ( page_x != -1 )
            {
                a_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                                GTK_NOTEBOOK( main_window->panel[p-1] ),
                                                page_x ) );
                if ( a_browser && a_browser->view_mode == PTK_FB_LIST_VIEW )
                    ptk_file_browser_save_column_widths( 
                                    GTK_TREE_VIEW( a_browser->folder_view ),
                                    a_browser );
            }
        }
    }
}

gboolean fm_main_window_delete_event ( GtkWidget *widget,
                              GdkEvent *event )
{
//printf("fm_main_window_delete_event\n");

    FMMainWindow* main_window = (FMMainWindow*)widget;

    fm_main_window_store_positions( main_window );
    
    // save settings
    app_settings.maximized = main_window->maximized;
    xset_autosave_cancel();
    char* err_msg = save_settings( main_window );
    if ( err_msg )
    {
        char* msg = g_strdup_printf( _("Error: Unable to save session file.  Do you want to exit without saving?\n\n( %s )"), err_msg );
        g_free( err_msg );
        GtkWidget* dlg = gtk_message_dialog_new( GTK_WINDOW( widget ), GTK_DIALOG_MODAL,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_YES_NO,
                                        msg, NULL );
        g_free( msg );
        gtk_dialog_set_default_response( GTK_DIALOG( dlg ), GTK_RESPONSE_NO );
        gtk_widget_show_all( dlg );
        gtk_window_set_title( GTK_WINDOW( dlg ), _("SpaceFM Error") );
        if ( gtk_dialog_run( GTK_DIALOG( dlg ) ) == GTK_RESPONSE_NO )
        {
            gtk_widget_destroy( dlg );
            return TRUE;
        }
        gtk_widget_destroy( dlg );
    }
    
    // tasks running?
    if ( main_tasks_running( main_window ) )
    {
        GtkWidget* dlg = gtk_message_dialog_new( GTK_WINDOW( widget ),
                                              GTK_DIALOG_MODAL,
                                              GTK_MESSAGE_QUESTION,
                                              GTK_BUTTONS_YES_NO,
                                              _( "Stop all tasks running in this window?" ) );
        gtk_dialog_set_default_response( GTK_DIALOG( dlg ), GTK_RESPONSE_NO );
        if ( gtk_dialog_run( GTK_DIALOG( dlg ) ) == GTK_RESPONSE_YES )
        {
            gtk_widget_destroy( dlg );
            dlg = gtk_message_dialog_new( GTK_WINDOW( widget ),
                                                  GTK_DIALOG_MODAL,
                                                  GTK_MESSAGE_INFO,
                                                  GTK_BUTTONS_CLOSE,
                                                  _( "Aborting tasks..." ) );
            g_signal_connect( dlg, "response",
                      G_CALLBACK( on_abort_tasks_response ), widget );
            g_signal_connect( dlg, "destroy",
                      G_CALLBACK( gtk_widget_destroy ), dlg );
            gtk_widget_show_all( dlg );
            
            on_task_stop( NULL, main_window->task_view, xset_get( "task_stop_all" ),
                                                                        NULL );
            while ( main_tasks_running( main_window ) )
            {
                while( gtk_events_pending() )
                    gtk_main_iteration();
            }
        }
        else
        {
            gtk_widget_destroy( dlg );
            return TRUE;
        }
    }
    fm_main_window_close( main_window );
    return TRUE;
}

static gboolean fm_main_window_window_state_event ( GtkWidget *widget,
                                                    GdkEventWindowState *event )
{
    FMMainWindow* main_window = (FMMainWindow*)widget;

    main_window->maximized = app_settings.maximized =
                ((event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0);
    if ( !main_window->maximized )
    {
        if ( main_window->opened_maximized )
            main_window->opened_maximized = FALSE;
        show_panels( NULL, main_window );  // restore columns
    }

    return TRUE;
}

char* main_window_get_tab_cwd( PtkFileBrowser* file_browser, int tab_num )
{
    if ( !file_browser )
        return NULL;
    int page_x;
    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;
    GtkWidget* notebook = main_window->panel[file_browser->mypanel - 1];
    int pages = gtk_notebook_get_n_pages( GTK_NOTEBOOK( notebook ) );
    int page_num = gtk_notebook_page_num( GTK_NOTEBOOK( notebook ),
                                                GTK_WIDGET( file_browser ) );
    if ( tab_num == -1 ) // prev
        page_x = page_num - 1;
    else if ( tab_num == -2 ) // next
        page_x = page_num + 1;
    else
        page_x = tab_num - 1; // tab_num starts counting at 1
    
    if ( page_x > -1 && page_x < pages )
    {
        return g_strdup( ptk_file_browser_get_cwd( PTK_FILE_BROWSER( 
                            gtk_notebook_get_nth_page( GTK_NOTEBOOK( notebook ),
                            page_x ) ) ) );
    }
    else
        return NULL;
}

char* main_window_get_panel_cwd( PtkFileBrowser* file_browser, int panel_num )
{
    if ( !file_browser )
        return NULL;
    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;
    int panel_x = file_browser->mypanel;
    
    if ( panel_num == -1 ) // prev
    {
        do
        {
            if ( --panel_x < 1 )
                panel_x = 4;
            if ( panel_x == file_browser->mypanel )
                return NULL;
        } while ( !gtk_widget_get_visible( main_window->panel[panel_x - 1] ) );
    }
    else if ( panel_num == -2 ) // next
    {
        do
        {
            if ( ++panel_x > 4 )
                panel_x = 1;
            if ( panel_x == file_browser->mypanel )
                return NULL;
        } while ( !gtk_widget_get_visible( main_window->panel[panel_x - 1] ) );
    }
    else
    {
        panel_x = panel_num;
        if ( !gtk_widget_get_visible( main_window->panel[panel_x - 1] ) )
            return NULL;
    }
    
    GtkWidget* notebook = main_window->panel[panel_x - 1];
    int page_x = gtk_notebook_get_current_page( GTK_NOTEBOOK( notebook ) );
    return g_strdup( ptk_file_browser_get_cwd( PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                        GTK_NOTEBOOK( notebook ), page_x ) ) ) );
}

void main_window_open_in_panel( PtkFileBrowser* file_browser, int panel_num,
                                                        char* file_path )
{
    if ( !file_browser )
        return;
    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;
    int panel_x = file_browser->mypanel;
    
    if ( panel_num == -1 ) // prev
    {
        do
        {
            if ( --panel_x < 1 )
                panel_x = 4;
            if ( panel_x == file_browser->mypanel )
                return;
        } while ( !gtk_widget_get_visible( main_window->panel[panel_x - 1] ) );
    }
    else if ( panel_num == -2 ) // next
    {
        do
        {
            if ( ++panel_x > 4 )
                panel_x = 1;
            if ( panel_x == file_browser->mypanel )
                return;
        } while ( !gtk_widget_get_visible( main_window->panel[panel_x - 1] ) );
    }
    else
    {
        panel_x = panel_num;
    }

    if ( panel_x < 1 || panel_x > 4 )
        return;

    // show panel
    if ( !gtk_widget_get_visible( main_window->panel[panel_x - 1] ) )
    {
        xset_set_b_panel( panel_x, "show", TRUE );
        show_panels_all_windows( NULL, main_window );
    }
    
    // open in tab in panel
    int save_curpanel = main_window->curpanel;
    
    main_window->curpanel = panel_x;
    main_window->notebook = main_window->panel[panel_x - 1];
    
    fm_main_window_add_new_tab( main_window, file_path );
        
    main_window->curpanel = save_curpanel;
    main_window->notebook = main_window->panel[main_window->curpanel - 1];

    // focus original panel
    //while( gtk_events_pending() )
    //    gtk_main_iteration();
    //gtk_widget_grab_focus( GTK_WIDGET( main_window->notebook ) );
    //gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );
    g_idle_add( ( GSourceFunc ) delayed_focus_file_browser, file_browser );
}

gboolean main_window_panel_is_visible( PtkFileBrowser* file_browser, int panel )
{
    if ( panel < 1 || panel > 4 )
        return FALSE;
    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;
    return gtk_widget_get_visible( main_window->panel[panel - 1] );
}

void main_window_get_counts( PtkFileBrowser* file_browser, int* panel_count,
                                                int* tab_count, int* tab_num )
{
    if ( !file_browser )
        return;
    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;
    GtkWidget* notebook = main_window->panel[file_browser->mypanel - 1];
    *tab_count = gtk_notebook_get_n_pages( GTK_NOTEBOOK( notebook ) );
    // tab_num starts counting from 1
    *tab_num = gtk_notebook_page_num( GTK_NOTEBOOK( notebook ),
                                            GTK_WIDGET( file_browser ) ) + 1;
    int count = 0;
    int i;
    for ( i = 0; i < 4; i++ )
    {
        if ( gtk_widget_get_visible( main_window->panel[i] ) )
            count++;
    }
    *panel_count = count;
}

void on_close_notebook_page( GtkButton* btn, PtkFileBrowser* file_browser )
{
    PtkFileBrowser* a_browser;
    
//printf( "\n============== on_close_notebook_page fb=%#x\n", file_browser );
    if ( !GTK_IS_WIDGET( file_browser ) )
        return;
    GtkNotebook* notebook = GTK_NOTEBOOK(
                                 gtk_widget_get_ancestor( GTK_WIDGET( file_browser ),
                                                          GTK_TYPE_NOTEBOOK ) );
    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;

    main_window->curpanel = file_browser->mypanel;
    main_window->notebook = main_window->panel[main_window->curpanel - 1];

    if ( evt_tab_close->s || evt_tab_close->ob2_data )
        main_window_event( main_window, evt_tab_close, "evt_tab_close",
                            file_browser->mypanel, 
                            gtk_notebook_page_num( 
                                            GTK_NOTEBOOK( main_window->notebook ),
                                            GTK_WIDGET( file_browser ) ) + 1,
                            NULL, 0, 0, 0, FALSE );
    
    // save solumns and slider positions of tab to be closed
    ptk_file_browser_slider_release( NULL, NULL, file_browser );
    ptk_file_browser_save_column_widths( GTK_TREE_VIEW( file_browser->folder_view ),
                                                        file_browser );

    // without this signal blocked, on_close_notebook_page is called while
    // ptk_file_browser_update_views is still in progress causing segfault
    g_signal_handlers_block_matched( main_window->notebook,
            G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_folder_notebook_switch_pape, NULL );

    // remove page can also be used to destroy - same result
    //gtk_notebook_remove_page( notebook, gtk_notebook_get_current_page( notebook ) );
    gtk_widget_destroy( GTK_WIDGET( file_browser ) );

    if ( !app_settings.always_show_tabs )
    {
        if ( gtk_notebook_get_n_pages ( notebook ) == 1 )
            gtk_notebook_set_show_tabs( notebook, FALSE );
    }
    if ( gtk_notebook_get_n_pages ( notebook ) == 0 )
    {
        const char* path = xset_get_s( "go_set_default" );
        if ( !( path && path[0] != '\0' ) )
        {
            if ( geteuid() != 0 )
                path =  g_get_home_dir();
            else
                path = "/";
        }
        fm_main_window_add_new_tab( main_window, path );
        a_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page( 
                                    GTK_NOTEBOOK( notebook ), 0 ) );
        if ( GTK_IS_WIDGET( a_browser ) )
            ptk_file_browser_update_views( NULL, a_browser );
        goto _done_close;
    }

    // update view of new current tab
    int cur_tabx = gtk_notebook_get_current_page( GTK_NOTEBOOK( main_window->notebook ) );
    if ( cur_tabx != -1 )
    {
        a_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page( 
                                    GTK_NOTEBOOK( notebook ), cur_tabx ) );
 
        ptk_file_browser_update_views( NULL, a_browser );
        if ( GTK_IS_WIDGET( a_browser ) )
        {
            fm_main_window_update_status_bar( main_window, a_browser );
            g_idle_add( ( GSourceFunc ) delayed_focus, a_browser->folder_view );
        }
        if ( evt_tab_focus->s || evt_tab_focus->ob2_data )
            main_window_event( main_window, evt_tab_focus, "evt_tab_focus",
                                        main_window->curpanel,
                                        cur_tabx + 1, NULL, 0, 0, 0, FALSE );
    }

_done_close:
    g_signal_handlers_unblock_matched( main_window->notebook,
            G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_folder_notebook_switch_pape, NULL );

    update_window_title( NULL, main_window );
    if ( xset_get_b( "main_save_tabs" ) )
        xset_autosave( FALSE, TRUE );
}

gboolean notebook_clicked (GtkWidget* widget, GdkEventButton * event,
                           PtkFileBrowser* file_browser)  //MOD added
{
    on_file_browser_panel_change( file_browser,
                                        (FMMainWindow*)file_browser->main_window );
    if ( ( evt_win_click->s || evt_win_click->ob2_data ) && 
            main_window_event( file_browser->main_window, evt_win_click, "evt_win_click",
                            0, 0, "tabbar", 0, event->button, event->state, TRUE ) )
        return TRUE;
    // middle-click on tab closes
    if (event->type == GDK_BUTTON_PRESS)
    {
        if ( event->button == 2 )
        {
            on_close_notebook_page( NULL, file_browser );
            return TRUE;
        }
        else if ( event->button == 3 )
        {
            GtkWidget* popup = gtk_menu_new();
            GtkAccelGroup* accel_group = gtk_accel_group_new();
            XSetContext* context = xset_context_new();
            main_context_fill( file_browser, context );
            
            XSet* set = xset_set_cb( "tab_close", on_close_notebook_page,
                                                                file_browser );
            xset_add_menuitem( NULL, file_browser, popup, accel_group, set );
            set = xset_set_cb( "tab_new", ptk_file_browser_new_tab, file_browser );
            xset_add_menuitem( NULL, file_browser, popup, accel_group, set );
            set = xset_set_cb( "tab_new_here", ptk_file_browser_new_tab_here,
                                                                file_browser );
            xset_add_menuitem( NULL, file_browser, popup, accel_group, set );
            set = xset_get( "sep_tab" );
            xset_add_menuitem( NULL, file_browser, popup, accel_group, set );
            set = xset_set_cb_panel( file_browser->mypanel, "icon_tab",
                                                main_update_fonts, file_browser );
            xset_add_menuitem( NULL, file_browser, popup, accel_group, set );
            set = xset_set_cb_panel( file_browser->mypanel, "font_tab",
                                                main_update_fonts, file_browser );
            xset_add_menuitem( NULL, file_browser, popup, accel_group, set );
            gtk_widget_show_all( GTK_WIDGET( popup ) );
            g_signal_connect( popup, "selection-done",
                  G_CALLBACK( gtk_widget_destroy ), NULL );
            g_signal_connect( popup, "key-press-event",
                              G_CALLBACK( xset_menu_keypress ), NULL );
            gtk_menu_popup( GTK_MENU( popup ), NULL, NULL,
                                NULL, NULL, event->button, event->time );
            return TRUE;
        }
    }    
    return FALSE;
}

void on_file_browser_begin_chdir( PtkFileBrowser* file_browser,
                                  FMMainWindow* main_window )
{
    fm_main_window_update_status_bar( main_window, file_browser );
}

void on_file_browser_after_chdir( PtkFileBrowser* file_browser,
                                  FMMainWindow* main_window )
{
    //fm_main_window_stop_busy_task( main_window );

    if ( fm_main_window_get_current_file_browser( main_window ) == GTK_WIDGET( file_browser ) )
    {
        set_window_title( main_window, file_browser );
        //gtk_entry_set_text( main_window->address_bar, file_browser->dir->disp_path );
        //gtk_statusbar_push( GTK_STATUSBAR( main_window->status_bar ), 0, "" );
        //fm_main_window_update_command_ui( main_window, file_browser );
    }

    //fm_main_window_update_tab_label( main_window, file_browser, file_browser->dir->disp_path );
    
    if ( file_browser->inhibit_focus )
    {
        // complete ptk_file_browser.c ptk_file_browser_seek_path()
        file_browser->inhibit_focus = FALSE;
        if ( file_browser->seek_name )
        {
            ptk_file_browser_seek_path( file_browser, NULL, file_browser->seek_name );
            g_free( file_browser->seek_name );
            file_browser->seek_name = NULL;
        }
    }
    else
    {
        ptk_file_browser_select_last( file_browser );  //MOD restore last selections
        gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );  //MOD
    }
    if ( xset_get_b( "main_save_tabs" ) )
        xset_autosave( FALSE, TRUE );

    if ( evt_tab_chdir->s || evt_tab_chdir->ob2_data )
        main_window_event( main_window, evt_tab_chdir, "evt_tab_chdir", 0, 0, NULL,
                                                                0, 0, 0, TRUE );
}

GtkWidget* fm_main_window_create_tab_label( FMMainWindow* main_window,
                                            PtkFileBrowser* file_browser )
{
    GtkEventBox * evt_box;
    GtkWidget* tab_label;
    GtkWidget* tab_text;
    GtkWidget* tab_icon = NULL;
    GtkWidget* close_btn;
    GtkWidget* close_icon;
    GdkPixbuf* pixbuf = NULL;
    GtkIconTheme* icon_theme = gtk_icon_theme_get_default();
    
    /* Create tab label */
    evt_box = GTK_EVENT_BOX( gtk_event_box_new () );
    gtk_event_box_set_visible_window ( GTK_EVENT_BOX( evt_box ), FALSE );

    tab_label = gtk_hbox_new( FALSE, 0 );
    XSet* set = xset_get_panel( file_browser->mypanel, "icon_tab" );
    if ( set->icon )
    {
        pixbuf = vfs_load_icon( icon_theme, set->icon, 16 );
        if ( pixbuf )
        {
            tab_icon = gtk_image_new_from_pixbuf( pixbuf );
            g_object_unref( pixbuf );
        }
        else
            tab_icon = xset_get_image( set->icon, GTK_ICON_SIZE_MENU );
    }
    if ( !tab_icon )
        tab_icon = gtk_image_new_from_icon_name ( "gtk-directory",
                                              GTK_ICON_SIZE_MENU );
    gtk_box_pack_start( GTK_BOX( tab_label ),
                            tab_icon, FALSE, FALSE, 4 );

    if ( ptk_file_browser_get_cwd( file_browser ) )
    {
        char* name = g_path_get_basename( ptk_file_browser_get_cwd( file_browser ) );
        if ( name )
        {
            tab_text = gtk_label_new( name );
            g_free( name );
        }
    }
    else
        tab_text = gtk_label_new( "" );

    // set font
    char* fontname = xset_get_s_panel( file_browser->mypanel, "font_tab" );
    if ( fontname )
    {

        PangoFontDescription* font_desc = pango_font_description_from_string( fontname );
        gtk_widget_modify_font( tab_text, font_desc );
        pango_font_description_free( font_desc );
    }

    gtk_label_set_ellipsize( GTK_LABEL( tab_text ), PANGO_ELLIPSIZE_MIDDLE );
#if GTK_CHECK_VERSION (3, 0, 0)
    if (strlen( gtk_label_get_text( GTK_LABEL( tab_text ) ) ) < 30)
    {
        gtk_label_set_ellipsize( GTK_LABEL( tab_text ), PANGO_ELLIPSIZE_NONE );
        gtk_label_set_width_chars( GTK_LABEL( tab_text ), -1 );
    }
    else
        gtk_label_set_width_chars( GTK_LABEL( tab_text ), 30 );
#endif
    gtk_label_set_max_width_chars( GTK_LABEL( tab_text ), 30 );
    gtk_box_pack_start( GTK_BOX( tab_label ),
                        tab_text, FALSE, FALSE, 4 );

    if ( !app_settings.hide_close_tab_buttons ) {
        close_btn = gtk_button_new ();
        gtk_button_set_focus_on_click ( GTK_BUTTON ( close_btn ), FALSE );
        gtk_button_set_relief( GTK_BUTTON ( close_btn ), GTK_RELIEF_NONE );
        pixbuf = vfs_load_icon( icon_theme, GTK_STOCK_CLOSE, 16 );
        if ( pixbuf )
        {
            close_icon = gtk_image_new_from_pixbuf( pixbuf );
            g_object_unref( pixbuf );
            //shorten tab since we have a 16 icon
            gtk_widget_set_size_request ( close_btn, 24, 20 );
#if GTK_CHECK_VERSION (3, 20, 0)
            /* code below produces Adwaita theme parse warnings on GTK >= 3.20
             * Theme parsing error: <data>:2:28: The style property
             * GtkButton:default-border is deprecated and shouldn't be used
             * anymore. It will be removed in a future version  */
#elif GTK_CHECK_VERSION (3, 0, 0)
            /* Code modified from gedit: gedit-close-button.c */
            static const gchar button_style[] =
                  "* {\n"
                  "-GtkButton-default-border : 0;\n"
                  "-GtkButton-default-outside-border : 0;\n"
                  "-GtkButton-inner-border: 0;\n"
                  "-GtkWidget-focus-line-width : 0;\n"
                  "-GtkWidget-focus-padding : 0;\n"
                  "padding: 0;\n"
                "}"; 
            GtkCssProvider *css_prov = gtk_css_provider_new ();
            gtk_css_provider_load_from_data(css_prov, button_style, -1, NULL);
            GtkStyleContext *ctx = gtk_widget_get_style_context( close_btn );
            gtk_style_context_add_provider( ctx, GTK_STYLE_PROVIDER( css_prov ), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION );
#endif
        }
        else
        {
            close_icon = gtk_image_new_from_stock( GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU );
        }
        gtk_container_add ( GTK_CONTAINER ( close_btn ), close_icon );
        gtk_box_pack_end ( GTK_BOX( tab_label ),
                             close_btn, FALSE, FALSE, 0 );
        g_signal_connect( G_OBJECT( close_btn ), "clicked",
                          G_CALLBACK( on_close_notebook_page ), file_browser );
    }

    gtk_container_add ( GTK_CONTAINER ( evt_box ), tab_label );

    gtk_widget_set_events ( GTK_WIDGET( evt_box ), GDK_ALL_EVENTS_MASK );
    gtk_drag_dest_set ( GTK_WIDGET( evt_box ), GTK_DEST_DEFAULT_ALL,
                        drag_targets,
                        sizeof( drag_targets ) / sizeof( GtkTargetEntry ),
                        GDK_ACTION_DEFAULT | GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK );
    g_signal_connect ( ( gpointer ) evt_box, "drag-motion",
                       G_CALLBACK ( on_tab_drag_motion ),
                       file_browser );

    //MOD  middle-click to close tab
    g_signal_connect(G_OBJECT(evt_box), "button-press-event",
            G_CALLBACK(notebook_clicked), file_browser);

    gtk_widget_show_all( GTK_WIDGET( evt_box ) );
    if ( !set->icon )
        gtk_widget_hide( tab_icon );
    return GTK_WIDGET( evt_box );
}


void fm_main_window_update_tab_label( FMMainWindow* main_window,
                                      PtkFileBrowser* file_browser,
                                      const char * path )
{
    GtkWidget * label;
    GtkContainer* hbox;
    GtkImage* icon;
    GtkLabel* text;
    GList* children;
    gchar* name;

    label = gtk_notebook_get_tab_label ( GTK_NOTEBOOK( main_window->notebook ),
                                         GTK_WIDGET( file_browser ) );
    if ( label )
    {
        hbox = GTK_CONTAINER( gtk_bin_get_child ( GTK_BIN( label ) ) );
        children = gtk_container_get_children( hbox );
        icon = GTK_IMAGE( children->data );
        text = GTK_LABEL( children->next->data );

        // TODO: Change the icon

        name = g_path_get_basename( path );
        gtk_label_set_text( text, name );
#if GTK_CHECK_VERSION (3, 0, 0)
    gtk_label_set_ellipsize( text, PANGO_ELLIPSIZE_MIDDLE );
    if (strlen( name ) < 30)
    {
        gtk_label_set_ellipsize( text, PANGO_ELLIPSIZE_NONE );
        gtk_label_set_width_chars( text, -1 );
    }
    else
        gtk_label_set_width_chars( text, 30 );
#endif
        g_free( name );

        g_list_free( children );  //sfm 0.6.0 enabled
    }
}


void fm_main_window_add_new_tab( FMMainWindow* main_window,
                                 const char* folder_path )
{
    GtkWidget * tab_label;
    gint idx;
    GtkWidget* notebook = main_window->notebook;

    PtkFileBrowser* curfb = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser
                                                                ( main_window ) );
    if ( GTK_IS_WIDGET( curfb ) )
    {
        // save sliders of current fb ( new tab while task manager is shown changes vals )
        ptk_file_browser_slider_release( NULL, NULL, curfb );
        // save column widths of fb so new tab has same
        ptk_file_browser_save_column_widths( GTK_TREE_VIEW( curfb->folder_view ),
                                                                curfb );
    }
    int i = main_window->curpanel -1;
    PtkFileBrowser* file_browser = (PtkFileBrowser*)ptk_file_browser_new(
                                    main_window->curpanel, notebook,
                                    main_window->task_view, main_window );
    if ( !file_browser )
        return;
//printf( "++++++++++++++fm_main_window_add_new_tab fb=%#x\n", file_browser );
    ptk_file_browser_set_single_click( file_browser, app_settings.single_click );
    // FIXME: this shouldn't be hard-code
    ptk_file_browser_set_single_click_timeout( file_browser,
                    app_settings.no_single_hover ? 0 : SINGLE_CLICK_TIMEOUT );
    ptk_file_browser_show_thumbnails( file_browser,
                    app_settings.show_thumbnail ? app_settings.max_thumb_size : 0 );

    ptk_file_browser_set_sort_order( file_browser, 
            xset_get_int_panel( file_browser->mypanel, "list_detailed", "x" ) );
    ptk_file_browser_set_sort_type( file_browser,
            xset_get_int_panel( file_browser->mypanel, "list_detailed", "y" ) );

    gtk_widget_show( GTK_WIDGET( file_browser ) );

/*
    g_signal_connect( file_browser, "before-chdir",
                      G_CALLBACK( on_file_browser_before_chdir ), main_window );
*/
    g_signal_connect( file_browser, "begin-chdir",
                      G_CALLBACK( on_file_browser_begin_chdir ), main_window );
    g_signal_connect( file_browser, "content-change",
                      G_CALLBACK( on_file_browser_content_change ), main_window );
    g_signal_connect( file_browser, "after-chdir",
                      G_CALLBACK( on_file_browser_after_chdir ), main_window );
    g_signal_connect( file_browser, "open-item",
                      G_CALLBACK( on_file_browser_open_item ), main_window );
    g_signal_connect( file_browser, "sel-change",
                      G_CALLBACK( on_file_browser_sel_change ), main_window );
    g_signal_connect( file_browser, "pane-mode-change",
                      G_CALLBACK( on_file_browser_panel_change ), main_window );

    tab_label = fm_main_window_create_tab_label( main_window, file_browser );
    idx = gtk_notebook_append_page( GTK_NOTEBOOK( notebook ),
                                        GTK_WIDGET( file_browser ), tab_label );
    gtk_notebook_set_tab_reorderable( GTK_NOTEBOOK( notebook ), GTK_WIDGET( file_browser ), TRUE );
    gtk_notebook_set_current_page ( GTK_NOTEBOOK( notebook ), idx );

    if (app_settings.always_show_tabs)
        gtk_notebook_set_show_tabs( GTK_NOTEBOOK( notebook ), TRUE );
    else
        if ( gtk_notebook_get_n_pages ( GTK_NOTEBOOK( notebook ) ) > 1 )
            gtk_notebook_set_show_tabs( GTK_NOTEBOOK( notebook ), TRUE );
        else
            gtk_notebook_set_show_tabs( GTK_NOTEBOOK( notebook ), FALSE );

    if ( !ptk_file_browser_chdir( file_browser, folder_path, PTK_FB_CHDIR_ADD_HISTORY ) )
        ptk_file_browser_chdir( file_browser, "/", PTK_FB_CHDIR_ADD_HISTORY );

    if ( evt_tab_new->s || evt_tab_new->ob2_data )
        main_window_event( main_window, evt_tab_new, "evt_tab_new", 0, 0, NULL,
                                                                0, 0, 0, TRUE );

    set_panel_focus( main_window, file_browser );
//    while( gtk_events_pending() )  // wait for chdir to grab focus
//        gtk_main_iteration();
    //gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );
//printf("focus browser #%d %d\n", idx, file_browser->folder_view );
//printf("call delayed (newtab) #%d %#x\n", idx, file_browser->folder_view);
//    g_idle_add( ( GSourceFunc ) delayed_focus, file_browser->folder_view );
}

GtkWidget* fm_main_window_new()
{
    return ( GtkWidget* ) g_object_new ( FM_TYPE_MAIN_WINDOW, NULL );
}

GtkWidget* fm_main_window_get_current_file_browser ( FMMainWindow* main_window )
{
    if ( !main_window )
    {
        main_window = fm_main_window_get_last_active();
        if ( !main_window )
            return NULL;
    }
    if ( main_window->notebook )
    {
        gint idx = gtk_notebook_get_current_page( GTK_NOTEBOOK( main_window->notebook ) );
        if ( idx >= 0 )
            return gtk_notebook_get_nth_page( GTK_NOTEBOOK( main_window->notebook ), idx );
    }
    return NULL;
}

void
on_preference_activate ( GtkMenuItem *menuitem,
                         gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    fm_main_window_preference( main_window );
}

void fm_main_window_preference( FMMainWindow* main_window )
{
    fm_edit_preference( (GtkWindow*)main_window, PREF_GENERAL );
}


#if 0
void
on_file_assoc_activate ( GtkMenuItem *menuitem,
                         gpointer user_data )
{
    GtkWindow * main_window = GTK_WINDOW( user_data );
    edit_file_associations( main_window );
}
#endif

/* callback used to open default browser when URLs got clicked */
static void open_url( GtkAboutDialog *dlg, const gchar *url, gpointer data)
{
    xset_open_url( GTK_WIDGET( dlg ), url );
#if 0
    /* FIXME: is there any better way to do this? */
    char* programs[] = { "xdg-open", "gnome-open" /* Sorry, KDE users. :-P */, "exo-open" };
    int i;
    for(  i = 0; i < G_N_ELEMENTS(programs); ++i)
    {
        char* open_cmd = NULL;
        if( (open_cmd = g_find_program_in_path( programs[i] )) )
        {
             char* cmd = g_strdup_printf( "%s \'%s\'", open_cmd, url );
             g_spawn_command_line_async( cmd, NULL );
             g_free( cmd );
             g_free( open_cmd );
             break;
        }
    }
#endif
}

void on_main_help_activate ( GtkMenuItem *menuitem, FMMainWindow* main_window )
{
    const char* help;
    
    PtkFileBrowser* browser = PTK_FILE_BROWSER( 
                        fm_main_window_get_current_file_browser( main_window ) );
    if ( browser && browser->path_bar && gtk_widget_has_focus( 
                                                GTK_WIDGET( browser->path_bar ) ) )
        help = "#gui-pathbar";
    else if ( browser && browser->side_dev && gtk_widget_has_focus( 
                                                GTK_WIDGET( browser->side_dev ) ) )
        help = "#devices";
    else if ( browser && browser->side_book && gtk_widget_has_focus( 
                                                GTK_WIDGET( browser->side_book ) ) )
        help = "#gui-book";
    else if ( main_window->task_view && 
                    gtk_widget_has_focus( GTK_WIDGET( main_window->task_view ) ) )
        help = "#tasks-man";
    else
        help = NULL;
    xset_show_help( GTK_WIDGET( main_window ), NULL, help );
}

void on_main_faq ( GtkMenuItem *menuitem, FMMainWindow* main_window )
{
    xset_show_help( GTK_WIDGET( main_window ), NULL, "#quickstart-faq" );
}

void on_homepage_activate ( GtkMenuItem *menuitem, FMMainWindow* main_window )
{
    xset_open_url( GTK_WIDGET( main_window ), NULL );
}

void on_news_activate ( GtkMenuItem *menuitem, FMMainWindow* main_window )
{
    xset_open_url( GTK_WIDGET( main_window ), "http://ignorantguru.github.io/spacefm/news.html" );
}

void on_getplug_activate ( GtkMenuItem *menuitem, FMMainWindow* main_window )
{
    xset_open_url( GTK_WIDGET( main_window ), "https://github.com/IgnorantGuru/spacefm/wiki/plugins/" );
}

void
on_about_activate ( GtkMenuItem *menuitem,
                    gpointer user_data )
{
    static GtkWidget * about_dlg = NULL;
    GtkBuilder* builder = gtk_builder_new();
    if( ! about_dlg )
    {
        GtkBuilder* builder;

        pcmanfm_ref();
        builder = _gtk_builder_new_from_file( PACKAGE_UI_DIR "/about-dlg.ui", NULL );
        about_dlg = GTK_WIDGET( gtk_builder_get_object( builder, "dlg" ) );
        g_object_unref( builder );
        gtk_about_dialog_set_version ( GTK_ABOUT_DIALOG ( about_dlg ), VERSION );

        char* name;
        XSet* set = xset_get( "main_icon" );
        if ( set->icon )
            name = set->icon;
        else if ( geteuid() == 0 )
            name = "spacefm-root";
        else
            name = "spacefm";
        gtk_about_dialog_set_logo_icon_name( GTK_ABOUT_DIALOG ( about_dlg ), name );

        g_object_add_weak_pointer( G_OBJECT(about_dlg), (gpointer)&about_dlg );
        g_signal_connect( about_dlg, "response", G_CALLBACK(gtk_widget_destroy), NULL );
        g_signal_connect( about_dlg, "destroy", G_CALLBACK(pcmanfm_unref), NULL );
        g_signal_connect( about_dlg, "activate-link", G_CALLBACK(open_url), NULL );
    }
    gtk_window_set_transient_for( GTK_WINDOW( about_dlg ), GTK_WINDOW( user_data ) );
    gtk_window_present( (GtkWindow*)about_dlg );
}


#if 0
gboolean
on_back_btn_popup_menu ( GtkWidget *widget,
                         gpointer user_data )
{
    
    //GtkWidget* file_browser = fm_main_window_get_current_file_browser( widget );
    
    return FALSE;
}
#endif


#if 0
gboolean
on_forward_btn_popup_menu ( GtkWidget *widget,
                            gpointer user_data )
{
    
    //GtkWidget* file_browser = fm_main_window_get_current_file_browser( widget );
    
    return FALSE;
}
#endif

void fm_main_window_add_new_window( FMMainWindow* main_window )
{
    if ( main_window && !main_window->maximized && !main_window->fullscreen )
    {
        // use current main_window's size for new window
        GtkAllocation allocation;
        gtk_widget_get_allocation ( GTK_WIDGET( main_window ), &allocation);
        if ( allocation.width > 0 )
        {
            app_settings.width = allocation.width;
            app_settings.height = allocation.height;
        }
    }
    GtkWidget* new_win = fm_main_window_new();
}

void
on_new_window_activate ( GtkMenuItem *menuitem,
                         gpointer user_data )
{
    FMMainWindow* main_window = FM_MAIN_WINDOW( user_data );

    xset_autosave_cancel();
    fm_main_window_store_positions( main_window );
    save_settings( main_window );

/* this works - enable if desired
    // open active tabs only
    for ( p = 1; p < 5; p++ )
    {
        set = xset_get_panel( p, "show" );
        if ( set->b == XSET_B_TRUE )
        {
            // set preload tab to current
            cur_tabx = gtk_notebook_get_current_page( GTK_NOTEBOOK( main_window->panel[p-1] ) );
            a_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page( 
                            GTK_NOTEBOOK( main_window->panel[p-1] ), cur_tabx ) );
            if ( GTK_IS_WIDGET( a_browser ) )
                set->ob1 = g_strdup( ptk_file_browser_get_cwd( a_browser ) );
            // clear other saved tabs
            g_free( set->s );
            set->s = NULL;
        }
    }
*/
    fm_main_window_add_new_window( main_window );
}

static gboolean delayed_focus( GtkWidget* widget )
{
    if ( GTK_IS_WIDGET( widget ) )
    {
        ///gdk_threads_enter();
//printf( "delayed_focus %#x\n", widget);
        if ( GTK_IS_WIDGET( widget ) )
            gtk_widget_grab_focus( widget );
        ///gdk_threads_leave();
    }
    return FALSE;
}

static gboolean delayed_focus_file_browser( PtkFileBrowser* file_browser )
{
    if ( GTK_IS_WIDGET( file_browser ) && GTK_IS_WIDGET( file_browser->folder_view ) )
    {
        ///gdk_threads_enter();
//printf( "delayed_focus_file_browser fb=%#x\n", file_browser );
        if ( GTK_IS_WIDGET( file_browser ) && GTK_IS_WIDGET( file_browser->folder_view ) )
        {
            gtk_widget_grab_focus( file_browser->folder_view );
            set_panel_focus( NULL, file_browser );
        }
        ///gdk_threads_leave();
    }
    return FALSE;
}

void set_panel_focus( FMMainWindow* main_window, PtkFileBrowser* file_browser )
{
    int p, pages, cur_tabx;
    GtkWidget* notebook;
    PtkFileBrowser* a_browser;
    
    if ( !file_browser && !main_window )
        return;
    
    FMMainWindow* mw = main_window;
    if ( !mw )
        mw = (FMMainWindow*)file_browser->main_window;
        
    if ( file_browser )
        ptk_file_browser_status_change( file_browser, 
                                file_browser->mypanel == mw->curpanel );

    for ( p = 1; p < 5; p++ )
    {
        notebook = mw->panel[p-1];
        pages = gtk_notebook_get_n_pages( GTK_NOTEBOOK( notebook ) );
        for ( cur_tabx = 0; cur_tabx < pages; cur_tabx++ )
        {
            a_browser = PTK_FILE_BROWSER( 
                        gtk_notebook_get_nth_page( GTK_NOTEBOOK( notebook ), cur_tabx ) );
            if ( a_browser != file_browser )
                ptk_file_browser_status_change( a_browser, p == mw->curpanel );
        }
        gtk_widget_set_sensitive( mw->panel_image[p-1],
                                                p == mw->curpanel );
    }
    
    update_window_title( NULL, mw );
    if ( evt_pnl_focus->s || evt_pnl_focus->ob2_data )
        main_window_event( main_window, evt_pnl_focus, "evt_pnl_focus",
                                        mw->curpanel, 0, NULL, 0, 0, 0, TRUE );        
}

void on_fullscreen_activate ( GtkMenuItem *menuitem, FMMainWindow* main_window )
{
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER(
                                    fm_main_window_get_current_file_browser
                                                            ( main_window ) );
    if ( xset_get_b( "main_full" ) )
    {
        if ( file_browser && file_browser->view_mode == PTK_FB_LIST_VIEW )
            ptk_file_browser_save_column_widths( 
                                GTK_TREE_VIEW( file_browser->folder_view ),
                                file_browser );
        gtk_widget_hide( main_window->menu_bar );
        gtk_widget_hide( main_window->panelbar );
        gtk_window_fullscreen( GTK_WINDOW( main_window ) );
        main_window->fullscreen = TRUE;
    }
    else
    {
        main_window->fullscreen = FALSE;
        gtk_window_unfullscreen( GTK_WINDOW( main_window ) );
        gtk_widget_show( main_window->menu_bar );
        if ( xset_get_b( "main_pbar" ) )
            gtk_widget_show( main_window->panelbar );
        
        if ( !main_window->maximized )
            show_panels( NULL, main_window );  // restore columns
    }
}

void set_window_title( FMMainWindow* main_window, PtkFileBrowser* file_browser )
{
    char* disp_path;
    char* disp_name;
    char* tab_count = NULL;
    char* tab_num = NULL;
    char* panel_count = NULL;
    char* panel_num = NULL;
    char* s;
    
    if ( !file_browser || !main_window )
        return;
        
    if ( file_browser->dir && file_browser->dir->disp_path )
    {
        disp_path = g_strdup( file_browser->dir->disp_path );
        disp_name = g_path_get_basename( disp_path );
    }
    else
    {
        const char* path = ptk_file_browser_get_cwd( file_browser );
        if ( path )
        {
            disp_path = g_filename_display_name( path );
            disp_name = g_path_get_basename( disp_path );
        }
        else
        {
            disp_name = g_strdup( "" );
            disp_path = g_strdup( "" );
        }
    }

    char* orig_fmt = xset_get_s( "main_title" );
    char* fmt = g_strdup( orig_fmt );
    if ( !fmt )
        fmt = g_strdup( "%d" );
        
    if ( strstr( fmt, "%t" ) || strstr( fmt, "%T" ) || strstr( fmt, "%p" )
                                                        || strstr( fmt, "%P" ) )
    {
        // get panel/tab info
        int ipanel_count = 0, itab_count = 0, itab_num = 0;
        main_window_get_counts( file_browser, &ipanel_count, &itab_count, &itab_num );
        panel_count = g_strdup_printf( "%d", ipanel_count );
        tab_count = g_strdup_printf( "%d", itab_count );
        tab_num = g_strdup_printf( "%d", itab_num );
        panel_count = g_strdup_printf( "%d", ipanel_count );
        panel_num = g_strdup_printf( "%d", main_window->curpanel );
        s = replace_string( fmt, "%t", tab_num, FALSE );
        g_free( fmt );
        fmt = replace_string( s, "%T", tab_count, FALSE );
        g_free( s );
        s = replace_string( fmt, "%p", panel_num, FALSE );
        g_free( fmt );
        fmt = replace_string( s, "%P", panel_count, FALSE );
        g_free( panel_count );
        g_free( tab_count );
        g_free( tab_num );
        g_free( panel_num );
    }

    if ( strchr( fmt, '*' ) && !main_tasks_running( main_window ) )
    {
        s = fmt;
        fmt = replace_string( s, "*", "", FALSE );
        g_free( s );
    }

    if ( strstr( fmt, "%n" ) )
    {
        s = fmt;
        fmt = replace_string( s, "%n", disp_name, FALSE );
        g_free( s );
    }
    if ( orig_fmt && strstr( orig_fmt, "%d" ) )
    {
        s = fmt;
        fmt = replace_string( s, "%d", disp_path, FALSE );
        g_free( s );
    }
    g_free( disp_name );
    g_free( disp_path );
    
    gtk_window_set_title( GTK_WINDOW( main_window ), fmt );
    g_free( fmt );

/*
    if ( file_browser->dir && ( disp_path = file_browser->dir->disp_path ) )
    {
        disp_name = g_path_get_basename( disp_path );
        //gtk_entry_set_text( main_window->address_bar, disp_path );
        gtk_window_set_title( GTK_WINDOW( main_window ), disp_name );
        g_free( disp_name );
    }
    else
    {
        char* path = ptk_file_browser_get_cwd( file_browser );
        if ( path )
        {
            disp_path = g_filename_display_name( path );
            //gtk_entry_set_text( main_window->address_bar, disp_path );
            disp_name = g_path_get_basename( disp_path );
            g_free( disp_path );
            gtk_window_set_title( GTK_WINDOW( main_window ), disp_name );
            g_free( disp_name );
        }
    }
*/
}

void update_window_title( GtkMenuItem* item, FMMainWindow* main_window )
{
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser
                                                                ( main_window ) );
    if ( file_browser )
        set_window_title( main_window, file_browser );
}

void
on_folder_notebook_switch_pape ( GtkNotebook *notebook,
                                 GtkWidget *page,
                                 guint page_num,
                                 gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser;
    const char* path;
    char* disp_path;

    // save sliders of current fb ( new tab while task manager is shown changes vals )
    PtkFileBrowser* curfb = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser
                                                    ( main_window ) );
    if ( curfb )
    {
        ptk_file_browser_slider_release( NULL, NULL, curfb );
        if ( curfb->view_mode == PTK_FB_LIST_VIEW )
            ptk_file_browser_save_column_widths(
                                    GTK_TREE_VIEW( curfb->folder_view ),
                                    curfb );
    }
    
    file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page( notebook, page_num ) );
//printf("on_folder_notebook_switch_pape fb=%p   panel=%d   page=%d\n", file_browser, file_browser->mypanel, page_num );
    main_window->curpanel = file_browser->mypanel;
    main_window->notebook = main_window->panel[main_window->curpanel - 1];

    fm_main_window_update_status_bar( main_window, file_browser );

    set_window_title( main_window, file_browser );

    if ( evt_tab_focus->ob2_data || evt_tab_focus->s )
        main_window_event( main_window, evt_tab_focus, "evt_tab_focus",
                                        main_window->curpanel,
                                        page_num + 1, NULL, 0, 0, 0, TRUE );

    ptk_file_browser_update_views( NULL, file_browser );
    
    if ( GTK_IS_WIDGET( file_browser ) )
        g_idle_add( ( GSourceFunc ) delayed_focus, file_browser->folder_view );
}

void main_window_open_path_in_current_tab( FMMainWindow* main_window, const char* path )
{
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( 
                        fm_main_window_get_current_file_browser( main_window ) );
    if ( !file_browser )
        return;
    ptk_file_browser_chdir( file_browser, path, PTK_FB_CHDIR_ADD_HISTORY );
}

void main_window_open_network( FMMainWindow* main_window, const char* path,
                                                            gboolean new_tab )
{
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( 
                        fm_main_window_get_current_file_browser( main_window ) );
    if ( !file_browser )
        return;
    char* str = g_strdup( path );
    ptk_location_view_mount_network( file_browser, str, new_tab, FALSE );
    g_free( str );
}

void on_file_browser_open_item( PtkFileBrowser* file_browser,
                                const char* path, PtkOpenAction action,
                                FMMainWindow* main_window )
{
    if ( G_LIKELY( path ) )
    {
        switch ( action )
        {
        case PTK_OPEN_DIR:
            ptk_file_browser_chdir( file_browser, path, PTK_FB_CHDIR_ADD_HISTORY );
            break;
        case PTK_OPEN_NEW_TAB:
            //file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
            fm_main_window_add_new_tab( main_window, path );
            break;
        case PTK_OPEN_NEW_WINDOW:
            //file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
            //fm_main_window_add_new_window( main_window, path,
            //                               file_browser->show_side_pane,
             //                              file_browser->side_pane_mode );
            break;
        case PTK_OPEN_TERMINAL:
            //fm_main_window_open_terminal( GTK_WINDOW(main_window), path );
            break;
        case PTK_OPEN_FILE:
            //fm_main_window_start_busy_task( main_window );
            //g_timeout_add( 1000, ( GSourceFunc ) fm_main_window_stop_busy_task, main_window );
            break;
        }
    }
}

void fm_main_window_update_status_bar( FMMainWindow* main_window,
                                       PtkFileBrowser* file_browser )
{
    int num_sel, num_vis, num_hid, num_hidx;
    guint64 total_size;
    char *msg;
    char size_str[ 64 ];
    char free_space[100];
#ifdef HAVE_STATVFS
    struct statvfs fs_stat = {0};
#endif

    if ( !( GTK_IS_WIDGET( file_browser ) && GTK_IS_STATUSBAR( file_browser->status_bar ) ) )
        return;
        //file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    
    if ( file_browser->status_bar_custom )
    {
        gtk_statusbar_push( GTK_STATUSBAR( file_browser->status_bar ), 0,
                                           file_browser->status_bar_custom );
        return;
    }

    free_space[0] = '\0';
#ifdef HAVE_STATVFS
// FIXME: statvfs support should be moved to src/vfs

    if( statvfs( ptk_file_browser_get_cwd(file_browser), &fs_stat ) == 0 )
    {
        char total_size_str[ 64 ];
        // calc free space
        vfs_file_size_to_string_format( size_str,
                                    fs_stat.f_bsize * fs_stat.f_bavail, NULL );        
        // calc total space
        vfs_file_size_to_string_format( total_size_str,
                                    fs_stat.f_frsize * fs_stat.f_blocks, NULL );
        g_snprintf( free_space, G_N_ELEMENTS(free_space),
                    _(" %s free / %s   "), size_str, total_size_str );  //MOD
    }
#endif

    // Show Reading... while still loading
    if ( file_browser->busy )
    {
        msg = g_strdup_printf( _("%sReading %s ..."), free_space,
                                ptk_file_browser_get_cwd(file_browser) );
        gtk_statusbar_push( GTK_STATUSBAR( file_browser->status_bar ), 0,
                                           msg );
        g_free( msg );
        return;
    }

    // note: total size won't include content changes since last selection change
    num_sel = ptk_file_browser_get_n_sel( file_browser, &total_size );
    num_vis = ptk_file_browser_get_n_visible_files( file_browser );

    char* link_info = NULL;  //MOD added
    if ( num_sel > 0 )
    {
        if ( num_sel == 1 )  //MOD added
        // display file name or symlink info in status bar if one file selected
        {
            GList* files;
            VFSFileInfo* file;
            struct stat results;
            char buf[ 64 ];
            char* lsize;
            
            files = ptk_file_browser_get_selected_files( file_browser );
            if ( files )
            {
                const char* cwd = ptk_file_browser_get_cwd( file_browser );
                file = vfs_file_info_ref( ( VFSFileInfo* ) files->data );
                g_list_foreach( files, ( GFunc ) vfs_file_info_unref, NULL );
                g_list_free( files );
                if ( file )
                {
                    if ( vfs_file_info_is_symlink( file ) )
                    {
                        char* full_target = NULL;
                        char* target_path;
                        char* file_path = g_build_filename( cwd,
                                                  vfs_file_info_get_name( file ), NULL );
                        char* target = g_file_read_link( file_path, NULL );
                        if ( target )
                        {                                
                            //printf("LINK: %s\n", file_path );
                            if ( target[0] != '/' )
                            {
                                // relative link
                                full_target = g_build_filename( cwd, target, NULL );
                                target_path = full_target;
                            }
                            else
                                target_path = target;
                                
                            if ( vfs_file_info_is_dir( file ) )
                            {
                                if ( g_file_test( target_path, G_FILE_TEST_EXISTS ) )
                                {
                                    if ( !strcmp( target, "/" ) )
                                        link_info = g_strdup_printf( _("   Link → %s"), target );
                                    else
                                        link_info = g_strdup_printf( _("   Link → %s/"), target );
                                }
                                else
                                    link_info = g_strdup_printf( _("   !Link → %s/ (missing)"), target );
                            }
                            else
                            {
                                if ( stat( target_path, &results ) == 0 )
                                {
                                    vfs_file_size_to_string( buf, results.st_size );
                                    lsize = g_strdup( buf );
                                    link_info = g_strdup_printf( _("   Link → %s (%s)"), target, lsize );
                                }
                                else
                                    link_info = g_strdup_printf( _("   !Link → %s (missing)"), target );
                            }
                            g_free( target );
                            if ( full_target )
                                g_free( full_target );
                        }
                        else
                            link_info = g_strdup_printf( _("   !Link → ( error reading target )") );
                            
                        g_free( file_path );
                    }
                    else
                        link_info = g_strdup_printf( "   %s", vfs_file_info_get_name( file ) );
                    vfs_file_info_unref( file );
                }
            }
        }
        if ( ! link_info )
            link_info = g_strdup( "" );
            
        vfs_file_size_to_string( size_str, total_size );
        msg = g_strdup_printf( "%s%d / %d (%s)%s", free_space, num_sel, num_vis,
                                                    size_str, link_info );
        //msg = g_strdup_printf( ngettext( _("%s%d sel (%s)%s"),  //MOD
        //                 _("%s%d sel (%s)"), num_sel ), free_space, num_sel,
        //                                        size_str, link_info );  //MOD
    }
    else
    {
        // cur dir is link ?  canonicalize
        char* dirmsg;
        const char* cwd = ptk_file_browser_get_cwd( file_browser );
        char buf[ PATH_MAX + 1 ];
        char* canon = realpath( cwd, buf );
        if ( !canon || !g_strcmp0( canon, cwd ) )
            dirmsg = g_strdup_printf( "%s", cwd );
        else
            dirmsg = g_strdup_printf( "./ → %s", canon );

        // MOD add count for .hidden files
        char* xhidden;
        num_hid = ptk_file_browser_get_n_all_files( file_browser ) - num_vis;
        if ( num_hid < 0 ) num_hid = 0;  // needed due to bug in get_n_visible_files?
        num_hidx = file_browser->dir ? file_browser->dir->xhidden_count : 0;
        //VFSDir* xdir = file_browser->dir;
        if ( num_hid || num_hidx )
        {
            if ( num_hidx )
                xhidden = g_strdup_printf( "+%d", num_hidx );
            else
                xhidden = g_strdup( "" );

            char hidden[128];
            char *hnc = NULL;
            char* hidtext = ngettext( "hidden", "hidden", num_hid);
            g_snprintf( hidden, 127, g_strdup_printf( "%d%s %s", num_hid,
                                                xhidden, hidtext ), num_hid );
            msg = g_strdup_printf( ngettext( "%s%d visible (%s)   %s",
                                             "%s%d visible (%s)   %s", num_vis ),
                                             free_space, num_vis, hidden, dirmsg );
        }
        else
            msg = g_strdup_printf( ngettext( "%s%d item   %s",
                                             "%s%d items   %s", num_vis ),
                                                  free_space, num_vis, dirmsg );
        g_free( dirmsg );
    }
    gtk_statusbar_push( GTK_STATUSBAR( file_browser->status_bar ), 0, msg );
    g_free( msg );
}

void on_file_browser_panel_change( PtkFileBrowser* file_browser,
                                 FMMainWindow* main_window )
{
//printf("panel_change  panel %d\n", file_browser->mypanel );
    main_window->curpanel = file_browser->mypanel;
    main_window->notebook = main_window->panel[main_window->curpanel - 1];
    //set_window_title( main_window, file_browser );
    set_panel_focus( main_window, file_browser );
}

void on_file_browser_sel_change( PtkFileBrowser* file_browser,
                                 FMMainWindow* main_window )
{
//printf("sel_change  panel %d\n", file_browser->mypanel );
    if ( ( evt_pnl_sel->ob2_data || evt_pnl_sel->s ) &&
            main_window_event( main_window, evt_pnl_sel, "evt_pnl_sel", 0, 0, NULL,
                                                            0, 0, 0, TRUE ) )
        return;
    fm_main_window_update_status_bar( main_window, file_browser );
/*
    int i = gtk_notebook_get_current_page( main_window->panel[ 
                                                    file_browser->mypanel - 1 ] );
    if ( i != -1 )
    {
        if ( file_browser == (PtkFileBrowser*)gtk_notebook_get_nth_page(
                            main_window->panel[ file_browser->mypanel - 1 ], i ) )
            fm_main_window_update_status_bar( main_window, file_browser );
    }
*/
//    main_window->curpanel = file_browser->mypanel;
//    main_window->notebook = main_window->panel[main_window->curpanel - 1];

//    if ( fm_main_window_get_current_file_browser( main_window ) == GTK_WIDGET( file_browser ) )
}

void on_file_browser_content_change( PtkFileBrowser* file_browser,
                                     FMMainWindow* main_window )
{
//printf("content_change  panel %d\n", file_browser->mypanel );
    fm_main_window_update_status_bar( main_window, file_browser );
}

gboolean on_tab_drag_motion ( GtkWidget *widget,
                              GdkDragContext *drag_context,
                              gint x,
                              gint y,
                              guint time,
                              PtkFileBrowser* file_browser )
{
    GtkNotebook * notebook;
    gint idx;

    notebook = GTK_NOTEBOOK( gtk_widget_get_parent( GTK_WIDGET( file_browser ) ) );
    // TODO: Add a timeout here and don't set current page immediately
    idx = gtk_notebook_page_num ( notebook, GTK_WIDGET( file_browser ) );
    gtk_notebook_set_current_page( notebook, idx );
    return FALSE;
}

gboolean on_window_button_press_event( GtkWidget* widget, GdkEventButton *event,
                                       FMMainWindow* main_window )     //sfm
{
    if ( event->type != GDK_BUTTON_PRESS )
        return FALSE;
    
    // handle mouse back/forward buttons anywhere in the main window
    if ( event->button == 4 || event->button == 5 ||
         event->button == 8 || event->button == 9 )   //sfm
    {
        PtkFileBrowser* file_browser = PTK_FILE_BROWSER( 
                    fm_main_window_get_current_file_browser( main_window ) );
        if ( !file_browser )
            return FALSE;
        if ( event->button == 4 || event->button == 8 )
            ptk_file_browser_go_back( NULL, file_browser );
        else
            ptk_file_browser_go_forward( NULL, file_browser );
        return TRUE;
    }
    return FALSE;
}

gboolean on_main_window_focus( GtkWidget* main_window,
                               GdkEventFocus *event,
                               gpointer user_data )
{
    //this causes a widget not realized loop by running rebuild_menus while
    //rebuild_menus is already running
    // but this unneeded anyway?  cross-window menu changes seem to work ok
    //rebuild_menus( main_window );  // xset may change in another window

    GList * active;
    if ( all_windows->data != ( gpointer ) main_window )
    {
        active = g_list_find( all_windows, main_window );
        if ( active )
        {
            all_windows = g_list_remove_link( all_windows, active );
            all_windows->prev = active;
            active->next = all_windows;
            all_windows = active;
        }
    }
    if ( evt_win_focus->s || evt_win_focus->ob2_data )
        main_window_event( (FMMainWindow*)main_window, evt_win_focus,
                                    "evt_win_focus", 0, 0, NULL, 0, 0, 0, TRUE );    
    return FALSE;
}

static gboolean on_main_window_keypress( FMMainWindow* main_window,
                                         GdkEventKey* event, XSet* known_set )
{
//printf("main_keypress %d %d\n", event->keyval, event->state );

    GList* l;
    XSet* set;
    PtkFileBrowser* browser;
    guint nonlatin_key = 0;
    
    if ( known_set )
    {
        set = known_set;
        goto _key_found;
    }

    if ( event->keyval == 0 )
        return FALSE;

    int keymod = ( event->state & ( GDK_SHIFT_MASK | GDK_CONTROL_MASK |
                 GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK ) );

    if (       ( event->keyval == GDK_KEY_Home &&  ( keymod == 0 || 
                                                     keymod == GDK_SHIFT_MASK ) )
            || ( event->keyval == GDK_KEY_End &&   ( keymod == 0 || 
                                                     keymod == GDK_SHIFT_MASK ) )
            || ( event->keyval == GDK_KEY_Delete  && keymod == 0 )
            || ( event->keyval == GDK_KEY_Tab     && keymod == 0 )
            || ( keymod == 0 && ( event->keyval == GDK_KEY_Return || 
                                  event->keyval == GDK_KEY_KP_Enter ) )
            || ( event->keyval == GDK_KEY_Left &&  ( keymod == 0 || 
                                                     keymod == GDK_SHIFT_MASK ) )
            || ( event->keyval == GDK_KEY_Right && ( keymod == 0 || 
                                                     keymod == GDK_SHIFT_MASK ) )
            || ( event->keyval == GDK_KEY_BackSpace && keymod == 0 )
            || ( keymod == 0 && event->keyval != GDK_KEY_Escape && 
                            gdk_keyval_to_unicode( event->keyval ) ) ) // visible char
    {
        browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
        if ( browser && browser->path_bar && gtk_widget_has_focus( 
                                                GTK_WIDGET( browser->path_bar ) ) )
            return FALSE;  // send to pathbar
    }

    // need to transpose nonlatin keyboard layout ?
    if ( !( ( GDK_KEY_0 <= event->keyval && event->keyval <= GDK_KEY_9 ) ||
            ( GDK_KEY_A <= event->keyval && event->keyval <= GDK_KEY_Z ) ||
            ( GDK_KEY_a <= event->keyval && event->keyval <= GDK_KEY_z ) ) )
    {
        nonlatin_key = event->keyval;
        transpose_nonlatin_keypress( event );
    }

    if ( ( evt_win_key->s || evt_win_key->ob2_data ) && 
            main_window_event( main_window, evt_win_key, "evt_win_key", 0, 0, NULL,
                                            event->keyval, 0, keymod, TRUE ) )
        return TRUE;

    for ( l = xsets; l; l = l->next )
    {
        if ( ((XSet*)l->data)->shared_key )
        {
            // set has shared key
            // nonlatin key match is for nonlatin keycodes set prior to 1.0.3
            set = xset_get( ((XSet*)l->data)->shared_key );
            if ( ( set->key == event->keyval ||
                            ( nonlatin_key && set->key == nonlatin_key ) ) &&
                        set->keymod == keymod )
            {
                // shared key match
                if ( g_str_has_prefix( set->name, "panel" ) )
                {
                    // use current panel's set
                    browser = PTK_FILE_BROWSER( 
                                fm_main_window_get_current_file_browser( main_window ) );
                    if ( browser )
                    {
                        char* new_set_name = g_strdup_printf( "panel%d%s",
                                                    browser->mypanel, set->name + 6 );
                        set = xset_get( new_set_name );
                        g_free( new_set_name );
                    }
                    else
                        return FALSE;  // failsafe
                }
                goto _key_found;  // for speed
            }
            else
                continue;
        }
        
        // nonlatin key match is for nonlatin keycodes set prior to 1.0.3
        if ( ( ((XSet*)l->data)->key == event->keyval ||
               ( nonlatin_key && ((XSet*)l->data)->key == nonlatin_key ) )
                                        && ((XSet*)l->data)->keymod == keymod )
        {
            set = (XSet*)l->data;
_key_found:
            browser = PTK_FILE_BROWSER( 
                        fm_main_window_get_current_file_browser( main_window ) );
            if ( !browser )
                return TRUE;
            
            char* xname;
            int i;

            // special edit items
            if ( !strcmp( set->name, "edit_cut" )
                        || !strcmp( set->name, "edit_copy" )
                        || !strcmp( set->name, "edit_delete" )
                        || !strcmp( set->name, "select_all" ) )
            {
                if ( !gtk_widget_is_focus( browser->folder_view ) )
                    return FALSE;
            }
            else if ( !strcmp( set->name, "edit_paste" ) )
            {
                gboolean side_dir_focus = ( browser->side_dir
                                    && gtk_widget_is_focus( 
                                    GTK_WIDGET( browser->side_dir ) ) );
                if ( !gtk_widget_is_focus( GTK_WIDGET( browser->folder_view ) ) 
                                                            && !side_dir_focus )
                    return FALSE;
            }                

            // run menu_cb
            if ( set->menu_style < XSET_MENU_SUBMENU )
            {
                set->browser = browser;
                xset_menu_cb( NULL, set );  // also does custom activate
            }
            if ( !set->lock )
                return TRUE;
                
            // handlers
            if ( g_str_has_prefix( set->name, "dev_" ) )
#ifndef HAVE_HAL
                ptk_location_view_on_action( GTK_WIDGET( browser->side_dev ), set );
#else
g_warning( _("Device manager key shortcuts are disabled in HAL mode") );
#endif
            else if ( g_str_has_prefix( set->name, "main_" ) )
            {
                xname = set->name + 5;
                if ( !strcmp( xname, "new_window" ) )
                    on_new_window_activate( NULL, main_window );
                else if ( !strcmp( xname, "root_window" ) )
                    on_open_current_folder_as_root( NULL, main_window );
                else if ( !strcmp( xname, "search" ) )
                    on_find_file_activate( NULL, main_window );
                else if ( !strcmp( xname, "terminal" ) )
                    on_open_terminal_activate( NULL, main_window );
                else if ( !strcmp( xname, "root_terminal" ) )
                    on_open_root_terminal_activate( NULL, main_window );
                else if ( !strcmp( xname, "save_session" ) )
                    on_open_url( NULL, main_window );
                else if ( !strcmp( xname, "exit" ) )
                    on_quit_activate( NULL, main_window );
                else if ( !strcmp( xname, "full" ) )
                {
                    xset_set_b( "main_full", !main_window->fullscreen );
                    on_fullscreen_activate( NULL, main_window );
                }
                else if ( !strcmp( xname, "prefs" ) )
                    on_preference_activate( NULL, main_window );
                else if ( !strcmp( xname, "design_mode" ) )
                    main_design_mode( NULL, main_window );
                else if ( !strcmp( xname, "pbar" ) )
                    show_panels_all_windows( NULL, main_window );
                else if ( !strcmp( xname, "icon" ) )
                    on_main_icon();
                else if ( !strcmp( xname, "title" ) )
                    update_window_title( NULL, main_window );
                else if ( !strcmp( xname, "about" ) )
                    on_about_activate( NULL, main_window );
                else if ( !strcmp( xname, "help" ) )
                    on_main_help_activate( NULL, main_window );
                else if ( !strcmp( xname, "faq" ) )
                    on_main_faq( NULL, main_window );
                else if ( !strcmp( xname, "homepage" ) )
                    on_homepage_activate( NULL, main_window );
                else if ( !strcmp( xname, "news" ) )
                    on_news_activate( NULL, main_window );
                else if ( !strcmp( xname, "getplug" ) )
                    on_getplug_activate( NULL, main_window );
            }
            else if ( g_str_has_prefix( set->name, "panel_" ) )
            {
                xname = set->name + 6;
                if ( !strcmp( xname, "prev" ) )
                    i = -1;
                else if ( !strcmp( xname, "next" ) )
                    i = -2;
                else if ( !strcmp( xname, "hide" ) )
                    i = -3;
                else
                    i = atoi( xname );                
                focus_panel( NULL, main_window, i );
            }
            else if ( g_str_has_prefix( set->name, "plug_" ) )
                on_plugin_install( NULL, main_window, set );
            else if ( g_str_has_prefix( set->name, "task_" ) )
            {
                xname = set->name + 5;
                if ( strstr( xname, "_manager" ) )
                    on_task_popup_show( NULL, main_window, set->name );
                else if ( !strcmp( xname, "col_reorder" ) )
                    on_reorder( NULL, GTK_WIDGET( browser->task_view ) );
                else if ( g_str_has_prefix( xname, "col_" ) )
                    on_task_column_selected( NULL, browser->task_view );
                else if ( g_str_has_prefix( xname, "stop" ) 
                       || g_str_has_prefix( xname, "pause" )
                       || g_str_has_prefix( xname, "que_" )
                       || !strcmp( xname, "que" )
                       || g_str_has_prefix( xname, "resume" ) )
                {
                    PtkFileTask* ptask = get_selected_task( browser->task_view );
                    on_task_stop( NULL, browser->task_view, set, ptask );
                }
                else if ( !strcmp( xname, "showout" ) )
                    show_task_dialog( NULL, browser->task_view );
                else if ( g_str_has_prefix( xname, "err_" ) )
                    on_task_popup_errset( NULL, main_window, set->name );
            }
            else if ( !strcmp( set->name, "font_task" ) )
                main_update_fonts( NULL, browser );
            else if ( !strcmp( set->name, "rubberband" ) )
                main_window_rubberband_all();
            else
                ptk_file_browser_on_action( browser, set->name );
            
            return TRUE;
        }
    }

    if ( nonlatin_key != 0 )
    {
        // use literal keycode for pass-thru, eg for find-as-you-type search
        event->keyval = nonlatin_key;
    }

    if ( ( event->state & GDK_MOD1_MASK ) )
        rebuild_menus( main_window );

    return FALSE;
}

FMMainWindow* fm_main_window_get_last_active()
{
    return all_windows ? FM_MAIN_WINDOW( all_windows->data ) : NULL;
}

const GList* fm_main_window_get_all()
{
    return all_windows;
}

static long get_desktop_index( GtkWindow* win )
{
    long desktop = -1;
    GdkDisplay* display;
    GdkWindow* window = NULL;

    if ( win )
    {
        // get desktop of win
        display = gtk_widget_get_display( GTK_WIDGET( win ) );
        window = gtk_widget_get_window( GTK_WIDGET( win ) );
    }
    else
    {
        // get current desktop
        display = gdk_display_get_default();
#if GTK_CHECK_VERSION (3, 0, 0)
        if ( display && GDK_IS_X11_DISPLAY (display ) )
#else
        if ( display )
#endif
            window = gdk_x11_window_lookup_for_display( display,
                                    gdk_x11_get_default_root_xwindow() );
    }

    if ( !( GDK_IS_DISPLAY( display ) && GDK_IS_WINDOW( window ) ) )
        return desktop;

    // find out what desktop (workspace) window is on   #include <gdk/gdkx.h>
    Atom type;
    gint format;
    gulong nitems;
    gulong bytes_after;
    guchar *data;
    const gchar* atom_name = win ? "_NET_WM_DESKTOP" : "_NET_CURRENT_DESKTOP";
    Atom net_wm_desktop = gdk_x11_get_xatom_by_name_for_display ( display, atom_name );

    if ( net_wm_desktop == None )
        fprintf( stderr, "spacefm: %s atom not found\n", atom_name );
    else if ( XGetWindowProperty( GDK_DISPLAY_XDISPLAY( display ),
                                  GDK_WINDOW_XID( window ), 
                                  net_wm_desktop, 0, 1, 
                                  False, XA_CARDINAL, (Atom*)&type,
                                  &format, &nitems, &bytes_after, 
                                  &data ) != Success || type == None || data == NULL )
    {
        if ( type == None )
            fprintf( stderr, "spacefm: No such property from XGetWindowProperty() %s\n",
                                                                    atom_name );
        else if ( data == NULL )
            fprintf( stderr, "spacefm: No data returned from XGetWindowProperty() %s\n",
                                                                    atom_name );
        else
            fprintf( stderr, "spacefm: XGetWindowProperty() %s failed\n",
                                                                    atom_name );
    }
    else
    {
        desktop = *data;
        XFree( data );
    }
    return desktop;
}

FMMainWindow* fm_main_window_get_on_current_desktop()
{   // find the last used spacefm window on the current desktop
    long desktop;
    long cur_desktop = get_desktop_index( NULL );
    //printf("current_desktop = %ld\n", cur_desktop );
    if ( cur_desktop == -1 )
        return fm_main_window_get_last_active(); // revert to dumb if no current

    GList* l;
    gboolean invalid = FALSE;
    for ( l = all_windows; l; l = l->next )
    {
        desktop = get_desktop_index( GTK_WINDOW( (FMMainWindow*)l->data ) );
        //printf( "    test win %p = %ld\n", (FMMainWindow*)l->data, desktop );
        if ( desktop == cur_desktop || desktop > 254 /* 255 == all desktops */ )
            return (FMMainWindow*)l->data;
        else if ( desktop == -1 && !invalid )
            invalid = TRUE;
    }
    // revert to dumb if one or more window desktops unreadable
    return invalid ? fm_main_window_get_last_active() : NULL;
}

enum {
    TASK_COL_STATUS,
    TASK_COL_COUNT,
    TASK_COL_PATH,
    TASK_COL_FILE,
    TASK_COL_TO,
    TASK_COL_PROGRESS,
    TASK_COL_TOTAL,
    TASK_COL_STARTED,
    TASK_COL_ELAPSED,
    TASK_COL_CURSPEED,
    TASK_COL_CUREST,
    TASK_COL_AVGSPEED,
    TASK_COL_AVGEST,
    TASK_COL_STARTTIME,
    TASK_COL_ICON,
    TASK_COL_DATA
};

const char* task_titles[] =
    {
        // If you change "Status", also change it in on_task_button_press_event
        N_( "Status" ), N_( "#" ), N_( "Folder" ), N_( "Item" ),
        N_( "To" ), N_( "Progress" ), N_( "Total" ),
        N_( "Started" ), N_( "Elapsed" ), N_( "Current" ), N_( "CRemain" ),
        N_( "Average" ), N_( "Remain" ), "StartTime"
    };
const char* task_names[] =
    {
        "task_col_status", "task_col_count",
        "task_col_path", "task_col_file", "task_col_to",
        "task_col_progress", "task_col_total", "task_col_started",
        "task_col_elapsed", "task_col_curspeed", "task_col_curest",
        "task_col_avgspeed", "task_col_avgest"
    };

void on_reorder( GtkWidget* item, GtkWidget* parent )
{
    xset_msg_dialog( parent, 0, _("Reorder Columns Help"), NULL, 0, _("To change the order of the columns, drag the column header to the desired location."), NULL, NULL );
}

void main_context_fill( PtkFileBrowser* file_browser, XSetContext* c )
{
    PtkFileBrowser* a_browser;
    VFSFileInfo* file;
    VFSMimeType* mime_type;
    GtkClipboard* clip = NULL;
    GList* sel_files;
    int no_write_access = 0, no_read_access = 0;
    VFSVolume* vol;
    PtkFileTask* ptask;
    char* path;
    GtkTreeModel* model;
    GtkTreeModel* model_task;
    GtkTreeIter it;

    c->valid = FALSE;
    if ( !GTK_IS_WIDGET( file_browser ) )
        return;

    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;
    if ( !main_window )
        return;

    if ( !c->var[CONTEXT_NAME] )
    {
        // if name is set, assume we don't need all selected files info
        c->var[CONTEXT_DIR] = g_strdup( ptk_file_browser_get_cwd( file_browser ) );
        if ( !c->var[CONTEXT_DIR] )
            c->var[CONTEXT_DIR] = g_strdup( "" );
        else
        {
            c->var[CONTEXT_WRITE_ACCESS] = ptk_file_browser_no_access( 
                                                    c->var[CONTEXT_DIR], NULL ) ?
                                    g_strdup( "false" ) : g_strdup( "true" );
        }
    
        if ( sel_files = ptk_file_browser_get_selected_files( file_browser ) )
            file = vfs_file_info_ref( (VFSFileInfo*)sel_files->data );
        else
            file = NULL;
        if ( !file )
        {
            c->var[CONTEXT_NAME] = g_strdup( "" );
            c->var[CONTEXT_IS_DIR] = g_strdup( "false" );
            c->var[CONTEXT_IS_TEXT] = g_strdup( "false" );
            c->var[CONTEXT_IS_LINK] = g_strdup( "false" );
            c->var[CONTEXT_MIME] = g_strdup( "" );
            c->var[CONTEXT_MUL_SEL] = g_strdup( "false" );
        }
        else
        {
            c->var[CONTEXT_NAME] = g_strdup( vfs_file_info_get_name( file ) );
            path = g_build_filename( c->var[CONTEXT_DIR],
                                                    c->var[CONTEXT_NAME], NULL );
            c->var[CONTEXT_IS_DIR] = path &&
                                    g_file_test( path, G_FILE_TEST_IS_DIR ) ?
                                    g_strdup( "true" ) : g_strdup( "false" );
            c->var[CONTEXT_IS_TEXT] = vfs_file_info_is_text( file, path ) ?
                                    g_strdup( "true" ) : g_strdup( "false" );
            c->var[CONTEXT_IS_LINK] = vfs_file_info_is_symlink( file ) ?
                                    g_strdup( "true" ) : g_strdup( "false" );

            mime_type = vfs_file_info_get_mime_type( file );
            if ( mime_type )
            {
                c->var[CONTEXT_MIME] = g_strdup( vfs_mime_type_get_type( mime_type ) );
                vfs_mime_type_unref( mime_type );
            }
            else
                c->var[CONTEXT_MIME] = g_strdup( "" );
            
            c->var[CONTEXT_MUL_SEL] = sel_files->next ? 
                                        g_strdup( "true" ) : g_strdup( "false" );

            vfs_file_info_unref( file );
            g_free( path );
        }
        if ( sel_files )
        {
            g_list_foreach( sel_files, ( GFunc ) vfs_file_info_unref, NULL );
            g_list_free( sel_files );
        }
    }

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

    // hack - Due to ptk_file_browser_update_views main iteration, fb tab may be destroyed
    // asynchronously - common if gui thread is blocked on stat
    // NOTE:  this is no longer needed
    if ( !GTK_IS_WIDGET( file_browser ) )
        return;
        
    if ( file_browser->side_book )
    {
        if ( !GTK_IS_WIDGET( file_browser->side_book ) )
            return;
        c->var[CONTEXT_BOOKMARK] = g_strdup( ptk_bookmark_view_get_selected_dir( 
                                        GTK_TREE_VIEW( file_browser->side_book ) ) );
    }
    if ( !c->var[CONTEXT_BOOKMARK] )
        c->var[CONTEXT_BOOKMARK] = g_strdup( "" );
    
#ifndef HAVE_HAL
    // device
    if ( file_browser->side_dev && 
            ( vol = ptk_location_view_get_selected_vol( 
                                    GTK_TREE_VIEW( file_browser->side_dev ) ) ) )
    {
        c->var[CONTEXT_DEVICE] = g_strdup( vol->device_file );
        if ( !c->var[CONTEXT_DEVICE] )
            c->var[CONTEXT_DEVICE] = g_strdup( "" );
        c->var[CONTEXT_DEVICE_LABEL] = g_strdup( vol->label );
        if ( !c->var[CONTEXT_DEVICE_LABEL] )
            c->var[CONTEXT_DEVICE_LABEL] = g_strdup( "" );
        c->var[CONTEXT_DEVICE_MOUNT_POINT] = g_strdup( vol->mount_point );
        if ( !c->var[CONTEXT_DEVICE_MOUNT_POINT] )
            c->var[CONTEXT_DEVICE_MOUNT_POINT] = g_strdup( "" );
        c->var[CONTEXT_DEVICE_UDI] = g_strdup( vol->udi );
        if ( !c->var[CONTEXT_DEVICE_UDI] )
            c->var[CONTEXT_DEVICE_UDI] = g_strdup( "" );
        c->var[CONTEXT_DEVICE_FSTYPE] = g_strdup( vol->fs_type );
        if ( !c->var[CONTEXT_DEVICE_FSTYPE] )
            c->var[CONTEXT_DEVICE_FSTYPE] = g_strdup( "" );

        char* flags = g_strdup( "" );
        char* old_flags;
        if ( vol->is_removable )
            { old_flags = flags; flags = g_strdup_printf( "%s removable", flags ); g_free( old_flags ); }
        else
            { old_flags = flags; flags = g_strdup_printf( "%s internal", flags ); g_free( old_flags ); }

        if ( vol->requires_eject )
            { old_flags = flags; flags = g_strdup_printf( "%s ejectable", flags ); g_free( old_flags ); }
        
        if ( vol->is_optical )
            { old_flags = flags; flags = g_strdup_printf( "%s optical", flags ); g_free( old_flags ); }
        if ( vol->is_table )
            { old_flags = flags; flags = g_strdup_printf( "%s table", flags ); g_free( old_flags ); }
        if ( vol->is_floppy )
            { old_flags = flags; flags = g_strdup_printf( "%s floppy", flags ); g_free( old_flags ); }

        if ( !vol->is_user_visible )
            { old_flags = flags; flags = g_strdup_printf( "%s policy_hide", flags ); g_free( old_flags ); }
        if ( vol->nopolicy )
            { old_flags = flags; flags = g_strdup_printf( "%s policy_noauto", flags ); g_free( old_flags ); }

        if ( vol->is_mounted )
            { old_flags = flags; flags = g_strdup_printf( "%s mounted", flags ); g_free( old_flags ); }
        else if ( vol->is_mountable && !vol->is_table )    
            { old_flags = flags; flags = g_strdup_printf( "%s mountable", flags ); g_free( old_flags ); }
        else
            { old_flags = flags; flags = g_strdup_printf( "%s no_media", flags ); g_free( old_flags ); }

        if ( vol->is_blank )
            { old_flags = flags; flags = g_strdup_printf( "%s blank", flags ); g_free( old_flags ); }
        if ( vol->is_audiocd )
            { old_flags = flags; flags = g_strdup_printf( "%s audiocd", flags ); g_free( old_flags ); }
        if ( vol->is_dvd )
            { old_flags = flags; flags = g_strdup_printf( "%s dvd", flags ); g_free( old_flags ); }

        c->var[CONTEXT_DEVICE_PROP] = flags;
    }
    else
    {
#endif
        c->var[CONTEXT_DEVICE] = g_strdup( "" );
        c->var[CONTEXT_DEVICE_LABEL] = g_strdup( "" );
        c->var[CONTEXT_DEVICE_MOUNT_POINT] = g_strdup( "" );
        c->var[CONTEXT_DEVICE_UDI] = g_strdup( "" );
        c->var[CONTEXT_DEVICE_FSTYPE] = g_strdup( "" );
        c->var[CONTEXT_DEVICE_PROP] = g_strdup( "" );
#ifndef HAVE_HAL
    }
#endif
    
    // panels
    int i, p;
    int panel_count = 0;
    for ( p = 1; p < 5; p++ )
    {
        if ( !xset_get_b_panel( p, "show" ) )
            continue;
        i = gtk_notebook_get_current_page( GTK_NOTEBOOK( main_window->panel[p-1] ) );
        if ( i != -1 )
        {
            a_browser = (PtkFileBrowser*)
                        gtk_notebook_get_nth_page( GTK_NOTEBOOK(
                                                    main_window->panel[p-1] ), i );
        }
        else
            continue;
        if ( !a_browser || !gtk_widget_get_visible( GTK_WIDGET( a_browser ) ) )
            continue;

        panel_count++;
        c->var[CONTEXT_PANEL1_DIR + p - 1] = g_strdup( ptk_file_browser_get_cwd( 
                                                                    a_browser ) );
#ifndef HAVE_HAL
        if ( a_browser->side_dev && 
                ( vol = ptk_location_view_get_selected_vol( 
                                        GTK_TREE_VIEW( a_browser->side_dev ) ) ) )
            c->var[CONTEXT_PANEL1_DEVICE + p - 1] = g_strdup( vol->device_file );
#endif

        // panel has files selected?
        if ( a_browser->view_mode == PTK_FB_ICON_VIEW ||
                                    a_browser->view_mode == PTK_FB_COMPACT_VIEW )
        {
            sel_files = folder_view_get_selected_items( a_browser, &model );
            if ( sel_files )
                c->var[CONTEXT_PANEL1_SEL + p - 1] = g_strdup( "true" );
            else
                c->var[CONTEXT_PANEL1_SEL + p - 1] = g_strdup( "false" );
            g_list_foreach( sel_files, (GFunc)gtk_tree_path_free, NULL );
            g_list_free( sel_files );

        }
        else if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
        {
            GtkTreeSelection* tree_sel = gtk_tree_view_get_selection( 
                                    GTK_TREE_VIEW( a_browser->folder_view ) );
            if ( gtk_tree_selection_count_selected_rows( tree_sel ) > 0 )
                c->var[CONTEXT_PANEL1_SEL + p - 1] = g_strdup( "true" );
            else
                c->var[CONTEXT_PANEL1_SEL + p - 1] = g_strdup( "false" );
        }
        else
            c->var[CONTEXT_PANEL1_SEL + p - 1] = g_strdup( "false" );
            
        if ( file_browser == a_browser )
        {
            c->var[CONTEXT_TAB] = g_strdup_printf( "%d", i + 1 );
            c->var[CONTEXT_TAB_COUNT] = g_strdup_printf( "%d",
                            gtk_notebook_get_n_pages( GTK_NOTEBOOK(
                            main_window->panel[p-1] ) ) );
        }
    }
    c->var[CONTEXT_PANEL_COUNT] = g_strdup_printf( "%d", panel_count );
    c->var[CONTEXT_PANEL] = g_strdup_printf( "%d", file_browser->mypanel );
    if ( !c->var[CONTEXT_TAB] )
        c->var[CONTEXT_TAB] = g_strdup( "" );
    if ( !c->var[CONTEXT_TAB_COUNT] )
        c->var[CONTEXT_TAB_COUNT] = g_strdup( "" );
    for ( p = 1; p < 5; p++ )
    {
        if ( !c->var[CONTEXT_PANEL1_DIR + p - 1] )
            c->var[CONTEXT_PANEL1_DIR + p - 1] = g_strdup( "" );
        if ( !c->var[CONTEXT_PANEL1_SEL + p - 1] )
            c->var[CONTEXT_PANEL1_SEL + p - 1] = g_strdup( "false" );
        if ( !c->var[CONTEXT_PANEL1_DEVICE + p - 1] )
            c->var[CONTEXT_PANEL1_DEVICE + p - 1] = g_strdup( "" );
    }

    // tasks
    const char* job_titles[] =
    {
        "move",
        "copy",
        "trash",
        "delete",
        "link",
        "change",
        "run"
    };
    if ( ptask = get_selected_task( file_browser->task_view ) )
    {
        c->var[CONTEXT_TASK_TYPE] = g_strdup( job_titles[ptask->task->type] );
        if ( ptask->task->type == VFS_FILE_TASK_EXEC )
        {
            c->var[CONTEXT_TASK_NAME] = g_strdup( ptask->task->current_file );
            c->var[CONTEXT_TASK_DIR] = g_strdup( ptask->task->dest_dir );
        }
        else
        {
            c->var[CONTEXT_TASK_NAME] = g_strdup( "" );
            g_mutex_lock( ptask->task->mutex );
            if ( ptask->task->current_file )
                c->var[CONTEXT_TASK_DIR] = g_path_get_dirname( ptask->task->current_file );
            else
                c->var[CONTEXT_TASK_DIR] = g_strdup( "" );
            g_mutex_unlock( ptask->task->mutex );
        }
    }
    else
    {
        c->var[CONTEXT_TASK_TYPE] =  g_strdup( "" );
        c->var[CONTEXT_TASK_NAME] =  g_strdup( "" );
        c->var[CONTEXT_TASK_DIR] =  g_strdup( "" );
    }
    if ( !main_window->task_view || !GTK_IS_TREE_VIEW( main_window->task_view ) )
        c->var[CONTEXT_TASK_COUNT] = g_strdup( "0" );
    else
    {
        model_task = gtk_tree_view_get_model( GTK_TREE_VIEW( main_window->task_view ) );
        int task_count = 0;
        if ( gtk_tree_model_get_iter_first( model_task, &it ) )
        {
            task_count++;
            while ( gtk_tree_model_iter_next( model_task, &it ) );
                task_count++;
        }
        c->var[CONTEXT_TASK_COUNT] = g_strdup_printf( "%d", task_count );
    }

    c->valid = TRUE;
}

FMMainWindow* get_task_view_window( GtkWidget* view )
{
    FMMainWindow* a_window;
    GList* l;
    for ( l = all_windows; l; l = l->next )
    {
        if ( ((FMMainWindow*)l->data)->task_view == view )
            return (FMMainWindow*)l->data;
    }
    return NULL;
}

gboolean main_write_exports( VFSFileTask* vtask, const char* value, FILE* file )
{
    int result, p, num_pages, i;
    const char* cwd;
    char* path;
    char* esc_path;
    GList* sel_files;
    GList* l;
    PtkFileBrowser* a_browser;
    VFSVolume* vol;
    PtkFileTask* ptask;

    if ( !vtask->exec_browser )
        return FALSE;
    PtkFileBrowser* file_browser = (PtkFileBrowser*)vtask->exec_browser;
    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;
    XSet* set = (XSet*)vtask->exec_set;
    
    if ( !file )
        return FALSE;
        
    result = fputs( "# source\n\n", file );
    if ( result < 0 ) return FALSE;
    
    write_src_functions( file );
    
    // panels
    for ( p = 1; p < 5; p++ )
    {
        if ( !xset_get_b_panel( p, "show" ) )
            continue;
        i = gtk_notebook_get_current_page( GTK_NOTEBOOK( main_window->panel[p-1] ) );
        if ( i != -1 )
        {
            a_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page( 
                                GTK_NOTEBOOK( main_window->panel[p-1] ), i ) );
        }
        else
            continue;
        if ( !a_browser || !gtk_widget_get_visible( GTK_WIDGET( a_browser ) ) )
            continue;

        // cwd
        gboolean cwd_needs_quote;
        cwd = ptk_file_browser_get_cwd( a_browser );
        if ( cwd_needs_quote = !!strchr( cwd, '\'' ) )
        {
            path = bash_quote( cwd );
            fprintf( file, "\nfm_pwd_panel[%d]=%s\n", p, path );
            g_free( path );
        }
        else
            fprintf( file, "\nfm_pwd_panel[%d]='%s'\n", p, cwd );
        fprintf( file, "\nfm_tab_panel[%d]=%d\n", p, i + 1 );
        
        // selected files
        sel_files = ptk_file_browser_get_selected_files( a_browser );
        if ( sel_files )
        {
            fprintf( file, "fm_panel%d_files=(\n", p );
            for ( l = sel_files; l; l = l->next )
            {
                path = (char*)vfs_file_info_get_name( (VFSFileInfo*)l->data );
                if ( G_LIKELY( !cwd_needs_quote && !strchr( path, '\'' ) ) )
                    fprintf( file, "'%s%s%s'\n", cwd,
                                ( cwd[0] != '\0' && cwd[1] == '\0' ) ? "" : "/",
                                path );
                else
                {
                    path = g_build_filename( cwd, path, NULL );
                    esc_path = bash_quote( path );
                    fprintf( file, "%s\n", esc_path );
                    g_free( esc_path );
                    g_free( path );
                }
            }
            fputs( ")\n", file );

            if ( file_browser == a_browser )
            {
                fprintf( file, "fm_filenames=(\n" );
                for ( l = sel_files; l; l = l->next )
                {
                    path = (char*)vfs_file_info_get_name( (VFSFileInfo*)l->data );
                    if ( G_LIKELY( !strchr( path, '\'' ) ) )
                        fprintf( file, "'%s'\n", path );
                    else
                    {
                        esc_path = bash_quote( path );
                        fprintf( file, "%s\n", esc_path );
                        g_free( esc_path );
                    }
                }
                fputs( ")\n", file );
            }
            
            g_list_foreach( sel_files, ( GFunc ) vfs_file_info_unref, NULL );
            g_list_free( sel_files );
        }
/*
        // cwd
        cwd = ptk_file_browser_get_cwd( a_browser );
        path = bash_quote( cwd );
        fprintf( file, "\nfm_panel%d_pwd=%s\n", p, path );
        g_free( path );
        
        // selected files
        sel_files = ptk_file_browser_get_selected_files( a_browser );
        if ( sel_files )
        {
            fprintf( file, "fm_panel%d_files=(\n", p );
            for ( l = sel_files; l; l = l->next )
            {
                path = g_build_filename( cwd,
                        vfs_file_info_get_name( (VFSFileInfo*)l->data ), NULL );
                esc_path = bash_quote( path );
                fprintf( file, "%s\n", esc_path );
                g_free( esc_path );
                g_free( path );
            }
            fputs( ")\n", file );

            if ( file_browser == a_browser )
            {
                fprintf( file, "fm_filenames=(\n", p );
                for ( l = sel_files; l; l = l->next )
                {
                    esc_path = bash_quote( vfs_file_info_get_name( (VFSFileInfo*)l->data ) );
                    fprintf( file, "%s\n", esc_path );
                    g_free( esc_path );
                }
                fputs( ")\n", file );
            }
            
            g_list_foreach( sel_files, ( GFunc ) vfs_file_info_unref, NULL );
            g_list_free( sel_files );
        }
*/
        // bookmark
        if ( a_browser->side_book )
        {
            path = ptk_bookmark_view_get_selected_dir( GTK_TREE_VIEW(
                                                        a_browser->side_book ) );
            if ( path )
            {
                esc_path = bash_quote( path );
                if ( file_browser == a_browser )
                    fprintf( file, "fm_bookmark=%s\n", esc_path );
                fprintf( file, "fm_panel%d_bookmark=%s\n", p, esc_path );
                g_free( esc_path );
            }
        }
#ifndef HAVE_HAL
        // device
        if ( a_browser->side_dev )
        {
            vol = ptk_location_view_get_selected_vol( 
                                        GTK_TREE_VIEW( a_browser->side_dev ) );
            if ( vol )
            {
                if ( file_browser == a_browser )
                {
                    fprintf( file, "fm_device='%s'\n", vol->device_file );
                    if ( vol->udi )
                    {
                        esc_path = bash_quote( vol->udi );
                        fprintf( file, "fm_device_udi=%s\n", esc_path );
                        g_free( esc_path );
                    }
                    if ( vol->mount_point )
                    {
                        esc_path = bash_quote( vol->mount_point );
                        fprintf( file, "fm_device_mount_point=%s\n",
                                                            esc_path );
                        g_free( esc_path );
                    }
                    if ( vol->label )
                    {
                        esc_path = bash_quote( vol->label );
                        fprintf( file, "fm_device_label=%s\n", esc_path );
                        g_free( esc_path );
                    }
                    if ( vol->fs_type )
                        fprintf( file, "fm_device_fstype='%s'\n", vol->fs_type );
                    fprintf( file, "fm_device_size=\"%lu\"\n", vol->size );
                    if ( vol->disp_name )
                    {
                        esc_path = bash_quote( vol->disp_name );
                        fprintf( file, "fm_device_display_name=%s\n", esc_path );
                        g_free( esc_path );
                    }
                    fprintf( file, "fm_device_icon='%s'\n", vol->icon );
                    fprintf( file, "fm_device_is_mounted=%d\n",
                                                vol->is_mounted ? 1 : 0 );
                    fprintf( file, "fm_device_is_optical=%d\n",
                                                vol->is_optical ? 1 : 0 );
                    fprintf( file, "fm_device_is_table=%d\n",
                                                vol->is_table ? 1 : 0 );
                    fprintf( file, "fm_device_is_floppy=%d\n",
                                                vol->is_floppy ? 1 : 0 );
                    fprintf( file, "fm_device_is_removable=%d\n",
                                                vol->is_removable ? 1 : 0 );
                    fprintf( file, "fm_device_is_audiocd=%d\n",
                                                vol->is_audiocd ? 1 : 0 );
                    fprintf( file, "fm_device_is_dvd=%d\n",
                                                vol->is_dvd ? 1 : 0 );
                    fprintf( file, "fm_device_is_blank=%d\n",
                                                vol->is_blank ? 1 : 0 );
                    fprintf( file, "fm_device_is_mountable=%d\n",
                                                vol->is_mountable ? 1 : 0 );
                    fprintf( file, "fm_device_nopolicy=%d\n",
                                                vol->nopolicy ? 1 : 0 );
                }
                fprintf( file, "fm_panel%d_device='%s'\n", p, vol->device_file );
                if ( vol->udi )
                {
                    esc_path = bash_quote( vol->udi );
                    fprintf( file, "fm_panel%d_device_udi=%s\n", p, esc_path );
                    g_free( esc_path );
                }
                if ( vol->mount_point )
                {
                    esc_path = bash_quote( vol->mount_point );
                    fprintf( file, "fm_panel%d_device_mount_point=%s\n",
                                                        p, esc_path );
                    g_free( esc_path );
                }
                if ( vol->label )
                {
                    esc_path = bash_quote( vol->label );
                    fprintf( file, "fm_panel%d_device_label=%s\n", p, esc_path );
                    g_free( esc_path );
                }
                if ( vol->fs_type )
                    fprintf( file, "fm_panel%d_device_fstype='%s'\n", p,
                                                                    vol->fs_type );
                fprintf( file, "fm_panel%d_device_size=\"%lu\"\n", p, vol->size );
                if ( vol->disp_name )
                {
                    esc_path = bash_quote( vol->disp_name );
                    fprintf( file, "fm_panel%d_device_display_name=%s\n", p,
                                                                        esc_path );
                    g_free( esc_path );
                }
                fprintf( file, "fm_panel%d_device_icon='%s'\n", p, vol->icon );
                fprintf( file, "fm_panel%d_device_is_mounted=%d\n",
                                            p, vol->is_mounted ? 1 : 0 );
                fprintf( file, "fm_panel%d_device_is_optical=%d\n",
                                            p, vol->is_optical ? 1 : 0 );
                fprintf( file, "fm_panel%d_device_is_table=%d\n",
                                            p, vol->is_table ? 1 : 0 );
                fprintf( file, "fm_panel%d_device_is_floppy=%d\n",
                                            p, vol->is_floppy ? 1 : 0 );
                fprintf( file, "fm_panel%d_device_is_removable=%d\n",
                                            p, vol->is_removable ? 1 : 0 );
                fprintf( file, "fm_panel%d_device_is_audiocd=%d\n",
                                            p, vol->is_audiocd ? 1 : 0 );
                fprintf( file, "fm_panel%d_device_is_dvd=%d\n",
                                            p, vol->is_dvd ? 1 : 0 );
                fprintf( file, "fm_panel%d_device_is_blank=%d\n",
                                            p, vol->is_blank ? 1 : 0 );
                fprintf( file, "fm_panel%d_device_is_mountable=%d\n",
                                            p, vol->is_mountable ? 1 : 0 );
                fprintf( file, "fm_panel%d_device_nopolicy=%d\n",
                                            p, vol->nopolicy ? 1 : 0 );
            }
        }
#endif
        // tabs
        PtkFileBrowser* t_browser;
        num_pages = gtk_notebook_get_n_pages( GTK_NOTEBOOK( main_window->panel[p-1] ) );
        for ( i = 0; i < num_pages; i++ )
        {
            t_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                    GTK_NOTEBOOK( main_window->panel[p-1] ), i ) );
            path = bash_quote( ptk_file_browser_get_cwd( t_browser ) );
            fprintf( file, "fm_pwd_panel%d_tab[%d]=%s\n", p, i + 1, path );
            if ( p == file_browser->mypanel )
            {
                fprintf( file, "fm_pwd_tab[%d]=%s\n", i + 1, path );
            }
            if ( file_browser == t_browser )
            {
                // my browser
                fprintf( file, "fm_pwd=%s\n", path );
                fprintf( file, "fm_panel=%d\n", p );
                fprintf( file, "fm_tab=%d\n", i + 1 );
                //if ( i > 0 )
                //    fprintf( file, "fm_tab_prev=%d\n", i );
                //if ( i < num_pages - 1 )
                //    fprintf( file, "fm_tab_next=%d\n", i + 2 );
            }
            g_free( path );
        }
    }
    // my selected files 
    fprintf( file, "\nfm_files=(\"${fm_panel%d_files[@]}\")\n",
                                                    file_browser->mypanel );
    fprintf( file, "fm_file=\"${fm_panel%d_files[0]}\"\n",
                                                    file_browser->mypanel );
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
    if ( vtask->exec_ptask )
    {
        fprintf( file, "fm_my_task=%p\n", vtask->exec_ptask );
        fprintf( file, "fm_my_task_id=%p\n", vtask->exec_ptask );
    }
    fprintf( file, "fm_my_window=%p\n", main_window );
    fprintf( file, "fm_my_window_id=%p\n", main_window );
    
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
    
    // tasks
    const char* job_titles[] =
    {
         "move",
         "copy",
         "trash",
         "delete",
         "link",
         "change",
         "run"
    };
    if ( ptask = get_selected_task( file_browser->task_view ) )
    {
        fprintf( file, "\nfm_task_type='%s'\n", job_titles[ptask->task->type] );
        if ( ptask->task->type == VFS_FILE_TASK_EXEC )
        {
            esc_path = bash_quote( ptask->task->dest_dir );
            fprintf( file, "fm_task_pwd=%s\n", esc_path );
            g_free( esc_path );
            esc_path = bash_quote( ptask->task->current_file );
            fprintf( file, "fm_task_name=%s\n", esc_path );
            g_free( esc_path );
            esc_path = bash_quote( ptask->task->exec_command );
            fprintf( file, "fm_task_command=%s\n", esc_path );
            g_free( esc_path );
            if ( ptask->task->exec_as_user )
                fprintf( file, "fm_task_user='%s'\n", ptask->task->exec_as_user );
            if ( ptask->task->exec_icon )
                fprintf( file, "fm_task_icon='%s'\n", ptask->task->exec_icon );
            if ( ptask->task->exec_pid )
                fprintf( file, "fm_task_pid=%d\n", ptask->task->exec_pid );
        }
        else
        {
            esc_path = bash_quote( ptask->task->dest_dir );
            fprintf( file, "fm_task_dest_dir=%s\n", esc_path );
            g_free( esc_path );
            esc_path = bash_quote( ptask->task->current_file );
            fprintf( file, "fm_task_current_src_file=%s\n", esc_path );
            g_free( esc_path );
            esc_path = bash_quote( ptask->task->current_dest );
            fprintf( file, "fm_task_current_dest_file=%s\n", esc_path );
            g_free( esc_path );
        }
        fprintf( file, "fm_task_id=%p\n", ptask );
        if ( ptask->task_view &&
                    ( main_window = get_task_view_window( ptask->task_view ) ) )
        {
            fprintf( file, "fm_task_window=%p\n", main_window );
            fprintf( file, "fm_task_window_id=%p\n", main_window );
        }
    }

    result = fputs( "\n", file );
    return result >= 0;
}

void on_task_columns_changed( GtkWidget *view, gpointer user_data )
{
    const char* title;
    char* pos;
    XSet* set = NULL;
    int i, j, width;
    GtkTreeViewColumn* col;

    FMMainWindow* main_window = get_task_view_window( view );
    if ( !main_window || !view )
        return;
    for ( i = 0; i < 13; i++ )
    {
        col = gtk_tree_view_get_column( GTK_TREE_VIEW( view ), i );
        if ( !col )
            return;
        title = gtk_tree_view_column_get_title( col );
        for ( j = 0; j < 13; j++ )
        {
            if ( !strcmp( title, _(task_titles[j]) ) )
                break;
        }
        if ( j != 13 )
        {
            set = xset_get( task_names[j] );
            // save column position
            pos = g_strdup_printf( "%d", i );
            xset_set_set( set, "x", pos );
            g_free( pos );
            // if the window was opened maximized and stayed maximized, or the
            // window is unmaximized and not fullscreen, save the columns
            if ( ( !main_window->maximized || main_window->opened_maximized ) &&
                                                !main_window->fullscreen )
            {
                width = gtk_tree_view_column_get_width( col );
                if ( width )  // manager unshown, all widths are zero
                {
                    // save column width
                    pos = g_strdup_printf( "%d", width );
                    xset_set_set( set, "y", pos );
                    g_free( pos );
                }
            }
            // set column visibility
            gtk_tree_view_column_set_visible( col, xset_get_b( task_names[j] ) );
        }
    }
}

void on_task_destroy( GtkWidget *view, gpointer user_data )
{
    guint id = g_signal_lookup ("columns-changed", G_TYPE_FROM_INSTANCE(view) );
    if ( id )
    {
        gulong hand = g_signal_handler_find( ( gpointer ) view, G_SIGNAL_MATCH_ID,
                                        id, 0, NULL, NULL, NULL );
        if ( hand )
            g_signal_handler_disconnect( ( gpointer ) view, hand );
    }
    on_task_columns_changed( view, NULL ); // save widths
}

void on_task_column_selected( GtkMenuItem* item, GtkWidget* view )
{
    on_task_columns_changed( view, NULL );    
}

gboolean main_tasks_running( FMMainWindow* main_window )
{
    GtkTreeIter it;

    if ( !main_window->task_view || !GTK_IS_TREE_VIEW( main_window->task_view ) )
        return FALSE;

    GtkTreeModel* model = gtk_tree_view_get_model( 
                                    GTK_TREE_VIEW( main_window->task_view ) );
    gboolean ret = gtk_tree_model_get_iter_first( model, &it );
    
    return ret;
}

void main_task_pause_all_queued( PtkFileTask* ptask )
{
    PtkFileTask* qtask;
    GtkTreeIter it;
    
    if ( !ptask->task_view )
        return;
    GtkTreeModel* model = gtk_tree_view_get_model( GTK_TREE_VIEW( ptask->task_view ) );
    if ( gtk_tree_model_get_iter_first( model, &it ) )
    {
        do
        {
            gtk_tree_model_get( model, &it, TASK_COL_DATA, &qtask, -1 );
            if ( qtask && qtask != ptask && qtask->task && !qtask->complete &&
                            qtask->task->state_pause == VFS_FILE_TASK_QUEUE )
                ptk_file_task_pause( qtask, VFS_FILE_TASK_PAUSE );
        }
        while ( gtk_tree_model_iter_next( model, &it ) );
    }
}

void main_task_start_queued( GtkWidget* view, PtkFileTask* new_task )
{
    GtkTreeModel* model;
    GtkTreeIter it;
    PtkFileTask* qtask;
    PtkFileTask* rtask;
    GSList* running = NULL;
    GSList* queued = NULL;
    gboolean smart;
#ifdef HAVE_HAL
    smart = FALSE;
#else
    smart = xset_get_b( "task_q_smart" );
#endif
    if ( !GTK_IS_TREE_VIEW( view ) )
        return;

    model = gtk_tree_view_get_model( GTK_TREE_VIEW( view ) );
    if ( gtk_tree_model_get_iter_first( model, &it ) )
    {
        do
        {
            gtk_tree_model_get( model, &it, TASK_COL_DATA, &qtask, -1 );
            if ( qtask && qtask->task && !qtask->complete &&
                                    qtask->task->state == VFS_FILE_TASK_RUNNING )
            {
                if ( qtask->task->state_pause == VFS_FILE_TASK_QUEUE )
                    queued = g_slist_append( queued, qtask );
                else if ( qtask->task->state_pause == VFS_FILE_TASK_RUNNING )
                    running = g_slist_append( running, qtask );
            }
        }
        while ( gtk_tree_model_iter_next( model, &it ) );
    }
    
    if ( new_task && new_task->task && !new_task->complete && 
                            new_task->task->state_pause == VFS_FILE_TASK_QUEUE &&
                            new_task->task->state == VFS_FILE_TASK_RUNNING )
        queued = g_slist_append( queued, new_task );

    if ( !queued || ( !smart && running ) )
        goto _done;
    
    if ( !smart )
    {
        ptk_file_task_pause( (PtkFileTask*)queued->data, VFS_FILE_TASK_RUNNING );
        goto _done;
    }

    // smart
    GSList* d;
    GSList* r;
    GSList* q;
    for ( q = queued; q; q = q->next )
    {
        qtask = (PtkFileTask*)q->data;
        if ( !qtask->task->devs )
        {
            // qtask has no devices so run it
            running = g_slist_append( running, qtask );
            ptk_file_task_pause( qtask, VFS_FILE_TASK_RUNNING );
            continue;
        }
        // does qtask have running devices?
        for ( r = running; r; r = r->next )
        {
            rtask = (PtkFileTask*)r->data;
            for ( d = qtask->task->devs; d; d = d->next )
            {
                if ( g_slist_find( rtask->task->devs, d->data ) )
                    break;
            }
            if ( d )
                break;
        }
        if ( !r )
        {
            // qtask has no running devices so run it
            running = g_slist_append( running, qtask );
            ptk_file_task_pause( qtask, VFS_FILE_TASK_RUNNING );
            continue;
        }
    }
_done:
    g_slist_free( queued );
    g_slist_free( running );
}

void on_task_stop( GtkMenuItem* item, GtkWidget* view, XSet* set2,
                                                            PtkFileTask* task2 )
{
    GtkTreeModel* model = NULL;
    GtkTreeIter it;
    PtkFileTask* ptask;
    XSet* set;
    int job;
    enum { JOB_STOP, JOB_PAUSE, JOB_QUEUE, JOB_RESUME };

    if ( item )
        set = (XSet*)g_object_get_data( G_OBJECT( item ), "set" );
    else
        set = set2;
    if ( !set || !g_str_has_prefix( set->name, "task_" ) )
        return;
    
    char* name = set->name + 5;
    if ( g_str_has_prefix( name, "stop" ) )
        job = JOB_STOP;
    else if ( g_str_has_prefix( name, "pause" ) )
        job = JOB_PAUSE;
    else if ( g_str_has_prefix( name, "que" ) )
        job = JOB_QUEUE;
    else if ( g_str_has_prefix( name, "resume" ) )
        job = JOB_RESUME;
    else
        return;
    gboolean all = ( g_str_has_suffix( name, "_all" ) );

    if ( all )
    {
        model = gtk_tree_view_get_model( GTK_TREE_VIEW( view ) );
        ptask = NULL;
    }
    else
    {
        if ( item )
            ptask = ( PtkFileTask* ) g_object_get_data( G_OBJECT( item ),
                                                             "task" );
        else
            ptask = task2;
        if ( !ptask )
            return;
    }
    
    if ( !model || ( model && gtk_tree_model_get_iter_first( model, &it ) ) )
    {
        do
        {
            if ( model )
                gtk_tree_model_get( model, &it, TASK_COL_DATA, &ptask, -1 );
            if ( ptask && ptask->task && !ptask->complete &&
                        ( ptask->task->type != VFS_FILE_TASK_EXEC
                                || ptask->task->exec_pid || job == JOB_STOP ) )
            {
                switch ( job )
                {
                    case JOB_STOP:
                        ptk_file_task_cancel( ptask );
                        break;
                    case JOB_PAUSE:
                        ptk_file_task_pause( ptask, VFS_FILE_TASK_PAUSE );
                        break;
                    case JOB_QUEUE:
                        ptk_file_task_pause( ptask, VFS_FILE_TASK_QUEUE );
                        break;
                    case JOB_RESUME:
                        ptk_file_task_pause( ptask, VFS_FILE_TASK_RUNNING );
                        break;
                }
            }
        }
        while ( model && gtk_tree_model_iter_next( model, &it ) );
    }
    main_task_start_queued( view, NULL );
}

static gboolean idle_set_task_height( FMMainWindow* main_window )
{
    GtkAllocation allocation;
    int pos, taskh;

    gtk_widget_get_allocation( GTK_WIDGET( main_window ), &allocation );

    // set new config panel sizes to half of window
    if ( !xset_is( "panel_sliders" ) )
    {
        // this isn't perfect because panel half-width is set before user
        // adjusts window size
        XSet* set = xset_get( "panel_sliders" );
        set->x = g_strdup_printf( "%d", allocation.width / 2 );
        set->y = g_strdup_printf( "%d", allocation.width / 2 );
        set->s = g_strdup_printf( "%d", allocation.height / 2 );
    }

    // restore height (in case window height changed)
    taskh = xset_get_int( "task_show_manager", "x" ); // task height >=0.9.2
    if ( taskh == 0 )
    {
        // use pre-0.9.2 slider pos to calculate height
        pos = xset_get_int( "panel_sliders", "z" );       // < 0.9.2 slider pos
        if ( pos == 0 )
            taskh = 200;
        else
            taskh = allocation.height - pos;
    }
    if ( taskh > allocation.height / 2 )
        taskh = allocation.height / 2;
    if ( taskh < 1 )
        taskh = 90;
//printf("SHOW  win %dx%d    task height %d   slider %d\n", allocation.width, allocation.height, taskh, allocation.height - taskh );
    gtk_paned_set_position( GTK_PANED( main_window->task_vpane ),
                                            allocation.height - taskh );
    return FALSE;
}

void show_task_manager( FMMainWindow* main_window, gboolean show )
{
    GtkAllocation allocation;
    
    gtk_widget_get_allocation( GTK_WIDGET( main_window ), &allocation );

    if ( show )
    {
        if ( !gtk_widget_get_visible( GTK_WIDGET( main_window->task_scroll ) ) )
        {
            gtk_widget_show( main_window->task_scroll );
            // allow vpane to auto-adjust before setting new slider pos
            g_idle_add( (GSourceFunc)idle_set_task_height, main_window );
        }
    }
    else
    {
        // save height
        if ( gtk_widget_get_visible( GTK_WIDGET( main_window->task_scroll ) ) )
        {
            int pos = gtk_paned_get_position(
                                        GTK_PANED( main_window->task_vpane ) );
            if ( pos )
            {
                // save slider pos for version < 0.9.2 (in case of downgrade)
                char* posa = g_strdup_printf( "%d", pos );
                xset_set( "panel_sliders", "z", posa );
                g_free( posa );
                // save absolute height introduced v0.9.2
                posa = g_strdup_printf( "%d", allocation.height - pos );
                xset_set( "task_show_manager", "x", posa );
                g_free( posa );
//printf("HIDE  win %dx%d    task height %d   slider %d\n", allocation.width, allocation.height, allocation.height - pos, pos );
            }
        }
        // hide
        gboolean tasks_has_focus = gtk_widget_is_focus( 
                                    GTK_WIDGET( main_window->task_view ) );
        gtk_widget_hide( GTK_WIDGET( main_window->task_scroll ) );
        if ( tasks_has_focus )
        {
            // focus the file list
            PtkFileBrowser* file_browser = PTK_FILE_BROWSER( 
                    fm_main_window_get_current_file_browser( main_window ) );   
            if ( file_browser )
                gtk_widget_grab_focus( file_browser->folder_view );
        }
    }
}

void on_task_popup_show( GtkMenuItem* item, FMMainWindow* main_window, char* name2 )
{
    GtkTreeModel* model = NULL;
    GtkTreeIter it;
    char* name = NULL;
    
    if ( item )
        name = ( char* ) g_object_get_data( G_OBJECT( item ), "name" );
    else
        name = name2;

    if ( name )
    {
        if ( !strcmp( name, "task_show_manager" ) )
        {
            if ( xset_get_b( "task_show_manager" ) )
                xset_set_b("task_hide_manager", FALSE );
            else
            {
                xset_set_b("task_hide_manager", TRUE );
                xset_set_b("task_show_manager", FALSE );
            }
        }
        else
        {
            if ( xset_get_b( "task_hide_manager" ) )
                xset_set_b("task_show_manager", FALSE );
            else
            {
                xset_set_b("task_hide_manager", FALSE );
                xset_set_b("task_show_manager", TRUE );
            }
        }
    }
    
    if ( xset_get_b( "task_show_manager" ) )
        show_task_manager( main_window, TRUE );
    else
    {
        model = gtk_tree_view_get_model( GTK_TREE_VIEW( main_window->task_view ) );
        if ( gtk_tree_model_get_iter_first( model, &it ) )
            show_task_manager( main_window, TRUE );
        else if ( xset_get_b( "task_hide_manager" ) )
            show_task_manager( main_window, FALSE );
    }
}

void on_task_popup_errset( GtkMenuItem* item, FMMainWindow* main_window, char* name2 )
{
    char* name;
    if ( item )
        name = ( char* ) g_object_get_data( G_OBJECT( item ), "name" );
    else
        name = name2;
        
    if ( !name )
        return;

    if ( !strcmp( name, "task_err_first" ) )
    {
        if ( xset_get_b( "task_err_first" ) )
        {
            xset_set_b("task_err_any", FALSE );
            xset_set_b("task_err_cont", FALSE );
        }
        else
        {
            xset_set_b("task_err_any", FALSE );
            xset_set_b("task_err_cont", TRUE );
        }
    }
    else if ( !strcmp( name, "task_err_any" ) )
    {
        if ( xset_get_b( "task_err_any" ) )
        {
            xset_set_b("task_err_first", FALSE );
            xset_set_b("task_err_cont", FALSE );
        }
        else
        {
            xset_set_b("task_err_first", FALSE );
            xset_set_b("task_err_cont", TRUE );
        }
    }
    else
    {
        if ( xset_get_b( "task_err_cont" ) )
        {
            xset_set_b("task_err_first", FALSE );
            xset_set_b("task_err_any", FALSE );
        }
        else
        {
            xset_set_b("task_err_first", TRUE );
            xset_set_b("task_err_any", FALSE );
        }
    }
}

void main_task_prepare_menu( FMMainWindow* main_window, GtkWidget* menu,
                                                GtkAccelGroup* accel_group )
{
    XSet* set;
    XSet* set_radio;
    
    GtkWidget* parent = main_window->task_view;
    set = xset_set_cb( "task_show_manager", on_task_popup_show, main_window );
        xset_set_ob1( set, "name", set->name );
        xset_set_ob2( set, NULL, NULL );
        set_radio = set;
    set = xset_set_cb( "task_hide_manager", on_task_popup_show, main_window );
        xset_set_ob1( set, "name", set->name );
        xset_set_ob2( set, NULL, set_radio );

    xset_set_cb( "task_col_count", on_task_column_selected, parent );
    xset_set_cb( "task_col_path", on_task_column_selected, parent );
    xset_set_cb( "task_col_file", on_task_column_selected, parent );
    xset_set_cb( "task_col_to", on_task_column_selected, parent );
    xset_set_cb( "task_col_progress", on_task_column_selected, parent );
    xset_set_cb( "task_col_total", on_task_column_selected, parent );
    xset_set_cb( "task_col_started", on_task_column_selected, parent );
    xset_set_cb( "task_col_elapsed", on_task_column_selected, parent );
    xset_set_cb( "task_col_curspeed", on_task_column_selected, parent );
    xset_set_cb( "task_col_curest", on_task_column_selected, parent );
    xset_set_cb( "task_col_avgspeed", on_task_column_selected, parent );
    xset_set_cb( "task_col_avgest", on_task_column_selected, parent );
    xset_set_cb( "task_col_reorder", on_reorder, parent );

    set = xset_set_cb( "task_err_first", on_task_popup_errset, main_window );
        xset_set_ob1( set, "name", set->name );
        xset_set_ob2( set, NULL, NULL );
        set_radio = set;
    set = xset_set_cb( "task_err_any", on_task_popup_errset, main_window );
        xset_set_ob1( set, "name", set->name );
        xset_set_ob2( set, NULL, set_radio );
    set = xset_set_cb( "task_err_cont", on_task_popup_errset, main_window );
        xset_set_ob1( set, "name", set->name );
        xset_set_ob2( set, NULL, set_radio );
}

PtkFileTask* get_selected_task( GtkWidget* view )
{
    GtkTreeModel* model;
    GtkTreeSelection* tree_sel;
    GtkTreeViewColumn* col = NULL;
    GtkTreeIter it;
    PtkFileTask* ptask = NULL;    

    if ( !view )
        return NULL;
    FMMainWindow* main_window = get_task_view_window( view );
    if ( !main_window )
        return NULL;

    model = gtk_tree_view_get_model( GTK_TREE_VIEW( view ) );
    tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( view ) );
    if ( gtk_tree_selection_get_selected( tree_sel, NULL, &it ) )
    {
        gtk_tree_model_get( model, &it, TASK_COL_DATA, &ptask, -1 );
    }
    return ptask;
}

void show_task_dialog( GtkWidget* widget, GtkWidget* view )
{
    PtkFileTask* ptask = get_selected_task( view );
    if ( !ptask )
        return;
        
    g_mutex_lock( ptask->task->mutex );
    ptk_file_task_progress_open( ptask );
    if ( ptask->task->state_pause != VFS_FILE_TASK_RUNNING )
    {
        // update dlg
        ptask->pause_change = TRUE;
        ptask->progress_count = 50;  // trigger fast display
    }   
    if ( ptask->progress_dlg )
        gtk_window_present( GTK_WINDOW( ptask->progress_dlg ) );
    g_mutex_unlock( ptask->task->mutex );
}

gboolean on_task_button_press_event( GtkWidget* view, GdkEventButton *event,
                                     FMMainWindow* main_window )
{
    GtkTreeModel* model = NULL;
    GtkTreePath* tree_path;
    GtkTreeViewColumn* col = NULL;
    GtkTreeIter it;
    GtkTreeSelection* tree_sel;
    PtkFileTask* ptask = NULL;
    XSet* set;
    gboolean is_tasks;
    
    if ( event->type != GDK_BUTTON_PRESS )
        return FALSE;
        
    if ( ( evt_win_click->s || evt_win_click->ob2_data ) &&
            main_window_event( main_window, evt_win_click, "evt_win_click", 0, 0,
                            "tasklist", 0, event->button, event->state, TRUE ) )
        return FALSE;
    
    if ( event->button == 3 ) // right click
    {    
        // get selected task
        model = gtk_tree_view_get_model( GTK_TREE_VIEW( view ) );
        if ( is_tasks = gtk_tree_model_get_iter_first( model, &it ) )
        {
            if ( gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW( view ), event->x,
                                        event->y, &tree_path, &col, NULL, NULL ) )
            {
                if ( tree_path && gtk_tree_model_get_iter( model, &it, tree_path ) )
                    gtk_tree_model_get( model, &it, TASK_COL_DATA, &ptask, -1 );
                gtk_tree_path_free( tree_path );
            }
        }

        // build popup
        PtkFileBrowser* file_browser = PTK_FILE_BROWSER( 
                        fm_main_window_get_current_file_browser( main_window ) );
        if ( !file_browser )
            return FALSE;
        GtkWidget* popup = gtk_menu_new();
        GtkAccelGroup* accel_group = gtk_accel_group_new();
        XSetContext* context = xset_context_new();
        main_context_fill( file_browser, context );

        set = xset_set_cb( "task_stop", on_task_stop, view );
        xset_set_ob1( set, "task", ptask );
        set->disable = !ptask;

        set = xset_set_cb( "task_pause", on_task_stop, view );
        xset_set_ob1( set, "task", ptask );
        set->disable = ( !ptask || ptask->task->state_pause == VFS_FILE_TASK_PAUSE 
                        || ( ptask->task->type == VFS_FILE_TASK_EXEC && 
                                                    !ptask->task->exec_pid ) );

        set = xset_set_cb( "task_que", on_task_stop, view );
        xset_set_ob1( set, "task", ptask );
        set->disable = ( !ptask || ptask->task->state_pause == VFS_FILE_TASK_QUEUE 
                        || ( ptask->task->type == VFS_FILE_TASK_EXEC && 
                                                    !ptask->task->exec_pid ) );

        set = xset_set_cb( "task_resume", on_task_stop, view );
        xset_set_ob1( set, "task", ptask );
        set->disable = ( !ptask || ptask->task->state_pause == VFS_FILE_TASK_RUNNING
                        || ( ptask->task->type == VFS_FILE_TASK_EXEC && 
                                                    !ptask->task->exec_pid ) );

        xset_set_cb( "task_stop_all", on_task_stop, view );
        xset_set_cb( "task_pause_all", on_task_stop, view );
        xset_set_cb( "task_que_all", on_task_stop, view );
        xset_set_cb( "task_resume_all", on_task_stop, view );
        set = xset_get( "task_all" );
        set->disable = !is_tasks;
    
#ifdef HAVE_HAL
        set = xset_get( "task_q_smart" );
        set->disable = TRUE;
#endif

        const char* showout = "";
        if ( ptask && ptask->pop_handler )
        {
            xset_set_cb( "task_showout", show_task_dialog, view );
            showout = " task_showout";
        }

        main_task_prepare_menu( main_window, popup, accel_group );

        xset_set_cb( "font_task", main_update_fonts, file_browser );
        char* menu_elements = g_strdup_printf( "task_stop sep_t3 task_pause task_que task_resume%s task_all sep_t4 task_show_manager task_hide_manager sep_t5 task_columns task_popups task_errors task_queue", showout );
        xset_add_menu( NULL, file_browser, popup, accel_group, menu_elements );
        g_free( menu_elements );
        
        gtk_widget_show_all( GTK_WIDGET( popup ) );
        g_signal_connect( popup, "selection-done",
                          G_CALLBACK( gtk_widget_destroy ), NULL );
        g_signal_connect( popup, "key_press_event",
                          G_CALLBACK( xset_menu_keypress ), NULL );
        gtk_menu_popup( GTK_MENU( popup ), NULL, NULL, NULL, NULL, event->button,
                                                                    event->time );
    }
    else if ( event->button == 1 || event->button == 2 ) // left or middle click
    {    
        // get selected task
        model = gtk_tree_view_get_model( GTK_TREE_VIEW( view ) );
        //printf("x = %lf  y = %lf  \n", event->x, event->y );
        // due to bug in gtk_tree_view_get_path_at_pos (gtk 2.24), a click
        // on the column header resize divider registers as a click on the
        // first row first column.  So if event->x < 7 ignore
        if ( event->x < 7 )
            return FALSE;
        if ( !gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW( view ), event->x,
                                    event->y, &tree_path, &col, NULL, NULL ) )
            return FALSE;
        if ( tree_path && gtk_tree_model_get_iter( model, &it, tree_path ) )
            gtk_tree_model_get( model, &it, TASK_COL_DATA, &ptask, -1 ); 
        gtk_tree_path_free( tree_path );

        if ( !ptask )
            return FALSE;
        if ( event->button == 1 && g_strcmp0( gtk_tree_view_column_get_title( col ),
                                                                _("Status") ) )
            return FALSE;
        char* sname;
        switch ( ptask->task->state_pause )
        {
            case VFS_FILE_TASK_PAUSE:
                sname = "task_que";
                break;
            case VFS_FILE_TASK_QUEUE:
                sname = "task_resume";
                break;
            default:
                sname = "task_pause";
        }
        set = xset_get( sname );
        on_task_stop( NULL, view, set, ptask );
        return TRUE;
    }
    return FALSE;
}

void on_task_row_activated( GtkWidget* view, GtkTreePath* tree_path,
                       GtkTreeViewColumn *col, gpointer user_data )
{
    GtkTreeModel* model;
    GtkTreeIter it;
    PtkFileTask* ptask;
    
    FMMainWindow* main_window = get_task_view_window( view );
    if ( !main_window )
        return;

    model = gtk_tree_view_get_model( GTK_TREE_VIEW( view ) );
    if ( !gtk_tree_model_get_iter( model, &it, tree_path ) )
        return;

    gtk_tree_model_get( model, &it, TASK_COL_DATA, &ptask, -1 );
    if ( ptask )
    {
        if ( ptask->pop_handler )
        {
            // show custom dialog
            char* argv[4];
            argv[0] = g_strdup( "bash" );
            argv[1] = g_strdup( "-c" );
            argv[2] = g_strdup( ptask->pop_handler );
            argv[3] = NULL;
            printf( "TASK_POPUP >>> %s\n", argv[2] );
            g_spawn_async( NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                                                              NULL, NULL );
        }
        else
        {
            // show normal dialog
            show_task_dialog( NULL, view );
        }
    }
}

void main_task_view_remove_task( PtkFileTask* ptask )
{
    PtkFileTask* ptaskt = NULL;
    GtkWidget* view;
    GtkTreeModel* model;
    GtkTreeIter it;
//printf("main_task_view_remove_task  ptask=%d\n", ptask);

    view = ptask->task_view;
    if ( !view )
        return;

    FMMainWindow* main_window = get_task_view_window( view );
    if ( !main_window )
        return;

    model = gtk_tree_view_get_model( GTK_TREE_VIEW( view ) );
    if ( gtk_tree_model_get_iter_first( model, &it ) )
    {
        do
        {
            gtk_tree_model_get( model, &it, TASK_COL_DATA, &ptaskt, -1 );
        }
        while ( ptaskt != ptask && gtk_tree_model_iter_next( model, &it ) );
    }
    if ( ptaskt == ptask )
        gtk_list_store_remove( GTK_LIST_STORE( model ), &it );
    
    if ( !gtk_tree_model_get_iter_first( model, &it ) )
    {
        if ( xset_get_b( "task_hide_manager" ) )
            show_task_manager( main_window, FALSE );
    }
    
    update_window_title( NULL, main_window );
//printf("main_task_view_remove_task DONE ptask=%d\n", ptask);
}

void main_task_view_update_task( PtkFileTask* ptask )
{
    PtkFileTask* ptaskt = NULL;
    GtkWidget* view;
    GtkTreeModel* model;
    GtkTreeIter it;
    GdkPixbuf* pixbuf;
    char* dest_dir;
    char* path = NULL;
    char* file = NULL;
    XSet* set;
    
//printf("main_task_view_update_task  ptask=%d\n", ptask);
    const char* job_titles[] =
        {
            N_( "moving" ),
            N_( "copying" ),
            N_( "trashing" ),
            N_( "deleting" ),
            N_( "linking" ),
            N_( "changing" ),
            N_( "running" )
        };

    if ( !ptask )
        return;
    
    view = ptask->task_view;
    if ( !view )
        return;
    
    FMMainWindow* main_window = get_task_view_window( view );
    if ( !main_window )
        return;

    if ( ptask->task->type != VFS_FILE_TASK_EXEC )
        dest_dir = ptask->task->dest_dir;
    else
        dest_dir = NULL;

    model = gtk_tree_view_get_model( GTK_TREE_VIEW( view ) );
    if ( gtk_tree_model_get_iter_first( model, &it ) )
    {
        do
        {
            gtk_tree_model_get( model, &it, TASK_COL_DATA, &ptaskt, -1 );
        }
        while ( ptaskt != ptask && gtk_tree_model_iter_next( model, &it ) );
    }
    if ( ptaskt != ptask )
    {
        // new row
        char buf[ 64 ];
        strftime( buf, sizeof( buf ), "%H:%M", localtime( &ptask->task->start_time ) );
        char* started = g_strdup( buf );
        gtk_list_store_insert_with_values( GTK_LIST_STORE( model ), &it, 0,
                                    TASK_COL_TO, dest_dir,
                                    TASK_COL_STARTED, started,
                                    TASK_COL_STARTTIME, (gint64)ptask->task->start_time,
                                    TASK_COL_DATA, ptask,
                                    -1 );        
        g_free( started );
    }

    if ( ptask->task->state_pause == VFS_FILE_TASK_RUNNING || ptask->pause_change_view )
    {
        // update row
        int percent = ptask->task->percent;
        if ( percent < 0 )
            percent = 0;
        else if ( percent > 100 )
            percent = 100;
        if ( ptask->task->type != VFS_FILE_TASK_EXEC )
        {
            if ( ptask->task->current_file )
            {
                path = g_path_get_dirname( ptask->task->current_file );
                file = g_path_get_basename( ptask->task->current_file );
            }
        }
        else
        {
            path = g_strdup( ptask->task->dest_dir ); //cwd
            file = g_strdup_printf( "( %s )", ptask->task->current_file );
        }
        
        // status
        const char* status;
        char* status2 = NULL;
        char* status3;
        if ( ptask->task->type != VFS_FILE_TASK_EXEC )
        {
            if ( !ptask->err_count )
                status = _(job_titles[ ptask->task->type ]);
            else
            {
                status2 = g_strdup_printf( ngettext( _("%d error %s"),
                                                     _("%d errors %s"),
                                                     ptask->err_count ),
                                           ptask->err_count,
                                           _(job_titles[ ptask->task->type ]) );
                status = status2;
            }
        }
        else
        {
            // exec task
            if ( ptask->task->exec_action )
                status = ptask->task->exec_action;
            else
                status = _(job_titles[ ptask->task->type ]);
        }
        if ( ptask->task->state_pause == VFS_FILE_TASK_PAUSE )
            status3 = g_strdup_printf( "%s %s", _("paused"), status );
        else if ( ptask->task->state_pause == VFS_FILE_TASK_QUEUE )
            status3 = g_strdup_printf( "%s %s", _("queued"), status );
        else
            status3 = g_strdup( status );

        // update icon if queue state changed
        pixbuf = NULL;
        if ( ptask->pause_change_view )
        {
            // icon
            char* iname;
            if ( ptask->task->state_pause == VFS_FILE_TASK_PAUSE )
            {
                set = xset_get( "task_pause" );
                iname = g_strdup( set->icon ? set->icon : GTK_STOCK_MEDIA_PAUSE );
            }
            else if ( ptask->task->state_pause == VFS_FILE_TASK_QUEUE )
            {
                set = xset_get( "task_que" );
                iname = g_strdup( set->icon ? set->icon : GTK_STOCK_ADD );
            }
            else if ( ptask->err_count && ptask->task->type != VFS_FILE_TASK_EXEC )
                iname = g_strdup_printf( "error" );
            else if ( ptask->task->type == 0 || ptask->task->type == 1 || ptask->task->type == 4 )
                iname = g_strdup_printf( "stock_copy" );
            else if ( ptask->task->type == 2 || ptask->task->type == 3 )
                iname = g_strdup_printf( "stock_delete" );
            else if ( ptask->task->type == VFS_FILE_TASK_EXEC && ptask->task->exec_icon )
                iname = g_strdup( ptask->task->exec_icon );
            else
                iname = g_strdup_printf( "gtk-execute" );

            int icon_size = app_settings.small_icon_size;
            if ( icon_size > PANE_MAX_ICON_SIZE )
                icon_size = PANE_MAX_ICON_SIZE;

            pixbuf = gtk_icon_theme_load_icon( gtk_icon_theme_get_default(), iname,
                        icon_size, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );
            g_free( iname );
            if ( !pixbuf )
                pixbuf = gtk_icon_theme_load_icon( gtk_icon_theme_get_default(),
                      "gtk-execute", icon_size, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );
            ptask->pause_change_view = FALSE;
        }
        
        if ( ptask->task->type != VFS_FILE_TASK_EXEC || ptaskt != ptask /* new task */ )
        {
            if ( pixbuf )
                gtk_list_store_set( GTK_LIST_STORE( model ), &it,
                                    TASK_COL_ICON, pixbuf,
                                    TASK_COL_STATUS, status3,
                                    TASK_COL_COUNT, ptask->dsp_file_count,
                                    TASK_COL_PATH, path,
                                    TASK_COL_FILE, file,
                                    TASK_COL_PROGRESS, percent,
                                    TASK_COL_TOTAL, ptask->dsp_size_tally,
                                    TASK_COL_ELAPSED, ptask->dsp_elapsed,
                                    TASK_COL_CURSPEED, ptask->dsp_curspeed,
                                    TASK_COL_CUREST, ptask->dsp_curest,
                                    TASK_COL_AVGSPEED, ptask->dsp_avgspeed,
                                    TASK_COL_AVGEST, ptask->dsp_avgest,
                                    -1 );
            else
                gtk_list_store_set( GTK_LIST_STORE( model ), &it,
                                    TASK_COL_STATUS, status3,
                                    TASK_COL_COUNT, ptask->dsp_file_count,
                                    TASK_COL_PATH, path,
                                    TASK_COL_FILE, file,
                                    TASK_COL_PROGRESS, percent,
                                    TASK_COL_TOTAL, ptask->dsp_size_tally,
                                    TASK_COL_ELAPSED, ptask->dsp_elapsed,
                                    TASK_COL_CURSPEED, ptask->dsp_curspeed,
                                    TASK_COL_CUREST, ptask->dsp_curest,
                                    TASK_COL_AVGSPEED, ptask->dsp_avgspeed,
                                    TASK_COL_AVGEST, ptask->dsp_avgest,
                                    -1 );
        }
        else if ( pixbuf )
            gtk_list_store_set( GTK_LIST_STORE( model ), &it,
                                TASK_COL_ICON, pixbuf,
                                TASK_COL_STATUS, status3,
                                TASK_COL_PROGRESS, percent,
                                TASK_COL_ELAPSED, ptask->dsp_elapsed,
                                -1 );
        else
            gtk_list_store_set( GTK_LIST_STORE( model ), &it,
                                TASK_COL_STATUS, status3,
                                TASK_COL_PROGRESS, percent,
                                TASK_COL_ELAPSED, ptask->dsp_elapsed,
                                -1 );
        
        // Clearing up
        g_free( file );
        g_free( path );
        g_free( status2 );
        g_free( status3 );
        if ( pixbuf )
            g_object_unref( pixbuf );

        if ( !gtk_widget_get_visible( gtk_widget_get_parent( GTK_WIDGET( view ) ) ) )
            show_task_manager( main_window, TRUE );

        update_window_title( NULL, main_window );
    }
    else
    {
        // task is paused
        gtk_list_store_set( GTK_LIST_STORE( model ), &it,
                            TASK_COL_TOTAL, ptask->dsp_size_tally,
                            TASK_COL_ELAPSED, ptask->dsp_elapsed,
                            TASK_COL_CURSPEED, ptask->dsp_curspeed,
                            TASK_COL_CUREST, ptask->dsp_curest,
                            TASK_COL_AVGSPEED, ptask->dsp_avgspeed,
                            TASK_COL_AVGEST, ptask->dsp_avgest,
                            -1 );        
    }
//printf("DONE main_task_view_update_task\n");
}

GtkWidget* main_task_view_new( FMMainWindow* main_window )
{
    char* elements;
    char* space;
    char* name;
    int i, j, width;
    GtkTreeViewColumn* col;
    GtkCellRenderer* renderer;
    GtkCellRenderer* pix_renderer;

    int cols[] = {
        TASK_COL_STATUS,
        TASK_COL_COUNT,
        TASK_COL_PATH,
        TASK_COL_FILE,
        TASK_COL_TO,
        TASK_COL_PROGRESS,
        TASK_COL_TOTAL,
        TASK_COL_STARTED,
        TASK_COL_ELAPSED,
        TASK_COL_CURSPEED,
        TASK_COL_CUREST,
        TASK_COL_AVGSPEED,
        TASK_COL_AVGEST,
        TASK_COL_STARTTIME,
        TASK_COL_ICON,
        TASK_COL_DATA
    };
    int num_cols = G_N_ELEMENTS( cols );

    // Model
    GtkListStore* list = gtk_list_store_new( num_cols, G_TYPE_STRING,
                               G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, 
                               G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING, 
                               G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, 
                               G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, 
                               G_TYPE_INT64, GDK_TYPE_PIXBUF, G_TYPE_POINTER );

    // View
    GtkWidget* view = exo_tree_view_new();
    gtk_tree_view_set_model( GTK_TREE_VIEW( view ), GTK_TREE_MODEL( list ) );
    // gtk_tree_view_set_model adds a ref
    g_object_unref( list );
    exo_tree_view_set_single_click( (ExoTreeView*)view, TRUE );
    gtk_tree_view_set_enable_search( GTK_TREE_VIEW( view ), FALSE );
    //exo_tree_view_set_single_click_timeout( (ExoTreeView*)view, SINGLE_CLICK_TIMEOUT );

    // Columns
    for ( i = 0; i < 13; i++ )
    {
        col = gtk_tree_view_column_new();
        gtk_tree_view_column_set_resizable( col, TRUE );
        gtk_tree_view_column_set_sizing( col, GTK_TREE_VIEW_COLUMN_FIXED );
        gtk_tree_view_column_set_min_width( col, 20 );
        
        // column order
        for ( j = 0; j < 13; j++ )
        {
            if ( xset_get_int( task_names[j], "x" ) == i )
                break;
        }
        if ( j == 13 )
            j = i; // failsafe
        else
        {
            // column width
            width = xset_get_int( task_names[j], "y" );
            if ( width == 0 )
                width = 80;
            gtk_tree_view_column_set_fixed_width ( col, width );
        }
        
        if ( cols[j] == TASK_COL_STATUS )
        {
            // Icon and Text
            renderer = gtk_cell_renderer_text_new();
            //g_object_set( G_OBJECT( renderer ),
                                /* "editable", TRUE, */
            //                    "ellipsize", PANGO_ELLIPSIZE_END, NULL );
            pix_renderer = gtk_cell_renderer_pixbuf_new();
            gtk_tree_view_column_pack_start( col, pix_renderer, FALSE );
            gtk_tree_view_column_pack_end( col, renderer, TRUE );
            gtk_tree_view_column_set_attributes( col, pix_renderer,
                                                 "pixbuf", TASK_COL_ICON, NULL );
            gtk_tree_view_column_set_attributes( col, renderer,
                                                 "text", TASK_COL_STATUS, NULL );
            gtk_tree_view_column_set_expand ( col, FALSE );
            gtk_tree_view_column_set_sizing( col, GTK_TREE_VIEW_COLUMN_FIXED );
            gtk_tree_view_column_set_min_width( col, 60 );
        }
        else if ( cols[j] == TASK_COL_PROGRESS )
        {
            // Progress Bar
            renderer = gtk_cell_renderer_progress_new();
            gtk_tree_view_column_pack_start( col, renderer, TRUE );
            gtk_tree_view_column_set_attributes( col, renderer,
                                                 "value", cols[j], NULL );
        }
        else
        {
            // Text Column
            renderer = gtk_cell_renderer_text_new();
            gtk_tree_view_column_pack_start( col, renderer, TRUE );
            gtk_tree_view_column_set_attributes( col, renderer,
                                                 "text", cols[j], NULL );
                                                
            // ellipsize some columns
            if ( cols[j] == TASK_COL_FILE || cols[j] == TASK_COL_PATH
                                                    || cols[j] == TASK_COL_TO )
            {
                /* wrap to multiple lines 
                GValue val = G_VALUE_INIT;
                g_value_init (&val, G_TYPE_CHAR);
                g_value_set_char (&val, 100);  // set to width of cell?
                g_object_set_property (G_OBJECT (renderer), "wrap-width", &val);
                g_value_unset (&val);
                */
                GValue val = { 0 };   // G_VALUE_INIT (glib>=2.30) caused to slackware issue ?
                g_value_init (&val, G_TYPE_CHAR);
                g_value_set_char (&val, PANGO_ELLIPSIZE_MIDDLE);
                g_object_set_property (G_OBJECT (renderer), "ellipsize", &val);
                g_value_unset (&val);     
                           
            }
        }
        gtk_tree_view_append_column ( GTK_TREE_VIEW( view ), col );
        gtk_tree_view_column_set_title( col, _( task_titles[j] ) );
        gtk_tree_view_column_set_reorderable( col, TRUE );        
        gtk_tree_view_column_set_visible( col, xset_get_b( task_names[j] ) );
        if ( j == TASK_COL_FILE ) //|| j == TASK_COL_PATH || j == TASK_COL_TO
        {
            gtk_tree_view_column_set_sizing( col, GTK_TREE_VIEW_COLUMN_FIXED );
            gtk_tree_view_column_set_min_width( col, 20 );
            // If set_expand is TRUE, columns flicker and adjustment is
            // difficult during high i/o load on some systems
            gtk_tree_view_column_set_expand ( col, FALSE );
        }
    }
    
    // invisible Starttime col for sorting
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable ( col, TRUE );
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start( col, renderer, TRUE );
    gtk_tree_view_column_set_attributes( col, renderer,
                                         "text", TASK_COL_STARTTIME, NULL );
    gtk_tree_view_append_column ( GTK_TREE_VIEW( view ), col );
    gtk_tree_view_column_set_title( col, "StartTime" );
    gtk_tree_view_column_set_reorderable( col, FALSE );        
    gtk_tree_view_column_set_visible( col, FALSE );
    
    // Sort
    if ( GTK_IS_TREE_SORTABLE( list ) )
        gtk_tree_sortable_set_sort_column_id( GTK_TREE_SORTABLE( list ),
                                TASK_COL_STARTTIME, GTK_SORT_ASCENDING );

    gtk_tree_view_set_rules_hint ( GTK_TREE_VIEW( view ), TRUE );
    
    g_signal_connect( view, "row-activated", G_CALLBACK( on_task_row_activated ), NULL );
    g_signal_connect( view, "columns-changed",
                           G_CALLBACK ( on_task_columns_changed ), NULL );
    g_signal_connect( view, "destroy", G_CALLBACK ( on_task_destroy ), NULL );
    g_signal_connect ( view, "button-press-event",
                        G_CALLBACK ( on_task_button_press_event ), main_window );
                        
    // set font
    if ( xset_get_s( "font_task" ) )
    {
        PangoFontDescription* font_desc = pango_font_description_from_string(
                                                xset_get_s( "font_task" ) );
        gtk_widget_modify_font( view, font_desc );
        pango_font_description_free( font_desc );
    }

    return view;
}

// ============== socket commands

gboolean bool( const char* value )
{
    return ( !( value && value[0] ) || !strcmp( value, "1") || 
                    !strcmp( value, "true") || 
                    !strcmp( value, "True") || !strcmp( value, "TRUE") || 
                    !strcmp( value, "yes") || !strcmp( value, "Yes") || 
                    !strcmp( value, "YES") );
}

static gboolean delayed_show_menu( GtkWidget* menu )
{
    FMMainWindow* main_window = fm_main_window_get_last_active();
    if ( main_window )
        gtk_window_present( GTK_WINDOW( main_window ) );        
    gtk_widget_show_all( GTK_WIDGET( menu ) );
    gtk_menu_popup( GTK_MENU( menu ), NULL, NULL, NULL, NULL, 0,
                                                gtk_get_current_event_time() );
    g_signal_connect( G_OBJECT( menu ), "key-press-event",
                  G_CALLBACK( xset_menu_keypress ), NULL );
    g_signal_connect( menu, "selection-done",
                      G_CALLBACK( gtk_widget_destroy ), NULL );
    return FALSE;
}

char main_window_socket_command( char* argv[], char** reply )
{
    int i, j;
    int panel = 0, tab = 0;
    char* window = NULL;
    char* str;
    FMMainWindow* main_window;
    PtkFileBrowser* file_browser;
    GList* l;
    int height, width;
    GtkWidget* widget;
    // must match file-browser.c
    const char* column_titles[] =
    {
        N_( "Name" ), N_( "Size" ), N_( "Type" ),
        N_( "Permission" ), N_( "Owner" ), N_( "Modified" )
    };

    const char* column_names[] =
    {
        "detcol_name", "detcol_size", "detcol_type",
        "detcol_perm", "detcol_owner", "detcol_date"
    };

    *reply = NULL;
    if ( !( argv && argv[0] ) )
    {
        *reply = g_strdup( _("spacefm: invalid socket command\n") );
        return 1;
    }

    // cmd options
    i = 1;
    while ( argv[i] && argv[i][0] == '-' )
    {
        if ( !strcmp( argv[i], "--window" ) )
        {
            if ( !argv[i + 1] ) goto _missing_arg;
            window = argv[i + 1];
            i += 2;
            continue;
        }
        else if ( !strcmp( argv[i], "--panel" ) )
        {
            if ( !argv[i + 1] ) goto _missing_arg;
            panel = atoi( argv[i + 1] );
            i += 2;
            continue;
        }
        else if ( !strcmp( argv[i], "--tab" ) )
        {
            if ( !argv[i + 1] ) goto _missing_arg;
            tab = atoi( argv[i + 1] );
            i += 2;
            continue;
        }
        *reply = g_strdup_printf( _("spacefm: invalid option '%s'\n"), argv[i] );
        return 1;
_missing_arg:        
        *reply = g_strdup_printf( _("spacefm: option %s requires an argument\n"),
                                                                    argv[i] );
        return 1;        
    }
    
    // window
    if ( !window )
    {
        if ( !( main_window = fm_main_window_get_last_active() ) )
        {
            *reply = g_strdup( _("spacefm: invalid window\n") );
            return 2;
        }
    }
    else
    {
        main_window = NULL;
        for ( l = all_windows; l; l = l->next )
        {
            str = g_strdup_printf( "%p", l->data );
            if ( !strcmp( str, window ) )
            {
                main_window = (FMMainWindow*)l->data;
                g_free( str );
                break;
            }
            g_free( str );
        }
        if ( !main_window )
        {
            *reply = g_strdup_printf( _("spacefm: invalid window %s\n"), window );
            return 2;
        }
    }        

    // panel
    if ( !panel )
        panel = main_window->curpanel;
    if ( panel < 1 || panel > 4 )
    {
        *reply = g_strdup_printf( _("spacefm: invalid panel %d\n"), panel );
        return 2;
    }
    if ( !xset_get_b_panel( panel, "show" ) || 
                        gtk_notebook_get_current_page( 
                            GTK_NOTEBOOK( main_window->panel[panel-1] ) ) == -1 )
    {
        *reply = g_strdup_printf( _("spacefm: panel %d is not visible\n"), panel );
        return 2;
    }

    // tab
    if ( !tab )
    {
        tab = gtk_notebook_get_current_page( 
                                GTK_NOTEBOOK( main_window->panel[panel-1] ) ) + 1;
    }
    if ( tab < 1 || tab > gtk_notebook_get_n_pages( 
                                GTK_NOTEBOOK( main_window->panel[panel-1] ) ) )
    {
        *reply = g_strdup_printf( _("spacefm: invalid tab %d\n"), tab );
        return 2;
    }
    file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page( 
                                GTK_NOTEBOOK( main_window->panel[panel-1] ),
                                tab - 1 ) );

    // command
    if ( !strcmp( argv[0], "set" ) )
    {
        if ( !argv[i] )
        {
            *reply = g_strdup( _("spacefm: command set requires an argument\n") );
            return 1;
        }
        if ( !strcmp( argv[i], "window_size" ) || !strcmp( argv[i], "window_position" ) )
        {
            height = width = 0;
            if ( argv[i+1] )
            {
                str = strchr( argv[i+1], 'x' );
                if ( !str )
                {
                    if ( argv[i+2] )
                    {
                        width = atoi( argv[i+1] );
                        height = atoi( argv[i+2] );
                    }
                }
                else
                {
                    str[0] = '\0';
                    width = atoi( argv[i+1] );
                    height = atoi( str + 1 );
                }                        
            }
            if ( height < 1 || width < 1 )
            {
                *reply = g_strdup_printf( _("spacefm: invalid %s value\n"), argv[i] );
                return 2;
            }
            if ( !strcmp( argv[i], "window_size" ) )
                gtk_window_resize( GTK_WINDOW( main_window ), width, height );
            else
                gtk_window_move( GTK_WINDOW( main_window ), width, height );
        }
        else if ( !strcmp( argv[i], "window_maximized" ) )
        {
            if ( bool( argv[i+1] ) )
                gtk_window_maximize( GTK_WINDOW( main_window ) );
            else
                gtk_window_unmaximize( GTK_WINDOW( main_window ) );
        }
        else if ( !strcmp( argv[i], "window_fullscreen" ) )
        {
            xset_set_b( "main_full", bool( argv[i+1] ) );
            on_fullscreen_activate( NULL, main_window );
        }
        else if ( !strcmp( argv[i], "screen_size" ) )
        {
        }
        else if ( !strcmp( argv[i], "window_vslider_top" ) ||
                  !strcmp( argv[i], "window_vslider_bottom" ) ||
                  !strcmp( argv[i], "window_hslider" ) ||
                  !strcmp( argv[i], "window_tslider" ) )
        {
            width = -1;
            if ( argv[i+1] )
                width = atoi( argv[i+1] );
            if ( width < 0 )
            {
                *reply = g_strdup( _("spacefm: invalid slider value\n") );
                return 2;
            }
            if ( !strcmp( argv[i] + 7, "vslider_top" ) )
                widget = main_window->hpane_top;
            else if ( !strcmp( argv[i] + 7, "vslider_bottom" ) )
                widget = main_window->hpane_bottom;
            else if ( !strcmp( argv[i] + 7, "hslider" ) )
                widget = main_window->vpane;
            else
                widget = main_window->task_vpane;
            gtk_paned_set_position( GTK_PANED( widget ), width );
        }
        else if ( !strcmp( argv[i], "focused_panel" ) )
        {
            width = 0;
            if ( argv[i+1] )
            {
                if ( !strcmp( argv[i+1], "prev" ) )
                    width = -1;
                else if ( !strcmp( argv[i+1], "next" ) )
                    width = -2;
                else if ( !strcmp( argv[i+1], "hide" ) )
                    width = -3;
                else
                    width = atoi( argv[i+1] );
            }
            if ( width == 0 || width < -3 || width > 4 )
            {
                *reply = g_strdup( _("spacefm: invalid panel number\n") );
                return 2;
            }
            focus_panel( NULL, (gpointer)main_window, width );
        }
        else if ( !strcmp( argv[i], "focused_pane" ) )
        {
            widget = NULL;
            if ( argv[i+1] )
            {
                if ( !strcmp( argv[i+1], "filelist" ) )
                    widget = file_browser->folder_view;
                else if ( !strcmp( argv[i+1], "devices" ) )
                    widget = file_browser->side_dev;
                else if ( !strcmp( argv[i+1], "bookmarks" ) )
                    widget = file_browser->side_book;
                else if ( !strcmp( argv[i+1], "dirtree" ) )
                    widget = file_browser->side_dir;
                else if ( !strcmp( argv[i+1], "pathbar" ) )
                    widget = file_browser->path_bar;
            }
            if ( GTK_IS_WIDGET( widget ) )
                gtk_widget_grab_focus( widget );
        }
        else if ( !strcmp( argv[i], "current_tab" ) )
        {
            width = 0;
            if ( argv[i+1] )
            {
                if ( !strcmp( argv[i+1], "prev" ) )
                    width = -1;
                else if ( !strcmp( argv[i+1], "next" ) )
                    width = -2;
                else if ( !strcmp( argv[i+1], "close" ) )
                    width = -3;
                else
                    width = atoi( argv[i+1] );
            }
            if ( width == 0 || width < -3 || width > gtk_notebook_get_n_pages( 
                                GTK_NOTEBOOK( main_window->panel[panel-1] ) ) )
            {
                *reply = g_strdup( _("spacefm: invalid tab number\n") );
                return 2;
            }
            ptk_file_browser_go_tab( NULL, file_browser, width );
        }
        else if ( !strcmp( argv[i], "tab_count" ) )
        {}
        else if ( !strcmp( argv[i], "new_tab" ) )
        {
            focus_panel( NULL, (gpointer)main_window, panel );
            if ( !( argv[i+1] && g_file_test( argv[i+1], G_FILE_TEST_IS_DIR ) ) )
                ptk_file_browser_new_tab( NULL, file_browser );
            else
                fm_main_window_add_new_tab( main_window, argv[i+1] );
            main_window_get_counts( file_browser, &i, &tab, &j );
            *reply = g_strdup_printf( "#!%s\nnew_tab_window=%p\nnew_tab_panel=%d\nnew_tab_number=%d\n",
                                        BASHPATH, main_window, panel, tab );
        }
        else if ( g_str_has_suffix( argv[i], "_visible" ) )
        {
            gboolean use_mode = FALSE;
            if ( g_str_has_prefix( argv[i], "devices_" ) )
            {
                str = "show_devmon";
                use_mode = TRUE;
            }
            else if ( g_str_has_prefix( argv[i], "bookmarks_" ) )
            {
                str = "show_book";
                use_mode = TRUE;
            }
            else if ( g_str_has_prefix( argv[i], "dirtree_" ) )
            {
                str = "show_dirtree";
                use_mode = TRUE;
            }
            else if ( g_str_has_prefix( argv[i], "toolbar_" ) )
            {
                str = "show_toolbox";
                use_mode = TRUE;
            }
            else if ( g_str_has_prefix( argv[i], "sidetoolbar_" ) )
            {
                str = "show_sidebar";
                use_mode = TRUE;
            }
            else if ( g_str_has_prefix( argv[i], "hidden_files_" ) )
                str = "show_hidden";
            else if ( g_str_has_prefix( argv[i], "panel" ) )
            {
                j = argv[i][5] - 48;
                if ( j < 1 || j > 4 )
                {
                    *reply = g_strdup_printf( _("spacefm: invalid property %s\n"), 
                                                                    argv[i] );
                    return 2;
                }
                xset_set_b_panel( j, "show", bool( argv[i+1] ) );
                show_panels_all_windows( NULL, main_window );
                return 0;
            }
            else
                str = NULL;
            if ( !str )
                goto _invalid_set;
            if ( use_mode )
                xset_set_b_panel_mode( panel, str,
                                        main_window->panel_context[panel-1],
                                        bool( argv[i+1] ) );
            else
                xset_set_b_panel( panel, str, bool( argv[i+1] ) );
            update_views_all_windows( NULL, file_browser );
        }
        else if ( !strcmp( argv[i], "panel_hslider_top" ) ||
                  !strcmp( argv[i], "panel_hslider_bottom" ) ||
                  !strcmp( argv[i], "panel_vslider" ) )
        {
            width = -1;
            if ( argv[i+1] )
                width = atoi( argv[i+1] );
            if ( width < 0 )
            {
                *reply = g_strdup( _("spacefm: invalid slider value\n") );
                return 2;
            }
            if ( !strcmp( argv[i] + 6, "hslider_top" ) )
                widget = file_browser->side_vpane_top;
            else if ( !strcmp( argv[i] + 6, "hslider_bottom" ) )
                widget = file_browser->side_vpane_bottom;
            else
                widget = file_browser->hpane;
            gtk_paned_set_position( GTK_PANED( widget ), width );
            ptk_file_browser_slider_release( NULL, NULL, file_browser );
            update_views_all_windows( NULL, file_browser );
        }
        else if ( !strcmp( argv[i], "column_width" ) )
        {   // COLUMN WIDTH
            width = 0;
            if ( argv[i+1] && argv[i+2] )
                width = atoi( argv[i+2] );
            if ( width < 1 )
            {
                *reply = g_strdup( _("spacefm: invalid column width\n") );
                return 2;
            }
            if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
            {
                GtkTreeViewColumn* col;
                for ( j = 0; j < 6; j++ )
                {
                    col = gtk_tree_view_get_column( GTK_TREE_VIEW( 
                                                file_browser->folder_view ), j );
                    if ( !col )
                        continue;
                    str = (char*)gtk_tree_view_column_get_title( col );
                    if ( !g_strcmp0( argv[i+1], str ) )
                        break;
                    if ( !g_strcmp0( argv[i+1], "name" ) && 
                                        !g_strcmp0( str, _(column_titles[0]) ) )
                        break;
                    else if ( !g_strcmp0( argv[i+1], "size" ) && 
                                        !g_strcmp0( str, _(column_titles[1]) ) )
                        break;
                    else if ( !g_strcmp0( argv[i+1], "type" ) && 
                                        !g_strcmp0( str, _(column_titles[2]) ) )
                        break;
                    else if ( !g_strcmp0( argv[i+1], "permission" ) && 
                                        !g_strcmp0( str, _(column_titles[3]) ) )
                        break;
                    else if ( !g_strcmp0( argv[i+1], "owner" ) && 
                                        !g_strcmp0( str, _(column_titles[4]) ) )
                        break;
                    else if ( !g_strcmp0( argv[i+1], "modified" ) && 
                                        !g_strcmp0( str, _(column_titles[5]) ) )
                        break;
                }
                if ( j == 6 )
                {
                    *reply = g_strdup_printf( _("spacefm: invalid column name '%s'\n"),
                                                                    argv[i+1] );
                    return 2;
                }
                gtk_tree_view_column_set_fixed_width( col, width );
            }
        }
        else if ( !strcmp( argv[i], "sort_by" ) )
        {   // COLUMN
            j = -10;
            if ( !argv[i+1] )
            {}
            else if ( !strcmp( argv[i+1], "name" ) )
                j = PTK_FB_SORT_BY_NAME;
            else if ( !strcmp( argv[i+1], "size" ) )
                j = PTK_FB_SORT_BY_SIZE;
            else if ( !strcmp( argv[i+1], "type" ) )
                j = PTK_FB_SORT_BY_TYPE;
            else if ( !strcmp( argv[i+1], "permission" ) )
                j = PTK_FB_SORT_BY_PERM;
            else if ( !strcmp( argv[i+1], "owner" ) )
                j = PTK_FB_SORT_BY_OWNER;
            else if ( !strcmp( argv[i+1], "modified" ) )
                j = PTK_FB_SORT_BY_MTIME;
            
            if ( j == -10 )
            {
                *reply = g_strdup_printf( _("spacefm: invalid column name '%s'\n"),
                                                                argv[i+1] );
                return 2;
            }
            ptk_file_browser_set_sort_order( file_browser, j );
        }
        else if ( g_str_has_prefix( argv[i], "sort_" ) )
        {
            if ( !strcmp( argv[i] + 5, "ascend" ) )
            {
                ptk_file_browser_set_sort_type( file_browser, bool( argv[i+1] ) ?
                                        GTK_SORT_ASCENDING : GTK_SORT_DESCENDING );
                return 0;
            }
            else if ( !strcmp( argv[i] + 5, "natural" ) )
            {
                str = "sortx_natural";
                xset_set_b( str, bool( argv[i+1] ) );
            }
            else if ( !strcmp( argv[i] + 5, "case" ) )
            {
                str = "sortx_case";
                xset_set_b( str, bool( argv[i+1] ) );
            }
            else if ( !strcmp( argv[i] + 5, "hidden_first" ) )
            {
                str = bool( argv[i+1] ) ? "sortx_hidfirst" : "sortx_hidlast";
                xset_set_b( str, TRUE );
            }
            else if ( !strcmp( argv[i] + 5, "first" ) )
            {
                if ( !g_strcmp0( argv[i+1], "files" ) )
                    str = "sortx_files";
                else if ( !g_strcmp0( argv[i+1], "folders" ) )
                    str = "sortx_folders";
                else if ( !g_strcmp0( argv[i+1], "mixed" ) )
                    str = "sortx_mix";
                else
                {
                    *reply = g_strdup_printf( _("spacefm: invalid %s value\n"), argv[i] );
                    return 2;
                }
            }
            else
                goto _invalid_set;
            ptk_file_browser_set_sort_extra( file_browser, str );
        }
        else if ( !strcmp( argv[i], "show_thumbnails" ) )
        {
            if ( app_settings.show_thumbnail != bool( argv[i+1] ) )
                main_window_toggle_thumbnails_all_windows();
        }
        else if ( !strcmp( argv[i], "large_icons" ) )
        {
            if ( file_browser->view_mode != PTK_FB_ICON_VIEW )
            {
                xset_set_b_panel_mode( panel, "list_large",
                                        main_window->panel_context[panel-1],
                                        bool( argv[i+1] ) );
                update_views_all_windows( NULL, file_browser );
            }
        }
        else if ( !strcmp( argv[i], "statusbar_text" ) )
        {
            if ( !( argv[i+1] && argv[i+1][0] ) )
            {
                g_free( file_browser->status_bar_custom );
                file_browser->status_bar_custom = NULL;
            }
            else
            {
                g_free( file_browser->status_bar_custom );                
                file_browser->status_bar_custom = g_strdup( argv[i+1] );
            }
            fm_main_window_update_status_bar( main_window, file_browser );
        }
        else if ( !strcmp( argv[i], "pathbar_text" ) )
        {   // TEXT [[SELSTART] SELEND]
            if ( !GTK_IS_WIDGET( file_browser->path_bar ) )
                return 0;
            if ( !( argv[i+1] && argv[i+1][0] ) )
            {
                gtk_entry_set_text( GTK_ENTRY( file_browser->path_bar ), "" );
            }
            else
            {
                gtk_entry_set_text( GTK_ENTRY( file_browser->path_bar ), 
                                                                    argv[i+1] );
                if ( !argv[i+2] )
                {
                    width = 0;
                    height = -1;
                }
                else
                {
                    width = atoi( argv[i+2] );
                    height = argv[i+3] ? atoi( argv[i+3] ) : -1;
                }
                gtk_editable_set_position( GTK_EDITABLE( 
                                           file_browser->path_bar ), -1 );
                gtk_editable_select_region( GTK_EDITABLE( 
                                           file_browser->path_bar ), width, height );
                gtk_widget_grab_focus( file_browser->path_bar );
            }
        }
        else if ( !strcmp( argv[i], "clipboard_text" ) ||
                  !strcmp( argv[i], "clipboard_primary_text" ) )
        {
            if ( argv[i+1] && !g_utf8_validate( argv[i+1], -1, NULL ) )
            {
                *reply = g_strdup( _("spacefm: text is not valid UTF-8\n") );
                return 2;
            }
            GtkClipboard * clip = gtk_clipboard_get( 
                        !strcmp( argv[i], "clipboard_text" ) ? 
                                                    GDK_SELECTION_CLIPBOARD :
                                                    GDK_SELECTION_PRIMARY );
            str = unescape( argv[i+1] ? argv[i+1] : "" );
            gtk_clipboard_set_text( clip, str, -1 );
            g_free( str );
        }
        else if ( !strcmp( argv[i], "clipboard_from_file" ) || 
                  !strcmp( argv[i], "clipboard_primary_from_file" ) )
        {
            if ( !argv[i+1] )
            {
                *reply = g_strdup_printf( 
                        _("spacefm: %s requires a file path\n"), argv[i] );
                return 1;
            }
            if ( !g_file_get_contents( argv[i+1], &str, NULL, NULL ) )
            {
                *reply = g_strdup_printf( 
                        _("spacefm: error reading file '%s'\n"), argv[i+1] );
                return 2;
            }
            if ( !g_utf8_validate( str, -1, NULL ) )
            {
                *reply = g_strdup_printf( 
                        _("spacefm: file '%s' does not contain valid UTF-8 text\n"),
                                                                    argv[i+1] );
                g_free( str );
                return 2;
            }
            GtkClipboard * clip = gtk_clipboard_get( 
                        !strcmp( argv[i], "clipboard_from_file" ) ? 
                                                    GDK_SELECTION_CLIPBOARD :
                                                    GDK_SELECTION_PRIMARY );
            gtk_clipboard_set_text( clip, str, -1 );
            g_free( str );
        }
        else if ( !strcmp( argv[i], "clipboard_cut_files" ) || 
                  !strcmp( argv[i], "clipboard_copy_files" ) )
        {
            ptk_clipboard_copy_file_list( argv + i + 1,
                                    !strcmp( argv[i], "clipboard_copy_files" ) );
        }
        else if ( !strcmp( argv[i], "selected_filenames" ) ||
                  !strcmp( argv[i], "selected_files" ) )
        {
            if ( !argv[i+1] || argv[i+1][0] == '\0' )
                // unselect all
                ptk_file_browser_select_file_list( file_browser, NULL, FALSE );
            else
                ptk_file_browser_select_file_list( file_browser, argv + i + 1,
                                                                        TRUE );
        }
        else if ( !strcmp( argv[i], "selected_pattern" ) )
        {
            if ( !argv[i+1] )
                // unselect all
                ptk_file_browser_select_file_list( file_browser, NULL, FALSE );
            else
                ptk_file_browser_select_pattern( NULL, file_browser, argv[i+1] );
        }
        else if ( !strcmp( argv[i], "current_dir" ) )
        {
            if ( !argv[i+1] )
            {
                *reply = g_strdup_printf( 
                        _("spacefm: %s requires a directory path\n"), argv[i] );
                return 1;
            }
            if ( !g_file_test( argv[i+1], G_FILE_TEST_IS_DIR ) )
            {
                *reply = g_strdup_printf( 
                        _("spacefm: directory '%s' does not exist\n"), argv[i+1] );
                return 1;
            }
            ptk_file_browser_chdir( file_browser, argv[i+1],
                                                    PTK_FB_CHDIR_ADD_HISTORY );      
        }
        else if ( !strcmp( argv[i], "edit_file" ) ) // deprecated >= 0.8.7
        {
            if ( ( argv[i+1] && argv[i+1][0] ) && 
                                g_file_test( argv[i+1], G_FILE_TEST_IS_REGULAR ) )
                xset_edit( GTK_WIDGET( file_browser ), argv[i+1], FALSE, TRUE );
        }
        else if ( !strcmp( argv[i], "run_in_terminal" ) ) // deprecated >= 0.8.7
        {
            if ( ( argv[i+1] && argv[i+1][0] ) )
            {
                str = g_strjoinv( " ", &argv[i+1] );
                if ( str && str[0] )
                {
                    // async task
                    PtkFileTask* task = ptk_file_exec_new( "Run In Terminal",
                                    ptk_file_browser_get_cwd( file_browser ),
                                    GTK_WIDGET( file_browser ),
                                    main_window->task_view );
                    task->task->exec_command = str;
                    task->task->exec_terminal = TRUE;
                    task->task->exec_sync = FALSE;
                    task->task->exec_export = FALSE;
                    ptk_file_task_run( task );
                }
                else
                    g_free( str );
            }
        }
        else
        {
_invalid_set:
            *reply = g_strdup_printf( _("spacefm: invalid property %s\n"), argv[i] );
            return 1;
        }
    }
    else if ( !strcmp( argv[0], "get" ) )
    {
        // get
        if ( !argv[i] )
        {
            *reply = g_strdup_printf( _("spacefm: command %s requires an argument\n"),
                                                                    argv[0] );
            return 1;
        }
        if ( !strcmp( argv[i], "window_size" ) || !strcmp( argv[i], "window_position" ) )
        {
            if ( !strcmp( argv[i], "window_size" ) )
                gtk_window_get_size( GTK_WINDOW( main_window ), &width, &height );
            else
                gtk_window_get_position( GTK_WINDOW( main_window ), &width, &height );
            *reply = g_strdup_printf( "%dx%d\n", width, height );
        }
        else if ( !strcmp( argv[i], "window_maximized" ) )
        {
            *reply = g_strdup_printf( "%d\n", !!main_window->maximized );
        }
        else if ( !strcmp( argv[i], "window_fullscreen" ) )
        {
            *reply = g_strdup_printf( "%d\n", !!main_window->fullscreen );
        }
        else if ( !strcmp( argv[i], "screen_size" ) )
        {
            width = gdk_screen_get_width( 
                            gtk_widget_get_screen( (GtkWidget*)main_window ) );
            height = gdk_screen_get_height( 
                            gtk_widget_get_screen( (GtkWidget*)main_window ) );
            *reply = g_strdup_printf( "%dx%d\n", width, height );
        }
        else if ( !strcmp( argv[i], "window_vslider_top" ) ||
                  !strcmp( argv[i], "window_vslider_bottom" ) ||
                  !strcmp( argv[i], "window_hslider" ) ||
                  !strcmp( argv[i], "window_tslider" ) )
        {
            if ( !strcmp( argv[i] + 7, "vslider_top" ) )
                widget = main_window->hpane_top;
            else if ( !strcmp( argv[i] + 7, "vslider_bottom" ) )
                widget = main_window->hpane_bottom;
            else if ( !strcmp( argv[i] + 7, "hslider" ) )
                widget = main_window->vpane;
            else
                widget = main_window->task_vpane;
            *reply = g_strdup_printf( "%d\n", 
                                gtk_paned_get_position( GTK_PANED( widget ) ) );
        }
        else if ( !strcmp( argv[i], "focused_panel" ) )
        {
            *reply = g_strdup_printf( "%d\n", main_window->curpanel );
        }
        else if ( !strcmp( argv[i], "focused_pane" ) )
        {
            if ( file_browser->folder_view && 
                            gtk_widget_is_focus( file_browser->folder_view ) )
                str = "filelist";
            else if ( file_browser->side_dev &&
                            gtk_widget_is_focus( file_browser->side_dev ) )
                str = "devices";
            else if ( file_browser->side_book &&
                            gtk_widget_is_focus( file_browser->side_book ) )
                str = "bookmarks";
            else if ( file_browser->side_dir &&
                            gtk_widget_is_focus( file_browser->side_dir ) )
                str = "dirtree";
            else if ( file_browser->path_bar &&
                            gtk_widget_is_focus( file_browser->path_bar ) )
                str = "pathbar";
            else
                str = NULL;
            if ( str )
                *reply = g_strdup_printf( "%s\n", str );
        }
        else if ( !strcmp( argv[i], "current_tab" ) )
        {
            *reply = g_strdup_printf( "%d\n", gtk_notebook_page_num ( 
                            GTK_NOTEBOOK( main_window->panel[panel-1] ),
                                            GTK_WIDGET( file_browser ) ) + 1 );
        }
        else if ( !strcmp( argv[i], "tab_count" ) )
        {
            main_window_get_counts( file_browser, &panel, &tab, &j );
            *reply = g_strdup_printf( "%d\n", tab );
        }
        else if ( !strcmp( argv[i], "new_tab" ) )
        {}
        else if ( g_str_has_suffix( argv[i], "_visible" ) )
        {
            gboolean use_mode = FALSE;
            if ( g_str_has_prefix( argv[i], "devices_" ) )
            {
                str = "show_devmon";
                use_mode = TRUE;
            }
            else if ( g_str_has_prefix( argv[i], "bookmarks_" ) )
            {
                str = "show_book";
                use_mode = TRUE;
            }
            else if ( g_str_has_prefix( argv[i], "dirtree_" ) )
            {
                str = "show_dirtree";
                use_mode = TRUE;
            }
            else if ( g_str_has_prefix( argv[i], "toolbar_" ) )
            {
                str = "show_toolbox";
                use_mode = TRUE;
            }
            else if ( g_str_has_prefix( argv[i], "sidetoolbar_" ) )
            {
                str = "show_sidebar";
                use_mode = TRUE;
            }
            else if ( g_str_has_prefix( argv[i], "hidden_files_" ) )
                str = "show_hidden";
            else if ( g_str_has_prefix( argv[i], "panel" ) )
            {
                j = argv[i][5] - 48;
                if ( j < 1 || j > 4 )
                {
                    *reply = g_strdup_printf( _("spacefm: invalid property %s\n"), 
                                                                    argv[i] );
                    return 2;
                }
                *reply = g_strdup_printf( "%d\n", xset_get_b_panel( j, "show" ) );
                return 0;
            }
            else
                str = NULL;
            if ( !str )
                goto _invalid_get;
            if ( use_mode )
                *reply = g_strdup_printf( "%d\n", !!xset_get_b_panel_mode( panel,
                                str, main_window->panel_context[panel-1] ) );
            else
                *reply = g_strdup_printf( "%d\n", !!xset_get_b_panel( panel,
                                                                    str ) );
        }
        else if ( !strcmp( argv[i], "panel_hslider_top" ) ||
                  !strcmp( argv[i], "panel_hslider_bottom" ) ||
                  !strcmp( argv[i], "panel_vslider" ) )
        {
            if ( !strcmp( argv[i] + 6, "hslider_top" ) )
                widget = file_browser->side_vpane_top;
            else if ( !strcmp( argv[i] + 6, "hslider_bottom" ) )
                widget = file_browser->side_vpane_bottom;
            else
                widget = file_browser->hpane;
            *reply = g_strdup_printf( "%d\n", 
                                gtk_paned_get_position( GTK_PANED( widget ) ) );
        }
        else if ( !strcmp( argv[i], "column_width" ) )
        {   // COLUMN
            if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
            {
                GtkTreeViewColumn* col;
                for ( j = 0; j < 6; j++ )
                {
                    col = gtk_tree_view_get_column( GTK_TREE_VIEW( 
                                                file_browser->folder_view ), j );
                    if ( !col )
                        continue;
                    str = (char*)gtk_tree_view_column_get_title( col );
                    if ( !g_strcmp0( argv[i+1], str ) )
                        break;
                    if ( !g_strcmp0( argv[i+1], "name" ) && 
                                        !g_strcmp0( str, _(column_titles[0]) ) )
                        break;
                    else if ( !g_strcmp0( argv[i+1], "size" ) && 
                                        !g_strcmp0( str, _(column_titles[1]) ) )
                        break;
                    else if ( !g_strcmp0( argv[i+1], "type" ) && 
                                        !g_strcmp0( str, _(column_titles[2]) ) )
                        break;
                    else if ( !g_strcmp0( argv[i+1], "permission" ) && 
                                        !g_strcmp0( str, _(column_titles[3]) ) )
                        break;
                    else if ( !g_strcmp0( argv[i+1], "owner" ) && 
                                        !g_strcmp0( str, _(column_titles[4]) ) )
                        break;
                    else if ( !g_strcmp0( argv[i+1], "modified" ) && 
                                        !g_strcmp0( str, _(column_titles[5]) ) )
                        break;
                }
                if ( j == 6 )
                {
                    *reply = g_strdup_printf( _("spacefm: invalid column name '%s'\n"),
                                                                    argv[i+1] );
                    return 2;
                }
                *reply = g_strdup_printf( "%d\n", 
                                        gtk_tree_view_column_get_width( col ) );
            }
        }
        else if ( !strcmp( argv[i], "sort_by" ) )
        {   // COLUMN
            switch ( file_browser->sort_order ) {
                case PTK_FB_SORT_BY_NAME:
                    str = "name";
                    break;
                case PTK_FB_SORT_BY_SIZE:
                    str = "size";
                    break;
                case PTK_FB_SORT_BY_TYPE:
                    str = "type";
                    break;
                case PTK_FB_SORT_BY_PERM:
                    str = "permission";
                    break;
                case PTK_FB_SORT_BY_OWNER:
                    str = "owner";
                    break;
                case PTK_FB_SORT_BY_MTIME:
                    str = "modified";
                    break;
                default:
                    return 0;
            }
            *reply = g_strdup_printf( "%s\n", str );
        }
        else if ( g_str_has_prefix( argv[i], "sort_" ) )
        {
            if ( !strcmp( argv[i] + 5, "ascend" ) )
                *reply = g_strdup_printf( "%d\n", 
                            file_browser->sort_type == GTK_SORT_ASCENDING ? 1 : 0 );
            else if ( !strcmp( argv[i] + 5, "natural" ) )
                *reply = g_strdup_printf( "%d\n", 
                        xset_get_b_panel( file_browser->mypanel, "sort_extra" ) ?
                                                                        1 : 0 );
            else if ( !strcmp( argv[i] + 5, "case" ) )
                *reply = g_strdup_printf( "%d\n",
                        xset_get_b_panel( file_browser->mypanel, "sort_extra" ) &&
                        xset_get_int_panel( file_browser->mypanel, "sort_extra",
                                              "x" ) == XSET_B_TRUE ? 1 : 0 );
            else if ( !strcmp( argv[i] + 5, "hidden_first" ) )
                *reply = g_strdup_printf( "%d\n",
                        xset_get_int_panel( file_browser->mypanel, "sort_extra",
                                              "z" ) == XSET_B_TRUE ? 1 : 0 );
            else if ( !strcmp( argv[i] + 5, "first" ) )
            {
                switch ( xset_get_int_panel( file_browser->mypanel, "sort_extra",
                                                                        "y" ) ) {
                    case 0:
                        str = "mixed";
                        break;
                    case 1:
                        str = "folders";
                        break;
                    case 2:
                        str = "files";
                        break;
                    default:
                        return 0;  //failsafe for known
                }
                *reply = g_strdup_printf( "%s\n", str );
            }
            else
                goto _invalid_get;
        }
        else if ( !strcmp( argv[i], "show_thumbnails" ) )
        {
            *reply = g_strdup_printf( "%d\n", app_settings.show_thumbnail ?
                                                                    1 : 0 );
        }
        else if ( !strcmp( argv[i], "large_icons" ) )
        {
            *reply = g_strdup_printf( "%d\n", file_browser->large_icons ?
                                                                    1 : 0 );
        }
        else if ( !strcmp( argv[i], "statusbar_text" ) )
        {
            *reply = g_strdup_printf( "%s\n", gtk_label_get_text( 
                                    GTK_LABEL( file_browser->status_label ) ) );
        }
        else if ( !strcmp( argv[i], "pathbar_text" ) )
        {
            if ( GTK_IS_WIDGET( file_browser->path_bar ) )
                *reply = g_strdup_printf( "%s\n", gtk_entry_get_text( 
                                    GTK_ENTRY( file_browser->path_bar ) ) );
        }
        else if ( !strcmp( argv[i], "clipboard_text" ) ||
                  !strcmp( argv[i], "clipboard_primary_text" ) )
        {
            GtkClipboard * clip = gtk_clipboard_get( 
                        !strcmp( argv[i], "clipboard_text" ) ? 
                                                    GDK_SELECTION_CLIPBOARD :
                                                    GDK_SELECTION_PRIMARY );
            *reply = gtk_clipboard_wait_for_text( clip );
        }
        else if ( !strcmp( argv[i], "clipboard_from_file" ) || 
                  !strcmp( argv[i], "clipboard_primary_from_file" ) )
        {
        }
        else if ( !strcmp( argv[i], "clipboard_cut_files" ) || 
                  !strcmp( argv[i], "clipboard_copy_files" ) )
        {
            GtkClipboard * clip = gtk_clipboard_get( GDK_SELECTION_CLIPBOARD );
            GdkAtom gnome_target;
            GdkAtom uri_list_target;
            GtkSelectionData* sel_data;

            gnome_target = gdk_atom_intern( "x-special/gnome-copied-files", FALSE );
            sel_data = gtk_clipboard_wait_for_contents( clip, gnome_target );
            if ( !sel_data )
            {
                uri_list_target = gdk_atom_intern( "text/uri-list", FALSE );
                sel_data = gtk_clipboard_wait_for_contents( clip, uri_list_target );
                if ( !sel_data )
                    return 0;
            }
            if ( gtk_selection_data_get_length( sel_data ) <= 0 || 
                            gtk_selection_data_get_format( sel_data ) != 8 )
            {
                gtk_selection_data_free( sel_data );
                return 0;
            }
            if ( 0 == strncmp( ( char* ) 
                        gtk_selection_data_get_data( sel_data ), "cut", 3 ) )
            {
                if ( !strcmp( argv[i], "clipboard_copy_files" ) )
                {
                    gtk_selection_data_free( sel_data );
                    return 0;
                }
            }
            else if ( !strcmp( argv[i], "clipboard_cut_files" ) )
            {
                gtk_selection_data_free( sel_data );
                return 0;
            }
            str = gtk_clipboard_wait_for_text( clip );
            gtk_selection_data_free( sel_data );
            if ( !( str && str[0] ) )
            {
                g_free( str );
                return 0;
            }
            // build bash array
            char** pathv = g_strsplit( str, "\n", 0 );
            g_free( str );
            GString* gstr = g_string_new( "(" );
            j = 0;
            while ( pathv[j] )
            {
                if ( pathv[j][0] )
                {
                    str = bash_quote( pathv[j] );
                    g_string_append_printf( gstr, "%s ", str );
                    g_free( str );
                }
                j++;
            }
            g_strfreev( pathv );
            g_string_append_printf( gstr, ")\n" );
            *reply = g_string_free( gstr, FALSE );
        }
        else if ( !strcmp( argv[i], "selected_filenames" ) ||
                  !strcmp( argv[i], "selected_files" ) )
        {
            GList* sel_files;
            VFSFileInfo* file;
            
            sel_files = ptk_file_browser_get_selected_files( file_browser );
            if ( !sel_files )
                return 0;
            
            // build bash array
            GString* gstr = g_string_new( "(" );
            for ( l = sel_files; l; l = l->next )
            {
                file = vfs_file_info_ref( (VFSFileInfo*)l->data );
                if ( file )
                {
                    str = bash_quote( vfs_file_info_get_name( file ) );
                    g_string_append_printf( gstr, "%s ", str );
                    g_free( str );
                    vfs_file_info_unref( file );
                }
            }
            vfs_file_info_list_free( sel_files );
            g_string_append_printf( gstr, ")\n" );
            *reply = g_string_free( gstr, FALSE );
        }
        else if ( !strcmp( argv[i], "selected_pattern" ) )
        {
        }
        else if ( !strcmp( argv[i], "current_dir" ) )
        {
            *reply = g_strdup_printf( "%s\n", 
                                    ptk_file_browser_get_cwd( file_browser ) );
        }
        else if ( !strcmp( argv[i], "edit_file" ) )
        { }
        else if ( !strcmp( argv[i], "run_in_terminal" ) )
        { }
        else
        {
_invalid_get:
            *reply = g_strdup_printf( _("spacefm: invalid property %s\n"), argv[i] );
            return 1;
        }        
    }
    else if ( !strcmp( argv[0], "set-task" ) )
    {   // TASKNUM PROPERTY [VALUE]
        if ( !( argv[i] && argv[i+1] ) )
        {
            *reply = g_strdup_printf( _("spacefm: %s requires two arguments\n"),
                                                                        argv[0] );
            return 1;        
        }
        
        // find task
        GtkTreeIter it;
        PtkFileTask* ptask = NULL;
        GtkTreeModel* model = gtk_tree_view_get_model( 
                                        GTK_TREE_VIEW( main_window->task_view ) );
        if ( gtk_tree_model_get_iter_first( model, &it ) )
        {
            do
            {
                gtk_tree_model_get( model, &it, TASK_COL_DATA, &ptask, -1 );
                str = g_strdup_printf( "%p", ptask );
                if ( !strcmp( str, argv[i] ) )
                {
                    g_free( str );
                    break;
                }
                g_free( str );
                ptask=  NULL;
            }
            while ( gtk_tree_model_iter_next( model, &it ) );
        }
        if ( !ptask )
        {
            *reply = g_strdup_printf( _("spacefm: invalid task '%s'\n"), argv[i] );
            return 2;
        }
        if ( ptask->task->type != VFS_FILE_TASK_EXEC )
        {
            *reply = g_strdup_printf( _("spacefm: internal task %s is read-only\n"),
                                                                        argv[i] );
            return 2;
        }
        
        // set model value
        if ( !strcmp( argv[i+1], "icon" ) )
        {
            g_mutex_lock( ptask->task->mutex );
            g_free( ptask->task->exec_icon );
            ptask->task->exec_icon = g_strdup( argv[i+2] );
            ptask->pause_change_view = ptask->pause_change = TRUE;
            g_mutex_unlock( ptask->task->mutex );
            return 0;
        }
        else if ( !strcmp( argv[i+1], "count" ) )
            j = TASK_COL_COUNT;
        else if ( !strcmp( argv[i+1], "folder" ) || !strcmp( argv[i+1], "from" ) )
            j = TASK_COL_PATH;
        else if ( !strcmp( argv[i+1], "item" ) )
            j = TASK_COL_FILE;
        else if ( !strcmp( argv[i+1], "to" ) )
            j = TASK_COL_TO;
        else if ( !strcmp( argv[i+1], "progress" ) )
        {
            if ( !argv[i+2] )
                ptask->task->percent = 50;
            else
            {
                j = atoi( argv[i+2] );
                if ( j < 0 ) j = 0;
                if ( j > 100 ) j = 100;
                ptask->task->percent = j;
            }
            ptask->task->custom_percent = !!argv[i+2];
            ptask->pause_change_view = ptask->pause_change = TRUE;
            return 0;
        }
        else if ( !strcmp( argv[i+1], "total" ) )
            j = TASK_COL_TOTAL;
        else if ( !strcmp( argv[i+1], "curspeed" ) )
            j = TASK_COL_CURSPEED;
        else if ( !strcmp( argv[i+1], "curremain" ) )
            j = TASK_COL_CUREST;
        else if ( !strcmp( argv[i+1], "avgspeed" ) )
            j = TASK_COL_AVGSPEED;
        else if ( !strcmp( argv[i+1], "avgremain" ) )
            j = TASK_COL_AVGEST;
        else if ( !strcmp( argv[i+1], "elapsed" ) || 
                  !strcmp( argv[i+1], "started" ) || 
                  !strcmp( argv[i+1], "status" ) )
        {
            *reply = g_strdup_printf( _("spacefm: task property '%s' is read-only\n"),
                                                                    argv[i+1] );
            return 2;
        }
        else if ( !strcmp( argv[i+1], "queue_state" ) )
        {
            if ( !argv[i+2] || !g_strcmp0( argv[i+2], "run" ) )
                ptk_file_task_pause( ptask, VFS_FILE_TASK_RUNNING );
            else if ( !strcmp( argv[i+2], "pause" ) )
                ptk_file_task_pause( ptask, VFS_FILE_TASK_PAUSE );
            else if ( !strcmp( argv[i+2], "queue" ) || !strcmp( argv[i+2], "queued" ) )
                ptk_file_task_pause( ptask, VFS_FILE_TASK_QUEUE );
            else if ( !strcmp( argv[i+2], "stop" ) )
                on_task_stop( NULL, main_window->task_view, 
                                            xset_get( "task_stop_all" ), NULL );
            else
            {
                *reply = g_strdup_printf( _("spacefm: invalid queue_state '%s'\n"),
                                                                        argv[i+2] );
                return 2;
            }
            main_task_start_queued( main_window->task_view, NULL );
            return 0;
        }
        else if ( !strcmp( argv[i+1], "popup_handler" ) )
        {
            g_free( ptask->pop_handler );
            if ( argv[i+2] && argv[i+2][0] != '\0' )
                ptask->pop_handler = g_strdup( argv[i+2] );
            else
                ptask->pop_handler = NULL;
            return 0;
        }
        else
        {
            *reply = g_strdup_printf( _("spacefm: invalid task property '%s'\n"),
                                                                    argv[i+1] );
            return 2;
        }
        gtk_list_store_set( GTK_LIST_STORE( model ), &it, j, argv[i+2], -1 );
    }
    else if ( !strcmp( argv[0], "get-task" ) )
    {   // TASKNUM PROPERTY
        if ( !( argv[i] && argv[i+1] ) )
        {
            *reply = g_strdup_printf( _("spacefm: %s requires two arguments\n"),
                                                                        argv[0] );
            return 1;        
        }
        
        // find task
        GtkTreeIter it;
        PtkFileTask* ptask = NULL;
        GtkTreeModel* model = gtk_tree_view_get_model( 
                                        GTK_TREE_VIEW( main_window->task_view ) );
        if ( gtk_tree_model_get_iter_first( model, &it ) )
        {
            do
            {
                gtk_tree_model_get( model, &it, TASK_COL_DATA, &ptask, -1 );
                str = g_strdup_printf( "%p", ptask );
                if ( !strcmp( str, argv[i] ) )
                {
                    g_free( str );
                    break;
                }
                g_free( str );
                ptask=  NULL;
            }
            while ( gtk_tree_model_iter_next( model, &it ) );
        }
        if ( !ptask )
        {
            *reply = g_strdup_printf( _("spacefm: invalid task '%s'\n"), argv[i] );
            return 2;
        }

        // get model value
        if ( !strcmp( argv[i+1], "icon" ) )
        {
            g_mutex_lock( ptask->task->mutex );
            if ( ptask->task->exec_icon )
                *reply = g_strdup_printf( "%s\n", ptask->task->exec_icon );
            g_mutex_unlock( ptask->task->mutex );
            return 0;
        }
        else if ( !strcmp( argv[i+1], "count" ) )
            j = TASK_COL_COUNT;
        else if ( !strcmp( argv[i+1], "folder" ) || !strcmp( argv[i+1], "from" ) )
            j = TASK_COL_PATH;
        else if ( !strcmp( argv[i+1], "item" ) )
            j = TASK_COL_FILE;
        else if ( !strcmp( argv[i+1], "to" ) )
            j = TASK_COL_TO;
        else if ( !strcmp( argv[i+1], "progress" ) )
        {
            *reply = g_strdup_printf( "%d\n", ptask->task->percent );
            return 0;
        }
        else if ( !strcmp( argv[i+1], "total" ) )
            j = TASK_COL_TOTAL;
        else if ( !strcmp( argv[i+1], "curspeed" ) )
            j = TASK_COL_CURSPEED;
        else if ( !strcmp( argv[i+1], "curremain" ) )
            j = TASK_COL_CUREST;
        else if ( !strcmp( argv[i+1], "avgspeed" ) )
            j = TASK_COL_AVGSPEED;
        else if ( !strcmp( argv[i+1], "avgremain" ) )
            j = TASK_COL_AVGEST;
        else if ( !strcmp( argv[i+1], "elapsed" ) )
            j = TASK_COL_ELAPSED;
        else if ( !strcmp( argv[i+1], "started" ) )
            j = TASK_COL_STARTED;
        else if ( !strcmp( argv[i+1], "status" ) )
            j = TASK_COL_STATUS;
        else if ( !strcmp( argv[i+1], "queue_state" ) )
        {
            if ( ptask->task->state_pause == VFS_FILE_TASK_RUNNING )
                str = "run";
            else if ( ptask->task->state_pause == VFS_FILE_TASK_PAUSE )
                str = "pause";
            else if ( ptask->task->state_pause == VFS_FILE_TASK_QUEUE )
                str = "queue";
            else
                str = "stop";    // failsafe
            *reply = g_strdup_printf( "%s\n", str );
            return 0;
        }
        else if ( !strcmp( argv[i+1], "popup_handler" ) )
        {
            if ( ptask->pop_handler )
                *reply = g_strdup_printf( "%s\n", ptask->pop_handler );
            return 0;
        }
        else
        {
            *reply = g_strdup_printf( _("spacefm: invalid task property '%s'\n"),
                                                                    argv[i+1] );
            return 2;
        }
        gtk_tree_model_get( model, &it, j, &str, -1 );
        if ( str )
            *reply = g_strdup_printf( "%s\n", str );
        g_free( str );
    }
    else if ( !strcmp( argv[0], "run-task" ) )
    {   // TYPE [OPTIONS] ...
        if ( !( argv[i] && argv[i+1] ) )
        {
            *reply = g_strdup_printf( _("spacefm: %s requires two arguments\n"),
                                                                        argv[0] );
            return 1;        
        }
        if ( !strcmp( argv[i], "cmd" ) || !strcmp( argv[i], "command" ) )
        {
            // custom command task
            // cmd [--task [--popup] [--scroll]] [--terminal] 
            //                     [--user USER] [--title TITLE] 
            //                     [--icon ICON] [--dir DIR] COMMAND
            // get opts
            gboolean opt_task = FALSE;
            gboolean opt_popup = FALSE;
            gboolean opt_scroll = FALSE;
            gboolean opt_terminal = FALSE;
            const char* opt_user = NULL;
            const char* opt_title = NULL;
            const char* opt_icon = NULL;
            const char* opt_cwd = NULL;
            for ( j = i + 1; argv[j] && argv[j][0] == '-'; j++ )
            {
                if ( !strcmp( argv[j], "--task" ) )
                    opt_task = TRUE;
                else if ( !strcmp( argv[j], "--popup" ) )
                    opt_popup = opt_task = TRUE;
                else if ( !strcmp( argv[j], "--scroll" ) )
                    opt_scroll = opt_task = TRUE;
                else if ( !strcmp( argv[j], "--terminal" ) )
                    opt_terminal = TRUE;
                /* disabled due to potential misuse of password caching su programs
                else if ( !strcmp( argv[j], "--user" ) )
                    opt_user = argv[++j];
                */
                else if ( !strcmp( argv[j], "--title" ) )
                    opt_title = argv[++j];
                else if ( !strcmp( argv[j], "--icon" ) )
                    opt_icon = argv[++j];
                else if ( !strcmp( argv[j], "--dir" ) )
                {
                    opt_cwd = argv[++j];
                    if ( !( opt_cwd && opt_cwd[0] == '/' &&
                                g_file_test( opt_cwd, G_FILE_TEST_IS_DIR ) ) )
                    {
                        *reply = g_strdup_printf( _("spacefm: no such directory '%s'\n"),
                                                            opt_cwd );
                        return 2;        
                    }
                }
                else
                {
                    *reply = g_strdup_printf( _("spacefm: invalid %s task option '%s'\n"),
                                                        argv[i], argv[j] );
                    return 2;        
                }
            }
            if ( !argv[j] )
            {
                *reply = g_strdup_printf( _("spacefm: %s requires two arguments\n"),
                                                                            argv[0] );
                return 1;        
            }
            GString* gcmd = g_string_new( argv[j] );
            while ( argv[++j] )
                g_string_append_printf( gcmd, " %s", argv[j] );
            
            PtkFileTask* ptask = ptk_file_exec_new( opt_title ? opt_title : gcmd->str,
                                       opt_cwd ? opt_cwd : 
                                         ptk_file_browser_get_cwd( file_browser ),
                                       GTK_WIDGET( file_browser ),
                                       file_browser->task_view );
            ptask->task->exec_browser = file_browser;
            ptask->task->exec_command = g_string_free( gcmd, FALSE );
            ptask->task->exec_as_user = g_strdup( opt_user );
            ptask->task->exec_icon = g_strdup( opt_icon );
            ptask->task->exec_terminal = opt_terminal;
            ptask->task->exec_keep_terminal = FALSE;
            ptask->task->exec_sync = opt_task;
            ptask->task->exec_popup = opt_popup;
            ptask->task->exec_show_output = opt_popup;
            ptask->task->exec_show_error = TRUE;
            ptask->task->exec_scroll_lock = !opt_scroll;
            ptask->task->exec_export = TRUE;
            if ( opt_popup )
                gtk_window_present( GTK_WINDOW( main_window ) );
            ptk_file_task_run( ptask );
            if ( opt_task )
                *reply = g_strdup_printf( "#!%s\n# Note: $new_task_id not valid until approx one half second after task start\nnew_task_window=%p\nnew_task_id=%p\n",
                                            BASHPATH, main_window, ptask );
        }
        else if ( !strcmp( argv[i], "edit" ) || !strcmp( argv[i], "web" ) )
        {
            // edit or web
            // edit [--as-root] FILE
            // web URL
            gboolean opt_root = FALSE;
            for ( j = i + 1; argv[j] && argv[j][0] == '-'; j++ )
            {
                if ( !strcmp( argv[i], "edit" ) && !strcmp( argv[j], "--as-root" ) )
                    opt_root = TRUE;
                else
                {
                    *reply = g_strdup_printf( _("spacefm: invalid %s task option '%s'\n"),
                                                        argv[i], argv[j] );
                    return 2;
                }
            }
            if ( !argv[j] )
            {
                *reply = g_strdup_printf( _("spacefm: %s requires two arguments\n"),
                                                                            argv[0] );
                return 1;
            }
            if ( !strcmp( argv[i], "edit" ) )
            {
                if ( !( argv[j][0] == '/' && 
                                    g_file_test( argv[j], G_FILE_TEST_EXISTS ) ) )
                {
                    *reply = g_strdup_printf( _("spacefm: no such file '%s'\n"),
                                                                    argv[j] );
                    return 2;
                }
                xset_edit( GTK_WIDGET( file_browser ), argv[j], opt_root, !opt_root );
            }
            else
                xset_open_url( GTK_WIDGET( file_browser ), argv[j] );
        }
        else if ( !strcmp( argv[i], "mount" ) || !strcmp( argv[i], "unmount" ) )
        {
            // mount or unmount TARGET
#ifdef HAVE_HAL
            *reply = g_strdup_printf( _("spacefm: task type %s requires udev build\n"),
                                                argv[i] );
            return 2;
#else
            for ( j = i + 1; argv[j] && argv[j][0] == '-'; j++ )
            {
                *reply = g_strdup_printf( _("spacefm: invalid %s task option '%s'\n"),
                                                    argv[i], argv[j] );
                return 2;
            }
            if ( !argv[j] )
            {
                *reply = g_strdup_printf( _("spacefm: task type %s requires TARGET argument\n"),
                                                                    argv[0] );
                return 1;
            }

            // Resolve TARGET
            struct stat64 statbuf;
            char* real_path = argv[j];
            char* device_file = NULL;
            VFSVolume* vol = NULL;
            netmount_t* netmount = NULL;
            if ( !strcmp( argv[i], "unmount" ) &&
                                g_file_test( real_path, G_FILE_TEST_IS_DIR ) )
            {
                // unmount DIR
                if ( path_is_mounted_mtab( NULL, real_path, &device_file, NULL )
                                                            && device_file )
                {
                    if ( !( stat64( device_file, &statbuf ) == 0 &&
                                                S_ISBLK( statbuf.st_mode ) ) )
                    {
                        // NON-block device - try to find vol by mount point
                        if ( !( vol = vfs_volume_get_by_device_or_point(
                                                device_file, real_path ) ) )
                        {
                            *reply = g_strdup_printf( _("spacefm: invalid TARGET '%s'\n"),
                                                                argv[j] );
                            return 2;
                        }
                        g_free( device_file );
                        device_file = NULL;
                    }
                }
            }
            else if ( stat64( real_path, &statbuf ) == 0 &&
                                                S_ISBLK( statbuf.st_mode ) )
            {
                // block device eg /dev/sda1
                device_file = g_strdup( real_path );
            }
            else if ( !strcmp( argv[i], "mount" ) &&
                        ( ( real_path[0] != '/' && strstr( real_path, ":/" ) )
                          || g_str_has_prefix( real_path, "//" ) ) )
            {
                // mount URL
                if ( split_network_url( real_path, &netmount ) != 1 )
                {
                    // not a valid url
                    *reply = g_strdup_printf( _("spacefm: invalid TARGET '%s'\n"),
                                                                    argv[j] );
                    return 2;
                }
            }
            
            if ( device_file )
            {
                // block device - get vol
                vol = vfs_volume_get_by_device_or_point( device_file, NULL );
                g_free( device_file );
                device_file = NULL;                
            }
            
            // Create command
            gboolean run_in_terminal = FALSE;
            char* cmd = NULL;
            if ( vol )
            {
                // mount/unmount vol
                if ( !strcmp( argv[i], "mount" ) )
                    cmd = vfs_volume_get_mount_command( vol,
                        xset_get_s( "dev_mount_options" ), &run_in_terminal );
                else
                    cmd = vfs_volume_device_unmount_cmd( vol, &run_in_terminal );
            }
            else if ( netmount )
            {
                // URL mount only
                cmd = vfs_volume_handler_cmd( HANDLER_MODE_NET, HANDLER_MOUNT,
                                            NULL, NULL, netmount,
                                            &run_in_terminal, NULL );
                g_free( netmount->url );
                g_free( netmount->fstype );
                g_free( netmount->host );
                g_free( netmount->ip );
                g_free( netmount->port );
                g_free( netmount->user );
                g_free( netmount->pass );
                g_free( netmount->path );
                g_slice_free( netmount_t, netmount );
            }
            if ( !cmd )
            {
                *reply = g_strdup_printf( _("spacefm: invalid TARGET '%s'\n"),
                                                                argv[j] );
                return 2;
            }
            // Task
            PtkFileTask* ptask = ptk_file_exec_new( argv[i],
                                    ptk_file_browser_get_cwd( file_browser ),
                                    GTK_WIDGET( file_browser ),
                                    file_browser->task_view );
            ptask->task->exec_browser = file_browser;
            ptask->task->exec_command = cmd;
            ptask->task->exec_terminal = run_in_terminal;
            ptask->task->exec_keep_terminal = FALSE;
            ptask->task->exec_sync = TRUE;
            ptask->task->exec_export = FALSE;
            ptask->task->exec_show_error = TRUE;
            ptask->task->exec_scroll_lock = FALSE;
            ptk_file_task_run( ptask );
#endif
        }
        else if ( !strcmp( argv[i], "copy" ) || !strcmp( argv[i], "move" )
                                             || !strcmp( argv[i], "link" )
                                             || !strcmp( argv[i], "delete" ) )
        {
            // built-in task
            // copy SOURCE FILENAME [...] TARGET
            // move SOURCE FILENAME [...] TARGET
            // link SOURCE FILENAME [...] TARGET
            // delete SOURCE FILENAME [...]
            // get opts
            const char* opt_cwd = NULL;
            for ( j = i + 1; argv[j] && argv[j][0] == '-'; j++ )
            {
                if ( !strcmp( argv[j], "--dir" ) )
                {
                    opt_cwd = argv[++j];
                    if ( !( opt_cwd && opt_cwd[0] == '/' &&
                                g_file_test( opt_cwd, G_FILE_TEST_IS_DIR ) ) )
                    {
                        *reply = g_strdup_printf( _("spacefm: no such directory '%s'\n"),
                                                            opt_cwd );
                        return 2;        
                    }
                }
                else
                {
                    *reply = g_strdup_printf( _("spacefm: invalid %s task option '%s'\n"),
                                                        argv[i], argv[j] );
                    return 2;        
                }
            }
            l = NULL;  // file list
            char* target_dir = NULL;
            for ( ; argv[j]; j++ )
            {
                if ( strcmp( argv[i], "delete" ) && !argv[j+1] )
                {
                    // last argument - use as TARGET
                    if ( argv[j][0] != '/' )
                    {
                        *reply = g_strdup_printf( _("spacefm: no such directory '%s'\n"),
                                                                    argv[j] );
                        g_list_foreach( l, (GFunc)g_free, NULL );
                        g_list_free( l );
                        return 2;
                    }
                    target_dir = argv[j];
                    break;
                }
                else
                {
                    if ( argv[j][0] == '/' )
                        // absolute path
                        str = g_strdup( argv[j] );
                    else
                    {
                        // relative path
                        if ( !opt_cwd )
                        {
                            *reply = g_strdup_printf( _("spacefm: relative path '%s' requires %s option --dir DIR\n"),
                                                               argv[j], argv[i] );
                            g_list_foreach( l, (GFunc)g_free, NULL );
                            g_list_free( l );
                            return 2;
                        }
                        str = g_build_filename( opt_cwd, argv[j], NULL );
                    }
                    /*   Let vfs task show error instead
                    if ( !g_file_test( str, G_FILE_TEST_EXISTS ) )
                    {
                        *reply = g_strdup_printf( _("spacefm: no such file '%s'\n"),
                                                            str );
                        g_free( str );
                        g_list_foreach( l, (GFunc)g_free, NULL );
                        g_list_free( l );
                        return 2;
                    }
                    */
                    l = g_list_prepend( l, str );
                }
            }
            if ( !l || ( strcmp( argv[i], "delete" ) && !target_dir ) )
            {
                *reply = g_strdup_printf( _("spacefm: task type %s requires FILE argument(s)\n"),
                                                                    argv[i] );
                return 2;
            }
            l = g_list_reverse( l );
            if ( !strcmp( argv[i], "copy" ) )
                j = VFS_FILE_TASK_COPY;
            else if ( !strcmp( argv[i], "move" ) )
                j = VFS_FILE_TASK_MOVE;
            else if ( !strcmp( argv[i], "link" ) )
                j = VFS_FILE_TASK_LINK;
            else if ( !strcmp( argv[i], "delete" ) )
                j = VFS_FILE_TASK_DELETE;
            else
                return 1; // failsafe
            PtkFileTask* ptask = ptk_file_task_new( j, l, target_dir,
                                        GTK_WINDOW( gtk_widget_get_toplevel( 
                                            GTK_WIDGET( file_browser ) ) ),
                                        file_browser->task_view );
            ptk_file_task_run( ptask );
            *reply = g_strdup_printf( "#!%s\n# Note: $new_task_id not valid until approx one half second after task start\nnew_task_window=%p\nnew_task_id=%p\n",
                                        BASHPATH, main_window, ptask );
        }
        else
        {
            *reply = g_strdup_printf( _("spacefm: invalid task type '%s'\n"),
                                                                    argv[i] );
            return 2;
        }
    }
    else if ( !strcmp( argv[0], "emit-key" ) )
    {   // KEYCODE [KEYMOD]
        if ( !argv[i] )
        {
            *reply = g_strdup_printf( _("spacefm: command %s requires an argument\n"),
                                                                    argv[0] );
            return 1;
        }        
        // this only handles keys assigned to menu items
        GdkEventKey* event = (GdkEventKey*)gdk_event_new(GDK_KEY_PRESS);
        event->keyval = (guint)strtol( argv[i], NULL, 0 );
        event->state = argv[i+1] ? (guint)strtol( argv[i+1], NULL, 0 ) : 0;
        if ( event->keyval )
        {
            gtk_window_present( GTK_WINDOW( main_window ) );        
            on_main_window_keypress( main_window, event, NULL );
        }
        else
        {
            *reply = g_strdup_printf( _("spacefm: invalid keycode '%s'\n"),
                                                                    argv[i] );
            gdk_event_free( (GdkEvent*)event );     
            return 2;
        }
        gdk_event_free( (GdkEvent*)event );     
    }
    else if ( !strcmp( argv[0], "activate" ) ||
              !strcmp( argv[0], "show-menu" ) /* backwards compat <1.0.4 */ )
    {
        if ( !argv[i] )
        {
            *reply = g_strdup_printf( _("spacefm: command %s requires an argument\n"),
                                                                    argv[0] );
            return 1;
        }
        XSet* set = xset_find_custom( argv[i] );
        if ( !set )
        {
            *reply = g_strdup_printf( _("spacefm: custom command or submenu '%s' not found\n"),
                                                                    argv[i] );
            return 2;
        }
        XSetContext* context = xset_context_new();
        main_context_fill( file_browser, context );
        if ( context && context->valid )
        {
            if ( !xset_get_b( "context_dlg" ) && 
                        xset_context_test( context, set->context, FALSE ) != 
                                                                CONTEXT_SHOW )
            {
                *reply = g_strdup_printf( _("spacefm: item '%s' context hidden or disabled\n"),
                                                                        argv[i] );
                return 2;
            }
        }
        if ( set->menu_style == XSET_MENU_SUBMENU )
        {
            // show submenu as popup menu
            set = xset_get( set->child );
            widget = gtk_menu_new();
            GtkAccelGroup* accel_group = gtk_accel_group_new();
     
            xset_add_menuitem( NULL, file_browser, GTK_WIDGET( widget ), accel_group,
                                                                    set );
            g_idle_add( (GSourceFunc)delayed_show_menu, widget );
        }
        else
        {
            // activate item
            on_main_window_keypress( NULL, NULL, set );
        }
    }
    else if ( !strcmp( argv[0], "add-event" ) ||
              !strcmp( argv[0], "replace-event" ) ||
              !strcmp( argv[0], "remove-event" ) )
    {
        XSet* set;
        
        if ( !( argv[i] && argv[i+1] ) )
        {
            *reply = g_strdup_printf( _("spacefm: %s requires two arguments\n"),
                                                                        argv[0] );
            return 1;        
        }
        if ( !( set = xset_is( argv[i] ) ) )
        {
            *reply = g_strdup_printf( _("spacefm: invalid event type '%s'\n"),
                                                                        argv[i] );
            return 2;
        }
        // build command
        GString* gstr = g_string_new( !strcmp( argv[0], "replace-event" ) ? "*" : "" );
        for ( j = i + 1; argv[j]; j++ )
            g_string_append_printf( gstr, "%s%s", j == i + 1 ? "" : " ", argv[j] );
        str = g_string_free( gstr, FALSE );
        // modify list
        if ( !strcmp( argv[0], "remove-event" ) )
        {
            l = g_list_find_custom( (GList*)set->ob2_data, str,
                                                    (GCompareFunc)g_strcmp0 );
            if ( !l )
            {
                // remove replace event
                char* str2 = str;
                str = g_strdup_printf( "*%s", str2 );
                g_free( str2 );
                l = g_list_find_custom( (GList*)set->ob2_data, str,
                                                    (GCompareFunc)g_strcmp0 );
            }
            g_free( str );
            if ( !l )
            {
                *reply = g_strdup_printf( _("spacefm: event handler not found\n") );
                return 2;
            }
            l = g_list_remove( (GList*)set->ob2_data, l->data );
        }
        else
            l = g_list_append( (GList*)set->ob2_data, str );
        set->ob2_data = (gpointer)l;
    }
    else
    {
        *reply = g_strdup_printf( _("spacefm: invalid socket method '%s'\n"),
                                                                    argv[0] );
        return 1;        
    }
    return 0;
}

gboolean run_event( FMMainWindow* main_window, PtkFileBrowser* file_browser, 
                            XSet* preset, const char* event,
                            int panel, int tab, const char* focus, 
                            int keyval, int button, int state,
                            gboolean visible, XSet* set, char* ucmd )
{
    char* cmd;
    gboolean inhibit;
    char* argv[4];
    gint exit_status;

    if ( !ucmd )
        return FALSE;

    if ( ucmd[0] == '*' )
    {
        ucmd++;
        inhibit = TRUE;
    }
    else
        inhibit = FALSE;

    if ( !preset && ( !strcmp( event, "evt_start" ) || 
                      !strcmp( event, "evt_exit" ) ||
                      !strcmp( event, "evt_device" ) ) )
    {
        cmd = replace_string( ucmd, "%e", event, FALSE );
        if ( !strcmp( event, "evt_device" ) )
        {
            if ( !focus )
                return FALSE;
            char* str = cmd;
            cmd = replace_string( str, "%f", focus, FALSE );
            g_free( str );
            char* change;
            if ( state == VFS_VOLUME_ADDED )
                change = "added";
            else if ( state == VFS_VOLUME_REMOVED )
                change = "removed";
            else
                change = "changed";
            str = cmd;
            cmd = replace_string( str, "%v", change, FALSE );
            g_free( str );            
        }
        argv[0] = g_strdup( "bash" );
        argv[1] = g_strdup( "-c" );
        argv[2] = cmd;
        argv[3] = NULL;
        printf( "EVENT %s >>> %s\n", event, argv[2] );
        //g_spawn_command_line_async( cmd, NULL );
        //system( cmd );
        g_spawn_async( NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                                                              NULL, NULL );
        return FALSE;
    }

    if ( !main_window )
        return FALSE;

    // replace vars
    char* replace = "ewpt";
    if ( set == evt_win_click )
    {
        replace = "ewptfbm";
        state = ( state & ( GDK_SHIFT_MASK | GDK_CONTROL_MASK |
                  GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK ) );
    }
    else if ( set == evt_win_key )
        replace = "ewptkm";
    else if ( set == evt_pnl_show )
        replace = "ewptfv";
    else if ( set == evt_tab_chdir )
        replace = "ewptd";

    char* str;
    char* rep;
    char var[3];
    var[0] = '%';
    var[2] = '\0';
    int i = 0;
    cmd = NULL;
    while ( replace[i] )
    {
        /*
        %w  windowid
        %p  panel
        %t  tab
        %f  focus
        %e  event
        %k  keycode
        %m  modifier
        %b  button
        %v  visible
        %d  cwd
        */
        var[1] = replace[i];
        str = cmd;
        if ( var[1] == 'e' )
            cmd = replace_string( ucmd, var, event, FALSE );
        else if ( strstr( str, var ) )
        {
            if ( var[1] == 'f' )
            {
                if ( !focus )
                {
                    rep = g_strdup_printf( "panel%d", panel );
                    cmd = replace_string( str, var, rep, FALSE );
                    g_free( rep );
                }
                else
                    cmd = replace_string( str, var, focus, FALSE );
            }
            else if ( var[1] == 'w' )
            {
                rep = g_strdup_printf( "%p", main_window );
                cmd = replace_string( str, var, rep, FALSE );
                g_free( rep );
            }
            else if ( var[1] == 'p' )
            {
                rep = g_strdup_printf( "%d", panel );
                cmd = replace_string( str, var, rep, FALSE );
                g_free( rep );
            }
            else if ( var[1] == 't' )
            {
                rep = g_strdup_printf( "%d", tab );
                cmd = replace_string( str, var, rep, FALSE );
                g_free( rep );
            }
            else if ( var[1] == 'v' )
                cmd = replace_string( str, var, visible ? "1" : "0", FALSE );
            else if ( var[1] == 'k' )
            {
                rep = g_strdup_printf( "%#x", keyval );
                cmd = replace_string( str, var, rep, FALSE );
                g_free( rep );
            }
            else if ( var[1] == 'b' )
            {
                rep = g_strdup_printf( "%d", button );
                cmd = replace_string( str, var, rep, FALSE );
                g_free( rep );
            }
            else if ( var[1] == 'm' )
            {
                rep = g_strdup_printf( "%#x", state );
                cmd = replace_string( str, var, rep, FALSE );
                g_free( rep );
            }
            else if ( var[1] == 'd' )
            {
                rep = bash_quote( file_browser ?
                                    ptk_file_browser_get_cwd(
                                                    file_browser ) : NULL );
                cmd = replace_string( str, var, rep, FALSE );
                g_free( rep );
            }
            else
            {
                // failsafe
                g_free( str );
                g_free( cmd );
                return FALSE;
            }
            g_free( str );
        }
        i++;
    }

    if ( !inhibit )
    {
        printf( "\nEVENT %s >>> %s\n", event, cmd );
        if ( !strcmp( event, "evt_tab_close" ) )
        {
            // file_browser becomes invalid so spawn
            argv[0] = g_strdup( "bash" );
            argv[1] = g_strdup( "-c" );
            argv[2] = cmd;
            argv[3] = NULL;
            g_spawn_async( NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                                                    NULL, NULL );
        }
        else
        {
            // task
            PtkFileTask* task = ptk_file_exec_new( event, 
                                                   ptk_file_browser_get_cwd( file_browser ),
                                                   GTK_WIDGET( file_browser ),
                                                   main_window->task_view );
            task->task->exec_browser = file_browser;
            task->task->exec_command = cmd;
            task->task->exec_icon = g_strdup( set->icon );
            task->task->exec_sync = FALSE;
            task->task->exec_export = TRUE;
            ptk_file_task_run( task );
        }
        return FALSE;
    }

    argv[0] = g_strdup( "bash" );
    argv[1] = g_strdup( "-c" );
    argv[2] = cmd;
    argv[3] = NULL;
    printf( "REPLACE_EVENT %s >>> %s\n", event, argv[2] );
    inhibit = FALSE;
    if ( g_spawn_sync( NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                       NULL, NULL, &exit_status, NULL ) )
    {
        if ( WIFEXITED( exit_status ) && WEXITSTATUS( exit_status ) == 0 )
            inhibit = TRUE;
    }
    printf( "REPLACE_EVENT ? %s\n", inhibit ? "TRUE" : "FALSE" );
    return inhibit;
}

gboolean main_window_event( gpointer mw, XSet* preset, const char* event,
                            int panel, int tab, const char* focus, 
                            int keyval, int button, int state,
                            gboolean visible )
{
    XSet* set;
    gboolean inhibit = FALSE;
    
//printf("main_window_event %s\n", event );
    // get set
    if ( preset )
        set = preset;
    else
    {
        set = xset_get( event );
        if ( !set->s && !set->ob2_data )
            return FALSE;
    }

    // get main_window, panel, and tab
    FMMainWindow* main_window;
    PtkFileBrowser* file_browser;
    if ( !mw )
        main_window = fm_main_window_get_last_active();
    else
        main_window = (FMMainWindow*)mw;
    if ( main_window )
    {
        file_browser = PTK_FILE_BROWSER( 
                        fm_main_window_get_current_file_browser( main_window ) );
        if ( !file_browser )
            return FALSE;
        if ( !panel )
            panel = main_window->curpanel;
        if ( !tab )
        {
            tab = gtk_notebook_page_num( 
                        GTK_NOTEBOOK( main_window->panel[file_browser->mypanel - 1] ),
                        GTK_WIDGET( file_browser ) ) + 1;
        }
    }
    else
        file_browser = NULL;
    
    // dynamic handlers
    if ( set->ob2_data )
    {
        GList* l;
        for ( l = (GList*)set->ob2_data; l; l = l->next )
        {
            if ( run_event( main_window, file_browser, preset, event, panel,
                                    tab, focus, keyval, button, state,
                                    visible, set, (char*)l->data ) )
                inhibit = TRUE;
        }
    }

    // Events menu handler
    return ( run_event( main_window, file_browser, preset, event, panel,
                                    tab, focus, keyval, button, state,
                                    visible, set, set->s ) || inhibit );
}

