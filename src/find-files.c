/*
 *      find-files.c
 *
 *      Copyright 2008 PCMan <pcman.tw@gmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 3 of the License, or
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

/* FIXME: Currently this only works with GNU find-utils.
 * Compatibility with other systems like BSD, need to be improved.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>

#include "pcmanfm.h"

#include "glib-mem.h"

#include "vfs-dir.h"
#include "vfs-file-info.h"

#include "vfs-async-task.h"
#include "exo-tree-view.h"
#include "vfs-volume.h"

#include "main-window.h"
#include "settings.h"

#include "ptk-file-misc.h"
#include "ptk-utils.h"

enum
{
    COL_ICON,
    COL_NAME,
    COL_DIR,
    COL_SIZE,
    COL_TYPE,
    COL_MTIME,
    COL_INFO,
    N_RES_COLS
};

typedef struct _FindFile
{
    GtkWidget* win;
    GtkWidget* search_criteria;

    GtkWidget* fn_pattern;
    GtkWidget* fn_pattern_entry;
    GtkWidget* fn_case_sensitive;

    /* file content */
    GtkWidget* fc_pattern;
    GtkWidget* fc_case_sensitive;
    GtkWidget* fc_use_regexp;

    /* advanced options */
    GtkWidget* search_hidden;

    /* size & date */
    GtkWidget* use_size_lower;
    GtkWidget* use_size_upper;
    GtkWidget* size_lower;
    GtkWidget* size_upper;
    GtkWidget* size_lower_unit;
    GtkWidget* size_upper_unit;

    GtkWidget* date_limit;
    GtkWidget* date1;
    GtkWidget* date2;

    /* file types */
    GtkWidget* all_files;
    GtkWidget* text_files;
    GtkWidget* img_files;
    GtkWidget* audio_files;
    GtkWidget* video_files;

    /* places */
    GtkListStore* places_list;
    GtkWidget* places_view;
    GtkWidget* add_folder_btn;
    GtkWidget* remove_folder_btn;
    GtkWidget* include_sub;

    /* search result pane */
    GtkWidget* search_result;
    GtkWidget* result_view;
    GtkListStore* result_list;

    /* buttons */
    GtkWidget* start_btn;
    GtkWidget* stop_btn;
    GtkWidget* again_btn;

    GPid pid;
    int stdo;

    VFSAsyncTask* task;
}FindFile;

typedef struct
{
    VFSFileInfo* fi;
    char* dir_path;
}FoundFile;

static const char menu_def[] =
"<ui>"
"<popup name=\"Popup\">"
  "<menuitem name=\"Open\" action=\"OpenAction\" />"
  "<menuitem name=\"OpenFolder\" action=\"OpenFolderAction\" />"
"</popup>"
"</ui>";

static gboolean open_file( char* dir, GList* files, PtkFileBrowser* file_browser )
{
    if( files )
    {
/*igtodo test passing file_browser here? */
        ptk_open_files_with_app( dir, files, NULL, NULL, NULL, FALSE, TRUE );

        //sfm open selected dirs
        if ( file_browser )
        {
            GList * l;
            gchar* full_path;
            VFSFileInfo* file;
            
            for ( l = files; l; l = l->next )
            {
                file = ( VFSFileInfo* ) l->data;
                if ( G_UNLIKELY( ! file ) )
                    continue;

                full_path = g_build_filename( dir,
                                              vfs_file_info_get_name( file ),
                                              NULL );
                if ( G_LIKELY( full_path ) )
                {
                    if ( g_file_test( full_path, G_FILE_TEST_IS_DIR ) )
                    {
                        ptk_file_browser_emit_open( file_browser,
                                                            full_path, PTK_OPEN_NEW_TAB );
                    }
                    g_free( full_path );
                }
            }
        }
        vfs_file_info_list_free( files );  //sfm moved free list to here
        return TRUE;
    }
    return FALSE;
}

static void open_dir( char* dir, GList* files, FMMainWindow* w )
{
    fm_main_window_add_new_tab( w, dir );
}

