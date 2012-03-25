/*
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "pcmanfm.h"
#include "private.h"

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>

#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include "ptk-location-view.h"
#include "exo-tree-view.h"

#include "ptk-file-browser.h"

#include "main-window.h"
#include "ptk-utils.h"

#include "pref-dialog.h"
#include "ptk-bookmarks.h"
#include "edit-bookmarks.h"
#include "ptk-file-properties.h"
#include "ptk-path-entry.h"

#include "settings.h"
#include "find-files.h"

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
                                             GtkNotebookPage *page,
                                             guint page_num,
                                             gpointer user_data );
static void on_close_tab_activate ( GtkMenuItem *menuitem,
                                    gpointer user_data );

static void on_file_browser_before_chdir( PtkFileBrowser* file_browser,
                                          const char* path, gboolean* cancel,
                                          FMMainWindow* main_window );
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

static gboolean on_main_window_keypress( FMMainWindow* widget,
                                         GdkEventKey* event, gpointer data);
//static gboolean on_main_window_keyrelease( FMMainWindow* widget,
//                                        GdkEventKey* event, gpointer data);
static void on_new_window_activate ( GtkMenuItem *menuitem,
                                     gpointer user_data );
GtkWidget* create_bookmarks_menu ( FMMainWindow* main_window );
static void fm_main_window_close( FMMainWindow* main_window );

GtkTreeView* main_task_view_new( FMMainWindow* main_window );
void main_task_add_menu( FMMainWindow* main_window, GtkMenu* menu,
                                                GtkAccelGroup* accel_group );
void on_task_popup_show( GtkMenuItem* item, FMMainWindow* main_window, char* name2 );
gboolean main_tasks_running( FMMainWindow* main_window );
void on_task_stop( GtkMenuItem* item, GtkTreeView* view, PtkFileTask* task2 );
void on_task_stop_all( GtkMenuItem* item, GtkTreeView* view );
void on_preference_activate ( GtkMenuItem *menuitem, gpointer user_data );
void main_task_prepare_menu( FMMainWindow* main_window, GtkMenu* menu,
                                                GtkAccelGroup* accel_group );
void on_task_columns_changed( GtkTreeView *view, gpointer user_data );
PtkFileTask* get_selected_task( GtkTreeView* view );
static void fm_main_window_update_status_bar( FMMainWindow* main_window,
                                              PtkFileBrowser* file_browser );
void set_window_title( FMMainWindow* main_window, PtkFileBrowser* file_browser );
void on_task_column_selected( GtkMenuItem* item, GtkTreeView* view );
void on_task_popup_errset( GtkMenuItem* item, FMMainWindow* main_window, char* name2 );
void on_about_activate ( GtkMenuItem *menuitem, gpointer user_data );
void update_window_title( GtkMenuItem* item, FMMainWindow* main_window );
void on_toggle_panelbar( GtkWidget* widget, FMMainWindow* main_window );
void on_fullscreen_activate ( GtkMenuItem *menuitem, FMMainWindow* main_window );
static gboolean delayed_focus( GtkWidget* widget );


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
}

void on_bookmark_item_activate ( GtkMenuItem* menu, gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    char* path = ( char* ) g_object_get_data( G_OBJECT( menu ), "path" );
    if ( !path )
        return ;

    if ( !xset_get_b( "book_newtab" ) )
        ptk_file_browser_chdir( PTK_FILE_BROWSER( file_browser ), path, PTK_FB_CHDIR_ADD_HISTORY );
    else
        fm_main_window_add_new_tab( main_window, path );
}

void on_bookmarks_change( PtkBookmarks* bookmarks, FMMainWindow* main_window )
{
    main_window->book_menu = create_bookmarks_menu( main_window );
    /* FIXME: We have to popupdown the menu first, if it's showed on screen.
      * Otherwise, it's rare but possible that we try to replace the menu while it's in use.
      *  In that way, gtk+ will hang.  So some dirty hack is used here.
      *  We popup down the old menu, if it's currently shown.
      */
    gtk_menu_popdown( (GtkMenu*)gtk_menu_item_get_submenu ( GTK_MENU_ITEM (
                                            main_window->book_menu_item ) ) );
    gtk_menu_item_set_submenu ( GTK_MENU_ITEM ( main_window->book_menu_item ),
                                                    main_window->book_menu );
}

void main_window_update_bookmarks()
{
    GList* l;
    for ( l = all_windows; l; l = l->next )
    {
        on_bookmarks_change( NULL, (FMMainWindow*)l->data );
    }
}

GtkWidget* create_bookmarks_menu_item ( FMMainWindow* main_window,
                                        const char* item )
{
    GtkWidget * folder_image = NULL;
    GtkWidget* menu_item;
    const char* path;

    menu_item = gtk_image_menu_item_new_with_label( item );
    path = ptk_bookmarks_item_get_path( item );
    g_object_set_data( G_OBJECT( menu_item ), "path", (gpointer) path );
    
    //folder_image = gtk_image_new_from_icon_name( "gnome-fs-directory",
    //                                             GTK_ICON_SIZE_MENU );
    XSet* set = xset_get( "book_icon" );
    if ( set->icon )
        folder_image = xset_get_image( set->icon, GTK_ICON_SIZE_MENU );
    if ( !folder_image )
        folder_image = xset_get_image( "gtk-directory", GTK_ICON_SIZE_MENU );
    if ( folder_image )
        gtk_image_menu_item_set_image ( GTK_IMAGE_MENU_ITEM ( menu_item ),
                                                        folder_image );
    g_signal_connect( menu_item, "activate",
                      G_CALLBACK( on_bookmark_item_activate ), main_window );
    return menu_item;
}

void add_bookmark( GtkWidget* item, FMMainWindow* main_window )
{
    PtkFileBrowser* file_browser = fm_main_window_get_current_file_browser(
                                                                main_window );
    ptk_file_browser_add_bookmark( NULL, file_browser );
}

void show_bookmarks( GtkWidget* item, FMMainWindow* main_window )
{
    PtkFileBrowser* file_browser = fm_main_window_get_current_file_browser(
                                                                main_window );
    if ( !file_browser->side_book )
    {
        xset_set_panel( file_browser->mypanel, "show_book", "b", "1" );
        update_views_all_windows( NULL, file_browser );
    }
    if ( file_browser->side_book )
        gtk_widget_grab_focus( file_browser->side_book );
}

GtkWidget* create_bookmarks_menu ( FMMainWindow* main_window )
{
    GtkWidget * bookmark_menu;
    GtkWidget* menu_item;
    GList* l;

    /*    this won't update with xset change so don't bother with it
    XSet* set_add = xset_get( "book_new" );
    XSet* set_show = xset_get( "panel1_show_book" );
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    
    PtkMenuItemEntry fm_bookmarks_menu[] = {
        PTK_IMG_MENU_ITEM( N_( "_Add" ), "gtk-add", add_bookmark, set_add->key, set_add->keymod ),
        PTK_IMG_MENU_ITEM( N_( "_Show" ), "gtk-edit", show_bookmarks, set_show->key, set_show->keymod ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_MENU_END
    };

    bookmark_menu = ptk_menu_new_from_data( fm_bookmarks_menu, main_window,
                                            accel_group );
    */

    static PtkMenuItemEntry fm_bookmarks_menu[] = {
        PTK_IMG_MENU_ITEM( N_( "_Add" ), "gtk-add", add_bookmark, 0, 0 ),
        PTK_IMG_MENU_ITEM( N_( "_Open" ), "gtk-open", show_bookmarks, 0, 0 ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_MENU_END
    };

    bookmark_menu = ptk_menu_new_from_data( fm_bookmarks_menu, main_window,
                                            main_window->accel_group );

    int count = 0;
    for ( l = app_settings.bookmarks->list; l; l = l->next )
    {
        menu_item = create_bookmarks_menu_item( main_window,
                                                ( char* ) l->data );
        gtk_menu_shell_append ( GTK_MENU_SHELL( bookmark_menu ), menu_item );
        count++;
        if ( count > 50 )
            break;
    }
    gtk_widget_show_all( bookmark_menu );
    return bookmark_menu;
}

void on_plugin_install( GtkMenuItem* item, FMMainWindow* main_window, XSet* set2 )
{
    XSet* set;
    char* path = NULL;
    char* deffolder;
    char* plug_dir;
    char* msg;
    int type = 0;
    int job = 0;
    
    if ( !item )
        set = set2;
    else
        set = (XSet*)g_object_get_data( G_OBJECT(item), "set" );
    if ( !set )
        return;
    
    if ( g_str_has_suffix( set->name, "cfile" ) || g_str_has_suffix( set->name, "curl" ) )
        job = 1;  // copy

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
        path = xset_file_dialog( main_window, GTK_FILE_CHOOSER_ACTION_OPEN,
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
        if ( !xset_text_dialog( main_window, _("Enter Plugin URL"), NULL, FALSE, _("Enter SpaceFM Plugin URL:\n\n(wget will be used to download the plugin file)"), NULL, NULL, &path, NULL, FALSE ) || !path || path[0] == '\0' )
            return;
        type = 1;  //url
    }

    if ( job == 0 )
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
            xset_msg_dialog( main_window, GTK_MESSAGE_ERROR,
                                        _("Invalid Plugin Filename"), NULL,
                                        0, msg, NULL );
            {
                g_free( plug_dir_name );
                g_free( path );
                g_free( msg );
                return;
            }
        }
        
        if ( !g_file_test( "/usr/share/spacefm/plugins", G_FILE_TEST_IS_DIR ) &&
                g_file_test( "/usr/local/share/spacefm/plugins", G_FILE_TEST_IS_DIR ) )
            plug_dir = g_build_filename( "/usr/local/share/spacefm/plugins",
                                                    plug_dir_name, NULL );
        else
            plug_dir = g_build_filename( "/usr/share/spacefm/plugins",
                                                    plug_dir_name, NULL );

        if ( g_file_test( plug_dir, G_FILE_TEST_EXISTS ) )
        {
            msg = g_strdup_printf( _("There is already a plugin installed as '%s'.  Overwrite ?\n\nTip: You can also rename this plugin file to install it under a different name."), plug_dir_name );
            if ( xset_msg_dialog( main_window, GTK_MESSAGE_WARNING,
                                        _("Overwrite Plugin ?"), NULL,
                                        GTK_BUTTONS_YES_NO,
                                        msg, NULL ) != GTK_RESPONSE_YES )
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
        char* hex8;
        plug_dir = NULL;
        while ( !plug_dir || g_file_test( plug_dir, G_FILE_TEST_EXISTS ) )
        {
            hex8 = randhex8();
            if ( plug_dir )
                g_free( plug_dir );
            plug_dir = g_build_filename( xset_get_user_tmp_dir(), hex8, NULL );
            g_free( hex8 );
        }
    }

    install_plugin_file( main_window, path, plug_dir, type, job );
    g_free( path );
    g_free( plug_dir );
}

