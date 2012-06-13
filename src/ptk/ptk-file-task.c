/*
*  C Implementation: ptk-file-task
*
* Description:
*
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include <glib.h>
#include <glib/gi18n.h>
#include "glib-mem.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "ptk-file-task.h"
#include "ptk-utils.h"
#include "vfs-file-info.h"  //MOD
#include "main-window.h"

// waitpid
#include <unistd.h>
#include <sys/wait.h>

static void on_vfs_file_task_progress_cb( VFSFileTask* task,
                                          int percent,
                                          const char* src_file,
                                          const char* dest_file,
                                          gpointer user_data );

static gboolean on_vfs_file_task_state_cb( VFSFileTask* task,
                                           VFSFileTaskState state,
                                           gpointer state_data,
                                           gpointer user_data );

static gboolean query_overwrite( PtkFileTask* task, char** new_dest );

static void enter_callback( GtkEntry* entry, GtkDialog* dlg );   //MOD

PtkFileTask* ptk_file_exec_new( const char* item_name, const char* dir,
                                    GtkWidget* parent, GtkWidget* task_view )
{
    GList* files = NULL;
    GtkWidget* parent_win = NULL;
    if ( parent )
        parent_win = gtk_widget_get_toplevel( GTK_WIDGET( parent ) );
    char* file = g_strdup( item_name );
    files = g_list_prepend( files, file );
    return ptk_file_task_new( VFS_FILE_TASK_EXEC, files, dir,
                            GTK_WINDOW( parent_win ), task_view );
}

PtkFileTask* ptk_file_task_new( VFSFileTaskType type,
                                GList* src_files,
                                const char* dest_dir,
                                GtkWindow* parent_window,
                                GtkWidget* task_view )
{
    PtkFileTask * task = g_slice_new0( PtkFileTask );
    task->task = vfs_task_new( type, src_files, dest_dir );
    vfs_file_task_set_progress_callback( task->task,
                                         on_vfs_file_task_progress_cb,
                                         task );
    vfs_file_task_set_state_callback( task->task,
                                      on_vfs_file_task_state_cb, task );
    task->parent_window = parent_window;
    task->task_view = task_view;
    task->complete = FALSE;
    task->aborted = FALSE;
    task->keep_dlg = FALSE;
    
    GtkTextIter iter;
    task->err_buf = gtk_text_buffer_new( NULL );
    task->mark_end = gtk_text_mark_new( NULL, FALSE );
    gtk_text_buffer_get_end_iter( task->err_buf, &iter);
    gtk_text_buffer_add_mark( task->err_buf, task->mark_end, &iter );
    if ( task->task->type == VFS_FILE_TASK_EXEC )
    {
        task->task->exec_err_buf = task->err_buf;
        task->task->exec_mark_end = task->mark_end;
    }
    
    task->task->start_time = time( NULL );
    task->task->last_time = time( NULL );
    task->task->update_time = 0;
    task->task->current_speed = 0;
    task->task->current_progress = 0;
    task->task->current_item = 0;
    
    task->old_err_count = 0;
    task->old_percent = 0;

    task->dsp_file_count = NULL;
    task->dsp_size_tally = NULL;
    task->dsp_elapsed = NULL;
    task->dsp_curspeed = NULL;
    task->dsp_curest = NULL;
    task->dsp_avgspeed = NULL;
    task->dsp_avgest = NULL;
    return task;
}

/*
void ptk_file_task_destroy_delayed( PtkFileTask* task )
{
    // in case of channel output following process exit
//printf("ptk_file_task_destroy_delayed %d\n", task->keep_dlg);
    if ( task->destroy_timer )
    {
        g_source_remove( task->destroy_timer );
        task->destroy_timer = 0;
    }
    if ( !task->keep_dlg && gtk_text_buffer_get_char_count( task->task->exec_err_buf ) )
        on_vfs_file_task_progress_cb( task->task,
                                   task->task->percent,
                                   task->task->current_file,
                                   task->old_dest_file,
                                   task );
    if ( !task->keep_dlg )
        ptk_file_task_destroy( task );
    return FALSE;
}
*/

void ptk_file_task_destroy( PtkFileTask* task )
{
//printf("ptk_file_task_destroy\n");
    if ( task->timeout )
    {
        g_source_remove( task->timeout );
        task->timeout = 0;
    }
    if ( task->exec_timer )
    {
        g_source_remove( task->exec_timer );
        task->exec_timer = 0;
    }

    main_task_view_remove_task( task );
    if ( task->progress_dlg )
    {
        gtk_widget_destroy( task->progress_dlg );
        task->progress_dlg = NULL;
    }
    if ( task->task->type == VFS_FILE_TASK_EXEC )
    {
//printf("    g_io_channel_shutdown\n");
        // channel shutdowns are needed to stop channel reads after task ends.
        // Can't be placed in cb_exec_child_watch because it causes single
        // line output to be lost
        if ( task->task->exec_channel_out )
            g_io_channel_shutdown( task->task->exec_channel_out, TRUE, NULL );
        if ( task->task->exec_channel_err )
            g_io_channel_shutdown( task->task->exec_channel_err, TRUE, NULL );
        task->task->exec_channel_out = task->task->exec_channel_err = 0;
//printf("    g_io_channel_shutdown DONE\n");
    }

    if ( task->task )
        vfs_file_task_free( task->task );
        
    gtk_text_buffer_set_text( task->err_buf, "", -1 );
    g_object_unref( task->err_buf );
    
    if ( task->dsp_file_count )
        g_free( task->dsp_file_count );
    if ( task->dsp_size_tally )
        g_free( task->dsp_size_tally );
    if ( task->dsp_elapsed )
        g_free( task->dsp_elapsed );
    if ( task->dsp_curspeed )
        g_free( task->dsp_curspeed );
    if ( task->dsp_curest )
        g_free( task->dsp_curest );
    if ( task->dsp_avgspeed )
        g_free( task->dsp_avgspeed );
    if ( task->dsp_avgest )
        g_free( task->dsp_avgest );
    g_slice_free( PtkFileTask, task );
//printf("ptk_file_task_destroy DONE\n");
}

void ptk_file_task_set_complete_notify( PtkFileTask* task,
                                        GFunc callback,
                                        gpointer user_data )
{
    task->complete_notify = callback;
    task->user_data = user_data;
}

gboolean on_exec_timer( PtkFileTask* task )
{
    if ( !task || task->complete )
    {
        g_source_remove( task->exec_timer );
        task->exec_timer = 0;
        return FALSE;
    }
    if ( task->progress_dlg )
        gtk_progress_bar_pulse( task->progress );
    on_vfs_file_task_progress_cb( task->task,
                                   task->task->percent,
                                   task->task->current_file,
                                   task->old_dest_file,
                                   task ); // race condition with non-exec tasks?
    return TRUE;
}