static void on_open_files( GtkAction* action, FindFile* data )
{
    GtkTreeModel* model;
    GtkTreeSelection* sel;
    GtkTreeIter it;
    GList *row, *rows, *sel_files;
    GHashTable* hash;
    GtkWidget* w;
    VFSFileInfo* fi;
    gboolean open_files_has_dir = FALSE;  //sfm
    PtkFileBrowser* file_browser = NULL;  //sfm
    gboolean open_files = TRUE;
    
    if ( action )
        open_files = (0 == strcmp( gtk_action_get_name(action), "OpenAction") );

    sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( data->result_view ) );
    rows = gtk_tree_selection_get_selected_rows( sel, &model );
    if( ! rows )
        return;

    //sfm this frees list when new value inserted - caused segfault
    //hash = g_hash_table_new_full( g_str_hash, g_str_equal, (GDestroyNotify)g_free, open_files ? (GDestroyNotify)vfs_file_info_list_free : NULL );
    hash = g_hash_table_new_full( g_str_hash, g_str_equal, (GDestroyNotify)g_free, NULL );

    for( row = rows; row; row = row->next )
    {
        char* dir;
        GtkTreePath* tp = (GtkTreePath*)row->data;

        if( gtk_tree_model_get_iter( model, &it, tp ) )
        {
            if( open_files ) /* open files */
                gtk_tree_model_get( model, &it, COL_INFO, &fi, COL_DIR, &dir, -1 );
            else  /* open containing folders */
                gtk_tree_model_get( model, &it, COL_DIR, &dir, -1 );

            if( open_files )
            {
                GList *l;
                l = g_hash_table_lookup( hash, dir );
                l = g_list_prepend( l, vfs_file_info_ref(fi) );
                g_hash_table_insert( hash, dir, l );  //sfm caused segfault with destroy function
                if ( vfs_file_info_is_dir( fi ) )  //sfm
                    open_files_has_dir = TRUE;
            }
            else
            {
                if( g_hash_table_lookup( hash, dir ) )
                    g_free( dir );
                g_hash_table_insert( hash, dir, NULL );
            }
        }
        gtk_tree_path_free( tp );
    }
    g_list_free( rows );

    if( open_files )
    {
        if ( open_files_has_dir )
        {
            w = GTK_WIDGET( fm_main_window_get_on_current_desktop() );
            if( ! w )
            {
                w = fm_main_window_new();
                // now done in fm_main_window_new
                //gtk_window_set_default_size( GTK_WINDOW( w ), app_settings.width, app_settings.height );
            }
            gtk_window_present( (GtkWindow*)w );
            file_browser = (PtkFileBrowser*)fm_main_window_get_current_file_browser(
                                                                (FMMainWindow*)w );
        }
        g_hash_table_foreach_steal( hash, (GHRFunc)open_file, file_browser );
    }
    else
    {
        w = GTK_WIDGET( fm_main_window_get_on_current_desktop() );
        if( ! w )
        {
            w = fm_main_window_new();
            // now done in fm_main_window_new
            //gtk_window_set_default_size( GTK_WINDOW( w ), app_settings.width, app_settings.height );
        }

        g_hash_table_foreach( hash, (GHFunc)open_dir, w );
        gtk_window_present( (GtkWindow*)w );
    }

    g_hash_table_destroy( hash );
}

static GtkActionEntry menu_actions[] =
{
    { "OpenAction", GTK_STOCK_OPEN, N_("_Open"), NULL, NULL, G_CALLBACK(on_open_files) },
    { "OpenFolderAction", GTK_STOCK_OPEN, N_("Open Containing _Folder"), NULL, NULL, G_CALLBACK(on_open_files) }
};

static int get_date_offset( GtkCalendar* calendar )
{
    /* FIXME: I think we need a better implementation for this */
    GDate* date;
    GDate* today;
    int y, m, d, offset;
    time_t timeval = time(NULL);
    struct tm* lt = localtime( &timeval );

    gtk_calendar_get_date( calendar, &y, &m, &d );

    date = g_date_new_dmy( d, m, y );
    today = g_date_new_dmy( lt->tm_mday, lt->tm_mon, lt->tm_year+1900 );

    offset = g_date_days_between( date, today );

    g_date_free(date);
    g_date_free(today);
    return ABS(offset);
}

