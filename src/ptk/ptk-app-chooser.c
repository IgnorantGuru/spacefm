/*
*  C Implementation: appchooserdlg
*
* Description:
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "ptk-app-chooser.h"
#include "ptk-utils.h"

#include <gtk/gtk.h>
#include <glib.h>
#include <string.h>

#include "private.h"

#include "vfs-mime-type.h"
#include "vfs-app-desktop.h"

#include "vfs-async-task.h"

#include "settings.h"
#include "ptk-handler.h"

enum{
    COL_APP_ICON = 0,
    COL_APP_NAME,
    COL_DESKTOP_FILE,
    COL_FULL_PATH,
    N_COLS
};
extern gboolean is_my_lock;

static void load_all_apps_in_dir( const char* dir_path, GtkListStore* list, VFSAsyncTask* task );
static gpointer load_all_known_apps_thread( VFSAsyncTask* task );

static void init_list_view( GtkTreeView* view )
{
    GtkTreeViewColumn * col = gtk_tree_view_column_new();
    GtkCellRenderer* renderer;

    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start( col, renderer, FALSE );
    gtk_tree_view_column_set_attributes( col, renderer, "pixbuf",
                                         COL_APP_ICON, NULL );

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start( col, renderer, TRUE );
    gtk_tree_view_column_set_attributes( col, renderer, "text",
                                         COL_APP_NAME, NULL );

    gtk_tree_view_append_column ( view, col );
    
    // add tooltip
    gtk_tree_view_set_tooltip_column( view, COL_FULL_PATH );
}

static gint sort_by_name( GtkTreeModel *model,
                          GtkTreeIter *a,
                          GtkTreeIter *b,
                          gpointer user_data )
{
    char * name_a, *name_b;
    gint ret = 0;
    gtk_tree_model_get( model, a, COL_APP_NAME, &name_a, -1 );
    if ( name_a )
    {
        gtk_tree_model_get( model, b, COL_APP_NAME, &name_b, -1 );
        if ( name_b )
        {
            ret = g_ascii_strcasecmp( name_a, name_b );
            g_free( name_b );
        }
        g_free( name_a );
    }
    return ret;
}

static void add_list_item( GtkListStore* list, VFSAppDesktop* desktop )
{
    GtkTreeIter it;
    GdkPixbuf* icon = NULL;
    char* file;
    const char* name = vfs_app_desktop_get_name( desktop );
    
    // desktop file already in list?
    if ( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( list ), &it ) )
    {
        do
        {
            file = NULL;
            gtk_tree_model_get( GTK_TREE_MODEL( list ), &it, COL_DESKTOP_FILE,
                                                            &file, -1 );
            if ( file )
            {
                if ( !g_strcmp0( file, name ) )
                {
                    // already exists
                    g_free( file );
                    return;
                }
                g_free( file );
            }
        }
        while ( gtk_tree_model_iter_next( GTK_TREE_MODEL( list ), &it ) );
    }

    // tooltip
    char* tooltip = g_markup_printf_escaped( "%s\nName=%s\nExec=%s%s",
                                     desktop->full_path,
                                     vfs_app_desktop_get_disp_name( desktop ),
                                     desktop->exec,
                                     desktop->terminal ? "\nTerminal=true" : "" );

    icon = vfs_app_desktop_get_icon( desktop, 20, TRUE );
    gtk_list_store_append( list, &it );
    gtk_list_store_set( list, &it, COL_APP_ICON, icon,
                        COL_APP_NAME, vfs_app_desktop_get_disp_name( desktop ),
                        COL_DESKTOP_FILE, vfs_app_desktop_get_name( desktop ),
                        COL_FULL_PATH, tooltip,
                        -1 );
    g_free( tooltip );
    if ( icon )
        g_object_unref( icon );
}

static GtkTreeModel* create_model_from_mime_type( VFSMimeType* mime_type )
{
    char** apps, **app;
    const char *type;
    GtkListStore* list = gtk_list_store_new( N_COLS, GDK_TYPE_PIXBUF,
                                                     G_TYPE_STRING,
                                                     G_TYPE_STRING,
                                                     G_TYPE_STRING );
    if ( mime_type )
    {
        apps = vfs_mime_type_get_actions( mime_type );
        type = vfs_mime_type_get_type( mime_type );
        if ( !apps && mime_type_is_text_file( NULL, type ) )
        {
            mime_type = vfs_mime_type_get_from_type( XDG_MIME_TYPE_PLAIN_TEXT );
            apps = vfs_mime_type_get_actions( mime_type );
            vfs_mime_type_unref( mime_type );
        }
        if( apps )
        {
            for( app = apps; *app; ++app )
            {
                VFSAppDesktop* desktop = vfs_app_desktop_new( *app );
                add_list_item( list, desktop );
                vfs_app_desktop_unref( desktop );
            }
            g_strfreev( apps );
        }
    }
    return (GtkTreeModel*) list;
}

gboolean on_cmdline_keypress( GtkWidget *widget,
                                 GdkEventKey *event,
                                 GtkNotebook* notebook )
{
    gtk_widget_set_sensitive( GTK_WIDGET( notebook ), gtk_entry_get_text_length(
                                            (GtkEntry*)widget ) == 0 );
    return FALSE;
}

void on_view_row_activated ( GtkTreeView *tree_view,
                             GtkTreePath *path,
                             GtkTreeViewColumn* col,
                             GtkWidget* dlg )
{
    GtkBuilder* builder = (GtkBuilder*)g_object_get_data(G_OBJECT(dlg), "builder");
    GtkWidget* ok = (GtkWidget*)gtk_builder_get_object( builder, "okbutton");
    gtk_button_clicked( GTK_BUTTON( ok ) );
}

GtkWidget* app_chooser_dialog_new( GtkWindow* parent, VFSMimeType* mime_type,
                                     gboolean focus_all_apps,
                                     gboolean show_command,
                                     gboolean show_default,
                                     gboolean dir_default )
{
    /*
    focus_all_apps      Focus All Apps tab by default
    show_command        Show custom Command entry
    show_default        Show 'Set as default' checkbox
    dir_default         Show 'Set as default' also for type dir
    */
    GtkBuilder* builder = _gtk_builder_new_from_file( PACKAGE_UI_DIR "/appchooserdlg.ui", NULL );
    GtkWidget * dlg = (GtkWidget*)gtk_builder_get_object( builder, "dlg" );
    GtkWidget* file_type = (GtkWidget*)gtk_builder_get_object( builder, "file_type" );
    char* mime_desc;
    GtkTreeView* view;
    GtkTreeModel* model;
    GtkEntry* entry;
    GtkNotebook* notebook;
    
    g_object_set_data_full( G_OBJECT(dlg), "builder", builder, (GDestroyNotify)g_object_unref );

    xset_set_window_icon( GTK_WINDOW( dlg ) );

    gtk_dialog_set_alternative_button_order( GTK_DIALOG(dlg), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1 );
    ptk_dialog_fit_small_screen( GTK_DIALOG(dlg) );

    int width = xset_get_int( "app_dlg", "x" );
    int height = xset_get_int( "app_dlg", "y" );
    if ( width && height )
        gtk_window_set_default_size( GTK_WINDOW( dlg ), width, height );
    else
        gtk_window_set_default_size( GTK_WINDOW( dlg ), 600, 600 );

    mime_desc = g_strdup_printf( " %s\n ( %s )",
                                vfs_mime_type_get_description( mime_type ),
                                mime_type->type );
    if ( mime_desc )
    {
        gtk_label_set_text( GTK_LABEL( file_type ), mime_desc );
        g_free( mime_desc );
    }
    /* Don't set default handler for directories and files with unknown type */
    if ( !show_default ||
       /*  0 == strcmp( vfs_mime_type_get_type( mime_type ), XDG_MIME_TYPE_UNKNOWN ) || */
         ( 0 == strcmp( vfs_mime_type_get_type( mime_type ), XDG_MIME_TYPE_DIRECTORY ) &&
           !dir_default ) )
    {
        gtk_widget_hide( (GtkWidget*)gtk_builder_get_object( builder,
                                                            "set_default" ) );
    }
    if ( !show_command )
    {
        gtk_widget_hide( (GtkWidget*)gtk_builder_get_object( builder,
                                                            "hbox_command" ) );
        gtk_label_set_text( GTK_LABEL( gtk_builder_get_object( builder,
                                                        "label_command" ) ),
                            _("Please choose an application:") );
    }
    
    view = GTK_TREE_VIEW( (GtkWidget*)gtk_builder_get_object(
                                            builder, "recommended_apps" ) );
    notebook = GTK_NOTEBOOK( (GtkWidget*)gtk_builder_get_object( builder,
                                            "notebook" ) );
    entry = GTK_ENTRY( (GtkWidget*)gtk_builder_get_object( builder,
                                            "cmdline" ) );

    model = create_model_from_mime_type( mime_type );
    gtk_tree_view_set_model( view, model );
    g_object_unref( G_OBJECT( model ) );
    init_list_view( view );
    gtk_widget_grab_focus( GTK_WIDGET( view ) );

    g_signal_connect( entry, "key_release_event",
                                    G_CALLBACK(on_cmdline_keypress), notebook );
    g_signal_connect( (GtkWidget*)gtk_builder_get_object( builder, "notebook"),
                                    "switch_page",
                                    G_CALLBACK(on_notebook_switch_page), dlg );
    g_signal_connect( (GtkWidget*)gtk_builder_get_object( builder, "browse_btn"),
                                    "clicked",
                                    G_CALLBACK(on_browse_btn_clicked), dlg );
    g_signal_connect ( G_OBJECT( view ), "row_activated",
                                    G_CALLBACK ( on_view_row_activated ),
                                    dlg );

    gtk_window_set_transient_for( GTK_WINDOW( dlg ), parent );
    
    if ( focus_all_apps )
    {
        // select All Apps tab
        gtk_widget_show( dlg );
        gtk_notebook_next_page( notebook );
    }
    return dlg;
}