gboolean ptk_file_task_add_main( PtkFileTask* task )
{
    main_task_view_update_task( task );

    if ( task->timeout )
    {
        g_source_remove( task->timeout );
        task->timeout = 0;
    }

    if ( ( task->task->type != VFS_FILE_TASK_EXEC
                    && task->old_err_count != task->task->err_count )
                || xset_get_b( "task_pop_all" ) || task->task->exec_popup )
    {
        ptk_file_task_progress_open( task );
        if ( task->progress_dlg )
            gtk_window_present( GTK_WINDOW( task->progress_dlg ) );
    }

    return FALSE;
}

void ptk_file_task_run( PtkFileTask* task )
{
    task->timeout = g_timeout_add( 500,
                                   ( GSourceFunc ) ptk_file_task_add_main, task );
    vfs_file_task_run( task->task );
    if ( task->task->type == VFS_FILE_TASK_EXEC )
    {
        if ( task->complete && task->timeout )
        {
            g_source_remove( task->timeout );
            task->timeout = 0;
        }
        if ( !task->complete && task->task->exec_sync )
            task->exec_timer = g_timeout_add( 200, ( GSourceFunc ) on_exec_timer, task );
        else if ( !task->task->exec_sync )
            on_vfs_file_task_state_cb( task->task, VFS_FILE_TASK_FINISH, NULL, task );
    }
}

gboolean ptk_file_task_kill( gpointer pid )
{
//printf("SIGKILL %d\n", GPOINTER_TO_INT( pid ) );
    kill( GPOINTER_TO_INT( pid ), SIGKILL );
    return FALSE;
}

gboolean ptk_file_task_kill_cpids( char* cpids )
{
    vfs_file_task_kill_cpids( cpids, SIGKILL );
    g_free( cpids );
    return FALSE;
}

void ptk_file_task_cancel( PtkFileTask* task )
{
    task->aborted = TRUE;
    if ( task->task->type == VFS_FILE_TASK_EXEC )
    {
        task->keep_dlg = TRUE;
        if ( task->exec_timer )
        {
            g_source_remove( task->exec_timer );
            task->exec_timer = 0;
        }
    
        vfs_file_task_abort( task->task );
        
        if ( task->task->exec_pid )
        {
            //printf("SIGTERM %d\n", task->task->exec_pid );
            char* cpids = vfs_file_task_get_cpids( task->task->exec_pid );
            kill( task->task->exec_pid, SIGTERM );
            if ( cpids )
                vfs_file_task_kill_cpids( cpids, SIGTERM );
            // SIGKILL 2 seconds later in case SIGTERM fails
            g_timeout_add( 2500, ( GSourceFunc ) ptk_file_task_kill,
                                        GINT_TO_POINTER( task->task->exec_pid ) );
            if ( cpids )
                g_timeout_add( 2500, ( GSourceFunc ) ptk_file_task_kill_cpids,
                                                    cpids );

            // other user run - need to kill as other
            char* gsu;
            if ( task->task->exec_as_user && geteuid() != 0 && ( gsu = get_valid_gsu() ) )
            {
                char* cmd;
                
                // remove files
                char* rm_cmd;
                if ( task->task->exec_script )
                    rm_cmd = g_strdup_printf( " ; rm -f %s",
                                        task->task->exec_script );
                else
                    rm_cmd = g_strdup_printf( "" );

                // kill command
                if ( cpids )
                {
                    // convert linefeeds to spaces
                    char* scpids = g_strdup( cpids );
                    char* lf;
                    while ( lf = strchr( scpids, '\n' ) )
                        lf[0] = ' ';
                        
                    cmd = g_strdup_printf( "/bin/kill %d %s ; sleep 3 ; /bin/kill -s KILL %d %s %s",  task->task->exec_pid, scpids, task->task->exec_pid, scpids, rm_cmd );
                    g_free( scpids );
                }
                else
                    cmd = g_strdup_printf( "/bin/kill %d ; sleep 3 ; /bin/kill -s KILL %d %s", task->task->exec_pid, task->task->exec_pid, rm_cmd );
                g_free( rm_cmd );

                PtkFileTask* task2 = ptk_file_exec_new( _("Kill As Other"), NULL,
                                GTK_WIDGET( task->parent_window ), task->task_view );
                task2->task->exec_command = cmd;
                task2->task->exec_as_user = g_strdup( task->task->exec_as_user );
                task2->task->exec_sync = FALSE;
                task2->task->exec_browser = task->task->exec_browser;
                ptk_file_task_run( task2 );                
            }
        }
//printf("ptk_file_task_cancel\n    g_io_channel_shutdown\n");
        // channel shutdowns are needed to stop channel reads after task ends.
        // Can't be placed in cb_exec_child_watch because it causes single
        // line output to be lost
        if ( task->task->exec_channel_out )
            g_io_channel_shutdown( task->task->exec_channel_out, TRUE, NULL );
        if ( task->task->exec_channel_err )
            g_io_channel_shutdown( task->task->exec_channel_err, TRUE, NULL );
        task->task->exec_channel_out = task->task->exec_channel_err = 0;
//printf("    g_io_channel_shutdown DONE\n");
    }
    else
        vfs_file_task_try_abort( task->task );
}

void on_progress_dlg_response( GtkDialog* dlg, int response, PtkFileTask* task )
{
    if ( task->complete && !task->complete_notify )
    {
        ptk_file_task_destroy( task );
        return;
    }
    switch ( response )
    {
    case GTK_RESPONSE_CANCEL:
        task->keep_dlg = FALSE;
        gtk_widget_destroy( task->progress_dlg );
        task->progress_dlg = NULL;
        ptk_file_task_cancel( task );
        break;
    case GTK_RESPONSE_OK:
    case GTK_RESPONSE_NONE:
        task->keep_dlg = FALSE;
        gtk_widget_destroy( task->progress_dlg );
        task->progress_dlg = NULL;
        break;
    }
}

void on_progress_dlg_destroy( GtkDialog* dlg, PtkFileTask* task )
{
    char* s;
    
    s = g_strdup_printf( "%d", GTK_WIDGET( dlg ) ->allocation.width );
    if ( task->task->type == VFS_FILE_TASK_EXEC )
        xset_set( "task_pop_top", "s", s );
    else
        xset_set( "task_pop_top", "x", s );
    g_free( s );

    s = g_strdup_printf( "%d", GTK_WIDGET( dlg ) ->allocation.height );
    if ( task->task->type == VFS_FILE_TASK_EXEC )
        xset_set( "task_pop_top", "z", s );
    else
        xset_set( "task_pop_top", "y", s );
    g_free( s );

    task->progress_dlg = NULL;
}