static char** compose_command( FindFile* data )
{
    GArray* argv = g_array_sized_new( TRUE, TRUE, sizeof(char*), 10 );
    char *arg, *tmp;
    GtkTreeIter it;
    char size_units[] = {"ckMG"};
    int idx;
    gboolean print = FALSE;

    arg = g_strdup( "find" );
    g_array_append_val( argv, arg );
    arg = g_strdup("-H");
    g_array_append_val( argv, arg );

    if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( data->places_list ), &it ) )
    {
        do {
            gtk_tree_model_get( GTK_TREE_MODEL( data->places_list ), &it, 0, &arg, -1 );
            if( arg )
            {
                if( *arg )
                    g_array_append_val( argv, arg );
                else
                    g_free( arg );
            }
        }while( gtk_tree_model_iter_next( GTK_TREE_MODEL( data->places_list ), &it ) );
    }

    /* if include sub is excluded */  //MOD added
    if( ! gtk_toggle_button_get_active((GtkToggleButton*)data->include_sub ) )
    {
        arg = g_strdup("-maxdepth");
        g_array_append_val( argv, arg );
        arg = g_strdup("1");
        g_array_append_val( argv, arg );
    }

    /* if hidden files is excluded */
    if( ! gtk_toggle_button_get_active((GtkToggleButton*)data->search_hidden ) )
    {
        arg = g_strdup("-name");
        g_array_append_val( argv, arg );
        arg = g_strdup(".*");
        g_array_append_val( argv, arg );
        arg = g_strdup("-prune");
        g_array_append_val( argv, arg );
        arg = g_strdup("-or");
        g_array_append_val( argv, arg );
    }

    /* if lower limit of file size is set */
    if( gtk_toggle_button_get_active((GtkToggleButton*)data->use_size_lower ) )
    {
//        arg = g_strdup("(");
//        g_array_append_val( argv, arg );

        arg = g_strdup("-size");
        g_array_append_val( argv, arg );

        tmp = g_strdup_printf( "+%u%c",
                        gtk_spin_button_get_value_as_int( (GtkSpinButton*)data->size_lower ),
                        size_units[ gtk_combo_box_get_active( (GtkComboBox*)data->size_lower_unit ) ] );
    g_array_append_val( argv, tmp );

/*        arg = g_strdup( tmp + 1 );
        g_array_append_val( argv, arg );

        arg = g_strdup("-o"); // -or
        g_array_append_val( argv, arg );

        arg = g_strdup("-size");
        g_array_append_val( argv, arg );

        g_array_append_val( argv, tmp );

        arg = g_strdup(")");
        g_array_append_val( argv, arg );
*/    }

    /* if upper limit of file size is set */
    if( gtk_toggle_button_get_active((GtkToggleButton*)data->use_size_upper ) )
    {
//        arg = g_strdup("(");
//        g_array_append_val( argv, arg );

        arg = g_strdup("-size");
        g_array_append_val( argv, arg );

        tmp = g_strdup_printf( "-%u%c",
                        gtk_spin_button_get_value_as_int( (GtkSpinButton*)data->size_upper ),
                        size_units[ gtk_combo_box_get_active( (GtkComboBox*)data->size_upper_unit ) ] );

        arg = g_strdup( tmp + 1 );
        g_array_append_val( argv, arg );
/*
        arg = g_strdup("-o"); // -or
        g_array_append_val( argv, arg );

        arg = g_strdup("-size");
        g_array_append_val( argv, arg );

        g_array_append_val( argv, tmp );

        arg = g_strdup(")");
        g_array_append_val( argv, arg );
*/
    }

    /* If -name is used */
    tmp = (char*)gtk_entry_get_text( (GtkEntry*)data->fn_pattern_entry );
    if( tmp && strcmp(tmp, "*") )
    {
        if( gtk_toggle_button_get_active((GtkToggleButton*)data->fn_case_sensitive) )
            arg = g_strdup("-name");
        else
            arg = g_strdup("-iname");
        g_array_append_val( argv, arg );

        arg = g_strdup( tmp );
        g_array_append_val( argv, arg );
    }

    /* match by mtime */
    idx = gtk_combo_box_get_active( (GtkComboBox*)data->date_limit );
    if( idx > 0 )
    {
        if( idx == 5 )  /* range */
        {
            arg = g_strdup("(");
            g_array_append_val( argv, arg );

            arg = g_strdup( "-mtime" );
            g_array_append_val( argv, arg );

            /* date1 */
            arg = g_strdup_printf( "-%d",  get_date_offset( (GtkCalendar*)data->date1 ) );
            g_array_append_val( argv, arg );

            arg = g_strdup( "-mtime" );
            g_array_append_val( argv, arg );

            /* date2 */
            arg = g_strdup_printf( "+%d", get_date_offset( (GtkCalendar*)data->date2 ) );
            g_array_append_val( argv, arg );

            arg = g_strdup(")");
            g_array_append_val( argv, arg );
        }
        else
        {
            arg = g_strdup( "-mtime" );
            g_array_append_val( argv, arg );

            switch( idx )
            {
            case 1: /* within one day */
                arg = g_strdup( "-1" );
                break;
            case 2: /* within one week */
                arg = g_strdup( "-7" );
                break;
            case 3: /* within one month */
                arg = g_strdup( "-30" );
                break;
            case 4: /* within one year */
                arg = g_strdup( "-365" );
                break;
            }
            g_array_append_val( argv, arg );
        }
    }

    /* grep text inside files */
    tmp = (char*)gtk_entry_get_text( (GtkEntry*)data->fc_pattern );
    if( tmp && *tmp )
    {
        print = TRUE;

        /* ensure we only call 'grep' on regular files */
        arg = g_strdup("-type");
        g_array_append_val( argv, arg );
        arg = g_strdup("f");
        g_array_append_val( argv, arg );

        arg = g_strdup("-exec");
        g_array_append_val( argv, arg );

        arg = g_strdup("grep");
        g_array_append_val( argv, arg );

        if( !gtk_toggle_button_get_active((GtkToggleButton*)data->fc_case_sensitive) )
        {
            arg = g_strdup("-i");
            g_array_append_val( argv, arg );
        }

        arg = g_strdup("--files-with-matches");
        g_array_append_val( argv, arg );

        if( gtk_toggle_button_get_active((GtkToggleButton*)data->fc_use_regexp) )
            arg = g_strdup( "--regexp" );
        else
            arg = g_strdup( "--fixed-strings" );
        g_array_append_val( argv, arg );

        arg = g_strdup(tmp);
        g_array_append_val( argv, arg );

        arg = g_strdup("{}");
        g_array_append_val( argv, arg );

        arg = g_strdup(";");
        g_array_append_val( argv, arg );
    }

    if( ! print )
    {
        arg = g_strdup("-print");
        g_array_append_val( argv, arg );
    }
    return (char**)g_array_free( argv, FALSE );
}

static void finish_search( FindFile* data )
{
    /* NOTE: This function runs in task thread, not main loop, within
     * GDK_THREADS_ENTER block in vfs-async-task.c:vfs_async_task_thread() */
    if( data->pid )
    {
        int status;
        kill( data->pid, SIGTERM );
        waitpid( data->pid, &status, 0 );
        data->pid = 0;
        /* g_debug( "find process is killed!" ); */
    }
    if( data->task )
    {
        g_object_unref( data->task );
        data->task = NULL;
    }
    gdk_window_set_cursor( gtk_widget_get_window( data->search_result ), NULL );
    gtk_widget_hide( data->stop_btn );
    gtk_widget_show( data->again_btn );
}