GtkMenu* create_plugins_menu( FMMainWindow* main_window )
{
    GtkMenu* plug_menu;
    GtkWidget* item;
    GList* l;
    GList* plugins = NULL;
    XSet* set;
    
    PtkFileBrowser* file_browser = fm_main_window_get_current_file_browser(
                                                                main_window );
    GtkAccelGroup* accel_group = gtk_accel_group_new ();
    plug_menu = gtk_menu_new();
    
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
    gtk_menu_shell_append( plug_menu, item );

    set = xset_get( "plug_inc" );
    item = xset_add_menuitem( NULL, file_browser, plug_menu, accel_group, set );
    if ( item )
    {
        GtkMenu* inc_menu = gtk_menu_item_get_submenu( GTK_MENU_ITEM ( item ) );

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
    char* name;
    char* plug_dir;
    char* plug_file;
    int i;
    gboolean found = FALSE;
    gboolean found_plugins = FALSE;
    const char* paths[] = {
        "/usr/local/share/spacefm/included",
        "/usr/local/share/spacefm/plugins",
        "/usr/share/spacefm/included",
        "/usr/share/spacefm/plugins"
    };

    for ( i = 0; i < G_N_ELEMENTS( paths ); i++ )
    {
        dir = g_dir_open( paths[i], 0, NULL );
        if ( dir )
        {
            while ( ( name = g_dir_read_name( dir ) ) )
            {
                plug_file = g_build_filename( paths[i], name, "plugin", NULL );
                if ( g_file_test( plug_file, G_FILE_TEST_EXISTS ) )
                {
                    plug_dir = g_build_filename( paths[i], name, NULL );
                    xset_import_plugin( plug_dir );
                    g_free( plug_dir );
                    found = TRUE;
                    if ( i == 1 )
                        found_plugins = TRUE;
                }
                g_free( plug_file );
            }
            g_dir_close( dir );
        }
    }
    clean_plugin_mirrors();

    //if ( found )
    //    main_window_on_plugins_change( main_window );
}

gboolean on_autosave_timer( FMMainWindow* main_window )
{
    //printf("AUTOSAVE on_timer\n" );
    char* err_msg = save_settings( main_window );
    if ( err_msg )
    {
        printf( _("SpaceFM Error: Unable to autosave session file ( %s )\n"), err_msg );
        g_free( err_msg );
    }
    g_source_remove( main_window->autosave_timer );
    main_window->autosave_timer = 0;
    return FALSE;
}

void main_window_autosave( PtkFileBrowser* file_browser )
{
    if ( !file_browser )
        return;
    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;
    if ( main_window->autosave_timer )
    {
        // autosave timer is already running, so ignore request
        //printf("AUTOSAVE already set\n" );
        return;
    }
    // autosave settings in 30 seconds 
    main_window->autosave_timer = g_timeout_add_seconds( 30,
                                ( GSourceFunc ) on_autosave_timer, main_window );
    //printf("AUTOSAVE timer started\n" );
}

void on_save_session( GtkWidget* widget, FMMainWindow* main_window )
{
    GList* l;
    FMMainWindow* a_window;
    
    // disable timer all windows
    for ( l = all_windows; l; l = l->next )
    {
        a_window = (FMMainWindow*)l->data;
        if ( a_window->autosave_timer )
        {
            g_source_remove( a_window->autosave_timer );
            a_window->autosave_timer = 0;
        }
    }

    char* err_msg = save_settings( main_window );
    if ( err_msg )
    {
        char* msg = g_strdup_printf( _("Error: Unable to save session file\n\n( %s )"), err_msg );
        g_free( err_msg );
        xset_msg_dialog( main_window, GTK_MESSAGE_ERROR, _("Save Session Error"),
                                                            NULL, 0, msg, NULL );
        g_free( msg );
    }    
}

void on_find_file_activate ( GtkMenuItem *menuitem, gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    const char* cwd;
    const char* dirs[2];
    PtkFileBrowser* file_browser = fm_main_window_get_current_file_browser(
                                                                main_window );
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
    // task
    PtkFileTask* task = ptk_file_exec_new( _("Open Root Window"),
                                    ptk_file_browser_get_cwd( file_browser ),
                                    file_browser, file_browser->task_view );
    char* prog = g_find_program_in_path( g_get_prgname() );
    if ( !prog )
        prog = g_strdup( g_get_prgname() );
    if ( !prog )
        prog = g_strdup( "spacefm" );
    task->task->exec_command = prog;
    task->task->exec_as_user = g_strdup( "root" );
    task->task->exec_sync = FALSE;
    task->task->exec_export = FALSE;
    task->task->exec_browser = NULL; //file_browser;
    ptk_file_task_run( task );
}

void main_window_open_terminal( FMMainWindow* main_window, gboolean as_root )
{
    PtkFileBrowser* file_browser = fm_main_window_get_current_file_browser(
                                                                main_window );
    GtkWidget* parent = gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) );
    char* main_term = xset_get_s( "main_terminal" );
    if ( !main_term || main_term[0] == '\0' )
    {
        ptk_show_error( parent,
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
                            file_browser, file_browser->task_view );\

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
    GtkNotebook* notebook;
    
    for ( l = all_windows; l; l = l->next )
    {
        main_window = (FMMainWindow*)l->data;
        for ( p = 1; p < 5; p++ )
        {
            notebook = main_window->panel[p-1];
            num_pages = gtk_notebook_get_n_pages( notebook );
            for ( i = 0; i < num_pages; i++ )
            {
                a_browser = (PtkFileBrowser*)
                                        gtk_notebook_get_nth_page( notebook, i );
                if ( a_browser->view_mode == PTK_FB_LIST_VIEW )
                {
    #if GTK_CHECK_VERSION(2, 10, 0)
                    gtk_tree_view_set_rubber_banding( (GtkTreeView*)a_browser->folder_view,
                                                        xset_get_b( "rubberband" ) );
    #endif
                }
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
            gtk_widget_modify_bg( a_window, GTK_STATE_NORMAL, &color );
            gtk_widget_modify_bg( a_window->menu_bar, GTK_STATE_NORMAL, &color );
            gtk_widget_modify_bg( a_window->panelbar, GTK_STATE_NORMAL, &color );
            // how to change menu bar text color?
            //gtk_widget_modify_fg( GTK_MENU_ITEM( a_window->file_menu_item ), GTK_STATE_NORMAL, &color_white );
        }
    }
    else
    {
        for ( l = all_windows; l; l = l->next )
        {
            a_window = (FMMainWindow*)l->data;
            gtk_widget_modify_bg( a_window, GTK_STATE_NORMAL, NULL );
            gtk_widget_modify_bg( a_window->menu_bar, GTK_STATE_NORMAL, NULL );
            gtk_widget_modify_bg( a_window->panelbar, GTK_STATE_NORMAL, NULL );
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
        num_pages = gtk_notebook_get_n_pages( main_window->panel[p-1] );
        for ( i = 0; i < num_pages; i++ )
        {
            a_browser = (PtkFileBrowser*)
                                    gtk_notebook_get_nth_page( main_window->panel[p-1],
                                    i );
            // file list / tree
            fontname = xset_get_s_panel( p, "font_file" );
            if ( fontname )
            {
                font_desc = pango_font_description_from_string( fontname );
                gtk_widget_modify_font( a_browser->folder_view, font_desc );
                if ( a_browser->side_dir )
                    gtk_widget_modify_font( a_browser->side_dir, font_desc );
                pango_font_description_free( font_desc );
            }
            else
            {
                gtk_widget_modify_font( a_browser->folder_view, NULL );        
                if ( a_browser->side_dir )
                    gtk_widget_modify_font( a_browser->side_dir, NULL );        
            }
            // devices
            if ( a_browser->side_dev )
            {
                fontname = xset_get_s_panel( p, "font_dev" );
                if ( fontname )
                {
                    font_desc = pango_font_description_from_string( fontname );
                    gtk_widget_modify_font( a_browser->side_dev, font_desc );
                    pango_font_description_free( font_desc );
                }
                else
                    gtk_widget_modify_font( a_browser->side_dev, NULL );        
            }
            // bookmarks
            if ( a_browser->side_book )
            {
                fontname = xset_get_s_panel( p, "font_book" );
                if ( fontname )
                {
                    font_desc = pango_font_description_from_string( fontname );
                    gtk_widget_modify_font( a_browser->side_book, font_desc );
                    pango_font_description_free( font_desc );
                }
                else
                    gtk_widget_modify_font( a_browser->side_book, NULL );
            }
            // smartbar
            if ( a_browser->path_bar )
            {
                fontname = xset_get_s_panel( p, "font_path" );
                if ( fontname )
                {
                    font_desc = pango_font_description_from_string( fontname );
                    gtk_widget_modify_font( a_browser->path_bar, font_desc );
                    pango_font_description_free( font_desc );
                }
                else
                    gtk_widget_modify_font( a_browser->path_bar, NULL );
            }
            
            // status bar font and icon
            if ( a_browser->status_label )
            {
                fontname = xset_get_s_panel( p, "font_status" );
                if ( fontname )
                {
                    font_desc = pango_font_description_from_string( fontname );
                    gtk_widget_modify_font( a_browser->status_label, font_desc );
                    pango_font_description_free( font_desc );
                }
                else
                    gtk_widget_modify_font( a_browser->status_label, NULL );

                gtk_image_set_from_icon_name( a_browser->status_image, icon_name,
                                                            GTK_ICON_SIZE_MENU );
            }
            
            // tabs
            // need to recreate to change icon
            tab_label = fm_main_window_create_tab_label( main_window,
                                                        a_browser );
            gtk_notebook_set_tab_label( main_window->panel[p-1],
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
            gtk_widget_modify_font( main_window->task_view, font_desc );
            pango_font_description_free( font_desc );
        }
        else
            gtk_widget_modify_font( main_window->task_view, NULL );
            
        // panelbar
        gtk_image_set_from_icon_name( main_window->panel_image[p-1], icon_name,
                                                    GTK_ICON_SIZE_MENU );
    }
}

static void update_window_icon( GtkWindow* window, GtkIconTheme* theme )
{
    GdkPixbuf* icon;
    char* name;

    XSet* set = xset_get( "main_icon" );
    if ( set->icon )
        name = set->icon;
    else if ( geteuid() == 0 )
        name = "spacefm-root";
    else
        name = "spacefm";
    
    icon = gtk_icon_theme_load_icon( theme, name, 48, 0, NULL );
    if ( icon )
    {
        gtk_window_set_icon( window, icon );
        g_object_unref( icon );
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
        update_window_icon( a_window, gtk_icon_theme_get_default() );
    }
}

void main_design_mode( FMMainWindow* main_window )
{
    xset_msg_dialog( main_window, 0, _("Design Mode Help"), NULL, 0, _("Design Mode allows you to change the name, shortcut key and icon of menu items and add your own custom commands to menus.\n\nTo open the design menu, simply right-click, middle-click or ctrl-click on a menu item.\n\nTo use Design Mode on a submenu, you must first close the submenu (by clicking on it).  The Bookmarks menu and variable parts of the Open context menu do not support Design Mode.\n\nTo modify a toolbar, click the leftmost tool icon to open the toolbar config menu and select Help."), NULL );
}

void rebuild_toolbar_all_windows( int job, PtkFileBrowser* file_browser )
{
    GList* l;
    FMMainWindow* a_window;
    PtkFileBrowser* a_browser;
    GtkNotebook* notebook;
    int cur_tabx, p;
    int pages;
    char* disp_path;
//printf("rebuild_toolbar_all_windows\n");

    // do this browser first
    if ( file_browser )
    {
        if ( job == 0 && xset_get_b_panel( file_browser->mypanel, "show_toolbox" ) )
        {
            ptk_file_browser_rebuild_toolbox( NULL, file_browser );
            disp_path = g_filename_display_name( ptk_file_browser_get_cwd( file_browser ) );
            gtk_entry_set_text( file_browser->path_bar, disp_path );
            g_free( disp_path );
        }
        else if ( job == 1 && xset_get_b_panel( file_browser->mypanel, "show_sidebar" ) )
            ptk_file_browser_rebuild_side_toolbox( NULL, file_browser );
    }
    
    // do all windows all panels all tabs
    for ( l = all_windows; l; l = l->next )
    {
        a_window = (FMMainWindow*)l->data;
        for ( p = 1; p < 5; p++ )
        {
            notebook = a_window->panel[p-1];
            pages = gtk_notebook_get_n_pages( notebook );
            for ( cur_tabx = 0; cur_tabx < pages; cur_tabx++ )
            {
                a_browser = (PtkFileBrowser*)
                            gtk_notebook_get_nth_page( notebook, cur_tabx );
                if ( a_browser != file_browser )
                {
                    if ( job == 0 && a_browser->toolbar )
                    {
                        ptk_file_browser_rebuild_toolbox( NULL, a_browser );
                        disp_path = g_filename_display_name( ptk_file_browser_get_cwd( a_browser ) );
                        gtk_entry_set_text( a_browser->path_bar, disp_path );
                        g_free( disp_path );
                    }
                    else if ( job == 1 && a_browser->side_toolbar )
                        ptk_file_browser_rebuild_side_toolbox( NULL, a_browser );
                }
            }
        }
    }
    if ( file_browser )
        main_window_autosave( file_browser );
}

void update_views_all_windows( GtkWidget* item, PtkFileBrowser* file_browser )
{
    GList* l;
    FMMainWindow* a_window;
    PtkFileBrowser* a_browser;
    GtkNotebook* notebook;
    int cur_tabx, p;
//printf("update_views_all_windows\n");
    // do this browser first
    ptk_file_browser_update_views( NULL, file_browser );
    p = file_browser->mypanel;

    // do other windows
    for ( l = all_windows; l; l = l->next )
    {
        a_window = (FMMainWindow*)l->data;
        if ( GTK_WIDGET_VISIBLE( a_window->panel[p-1] ) )
        {
            notebook = a_window->panel[p-1];
            cur_tabx = gtk_notebook_get_current_page( notebook );
            if ( cur_tabx != -1 )
            {
                a_browser = (PtkFileBrowser*)
                            gtk_notebook_get_nth_page( notebook, cur_tabx );
                if ( a_browser != file_browser )
                {
                    ptk_file_browser_update_views( NULL, a_browser );
                }
            }
        }
    }
    
    main_window_autosave( file_browser );
}

void focus_panel( GtkMenuItem* item, gpointer mw, int p )
{
    int panel, hidepanel;
    int panel_num;
    
    FMMainWindow* main_window = (FMMainWindow*)mw;
    
    if ( item )
        panel_num = g_object_get_data( G_OBJECT( item ), "panel_num" );
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
        if ( GTK_WIDGET_VISIBLE( main_window->panel[panel-1] ) )
        {
            gtk_widget_grab_focus( GTK_WIDGET( main_window->panel[panel-1] ) );
            main_window->curpanel = panel;
            main_window->notebook = main_window->panel[panel-1];
            PtkFileBrowser* file_browser = fm_main_window_get_current_file_browser( main_window );
            gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );
            set_panel_focus( main_window, file_browser );
        }
        else if ( panel_num != -3 )
        {
            xset_set_b_panel( panel, "show", TRUE );
            show_panels_all_windows( NULL, main_window );
            gtk_widget_grab_focus( GTK_WIDGET( main_window->panel[panel-1] ) );
            main_window->curpanel = panel;
            main_window->notebook = main_window->panel[panel-1];
            PtkFileBrowser* file_browser = fm_main_window_get_current_file_browser( main_window );
            gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );
            set_panel_focus( main_window, file_browser );
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
    show_panels( NULL, main_window );

    // do other windows
    for ( l = all_windows; l; l = l->next )
    {
        a_window = (FMMainWindow*)l->data;
        if ( main_window != a_window )
            show_panels( NULL, a_window );
    }
    
    PtkFileBrowser* file_browser = 
            (PtkFileBrowser*)fm_main_window_get_current_file_browser( main_window );
    main_window_autosave( file_browser );
}

void show_panels( GtkMenuItem* item, FMMainWindow* main_window )
{
    int p, cur_tabx, i;
    char* folder_path;
    XSet* set;
    char* tabs;
    char* end;
    char* tab_dir;
    PtkFileBrowser* file_browser;

    // show panelbar
    if ( !!GTK_WIDGET_VISIBLE( main_window->panelbar ) != !!xset_get_b( "main_pbar" ) )
    {
        if ( xset_get_b( "main_pbar" ) )
            gtk_widget_show( main_window->panelbar );
        else
            gtk_widget_hide( main_window->panelbar );
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
    {
        if ( xset_get_b_panel( p, "show" ) )
        {
            if ( !gtk_notebook_get_n_pages( main_window->panel[p-1] ) )
            {
                main_window->notebook = main_window->panel[p-1];
                main_window->curpanel = p;
                // load saved tabs
                set = xset_get_panel( p, "show" );
                if ( set->s )
                {
                    tabs = set->s;
                    while ( tabs && !strncmp( tabs, "///", 3 ) )
                    {
                        tabs += 3;
                        if ( end = strstr( tabs, "///" ) )
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
                            if ( g_file_test( tab_dir, G_FILE_TEST_IS_DIR ) )
                                folder_path = tab_dir;
                            else if ( !( folder_path = xset_get_s( "go_set_default" ) ) )
                                folder_path = g_get_home_dir();
                            fm_main_window_add_new_tab( main_window, folder_path );
                        }
                        g_free( tab_dir );
                    }
                    if ( set->x )
                    {
                        // set current tab
                        cur_tabx = atoi( set->x );
                        if ( cur_tabx >= 0 && cur_tabx < gtk_notebook_get_n_pages(
                                                    main_window->panel[p-1] ) )
                        {
                            gtk_notebook_set_current_page( main_window->panel[p-1],
                                                                        cur_tabx );
                            file_browser = (PtkFileBrowser*)
                                            gtk_notebook_get_nth_page( main_window->panel[p-1],
                                            cur_tabx );
                            //if ( file_browser->folder_view )
                            //      gtk_widget_grab_focus( file_browser->folder_view );
//printf("call delayed (showpanels) #%d %#x window=%#x\n", cur_tabx, file_browser->folder_view, main_window);
                            g_idle_add( ( GSourceFunc ) delayed_focus, file_browser->folder_view );
                        }
                    }
                }
                else
                {
                    // open default tab
                    if ( !( folder_path = xset_get_s( "go_set_default" ) ) )
                        folder_path = g_get_home_dir();                        
                    fm_main_window_add_new_tab( main_window, folder_path );
                }
            }
            gtk_widget_show( main_window->panel[p-1] );
            if ( !gtk_toggle_tool_button_get_active( 
                                GTK_TOOL_BUTTON( main_window->panel_btn[p-1] ) ) )
            {
                g_signal_handlers_block_matched( main_window->panel_btn[p-1],
                                                    G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                                    on_toggle_panelbar, NULL );
                gtk_toggle_tool_button_set_active( 
                                GTK_TOOL_BUTTON( main_window->panel_btn[p-1] ), TRUE );
                g_signal_handlers_unblock_matched( main_window->panel_btn[p-1],
                                                    G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                                    on_toggle_panelbar, NULL );
            }
        }
        else
        {
            gtk_widget_hide( main_window->panel[p-1] );
            if ( gtk_toggle_tool_button_get_active( 
                                GTK_TOOL_BUTTON( main_window->panel_btn[p-1] ) ) )
            {
                g_signal_handlers_block_matched( main_window->panel_btn[p-1],
                                                    G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                                    on_toggle_panelbar, NULL );
                gtk_toggle_tool_button_set_active( 
                                GTK_TOOL_BUTTON( main_window->panel_btn[p-1] ), FALSE );
                g_signal_handlers_unblock_matched( main_window->panel_btn[p-1],
                                                    G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                                    on_toggle_panelbar, NULL );
            }
        }
    }
    if ( xset_get_b_panel( 1, "show" ) || xset_get_b_panel( 2, "show" ) )
        gtk_widget_show( main_window->hpane_top );
    else
        gtk_widget_hide( main_window->hpane_top );
    if ( xset_get_b_panel( 3, "show" ) || xset_get_b_panel( 4, "show" ) )
        gtk_widget_show( main_window->hpane_bottom );
    else
        gtk_widget_hide( main_window->hpane_bottom );
    
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
                cur_tabx = gtk_notebook_get_current_page( main_window->notebook );
                file_browser = (PtkFileBrowser*)
                                gtk_notebook_get_nth_page( main_window->notebook,
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
                    gtk_toggle_tool_button_get_active( GTK_TOOL_BUTTON( widget ) ) );
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

void rebuild_menus( FMMainWindow* main_window )
{
    GtkMenu* newmenu;
    GtkMenu* submenu;
    GtkMenuItem* item;
    char* menu_elements;
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    XSet* set;

//printf("rebuild_menus\n");
    PtkFileBrowser* file_browser = 
            (PtkFileBrowser*)fm_main_window_get_current_file_browser( main_window );
    XSetContext* context = xset_context_new();
    main_context_fill( file_browser, context );

    // File
    newmenu = gtk_menu_new();
    xset_set_cb( "main_new_window", on_new_window_activate, main_window );
    xset_set_cb( "main_root_window", on_open_current_folder_as_root, main_window );
    xset_set_cb( "main_search", on_find_file_activate, main_window );
    xset_set_cb( "main_terminal", on_open_terminal_activate, main_window );
    xset_set_cb( "main_root_terminal", on_open_root_terminal_activate, main_window );
    xset_set_cb( "main_save_session", on_save_session, main_window );
    xset_set_cb( "main_exit", on_quit_activate, main_window );
    menu_elements = g_strdup_printf( "main_search main_terminal main_root_terminal sep_f1 main_new_window main_root_window sep_f2 main_save_session main_save_tabs sep_f3 main_exit" );
    xset_add_menu( NULL, file_browser, newmenu, accel_group, menu_elements );
    g_free( menu_elements );
    gtk_widget_show_all( GTK_WIDGET(newmenu) );
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( main_window->file_menu_item ), newmenu );
    
    // View
    newmenu = gtk_menu_new();
    xset_set_cb( "main_prefs", on_preference_activate, main_window );
    xset_set_cb( "font_task", main_update_fonts, file_browser );
    xset_set_cb( "main_full", on_fullscreen_activate, main_window );
    xset_set_cb( "main_design_mode", main_design_mode, NULL );
    xset_set_cb( "main_icon", on_main_icon, NULL );
    xset_set_cb( "main_title", update_window_title, main_window );
    menu_elements = g_strdup_printf( "panel1_show panel2_show panel3_show panel4_show main_pbar main_focus_panel sep_v1 main_tasks sep_v2 main_title main_icon sep_v3 main_full main_design_mode main_prefs" );
    
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
        xset_set_ob1( set, "panel_num", -1 );
        set->disable = ( vis_count == 1 );
    set = xset_set_cb( "panel_next", focus_panel, main_window );
        xset_set_ob1( set, "panel_num", -2 );
        set->disable = ( vis_count == 1 );
    set = xset_set_cb( "panel_hide", focus_panel, main_window );
        xset_set_ob1( set, "panel_num", -3 );
        set->disable = ( vis_count == 1 );
    set = xset_set_cb( "panel_1", focus_panel, main_window );
        xset_set_ob1( set, "panel_num", 1 );
        set->disable = ( main_window->curpanel == 1 );
    set = xset_set_cb( "panel_2", focus_panel, main_window );
        xset_set_ob1( set, "panel_num", 2 );
        set->disable = ( main_window->curpanel == 2 );
    set = xset_set_cb( "panel_3", focus_panel, main_window );
        xset_set_ob1( set, "panel_num", 3 );
        set->disable = ( main_window->curpanel == 3 );
    set = xset_set_cb( "panel_4", focus_panel, main_window );
        xset_set_ob1( set, "panel_num", 4 );
        set->disable = ( main_window->curpanel == 4 );
        
    main_task_prepare_menu( main_window, newmenu, accel_group );
    xset_add_menu( NULL, file_browser, newmenu, accel_group, menu_elements );
    g_free( menu_elements );
    gtk_widget_show_all( GTK_WIDGET(newmenu) );
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( main_window->view_menu_item ), newmenu );

    // Bookmarks
    if ( !main_window->book_menu )
    {
        main_window->book_menu = create_bookmarks_menu( main_window );
        gtk_menu_item_set_submenu( GTK_MENU_ITEM( main_window->book_menu_item ),
                                                        main_window->book_menu );
    }

    // Plugins
    main_window->plug_menu = create_plugins_menu( main_window );
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( main_window->plug_menu_item ),
                                                    main_window->plug_menu );
    
    // Tool
    XSet* child_set;
    newmenu = gtk_menu_new();
    set = xset_get( "main_tool" );
    if ( !set->child )
    {
        child_set = xset_custom_new();
        child_set->menu_label = g_strdup_printf( _("New _Command") );
        child_set->parent = g_strdup_printf( "main_tool" );
        set->child = g_strdup( child_set->name );
    }
    else
        child_set = xset_get( set->child );
    xset_add_menuitem( NULL, file_browser, newmenu, accel_group, child_set );
    gtk_widget_show_all( GTK_WIDGET(newmenu) );
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( main_window->tool_menu_item ), newmenu );
    
    // Help
    newmenu = gtk_menu_new();
    xset_set_cb( "main_design_help", main_design_mode, main_window );
    xset_set_cb( "main_about", on_about_activate, main_window );
    menu_elements = g_strdup_printf( "main_design_help main_about" );
    xset_add_menu( NULL, file_browser, newmenu, accel_group, menu_elements );
    g_free( menu_elements );
    gtk_widget_show_all( GTK_WIDGET(newmenu) );
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( main_window->help_menu_item ), newmenu );
//printf("rebuild_menus  DONE\n");
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

    /* Initialize parent class */
    GTK_WINDOW( main_window )->type = GTK_WINDOW_TOPLEVEL;

    main_window->autosave_timer = 0;
    
    /* this is used to limit the scope of gtk_grab and modal dialogs */
    main_window->wgroup = gtk_window_group_new();
    gtk_window_group_add_window( main_window->wgroup, (GtkWindow*)main_window );

    /* Add to total window count */
    ++n_windows;
    all_windows = g_list_prepend( all_windows, main_window );

    pcmanfm_ref();

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
    GtkHBox* menu_hbox = gtk_hbox_new ( FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX ( menu_hbox ),
                         main_window->menu_bar, TRUE, TRUE, 0 );

    // panelbar
    main_window->panelbar = gtk_toolbar_new();
    gtk_toolbar_set_style( main_window->panelbar, GTK_TOOLBAR_ICONS );
    gtk_toolbar_set_icon_size( main_window->panelbar, GTK_ICON_SIZE_MENU );
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
        gtk_toolbar_insert( main_window->panelbar,
                                GTK_TOOL_ITEM( main_window->panel_btn[i] ), -1 );
        g_signal_connect( main_window->panel_btn[i], "toggled",
                                G_CALLBACK( on_toggle_panelbar ), main_window );
        if ( i == 1 )
        {
            GtkWidget* sep = gtk_separator_tool_item_new();
            gtk_separator_tool_item_set_draw( sep, TRUE );
            gtk_toolbar_insert( main_window->panelbar, GTK_TOOL_ITEM( sep ), -1 );
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
    
    main_window->book_menu_item = gtk_menu_item_new_with_mnemonic( _("_Bookmarks") );
    gtk_menu_shell_append( GTK_MENU_SHELL( main_window->menu_bar ), main_window->book_menu_item );
    main_window->book_menu = NULL;
    // Set a monitor for changes of the bookmarks
    ptk_bookmarks_add_callback( ( GFunc ) on_bookmarks_change, main_window );

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
        gtk_notebook_set_show_border( main_window->panel[i], FALSE );
        gtk_notebook_set_scrollable ( main_window->panel[i], TRUE );
        g_signal_connect ( main_window->panel[i], "switch-page",
                        G_CALLBACK ( on_folder_notebook_switch_pape ), main_window );
        // create dynamic copies of panel slider positions
        main_window->panel_slide_x[i] = xset_get_int_panel( i + 1,
                                                        "slider_positions", "x" );
        main_window->panel_slide_y[i] = xset_get_int_panel( i + 1,
                                                        "slider_positions", "y" );
        main_window->panel_slide_s[i] = xset_get_int_panel( i + 1,
                                                        "slider_positions", "s" );
    }

    main_window->task_scroll = gtk_scrolled_window_new( NULL, NULL );
    
    gtk_paned_pack1( GTK_PANED(main_window->hpane_top), main_window->panel[0],
                                                                    TRUE, TRUE );
    gtk_paned_pack2( GTK_PANED(main_window->hpane_top), main_window->panel[1],
                                                                    TRUE, TRUE );
    gtk_paned_pack1( GTK_PANED(main_window->hpane_bottom), main_window->panel[2],
                                                                    TRUE, TRUE );
    gtk_paned_pack2( GTK_PANED(main_window->hpane_bottom), main_window->panel[3],
                                                                    TRUE, TRUE );

    gtk_paned_pack1( GTK_PANED(main_window->vpane), main_window->hpane_top,
                                                                    TRUE, TRUE );
    gtk_paned_pack2( GTK_PANED(main_window->vpane), main_window->hpane_bottom,
                                                                    TRUE, TRUE );

    gtk_paned_pack1( GTK_PANED(main_window->task_vpane), main_window->vpane,
                                                                    TRUE, TRUE );
    gtk_paned_pack2( GTK_PANED(main_window->task_vpane), main_window->task_scroll,
                                                                    TRUE, TRUE );

    gtk_box_pack_start ( GTK_BOX ( main_window->main_vbox ),
                               GTK_WIDGET( main_window->task_vpane ), TRUE, TRUE, 0 );

    int pos = xset_get_int( "panel_sliders", "x" );
    if ( pos < 150 ) pos = -1;
    gtk_paned_set_position( main_window->hpane_top, pos );

    pos = xset_get_int( "panel_sliders", "y" );
    if ( pos < 150 ) pos = -1;
    gtk_paned_set_position( main_window->hpane_bottom, pos );

    pos = xset_get_int( "panel_sliders", "s" );
    if ( pos < 200 ) pos = -1;
    gtk_paned_set_position( main_window->vpane, pos );

    pos = xset_get_int( "panel_sliders", "z" );
    if ( pos < 200 ) pos = -1;
    gtk_paned_set_position( main_window->task_vpane, pos );

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
    gtk_container_add( GTK_CONTAINER( main_window->task_scroll ), main_window->task_view );    

    gtk_widget_show_all( main_window->main_vbox );

    g_signal_connect( G_OBJECT( main_window->file_menu_item ), "button-press-event",
                      G_CALLBACK( on_menu_bar_event ), main_window );
    g_signal_connect( G_OBJECT( main_window->view_menu_item ), "button-press-event",
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

    import_all_plugins( main_window );
    on_task_popup_show( NULL, main_window, NULL );
    show_panels( NULL, main_window );
    main_window_root_bar_all();
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
    gtk_widget_destroy( main_window );
}