void on_view_popup( GtkTextView *entry, GtkMenu *menu, gpointer user_data )
{
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    xset_context_new();

    XSet* set = xset_get( "sep_v9" );
    set->browser = NULL;
    set->desktop = NULL;
    xset_add_menuitem( NULL, NULL, GTK_WIDGET( menu ), accel_group, set );
    set = xset_get( "task_pop_font" );
    set->browser = NULL;
    set->desktop = NULL;
    xset_add_menuitem( NULL, NULL, GTK_WIDGET( menu ), accel_group, set );
    gtk_widget_show_all( GTK_WIDGET( menu ) );
    g_signal_connect( menu, "key-press-event",
                      G_CALLBACK( xset_menu_keypress ), NULL );
}

void ptk_file_task_progress_open( PtkFileTask* task )
{
    GtkTable* table;
    GtkLabel* label;
    GdkPixbuf* pixbuf;

    const char * actions[] =
        {
            N_( "Move: " ),
            N_( "Copy: " ),
            N_( "Trash: " ),
            N_( "Delete: " ),
            N_( "Link: " ),
            N_( "Change: " ),
            N_( "Run: " )
        };
    const char* titles[] =
        {
            N_( "Moving..." ),
            N_( "Copying..." ),
            N_( "Trashing..." ),
            N_( "Deleting..." ),
            N_( "Linking..." ),
            N_( "Changing..." ),
            N_( "Running..." )
        };

    if ( task->progress_dlg )
        return;

//printf("ptk_file_task_progress_open\n");

    task->progress_dlg = gtk_dialog_new_with_buttons(
                             _( titles[ task->task->type ] ),
                             NULL /*was task->parent_window*/ , 0,
                             NULL );

    task->progress_btn_stop = gtk_button_new_from_stock( GTK_STOCK_STOP );
    gtk_dialog_add_action_widget( GTK_DIALOG( task->progress_dlg ),
                                                    task->progress_btn_stop,
                                                    GTK_RESPONSE_CANCEL);
    task->progress_btn_close = gtk_button_new_from_stock( GTK_STOCK_CLOSE );
    gtk_dialog_add_action_widget( GTK_DIALOG( task->progress_dlg ),
                                                    task->progress_btn_close,
                                                    GTK_RESPONSE_OK);

    if ( task->task->type != VFS_FILE_TASK_EXEC )
        table = GTK_TABLE(gtk_table_new( 5, 2, FALSE ));
    else
        table = GTK_TABLE(gtk_table_new( 3, 2, FALSE ));
    gtk_container_set_border_width( GTK_CONTAINER ( table ), 5 );
    gtk_table_set_row_spacings( table, 6 );
    gtk_table_set_col_spacings( table, 4 );
    int row = 0;
    
    /* From: */
    label = GTK_LABEL(gtk_label_new( _( actions[ task->task->type ] ) ));
    gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0.5 );
    gtk_table_attach( table,
                      GTK_WIDGET(label),
                      0, 1, row, row+1, GTK_FILL, 0, 0, 0 );
    task->from = GTK_LABEL(gtk_label_new( task->task->current_file ));
    gtk_misc_set_alignment( GTK_MISC ( task->from ), 0, 0.5 );
    gtk_label_set_ellipsize( task->from, PANGO_ELLIPSIZE_MIDDLE );
    gtk_label_set_selectable( task->from, TRUE );
    gtk_table_attach( table,
                      GTK_WIDGET( task->from ),
                      1, 2, row, row+1, GTK_FILL | GTK_EXPAND, 0, 0, 0 );

    if ( task->task->type != VFS_FILE_TASK_EXEC )
    {
        if ( task->task->dest_dir )
        {
            /* To: <Destination folder>
            ex. Copy file to..., Move file to...etc. */
            row++;
            label = GTK_LABEL(gtk_label_new( _( "To:" ) ));
            gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0.5 );
            gtk_table_attach( table,
                              GTK_WIDGET(label),
                              0, 1, row, row+1, GTK_FILL, 0, 0, 0 );
            task->to = GTK_LABEL(gtk_label_new( task->task->dest_dir ));
            gtk_misc_set_alignment( GTK_MISC ( task->to ), 0, 0.5 );
            gtk_label_set_ellipsize( task->to, PANGO_ELLIPSIZE_MIDDLE );
            gtk_label_set_selectable( task->to, TRUE );
            gtk_table_attach( table,
                              GTK_WIDGET( task->to ),
                              1, 2, row, row+1, GTK_FILL, 0, 0, 0 );
        }
        
        // Stats
        row++;
        label = GTK_LABEL(gtk_label_new( _( "Progress:  " ) ));
        gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0.5 );
        gtk_table_attach( table,
                          GTK_WIDGET(label),
                          0, 1, row, row+1, GTK_FILL, 0, 0, 0 );
        task->current = GTK_LABEL(gtk_label_new( "" ));
        gtk_misc_set_alignment( GTK_MISC ( task->current ), 0, 0.5 );
        gtk_label_set_ellipsize( task->current, PANGO_ELLIPSIZE_MIDDLE );
        gtk_label_set_selectable( task->current, TRUE );
        gtk_table_attach( table,
                          GTK_WIDGET( task->current ),
                          1, 2, row, row+1, GTK_FILL, 0, 0, 0 );
    }
    
    /* Processing: */
    /* Processing: <Name of currently proccesed file> */
/*    label = GTK_LABEL(gtk_label_new( _( "Processing:" ) ));
    gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0.5 );
    gtk_table_attach( table,
                      GTK_WIDGET(label),
                      0, 1, 2, 3, GTK_FILL, 0, 0, 0 );
*/  //MOD
    /* Preparing to do some file operation (Copy, Move, Delete...) */