static void process_found_files( FindFile* data, GQueue* queue, const char* path )
{
    char *name;
    gsize len, term;
    GtkTreeIter it;
    VFSFileInfo* fi;
    GdkPixbuf* icon;
    FoundFile* ff;

    if( path )
    {
        name = g_filename_display_basename( path );
        fi = vfs_file_info_new();
        if( vfs_file_info_get( fi, path, name, TRUE ) )
        {
            ff = g_slice_new0( FoundFile );
            ff->fi = fi;
            ff->dir_path = g_path_get_dirname( path );
            g_queue_push_tail( queue, ff );
        }
        else
        {
            vfs_file_info_unref( fi );
        }
        g_free( name );

        /* we queue the found files, and not add them to the tree view direclty.
         * when we queued more than 10 files, we add them at once. I think
         * this can prevent locking gtk+ too frequently and improve responsiveness.
         * FIXME: This could blocked the last queued files and delay their display
         * to the end of the whole search. A better method is needed.
         */
//MOD disabled this - causes last queued files to not display
//        if( g_queue_get_length( queue ) < 10 )
//            return;
    }

    while( ff = (FoundFile*)g_queue_pop_head(queue) )
    {
        GDK_THREADS_ENTER();
        gtk_list_store_append( data->result_list, &it );
        icon = vfs_file_info_get_small_icon( ff->fi );
        gtk_list_store_set( data->result_list, &it,
                                    COL_ICON, icon,
                                    COL_NAME, vfs_file_info_get_disp_name(ff->fi),
                                    COL_DIR, ff->dir_path, /* FIXME: non-UTF8? */
                                    COL_TYPE, vfs_file_info_get_mime_type_desc( ff->fi ),
                                    COL_SIZE, vfs_file_info_get_disp_size( ff->fi ),
                                    COL_MTIME, vfs_file_info_get_disp_mtime( ff->fi ),
                                    COL_INFO, ff->fi, -1 );
        g_object_unref( icon );
        GDK_THREADS_LEAVE();
        g_slice_free( FoundFile, ff );
    }
}

static gpointer search_thread( VFSAsyncTask* task, FindFile* data )
{
    ssize_t rlen;
    char buf[4096 ];
    GString* path = g_string_new_len( NULL, 256 );
    GQueue* queue = g_queue_new();

    while( ! data->task->cancel && ( rlen = read( data->stdo, buf, sizeof(buf) - 1 ) ) > 0 )
    {
        char*pbuf, *eol;
        buf[ rlen ] = '\0';
        pbuf = buf;

        while( ! data->task->cancel )
        {
            if( eol = strchr( pbuf, '\n' ) )    /* end of line is reached */
            {
                *eol = '\0';
                g_string_append( path, pbuf );

                /* we get a complete file path */
                if( ! data->task->cancel )
                {
                    process_found_files( data, queue, path->str );
                }

                pbuf = eol + 1; /* start reading the next line */
                g_string_assign( path, "" );   /* empty the line buffer */
            }
            else /* end of line is not reached */
            {
                g_string_append( path, pbuf );  /* store the partial path in the buffer */
                break;
            }
        }
    }
    /* end of stream (EOF) is reached */
    if( path->len > 0 ) /* this is the last line without eol character '\n' */
    {
        if( ! data->task->cancel )
        {
            process_found_files( data, queue, path->str );
            process_found_files( data, queue, NULL );
        }
    }

    g_queue_free( queue );
    g_string_free( path, TRUE );
    return NULL;
}

static void on_search_finish( VFSAsyncTask* task, gboolean cancelled, FindFile* data )
{
    finish_search( data );
}

static void on_start_search( GtkWidget* btn, FindFile* data )
{
    char** argv;
    GError* err = NULL;
    int stdo, stde;
    char* cmd_line;
    GtkAllocation allocation;

    gtk_widget_get_allocation ( GTK_WIDGET( data->win ), &allocation );
    int width =  allocation.width;
    int height = allocation.height;
    if ( width && height )
    {
        char* str = g_strdup_printf( "%d", width );
        xset_set( "main_search", "x", str );
        g_free( str );
        str = g_strdup_printf( "%d", height );
        xset_set( "main_search", "y", str );
        g_free( str );
    }

    gtk_widget_hide( data->search_criteria );
    gtk_widget_show( data->search_result );

    gtk_widget_hide( btn );
    gtk_widget_show( data->stop_btn );

    argv = compose_command( data );

    cmd_line = g_strjoinv( " ", argv );
    g_debug( "find command: %s", cmd_line );
    g_free( cmd_line );
    if( g_spawn_async_with_pipes( g_get_home_dir(), argv, NULL,
                                                  G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
                                                  NULL, NULL, &data->pid,
                                                  NULL, &data->stdo, NULL, &err ) )
    {
        GdkCursor* busy_cursor;
        data->task = vfs_async_task_new( (VFSAsyncFunc)search_thread, data );
        g_signal_connect( data->task, "finish", G_CALLBACK( on_search_finish ), data );
        vfs_async_task_execute( data->task );

        busy_cursor = gdk_cursor_new( GDK_WATCH );
        gdk_window_set_cursor( gtk_widget_get_window (data->search_result), busy_cursor );
        gdk_cursor_unref( busy_cursor );
    }
    else
    {
        g_error_free( err );
    }

    g_strfreev( argv );
}

static void on_stop_search( GtkWidget* btn, FindFile* data )
{
    if( data->task && ! vfs_async_task_is_finished( data->task ) )
    {
        // see note in vfs-async-task.c: vfs_async_task_cancel()
        GDK_THREADS_LEAVE(); 
        vfs_async_task_cancel( data->task );
        GDK_THREADS_ENTER();
    }
}