void on_abort_tasks_response( GtkDialog* dlg, int response, GtkWidget* main_window )
{
    gtk_widget_destroy( main_window );
}

gboolean fm_main_window_delete_event ( GtkWidget *widget,
                              GdkEvent *event )
{
//printf("fm_main_window_delete_event\n");

    FMMainWindow* main_window = (FMMainWindow*)widget;
    
    // store width/height + sliders
    int pos;
    char* posa;
    
    // fullscreen?
    if ( !xset_get_b( "main_full" ) )
    {
        app_settings.width = GTK_WIDGET( main_window ) ->allocation.width;
        app_settings.height = GTK_WIDGET( main_window ) ->allocation.height;
        if ( GTK_IS_PANED( main_window->hpane_top ) )
        {
            pos = gtk_paned_get_position( main_window->hpane_top );
            if ( pos )
            {
                posa = g_strdup_printf( "%d", pos );
                xset_set( "panel_sliders", "x", posa );
                g_free( posa );
            }
            
            pos = gtk_paned_get_position( main_window->hpane_bottom );
            if ( pos )
            {
                posa = g_strdup_printf( "%d", pos );
                xset_set( "panel_sliders", "y", posa );
                g_free( posa );
            }
            
            pos = gtk_paned_get_position( main_window->vpane );
            if ( pos )
            {
                posa = g_strdup_printf( "%d", pos );
                xset_set( "panel_sliders", "s", posa );
                g_free( posa );
            }

            pos = gtk_paned_get_position( main_window->task_vpane );
            if ( pos )
            {
                posa = g_strdup_printf( "%d", pos );
                xset_set( "panel_sliders", "z", posa );
                g_free( posa );
            }
        }

        // store task columns
        on_task_columns_changed( main_window->task_view, NULL );

        // store fb columns
        int p, page_x;
        PtkFileBrowser* a_browser;
        for ( p = 1; p < 5; p++ )
        {
            page_x = gtk_notebook_get_current_page( main_window->panel[p-1] );
            if ( page_x != -1 )
            {
                a_browser = ( PtkFileBrowser* ) gtk_notebook_get_nth_page(
                                                    main_window->panel[p-1], page_x );
                if ( a_browser->view_mode == PTK_FB_LIST_VIEW )
                    on_folder_view_columns_changed( a_browser->folder_view, a_browser );
            }
        }
    }

    // save settings
    if ( main_window->autosave_timer )
    {
        g_source_remove( main_window->autosave_timer );
        main_window->autosave_timer = 0;
    }
    char* err_msg = save_settings( main_window );
    if ( err_msg )
    {
        char* msg = g_strdup_printf( _("Error: Unable to save session file.  Do you want to exit without saving?\n\n( %s )"), err_msg );
        g_free( err_msg );
        GtkWidget* dlg = gtk_message_dialog_new( GTK_WINDOW( widget ), GTK_DIALOG_MODAL,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_YES_NO,
                                        msg );
        g_free( msg );
        gtk_dialog_set_default_response( dlg, GTK_RESPONSE_NO );
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
    if ( main_tasks_running( widget ) )
    {
        GtkWidget* dlg = gtk_message_dialog_new( GTK_WINDOW( widget ),
                                              GTK_DIALOG_MODAL,
                                              GTK_MESSAGE_QUESTION,
                                              GTK_BUTTONS_YES_NO,
                                              _( "Stop all tasks running in this window?" ) );
        gtk_dialog_set_default_response( dlg, GTK_RESPONSE_NO );
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
            
            on_task_stop_all( NULL, main_window->task_view );
            while ( main_tasks_running( widget ) )
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
    fm_main_window_close( widget );
    return TRUE;
}

static gboolean fm_main_window_window_state_event ( GtkWidget *widget,
                                                    GdkEventWindowState *event )
{
    app_settings.maximized = ((event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0);
    return TRUE;
}

char* main_window_get_tab_cwd( PtkFileBrowser* file_browser, int tab_num )
{
    if ( !file_browser )
        return NULL;
    int page_x;
    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;
    GtkNotebook* notebook = main_window->panel[file_browser->mypanel - 1];
    int pages = gtk_notebook_get_n_pages( notebook );
    int page_num = gtk_notebook_page_num( notebook, GTK_WIDGET( file_browser ) );
    if ( tab_num == -1 ) // prev
        page_x = page_num - 1;
    else if ( tab_num == -2 ) // next
        page_x = page_num + 1;
    else
        page_x = tab_num - 1; // tab_num starts counting at 1
    
    if ( page_x > -1 && page_x < pages )
    {
        return g_strdup( ptk_file_browser_get_cwd( gtk_notebook_get_nth_page(
                                                            notebook, page_x ) ) );
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
        } while ( !GTK_WIDGET_VISIBLE( main_window->panel[panel_x - 1] ) );
    }
    else if ( panel_num == -2 ) // next
    {
        do
        {
            if ( ++panel_x > 4 )
                panel_x = 1;
            if ( panel_x == file_browser->mypanel )
                return NULL;
        } while ( !GTK_WIDGET_VISIBLE( main_window->panel[panel_x - 1] ) );
    }
    else
    {
        panel_x = panel_num;
        if ( !GTK_WIDGET_VISIBLE( main_window->panel[panel_x - 1] ) )
            return NULL;
    }
    
    GtkNotebook* notebook = main_window->panel[panel_x - 1];
    int page_x = gtk_notebook_get_current_page( notebook );
    return g_strdup( ptk_file_browser_get_cwd( gtk_notebook_get_nth_page(
                                                        notebook, page_x ) ) );
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
        } while ( !GTK_WIDGET_VISIBLE( main_window->panel[panel_x - 1] ) );
    }
    else if ( panel_num == -2 ) // next
    {
        do
        {
            if ( ++panel_x > 4 )
                panel_x = 1;
            if ( panel_x == file_browser->mypanel )
                return;
        } while ( !GTK_WIDGET_VISIBLE( main_window->panel[panel_x - 1] ) );
    }
    else
    {
        panel_x = panel_num;
    }

    if ( panel_x < 1 || panel_x > 4 )
        return;

    // show panel
    if ( !GTK_WIDGET_VISIBLE( main_window->panel[panel_x - 1] ) )
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
    while( gtk_events_pending() )
        gtk_main_iteration();
    //gtk_widget_grab_focus( GTK_WIDGET( main_window->notebook ) );
    gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );
}

gboolean main_window_panel_is_visible( PtkFileBrowser* file_browser, int panel )
{
    if ( panel < 1 || panel > 4 )
        return FALSE;
    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;
    return GTK_WIDGET_VISIBLE( main_window->panel[panel - 1] );
}

void main_window_get_counts( PtkFileBrowser* file_browser, int* panel_count,
                                                int* tab_count, int* tab_num )
{
    if ( !file_browser )
        return;
    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;
    GtkNotebook* notebook = main_window->panel[file_browser->mypanel - 1];
    *tab_count = gtk_notebook_get_n_pages( notebook );
    // tab_num starts counting from 1
    *tab_num = gtk_notebook_page_num( notebook, GTK_WIDGET( file_browser ) ) + 1;
    int count = 0;
    int i;
    for ( i = 0; i < 4; i++ )
    {
        if ( GTK_WIDGET_VISIBLE( main_window->panel[i] ) )
            count++;
    }
    *panel_count = count;
}

void on_close_notebook_page( GtkButton* btn, PtkFileBrowser* file_browser )
{
    GtkNotebook* notebook = GTK_NOTEBOOK(
                                 gtk_widget_get_ancestor( GTK_WIDGET( file_browser ),
                                                          GTK_TYPE_NOTEBOOK ) );
    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;

    main_window->curpanel = file_browser->mypanel;
    main_window->notebook = main_window->panel[main_window->curpanel - 1];

    gtk_widget_destroy( GTK_WIDGET( file_browser ) );

    if ( !app_settings.always_show_tabs )
    {
        if ( gtk_notebook_get_n_pages ( notebook ) == 1 )
            gtk_notebook_set_show_tabs( notebook, FALSE );
    }
    if ( gtk_notebook_get_n_pages ( notebook ) == 0 )
        fm_main_window_add_new_tab( main_window,
                                        xset_get_s( "go_set_default" ) );
    update_window_title( NULL, main_window );
}

gboolean notebook_clicked (GtkWidget* widget, GdkEventButton * event,
                           PtkFileBrowser* file_browser)  //MOD added
{
    on_file_browser_panel_change( file_browser,
                                        (FMMainWindow*)file_browser->main_window );
    // middle-click on tab closes
    if (event->type == GDK_BUTTON_PRESS)
    {
        if ( event->button == 2 )
        {
            on_close_notebook_page( NULL, file_browser );
        }
        else if ( event->button == 3 )
        {
            GtkMenu* popup = gtk_menu_new();
            GtkAccelGroup* accel_group = gtk_accel_group_new();
            XSetContext* context = xset_context_new();
            main_context_fill( file_browser, context );
            
            XSet* set = xset_set_cb( "tab_close",
                                                on_close_notebook_page, file_browser );
            xset_add_menuitem( NULL, file_browser, popup, accel_group, set );
            set = xset_set_cb( "tab_new", on_shortcut_new_tab_activate, file_browser );
            xset_add_menuitem( NULL, file_browser, popup, accel_group, set );
            set = xset_set_cb( "tab_new_here", on_shortcut_new_tab_here, file_browser );
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
            gtk_menu_popup( GTK_MENU( popup ), NULL, NULL,
                                NULL, NULL, event->button, event->time );
        }
    }    
    return FALSE;
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
    
    ptk_file_browser_select_last( file_browser );  //MOD restore last selections

    gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );  //MOD
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

    gtk_label_set_ellipsize( tab_text, PANGO_ELLIPSIZE_MIDDLE );
    gtk_label_set_max_width_chars( tab_text, 30 );
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
        }
        else
        {
            close_icon = gtk_image_new_from_stock( GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU );
        }
        gtk_container_add ( GTK_CONTAINER ( close_btn ), close_icon );
        gtk_box_pack_start ( GTK_BOX( tab_label ),
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

    label = gtk_notebook_get_tab_label ( main_window->notebook,
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
        g_free( name );

g_list_free( children );  //sfm 0.6.0 enabled
    }
}