/*    task->current = GTK_LABEL(gtk_label_new( _( "Preparing..." ) ));
    gtk_label_set_ellipsize( task->current, PANGO_ELLIPSIZE_MIDDLE );
    gtk_misc_set_alignment( GTK_MISC ( task->current ), 0, 0.5 );
    gtk_table_attach( table,
                      GTK_WIDGET( task->current ),
                      1, 2, 2, 3, GTK_FILL, 0, 0, 0 );
*/  //MOD

    // Status
    row++;
    label = GTK_LABEL(gtk_label_new( _( "Status:  " ) ));
    gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0.5 );
    gtk_table_attach( table,
                      GTK_WIDGET(label),
                      0, 1, row, row+1, GTK_FILL, 0, 0, 0 );
    task->errors = GTK_LABEL(gtk_label_new( _("Running...") ));
    gtk_misc_set_alignment( GTK_MISC ( task->errors ), 0, 0.5 );
    gtk_label_set_ellipsize( task->errors, PANGO_ELLIPSIZE_MIDDLE );
    gtk_label_set_selectable( task->errors, TRUE );
    gtk_table_attach( table,
                      GTK_WIDGET( task->errors ),
                      1, 2, row, row+1, GTK_FILL | GTK_EXPAND, 0, 0, 0 );

    /* Progress: */
    row++;
    //label = GTK_LABEL(gtk_label_new( _( "Progress:" ) ));
    //gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0.5 );
    //gtk_table_attach( table,
    //                  GTK_WIDGET(label),
    //                  0, 1, 3, 4, GTK_FILL, 0, 0, 0 );
    task->progress = GTK_PROGRESS_BAR(gtk_progress_bar_new());
    gtk_progress_bar_set_pulse_step( task->progress, 0.08 );
    gtk_table_attach( table,
                      GTK_WIDGET( task->progress ),
                      0, 2, row, row+1, GTK_FILL | GTK_EXPAND, 0, 0, 0 );

    // Error log
    task->scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ) );
    task->error_view = gtk_text_view_new_with_buffer( task->err_buf );
    gtk_container_add ( GTK_CONTAINER ( task->scroll ), task->error_view );
    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( task->scroll ),
                                     GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_text_view_set_wrap_mode( GTK_TEXT_VIEW( task->error_view ), GTK_WRAP_WORD_CHAR );
    gtk_text_view_set_editable( GTK_TEXT_VIEW( task->error_view ), FALSE );
    if ( !task->task->exec_scroll_lock )
    {
        gtk_text_view_scroll_to_mark( GTK_TEXT_VIEW( task->error_view ), task->mark_end,
                                                            0.0, FALSE, 0, 0 );
    }
    char* fontname = xset_get_s( "task_pop_font" );
    if ( fontname )
    {
        PangoFontDescription* font_desc = pango_font_description_from_string( fontname );
        gtk_widget_modify_font( task->error_view, font_desc );
        pango_font_description_free( font_desc );
    }
    g_signal_connect( task->error_view, "populate-popup", G_CALLBACK(on_view_popup), NULL );
    GtkWidget* align = gtk_alignment_new( 1, 1, 1 ,1 );
    gtk_alignment_set_padding( GTK_ALIGNMENT( align ), 0, 0, 5, 5 );
    gtk_container_add ( GTK_CONTAINER ( align ), GTK_WIDGET( task->scroll ) );

    // Pack
    gtk_box_pack_start( GTK_BOX( GTK_DIALOG( task->progress_dlg ) ->vbox ),
                        GTK_WIDGET( table ),
                        FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( GTK_DIALOG( task->progress_dlg ) ->vbox ),
                        GTK_WIDGET( align ),
                        TRUE, TRUE, 0 );

    int win_width, win_height;
    if ( task->task->type == VFS_FILE_TASK_EXEC )
    {
        win_width = xset_get_int( "task_pop_top", "s" );
        win_height = xset_get_int( "task_pop_top", "z" );
    }
    else
    {
        win_width = xset_get_int( "task_pop_top", "x" );
        win_height = xset_get_int( "task_pop_top", "y" );
    }
    if ( !win_width ) win_width = 750;
    if ( !win_height ) win_height = -1;
    gtk_window_set_default_size( GTK_WINDOW( task->progress_dlg ),
                                 win_width, win_height );
    gtk_button_box_set_layout ( GTK_BUTTON_BOX ( GTK_DIALOG( task->progress_dlg ) ->action_area ),
                                GTK_BUTTONBOX_END );
    if ( xset_get_b( "task_pop_top" ) )
        gtk_window_set_type_hint ( GTK_WINDOW ( task->progress_dlg ),
                                   GDK_WINDOW_TYPE_HINT_DIALOG );
    else
        gtk_window_set_type_hint ( GTK_WINDOW ( task->progress_dlg ),
                                   GDK_WINDOW_TYPE_HINT_NORMAL );
    gtk_window_set_gravity ( GTK_WINDOW ( task->progress_dlg ),
                             GDK_GRAVITY_NORTH_EAST );

//    gtk_dialog_set_default_response( task->progress_dlg, GTK_RESPONSE_OK );
    g_signal_connect( task->progress_dlg, "response",
                      G_CALLBACK( on_progress_dlg_response ), task );
    g_signal_connect( task->progress_dlg, "destroy",
                      G_CALLBACK( on_progress_dlg_destroy ), task );

    gtk_widget_show_all( task->progress_dlg );
    gtk_widget_grab_focus( task->progress_btn_close );

    // icon
    if ( ( task->task->type != VFS_FILE_TASK_EXEC && task->old_err_count )
                                                || task->task->exec_is_error )
        pixbuf = gtk_icon_theme_load_icon( gtk_icon_theme_get_default(),
                            "error", 16, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );
    else if ( task->task->type == 0 || task->task->type == 1 || task->task->type == 4 )
        pixbuf = gtk_icon_theme_load_icon( gtk_icon_theme_get_default(),
                            "stock_copy", 16, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );
    else if ( task->task->type == 2 || task->task->type == 3 )
        pixbuf = gtk_icon_theme_load_icon( gtk_icon_theme_get_default(),
                            "stock_delete", 16, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );
    else if ( task->task->type == VFS_FILE_TASK_EXEC && task->task->exec_icon )
        pixbuf = gtk_icon_theme_load_icon( gtk_icon_theme_get_default(),
                        task->task->exec_icon, 16, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );
    else
        pixbuf = gtk_icon_theme_load_icon( gtk_icon_theme_get_default(),
                            "gtk-execute", 16, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );
    gtk_window_set_icon( GTK_WINDOW( task->progress_dlg ), pixbuf );    

    task->old_err_count = 0;
    task->task->ticks = 10000;
//printf("ptk_file_task_progress_open DONE\n");
}