static void on_load_all_apps_finish( VFSAsyncTask* task, gboolean is_cancelled, GtkWidget* dlg )
{
    GtkTreeModel* model;
    GtkTreeView* view;

    model = (GtkTreeModel*)vfs_async_task_get_data( task );
    if( is_cancelled )
    {
        g_object_unref( model );
        return;
    }

    view = (GtkTreeView*)g_object_get_data( G_OBJECT(task), "view" );

    gtk_tree_sortable_set_sort_func ( GTK_TREE_SORTABLE( model ),
                                    COL_APP_NAME, sort_by_name, NULL, NULL );
    gtk_tree_sortable_set_sort_column_id ( GTK_TREE_SORTABLE( model ),
                                        COL_APP_NAME, GTK_SORT_ASCENDING );

    gtk_tree_view_set_model( view, model );
    g_object_unref( model );

    gdk_window_set_cursor( gtk_widget_get_window ( dlg ), NULL );
}

void
on_notebook_switch_page ( GtkNotebook *notebook,
                          GtkWidget *page,
                          guint page_num,
                          gpointer user_data )
{
    GtkWidget * dlg = ( GtkWidget* ) user_data;
    GtkTreeView* view;

    GtkBuilder* builder = (GtkBuilder*)g_object_get_data(G_OBJECT(dlg), "builder");

    /* Load all known apps installed on the system */
    if ( page_num == 1 )
    {
        view = GTK_TREE_VIEW( (GtkWidget*)gtk_builder_get_object( builder, "all_apps" ) );
        if ( ! gtk_tree_view_get_model( view ) )
        {
            GdkCursor* busy;
            VFSAsyncTask* task;
            GtkListStore* list;
            init_list_view( view );
            gtk_widget_grab_focus( GTK_WIDGET( view ) );
            busy = gdk_cursor_new_for_display( gtk_widget_get_display(GTK_WIDGET( view )), GDK_WATCH );
            gdk_window_set_cursor( gtk_widget_get_window( GTK_WIDGET( gtk_widget_get_toplevel(GTK_WIDGET(view)) ) ), busy );
            gdk_cursor_unref( busy );

            list = gtk_list_store_new( N_COLS, GDK_TYPE_PIXBUF,
                                       G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING );
            task = vfs_async_task_new( (VFSAsyncFunc) load_all_known_apps_thread, list );
            g_object_set_data( G_OBJECT(task), "view", view );
            g_object_set_data( G_OBJECT(dlg), "task", task );
            g_signal_connect( task, "finish", G_CALLBACK(on_load_all_apps_finish), dlg );
            vfs_async_task_execute( task );
            g_signal_connect ( G_OBJECT( view ), "row_activated",
                                    G_CALLBACK ( on_view_row_activated ),
                                    dlg );
        }
    }
}