void fm_main_window_add_new_tab( FMMainWindow* main_window,
                                 const char* folder_path )
{
    GtkWidget * tab_label;
    gint idx;
    GtkNotebook* notebook = main_window->notebook;

    PtkFileBrowser* curfb = (PtkFileBrowser*)fm_main_window_get_current_file_browser
                                                                ( main_window );
    if ( curfb )
    {
        // save sliders of current fb ( new tab while task manager is shown changes vals )
        ptk_file_browser_slider_release( NULL, NULL, curfb );
        // save column widths of fb so new tab has same
        on_folder_view_columns_changed( curfb->folder_view, curfb );
    }
    int i = main_window->curpanel -1;
    PtkFileBrowser* file_browser = (PtkFileBrowser*)ptk_file_browser_new(
                                    main_window->curpanel, notebook,
                                    main_window->task_view, main_window,
                                    &main_window->panel_slide_x[i],
                                    &main_window->panel_slide_y[i],
                                    &main_window->panel_slide_s[i] );

    ptk_file_browser_set_single_click( file_browser, app_settings.single_click );
    // FIXME: this shouldn't be hard-code
    ptk_file_browser_set_single_click_timeout( file_browser, 400 );
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
    g_signal_connect( file_browser, "begin-chdir",
                      G_CALLBACK( on_file_browser_begin_chdir ), main_window );
    g_signal_connect( file_browser, "content-change",
                      G_CALLBACK( on_file_browser_content_change ), main_window );
*/
    g_signal_connect( file_browser, "after-chdir",
                      G_CALLBACK( on_file_browser_after_chdir ), main_window );
    g_signal_connect( file_browser, "open-item",
                      G_CALLBACK( on_file_browser_open_item ), main_window );
    g_signal_connect( file_browser, "sel-change",
                      G_CALLBACK( on_file_browser_sel_change ), main_window );
    g_signal_connect( file_browser, "pane-mode-change",
                      G_CALLBACK( on_file_browser_panel_change ), main_window );

    tab_label = fm_main_window_create_tab_label( main_window, file_browser );
    idx = gtk_notebook_append_page( notebook, GTK_WIDGET( file_browser ), tab_label );
#if GTK_CHECK_VERSION( 2, 10, 0 )
    gtk_notebook_set_tab_reorderable( notebook, GTK_WIDGET( file_browser ), TRUE );
#endif
    gtk_notebook_set_current_page ( notebook, idx );

    if (app_settings.always_show_tabs)
        gtk_notebook_set_show_tabs( notebook, TRUE );
    else
        if ( gtk_notebook_get_n_pages ( notebook ) > 1 )
            gtk_notebook_set_show_tabs( notebook, TRUE );
        else
            gtk_notebook_set_show_tabs( notebook, FALSE );

    if ( !ptk_file_browser_chdir( file_browser, folder_path, PTK_FB_CHDIR_ADD_HISTORY ) )
        ptk_file_browser_chdir( file_browser, "/", PTK_FB_CHDIR_ADD_HISTORY );

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
    if ( main_window->notebook )
    {
        gint idx = gtk_notebook_get_current_page( main_window->notebook );
        if ( idx >= 0 )
            return gtk_notebook_get_nth_page( main_window->notebook, idx );
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
        gtk_about_dialog_set_url_hook( open_url, NULL, NULL);

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

        g_object_add_weak_pointer( G_OBJECT(about_dlg), &about_dlg );
        g_signal_connect( about_dlg, "response", G_CALLBACK(gtk_widget_destroy), NULL );
        g_signal_connect( about_dlg, "destroy", G_CALLBACK(pcmanfm_unref), NULL );
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
    GtkWidget * new_win = fm_main_window_new();
    gtk_window_set_default_size( GTK_WINDOW( new_win ),
                                 GTK_WIDGET( main_window ) ->allocation.width,
                                 GTK_WIDGET( main_window ) ->allocation.height );
    gtk_widget_show( new_win );
}

void
on_new_window_activate ( GtkMenuItem *menuitem,
                         gpointer user_data )
{
    FMMainWindow* main_window = FM_MAIN_WINDOW( user_data );
    save_settings( main_window );  // reset saved tabs
    fm_main_window_add_new_window( main_window );
}

static gboolean delayed_focus( GtkWidget* widget )
{
    gdk_threads_enter();
//printf( "delayed_focus %#x\n", widget);
    gtk_widget_grab_focus( widget );
    gdk_threads_leave();
    return FALSE;
}

void set_panel_focus( FMMainWindow* main_window, PtkFileBrowser* file_browser )
{
    int p, pages, cur_tabx;
    GtkNotebook* notebook;
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
        pages = gtk_notebook_get_n_pages( notebook );
        for ( cur_tabx = 0; cur_tabx < pages; cur_tabx++ )
        {
            a_browser = (PtkFileBrowser*)
                        gtk_notebook_get_nth_page( notebook, cur_tabx );
            if ( a_browser != file_browser )
                ptk_file_browser_status_change( a_browser, p == mw->curpanel );
        }
        gtk_widget_set_sensitive( mw->panel_image[p-1],
                                                p == mw->curpanel );
    }
    
    update_window_title( NULL, mw );
}