void ptk_file_task_progress_update( PtkFileTask* task )
{
    char* ufile_path;
    char percent_str[ 16 ];
    char* stats;
    char* errs;
    GtkTextIter iter, siter;
    GdkPixbuf* pixbuf;

    if ( !task->progress_dlg )
        return;
//printf("ptk_file_task_progress_update\n");
    // current file
    if ( task->complete )
    {
        gtk_widget_set_sensitive( task->progress_btn_stop, FALSE );
        if ( task->task->type != VFS_FILE_TASK_EXEC )
            ufile_path = NULL;
        else if ( task->old_src_file )
            ufile_path = g_strdup( task->old_src_file );
        else
            ufile_path = NULL;        

        if ( task->aborted )
            gtk_window_set_title( GTK_WINDOW( task->progress_dlg ), _("Stopped") );                
        else
        {
            if ( ( task->task->type != VFS_FILE_TASK_EXEC && task->task->err_count )
                                                || task->task->exec_is_error )
                gtk_window_set_title( GTK_WINDOW( task->progress_dlg ), _("Errors") );
            else
                gtk_window_set_title( GTK_WINDOW( task->progress_dlg ), _("Done") );
        }
/*
        if ( task->aborted )
        {
            if ( task->task->type != VFS_FILE_TASK_EXEC || !task->old_src_file )
                ufile_path = g_strdup_printf( "( Stopped )" );
            else
                ufile_path = g_strdup_printf( "%s  ( Stopped )", task->old_src_file );
            gtk_window_set_title( task->progress_dlg, "Stopped" );                
        }
        else
        {
            if ( task->task->err_count )
            {
                if ( task->task->type != VFS_FILE_TASK_EXEC || !task->old_src_file )
                    ufile_path = g_strdup_printf( "( Errors )" );
                else
                    ufile_path = g_strdup_printf( "%s  ( Errors )", task->old_src_file );
                gtk_window_set_title( task->progress_dlg, "Errors" );
            }
            else
            {
                if ( task->task->type != VFS_FILE_TASK_EXEC || !task->old_src_file )
                    ufile_path = g_strdup_printf( "( Finished )" );
                else
                    ufile_path = g_strdup_printf( "%s  ( Finished )", task->old_src_file );
                gtk_window_set_title( task->progress_dlg, "Finished" );
            }
        }
*/
    }    
    else if ( task->old_src_file )
    {
        if ( task->task->type != VFS_FILE_TASK_EXEC )
            ufile_path = g_filename_display_basename( task->old_src_file );
        else
            ufile_path = g_strdup( task->old_src_file );
    }
    else
        ufile_path = NULL;
    gtk_label_set_text( task->from, ufile_path );
    if ( ufile_path )
        g_free( ufile_path );

    // current dest
    if ( task->old_dest_file )
    {    
        ufile_path = g_filename_display_name( task->old_dest_file );
        gtk_label_set_text( task->to, ufile_path );
        g_free( ufile_path );
    }

    // progress bar
    if ( task->task->type != VFS_FILE_TASK_EXEC )
    {
        if ( task->old_percent >= 0 )
        {
            gtk_progress_bar_set_fraction( task->progress,
                                           ( ( gdouble ) task->old_percent ) / 100 );
            g_snprintf( percent_str, 16, "%d %%", task->old_percent );
            gtk_progress_bar_set_text( task->progress, percent_str );
        }
        else
            gtk_progress_bar_set_fraction( task->progress, 0 );
    }
    else if ( task->complete )
    {
        if ( task->task->exec_is_error || task->aborted )
            gtk_progress_bar_set_fraction( task->progress, 0 );
        else
            gtk_progress_bar_set_fraction( task->progress, 1 );
    }

    // progress
    if ( task->task->type != VFS_FILE_TASK_EXEC )
    {
        if ( task->complete )
        {
            if ( xset_get_b( "task_pop_detail" ) )
                stats = g_strdup_printf( "#%s (%s) [%s] @avg %s",
                                    task->dsp_file_count, task->dsp_size_tally,
                                    task->dsp_elapsed, task->dsp_avgspeed );
            else
                stats = g_strdup_printf( "%s  (%s)",
                                    task->dsp_size_tally, task->dsp_avgspeed );
        }
        else
        {
            if ( xset_get_b( "task_pop_detail" ) )
                stats = g_strdup_printf( "#%s (%s) [%s] @cur %s (%s) @avg %s (%s)",
                                    task->dsp_file_count, task->dsp_size_tally,
                                    task->dsp_elapsed, task->dsp_curspeed,
                                    task->dsp_curest, task->dsp_avgspeed,
                                    task->dsp_avgest );
            else
                stats = g_strdup_printf( _("%s  (%s)  %s remaining"),
                                    task->dsp_size_tally, task->dsp_avgspeed,
                                    task->dsp_avgest );
        }
        gtk_label_set_text( task->current, stats );
        g_free( stats );
    }

    // error/output log
    if ( task->task->err_count != task->old_err_count )
    {
        task->old_err_count = task->task->err_count;
        
        if ( task->task->type == VFS_FILE_TASK_EXEC )
        {
            if ( gtk_text_buffer_get_line_count( task->err_buf ) > 500 )
            {
                gtk_text_buffer_get_iter_at_line( task->err_buf, &iter, 50 );
                gtk_text_buffer_get_start_iter( task->err_buf, &siter );
                gtk_text_buffer_delete( task->err_buf, &siter, &iter );
                gtk_text_buffer_get_start_iter( task->err_buf, &siter );
                gtk_text_buffer_insert( task->err_buf, &siter, _("[ SNIP - additional output above has been trimmed from this log ]\n"), -1 );
            }
        }
        else
        {
            if ( task->task->err_msgs )
            {
                gtk_text_buffer_get_iter_at_mark( task->err_buf, &iter, task->mark_end );
                gtk_text_buffer_insert( task->err_buf, &iter, task->task->err_msgs, -1 );
                g_free( task->task->err_msgs );
                task->task->err_msgs = NULL;
            }

            if ( gtk_text_buffer_get_line_count( task->err_buf ) > 500 )
            {
                gtk_text_buffer_get_iter_at_line( task->err_buf, &iter, 50 );
                gtk_text_buffer_get_start_iter( task->err_buf, &siter );
                gtk_text_buffer_delete( task->err_buf, &siter, &iter );
                gtk_text_buffer_get_start_iter( task->err_buf, &siter );
                gtk_text_buffer_insert( task->err_buf, &siter, _("[ SNIP - additional errors above have been trimmed from this log ]\n"), -1 );
            }
        }
        if ( !task->task->exec_scroll_lock )
        {
            //scroll to end if scrollbar is mostly down
            GtkAdjustment* adj =  gtk_scrolled_window_get_vadjustment (task->scroll);
            if (  adj->upper - adj->value < adj->page_size + 40 )
                gtk_text_view_scroll_to_mark( GTK_TEXT_VIEW( task->error_view ),
                                                            task->mark_end,
                                                            0.0, FALSE, 0, 0 );
        }
    }

    // error icon
    if ( ( task->task->type != VFS_FILE_TASK_EXEC && task->old_err_count )
                                                || task->task->exec_is_error )
    {
        pixbuf = gtk_icon_theme_load_icon( gtk_icon_theme_get_default(),
                            "error", 16, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );
        gtk_window_set_icon( GTK_WINDOW( task->progress_dlg ), pixbuf );    
    }

    // status
    if ( task->complete )
    {
        if ( task->aborted )
        {
            if ( task->old_err_count && task->task->type != VFS_FILE_TASK_EXEC )
            {
                if ( xset_get_b( "task_err_first" ) )
                    errs = g_strdup_printf( _("Error  ( Stop If First )") );
                else if ( xset_get_b( "task_err_any" ) )
                    errs = g_strdup_printf( _("Error  ( Stop On Any )") );
                else
                    errs = g_strdup_printf( ngettext( "Stopped with %d error",
                                                      "Stopped with %d errors",
                                                      task->old_err_count ),
                                            task->old_err_count );
            }
            else
                errs = g_strdup_printf( _("Stopped") );
        }
        else
        {
            if ( task->task->type != VFS_FILE_TASK_EXEC )
            {
                if ( task->old_err_count )
                    errs = g_strdup_printf( ngettext( "Finished with %d error",
                                                      "Finished with %d errors",
                                                      task->old_err_count ),
                                            task->old_err_count );
                else
                    errs = g_strdup_printf( _("Done") );
            }
            else
            {
                if ( task->task->exec_exit_status )
                    errs = g_strdup_printf( _("Finished with error  ( exit status %d )"),
                                        task->task->exec_exit_status );
                else if ( task->task->exec_is_error )
                    errs = g_strdup_printf( _("Finished with error") );
                else
                    errs = g_strdup_printf( _("Done") );
            }
        }
    }
    else
    {
        if ( task->task->type != VFS_FILE_TASK_EXEC )
        {
            if ( task->old_err_count )
                errs = g_strdup_printf( ngettext( "Running with %d error",
                                                  "Running with %d errors",
                                                  task->old_err_count ),
                                        task->old_err_count );
            else
                errs = g_strdup_printf( _("Running...") );
        }
        else
            errs = g_strdup_printf( _("Running...  ( pid %d )"), task->task->exec_pid );
    }
    gtk_label_set_text( task->errors, errs );
    g_free( errs );
//printf("ptk_file_task_progress_update DONE\n");
}