static void on_search_again( GtkWidget* btn, FindFile* data )
{
    GtkAllocation allocation;

    gtk_widget_get_allocation ( GTK_WIDGET( data->win ), &allocation );
    int width =  allocation.width;
    int height = allocation.height;
    if ( width && height )
    {
        char* str = g_strdup_printf( "%d", width );
        xset_set( "main_search", "x", str );
        g_free( str );
        str = g_strdup_printf( "%d", height );
        xset_set( "main_search", "y", str );
        g_free( str );
    }

    gtk_widget_show( data->search_criteria );
    gtk_widget_hide( data->search_result );

    gtk_widget_hide( btn );
    gtk_widget_show( data->start_btn );

    g_object_ref( data->result_list );
    gtk_tree_view_set_model( (GtkTreeView*)data->result_view, NULL );
    gtk_list_store_clear( data->result_list );
    gtk_tree_view_set_model( (GtkTreeView*)data->result_view, GTK_TREE_MODEL( data->result_list ) );
    g_object_unref( data->result_list );
}

static void menu_pos( GtkMenu* menu, int* x, int* y, gboolean *push_in, GtkWidget* btn )
{
    GtkAllocation allocation;

    /* FIXME: I'm not sure if this work well in different WMs */
    gdk_window_get_position( gtk_widget_get_window( btn ), x, y);
/*    gdk_window_get_root_origin( btn->window, x, y); */
    gtk_widget_get_allocation ( GTK_WIDGET( btn ), &allocation );
    *x += allocation.x;
    *y += allocation.y + allocation.height;
    *push_in = FALSE;
}

static void add_search_dir( FindFile* data, const char* path )
{
    GtkTreeIter it;
    gtk_list_store_append( data->places_list, &it );
    gtk_list_store_set( data->places_list, &it, 0, path, -1 );
}

static void on_add_search_browse(GtkWidget* menu, FindFile* data)
{
    GtkWidget* dlg = gtk_file_chooser_dialog_new(
      _("Select a folder"), GTK_WINDOW( data->win ),
      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
      GTK_STOCK_OPEN, GTK_RESPONSE_OK,
      NULL );
      
    gtk_dialog_set_alternative_button_order( GTK_DIALOG( dlg ), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL );
    if( gtk_dialog_run( GTK_DIALOG( dlg ) ) == GTK_RESPONSE_OK )
    {
        char* path = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER( dlg ) );
        add_search_dir( data, path );
        g_free( path );
    }
    gtk_widget_destroy( dlg );
}

static void on_add_search_home(GtkWidget* menu, FindFile* data)
{
    add_search_dir( data, g_get_home_dir() );
}

static void on_add_search_desktop(GtkWidget* menu, FindFile* data)
{
    add_search_dir( data, vfs_get_desktop_dir() );
}

static void on_add_search_volumes(GtkWidget* menu, FindFile* data)
{
    const char* path;
    const GList* vols = vfs_volume_get_all_volumes(), *l;
    for( l = vols; l; l = l->next )
    {
        VFSVolume* vol = (VFSVolume*)l->data;
        if ( vfs_volume_is_mounted( vol ) )
        {
            path = vfs_volume_get_mount_point( vol );
            if ( path && path[0] != '\0' )
                add_search_dir( data, path );
        }
    }
}

static void on_add_search_folder( GtkWidget* btn, FindFile* data )
{
    GtkWidget* menu = gtk_menu_new();
    GtkWidget* item;
    GtkWidget* img;
    const char* dir;

    item = gtk_image_menu_item_new_with_label( _("Browse...") );
    gtk_menu_shell_append( GTK_MENU_SHELL( menu ), item );
    g_signal_connect( item, "activate", G_CALLBACK(on_add_search_browse), data );

    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append( GTK_MENU_SHELL( menu ), item );

    item = gtk_image_menu_item_new_with_label( g_get_home_dir() );
    //img = gtk_image_new_from_icon_name( "gnome-fs-directory", GTK_ICON_SIZE_MENU );
    img = xset_get_image( "gtk-directory", GTK_ICON_SIZE_MENU );
    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM( item ), img );
    gtk_menu_shell_append( GTK_MENU_SHELL( menu ), item );
    g_signal_connect( item, "activate", G_CALLBACK(on_add_search_home), data );

    if( dir = vfs_get_desktop_dir() )
    {
        item = gtk_image_menu_item_new_with_label( dir );
        //img = gtk_image_new_from_icon_name( "gnome-fs-desktop", GTK_ICON_SIZE_MENU );
        img = xset_get_image( "gtk-directory", GTK_ICON_SIZE_MENU );
        gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM( item ), img );
        gtk_menu_shell_append( GTK_MENU_SHELL( menu ), item );
        g_signal_connect( item, "activate", G_CALLBACK(on_add_search_desktop), data );
    }

    item = gtk_image_menu_item_new_with_label( _("Local Volumes") );
    //img = gtk_image_new_from_icon_name( "gnome-dev-harddisk", GTK_ICON_SIZE_MENU );
    img = xset_get_image( "gtk-harddisk", GTK_ICON_SIZE_MENU );
    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM( item ), img );
    gtk_menu_shell_append( GTK_MENU_SHELL( menu ), item );
    g_signal_connect( item, "activate", G_CALLBACK(on_add_search_volumes), data );

    /* FIXME: Add all volumes */

    /* FIXME: Add all bookmarks */

    gtk_widget_show_all( menu );
    gtk_menu_popup( GTK_MENU( menu ), NULL, NULL, (GtkMenuPositionFunc)menu_pos,
                                        btn, 0, gtk_get_current_event_time() );
}