/*
* Return selected application in a ``newly allocated'' string.
* Returned string is the file name of the *.desktop file or a command line.
* These two can be separated by check if the returned string is ended
* with ".desktop" postfix.
*/
gchar* app_chooser_dialog_get_selected_app( GtkWidget* dlg )
{
    gchar * app = NULL;
    GtkBuilder* builder = (GtkBuilder*)g_object_get_data(G_OBJECT(dlg), "builder");
    GtkEntry* entry = GTK_ENTRY( (GtkWidget*)gtk_builder_get_object( builder, "cmdline" ) );
    GtkNotebook* notebook;
    int idx;
    GtkBin* scroll;
    GtkTreeView* view;
    GtkTreeSelection* tree_sel;
    GtkTreeModel* model;
    GtkTreeIter it;

    app = (char*)gtk_entry_get_text( entry );
    if ( app && *app )
    {
        return g_strdup( app );
    }

    notebook = GTK_NOTEBOOK( (GtkWidget*)gtk_builder_get_object( builder, "notebook" ) );
    idx = gtk_notebook_get_current_page ( notebook );
    scroll = GTK_BIN( gtk_notebook_get_nth_page( notebook, idx ) );
    view = GTK_TREE_VIEW(gtk_bin_get_child( scroll ));
    tree_sel = gtk_tree_view_get_selection( view );

    if ( gtk_tree_selection_get_selected ( tree_sel, &model, &it ) )
    {
        gtk_tree_model_get( model, &it, COL_DESKTOP_FILE, &app, -1 );
    }
    else
        app = NULL;
    return app;
}