void ptk_file_task_set_chmod( PtkFileTask* task,
                              guchar* chmod_actions )
{
    vfs_file_task_set_chmod( task->task, chmod_actions );
}

void ptk_file_task_set_chown( PtkFileTask* task,
                              uid_t uid, gid_t gid )
{
    vfs_file_task_set_chown( task->task, uid, gid );
}

void ptk_file_task_set_recursive( PtkFileTask* task, gboolean recursive )
{
    vfs_file_task_set_recursive( task->task, recursive );
}

void on_vfs_file_task_progress_cb( VFSFileTask* task,
                                   int percent,
                                   const char* src_file,
                                   const char* dest_file,
                                   gpointer user_data )
{
    task->ticks++;
    if ( task->ticks < 2000 && !( time( NULL ) - task->update_time ) )  //at least once per second
        return;

//printf("on_vfs_file_task_progress_cb\n");
    
    if ( task->type != VFS_FILE_TASK_EXEC )
        gdk_threads_enter();
    else if ( task->flood_ticks && ( time( NULL ) - task->update_time ) )
    {
        //printf("flood_ticks = %d\n", task->flood_ticks );
        task->flood_ticks = 0;
    }
    
    task->update_time = time( NULL );
    task->ticks = 0;
    PtkFileTask* data = ( PtkFileTask* ) user_data;

    if ( task->type != VFS_FILE_TASK_EXEC )
    {
        // calc percent
        gdouble dpercent = ( ( gdouble ) task->progress ) / task->total_size;
        int ipercent = ( int ) ( dpercent * 100 );

        if ( ipercent != task->percent )
            task->percent = ipercent;

        /* update current src file */
        if ( data->old_src_file != src_file )
        {
            data->old_src_file = src_file;
            task->current_item++;
        }

        /* update current dest file */
        if ( data->old_dest_file != dest_file )
        {
            data->old_dest_file = dest_file;
        }
    }
    else if ( data->old_src_file != src_file )
    {
        data->old_src_file = src_file;
        task->current_item++;
    }
    
    
    /* update progress */
    if ( data->old_percent != task->percent )
    {
        data->old_percent = task->percent;
    }

    //elapsed
    time_t etime = time( NULL ) - task->start_time;
    guint hours = etime / 3600;
    char* elapsed;
    char* elapsed2;
    char* elapsed3;
    if ( hours == 0 )
        elapsed = g_strdup_printf( "" );
    else
        elapsed = g_strdup_printf( "%d", hours );
    guint mins = ( etime - ( hours * 3600 ) ) / 60;
    if ( hours > 0 )
        elapsed2 = g_strdup_printf( "%s:%02d", elapsed, mins );
    else if ( mins > 0 )
        elapsed2 = g_strdup_printf( "%d", mins );    
    else
        elapsed2 = g_strdup( elapsed );
    guint secs = ( etime - ( hours * 3600 ) - ( mins * 60 ) );
    elapsed3 = g_strdup_printf( "%s:%02d", elapsed2, secs );
    g_free( elapsed );
    g_free( elapsed2 );

    char* file_count;
    char* size_tally;
    char* speed1;
    char* speed2;
    char* remain1;
    char* remain2;
    if ( task->type != VFS_FILE_TASK_EXEC )
    {
        char buf1[ 64 ];
        char buf2[ 64 ];
        //count
        //char* file_count = g_strdup_printf( "%d/%d", task->current_item, g_list_length( task->src_paths ) );
        file_count = g_strdup_printf( "%d", task->current_item );
        //size
        vfs_file_size_to_string_format( buf1, task->progress, "%.0f %s" );
        vfs_file_size_to_string_format( buf2, task->total_size, "%.0f %s" );
        size_tally = g_strdup_printf( "%s / %s", buf1, buf2 );
        //cur speed (based on 2 sec interval)
        time_t cur_speed = time( NULL ) - task->last_time;
        if ( cur_speed > 1 )
        {
            //g_warning ("timediff=%f  %f", (float) cur_speed, (float) time( NULL ));
            cur_speed = ( task->progress - task->current_progress ) / cur_speed;
            task->last_time = time( NULL );
            task->current_speed = cur_speed;
            task->current_progress = task->progress;
        }
        //avg speed
        time_t avg_speed = etime;
        if ( avg_speed > 0 )
            avg_speed = task->progress / avg_speed;
        else
            avg_speed = 0;
        vfs_file_size_to_string_format( buf2, avg_speed, "%.0f %s" );
        if ( task->current_speed == 0 && task->current_progress == 0 )
            cur_speed = avg_speed;
        else
            cur_speed = task->current_speed;
        vfs_file_size_to_string_format( buf1, cur_speed, "%.0f %s" );
        if ( cur_speed == 0 )
            speed1 = g_strdup_printf( "Stalled" );
        else
            speed1 = g_strdup_printf( "%s/s", buf1 );
        speed2 = g_strdup_printf( "%s/s", buf2 );
        //remain cur
        guint remain;
        if ( cur_speed > 0 )
            remain = ( task->total_size - task->progress ) / cur_speed;
        else
            remain = 0;
        if ( remain == 0 )
            remain1 = g_strdup_printf( "n/a" );
        else if ( remain > 3599 )
        {
            hours = remain / 3600;
            if ( remain - ( hours * 3600 ) > 1799 )
                hours++;
            remain1 = g_strdup_printf( "%dh", hours );
        }
        else if ( remain > 59 )
            remain1 = g_strdup_printf( "%d:%02d", remain / 60, remain - ( (guint)( remain / 60 ) * 60 ) );
        else
            remain1 = g_strdup_printf( ":%02d", remain );
        //remain avg
        if ( avg_speed > 0 )
            remain = ( task->total_size - task->progress ) / avg_speed;
        else
            remain = 0;
        if ( remain == 0 )
            remain2 = g_strdup_printf( "n/a" );
        else if ( remain > 3599 )
        {
            hours = remain / 3600;
            if ( remain - ( hours * 3600 ) > 1799 )
                hours++;
            remain2 = g_strdup_printf( "%dh", hours );
        }
        else if ( remain > 59 )
            remain2 = g_strdup_printf( "%d:%02d", remain / 60, remain - ( (guint)( remain / 60 ) * 60 ) );
        else
            remain2 = g_strdup_printf( ":%02d", remain );
    }
    else
    {
        file_count = g_strdup_printf( "" );
        size_tally = g_strdup_printf( "" );
        speed1 = g_strdup_printf( "" );
        speed2 = g_strdup_printf( "" );
        remain1 = g_strdup_printf( "" );
        remain2 = g_strdup_printf( "" );
    }

    if ( data->dsp_file_count )
        g_free( data->dsp_file_count );
    data->dsp_file_count = file_count;
    if ( data->dsp_size_tally )
        g_free( data->dsp_size_tally );
    data->dsp_size_tally = size_tally;
    if ( data->dsp_elapsed )
        g_free( data->dsp_elapsed );
    data->dsp_elapsed = elapsed3;
    if ( data->dsp_curspeed )
        g_free( data->dsp_curspeed );
    data->dsp_curspeed = speed1;
    if ( data->dsp_curest )
        g_free( data->dsp_curest );
    data->dsp_curest = remain1;
    if ( data->dsp_avgspeed )
        g_free( data->dsp_avgspeed );
    data->dsp_avgspeed = speed2;
    if ( data->dsp_avgest )
        g_free( data->dsp_avgest );
    data->dsp_avgest = remain2;

    if ( task->type == VFS_FILE_TASK_EXEC )
    {
        if ( task->exec_show_output ) //&& !data->complete )
        {
            if ( gtk_text_buffer_get_char_count( task->exec_err_buf ) )
            {
                task->exec_show_output = FALSE; // only open once
                data->keep_dlg = TRUE;
                ptk_file_task_progress_open( data );
            }
        }
    }

    ptk_file_task_progress_update( data );
    if ( !data->timeout && !data->complete )
        main_task_view_update_task( data );

    if ( task->type != VFS_FILE_TASK_EXEC )
        gdk_threads_leave();
//printf("on_vfs_file_task_progress_cb DONE\n");
}