static void on_remove_search_folder( GtkWidget* btn, FindFile* data )
{
    GtkTreeIter it;
    GtkTreeSelection* sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( data->places_view ) );
    if( gtk_tree_selection_get_selected(sel, NULL, &it) )
        gtk_list_store_remove( data->places_list, &it );
}

static void on_date_limit_changed( GtkWidget* date_limit, FindFile* data )
{
    int sel = gtk_combo_box_get_active( (GtkComboBox*)date_limit );
    gboolean sensitive = ( sel == 5 );  /* date range */
    gtk_widget_set_sensitive( data->date1, sensitive );
    gtk_widget_set_sensitive( data->date2, sensitive );
}

static void free_data( FindFile* data )
{
    g_slice_free( FindFile, data );
}

static void init_search_result( FindFile* data )
{
    GtkTreeIter it;
    GtkTreeViewColumn* col;
    GtkCellRenderer* render;

    gtk_tree_selection_set_mode( gtk_tree_view_get_selection((GtkTreeView*)data->result_view),
                                                 GTK_SELECTION_MULTIPLE );
    data->result_list = gtk_list_store_new( N_RES_COLS,
                                                            GDK_TYPE_PIXBUF, /* icon */
                                                            G_TYPE_STRING,  /* name */
                                                            G_TYPE_STRING, /* dir */
                                                            G_TYPE_STRING,  /* type */
                                                            G_TYPE_STRING,  /* size */
                                                            G_TYPE_STRING, /* mtime */
                                                            G_TYPE_POINTER /* VFSFileInfo */ );

    gtk_tree_view_set_model( (GtkTreeView*)data->result_view, (GtkTreeModel*)data->result_list );
    g_object_unref( data->result_list );
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title( col, _("Name") );
    render = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start( col, render, FALSE );
    gtk_tree_view_column_set_attributes( col, render, "pixbuf", COL_ICON, NULL );
    render = gtk_cell_renderer_text_new();
    g_object_set( render, "ellipsize", PANGO_ELLIPSIZE_END, NULL );
    gtk_tree_view_column_pack_start( col, render, TRUE );
    gtk_tree_view_column_set_attributes( col, render, "text", COL_NAME, NULL );
    gtk_tree_view_column_set_expand ( col, TRUE );
    gtk_tree_view_column_set_min_width( col, 200 );
    gtk_tree_view_column_set_resizable ( col, TRUE );
    gtk_tree_view_append_column( (GtkTreeView*)data->result_view, col );

    render = gtk_cell_renderer_text_new();
    g_object_set( render, "ellipsize", PANGO_ELLIPSIZE_END, NULL );
    col = gtk_tree_view_column_new_with_attributes( _("Folder"), render, "text", COL_DIR, NULL );
    gtk_tree_view_column_set_expand ( col, TRUE );
    gtk_tree_view_column_set_resizable ( col, TRUE );
    gtk_tree_view_column_set_min_width( col, 200 );
    gtk_tree_view_append_column( (GtkTreeView*)data->result_view, col );

    col = gtk_tree_view_column_new_with_attributes( _("Size"), gtk_cell_renderer_text_new(), "text", COL_SIZE, NULL );
    gtk_tree_view_column_set_resizable ( col, TRUE );
    gtk_tree_view_append_column( (GtkTreeView*)data->result_view, col );

    col = gtk_tree_view_column_new_with_attributes( _("Type"), gtk_cell_renderer_text_new(), "text", COL_TYPE, NULL );
    gtk_tree_view_column_set_sizing( col, GTK_TREE_VIEW_COLUMN_FIXED );
    gtk_tree_view_column_set_fixed_width ( col, 120 );
    gtk_tree_view_column_set_resizable ( col, TRUE );
    gtk_tree_view_append_column( (GtkTreeView*)data->result_view, col );

    col = gtk_tree_view_column_new_with_attributes( _("Last Modified"), gtk_cell_renderer_text_new(), "text", COL_MTIME, NULL );
    gtk_tree_view_column_set_resizable ( col, TRUE );
    gtk_tree_view_append_column( (GtkTreeView*)data->result_view, col );
}