/*
* Check if the user set the selected app default handler.
*/
gboolean app_chooser_dialog_get_set_default( GtkWidget* dlg )
{
    GtkBuilder* builder = (GtkBuilder*)g_object_get_data(G_OBJECT(dlg), "builder");
    GtkWidget * check = (GtkWidget*)gtk_builder_get_object( builder, "set_default" );
    return gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON( check ) );
}

void
on_browse_btn_clicked ( GtkButton *button,
                        gpointer user_data )
{
    char * filename;
    char* app_name;
    GtkEntry* entry;
    GtkNotebook* notebook;
    const char* app_path = "/usr/share/applications";

    GtkWidget* parent = GTK_WIDGET(user_data );
    GtkWidget* dlg = gtk_file_chooser_dialog_new( NULL, GTK_WINDOW( parent ),
                                                  GTK_FILE_CHOOSER_ACTION_OPEN,
                                                  GTK_STOCK_CANCEL,
                                                  GTK_RESPONSE_CANCEL,
                                                  GTK_STOCK_OPEN,
                                                  GTK_RESPONSE_OK,
                                                  NULL );
    GtkBuilder* builder = (GtkBuilder*)g_object_get_data(G_OBJECT(parent), "builder");

    xset_set_window_icon( GTK_WINDOW( dlg ) );

    gtk_file_chooser_set_current_folder ( GTK_FILE_CHOOSER ( dlg ),
                                          "/usr/bin" );
    if ( gtk_dialog_run( GTK_DIALOG( dlg ) ) == GTK_RESPONSE_OK )
    {
        filename = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER ( dlg ) );
        if( filename )
        {
            entry = GTK_ENTRY( (GtkWidget*)gtk_builder_get_object( builder, "cmdline" ) );
            notebook = GTK_NOTEBOOK( (GtkWidget*)gtk_builder_get_object( builder, "notebook" ) );
            /* FIXME: path shouldn't be hard-coded */
            if( g_str_has_prefix( filename, app_path )
                && g_str_has_suffix( filename, ".desktop" ) )
            {
                app_name = g_path_get_basename( filename );
                gtk_entry_set_text( entry, app_name );
                g_free( app_name );
            }
            else
            {
                gtk_entry_set_text( entry, filename );
            }
            g_free ( filename );
            gtk_widget_set_sensitive( GTK_WIDGET( notebook ), 
                                gtk_entry_get_text_length( entry ) == 0 );
            gtk_widget_grab_focus( GTK_WIDGET( entry ) );
            gtk_editable_set_position( GTK_EDITABLE( entry ), -1 );
        }
    }
    gtk_widget_destroy( dlg );
}