void on_fullscreen_activate ( GtkMenuItem *menuitem, FMMainWindow* main_window )
{
    if ( xset_get_b( "main_full" ) )
    {
        gtk_widget_hide( main_window->menu_bar );
        gtk_widget_hide( main_window->panelbar );
        gtk_window_fullscreen( GTK_WINDOW( main_window ) );
    }
    else
    {
        gtk_window_unfullscreen( GTK_WIDGET( main_window ) );
        gtk_widget_show( main_window->menu_bar );
        gtk_widget_show( main_window->panelbar );
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
        char* path = ptk_file_browser_get_cwd( file_browser );
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
    PtkFileBrowser* file_browser = (PtkFileBrowser*)fm_main_window_get_current_file_browser
                                                                ( main_window );
    if ( file_browser )
        set_window_title( main_window, file_browser );
}

void
on_folder_notebook_switch_pape ( GtkNotebook *notebook,
                                 GtkNotebookPage *page,
                                 guint page_num,
                                 gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser;
    const char* path;
    char* disp_path;

    // save sliders of current fb ( new tab while task manager is shown changes vals )
    PtkFileBrowser* curfb = (PtkFileBrowser*)fm_main_window_get_current_file_browser
                                                    ( main_window );
    if ( curfb )
        ptk_file_browser_slider_release( NULL, NULL, curfb );

    file_browser = ( PtkFileBrowser* ) gtk_notebook_get_nth_page( notebook, page_num );
//printf("on_folder_notebook_switch_pape  panel change %d %d\n", file_browser->mypanel, page_num );
    main_window->curpanel = file_browser->mypanel;
    main_window->notebook = main_window->panel[main_window->curpanel - 1];

//    fm_main_window_update_command_ui( main_window, file_browser );
//    fm_main_window_update_status_bar( main_window, file_browser );

    //gtk_paned_set_position ( GTK_PANED ( file_browser ), main_window->splitter_pos );

    set_window_title( main_window, file_browser );
    
    // block signal in case tab is being closed due to main iteration in update views
    g_signal_handlers_block_matched( main_window->panel[file_browser->mypanel - 1],
            G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_folder_notebook_switch_pape, NULL );
    
    ptk_file_browser_update_views( NULL, file_browser );
    
    g_signal_handlers_unblock_matched( main_window->panel[file_browser->mypanel - 1],
            G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_folder_notebook_switch_pape, NULL );
//printf("call delayed (switch) #%d %d\n", page_num, file_browser->folder_view);
    g_idle_add( ( GSourceFunc ) delayed_focus, file_browser->folder_view );
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
            file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
            fm_main_window_add_new_tab( main_window, path );
            break;
        case PTK_OPEN_NEW_WINDOW:
            file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
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
    int n, hn, hnx;
    guint64 total_size;
    char *msg;
    char size_str[ 64 ];
    char free_space[100];
#ifdef HAVE_STATVFS
    struct statvfs fs_stat = {0};
#endif

    if ( !file_browser )
        return;
        //file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );

    free_space[0] = '\0';
#ifdef HAVE_STATVFS
// FIXME: statvfs support should be moved to src/vfs

    if( statvfs( ptk_file_browser_get_cwd(file_browser), &fs_stat ) == 0 )
    {
        char total_size_str[ 64 ];
        vfs_file_size_to_string_format( size_str, fs_stat.f_bsize * fs_stat.f_bavail, "%.0f %s" );
        vfs_file_size_to_string_format( total_size_str, fs_stat.f_frsize * fs_stat.f_blocks, "%.0f %s" );
        g_snprintf( free_space, G_N_ELEMENTS(free_space),
                    _(" %s free / %s   "), size_str, total_size_str );  //MOD
    }
#endif

    n = ptk_file_browser_get_n_sel( file_browser, &total_size );

    char* link_info = NULL;  //MOD added
    if ( n > 0 )
    {
        if ( n == 1 )  //MOD added
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
                char* cwd = ptk_file_browser_get_cwd( file_browser );
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
                                        link_info = g_strdup_printf( _("  Link-> %s"), target );
                                    else
                                        link_info = g_strdup_printf( _("  Link-> %s/"), target );
                                }
                                else
                                    link_info = g_strdup_printf( _("  !Link-> %s/ (missing)"), target );
                            }
                            else
                            {
                                if ( stat( target_path, &results ) == 0 )
                                {
                                    vfs_file_size_to_string( buf, results.st_size );
                                    lsize = g_strdup( buf );
                                    link_info = g_strdup_printf( _("  Link-> %s (%s)"), target, lsize );
                                }
                                else
                                    link_info = g_strdup_printf( _("  !Link-> %s (missing)"), target );
                            }
                            g_free( target );
                            if ( full_target )
                                g_free( full_target );
                        }
                        else
                            link_info = g_strdup_printf( _("  !Link-> ( error reading target )") );
                            
                        g_free( file_path );
                    }
                    else
                        link_info = g_strdup_printf( "  %s", vfs_file_info_get_name( file ) );
                    vfs_file_info_unref( file );
                }
            }
        }
        if ( ! link_info )
            link_info = g_strdup_printf( "" );
            
        vfs_file_size_to_string( size_str, total_size );
        msg = g_strdup_printf( ngettext( _("%s%d sel (%s)%s"),  //MOD
                         _("%s%d sel (%s)"), n ), free_space, n, size_str, link_info );  //MOD
    }
    else
    {
        // MOD add count for .hidden files
        char* xhidden;
        n = ptk_file_browser_get_n_visible_files( file_browser );
        hn = ptk_file_browser_get_n_all_files( file_browser ) - n;
        if ( hn < 0 ) hn = 0;  // needed due to bug in get_n_visible_files?
        hnx = file_browser->dir ? file_browser->dir->xhidden_count : 0;
        //VFSDir* xdir = file_browser->dir;
        if ( hn || hnx )
        {
            if ( hnx )
                xhidden = g_strdup_printf( "+%d", hnx );
            else
                xhidden = g_strdup_printf( "" );

            char hidden[128];
            char *hnc = NULL;
            char* hidtext = ngettext( "hidden", "hidden", hn);
            g_snprintf( hidden, 127, g_strdup_printf( "%d%s %s", hn, xhidden, hidtext ), hn );
            //msg = g_strdup_printf( ngettext( "%d visible item (%s)%s",
            //                                 "%d visible items (%s)%s", n ), n, hidden, free_space );
            msg = g_strdup_printf( ngettext( "%s%d visible (%s)",
                                             "%s%d visible (%s)", n ), free_space, n, hidden );
        }
        else
            msg = g_strdup_printf( ngettext( "%s%d item",
                                             "%s%d items", n ), free_space, n );
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

gboolean on_main_window_focus( GtkWidget* main_window,
                               GdkEventFocus *event,
                               gpointer user_data )
{
    //this causes a widget not realized loop by running rebuild_menus while
    //rebuild_menus is already running
    // but this unneeded anyway?  cross-window menu changes seem to work ok
    //rebuild_menus( main_window );  // xset may change in another window

    GList * active;
    if ( all_windows->data == ( gpointer ) main_window )
    {
        return FALSE;
    }
    active = g_list_find( all_windows, main_window );
    if ( active )
    {
        all_windows = g_list_remove_link( all_windows, active );
        all_windows->prev = active;
        active->next = all_windows;
        all_windows = active;
    }
    return FALSE;
}

static gboolean on_main_window_keypress( FMMainWindow* main_window, GdkEventKey* event,
                                                                gpointer user_data)
{
    //MOD intercept xset key
//printf("main_keypress %d %d\n", event->keyval, event->state );

    GList* l;
    XSet* set;
    PtkFileBrowser* browser;

    if ( event->keyval == 0 )
        return FALSE;

    int keymod = ( event->state & ( GDK_SHIFT_MASK | GDK_CONTROL_MASK |
                 GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK ) );

    if ( event->keyval == GDK_Escape 
            || event->keyval == GDK_Home
            || event->keyval == GDK_End
            || event->keyval == GDK_Delete
            || event->keyval == GDK_BackSpace )
    {
        browser = (PtkFileBrowser*) 
                            fm_main_window_get_current_file_browser( main_window );
        if ( browser && browser->path_bar && gtk_widget_has_focus( browser->path_bar ) )
            return FALSE;  // send to smartbar
    }

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
                {
                    // use current panel's set
                    browser = (PtkFileBrowser*) 
                            fm_main_window_get_current_file_browser( main_window );
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
            
        if ( ((XSet*)l->data)->key == event->keyval
                                        && ((XSet*)l->data)->keymod == keymod )
        {
            set = (XSet*)l->data;
_key_found:
            browser = (PtkFileBrowser*) 
                            fm_main_window_get_current_file_browser( main_window );
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
                                    && gtk_widget_is_focus( browser->side_dir ) );
                if ( !gtk_widget_is_focus( browser->folder_view ) 
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
                ptk_location_view_on_action( browser->side_dev, set );
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
                else if ( !strcmp( xname, "search_window" ) )
                    on_find_file_activate( NULL, main_window );
                else if ( !strcmp( xname, "terminal" ) )
                    on_open_terminal_activate( NULL, main_window );
                else if ( !strcmp( xname, "root_terminal" ) )
                    on_open_root_terminal_activate( NULL, main_window );
                else if ( !strcmp( xname, "save_session" ) )
                    on_save_session( NULL, main_window );
                else if ( !strcmp( xname, "exit" ) )
                    on_quit_activate( NULL, main_window );
                else if ( !strcmp( xname, "full" ) )
                    on_fullscreen_activate( NULL, main_window );
                else if ( !strcmp( xname, "prefs" ) )
                    on_preference_activate( NULL, main_window );
                else if ( !strcmp( xname, "design_mode" ) 
                        || !strcmp( xname, "design_help" ) )
                    main_design_mode( main_window );
                else if ( !strcmp( xname, "pbar" ) )
                    show_panels_all_windows( NULL, main_window );
                else if ( !strcmp( xname, "icon" ) )
                    on_main_icon();
                else if ( !strcmp( xname, "title" ) )
                    update_window_title( NULL, main_window );
                else if ( !strcmp( xname, "about" ) )
                    on_about_activate( NULL, main_window );
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
                    on_reorder( NULL, browser->task_view );
                else if ( g_str_has_prefix( xname, "col_" ) )
                    on_task_column_selected( NULL, browser->task_view );
                else if ( !strcmp( xname, "stop" ) )
                {
                    PtkFileTask* task = get_selected_task( browser->task_view );
                    if ( task )
                        on_task_stop( NULL, browser->task_view, task );
                }
                else if ( !strcmp( xname, "stop_all" ) )
                    on_task_stop_all( NULL, browser->task_view );
                else if ( g_str_has_prefix( xname, "err_" ) )
                    on_task_popup_errset( NULL, main_window, set->name );
            }
            else if ( !strcmp( set->name, "font_task" ) )
                main_update_fonts( NULL, browser );
            else
                ptk_file_browser_on_action( browser, set->name );
            
            return TRUE;
        }
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
        N_( "Status" ), N_( "#" ), N_( "Path" ), N_( "Item" ),
        N_( "To" ), N_( "Progress" ), N_( "Total" ),
        N_( "Started" ), N_( "Elapsed" ), N_( "Current" ), N_( "Estimate" ),
        N_( "Speed" ), N_( "Remain" ), N_( "StartTime" )
    };
const char* task_names[] =
    {
        N_( "task_col_status" ), N_( "task_col_count" ),
        N_( "task_col_path" ), N_( "task_col_file" ), N_( "task_col_to" ),
        N_( "task_col_progress" ), N_( "task_col_total" ), N_( "task_col_started" ),
        N_( "task_col_elapsed" ), N_( "task_col_curspeed" ), N_( "task_col_curest" ),
        N_( "task_col_avgspeed" ), N_( "task_col_avgest" )
    };

void on_reorder( GtkWidget* item, GtkWidget* parent )
{
    xset_msg_dialog( parent, 0, _("Reorder Columns Help"), NULL, 0, _("To change the order of the columns, drag the column header to the desired location."), NULL );
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
    PtkFileTask* task;
    char* path;
    GtkTreeModel* model;
    GtkTreeModel* model_task;
    GtkTreeIter it;

    c->valid = FALSE;
    if ( !file_browser )
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
    
    if ( file_browser->side_book )
    {
        c->var[CONTEXT_BOOKMARK] = g_strdup( ptk_bookmark_view_get_selected_dir( 
                                                            file_browser->side_book ) );
    }
    if ( !c->var[CONTEXT_BOOKMARK] )
        c->var[CONTEXT_BOOKMARK] = g_strdup( "" );
    
#ifndef HAVE_HAL
    // device
    if ( file_browser->side_dev && 
            ( vol = ptk_location_view_get_selected_vol( file_browser->side_dev ) ) )
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
            { old_flags = flags; flags = g_strdup_printf( "%s udisks_hide", flags ); g_free( old_flags ); }
        if ( vol->nopolicy )
            { old_flags = flags; flags = g_strdup_printf( "%s udisks_noauto", flags ); g_free( old_flags ); }

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
        i = gtk_notebook_get_current_page( main_window->panel[p-1] );
        if ( i != -1 )
        {
            a_browser = (PtkFileBrowser*)
                        gtk_notebook_get_nth_page( main_window->panel[p-1], i );
        }
        else
            continue;
        if ( !a_browser || !GTK_WIDGET_VISIBLE( a_browser ) )
            continue;

        panel_count++;
        c->var[CONTEXT_PANEL1_DIR + p - 1] = g_strdup( ptk_file_browser_get_cwd( 
                                                                    a_browser ) );
#ifndef HAVE_HAL
        if ( a_browser->side_dev && 
                ( vol = ptk_location_view_get_selected_vol( a_browser->side_dev ) ) )
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
            g_list_foreach( sel_files, gtk_tree_path_free, NULL );
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
                            gtk_notebook_get_n_pages( main_window->panel[p-1] ) );
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
    if ( task = get_selected_task( file_browser->task_view ) )
    {
        c->var[CONTEXT_TASK_TYPE] = g_strdup( job_titles[task->task->type] );
        if ( task->task->type == VFS_FILE_TASK_EXEC )
        {
            c->var[CONTEXT_TASK_NAME] = g_strdup( task->task->current_file );
            c->var[CONTEXT_TASK_DIR] = g_strdup( task->task->dest_dir );
        }
        else
        {
            c->var[CONTEXT_TASK_NAME] = g_strdup( "" );
            if ( task->old_src_file )
                c->var[CONTEXT_TASK_DIR] = g_path_get_dirname( task->old_src_file );
            else
                c->var[CONTEXT_TASK_DIR] = g_strdup( "" );
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

gboolean main_write_exports( VFSFileTask* vtask, char* value, FILE* file )
{
    int result, p, num_pages, i;
    char* cwd;
    char* path;
    char* esc_path;
    GList* sel_files;
    GList* l;
    PtkFileBrowser* a_browser;
    VFSVolume* vol;
    PtkFileTask* task;

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
        i = gtk_notebook_get_current_page( main_window->panel[p-1] );
        if ( i != -1 )
        {
            a_browser = (PtkFileBrowser*)
                        gtk_notebook_get_nth_page( main_window->panel[p-1], i );
        }
        else
            continue;
        if ( !a_browser || !GTK_WIDGET_VISIBLE( a_browser ) )
            continue;

        // cwd
        gboolean cwd_needs_quote;
        cwd = ptk_file_browser_get_cwd( a_browser );
        if ( cwd_needs_quote = strchr( cwd, '\'' ) )
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
                path = vfs_file_info_get_name( (VFSFileInfo*)l->data );
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
                fprintf( file, "fm_filenames=(\n", p );
                for ( l = sel_files; l; l = l->next )
                {
                    path = vfs_file_info_get_name( (VFSFileInfo*)l->data );
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
            path = ptk_bookmark_view_get_selected_dir( a_browser->side_book );
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
            vol = ptk_location_view_get_selected_vol( a_browser->side_dev );
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
                    fprintf( file, "fm_device_size=\"%d\"\n", vol->size );
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
                fprintf( file, "fm_panel%d_device_size=\"%d\"\n", p, vol->size );
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
        num_pages = gtk_notebook_get_n_pages( main_window->panel[p-1] );
        for ( i = 0; i < num_pages; i++ )
        {
            t_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                                    main_window->panel[p-1], i ) );
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
    char* this_user = g_get_user_name();
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
    if ( task = get_selected_task( file_browser->task_view ) )
    {
        fprintf( file, "\nfm_task_type='%s'\n", job_titles[task->task->type] );
        if ( task->task->type == VFS_FILE_TASK_EXEC )
        {
            esc_path = bash_quote( task->task->dest_dir );
            fprintf( file, "fm_task_pwd=%s\n", esc_path );
            g_free( esc_path );
            esc_path = bash_quote( task->task->current_file );
            fprintf( file, "fm_task_name=%s\n", esc_path );
            g_free( esc_path );
            esc_path = bash_quote( task->task->exec_command );
            fprintf( file, "fm_task_command=%s\n", esc_path );
            g_free( esc_path );
            if ( task->task->exec_as_user )
                fprintf( file, "fm_task_user='%s'\n", task->task->exec_as_user );
            if ( task->task->exec_icon )
                fprintf( file, "fm_task_icon='%s'\n", task->task->exec_icon );
            if ( task->task->exec_pid )
                fprintf( file, "fm_task_pid=%d\n", task->task->exec_pid );
        }
        else
        {
            esc_path = bash_quote( task->task->dest_dir );
            fprintf( file, "fm_task_dest_dir=%s\n", esc_path );
            g_free( esc_path );
            esc_path = bash_quote( task->task->current_item );
            fprintf( file, "fm_task_current_src_file=%s\n", esc_path );
            g_free( esc_path );
            esc_path = bash_quote( task->task->current_dest );
            fprintf( file, "fm_task_current_dest_file=%s\n", esc_path );
            g_free( esc_path );
        }
    }

    result = fputs( "\n", file );
    return result >= 0;
}

void on_task_columns_changed( GtkTreeView *view, gpointer user_data )
{
    char* title;
    char* pos;
    XSet* set = NULL;
    int i, j, width;
    GtkTreeViewColumn* col;

    gboolean fullscreen = xset_get_b( "main_full" );
    if ( !view )
        return;
    for ( i = 0; i < 13; i++ )
    {
        col = gtk_tree_view_get_column( view, i );
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
            if ( !fullscreen )
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

void on_task_destroy( GtkTreeView *view, gpointer user_data )
{
    guint id = g_signal_lookup ("columns-changed", G_TYPE_FROM_INSTANCE(view) );
    if ( id )
    {
        gulong hand = g_signal_handler_find( ( gpointer ) view, G_SIGNAL_MATCH_ID,
                                        id, NULL, NULL, NULL, NULL );
        if ( hand )
            g_signal_handler_disconnect( ( gpointer ) view, hand );
    }
    on_task_columns_changed( view, NULL ); // save widths
}

void on_task_column_selected( GtkMenuItem* item, GtkTreeView* view )
{
    on_task_columns_changed( view, NULL );    
}

gboolean main_tasks_running( FMMainWindow* main_window )
{
    GtkTreeIter it;

    if ( !main_window->task_view || !GTK_IS_TREE_VIEW( main_window->task_view ) )
        return FALSE;

    GtkTreeModel* model = gtk_tree_view_get_model( main_window->task_view );
    return gtk_tree_model_get_iter_first( model, &it );
}

void on_task_stop( GtkMenuItem* item, GtkTreeView* view, PtkFileTask* task2 )
{
    PtkFileTask* task;
    if ( item )
        task = ( PtkFileTask* ) g_object_get_data( G_OBJECT( item ),
                                                         "task" );
    else
        task = task2;

    if ( task && task->task && !task->complete )
        ptk_file_task_cancel( task );
}

void on_task_stop_all( GtkMenuItem* item, GtkTreeView* view )
{
    GtkTreeModel* model;
    GtkTreeIter it;
    PtkFileTask* task = NULL;

    model = gtk_tree_view_get_model( GTK_TREE_VIEW( view ) );
    if ( gtk_tree_model_get_iter_first( model, &it ) )
    {
        do
        {
            gtk_tree_model_get( model, &it, TASK_COL_DATA, &task, -1 );
            if ( task && task->task && !task->complete )
                ptk_file_task_cancel( task );
        }
        while ( gtk_tree_model_iter_next( model, &it ) );
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
        gtk_widget_show( main_window->task_scroll );
    else
    {
        model = gtk_tree_view_get_model( main_window->task_view );
        if ( gtk_tree_model_get_iter_first( model, &it ) )
            gtk_widget_show( main_window->task_scroll );
        else
            gtk_widget_hide( main_window->task_scroll );
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

void main_task_prepare_menu( FMMainWindow* main_window, GtkMenu* menu,
                                                GtkAccelGroup* accel_group )
{
    GSList *radio_group;
    XSet* set;
    
    GtkTreeView* parent = main_window->task_view;
    radio_group = NULL;
    set = xset_set_cb( "task_show_manager", on_task_popup_show, main_window );
        xset_set_ob1( set, "name", set->name );
        xset_set_ob2( set, NULL, radio_group );
    set = xset_set_cb( "task_hide_manager", on_task_popup_show, main_window );
        xset_set_ob1( set, "name", set->name );
        xset_set_ob2( set, NULL, radio_group );

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

    radio_group = NULL;
    set = xset_set_cb( "task_err_first", on_task_popup_errset, main_window );
        xset_set_ob1( set, "name", set->name );
        xset_set_ob2( set, NULL, radio_group );
    set = xset_set_cb( "task_err_any", on_task_popup_errset, main_window );
        xset_set_ob1( set, "name", set->name );
        xset_set_ob2( set, NULL, radio_group );
    set = xset_set_cb( "task_err_cont", on_task_popup_errset, main_window );
        xset_set_ob1( set, "name", set->name );
        xset_set_ob2( set, NULL, radio_group );
}

PtkFileTask* get_selected_task( GtkTreeView* view )
{
    GtkTreeModel* model;
    //GtkTreePath* tree_path;
    GtkTreeSelection* tree_sel;
    GtkTreeViewColumn* col = NULL;
    GtkTreeIter it;
    PtkFileTask* task = NULL;    

    if ( !view )
        return NULL;
    model = gtk_tree_view_get_model( view );
    tree_sel = gtk_tree_view_get_selection( view );
    if ( gtk_tree_selection_get_selected( tree_sel, NULL, &it ) )
    {
        gtk_tree_model_get( model, &it, TASK_COL_DATA, &task, -1 );
    }
    return task;
}

gboolean on_task_button_press_event( GtkTreeView* view, GdkEventButton *event,
                                     FMMainWindow* main_window )
{
    GtkTreeModel* model = NULL;
    GtkTreePath* tree_path;
    GtkTreeViewColumn* col = NULL;
    GtkTreeIter it;
    GtkTreeSelection* tree_sel;
    PtkFileTask* task = NULL;
    XSet* set;
    gboolean is_tasks;
    
    if ( event->type != GDK_BUTTON_PRESS )
        return FALSE;
    if ( event->button == 3 ) // right click
    {    
        // get selected task
        model = gtk_tree_view_get_model( view );
        if ( is_tasks = gtk_tree_model_get_iter_first( model, &it ) )
        {
            gtk_tree_view_get_path_at_pos( view, event->x, event->y, &tree_path,
                                                                &col, NULL, NULL );
            //tree_sel = gtk_tree_view_get_selection( view );
            if ( tree_path && gtk_tree_model_get_iter( model, &it, tree_path ) )
                gtk_tree_model_get( model, &it, TASK_COL_DATA, &task, -1 );
        }

        // build popup
        PtkFileBrowser* file_browser = 
                (PtkFileBrowser*)fm_main_window_get_current_file_browser( main_window );
        GtkMenu* popup = gtk_menu_new();
        GtkAccelGroup* accel_group = gtk_accel_group_new();
        XSetContext* context = xset_context_new();
        main_context_fill( file_browser, context );

        set = xset_set_cb( "task_stop", on_task_stop, view );
        xset_set_ob1( set, "task", task );
        set->disable = !task;

        set = xset_set_cb( "task_stop_all", on_task_stop_all, view );
        set->disable = !is_tasks;
        
        main_task_prepare_menu( main_window, popup, accel_group );

        xset_set_cb( "font_task", main_update_fonts, file_browser );
        char* menu_elements = g_strdup_printf( "task_stop sep_t3 task_stop_all sep_t4 task_show_manager task_hide_manager sep_t5 task_columns task_popups task_errors" );
        xset_add_menu( NULL, file_browser, popup, accel_group, menu_elements );
        g_free( menu_elements );
        
        gtk_widget_show_all( GTK_WIDGET( popup ) );
        g_signal_connect( popup, "selection-done",
                          G_CALLBACK( gtk_widget_destroy ), NULL );
        gtk_menu_popup( popup, NULL, NULL, NULL, NULL, event->button, event->time );
    }
    return FALSE;
}

void on_task_row_activated( GtkTreeView* view, GtkTreePath* tree_path,
                       GtkTreeViewColumn *col, gpointer user_data )
{
    GtkTreeModel* model;
    GtkTreeIter it;
    PtkFileTask* task;
    
    model = gtk_tree_view_get_model( GTK_TREE_VIEW( view ) );
    if ( !gtk_tree_model_get_iter( model, &it, tree_path ) )
        return;
    gtk_tree_model_get( model, &it, TASK_COL_DATA, &task, -1 );
    if ( task )
    {
        ptk_file_task_progress_open( task );
        if ( task->progress_dlg )
            gtk_window_present( task->progress_dlg );
    }
}

void main_task_view_remove_task( PtkFileTask* task )
{
    PtkFileTask* taskt = NULL;
    GtkTreeView* view;
    GtkTreeModel* model;
    GtkTreeIter it;

    view = task->task_view;
    if ( !view )
        return;

    model = gtk_tree_view_get_model( GTK_TREE_VIEW( view ) );
    if ( gtk_tree_model_get_iter_first( model, &it ) )
    {
        do
        {
            gtk_tree_model_get( model, &it, TASK_COL_DATA, &taskt, -1 );
        }
        while ( taskt != task && gtk_tree_model_iter_next( model, &it ) );
    }
    if ( taskt == task )
        gtk_list_store_remove( GTK_LIST_STORE( model ), &it );
    
    if ( !gtk_tree_model_get_iter_first( model, &it ) )
    {
        if ( xset_get_b( "task_hide_manager" ) )
            gtk_widget_hide( gtk_widget_get_parent( view ) );
    }
    
    // update window title
    FMMainWindow* a_window;
    GList* l;
    for ( l = all_windows; l; l = l->next )
    {
        if ( ((FMMainWindow*)l->data)->task_view == view )
        {
            update_window_title( NULL, (FMMainWindow*)l->data );
            break;
        }
    }
}

void main_task_view_update_task( PtkFileTask* task )
{
    PtkFileTask* taskt = NULL;
    GtkTreeView* view;
    GtkTreeModel* model;
    GtkTreeIter it;
    GdkPixbuf* pixbuf;
    char* status;
    char* status2 = NULL;
    char* dest_dir;
    char* path;
    char* file;
    
//printf("main_task_view_update_task  task=%d\n", task);
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

    if ( !task )
        return;
    
    view = task->task_view;
    if ( !view )
        return;
    
    if ( task->task->type != 6 )
        dest_dir = task->task->dest_dir;
    else
        dest_dir = NULL;

    model = gtk_tree_view_get_model( GTK_TREE_VIEW( view ) );
    if ( gtk_tree_model_get_iter_first( model, &it ) )
    {
        do
        {
            gtk_tree_model_get( model, &it, TASK_COL_DATA, &taskt, -1 );
        }
        while ( taskt != task && gtk_tree_model_iter_next( model, &it ) );
    }
    if ( taskt != task )
    {
        // new row
        char buf[ 64 ];
        strftime( buf, sizeof( buf ), "%H:%M", localtime( &task->task->start_time ) );
        char* started = g_strdup( buf );
        gtk_list_store_insert_with_values( GTK_LIST_STORE( model ), &it, 0,
                                    TASK_COL_TO, dest_dir,
                                    TASK_COL_STARTED, started,
                                    TASK_COL_STARTTIME, (gint64)task->task->start_time,
                                    TASK_COL_DATA, task,
                                    -1 );        
        g_free( started );
    }

    // update row
    int percent = task->old_percent;
    if ( percent < 0 )
        percent = 0;
    if ( task->task->type != 6 )
    {
        path = g_path_get_dirname( task->task->current_file );
        file = g_path_get_basename( task->task->current_file );
    }
    else
    {
        path = g_strdup( task->task->dest_dir ); //cwd
        file = g_strdup_printf( "( %s )", task->task->current_file );
        //percent = task->complete ? 100 : 50;
    }
    
    // icon
    char* iname;
    if ( task->old_err_count && task->task->type != 6 )
        iname = g_strdup_printf( "error" );
    else if ( task->task->type == 0 || task->task->type == 1 || task->task->type == 4 )
        iname = g_strdup_printf( "stock_copy" );
    else if ( task->task->type == 2 || task->task->type == 3 )
        iname = g_strdup_printf( "stock_delete" );
    else if ( task->task->type == 6 && task->task->exec_icon )
        iname = g_strdup( task->task->exec_icon );
    else
        iname = g_strdup_printf( "gtk-execute" );

    pixbuf = gtk_icon_theme_load_icon( gtk_icon_theme_get_default(), iname,
                app_settings.small_icon_size, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );
    g_free( iname );
    if ( !pixbuf )
        pixbuf = gtk_icon_theme_load_icon( gtk_icon_theme_get_default(), "gtk-execute",
                app_settings.small_icon_size, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );
    
    if ( task->task->type != 6 )
    {
        if ( !task->old_err_count )
            status = _(job_titles[ task->task->type ]);
        else
        {
            status2 = g_strdup_printf( "%d error%s %s", task->old_err_count,
                   task->old_err_count > 1 ? "s" : "", _(job_titles[ task->task->type ]) );
            status = status2;
        }
    }
    else
    {
        // exec task
        if ( task->task->exec_action )
            status = task->task->exec_action;
        else
            status = _(job_titles[ task->task->type ]);
    }
    
    gtk_list_store_set( GTK_LIST_STORE( model ), &it,
                        TASK_COL_ICON, pixbuf,
                        TASK_COL_STATUS, status,
                        TASK_COL_COUNT, task->dsp_file_count,
                        TASK_COL_PATH, path,
                        TASK_COL_FILE, file,
                        TASK_COL_PROGRESS, percent,
                        TASK_COL_TOTAL, task->dsp_size_tally,
                        TASK_COL_ELAPSED, task->dsp_elapsed,
                        TASK_COL_CURSPEED, task->dsp_curspeed,
                        TASK_COL_CUREST, task->dsp_curest,
                        TASK_COL_AVGSPEED, task->dsp_avgspeed,
                        TASK_COL_AVGEST, task->dsp_avgest,
                        -1 );
    g_free( file );
    g_free( path );
    if ( status2 )
        g_free( status2 );
    if ( !gtk_widget_get_visible( gtk_widget_get_parent( view ) ) )
        gtk_widget_show( gtk_widget_get_parent( view ) );

    // update window title
    FMMainWindow* a_window;
    GList* l;
    for ( l = all_windows; l; l = l->next )
    {
        if ( ((FMMainWindow*)l->data)->task_view == view )
        {
            update_window_title( NULL, (FMMainWindow*)l->data );
            break;
        }
    }
//printf("DONE main_task_view_update_task\n");
}

GtkTreeView* main_task_view_new( FMMainWindow* main_window )
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
    GtkTreeView* view = exo_tree_view_new();
    gtk_tree_view_set_model( GTK_TREE_VIEW( view ), list );
    exo_tree_view_set_single_click( (ExoTreeView*)view, TRUE );
    //exo_tree_view_set_single_click_timeout( (ExoTreeView*)view, 400 );

    // Columns
    for ( i = 0; i < 13; i++ )
    {
        col = gtk_tree_view_column_new();
        gtk_tree_view_column_set_resizable ( col, TRUE );
        
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
            width = xset_get_int( task_names[j], "y" );
            if ( width )
            {
                gtk_tree_view_column_set_sizing( col, GTK_TREE_VIEW_COLUMN_FIXED );
                gtk_tree_view_column_set_min_width( col, 20 );
                gtk_tree_view_column_set_fixed_width ( col, width );
            }
        }
        
        if ( cols[j] == TASK_COL_STATUS )
        {
            // Icon and Text
            renderer = gtk_cell_renderer_text_new();
            //gtk_object_set( GTK_OBJECT( renderer ),
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
        }
        gtk_tree_view_append_column ( view, col );
        gtk_tree_view_column_set_title( col, _( task_titles[j] ) );
        gtk_tree_view_column_set_reorderable( col, TRUE );        
        gtk_tree_view_column_set_visible( col, xset_get_b( task_names[j] ) );
        if ( j == TASK_COL_FILE ) //|| j == TASK_COL_PATH || j == TASK_COL_TO
        {
            gtk_tree_view_column_set_sizing( col, GTK_TREE_VIEW_COLUMN_FIXED );
            gtk_tree_view_column_set_min_width( col, 20 );
            gtk_tree_view_column_set_expand ( col, TRUE );
        }
    }
    
    // invisible Starttime col for sorting
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable ( col, TRUE );
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start( col, renderer, TRUE );
    gtk_tree_view_column_set_attributes( col, renderer,
                                         "text", TASK_COL_STARTTIME, NULL );
    gtk_tree_view_append_column ( view, col );
    gtk_tree_view_column_set_title( col, "StartTime" );
    gtk_tree_view_column_set_reorderable( col, FALSE );        
    gtk_tree_view_column_set_visible( col, FALSE );
    
    // Sort
    if ( GTK_IS_TREE_SORTABLE( list ) )
        gtk_tree_sortable_set_sort_column_id( GTK_TREE_SORTABLE( list ),
                                TASK_COL_STARTTIME, GTK_SORT_ASCENDING );

    gtk_tree_view_set_rules_hint ( view, TRUE );
    
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


//================================================================================

/*
// Utilities
static void fm_main_window_update_command_ui( FMMainWindow* main_window,
                                              PtkFileBrowser* file_browser );
*/
/* Automatically process busy cursor
static void fm_main_window_start_busy_task( FMMainWindow* main_window );
static gboolean fm_main_window_stop_busy_task( FMMainWindow* main_window );
*/

/*
static void on_view_menu_popup( GtkMenuItem* item,
                                FMMainWindow* main_window );


// Callback called when the bookmarks change
static void on_bookmarks_change( PtkBookmarks* bookmarks,
                                 FMMainWindow* main_window );
*/

/*
static gboolean fm_main_window_edit_address ( FMMainWindow* widget );
*/

/*
static GtkWidget* create_bookmarks_menu ( FMMainWindow* main_window );
static GtkWidget* create_bookmarks_menu_item ( FMMainWindow* main_window,
                                               const char* item );
*/

/*
static void on_rename_activate ( GtkMenuItem *menuitem,
                                 gpointer user_data );
#if 0
static void on_fullscreen_activate ( GtkMenuItem *menuitem,
                                     gpointer user_data );
#endif
static void on_location_bar_activate ( GtkMenuItem *menuitem,
                                        gpointer user_data );
static void on_show_hidden_activate ( GtkMenuItem *menuitem,
                                      gpointer user_data );
static void on_sort_by_name_activate ( GtkMenuItem *menuitem,
                                       gpointer user_data );
static void on_sort_by_size_activate ( GtkMenuItem *menuitem,
                                       gpointer user_data );
static void on_sort_by_mtime_activate ( GtkMenuItem *menuitem,
                                        gpointer user_data );
static void on_sort_by_type_activate ( GtkMenuItem *menuitem,
                                       gpointer user_data );
static void on_sort_by_perm_activate ( GtkMenuItem *menuitem,
                                       gpointer user_data );
static void on_sort_by_owner_activate ( GtkMenuItem *menuitem,
                                        gpointer user_data );
static void on_sort_ascending_activate ( GtkMenuItem *menuitem,
                                         gpointer user_data );
static void on_sort_descending_activate ( GtkMenuItem *menuitem,
                                          gpointer user_data );
static void on_view_as_icon_activate ( GtkMenuItem *menuitem,
                                       gpointer user_data );
static void on_view_as_list_activate ( GtkMenuItem *menuitem,
                                       gpointer user_data );
static void on_view_as_compact_list_activate ( GtkMenuItem *menuitem,
                                       gpointer user_data );
static void on_open_side_pane_activate ( GtkMenuItem *menuitem,
                                         gpointer user_data );
static void on_show_dir_tree ( GtkMenuItem *menuitem,
                               gpointer user_data );
static void on_show_loation_pane ( GtkMenuItem *menuitem,
                                   gpointer user_data );
static void on_side_pane_toggled ( GtkToggleToolButton *toggletoolbutton,
                                   gpointer user_data );
static void on_go_btn_clicked ( GtkToolButton *toolbutton,
                                gpointer user_data );
static void on_open_terminal_activate ( GtkMenuItem *menuitem,
                                        gpointer user_data );
static void on_open_files_activate ( GtkMenuItem *menuitem, gpointer user_data );  //MOD added

static void on_run_command ( GtkMenuItem *menuitem,
                                        gpointer user_data );  //MOD
static void on_open_current_folder_as_root ( GtkMenuItem *menuitem,
                                             gpointer user_data );
static void on_find_file_activate ( GtkMenuItem *menuitem,
                                        gpointer user_data );
static void on_file_properties_activate ( GtkMenuItem *menuitem,
                                          gpointer user_data );
static void on_bookmark_item_activate ( GtkMenuItem* menu, gpointer user_data );

*/

/*
static void on_cut_activate ( GtkMenuItem *menuitem,
                              gpointer user_data );
static void on_copy_activate ( GtkMenuItem *menuitem,
                               gpointer user_data );
static void on_paste_activate ( GtkMenuItem *menuitem,
                                gpointer user_data );
static void on_paste_link_activate ( GtkMenuItem *menuitem,
                                gpointer user_data );  //MOD added
static void on_paste_target_activate ( GtkMenuItem *menuitem,
                                gpointer user_data );  //MOD added
static void on_copy_text_activate ( GtkMenuItem *menuitem,
                                gpointer user_data );  //MOD added
static void on_copy_name_activate ( GtkMenuItem *menuitem,
                                gpointer user_data );  //MOD added
static void on_hide_file_activate ( GtkMenuItem *menuitem,
                                gpointer user_data );  //MOD added
static void on_delete_activate ( GtkMenuItem *menuitem,
                                 gpointer user_data );
static void on_select_all_activate ( GtkMenuItem *menuitem,
                                     gpointer user_data );
static void on_add_to_bookmark_activate ( GtkMenuItem *menuitem,
                                          gpointer user_data );
static void on_invert_selection_activate ( GtkMenuItem *menuitem,
                                           gpointer user_data );
*/

/*
// Signal handlers 
static void on_back_activate ( GtkToolButton *toolbutton,
                               FMMainWindow *main_window );
static void on_forward_activate ( GtkToolButton *toolbutton,
                                  FMMainWindow *main_window );
static void on_up_activate ( GtkToolButton *toolbutton,
                             FMMainWindow *main_window );
static void on_home_activate( GtkToolButton *toolbutton,
                              FMMainWindow *main_window );
static void on_refresh_activate ( GtkToolButton *toolbutton,
                                  gpointer user_data );
static void on_quit_activate ( GtkMenuItem *menuitem,
                               gpointer user_data );
static void on_new_folder_activate ( GtkMenuItem *menuitem,
                                     gpointer user_data );
static void on_new_text_file_activate ( GtkMenuItem *menuitem,
                                        gpointer user_data );
static void on_preference_activate ( GtkMenuItem *menuitem,
                                     gpointer user_data );
#if 0
static void on_file_assoc_activate ( GtkMenuItem *menuitem,
                                     gpointer user_data );
#endif
static void on_about_activate ( GtkMenuItem *menuitem,
                                gpointer user_data );
#if 0
static gboolean on_back_btn_popup_menu ( GtkWidget *widget,
                                         gpointer user_data );
static gboolean on_forward_btn_popup_menu ( GtkWidget *widget,
                                            gpointer user_data );
#endif
static void on_address_bar_activate ( GtkWidget *entry,
                                      gpointer user_data );
static void on_new_window_activate ( GtkMenuItem *menuitem,
                                     gpointer user_data );
static void on_new_tab_activate ( GtkMenuItem *menuitem,
                                  gpointer user_data );

*/

/*
// Main menu definition

static PtkMenuItemEntry fm_file_create_new_manu[] =
    {
        PTK_IMG_MENU_ITEM( N_( "_Folder" ), "gtk-directory", on_new_folder_activate, GDK_f, GDK_CONTROL_MASK ),  //MOD stole ctrl-f
        PTK_IMG_MENU_ITEM( N_( "_Text File" ), "gtk-edit", on_new_text_file_activate, GDK_f, GDK_CONTROL_MASK | GDK_SHIFT_MASK ),  //MOD added ctrl-shift-f
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_file_menu[] =
    {
        PTK_IMG_MENU_ITEM( N_( "New _Window" ), "gtk-add", on_new_window_activate, GDK_N, GDK_CONTROL_MASK ),
        PTK_IMG_MENU_ITEM( N_( "New _Tab" ), "gtk-add", on_new_tab_activate, GDK_T, GDK_CONTROL_MASK ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_POPUP_IMG_MENU( N_( "_Create New" ), "gtk-new", fm_file_create_new_manu ),
        PTK_IMG_MENU_ITEM( N_( "_Hide File" ), "gtk-edit", on_hide_file_activate, 0, 0 ), //MOD added
        PTK_IMG_MENU_ITEM( N_( "File _Properties" ), "gtk-info", on_file_properties_activate, GDK_Return, GDK_MOD1_MASK ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_IMG_MENU_ITEM( N_( "Close Tab" ), "gtk-close", on_close_tab_activate, GDK_W, GDK_CONTROL_MASK ),
        PTK_IMG_MENU_ITEM( N_("_Close Window"), "gtk-quit", on_quit_activate, GDK_Q, GDK_CONTROL_MASK ),
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_edit_menu[] =
    {
        PTK_STOCK_MENU_ITEM( "gtk-cut", on_cut_activate ),
        PTK_STOCK_MENU_ITEM( "gtk-copy", on_copy_activate ),
        PTK_IMG_MENU_ITEM( N_( "Copy as Te_xt" ), GTK_STOCK_COPY, on_copy_text_activate, GDK_C, GDK_CONTROL_MASK | GDK_SHIFT_MASK ),   //MOD added
        PTK_IMG_MENU_ITEM( N_( "Copy _Name" ), GTK_STOCK_COPY, on_copy_name_activate, GDK_C, GDK_MOD1_MASK | GDK_SHIFT_MASK ),   //MOD added
        PTK_STOCK_MENU_ITEM( "gtk-paste", on_paste_activate ),
        PTK_IMG_MENU_ITEM( N_( "Paste as _Link" ), GTK_STOCK_PASTE, on_paste_link_activate, GDK_V, GDK_CONTROL_MASK | GDK_SHIFT_MASK ),   //MOD added
        PTK_IMG_MENU_ITEM( N_( "Paste as Tar_get" ), GTK_STOCK_PASTE, on_paste_target_activate, GDK_V, GDK_MOD1_MASK | GDK_SHIFT_MASK ),   //MOD added
        PTK_IMG_MENU_ITEM( N_( "_Delete" ), "gtk-delete", on_delete_activate, GDK_Delete, 0 ),
        PTK_IMG_MENU_ITEM( N_( "_Rename" ), "gtk-edit", on_rename_activate, GDK_F2, 0 ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_MENU_ITEM( N_( "Select _All" ), on_select_all_activate, GDK_A, GDK_CONTROL_MASK ),
        PTK_MENU_ITEM( N_( "_Invert Selection" ), on_invert_selection_activate, GDK_I, GDK_CONTROL_MASK ),
        PTK_SEPARATOR_MENU_ITEM,
        // PTK_IMG_MENU_ITEM( N_( "_File Associations" ), "gtk-execute", on_file_assoc_activate , 0, 0 ),
        PTK_STOCK_MENU_ITEM( "gtk-preferences", on_preference_activate ),
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_go_menu[] =
    {
        PTK_IMG_MENU_ITEM( N_( "Go _Back" ), "gtk-go-back", on_back_activate, GDK_Left, GDK_MOD1_MASK ),
        PTK_IMG_MENU_ITEM( N_( "Go _Forward" ), "gtk-go-forward", on_forward_activate, GDK_Right, GDK_MOD1_MASK ),
        PTK_IMG_MENU_ITEM( N_( "Go to _Parent Folder" ), "gtk-go-up", on_up_activate, GDK_Up, GDK_MOD1_MASK ),
        PTK_IMG_MENU_ITEM( N_( "Go _Home" ), "gtk-home", on_home_activate, GDK_Escape, 0 ),  //MOD  was   GDK_Home, GDK_MOD1_MASK ),
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_help_menu[] =
    {
        PTK_STOCK_MENU_ITEM( "gtk-about", on_about_activate ),
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_sort_menu[] =
    {
        PTK_RADIO_MENU_ITEM( N_( "Sort by _Name" ), on_sort_by_name_activate, 0, 0 ),
        PTK_RADIO_MENU_ITEM( N_( "Sort by _Size" ), on_sort_by_size_activate, 0, 0 ),
        PTK_RADIO_MENU_ITEM( N_( "Sort by _Modification Time" ), on_sort_by_mtime_activate, 0, 0 ),
        PTK_RADIO_MENU_ITEM( N_( "Sort by _Type" ), on_sort_by_type_activate, 0, 0 ),
        PTK_RADIO_MENU_ITEM( N_( "Sort by _Permission" ), on_sort_by_perm_activate, 0, 0 ),
        PTK_RADIO_MENU_ITEM( N_( "Sort by _Owner" ), on_sort_by_owner_activate, 0, 0 ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_RADIO_MENU_ITEM( N_( "Ascending" ), on_sort_ascending_activate, 0, 0 ),
        PTK_RADIO_MENU_ITEM( N_( "Descending" ), on_sort_descending_activate, 0, 0 ),
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_side_pane_menu[] =
    {
        PTK_CHECK_MENU_ITEM( N_( "_Open Side Pane" ), on_open_side_pane_activate, 0, 0 ), //MOD was GDK_F9
        PTK_SEPARATOR_MENU_ITEM,
        PTK_RADIO_MENU_ITEM( N_( "Show _Location Pane" ), on_show_loation_pane, GDK_B, GDK_CONTROL_MASK ),
        PTK_RADIO_MENU_ITEM( N_( "Show _Directory Tree" ), on_show_dir_tree, GDK_D, GDK_CONTROL_MASK ),
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_view_menu[] =
    {
        PTK_IMG_MENU_ITEM( N_( "_Refresh" ), "gtk-refresh", on_refresh_activate, GDK_F5, 0 ),
        PTK_POPUP_MENU( N_( "Side _Pane" ), fm_side_pane_menu ),
        // PTK_CHECK_MENU_ITEM( N_( "_Full Screen" ), on_fullscreen_activate, GDK_F11, 0 ), 
        PTK_CHECK_MENU_ITEM( N_( "L_ocation Bar" ), on_location_bar_activate, 0, 0 ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_CHECK_MENU_ITEM( N_( "Show _Hidden Files" ), on_show_hidden_activate, GDK_H, GDK_CONTROL_MASK ),
        PTK_POPUP_MENU( N_( "_Sort..." ), fm_sort_menu ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_RADIO_MENU_ITEM( N_( "View as _Icons" ), on_view_as_icon_activate, 0, 0 ),
        PTK_RADIO_MENU_ITEM( N_( "View as _Compact List" ), on_view_as_compact_list_activate, 0, 0 ),
        PTK_RADIO_MENU_ITEM( N_( "View as Detailed _List" ), on_view_as_list_activate, 0, 0 ),
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_tool_menu[] =
    {
        PTK_IMG_MENU_ITEM( N_( "Open _Terminal" ), GTK_STOCK_EXECUTE, on_open_terminal_activate, GDK_S, GDK_CONTROL_MASK ),   //MOD
        PTK_IMG_MENU_ITEM( N_( "Open Current Folder as _Root" ),
                           GTK_STOCK_DIALOG_AUTHENTICATION,
                           on_open_current_folder_as_root, 0, 0 ),
        PTK_IMG_MENU_ITEM( N_( "_Find Files" ), GTK_STOCK_FIND, on_find_file_activate, GDK_F3, 0 ),
        PTK_IMG_MENU_ITEM( N_( "Run Command..." ), GTK_STOCK_EXECUTE, on_run_command, GDK_r, GDK_CONTROL_MASK ),   //MOD added
        PTK_SEPARATOR_MENU_ITEM,
        PTK_IMG_MENU_ITEM( N_( "User Open Files" ), GTK_STOCK_EXECUTE, on_open_files_activate, GDK_F4, 0 ),   //MOD renamed
        PTK_IMG_MENU_ITEM( N_( "User Command 6" ), GTK_STOCK_EXECUTE, on_user_command_6, GDK_F6, 0 ), //MOD
        PTK_IMG_MENU_ITEM( N_( "User Command 7" ), GTK_STOCK_EXECUTE, on_user_command_7, GDK_F7, 0 ), //MOD
        PTK_IMG_MENU_ITEM( N_( "User Command 8" ), GTK_STOCK_EXECUTE, on_user_command_8, GDK_F8, 0 ), //MOD
        PTK_IMG_MENU_ITEM( N_( "User Command 9" ), GTK_STOCK_EXECUTE, on_user_command_9, GDK_F9, 0 ), //MOD
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_menu_bar[] =
    {
        PTK_POPUP_MENU( N_( "_File" ), fm_file_menu ),
        PTK_MENU_ITEM( N_( "_Edit" ), NULL, 0, 0 ),
        // PTK_POPUP_MENU( N_( "_Edit" ), fm_edit_menu ),
        PTK_POPUP_MENU( N_( "_Go" ), fm_go_menu ),
        PTK_MENU_ITEM( N_( "_Bookmark" ), NULL, 0, 0 ),
        PTK_POPUP_MENU( N_( "_View" ), fm_view_menu ),
        PTK_POPUP_MENU( N_( "_Tool" ), fm_tool_menu ),
        PTK_POPUP_MENU( N_( "_Help" ), fm_help_menu ),
        PTK_MENU_END
    };

// Toolbar items defiinition

static PtkToolItemEntry fm_toolbar_btns[] =
    {
        PTK_TOOL_ITEM( N_( "New Tab" ), "gtk-add", N_( "New Tab" ), on_new_tab_activate ),
        PTK_MENU_TOOL_ITEM( N_( "Back" ), "gtk-go-back", N_( "Back" ), G_CALLBACK(on_back_activate), PTK_EMPTY_MENU ),
        PTK_MENU_TOOL_ITEM( N_( "Forward" ), "gtk-go-forward", N_( "Forward" ), G_CALLBACK(on_forward_activate), PTK_EMPTY_MENU ),
        PTK_TOOL_ITEM( N_( "Parent Folder" ), "gtk-go-up", N_( "Parent Folder" ), on_up_activate ),
        PTK_TOOL_ITEM( N_( "Refresh" ), "gtk-refresh", N_( "Refresh" ), on_refresh_activate ),
        PTK_TOOL_ITEM( N_( "Home Directory" ), "gtk-home", N_( "Home Directory" ), on_home_activate ),
        PTK_CHECK_TOOL_ITEM( N_( "Open Side Pane" ), "gtk-open", N_( "Open Side Pane" ), on_side_pane_toggled ),
        PTK_TOOL_END
    };

static PtkToolItemEntry fm_toolbar_jump_btn[] =
    {
        PTK_TOOL_ITEM( NULL, "gtk-jump-to", N_( "Go" ), on_go_btn_clicked ),
        PTK_TOOL_END
    };
*/


/*
static void on_history_menu_item_activate( GtkWidget* menu_item, FMMainWindow* main_window )
{
    GList* l = (GList*)g_object_get_data( G_OBJECT(menu_item), "path"), *tmp;
    PtkFileBrowser* fb = (PtkFileBrowser*)fm_main_window_get_current_file_browser(main_window);
    tmp = fb->curHistory;
    fb->curHistory = l;

    if( !  ptk_file_browser_chdir( fb, (char*)l->data, PTK_FB_CHDIR_NO_HISTORY ) )
        fb->curHistory = tmp;
    else
    {
        //MOD sync curhistsel
        gint* elementn = -1;
        elementn = g_list_position( fb->history, fb->curHistory );
        if ( elementn != -1 )
            fb->curhistsel = g_list_nth( fb->histsel, elementn );
        else
            g_debug("missing history item - main-window.c");
    }
}

static void remove_all_menu_items( GtkWidget* menu, gpointer user_data )
{
    gtk_container_foreach( (GtkContainer*)menu, (GtkCallback)gtk_widget_destroy, NULL );
}

static GtkWidget* add_history_menu_item( FMMainWindow* main_window, GtkWidget* menu, GList* l )
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
                      G_CALLBACK( on_history_menu_item_activate ), main_window );

    gtk_menu_shell_append( GTK_MENU_SHELL(menu), menu_item );
    return menu_item;
}

static void on_show_history_menu( GtkMenuToolButton* btn, FMMainWindow* main_window )
{
    GtkMenuShell* menu = (GtkMenuShell*)gtk_menu_tool_button_get_menu(btn);
    GList *l;
    PtkFileBrowser* fb = (PtkFileBrowser*)fm_main_window_get_current_file_browser(main_window);

    if( btn == (GtkMenuToolButton *) main_window->back_btn )  // back history
    {
        for( l = fb->curHistory->prev; l != NULL; l = l->prev )
            add_history_menu_item( main_window, GTK_WIDGET(menu), l );
    }
    else    // forward history
    {
        for( l = fb->curHistory->next; l != NULL; l = l->next )
            add_history_menu_item( main_window, GTK_WIDGET(menu), l );
    }
    gtk_widget_show_all( GTK_WIDGET(menu) );
}
*/

/*
static gboolean
fm_main_window_edit_address ( FMMainWindow* main_window )
{
    if ( GTK_WIDGET_VISIBLE( main_window->toolbar ) )
        gtk_widget_grab_focus( GTK_WIDGET( main_window->address_bar ) );
    else
        fm_go( main_window );
    return TRUE;
}

static void
fm_main_window_next_tab ( FMMainWindow* widget )
{
    if ( gtk_notebook_get_current_page( widget->notebook ) + 1 ==
      gtk_notebook_get_n_pages( widget->notebook ) )
    {
        gtk_notebook_set_current_page( widget->notebook, 0 );
    }
    else
    {
        gtk_notebook_next_page( widget->notebook );
    }
}

static void
fm_main_window_prev_tab ( FMMainWindow* widget )
{
    if ( gtk_notebook_get_current_page( widget->notebook ) == 0 )
    {
        gtk_notebook_set_current_page( widget->notebook, 
          gtk_notebook_get_n_pages( widget->notebook ) - 1 );
    }
    else
    {
        gtk_notebook_prev_page( widget->notebook );
    }
}
*/

/*
void on_back_activate( GtkToolButton *toolbutton, FMMainWindow *main_window )
{
    PtkFileBrowser * file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    if ( file_browser && ptk_file_browser_can_back( file_browser ) )
        ptk_file_browser_go_back( file_browser );
}


void on_forward_activate( GtkToolButton *toolbutton, FMMainWindow *main_window )
{
    PtkFileBrowser * file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    if ( file_browser && ptk_file_browser_can_forward( file_browser ) )
        ptk_file_browser_go_forward( file_browser );
}


void on_up_activate( GtkToolButton *toolbutton, FMMainWindow *main_window )
{
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    ptk_file_browser_go_up( file_browser );
}


void on_home_activate( GtkToolButton *toolbutton, FMMainWindow *main_window )
{
    GtkWidget * file_browser = fm_main_window_get_current_file_browser( main_window );
    if ( app_settings.home_folder )
        ptk_file_browser_chdir( PTK_FILE_BROWSER( file_browser ), app_settings.home_folder, PTK_FB_CHDIR_ADD_HISTORY );  //MOD
    else
        ptk_file_browser_chdir( PTK_FILE_BROWSER( file_browser ), g_get_home_dir(), PTK_FB_CHDIR_ADD_HISTORY );
}


void on_address_bar_activate ( GtkWidget *entry,
                               gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    gchar *dir_path, *final_path;
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    gtk_editable_select_region( (GtkEditable*)entry, 0, 0 );    // clear selection 
    // Convert to on-disk encoding
    dir_path = g_filename_from_utf8( gtk_entry_get_text( GTK_ENTRY( entry ) ), -1,
                                     NULL, NULL, NULL );
    final_path = vfs_file_resolve_path( ptk_file_browser_get_cwd(file_browser), dir_path );
    g_free( dir_path );
    ptk_file_browser_chdir( file_browser, final_path, PTK_FB_CHDIR_ADD_HISTORY );
    g_free( final_path );
    gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );
}

#if 0
// This is not needed by a file manager 
// Patched by <cantona@cantona.net> 
void on_fullscreen_activate ( GtkMenuItem *menuitem,
                              gpointer user_data )
{
    GtkWindow * window;
    int is_fullscreen;

    window = GTK_WINDOW( user_data );
    is_fullscreen = gtk_check_menu_item_get_active( GTK_CHECK_MENU_ITEM( menuitem ) );

    if ( is_fullscreen )
    {
        gtk_window_fullscreen( GTK_WINDOW( window ) );
    }
    else
    {
        gtk_window_unfullscreen( GTK_WINDOW( window ) );
    }
}
#endif

void on_refresh_activate ( GtkToolButton *toolbutton,
                           gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_refresh( PTK_FILE_BROWSER( file_browser ) );
}


void
on_quit_activate ( GtkMenuItem *menuitem,
                   gpointer user_data )
{
    fm_main_window_close( GTK_WIDGET( user_data ) );
}


void
on_new_folder_activate ( GtkMenuItem *menuitem,
                         gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_create_new_file( PTK_FILE_BROWSER( file_browser ), TRUE );
}


void
on_new_text_file_activate ( GtkMenuItem *menuitem,
                            gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_create_new_file( PTK_FILE_BROWSER( file_browser ), FALSE );
}
*/

/*
void
on_new_tab_activate ( GtkMenuItem *menuitem,
                      gpointer user_data )
{
    // FIXME: There sould be an option to let the users choose wether
    // home dir or current dir should be opened in new windows and new tabs.
    
    PtkFileBrowser * file_browser;
    const char* path;
    gboolean show_side_pane;
    PtkFBSidePaneMode side_pane_mode;

    FMMainWindow* main_window = FM_MAIN_WINDOW( user_data );
    
    file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );

    if ( app_settings.home_folder )  //MOD
        path = app_settings.home_folder;
    else
        path = g_get_home_dir();
    if ( file_browser )
    {
        //path = ptk_file_browser_get_cwd( file_browser );
        show_side_pane = ptk_file_browser_is_side_pane_visible( file_browser );
        side_pane_mode = ptk_file_browser_get_side_pane_mode( file_browser );
    }
    else
    {
        show_side_pane = app_settings.show_side_pane;
        side_pane_mode = app_settings.side_pane_mode;
    }

    fm_main_window_add_new_tab( main_window, path,
                                show_side_pane, side_pane_mode );
}
*/

/*
void
on_cut_activate ( GtkMenuItem *menuitem,
                  gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser;
    file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_cut( PTK_FILE_BROWSER( file_browser ) );
}

void
on_copy_activate ( GtkMenuItem *menuitem,
                   gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser;
    file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_copy( PTK_FILE_BROWSER( file_browser ) );
}


void
on_paste_activate ( GtkMenuItem *menuitem,
                    gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser;
    file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_paste( PTK_FILE_BROWSER( file_browser ) );
}

on_paste_link_activate ( GtkMenuItem *menuitem,
                    gpointer user_data )   //MOD added
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser;
    file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_paste_link( PTK_FILE_BROWSER( file_browser ) );
}


on_paste_target_activate ( GtkMenuItem *menuitem,
                    gpointer user_data )   //MOD added
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser;
    file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_paste_target( PTK_FILE_BROWSER( file_browser ) );
}


on_copy_text_activate ( GtkMenuItem *menuitem,
                    gpointer user_data )   //MOD added
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser;
    file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_copy_text( PTK_FILE_BROWSER( file_browser ) );
}

on_copy_name_activate ( GtkMenuItem *menuitem,
                    gpointer user_data )   //MOD added
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser;
    file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_copy_name( PTK_FILE_BROWSER( file_browser ) );
}

on_hide_file_activate ( GtkMenuItem *menuitem,
                    gpointer user_data )   //MOD added
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser;
    file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_hide_selected( PTK_FILE_BROWSER( file_browser ) );
    // refresh sometimes segfaults so disabled
    //ptk_file_browser_refresh( PTK_FILE_BROWSER( file_browser ) );
}

void
on_delete_activate ( GtkMenuItem *menuitem,
                     gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser;

    file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_delete( PTK_FILE_BROWSER( file_browser ) );
}


void
on_select_all_activate ( GtkMenuItem *menuitem,
                         gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_select_all( PTK_FILE_BROWSER( file_browser ) );
}

void
on_edit_bookmark_activate ( GtkMenuItem *menuitem,
                            gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );

    edit_bookmarks( GTK_WINDOW( main_window ) );
}

int bookmark_item_comp( const char* item, const char* path )
{
    return strcmp( ptk_bookmarks_item_get_path( item ), path );
}

void
on_add_to_bookmark_activate ( GtkMenuItem *menuitem,
                              gpointer user_data )
{
    FMMainWindow* main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
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
}

void
on_invert_selection_activate ( GtkMenuItem *menuitem,
                               gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_invert_selection( PTK_FILE_BROWSER( file_browser ) );
}


void
on_close_tab_activate ( GtkMenuItem *menuitem,
                        gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkNotebook* notebook = main_window->notebook;
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    gint idx;

    if ( gtk_notebook_get_n_pages ( notebook ) <= 1 )
    {
        fm_main_window_close( GTK_WIDGET( main_window ) );
        return ;
    }
    idx = gtk_notebook_page_num ( GTK_NOTEBOOK( notebook ),
                                  file_browser );
    gtk_notebook_remove_page( notebook, idx );
    if ( !app_settings.always_show_tabs )
        if ( gtk_notebook_get_n_pages ( notebook ) == 1 )
            gtk_notebook_set_show_tabs( notebook, FALSE );
}


void
on_rename_activate ( GtkMenuItem *menuitem,
                     gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_rename_selected_file( PTK_FILE_BROWSER( file_browser ) );
}


void
on_open_side_pane_activate ( GtkMenuItem *menuitem,
                             gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkCheckMenuItem* check = GTK_CHECK_MENU_ITEM( menuitem );
    PtkFileBrowser* file_browser;
    GtkNotebook* nb = main_window->notebook;
    GtkToggleToolButton* btn = main_window->open_side_pane_btn;
    int i;
    int n = gtk_notebook_get_n_pages( nb );

    app_settings.show_side_pane = gtk_check_menu_item_get_active( check );
    g_signal_handlers_block_matched ( btn, G_SIGNAL_MATCH_FUNC,
                                      0, 0, NULL, on_side_pane_toggled, NULL );
    gtk_toggle_tool_button_set_active( btn, app_settings.show_side_pane );
    g_signal_handlers_unblock_matched ( btn, G_SIGNAL_MATCH_FUNC,
                                        0, 0, NULL, on_side_pane_toggled, NULL );

    for ( i = 0; i < n; ++i )
    {
        file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page( nb, i ) );
        if ( app_settings.show_side_pane )
        {
            ptk_file_browser_show_side_pane( file_browser,
                                             file_browser->side_pane_mode );
        }
        else
        {
            ptk_file_browser_hide_side_pane( file_browser );
        }
    }
}


void on_show_dir_tree ( GtkMenuItem *menuitem, gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser;
    int i, n;
    if ( ! GTK_CHECK_MENU_ITEM( menuitem ) ->active )
        return ;
    app_settings.side_pane_mode = PTK_FB_SIDE_PANE_DIR_TREE;

    n = gtk_notebook_get_n_pages( main_window->notebook );
    for ( i = 0; i < n; ++i )
    {
        file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                             main_window->notebook, i ) );
        ptk_file_browser_set_side_pane_mode( file_browser, PTK_FB_SIDE_PANE_DIR_TREE );
    }
}

void on_show_loation_pane ( GtkMenuItem *menuitem, gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser;
    int i, n;
    if ( ! GTK_CHECK_MENU_ITEM( menuitem ) ->active )
        return ;
    app_settings.side_pane_mode = PTK_FB_SIDE_PANE_BOOKMARKS;
    n = gtk_notebook_get_n_pages( main_window->notebook );
    for ( i = 0; i < n; ++i )
    {
        file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                             main_window->notebook, i ) );
        ptk_file_browser_set_side_pane_mode( file_browser, PTK_FB_SIDE_PANE_BOOKMARKS );
    }
}

void
on_location_bar_activate ( GtkMenuItem *menuitem,
                           gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    app_settings.show_location_bar = gtk_check_menu_item_get_active( GTK_CHECK_MENU_ITEM( menuitem ) );
    if ( app_settings.show_location_bar )
        gtk_widget_show( main_window->toolbar );
    else
        gtk_widget_hide( main_window->toolbar );
}

void
on_show_hidden_activate ( GtkMenuItem *menuitem,
                          gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    app_settings.show_hidden_files = gtk_check_menu_item_get_active( GTK_CHECK_MENU_ITEM( menuitem ) );
    if ( file_browser )
        ptk_file_browser_show_hidden_files( file_browser,
                                            app_settings.show_hidden_files );
}


void
on_sort_by_name_activate ( GtkMenuItem *menuitem,
                           gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    app_settings.sort_order = PTK_FB_SORT_BY_NAME;
    if ( file_browser )
        ptk_file_browser_set_sort_order( PTK_FILE_BROWSER( file_browser ), app_settings.sort_order );
}


void
on_sort_by_size_activate ( GtkMenuItem *menuitem,
                           gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    app_settings.sort_order = PTK_FB_SORT_BY_SIZE;
    if ( file_browser )
        ptk_file_browser_set_sort_order( PTK_FILE_BROWSER( file_browser ), app_settings.sort_order );
}


void
on_sort_by_mtime_activate ( GtkMenuItem *menuitem,
                            gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    app_settings.sort_order = PTK_FB_SORT_BY_MTIME;
    if ( file_browser )
        ptk_file_browser_set_sort_order( PTK_FILE_BROWSER( file_browser ), app_settings.sort_order );
}

void on_sort_by_type_activate ( GtkMenuItem *menuitem,
                                gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    app_settings.sort_order = PTK_FB_SORT_BY_TYPE;
    if ( file_browser )
        ptk_file_browser_set_sort_order( PTK_FILE_BROWSER( file_browser ), app_settings.sort_order );
}

void on_sort_by_perm_activate ( GtkMenuItem *menuitem,
                                gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    app_settings.sort_order = PTK_FB_SORT_BY_PERM;
    if ( file_browser )
        ptk_file_browser_set_sort_order( PTK_FILE_BROWSER( file_browser ), app_settings.sort_order );
}

void on_sort_by_owner_activate ( GtkMenuItem *menuitem,
                                 gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    app_settings.sort_order = PTK_FB_SORT_BY_OWNER;
    if ( file_browser )
        ptk_file_browser_set_sort_order( PTK_FILE_BROWSER( file_browser ), app_settings.sort_order );
}

void
on_sort_ascending_activate ( GtkMenuItem *menuitem,
                             gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    app_settings.sort_type = GTK_SORT_ASCENDING;
    if ( file_browser )
        ptk_file_browser_set_sort_type( PTK_FILE_BROWSER( file_browser ), app_settings.sort_type );
}


void
on_sort_descending_activate ( GtkMenuItem *menuitem,
                              gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    app_settings.sort_type = GTK_SORT_DESCENDING;
    if ( file_browser )
        ptk_file_browser_set_sort_type( PTK_FILE_BROWSER( file_browser ), app_settings.sort_type );
}


void
on_view_as_icon_activate ( GtkMenuItem *menuitem,
                           gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser;
    GtkCheckMenuItem* check_menu = GTK_CHECK_MENU_ITEM( menuitem );
    if ( ! check_menu->active )
        return ;
    file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    app_settings.view_mode = PTK_FB_ICON_VIEW;
    if ( file_browser && GTK_CHECK_MENU_ITEM( menuitem ) ->active )
        ptk_file_browser_view_as_icons( file_browser );
}

void
on_view_as_compact_list_activate ( GtkMenuItem *menuitem,
                           gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser;
    GtkCheckMenuItem* check_menu = GTK_CHECK_MENU_ITEM( menuitem );
    if ( ! check_menu->active )
        return ;
    file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    app_settings.view_mode = PTK_FB_COMPACT_VIEW;
    if ( file_browser && GTK_CHECK_MENU_ITEM( menuitem )->active )
        ptk_file_browser_view_as_compact_list( file_browser );
}

void
on_view_as_list_activate ( GtkMenuItem *menuitem,
                           gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser;
    GtkCheckMenuItem* check_menu = GTK_CHECK_MENU_ITEM( menuitem );
    if ( ! check_menu->active )
        return ;
    file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    app_settings.view_mode = PTK_FB_LIST_VIEW;
    if ( file_browser )
        ptk_file_browser_view_as_list( file_browser );
}
*/

/*
void
on_side_pane_toggled ( GtkToggleToolButton *toggletoolbutton,
                       gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    gtk_check_menu_item_set_active( main_window->open_side_pane_menu,
                                    !app_settings.show_side_pane );
}
*/

/*
void fm_main_window_start_busy_task( FMMainWindow* main_window )
{
    GdkCursor * cursor;
    if ( 0 == main_window->n_busy_tasks )                // Their is no busy task
    {
        // Create busy cursor
        cursor = gdk_cursor_new_for_display( gtk_widget_get_display( GTK_WIDGET(main_window) ), GDK_WATCH );
        if ( ! GTK_WIDGET_REALIZED( GTK_WIDGET( main_window ) ) )
            gtk_widget_realize( GTK_WIDGET( main_window ) );
        gdk_window_set_cursor ( GTK_WIDGET( main_window ) ->window, cursor );
        gdk_cursor_unref( cursor );
    }
    ++main_window->n_busy_tasks;
}

// Return gboolean and it can be used in a timeout callback
gboolean fm_main_window_stop_busy_task( FMMainWindow* main_window )
{
    --main_window->n_busy_tasks;
    if ( 0 == main_window->n_busy_tasks )                // Their is no more busy task
    {
        // Remove busy cursor
        gdk_window_set_cursor ( GTK_WIDGET( main_window ) ->window, NULL );
    }
    return FALSE;
}

void on_file_browser_splitter_pos_change( PtkFileBrowser* file_browser,
                                          GParamSpec *param,
                                          FMMainWindow* main_window )
{
    int pos;
    int i, n;
    GtkWidget* page;

    pos = gtk_paned_get_position( GTK_PANED( file_browser ) );
    main_window->splitter_pos = pos;
    n = gtk_notebook_get_n_pages( main_window->notebook );
    for ( i = 0; i < n; ++i )
    {
        page = gtk_notebook_get_nth_page( main_window->notebook, i );
        if ( page == GTK_WIDGET( file_browser ) )
            continue;
        g_signal_handlers_block_matched( page, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                         on_file_browser_splitter_pos_change, NULL );
        gtk_paned_set_position( GTK_PANED( page ), pos );
        g_signal_handlers_unblock_matched( page, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                           on_file_browser_splitter_pos_change, NULL );
    }
}
*/

/*
// FIXME: Dirty hack for fm-desktop.c.
        //This should be fixed someday. 
void fm_main_window_open_terminal( GtkWindow* parent,
                                   const char* path )
{
    char** argv = NULL;
    char* termshell = NULL;  //MOD
    int argc = 0;

    if ( !app_settings.terminal )
    {
        ptk_show_error( parent,
                        _("Error"),
                        _( "Terminal program has not been set" ) );
        fm_edit_preference( (GtkWindow*)parent, PREF_ADVANCED );
    }
    if ( app_settings.terminal )
    {
#if 0
        // FIXME: This should be support in the future once
        //          vfs_app_desktop_open_files can accept working dir.
        //          This requires API change, and shouldn't be added now.
        
        VFSAppDesktop* app = vfs_app_deaktop_new( NULL );
        app->exec = app_settings.terminal;
        vfs_app_desktop_execute( app, NULL );
        app->exec = NULL;
        vfs_app_desktop_unref( app );
#endif
        if ( geteuid() == 0 )  //MOD
            // this prevents dbus session errors in Roxterm
            termshell = g_strdup_printf( "env -u DBUS_SESSION_BUS_ADDRESS %s", app_settings.terminal );
        else
            termshell = app_settings.terminal;
        
        if ( g_shell_parse_argv( termshell,
             &argc, &argv, NULL ) )  //MOD
        {
            vfs_exec_on_screen( gtk_widget_get_screen(GTK_WIDGET(parent)),
                                path, argv, NULL, path,
                                VFS_EXEC_DEFAULT_FLAGS, NULL );
            g_strfreev( argv );
        }
    }
}

void on_open_terminal_activate ( GtkMenuItem *menuitem,
                                 gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    ptk_file_browser_open_terminal( file_browser );
}

void on_open_files_activate ( GtkMenuItem *menuitem, gpointer user_data )  //MOD added  F4
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    ptk_file_browser_open_files( file_browser, NULL );
}

void on_run_command ( GtkMenuItem *menuitem,
                                 gpointer user_data )  //MOD Ctrl-R
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    ptk_file_browser_run_command( file_browser );   
}                                        
                                        
void on_user_command_6 ( GtkMenuItem *menuitem, gpointer user_data )  //MOD
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    ptk_file_browser_open_files( file_browser, "/F6" );
}

void on_user_command_7 ( GtkMenuItem *menuitem, gpointer user_data )  //MOD
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    ptk_file_browser_open_files( file_browser, "/F7" );
}

void on_user_command_8 ( GtkMenuItem *menuitem, gpointer user_data )  //MOD
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    ptk_file_browser_open_files( file_browser, "/F8" );
}

void on_user_command_9 ( GtkMenuItem *menuitem, gpointer user_data )  //MOD
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    ptk_file_browser_open_files( file_browser, "/F9" );
}

void on_find_file_activate ( GtkMenuItem *menuitem,
                                      gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser;
    const char* cwd;
    const char* dirs[2];
    file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    cwd = ptk_file_browser_get_cwd( file_browser );

    dirs[0] = cwd;
    dirs[1] = NULL;
    fm_find_files( dirs );
}

void on_open_current_folder_as_root ( GtkMenuItem *menuitem,
                                      gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser;
    const char* cwd;
    char* cmd_line;
    GError *err = NULL;
    char* argv[5];  //MOD
    
    file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    cwd = ptk_file_browser_get_cwd( file_browser );

    //MOD separate arguments for ktsuss compatibility
    //cmd_line = g_strdup_printf( "%s --no-desktop '%s'", g_get_prgname(), cwd );
    argv[1] = g_get_prgname();
    argv[2] = g_strdup_printf ( "--no-desktop" );
    argv[3] = cwd;
    argv[4] = NULL;

    if( ! vfs_sudo_cmd_async( cwd, argv, &err ) )  //MOD
    {
        ptk_show_error( GTK_WINDOW( main_window ), _("Error"), err->message );
        g_error_free( err );
    }

    g_free( cmd_line );
}


/*
gboolean
fm_main_window_key_press_event ( GtkWidget *widget,
                                 GdkEventKey *event )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( widget );
    gint page;
    GdkModifierType mod = event->state & ( GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK );

    // Process Alt 1~0 to switch among tabs
    if ( mod == GDK_MOD1_MASK && event->keyval >= GDK_0 && event->keyval <= GDK_9 )
    {
        if ( event->keyval == GDK_0 )
            page = 9;
        else
            page = event->keyval - GDK_1;
        if ( page < gtk_notebook_get_n_pages( main_window->notebook ) )
            gtk_notebook_set_current_page( main_window->notebook, page );
        return TRUE;
    }
    return GTK_WIDGET_CLASS( parent_class ) ->key_press_event( widget, event );
}
*/

/*
void on_file_browser_before_chdir( PtkFileBrowser* file_browser,
                                   const char* path, gboolean* cancel,
                                   FMMainWindow* main_window )
{
    gchar* disp_path;

    // don't add busy cursor again if the previous state of file_browser is already busy
    if( ! file_browser->busy )
        fm_main_window_start_busy_task( main_window );

    if ( fm_main_window_get_current_file_browser( main_window ) == GTK_WIDGET( file_browser ) )
    {
        disp_path = g_filename_display_name( path );
        gtk_entry_set_text( main_window->address_bar, disp_path );
        g_free( disp_path );
        gtk_statusbar_push( GTK_STATUSBAR( main_window->status_bar ), 0, _( "Loading..." ) );
    }

    fm_main_window_update_tab_label( main_window, file_browser, path );
}

static void on_file_browser_begin_chdir( PtkFileBrowser* file_browser,
                                         FMMainWindow* main_window )
{
    gtk_widget_set_sensitive( main_window->back_btn,
                              ptk_file_browser_can_back( file_browser ) );
    gtk_widget_set_sensitive( main_window->forward_btn,
                              ptk_file_browser_can_forward( file_browser ) );
}

void on_file_browser_after_chdir( PtkFileBrowser* file_browser,
                                  FMMainWindow* main_window )
{
    fm_main_window_stop_busy_task( main_window );

    if ( fm_main_window_get_current_file_browser( main_window ) == GTK_WIDGET( file_browser ) )
    {
        char* disp_name = g_path_get_basename( file_browser->dir->disp_path );
        gtk_window_set_title( GTK_WINDOW( main_window ),
                              disp_name );
        g_free( disp_name );
        gtk_entry_set_text( main_window->address_bar, file_browser->dir->disp_path );
        gtk_statusbar_push( GTK_STATUSBAR( main_window->status_bar ), 0, "" );
        fm_main_window_update_command_ui( main_window, file_browser );
    }

    fm_main_window_update_tab_label( main_window, file_browser, file_browser->dir->disp_path );
    
    ptk_file_browser_select_last( file_browser );  //MOD restore last selections

    gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );  //MOD
}

void fm_main_window_update_command_ui( FMMainWindow* main_window,
                                       PtkFileBrowser* file_browser )
{
    if ( ! file_browser )
        file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );

    g_signal_handlers_block_matched( main_window->show_hidden_files_menu,
                                     G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                     on_show_hidden_activate, NULL );
    gtk_check_menu_item_set_active( main_window->show_hidden_files_menu,
                                    file_browser->show_hidden_files );
    g_signal_handlers_unblock_matched( main_window->show_hidden_files_menu,
                                       G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                       on_show_hidden_activate, NULL );

    // Open side pane
    g_signal_handlers_block_matched( main_window->open_side_pane_menu,
                                     G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                     on_open_side_pane_activate, NULL );
    gtk_check_menu_item_set_active( main_window->open_side_pane_menu,
                                    ptk_file_browser_is_side_pane_visible( file_browser ) );
    g_signal_handlers_unblock_matched( main_window->open_side_pane_menu,
                                       G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                       on_open_side_pane_activate, NULL );

    g_signal_handlers_block_matched ( main_window->open_side_pane_btn,
                                      G_SIGNAL_MATCH_FUNC,
                                      0, 0, NULL, on_side_pane_toggled, NULL );
    gtk_toggle_tool_button_set_active( main_window->open_side_pane_btn,
                                       ptk_file_browser_is_side_pane_visible( file_browser ) );
    g_signal_handlers_unblock_matched ( main_window->open_side_pane_btn,
                                        G_SIGNAL_MATCH_FUNC,
                                        0, 0, NULL, on_side_pane_toggled, NULL );

    switch ( ptk_file_browser_get_side_pane_mode( file_browser ) )
    {
    case PTK_FB_SIDE_PANE_BOOKMARKS:
        g_signal_handlers_block_matched( main_window->show_location_menu,
                                         G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                         on_show_loation_pane, NULL );
        gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( main_window->show_location_menu ), TRUE );
        g_signal_handlers_unblock_matched( main_window->show_location_menu,
                                           G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                           on_show_loation_pane, NULL );
        break;
    case PTK_FB_SIDE_PANE_DIR_TREE:
        g_signal_handlers_block_matched( main_window->show_dir_tree_menu,
                                         G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                         on_show_dir_tree, NULL );
        gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( main_window->show_dir_tree_menu ), TRUE );
        g_signal_handlers_unblock_matched( main_window->show_dir_tree_menu,
                                           G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                           on_show_dir_tree, NULL );
        break;
    }

    switch ( file_browser->view_mode )
    {
    case PTK_FB_ICON_VIEW:
        g_signal_handlers_block_matched( main_window->view_as_icon,
                                         G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                         on_view_as_icon_activate, NULL );
        gtk_check_menu_item_set_active( main_window->view_as_icon, TRUE );
        g_signal_handlers_unblock_matched( main_window->view_as_icon,
                                           G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                           on_view_as_icon_activate, NULL );
        break;
    case PTK_FB_COMPACT_VIEW:
        g_signal_handlers_block_matched( main_window->view_as_icon,
                                         G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                         on_view_as_icon_activate, NULL );
        gtk_check_menu_item_set_active( main_window->view_as_compact_list, TRUE );
        g_signal_handlers_unblock_matched( main_window->view_as_icon,
                                           G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                           on_view_as_icon_activate, NULL );
        break;
    case PTK_FB_LIST_VIEW:
        g_signal_handlers_block_matched( main_window->view_as_list,
                                         G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                         on_view_as_list_activate, NULL );
        gtk_check_menu_item_set_active( main_window->view_as_list, TRUE );
        g_signal_handlers_unblock_matched( main_window->view_as_list,
                                           G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                           on_view_as_list_activate, NULL );
        break;
    }

    gtk_widget_set_sensitive( main_window->back_btn,
                              ptk_file_browser_can_back( file_browser ) );
    gtk_widget_set_sensitive( main_window->forward_btn,
                              ptk_file_browser_can_forward( file_browser ) );
}

void on_view_menu_popup( GtkMenuItem* item, FMMainWindow* main_window )
{
    PtkFileBrowser * file_browser;

    file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    switch ( ptk_file_browser_get_sort_order( file_browser ) )
    {
    case PTK_FB_SORT_BY_NAME:
        gtk_check_menu_item_set_active( main_window->sort_by_name, TRUE );
        break;
    case PTK_FB_SORT_BY_SIZE:
        gtk_check_menu_item_set_active( main_window->sort_by_size, TRUE );
        break;
    case PTK_FB_SORT_BY_MTIME:
        gtk_check_menu_item_set_active( main_window->sort_by_mtime, TRUE );
        break;
    case PTK_FB_SORT_BY_TYPE:
        gtk_check_menu_item_set_active( main_window->sort_by_type, TRUE );
        break;
    case PTK_FB_SORT_BY_PERM:
        gtk_check_menu_item_set_active( main_window->sort_by_perm, TRUE );
        break;
    case PTK_FB_SORT_BY_OWNER:
        gtk_check_menu_item_set_active( main_window->sort_by_owner, TRUE );
        break;
    }

    if ( ptk_file_browser_get_sort_type( file_browser ) == GTK_SORT_ASCENDING )
        gtk_check_menu_item_set_active( main_window->sort_ascending, TRUE );
    else
        gtk_check_menu_item_set_active( main_window->sort_descending, TRUE );
}

*/

/*
void on_file_browser_pane_mode_change( PtkFileBrowser* file_browser,
                                       FMMainWindow* main_window )
{
    PtkFBSidePaneMode mode;
    GtkCheckMenuItem* check = NULL;

    if ( GTK_WIDGET( file_browser ) != fm_main_window_get_current_file_browser( main_window ) )
        return ;

    mode = ptk_file_browser_get_side_pane_mode( file_browser );
    switch ( mode )
    {
    case PTK_FB_SIDE_PANE_BOOKMARKS:
        check = GTK_CHECK_MENU_ITEM( main_window->show_location_menu );
        break;
    case PTK_FB_SIDE_PANE_DIR_TREE:
        check = GTK_CHECK_MENU_ITEM( main_window->show_dir_tree_menu );
        break;
    }
    gtk_check_menu_item_set_active( check, TRUE );
}
*/