static gboolean on_view_button_press( GtkTreeView* view, GdkEventButton* evt, FindFile* data )
{
    if( evt->type == GDK_BUTTON_PRESS )
    {
        if( evt->button == 3 ) /* right single click */
        {
            //sfm if current item not selected, unselect all and select it
            GtkTreePath *tree_path;
            GtkTreeSelection* tree_sel;
            gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW( view ),
                                           evt->x, evt->y, &tree_path, NULL, NULL, NULL );
            tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( view ) );

            if ( tree_path && tree_sel && 
                        !gtk_tree_selection_path_is_selected( tree_sel, tree_path ) )
            {
                gtk_tree_selection_unselect_all( tree_sel );
                gtk_tree_selection_select_path( tree_sel, tree_path );
            }
            gtk_tree_path_free( tree_path );
            
            GtkWidget* popup;
            GtkUIManager* menu_mgr;
            GtkActionGroup* action_group = gtk_action_group_new ("PopupActions");
            gtk_action_group_set_translation_domain( action_group, GETTEXT_PACKAGE );
            menu_mgr = gtk_ui_manager_new ();

            gtk_action_group_add_actions( action_group, menu_actions, G_N_ELEMENTS(menu_actions), data );
            gtk_ui_manager_insert_action_group( menu_mgr, action_group, 0 );
            gtk_ui_manager_add_ui_from_string( menu_mgr, menu_def, -1, NULL );

            popup = gtk_ui_manager_get_widget( menu_mgr, "/Popup" );
            g_object_unref( action_group );
            gtk_menu_popup( GTK_MENU( popup ), NULL, NULL, NULL, NULL, evt->button, evt->time );

            /* clean up */
            g_signal_connect( popup, "selection-done", G_CALLBACK(gtk_widget_destroy), NULL );
            g_object_weak_ref( G_OBJECT( popup ), (GWeakNotify)g_object_unref, menu_mgr );

            return TRUE;
        }
    }
    else if( evt->type == GDK_2BUTTON_PRESS )
    {
        if( evt->button == 1 )  /* left double click */
        {
            on_open_files( NULL, data );
            return TRUE;
        }
    }
    return FALSE;
}

void on_use_size_lower_toggled( GtkWidget* widget, FindFile* data )
{
    gtk_widget_set_sensitive( data->size_lower,
                    gtk_toggle_button_get_active( 
                    GTK_TOGGLE_BUTTON( data->use_size_lower ) ) );
    gtk_widget_set_sensitive( data->size_lower_unit, 
                    gtk_toggle_button_get_active( 
                    GTK_TOGGLE_BUTTON( data->use_size_lower ) ) );
}

void on_use_size_upper_toggled( GtkWidget* widget, FindFile* data )
{
    gtk_widget_set_sensitive( data->size_upper,
                    gtk_toggle_button_get_active( 
                    GTK_TOGGLE_BUTTON( data->use_size_upper ) ) );
    gtk_widget_set_sensitive( data->size_upper_unit, 
                    gtk_toggle_button_get_active( 
                    GTK_TOGGLE_BUTTON( data->use_size_upper ) ) );
}