static void on_dlg_response( GtkDialog* dlg, int id, gpointer user_data )
{
    VFSAsyncTask* task;
    GtkAllocation allocation;

    gtk_widget_get_allocation ( GTK_WIDGET( dlg ), &allocation );
    int width = allocation.width;
    int height = allocation.height;
    if ( width && height )
    {
        char* str = g_strdup_printf( "%d", width );
        xset_set( "app_dlg", "x", str );
        g_free( str );
        str = g_strdup_printf( "%d", height );
        xset_set( "app_dlg", "y", str );
        g_free( str );
    }

    switch( id )
    {
    /* The dialog is going to be closed */
    case GTK_RESPONSE_OK:
    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_NONE:
    case GTK_RESPONSE_DELETE_EVENT:
        /* cancel app loading on dialog closing... */
        task = (VFSAsyncTask*)g_object_get_data( G_OBJECT(dlg), "task" );
        if( task )
        {
            // see note in vfs-async-task.c: vfs_async_task_cancel()
            GDK_THREADS_LEAVE(); 
            vfs_async_task_cancel( task );
            GDK_THREADS_ENTER();
            /* The GtkListStore will be freed in "finish" handler of task - on_load_all_app_finish(). */
            g_object_unref( task );
        }
        break;
    }
}

void ptk_app_chooser_has_handler_warn( GtkWidget* parent, VFSMimeType* mime_type )
{
    // is file handler set for this type?
    char* msg;
    GSList* handlers_slist = ptk_handler_file_has_handlers(
                        HANDLER_MODE_FILE, HANDLER_MOUNT,
                        NULL, mime_type, FALSE, FALSE, TRUE );
    if ( handlers_slist )
    {
        msg = g_strdup_printf( _("Note:  MIME type '%s' is currently set to open with the '%s' file handler, rather than with your associated MIME application.\n\nYou may also need to disable this handler in Open|File Handlers for this type to be opened with your associated application by default."),
                        vfs_mime_type_get_type( mime_type ),
                        ((XSet*)handlers_slist->data)->menu_label );
        xset_msg_dialog( parent, 0, _("MIME Type Has Handler"),
                                        NULL, 0, msg, NULL, NULL );
        g_free( msg );
        g_slist_free( handlers_slist );
    }
    else if ( !xset_get_b( "arc_def_open" ) )
    {
        // is archive handler set for this type?
        handlers_slist = ptk_handler_file_has_handlers(
                            HANDLER_MODE_ARC, HANDLER_EXTRACT,
                            NULL, mime_type, FALSE, FALSE, TRUE );
        if ( handlers_slist )
        {
            msg = g_strdup_printf( _("Note:  MIME type '%s' is currently set to open with the '%s' archive handler, rather than with your associated MIME application.\n\nYou may also need to disable this handler in Open|Archive Defaults|Archive Handlers, OR select global option Open|Archive Defaults|Open With App, for this type to be opened with your associated application by default."),
                            vfs_mime_type_get_type( mime_type ),
                            ((XSet*)handlers_slist->data)->menu_label );
            xset_msg_dialog( parent, 0, _("MIME Type Has Handler"),
                                            NULL, 0, msg, NULL, NULL );
            g_free( msg );
            g_slist_free( handlers_slist );
        }
    }
}