gboolean on_vfs_file_task_state_cb( VFSFileTask* task,
                                    VFSFileTaskState state,
                                    gpointer state_data,
                                    gpointer user_data )
{
    PtkFileTask* data = ( PtkFileTask* ) user_data;
    GtkWidget* dlg;
    int response;
    gboolean ret = TRUE;
    char** new_dest;

    if ( task->type != VFS_FILE_TASK_EXEC )
        gdk_threads_enter();

    switch ( state )
    {
    case VFS_FILE_TASK_FINISH:
        //printf("VFS_FILE_TASK_FINISH\n");
        if ( data->timeout )
        {
            g_source_remove( data->timeout );
            data->timeout = 0;
        }

        data->complete = TRUE;
        if ( data->complete_notify )
        {
            GDK_THREADS_ENTER();
            data->complete_notify( task, data->user_data );
            data->complete_notify = NULL;
            GDK_THREADS_LEAVE();
        }
        main_task_view_remove_task( data );
        
        if ( !data->progress_dlg || ( !task->err_count && !data->keep_dlg ) )
        {
            //printf("FINISH DESTROY\n");
/*
            if ( task->type == VFS_FILE_TASK_EXEC
                        && task->exec_show_output
                        && !gtk_text_buffer_get_char_count( task->exec_err_buf ) )
            {
                // in case of channel output following process exit
                data->destroy_timer = g_timeout_add( 300,
                                    ( GSourceFunc ) ptk_file_task_destroy_delayed,
                                    ( gpointer ) data );           
            }
            else
*/
                ptk_file_task_destroy( data );
        }
        else if ( data->progress_dlg )
            ptk_file_task_progress_update( data );            

        break;
    case VFS_FILE_TASK_QUERY_OVERWRITE:
        new_dest = ( char** ) state_data;
        ret = query_overwrite( data, new_dest );
        break;
    case VFS_FILE_TASK_QUERY_ABORT:
        //printf("VFS_FILE_TASK_QUERY_ABORT\n");
/*        dlg = gtk_message_dialog_new( GTK_WINDOW( data->progress_dlg ),
                                      GTK_DIALOG_MODAL,
                                      GTK_MESSAGE_QUESTION,
                                      GTK_BUTTONS_YES_NO,
                                      _( "Cancel the operation?" ) );
        response = gtk_dialog_run( GTK_DIALOG( dlg ) );
        gtk_widget_destroy( dlg );
        ret = ( response != GTK_RESPONSE_YES );
*/      
        if ( task->type == VFS_FILE_TASK_EXEC )
            ptk_file_task_cancel( data );
        ret = FALSE;
        break;
    case VFS_FILE_TASK_ERROR:
        //printf("VFS_FILE_TASK_ERROR\n");
        if ( data->timeout )
        {
            g_source_remove( data->timeout );
            data->timeout = 0;
        }
        data->old_err_count = 0;

        if ( task->type == VFS_FILE_TASK_EXEC )
        {
            task->exec_is_error = TRUE;
            if ( !data->aborted && task->exec_show_error )
            {
                data->keep_dlg = TRUE;
                ptk_file_task_progress_open( data );
            }
            ret = FALSE;
        }
        else
        {
            ptk_file_task_progress_open( data );
            if ( xset_get_b( "task_err_any" ) )
            {
                ret = FALSE;
                data->aborted = TRUE;
            }
            else if ( task->current_item < 2 )
            {
                if ( xset_get_b( "task_err_first" ) )
                {
                    ret = FALSE;
                    data->aborted = TRUE;
                }
            }
        }
        break;
    default:
        break;
    }

    if ( task->type != VFS_FILE_TASK_EXEC )
        gdk_threads_leave();

    return ret;    /* return TRUE to continue */
}


enum{
    RESPONSE_OVERWRITE = 1 << 0,
    RESPONSE_OVERWRITEALL = 1 << 1,
    RESPONSE_RENAME = 1 << 2,
    RESPONSE_SKIP = 1 << 3,
    RESPONSE_SKIPALL = 1 << 4
};