void fm_find_files( const char** search_dirs )
{
    FindFile* data = g_slice_new0(FindFile);
    GtkTreeIter it;
    GtkTreeViewColumn* col;
    GtkWidget *add_folder_btn, *remove_folder_btn, *img;

#if GTK_CHECK_VERSION (3, 0, 0)
    GtkBuilder* builder = _gtk_builder_new_from_file( PACKAGE_UI_DIR "/find-files2.ui", NULL );
#else
    GtkBuilder* builder = _gtk_builder_new_from_file( PACKAGE_UI_DIR "/find-files.ui", NULL );
#endif
    data->win = (GtkWidget*)gtk_builder_get_object( builder, "win" );
    g_object_set_data_full( G_OBJECT( data->win ), "find-files", data, (GDestroyNotify)free_data );

    GdkPixbuf* icon = NULL;
    GtkIconTheme* theme = gtk_icon_theme_get_default();
    if ( theme )
        icon = gtk_icon_theme_load_icon( theme, "spacefm-find", 48, 0, NULL );
    if ( icon )
    {
        gtk_window_set_icon( GTK_WINDOW( data->win ), icon );
        g_object_unref( icon );
    }
    else
        gtk_window_set_icon_name( GTK_WINDOW( data->win ), GTK_STOCK_FIND );

    /* search criteria pane */
    data->search_criteria = (GtkWidget*)gtk_builder_get_object( builder, "search_criteria" );

    data->fn_pattern = (GtkWidget*)gtk_builder_get_object( builder, "fn_pattern" );
    data->fn_pattern_entry = gtk_bin_get_child( GTK_BIN( data->fn_pattern ) );
    data->fn_case_sensitive = (GtkWidget*)gtk_builder_get_object( builder, "fn_case_sensitive" );
    gtk_entry_set_activates_default( (GtkEntry*)data->fn_pattern_entry, TRUE );

    /* file content */
    data->fc_pattern = (GtkWidget*)gtk_builder_get_object( builder, "fc_pattern" );
    data->fc_case_sensitive = (GtkWidget*)gtk_builder_get_object( builder, "fc_case_sensitive" );
    data->fc_use_regexp = (GtkWidget*)gtk_builder_get_object( builder, "fc_use_regexp" );

    /* advanced options */
    data->search_hidden = (GtkWidget*)gtk_builder_get_object( builder, "search_hidden" );

    /* size & date */
    data->use_size_lower = (GtkWidget*)gtk_builder_get_object( builder, "use_size_lower" );
    data->use_size_upper = (GtkWidget*)gtk_builder_get_object( builder, "use_size_upper" );
    data->size_lower = (GtkWidget*)gtk_builder_get_object( builder, "size_lower" );
    data->size_upper = (GtkWidget*)gtk_builder_get_object( builder, "size_upper" );
    data->size_lower_unit = (GtkWidget*)gtk_builder_get_object( builder, "size_lower_unit" );
    data->size_upper_unit = (GtkWidget*)gtk_builder_get_object( builder, "size_upper_unit" );
    g_signal_connect( data->use_size_lower, "toggled",
                            G_CALLBACK( on_use_size_lower_toggled ), data );
    g_signal_connect( data->use_size_upper, "toggled",
                            G_CALLBACK( on_use_size_upper_toggled ), data );
    on_use_size_lower_toggled( data->use_size_lower, data );
    on_use_size_upper_toggled( data->use_size_upper, data );

    data->date_limit = (GtkWidget*)gtk_builder_get_object( builder, "date_limit" );
    data->date1 = (GtkWidget*)gtk_builder_get_object( builder, "date1" );
    data->date2 = (GtkWidget*)gtk_builder_get_object( builder, "date2" );
    g_signal_connect( data->date_limit, "changed", G_CALLBACK( on_date_limit_changed ), data );

    /* file types */
    data->all_files = (GtkWidget*)gtk_builder_get_object( builder, "all_files" );
    data->text_files = (GtkWidget*)gtk_builder_get_object( builder, "text_files" );
    data->img_files = (GtkWidget*)gtk_builder_get_object( builder, "img_files" );
    data->audio_files = (GtkWidget*)gtk_builder_get_object( builder, "audio_files" );
    data->video_files = (GtkWidget*)gtk_builder_get_object( builder, "video_files" );

    /* places */
    data->places_list = gtk_list_store_new( 1, G_TYPE_STRING );
    data->places_view = (GtkWidget*)gtk_builder_get_object( builder, "places_view" );
    add_folder_btn = (GtkWidget*)gtk_builder_get_object( builder, "add_folder_btn" );
    remove_folder_btn = (GtkWidget*)gtk_builder_get_object( builder, "remove_folder_btn" );
    data->include_sub = (GtkWidget*)gtk_builder_get_object( builder, "include_sub" );

    if( search_dirs )
    {
        const char** dir;
        for( dir = search_dirs; *dir; ++dir )
        {
            if( g_file_test( *dir, G_FILE_TEST_IS_DIR ) )
                gtk_list_store_insert_with_values( data->places_list, &it, 0, 0, *dir, -1 );
        }
    }

    gtk_tree_view_set_model( (GtkTreeView*)data->places_view, (GtkTreeModel*)data->places_list );
    g_object_unref( data->places_list );
    col = gtk_tree_view_column_new_with_attributes(NULL, gtk_cell_renderer_text_new(), "text", 0, NULL );
    gtk_tree_view_append_column( (GtkTreeView*)data->places_view, col );

    g_signal_connect(add_folder_btn, "clicked", G_CALLBACK( on_add_search_folder ), data );
    g_signal_connect(remove_folder_btn, "clicked", G_CALLBACK( on_remove_search_folder ), data );

    /* search result pane */
    data->search_result = (GtkWidget*)gtk_builder_get_object( builder, "search_result" );
    /* replace the problematic GtkTreeView with ExoTreeView */
    data->result_view = exo_tree_view_new();
    if( app_settings.single_click )
    {
        exo_tree_view_set_single_click( EXO_TREE_VIEW( data->result_view ), TRUE );
        exo_tree_view_set_single_click_timeout( EXO_TREE_VIEW( data->result_view ),
                                                        SINGLE_CLICK_TIMEOUT );
    }
    gtk_widget_show( data->result_view );
    gtk_container_add( (GtkContainer*)gtk_builder_get_object(builder, "result_scroll"), data->result_view );
    init_search_result( data );
    g_signal_connect(data->result_view, "button-press-event", G_CALLBACK( on_view_button_press ), data );

    /* buttons */
    data->start_btn = (GtkWidget*)gtk_builder_get_object( builder, "start_btn" );
    data->stop_btn = (GtkWidget*)gtk_builder_get_object( builder, "stop_btn" );
    data->again_btn = (GtkWidget*)gtk_builder_get_object( builder, "again_btn" );
    img = gtk_image_new_from_icon_name( GTK_STOCK_REFRESH, GTK_ICON_SIZE_BUTTON );
    gtk_button_set_image( (GtkButton*)data->again_btn, img );

    g_signal_connect(data->start_btn, "clicked", G_CALLBACK( on_start_search ), data );
    g_signal_connect(data->stop_btn, "clicked", G_CALLBACK( on_stop_search ), data );
    g_signal_connect(data->again_btn, "clicked", G_CALLBACK( on_search_again ), data );


    gtk_entry_set_text( (GtkEntry*)data->fn_pattern_entry, "*" );
    gtk_editable_select_region( (GtkEditable*)data->fn_pattern_entry, 0, -1 );

    gtk_combo_box_set_active( (GtkComboBox*)data->size_lower_unit, 1 );
    gtk_spin_button_set_range( (GtkSpinButton*)data->size_lower, 0, G_MAXINT );
    gtk_combo_box_set_active( (GtkComboBox*)data->size_upper_unit, 2 );
    gtk_spin_button_set_range( (GtkSpinButton*)data->size_upper, 0, G_MAXINT );

    gtk_combo_box_set_active( (GtkComboBox*)data->date_limit, 0 );

    g_signal_connect( data->win, "delete-event", G_CALLBACK(gtk_widget_destroy), NULL );

    pcmanfm_ref();
    g_signal_connect( data->win, "destroy", G_CALLBACK(pcmanfm_unref), NULL );

    int width = xset_get_int( "main_search", "x" );
    int height = xset_get_int( "main_search", "y" );
    if ( width && height )
        gtk_window_set_default_size( GTK_WINDOW( data->win ), width, height );
    
    gtk_widget_show( data->win );

}