gchar* ptk_choose_app_for_mime_type( GtkWindow* parent,
                                     VFSMimeType* mime_type,
                                     gboolean focus_all_apps,
                                     gboolean show_command,
                                     gboolean show_default,
                                     gboolean dir_default )
{
    /*
    focus_all_apps      Focus All Apps tab by default
    show_command        Show custom Command entry
    show_default        Show 'Set as default' checkbox
    dir_default         Show 'Set as default' also for type dir
    */
    GtkWidget * dlg;
    gchar* app = NULL;
    gchar* custom = NULL;

    dlg = app_chooser_dialog_new( parent, mime_type, focus_all_apps,
                                                     show_command,
                                                     show_default,
                                                     dir_default );

    g_signal_connect( dlg, "response",  G_CALLBACK(on_dlg_response), NULL );

    if ( gtk_dialog_run( GTK_DIALOG( dlg ) ) == GTK_RESPONSE_OK )
    {
        app = app_chooser_dialog_get_selected_app( dlg );
        if ( app )
        {
            /* The selected app is set to default action */
            /* TODO: full-featured mime editor??? */
            if ( app_chooser_dialog_get_set_default( dlg ) )
            {
                vfs_mime_type_set_default_action( mime_type, app );
                ptk_app_chooser_has_handler_warn( dlg, mime_type );
            }
            else if (/* strcmp( vfs_mime_type_get_type( mime_type ),
                                                    XDG_MIME_TYPE_UNKNOWN ) && */
                    ( dir_default ||
                      strcmp( vfs_mime_type_get_type( mime_type ),
                                                    XDG_MIME_TYPE_DIRECTORY ) ) )
            {
                vfs_mime_type_add_action( mime_type, app, &custom );
                g_free( app );
                app = custom;
            }
        }
    }

    gtk_widget_destroy( dlg );
    return app;
}

void load_all_apps_in_dir( const char* dir_path, GtkListStore* list, VFSAsyncTask* task )
{

    GDir* dir = g_dir_open( dir_path, 0, NULL );
    if( dir )
    {
        const char* name;
        char* path;
        VFSAppDesktop* app;
        while( (name = g_dir_read_name( dir )) )
        {
            vfs_async_task_lock( task );
            if( task->cancel )
            {
                vfs_async_task_unlock( task );
                break;
            }
            vfs_async_task_unlock( task );

            path = g_build_filename( dir_path, name, NULL );
            if( G_UNLIKELY( g_file_test( path, G_FILE_TEST_IS_DIR ) ) )
            {
                /* recursively load sub dirs */
                load_all_apps_in_dir( path, list, task );
                g_free( path );
                continue;
            }
            if( ! g_str_has_suffix(name, ".desktop") )
                continue;

            vfs_async_task_lock( task );
            if( task->cancel )
            {
                vfs_async_task_unlock( task );
                break;
            }
            vfs_async_task_unlock( task );

            app = vfs_app_desktop_new( path );

            GDK_THREADS_ENTER();
            add_list_item( list, app ); /* There are some operations using GTK+, so lock may be needed. */
            GDK_THREADS_LEAVE();

            vfs_app_desktop_unref( app );
            g_free( path );
        }
        g_dir_close( dir );
    }
}

gpointer load_all_known_apps_thread( VFSAsyncTask* task )
{
    gchar* dir, **dirs;
    GtkListStore* list;
    gboolean cancel = FALSE;

    GDK_THREADS_ENTER();
    list = GTK_LIST_STORE( vfs_async_task_get_data(task) );
    GDK_THREADS_LEAVE();

    dir = g_build_filename( g_get_user_data_dir(), "applications", NULL );
    load_all_apps_in_dir( dir, list, task );
    g_free( dir );

    for( dirs = (gchar **) g_get_system_data_dirs(); ! task->cancel && *dirs; ++dirs )
    {
        dir = g_build_filename( *dirs, "applications", NULL );
        load_all_apps_in_dir( dir, list, task );
        g_free( dir );
    }

    vfs_async_task_lock( task );
    cancel = task->cancel;
    vfs_async_task_unlock( task );
    return NULL;
}