static void on_file_name_entry_changed( GtkEntry* entry, GtkDialog* dlg )
{
    const char * old_name;
    gboolean can_rename;
    const char* new_name = gtk_entry_get_text( entry );

    old_name = ( const char* ) g_object_get_data( G_OBJECT( entry ), "old_name" );
    can_rename = new_name && ( 0 != strcmp( new_name, old_name ) );

    gtk_dialog_set_response_sensitive ( dlg, RESPONSE_RENAME, can_rename );
    gtk_dialog_set_response_sensitive ( dlg, RESPONSE_OVERWRITE, !can_rename );
    gtk_dialog_set_response_sensitive ( dlg, RESPONSE_OVERWRITEALL, !can_rename );
}

gboolean query_overwrite( PtkFileTask* task, char** new_dest )
{
    const char * message;
    const char* question;
    GtkWidget* dlg;
    GtkWidget* parent_win;
    GtkEntry* entry;

    int response;
    gboolean has_overwrite_btn = TRUE;
    char* udest_file;
    char* file_name;
    char* ufile_name;
    char* dir_name;
    gboolean different_files, is_src_dir, is_dest_dir;
    struct stat src_stat, dest_stat;
    gboolean restart_timer = FALSE;

    different_files = ( 0 != strcmp( task->task->current_file,
                                     task->task->current_dest ) );

    lstat( task->task->current_file, &src_stat );
    lstat( task->task->current_dest, &dest_stat );

    is_src_dir = !!S_ISDIR( dest_stat.st_mode );
    is_dest_dir = !!S_ISDIR( src_stat.st_mode );

    if ( task->timeout )
    {
        g_source_remove( task->timeout );
        task->timeout = 0;
        restart_timer = TRUE;
    }

    if ( different_files && is_dest_dir == is_src_dir )
    {
        if ( is_dest_dir )
        {
            /* Ask the user whether to overwrite dir content or not */
            question = _( "Do you want to overwrite the folder and its content?" );
        }
        else
        {
            /* Ask the user whether to overwrite the file or not */
            question = _( "Do you want to overwrite this file?" );
        }
    }
    else
    { /* Rename is required */
        question = _( "Please choose a new file name." );
        has_overwrite_btn = FALSE;
    }

    if ( different_files )
    {
        /* Ths first %s is a file name, and the second one represents followed message.
        ex: "/home/pcman/some_file" already exists.\n\nDo you want to overwrite existing file?" */
        message = _( "\"%s\" already exists.\n\n%s" );
    }
    else
    {
        /* Ths first %s is a file name, and the second one represents followed message. */
        message = _( "\"%s\"\n\nDestination and source are the same file.\n\n%s" );
    }

    udest_file = g_filename_display_name( task->task->current_dest );
    parent_win = task->progress_dlg ? task->progress_dlg : GTK_WIDGET(task->parent_window);
    dlg = gtk_message_dialog_new( GTK_WINDOW( parent_win ),
                                  GTK_DIALOG_MODAL,
                                  GTK_MESSAGE_QUESTION,
                                  GTK_BUTTONS_NONE,
                                  message,
                                  udest_file,
                                  question );
    g_free( udest_file );

    if ( has_overwrite_btn )
    {
        gtk_dialog_add_buttons ( GTK_DIALOG( dlg ),
                                 _( "Overwrite" ), RESPONSE_OVERWRITE,
                                 _( "Overwrite All" ), RESPONSE_OVERWRITEALL,
                                 NULL );
    }

    gtk_dialog_add_buttons ( GTK_DIALOG( dlg ),
                             _( "Rename" ), RESPONSE_RENAME,
                             _( "Skip" ), RESPONSE_SKIP,
                             _( "Skip All" ), RESPONSE_SKIPALL,
                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                             NULL );
    file_name = g_path_get_basename( task->task->current_dest );
    ufile_name = g_filename_display_name( file_name );
    g_free( file_name );

    entry = ( GtkEntry* ) gtk_entry_new();
    g_object_set_data_full( G_OBJECT( entry ), "old_name",
                            ufile_name, g_free );
    g_signal_connect( G_OBJECT( entry ), "changed",
                      G_CALLBACK( on_file_name_entry_changed ), dlg );

    gtk_entry_set_text( entry, ufile_name );
	g_signal_connect (G_OBJECT (entry), "activate",
                      G_CALLBACK( enter_callback ), dlg );  //MOD added

    gtk_box_pack_start( GTK_BOX( GTK_DIALOG( dlg ) ->vbox ), GTK_WIDGET( entry ),
                        FALSE, TRUE, 4 );

    gtk_widget_show_all( dlg );
    response = gtk_dialog_run( GTK_DIALOG( dlg ) );

    switch ( response )
    {
    case RESPONSE_OVERWRITEALL:
        vfs_file_task_set_overwrite_mode( task->task, VFS_FILE_TASK_OVERWRITE_ALL );
        break;
    case RESPONSE_OVERWRITE:
        vfs_file_task_set_overwrite_mode( task->task, VFS_FILE_TASK_OVERWRITE );
        break;
    case RESPONSE_SKIPALL:
        vfs_file_task_set_overwrite_mode( task->task, VFS_FILE_TASK_SKIP_ALL );
        break;
    case RESPONSE_SKIP:
        vfs_file_task_set_overwrite_mode( task->task, VFS_FILE_TASK_SKIP );
        break;
    case RESPONSE_RENAME:
        vfs_file_task_set_overwrite_mode( task->task, VFS_FILE_TASK_RENAME );
        file_name = g_filename_from_utf8( gtk_entry_get_text( entry ),
                                          - 1, NULL, NULL, NULL );
        if ( file_name )
        {
            dir_name = g_path_get_dirname( task->task->current_dest );
            *new_dest = g_build_filename( dir_name, file_name, NULL );
            g_free( file_name );
            g_free( dir_name );
        }
        break;
    case GTK_RESPONSE_DELETE_EVENT: /* escape was pressed */
    case GTK_RESPONSE_CANCEL:
        vfs_file_task_abort( task->task );
        break;
    }
    gtk_widget_destroy( dlg );
    if( restart_timer )
    {
        task->timeout = g_timeout_add( 500,
                                    ( GSourceFunc ) ptk_file_task_add_main,
                                    ( gpointer ) task );
    }

    return (response != GTK_RESPONSE_DELETE_EVENT)
        && (response != GTK_RESPONSE_CANCEL);
}

void enter_callback( GtkEntry* entry, GtkDialog* dlg )   //MOD added
{
	// User pressed enter in rename/overwrite dialog
    const char * old_name;
    gboolean can_rename;
    const char* new_name = gtk_entry_get_text( entry );

    old_name = ( const char* ) g_object_get_data( G_OBJECT( entry ), "old_name" );
    can_rename = new_name && ( 0 != strcmp( new_name, old_name ) );
	if ( can_rename )
		gtk_dialog_response( dlg, RESPONSE_RENAME );
}

