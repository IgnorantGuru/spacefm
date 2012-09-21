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

static gboolean on_vfs_file_task_state_cb( VFSFileTask* task,
                                           VFSFileTaskState state,
                                           gpointer state_data,
                                           gpointer user_data );

static void query_overwrite( PtkFileTask* ptask, char** new_dest );

static void enter_callback( GtkEntry* entry, GtkDialog* dlg );   //MOD
void ptk_file_task_update( PtkFileTask* ptask );
//void ptk_file_task_notify_handler( GObject* o, PtkFileTask* ptask );
gboolean ptk_file_task_add_main( PtkFileTask* ptask );
void on_progress_dlg_response( GtkDialog* dlg, int response, PtkFileTask* ptask );

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
//printf("ptk_file_task_new\n");
    PtkFileTask* ptask = g_slice_new0( PtkFileTask );
    ptask->task = vfs_task_new( type, src_files, dest_dir );
    //vfs_file_task_set_progress_callback( ptask->task,
    //                                     on_vfs_file_task_progress_cb,
    //                                     ptask );
    vfs_file_task_set_state_callback( ptask->task,
                                      on_vfs_file_task_state_cb, ptask );
    ptask->parent_window = parent_window;
    ptask->task_view = task_view;
    ptask->progress_dlg = NULL;
    ptask->complete = FALSE;
    ptask->aborted = FALSE;
    ptask->pause_change = FALSE;
    ptask->keep_dlg = FALSE;
    ptask->err_count = 0;
    
    GtkTextIter iter;
    ptask->log_buf = gtk_text_buffer_new( NULL );
    ptask->log_end = gtk_text_mark_new( NULL, FALSE );
    gtk_text_buffer_get_end_iter( ptask->log_buf, &iter);
    gtk_text_buffer_add_mark( ptask->log_buf, ptask->log_end, &iter );
    ptask->log_appended = FALSE;
    ptask->restart_timeout = FALSE;

    ptask->dsp_file_count = g_strdup( "" );
    ptask->dsp_size_tally = g_strdup( "" );
    ptask->dsp_elapsed = g_strdup( "" );
    ptask->dsp_curspeed = g_strdup( "" );
    ptask->dsp_curest = g_strdup( "" );
    ptask->dsp_avgspeed = g_strdup( "" );
    ptask->dsp_avgest = g_strdup( "" );

    ptask->progress_count = 0;

    ptask->query_cond = NULL;
    ptask->query_cond_last = NULL;
    ptask->query_new_dest = NULL;

    // queue task
    if ( ptask->task->exec_sync && ptask->task->type != VFS_FILE_TASK_EXEC &&
                                   ptask->task->type != VFS_FILE_TASK_LINK &&
                                   ptask->task->type != VFS_FILE_TASK_CHMOD_CHOWN &&
                                   xset_get_b( "task_q_new" ) )
        ptk_file_task_pause( ptask, VFS_FILE_TASK_QUEUE );
    
    /*  this method doesn't work because sig handler runs in task thread
    // setup signal
    ptask->signal_widget = gtk_label_new( NULL );  // dummy object for signal
    g_signal_new( "task-notify",
                     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
                     0, NULL, NULL,
                     g_cclosure_marshal_VOID__POINTER,
                     G_TYPE_NONE, 1, G_TYPE_POINTER);
    g_signal_connect( G_OBJECT( ptask->signal_widget ), "task-notify",
                            G_CALLBACK( ptk_file_task_notify_handler ), NULL );
    */
//GThread *self = g_thread_self ();
//printf("GUI_THREAD = %#x\n", self );
//printf("ptk_file_task_new DONE ptask=%#x\n", ptask);
    return ptask;
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

void ptk_file_task_destroy( PtkFileTask* ptask )
{
//printf("ptk_file_task_destroy ptask=%#x\n", ptask);
    if ( ptask->timeout )
    {
        g_source_remove( ptask->timeout );
        ptask->timeout = 0;
    }
    if ( ptask->progress_timer )
    {
        g_source_remove( ptask->progress_timer );
        ptask->progress_timer = 0;
    }
    main_task_view_remove_task( ptask );
    task_start_queued( ptask->task_view, NULL );
    
    if ( ptask->progress_dlg )
    {
        gtk_widget_destroy( ptask->progress_dlg );
        ptask->progress_dlg = NULL;
    }
    if ( ptask->task->type == VFS_FILE_TASK_EXEC )
    {
//printf("    g_io_channel_shutdown\n");
        // channel shutdowns are needed to stop channel reads after task ends.
        // Can't be placed in cb_exec_child_watch because it causes single
        // line output to be lost
        if ( ptask->task->exec_channel_out )
            g_io_channel_shutdown( ptask->task->exec_channel_out, TRUE, NULL );
        if ( ptask->task->exec_channel_err )
            g_io_channel_shutdown( ptask->task->exec_channel_err, TRUE, NULL );
        ptask->task->exec_channel_out = ptask->task->exec_channel_err = 0;
//printf("    g_io_channel_shutdown DONE\n");
    }

    if ( ptask->task )
        vfs_file_task_free( ptask->task );

    /*
    g_signal_handlers_disconnect_by_func(
                            G_OBJECT( ptask->signal_widget ),
                            G_CALLBACK( ptk_file_task_notify_handler ), NULL );
    gtk_widget_destroy( ptask->signal_widget );
    */
    
    gtk_text_buffer_set_text( ptask->log_buf, "", -1 );
    g_object_unref( ptask->log_buf );
    
    g_free( ptask->dsp_file_count );
    g_free( ptask->dsp_size_tally );
    g_free( ptask->dsp_elapsed );
    g_free( ptask->dsp_curspeed );
    g_free( ptask->dsp_curest );
    g_free( ptask->dsp_avgspeed );
    g_free( ptask->dsp_avgest );

    g_slice_free( PtkFileTask, ptask );
//printf("ptk_file_task_destroy DONE ptask=%#x\n", ptask);
}

void ptk_file_task_set_complete_notify( PtkFileTask* ptask,
                                        GFunc callback,
                                        gpointer user_data )
{
    ptask->complete_notify = callback;
    ptask->user_data = user_data;
}

gboolean on_progress_timer( PtkFileTask* ptask )
{
    //GThread *self = g_thread_self ();
    //printf("PROGRESS_TIMER_THREAD = %#x\n", self );

    // query condition?
    if ( ptask->query_cond && ptask->query_cond != ptask->query_cond_last )
    {
        //printf("QUERY = %#x  mutex = %#x\n", ptask->query_cond, ptask->task->mutex );
        ptask->restart_timeout = ( ptask->timeout != 0 );
        if ( ptask->timeout )
        {
            g_source_remove( ptask->timeout );
            ptask->timeout = 0;
        }
        if ( ptask->progress_timer )
        {
            g_source_remove( ptask->progress_timer );
            ptask->progress_timer = 0;
        }

        g_mutex_lock( ptask->task->mutex );
        query_overwrite( ptask, ptask->query_new_dest );
        g_mutex_unlock( ptask->task->mutex );
        return FALSE;
    }

    // start new queued task
    if ( ptask->task->queue_start )
    {
        ptask->task->queue_start = FALSE;
        if ( ptask->task->state_pause == VFS_FILE_TASK_RUNNING )
            ptk_file_task_pause( ptask, VFS_FILE_TASK_RUNNING );
        else
            task_start_queued( ptask->task_view, ptask );
        if ( ptask->timeout && ptask->task->state_pause != VFS_FILE_TASK_RUNNING && 
                                    ptask->task->state == VFS_FILE_TASK_RUNNING )
        {
            // task is waiting in queue so list it
            g_source_remove( ptask->timeout );
            ptask->timeout = 0;
        }
    }
    
    // only update every 300ms (6 * 50ms)
    if ( ++ptask->progress_count < 6 )
        return TRUE;
    ptask->progress_count = 0;
//printf("on_progress_timer ptask=%#x\n", ptask);
    
    if ( ptask->complete )
    {
        if ( ptask->progress_timer )
        {
            g_source_remove( ptask->progress_timer );
            ptask->progress_timer = 0;
        }
        if ( ptask->complete_notify )
        {
            ptask->complete_notify( ptask->task, ptask->user_data );
            ptask->complete_notify = NULL;
        }
        main_task_view_remove_task( ptask );
        task_start_queued( ptask->task_view, NULL );
    }
    else if ( ptask->task->state_pause != VFS_FILE_TASK_RUNNING
                                    && !ptask->pause_change 
                                    && ptask->task->type != VFS_FILE_TASK_EXEC )
        return TRUE;
    
    ptk_file_task_update( ptask );

    if ( ptask->complete )
    {
        if ( !ptask->progress_dlg || ( !ptask->err_count && !ptask->keep_dlg ) )
        {
            ptk_file_task_destroy( ptask );
//printf("on_progress_timer DONE FALSE-COMPLETE ptask=%#x\n", ptask);
            return FALSE;
        }
    }
//printf("on_progress_timer DONE TRUE ptask=%#x\n", ptask);
    return !ptask->complete;
}

gboolean ptk_file_task_add_main( PtkFileTask* ptask )
{
//printf("ptk_file_task_add_main ptask=%#x\n", ptask);
    if ( ptask->timeout )
    {
        g_source_remove( ptask->timeout );
        ptask->timeout = 0;
    }

    if ( ptask->task->exec_popup || xset_get_b( "task_pop_all" ) )
        ptk_file_task_progress_open( ptask );

    if ( ptask->task->state_pause != VFS_FILE_TASK_RUNNING && !ptask->pause_change )
        ptask->pause_change = TRUE;
        
    on_progress_timer( ptask );
    
//printf("ptk_file_task_add_main DONE ptask=%#x\n", ptask);
    return FALSE;
}

/*
void ptk_file_task_notify_handler( GObject* o, PtkFileTask* ptask )
{
printf("ptk_file_task_notify_handler ptask=%#x\n", ptask);
    //gdk_threads_enter();
    on_progress_timer( ptask );
    //gdk_threads_leave();
}
*/

void ptk_file_task_run( PtkFileTask* ptask )
{
//printf("ptk_file_task_run ptask=%#x\n", ptask);
    // wait this long to first show task in manager, popup
    ptask->timeout = g_timeout_add( 500,
                                (GSourceFunc)ptk_file_task_add_main, ptask );
    ptask->progress_timer = 0;
    vfs_file_task_run( ptask->task );
    if ( ptask->task->type == VFS_FILE_TASK_EXEC )
    {
        if ( ( ptask->complete || !ptask->task->exec_sync ) && ptask->timeout )
        {
            g_source_remove( ptask->timeout );
            ptask->timeout = 0;
        }
    }
    ptask->progress_timer = g_timeout_add( 50, ( GSourceFunc ) on_progress_timer,
                                                                        ptask );
//printf("ptk_file_task_run DONE ptask=%#x\n", ptask);
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

gboolean ptk_file_task_cancel( PtkFileTask* ptask )
{
    //GThread *self = g_thread_self ();
    //printf("CANCEL_THREAD = %#x\n", self );
    if ( ptask->timeout )
    {
        g_source_remove( ptask->timeout );
        ptask->timeout = 0;
    }
    ptask->aborted = TRUE;
    if ( ptask->task->type == VFS_FILE_TASK_EXEC )
    {
        ptask->keep_dlg = TRUE;

        // resume task for task list responsiveness
        if ( ptask->task->state_pause != VFS_FILE_TASK_RUNNING )
            ptk_file_task_pause( ptask, VFS_FILE_TASK_RUNNING );

        vfs_file_task_abort( ptask->task );
        
        if ( ptask->task->exec_pid )
        {
            //printf("SIGTERM %d\n", ptask->task->exec_pid );
            char* cpids = vfs_file_task_get_cpids( ptask->task->exec_pid );
            kill( ptask->task->exec_pid, SIGTERM );
            if ( cpids )
                vfs_file_task_kill_cpids( cpids, SIGTERM );
            // SIGKILL 2 seconds later in case SIGTERM fails
            g_timeout_add( 2500, ( GSourceFunc ) ptk_file_task_kill,
                                        GINT_TO_POINTER( ptask->task->exec_pid ) );
            if ( cpids )
                g_timeout_add( 2500, ( GSourceFunc ) ptk_file_task_kill_cpids,
                                                    cpids );

            // other user run - need to kill as other
            char* gsu;
            if ( ptask->task->exec_as_user && geteuid() != 0 && ( gsu = get_valid_gsu() ) )
            {
                char* cmd;
                
                // remove files
                char* rm_cmd;
                if ( ptask->task->exec_script )
                    rm_cmd = g_strdup_printf( " ; rm -f %s",
                                        ptask->task->exec_script );
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
                        
                    cmd = g_strdup_printf( "/bin/kill %d %s ; sleep 3 ; /bin/kill -s KILL %d %s %s",  ptask->task->exec_pid, scpids, ptask->task->exec_pid, scpids, rm_cmd );
                    g_free( scpids );
                }
                else
                    cmd = g_strdup_printf( "/bin/kill %d ; sleep 3 ; /bin/kill -s KILL %d %s", ptask->task->exec_pid, ptask->task->exec_pid, rm_cmd );
                g_free( rm_cmd );

                PtkFileTask* ptask2 = ptk_file_exec_new( _("Kill As Other"), NULL,
                                GTK_WIDGET( ptask->parent_window ), ptask->task_view );
                ptask2->task->exec_command = cmd;
                ptask2->task->exec_as_user = g_strdup( ptask->task->exec_as_user );
                ptask2->task->exec_sync = FALSE;
                ptask2->task->exec_browser = ptask->task->exec_browser;
                ptk_file_task_run( ptask2 );                
            }
        }

        if ( ptask->task->exec_cond )
        {
            // this is used only if exec task run in non-main loop thread
            g_mutex_lock( ptask->task->mutex );
            if ( ptask->task->exec_cond )
                g_cond_broadcast( ptask->task->exec_cond );
            g_mutex_unlock( ptask->task->mutex );
        }
    }
    else
        vfs_file_task_try_abort( ptask->task );
    return FALSE;
}

void set_button_states( PtkFileTask* ptask )
{
    char* icon;
    char* iconset;
    char* label;
    gboolean sens = !ptask->complete;
    
    if ( !ptask->progress_dlg )
        return;
    
    if ( ptask->task->state_pause == VFS_FILE_TASK_PAUSE )
    {
        label = _("Q_ueue");
        iconset = "task_que";
        icon = GTK_STOCK_ADD;
    }
    else if ( ptask->task->state_pause == VFS_FILE_TASK_QUEUE )
    {
        label = _("Res_ume");
        iconset = "task_resume";
        icon = GTK_STOCK_MEDIA_PLAY;
    }
    else
    {
        label = _("Pa_use");
        iconset = "task_pause";
        icon = GTK_STOCK_MEDIA_PAUSE;
        sens = sens && !( ptask->task->type == VFS_FILE_TASK_EXEC && 
                                                    !ptask->task->exec_pid );
    }

    XSet* set = xset_get( iconset );
    if ( set->icon )
        icon = set->icon;

    gtk_widget_set_sensitive( ptask->progress_btn_pause, sens );
    gtk_button_set_image( GTK_BUTTON( ptask->progress_btn_pause ),
                            xset_get_image( icon,
                                            GTK_ICON_SIZE_BUTTON ) );
    gtk_button_set_label( GTK_BUTTON( ptask->progress_btn_pause ), label );
}

void ptk_file_task_pause( PtkFileTask* ptask, int state )
{
    if ( ptask->task->type == VFS_FILE_TASK_EXEC )
    {
        // exec task
        if ( !ptask->task->exec_pid )
            return;
        //ptask->keep_dlg = TRUE;
        int sig;
        if ( state == VFS_FILE_TASK_PAUSE || state == VFS_FILE_TASK_QUEUE )
        {
            sig = SIGSTOP;
            ptask->task->state_pause = state;
        }
        else
        {
            sig = SIGCONT;
            ptask->task->state_pause = VFS_FILE_TASK_RUNNING;
        }
        char* cpids = vfs_file_task_get_cpids( ptask->task->exec_pid );
 
        char* gsu;
        if ( ptask->task->exec_as_user && geteuid() != 0 && ( gsu = get_valid_gsu() ) )
        {
            // other user run - need to signal as other
            char* cmd;
            if ( cpids )
            {
                // convert linefeeds to spaces
                char* scpids = g_strdup( cpids );
                char* lf;
                while ( lf = strchr( scpids, '\n' ) )
                    lf[0] = ' ';
                cmd = g_strdup_printf( "/bin/kill -s %d %d %s", sig,
                                                ptask->task->exec_pid, scpids );
                g_free( scpids );
            }
            else
                cmd = g_strdup_printf( "/bin/kill -s %d %d", sig,
                                                ptask->task->exec_pid );

            PtkFileTask* ptask2 = ptk_file_exec_new( sig == SIGSTOP ?
                                    _("Stop As Other") : _("Cont As Other"),
                                    NULL,
                                    GTK_WIDGET( ptask->parent_window ),
                                    ptask->task_view );
            ptask2->task->exec_command = cmd;
            ptask2->task->exec_as_user = g_strdup( ptask->task->exec_as_user );
            ptask2->task->exec_sync = FALSE;
            ptask2->task->exec_browser = ptask->task->exec_browser;
            ptk_file_task_run( ptask2 );                
        }
        else
        {
            kill( ptask->task->exec_pid, sig );
            if ( cpids )
                vfs_file_task_kill_cpids( cpids, sig );
        }
    }
    else if ( state == VFS_FILE_TASK_PAUSE )
        ptask->task->state_pause = VFS_FILE_TASK_PAUSE;
    else if ( state == VFS_FILE_TASK_QUEUE )
        ptask->task->state_pause = VFS_FILE_TASK_QUEUE;
    else
    {
        // Resume
        if ( ptask->task->pause_cond )
        {
            g_mutex_lock( ptask->task->mutex );
            g_cond_broadcast( ptask->task->pause_cond );
            g_mutex_unlock( ptask->task->mutex );
        }
        ptask->task->state_pause = VFS_FILE_TASK_RUNNING;
    }    
    set_button_states( ptask );
    ptask->pause_change = TRUE;
    ptask->progress_count = 50;  // trigger fast display
}

void on_progress_dlg_response( GtkDialog* dlg, int response, PtkFileTask* ptask )
{
    if ( ptask->complete && !ptask->complete_notify )
    {
        ptk_file_task_destroy( ptask );
        return;
    }
    switch ( response )
    {
    case GTK_RESPONSE_CANCEL:   // Stop btn
        ptask->keep_dlg = FALSE;
        gtk_widget_destroy( ptask->progress_dlg );
        ptask->progress_dlg = NULL;
        ptk_file_task_cancel( ptask );
        break;
    case GTK_RESPONSE_NO:       // Pause btn
        if ( ptask->task->state_pause == VFS_FILE_TASK_PAUSE )
        {
            ptk_file_task_pause( ptask, VFS_FILE_TASK_QUEUE );
        }
        else if ( ptask->task->state_pause == VFS_FILE_TASK_QUEUE )
        {
            ptk_file_task_pause( ptask, VFS_FILE_TASK_RUNNING );
        }
        else
        {
            ptk_file_task_pause( ptask, VFS_FILE_TASK_PAUSE );
        }
        task_start_queued( ptask->task_view, NULL );
        break;
/*
    case GTK_RESPONSE_YES:      // Queue btn
        ptk_file_task_pause( ptask, VFS_FILE_TASK_PAUSE );
        task_start_queued( ptask->task_view, NULL );
        ptk_file_task_pause( ptask, VFS_FILE_TASK_QUEUE );
        task_start_queued( ptask->task_view, NULL );
        break;
*/
    case GTK_RESPONSE_HELP:
        xset_show_help( GTK_WIDGET( ptask->parent_window ), NULL, "#tasks-dlg" );
        break;
    case GTK_RESPONSE_OK:
    case GTK_RESPONSE_NONE:
        ptask->keep_dlg = FALSE;
        gtk_widget_destroy( ptask->progress_dlg );
        ptask->progress_dlg = NULL;
        break;
    }
}

void on_progress_dlg_destroy( GtkDialog* dlg, PtkFileTask* ptask )
{
    char* s;
    
    s = g_strdup_printf( "%d", GTK_WIDGET( dlg ) ->allocation.width );
    if ( ptask->task->type == VFS_FILE_TASK_EXEC )
        xset_set( "task_pop_top", "s", s );
    else
        xset_set( "task_pop_top", "x", s );
    g_free( s );

    s = g_strdup_printf( "%d", GTK_WIDGET( dlg ) ->allocation.height );
    if ( ptask->task->type == VFS_FILE_TASK_EXEC )
        xset_set( "task_pop_top", "z", s );
    else
        xset_set( "task_pop_top", "y", s );
    g_free( s );

    ptask->progress_dlg = NULL;
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

void set_progress_icon( PtkFileTask* ptask )
{
    GdkPixbuf* pixbuf;
    VFSFileTask* task = ptask->task;

    if ( task->state_pause != VFS_FILE_TASK_RUNNING )
        pixbuf = gtk_icon_theme_load_icon( gtk_icon_theme_get_default(),
                            GTK_STOCK_MEDIA_PAUSE, 16, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );
    else if ( task->err_count )
        pixbuf = gtk_icon_theme_load_icon( gtk_icon_theme_get_default(),
                            "error", 16, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );
    else if ( task->type == 0 || task->type == 1 || task->type == 4 )
        pixbuf = gtk_icon_theme_load_icon( gtk_icon_theme_get_default(),
                            "stock_copy", 16, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );
    else if ( task->type == 2 || task->type == 3 )
        pixbuf = gtk_icon_theme_load_icon( gtk_icon_theme_get_default(),
                            "stock_delete", 16, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );
    else if ( task->type == VFS_FILE_TASK_EXEC && task->exec_icon )
        pixbuf = gtk_icon_theme_load_icon( gtk_icon_theme_get_default(),
                        task->exec_icon, 16, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );
    else
        pixbuf = gtk_icon_theme_load_icon( gtk_icon_theme_get_default(),
                            "gtk-execute", 16, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );
    gtk_window_set_icon( GTK_WINDOW( ptask->progress_dlg ), pixbuf );    
}

void ptk_file_task_progress_open( PtkFileTask* ptask )
{
    GtkTable* table;
    GtkLabel* label;

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

    if ( ptask->progress_dlg )
        return;

//printf("ptk_file_task_progress_open\n");

    VFSFileTask* task = ptask->task;

    ptask->progress_dlg = gtk_dialog_new_with_buttons(
                             _( titles[ task->type ] ),
                             NULL /*was task->parent_window*/ , 0,
                             NULL );

    // Buttons
    // Pause
    XSet* set = xset_get( "task_pause" );
    char* pause_icon = set->icon;
    if ( !pause_icon )
        pause_icon = GTK_STOCK_MEDIA_PAUSE;
    ptask->progress_btn_pause = gtk_button_new_with_mnemonic( _("_Pause") );
    gtk_button_set_image( GTK_BUTTON( ptask->progress_btn_pause ),
                            xset_get_image( pause_icon,
                                            GTK_ICON_SIZE_BUTTON ) );
    gtk_dialog_add_action_widget( GTK_DIALOG( ptask->progress_dlg ),
                                            ptask->progress_btn_pause,
                                            GTK_RESPONSE_NO);
    gtk_button_set_focus_on_click( GTK_BUTTON( ptask->progress_btn_pause ),
                                            FALSE );
    // Stop
    ptask->progress_btn_stop = gtk_button_new_from_stock( GTK_STOCK_STOP );
    gtk_dialog_add_action_widget( GTK_DIALOG( ptask->progress_dlg ),
                                                    ptask->progress_btn_stop,
                                                    GTK_RESPONSE_CANCEL);
    gtk_button_set_focus_on_click( GTK_BUTTON( ptask->progress_btn_stop ),
                                            FALSE );
    // Close
    ptask->progress_btn_close = gtk_button_new_from_stock( GTK_STOCK_CLOSE );
    gtk_dialog_add_action_widget( GTK_DIALOG( ptask->progress_dlg ),
                                                    ptask->progress_btn_close,
                                                    GTK_RESPONSE_OK);
    // Help
    GtkWidget* help_btn = gtk_button_new_from_stock( GTK_STOCK_HELP );
    gtk_dialog_add_action_widget( GTK_DIALOG( ptask->progress_dlg ),
                                                    help_btn,
                                                    GTK_RESPONSE_HELP);
    gtk_button_set_focus_on_click( GTK_BUTTON( help_btn ),
                                            FALSE );
                                            
    set_button_states( ptask );

    // table
    if ( task->type != VFS_FILE_TASK_EXEC )
        table = GTK_TABLE(gtk_table_new( 5, 2, FALSE ));
    else
        table = GTK_TABLE(gtk_table_new( 3, 2, FALSE ));
    gtk_container_set_border_width( GTK_CONTAINER ( table ), 5 );
    gtk_table_set_row_spacings( table, 6 );
    gtk_table_set_col_spacings( table, 4 );
    int row = 0;

    /* From: */
    label = GTK_LABEL(gtk_label_new( _( actions[ task->type ] ) ));
    gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0.5 );
    gtk_table_attach( table,
                      GTK_WIDGET(label),
                      0, 1, row, row+1, GTK_FILL, 0, 0, 0 );
    ptask->from = GTK_LABEL(gtk_label_new( 
                                    ptask->complete ? "" : task->current_file ));
    gtk_misc_set_alignment( GTK_MISC ( ptask->from ), 0, 0.5 );
    gtk_label_set_ellipsize( ptask->from, PANGO_ELLIPSIZE_MIDDLE );
    gtk_label_set_selectable( ptask->from, TRUE );
    gtk_table_attach( table,
                      GTK_WIDGET( ptask->from ),
                      1, 2, row, row+1, GTK_FILL | GTK_EXPAND, 0, 0, 0 );

    if ( task->type != VFS_FILE_TASK_EXEC )
    {
        if ( task->dest_dir )
        {
            /* To: <Destination folder>
            ex. Copy file to..., Move file to...etc. */
            row++;
            label = GTK_LABEL(gtk_label_new( _( "To:" ) ));
            gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0.5 );
            gtk_table_attach( table,
                              GTK_WIDGET(label),
                              0, 1, row, row+1, GTK_FILL, 0, 0, 0 );
            ptask->to = GTK_LABEL( gtk_label_new( task->dest_dir ) );
            gtk_misc_set_alignment( GTK_MISC ( ptask->to ), 0, 0.5 );
            gtk_label_set_ellipsize( ptask->to, PANGO_ELLIPSIZE_MIDDLE );
            gtk_label_set_selectable( ptask->to, TRUE );
            gtk_table_attach( table,
                              GTK_WIDGET( ptask->to ),
                              1, 2, row, row+1, GTK_FILL, 0, 0, 0 );
        }
        
        // Stats
        row++;
        label = GTK_LABEL(gtk_label_new( _( "Progress:  " ) ));
        gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0.5 );
        gtk_table_attach( table,
                          GTK_WIDGET(label),
                          0, 1, row, row+1, GTK_FILL, 0, 0, 0 );
        ptask->current = GTK_LABEL(gtk_label_new( "" ));
        gtk_misc_set_alignment( GTK_MISC ( ptask->current ), 0, 0.5 );
        gtk_label_set_ellipsize( ptask->current, PANGO_ELLIPSIZE_MIDDLE );
        gtk_label_set_selectable( ptask->current, TRUE );
        gtk_table_attach( table,
                          GTK_WIDGET( ptask->current ),
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
/*    ptask->current = GTK_LABEL(gtk_label_new( _( "Preparing..." ) ));
    gtk_label_set_ellipsize( ptask->current, PANGO_ELLIPSIZE_MIDDLE );
    gtk_misc_set_alignment( GTK_MISC ( ptask->current ), 0, 0.5 );
    gtk_table_attach( table,
                      GTK_WIDGET( ptask->current ),
                      1, 2, 2, 3, GTK_FILL, 0, 0, 0 );
*/  //MOD

    // Status
    row++;
    label = GTK_LABEL(gtk_label_new( _( "Status:  " ) ));
    gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0.5 );
    gtk_table_attach( table,
                      GTK_WIDGET(label),
                      0, 1, row, row+1, GTK_FILL, 0, 0, 0 );
    char* status;
    if ( task->state_pause == VFS_FILE_TASK_PAUSE )
        status = _("Paused");
    else if ( task->state_pause == VFS_FILE_TASK_QUEUE )
        status = _("Queued");
    else
        status = _("Running...");
    ptask->errors = GTK_LABEL( gtk_label_new( status ) );
    gtk_misc_set_alignment( GTK_MISC ( ptask->errors ), 0, 0.5 );
    gtk_label_set_ellipsize( ptask->errors, PANGO_ELLIPSIZE_MIDDLE );
    gtk_label_set_selectable( ptask->errors, TRUE );
    gtk_table_attach( table,
                      GTK_WIDGET( ptask->errors ),
                      1, 2, row, row+1, GTK_FILL | GTK_EXPAND, 0, 0, 0 );

    /* Progress: */
    row++;
    //label = GTK_LABEL(gtk_label_new( _( "Progress:" ) ));
    //gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0.5 );
    //gtk_table_attach( table,
    //                  GTK_WIDGET(label),
    //                  0, 1, 3, 4, GTK_FILL, 0, 0, 0 );
    ptask->progress_bar = GTK_PROGRESS_BAR(gtk_progress_bar_new());
    gtk_progress_bar_set_pulse_step( ptask->progress_bar, 0.08 );
    gtk_table_attach( table,
                      GTK_WIDGET( ptask->progress_bar ),
                      0, 2, row, row+1, GTK_FILL | GTK_EXPAND, 0, 0, 0 );

    // Error log
    ptask->scroll = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new( NULL, NULL ) );
    ptask->error_view = gtk_text_view_new_with_buffer( ptask->log_buf );
    // ubuntu shows input too small so use mininum height
    gtk_widget_set_size_request( GTK_WIDGET( ptask->error_view ), -1, 70 );
    gtk_widget_set_size_request( GTK_WIDGET( ptask->scroll ), -1, 70 );
    gtk_container_add ( GTK_CONTAINER ( ptask->scroll ), ptask->error_view );
    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( ptask->scroll ),
                                     GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_text_view_set_wrap_mode( GTK_TEXT_VIEW( ptask->error_view ), GTK_WRAP_WORD_CHAR );
    gtk_text_view_set_editable( GTK_TEXT_VIEW( ptask->error_view ), FALSE );
    if ( !task->exec_scroll_lock )
    {
        gtk_text_view_scroll_to_mark( GTK_TEXT_VIEW( ptask->error_view ), ptask->log_end,
                                                            0.0, FALSE, 0, 0 );
    }
    char* fontname = xset_get_s( "task_pop_font" );
    if ( fontname )
    {
        PangoFontDescription* font_desc = pango_font_description_from_string( fontname );
        gtk_widget_modify_font( ptask->error_view, font_desc );
        pango_font_description_free( font_desc );
    }
    g_signal_connect( ptask->error_view, "populate-popup", G_CALLBACK(on_view_popup), NULL );
    GtkWidget* align = gtk_alignment_new( 1, 1, 1 ,1 );
    gtk_alignment_set_padding( GTK_ALIGNMENT( align ), 0, 0, 5, 5 );
    gtk_container_add ( GTK_CONTAINER ( align ), GTK_WIDGET( ptask->scroll ) );

    // Pack
    gtk_box_pack_start( GTK_BOX( GTK_DIALOG( ptask->progress_dlg ) ->vbox ),
                        GTK_WIDGET( table ),
                        FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( GTK_DIALOG( ptask->progress_dlg ) ->vbox ),
                        GTK_WIDGET( align ),
                        TRUE, TRUE, 0 );

    int win_width, win_height;
    if ( task->type == VFS_FILE_TASK_EXEC )
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
    gtk_window_set_default_size( GTK_WINDOW( ptask->progress_dlg ),
                                 win_width, win_height );
    gtk_button_box_set_layout ( GTK_BUTTON_BOX ( GTK_DIALOG( ptask->progress_dlg ) ->action_area ),
                                GTK_BUTTONBOX_END );
    if ( xset_get_b( "task_pop_top" ) )
        gtk_window_set_type_hint ( GTK_WINDOW ( ptask->progress_dlg ),
                                   GDK_WINDOW_TYPE_HINT_DIALOG );
    else
        gtk_window_set_type_hint ( GTK_WINDOW ( ptask->progress_dlg ),
                                   GDK_WINDOW_TYPE_HINT_NORMAL );
    gtk_window_set_gravity ( GTK_WINDOW ( ptask->progress_dlg ),
                             GDK_GRAVITY_NORTH_EAST );

//    gtk_dialog_set_default_response( ptask->progress_dlg, GTK_RESPONSE_OK );
    g_signal_connect( ptask->progress_dlg, "response",
                      G_CALLBACK( on_progress_dlg_response ), ptask );
    g_signal_connect( ptask->progress_dlg, "destroy",
                      G_CALLBACK( on_progress_dlg_destroy ), ptask );

    gtk_widget_show_all( ptask->progress_dlg );
    gtk_widget_grab_focus( ptask->progress_btn_close );

    // icon
    set_progress_icon( ptask );

//printf("ptk_file_task_progress_open DONE\n");
}

void ptk_file_task_progress_update( PtkFileTask* ptask )
{
    char* ufile_path;
    char percent_str[ 16 ];
    char* stats;
    char* errs;
    GtkTextIter iter, siter;
    GdkPixbuf* pixbuf;

    if ( !ptask->progress_dlg )
    {
        if ( ptask->pause_change )
            ptask->pause_change = FALSE;  // stop elapsed timer
        return;
    }

//printf("ptk_file_task_progress_update ptask=%#x\n", ptask);

    VFSFileTask* task = ptask->task;

    // current file
    if ( ptask->complete )
    {
        gtk_widget_set_sensitive( ptask->progress_btn_stop, FALSE );
        gtk_widget_set_sensitive( ptask->progress_btn_pause, FALSE );
        if ( task->type != VFS_FILE_TASK_EXEC )
            ufile_path = NULL;
        else
            ufile_path = g_strdup( task->current_file );

        if ( ptask->aborted )
            gtk_window_set_title( GTK_WINDOW( ptask->progress_dlg ), _("Stopped") );                
        else
        {
            if ( task->err_count )
                gtk_window_set_title( GTK_WINDOW( ptask->progress_dlg ), _("Errors") );
            else
                gtk_window_set_title( GTK_WINDOW( ptask->progress_dlg ), _("Done") );
        }
    }
    else if ( task->current_file )
    {
        if ( task->type != VFS_FILE_TASK_EXEC )
            ufile_path = g_filename_display_basename( task->current_file );
        else
            ufile_path = g_strdup( task->current_file );
    }
    else
        ufile_path = NULL;
    gtk_label_set_text( ptask->from, ufile_path );
    if ( ufile_path )
        g_free( ufile_path );

/*
    // current dest
    if ( ptask->old_dest_file )
    {    
        ufile_path = g_filename_display_name( ptask->old_dest_file );
        gtk_label_set_text( ptask->to, ufile_path );
        g_free( ufile_path );
    }
*/

    // progress bar
    if ( task->type != VFS_FILE_TASK_EXEC )
    {
        if ( task->percent >= 0 )
        {
            gtk_progress_bar_set_fraction( ptask->progress_bar,
                                           ( ( gdouble ) task->percent ) / 100 );
            g_snprintf( percent_str, 16, "%d %%", task->percent );
            gtk_progress_bar_set_text( ptask->progress_bar, percent_str );
        }
        else
            gtk_progress_bar_set_fraction( ptask->progress_bar, 0 );
    }
    else if ( ptask->complete )
    {
        if ( task->exec_is_error || ptask->aborted )
            gtk_progress_bar_set_fraction( ptask->progress_bar, 0 );
        else
            gtk_progress_bar_set_fraction( ptask->progress_bar, 1 );
    }
    else if ( task->type == VFS_FILE_TASK_EXEC
                                && task->state_pause == VFS_FILE_TASK_RUNNING )
        gtk_progress_bar_pulse( ptask->progress_bar );

    // progress
    if ( task->type != VFS_FILE_TASK_EXEC )
    {
        if ( ptask->complete )
        {
            if ( xset_get_b( "task_pop_detail" ) )
                stats = g_strdup_printf( "#%s (%s) [%s] @avg %s",
                                    ptask->dsp_file_count, ptask->dsp_size_tally,
                                    ptask->dsp_elapsed, ptask->dsp_avgspeed );
            else
                stats = g_strdup_printf( "%s  (%s)",
                                    ptask->dsp_size_tally, ptask->dsp_avgspeed );
        }
        else
        {
            if ( xset_get_b( "task_pop_detail" ) )
                stats = g_strdup_printf( "#%s (%s) [%s] @cur %s (%s) @avg %s (%s)",
                                    ptask->dsp_file_count, ptask->dsp_size_tally,
                                    ptask->dsp_elapsed, ptask->dsp_curspeed,
                                    ptask->dsp_curest, ptask->dsp_avgspeed,
                                    ptask->dsp_avgest );
            else
                stats = g_strdup_printf( _("%s  (%s)  %s remaining"),
                                    ptask->dsp_size_tally, ptask->dsp_avgspeed,
                                    ptask->dsp_avgest );
        }
        gtk_label_set_text( ptask->current, stats );
        g_free( stats );
    }

    // error/output log
    if ( ptask->log_appended )
    {
        // trim ?
        if ( gtk_text_buffer_get_char_count( ptask->log_buf ) > 64000 ||
                        gtk_text_buffer_get_line_count( ptask->log_buf ) > 800 )
        {
            if ( gtk_text_buffer_get_char_count( ptask->log_buf ) > 64000 )
            {
                // trim to 50000 characters - handles single line flood
                gtk_text_buffer_get_iter_at_offset( ptask->log_buf, &iter,
                        gtk_text_buffer_get_char_count( ptask->log_buf ) - 50000 );
            }
            else
                // trim to 700 lines
                gtk_text_buffer_get_iter_at_line( ptask->log_buf, &iter, 
                        gtk_text_buffer_get_line_count( ptask->log_buf ) - 700 );
            gtk_text_buffer_get_start_iter( ptask->log_buf, &siter );
            gtk_text_buffer_delete( ptask->log_buf, &siter, &iter );
            gtk_text_buffer_get_start_iter( ptask->log_buf, &siter );
            if ( task->type == VFS_FILE_TASK_EXEC )
                gtk_text_buffer_insert( ptask->log_buf, &siter, _("[ SNIP - additional output above has been trimmed from this log ]\n"), -1 );
            else
                gtk_text_buffer_insert( ptask->log_buf, &siter, _("[ SNIP - additional errors above have been trimmed from this log ]\n"), -1 );
        }

        if ( !task->exec_scroll_lock )
        {
            //scroll to end if scrollbar is mostly down
            GtkAdjustment* adj =  gtk_scrolled_window_get_vadjustment( ptask->scroll );
            if (  adj->upper - adj->value < adj->page_size + 40 )
                gtk_text_view_scroll_to_mark( GTK_TEXT_VIEW( ptask->error_view ),
                                                            ptask->log_end,
                                                            0.0, FALSE, 0, 0 );
        }
    }

    // icon
    if ( ptask->pause_change || ptask->err_count != task->err_count )
    {
        ptask->pause_change = FALSE;
        ptask->err_count = task->err_count;
        set_progress_icon( ptask );
    }
    
    // status
    if ( ptask->complete )
    {
        if ( ptask->aborted )
        {
            if ( task->err_count && task->type != VFS_FILE_TASK_EXEC )
            {
                if ( xset_get_b( "task_err_first" ) )
                    errs = g_strdup_printf( _("Error  ( Stop If First )") );
                else if ( xset_get_b( "task_err_any" ) )
                    errs = g_strdup_printf( _("Error  ( Stop On Any )") );
                else
                    errs = g_strdup_printf( ngettext( "Stopped with %d error",
                                                      "Stopped with %d errors",
                                                      task->err_count ),
                                                      task->err_count );
            }
            else
                errs = g_strdup_printf( _("Stopped") );
        }
        else
        {
            if ( task->type != VFS_FILE_TASK_EXEC )
            {
                if ( task->err_count )
                    errs = g_strdup_printf( ngettext( "Finished with %d error",
                                                      "Finished with %d errors",
                                                      task->err_count ),
                                                      task->err_count );
                else
                    errs = g_strdup_printf( _("Done") );
            }
            else
            {
                if ( task->exec_exit_status )
                    errs = g_strdup_printf( _("Finished with error  ( exit status %d )"),
                                                task->exec_exit_status );
                else if ( task->exec_is_error )
                    errs = g_strdup_printf( _("Finished with error") );
                else
                    errs = g_strdup_printf( _("Done") );
            }
        }
    }
    else if ( task->state_pause == VFS_FILE_TASK_PAUSE )
    {
        if ( task->type != VFS_FILE_TASK_EXEC || !task->exec_pid )
            errs = g_strdup_printf( _("Paused") );
        else
            errs = g_strdup_printf( _("Paused  ( pid %d )"), task->exec_pid );
    }
    else if ( task->state_pause == VFS_FILE_TASK_QUEUE )
    {
        if ( task->type != VFS_FILE_TASK_EXEC  || !task->exec_pid )
            errs = g_strdup_printf( _("Queued") );
        else
            errs = g_strdup_printf( _("Queued  ( pid %d )"), task->exec_pid );
    }
    else
    {
        if ( task->type != VFS_FILE_TASK_EXEC )
        {
            if ( task->err_count )
                errs = g_strdup_printf( ngettext( "Running with %d error",
                                                  "Running with %d errors",
                                                  task->err_count ),
                                                  task->err_count );
            else
                errs = g_strdup_printf( _("Running...") );
        }
        else
            errs = g_strdup_printf( _("Running...  ( pid %d )"), task->exec_pid );
    }
    gtk_label_set_text( ptask->errors, errs );
    g_free( errs );
//printf("ptk_file_task_progress_update DONE ptask=%#x\n", ptask);
}

void ptk_file_task_set_chmod( PtkFileTask* ptask,
                              guchar* chmod_actions )
{
    vfs_file_task_set_chmod( ptask->task, chmod_actions );
}

void ptk_file_task_set_chown( PtkFileTask* ptask,
                              uid_t uid, gid_t gid )
{
    vfs_file_task_set_chown( ptask->task, uid, gid );
}

void ptk_file_task_set_recursive( PtkFileTask* ptask, gboolean recursive )
{
    vfs_file_task_set_recursive( ptask->task, recursive );
}

void ptk_file_task_update( PtkFileTask* ptask )
{
//printf("ptk_file_task_update ptask=%#x\n", ptask);
    // calculate updated display data

    if ( !g_mutex_trylock( ptask->task->mutex ) )
    {
//printf("UPDATE LOCKED  @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
        return;
    }

    VFSFileTask* task = ptask->task;
    time_t cur_speed;

    if ( task->type != VFS_FILE_TASK_EXEC )
    {
        //cur speed (based on at least 2 sec interval)
        if ( task->state_pause != VFS_FILE_TASK_RUNNING )
        {
            cur_speed = task->last_speed;
            task->last_time = time( NULL );
            task->last_progress = task->progress;
        }
        else
        {
            cur_speed = time( NULL ) - task->last_time;
            if ( cur_speed > 1 )
            {
                //g_warning ("timediff=%f  %f", (float) cur_speed, (float) time( NULL ));
                cur_speed = ( task->progress - task->last_progress ) / cur_speed;
                task->last_time = time( NULL );
                task->last_speed = cur_speed;
                task->last_progress = task->progress;
            }
        }
        // calc percent
        int ipercent;
        if ( task->total_size )
        {
            gdouble dpercent = ( ( gdouble ) task->progress ) / task->total_size;
            ipercent = ( int ) ( dpercent * 100 );
        }
        else
            ipercent = 50;  // total_size calculation timed out
        if ( ipercent != task->percent )
            task->percent = ipercent;
    }

    //elapsed
    time_t etime = time( NULL ) - task->start_time - task->pause_time ;
    guint hours = etime / 3600;
    char* elapsed;
    char* elapsed2;
    char* elapsed3;
    if ( hours == 0 )
        elapsed = g_strdup( "" );
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
    g_free( ptask->dsp_elapsed );
    ptask->dsp_elapsed = elapsed3;

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
        file_count = g_strdup_printf( "%d", task->current_item );
        //size
        vfs_file_size_to_string_format( buf1, task->progress, NULL );
        if ( task->total_size )
            vfs_file_size_to_string_format( buf2, task->total_size, NULL );
        else
            sprintf( buf2, "??" );  // total_size calculation timed out
        size_tally = g_strdup_printf( "%s / %s", buf1, buf2 );
        //avg speed
        time_t cur_speed;
        time_t avg_speed = etime;
        if ( avg_speed > 0 )
            avg_speed = task->progress / avg_speed;
        else
            avg_speed = 0;
        vfs_file_size_to_string_format( buf2, avg_speed, NULL );
        if ( task->last_speed == 0 && task->progress == 0 )
            cur_speed = avg_speed;
        else
            cur_speed = task->last_speed;
        vfs_file_size_to_string_format( buf1, cur_speed, NULL );
        if ( cur_speed == 0 || task->state_pause != VFS_FILE_TASK_RUNNING )
        {
            if ( task->state_pause == VFS_FILE_TASK_PAUSE )
                speed1 = g_strdup_printf( _("paused") );
            else if ( task->state_pause == VFS_FILE_TASK_QUEUE )
                speed1 = g_strdup_printf( _("queued") );
            else
                speed1 = g_strdup_printf( _("stalled") );
        }
        else
            speed1 = g_strdup_printf( "%s/s", buf1 );
        speed2 = g_strdup_printf( "%s/s", buf2 );
        //remain cur
        off64_t remain;
        if ( cur_speed > 0 && task->total_size != 0 )
            remain = ( task->total_size - task->progress ) / cur_speed;
        else
            remain = 0;
        if ( remain <= 0 )
            remain1 = g_strdup_printf( "" );  // n/a
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
        if ( avg_speed > 0 && task->total_size != 0 )
            remain = ( task->total_size - task->progress ) / avg_speed;
        else
            remain = 0;
        if ( remain <= 0 )
            remain2 = g_strdup_printf( "" );  // n/a
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

        g_free( ptask->dsp_file_count );
        ptask->dsp_file_count = file_count;
        g_free( ptask->dsp_size_tally );
        ptask->dsp_size_tally = size_tally;
        g_free( ptask->dsp_curspeed );
        ptask->dsp_curspeed = speed1;
        g_free( ptask->dsp_curest );
        ptask->dsp_curest = remain1;
        g_free( ptask->dsp_avgspeed );
        ptask->dsp_avgspeed = speed2;
        g_free( ptask->dsp_avgest );
        ptask->dsp_avgest = remain2;
    }

    // move log lines from add_log_buf to log_buf
    if ( gtk_text_buffer_get_char_count( task->add_log_buf ) )
    {
        GtkTextIter iter, siter;
        char* text;
        // get add_log text and delete
        gtk_text_buffer_get_start_iter( task->add_log_buf, &siter );
        gtk_text_buffer_get_iter_at_mark( task->add_log_buf, &iter, task->add_log_end );
        text = gtk_text_buffer_get_text( task->add_log_buf, &siter, &iter, FALSE );
        gtk_text_buffer_delete( task->add_log_buf, &siter, &iter );
        // insert into log
        gtk_text_buffer_get_iter_at_mark( ptask->log_buf, &iter, ptask->log_end );
        gtk_text_buffer_insert( ptask->log_buf, &iter, text, -1 );
        g_free( text );
        ptask->log_appended = TRUE;

        if ( !ptask->progress_dlg && task->type == VFS_FILE_TASK_EXEC 
                                                    && task->exec_show_output )
        {
            task->exec_show_output = FALSE; // disable to open every time output occurs
            ptask->keep_dlg = TRUE;
            ptk_file_task_progress_open( ptask );
        }
    }

    if ( !ptask->progress_dlg )
    {
        if ( task->type != VFS_FILE_TASK_EXEC
                                    && ptask->err_count != task->err_count )
        {
            ptask->keep_dlg = TRUE;
            ptk_file_task_progress_open( ptask );
        }
        else if ( task->type == VFS_FILE_TASK_EXEC
                                    && ptask->err_count != task->err_count )
        {
            if ( !ptask->aborted && task->exec_show_error )
            {
                ptask->keep_dlg = TRUE;
                ptk_file_task_progress_open( ptask );
            }
        }
    }
    else
    {
        if ( task->type != VFS_FILE_TASK_EXEC
                                    && ptask->err_count != task->err_count )
        {
            ptask->keep_dlg = TRUE;
            gtk_window_present( GTK_WINDOW( ptask->progress_dlg ) );
        }
        else if ( task->type == VFS_FILE_TASK_EXEC
                                    && ptask->err_count != task->err_count )
        {
            if ( !ptask->aborted && task->exec_show_error )
            {
                ptask->keep_dlg = TRUE;
                gtk_window_present( GTK_WINDOW( ptask->progress_dlg ) );
            }
        }        
    }

    ptk_file_task_progress_update( ptask );

    if ( !ptask->timeout && !ptask->complete )
        main_task_view_update_task( ptask );

    g_mutex_unlock( task->mutex );
//printf("ptk_file_task_update DONE ptask=%#x\n", ptask);
}

gboolean on_vfs_file_task_state_cb( VFSFileTask* task,
                                    VFSFileTaskState state,
                                    gpointer state_data,
                                    gpointer user_data )
{
    PtkFileTask* ptask = ( PtkFileTask* ) user_data;
    GtkWidget* dlg;
    int response;
    gboolean ret = TRUE;
    char** new_dest;

    switch ( state )
    {
    case VFS_FILE_TASK_FINISH:
        //printf("VFS_FILE_TASK_FINISH\n");

        ptask->complete = TRUE;

        g_mutex_lock( task->mutex );
        if ( task->type != VFS_FILE_TASK_EXEC )
            string_copy_free( &task->current_file, NULL );
        ptask->progress_count = 50;  // trigger fast display
        g_mutex_unlock( task->mutex );
        //gtk_signal_emit_by_name( GTK_OBJECT( ptask->signal_widget ), "task-notify",
        //                                                                 ptask );
        break;
    case VFS_FILE_TASK_QUERY_OVERWRITE:
        //0; GThread *self = g_thread_self ();
        //printf("TASK_THREAD = %#x\n", self );
        g_mutex_lock( task->mutex );
        ptask->query_new_dest = ( char** ) state_data;
        ptask->query_cond = g_cond_new();
        g_cond_wait( ptask->query_cond, task->mutex );
        g_cond_free( ptask->query_cond );
        ptask->query_cond = NULL;
        ret = ptask->query_ret;
        g_mutex_unlock( task->mutex );
        break;
    case VFS_FILE_TASK_QUERY_ABORT:
        //printf("VFS_FILE_TASK_QUERY_ABORT\n");
        /*
        dlg = gtk_message_dialog_new( GTK_WINDOW( ptask->progress_dlg ),
                                      GTK_DIALOG_MODAL,
                                      GTK_MESSAGE_QUESTION,
                                      GTK_BUTTONS_YES_NO,
                                      _( "Cancel the operation?" ) );
        response = gtk_dialog_run( GTK_DIALOG( dlg ) );
        gtk_widget_destroy( dlg );
        ret = ( response != GTK_RESPONSE_YES );
        */
        /* exec task never has query abort ?
        if ( task->type == VFS_FILE_TASK_EXEC )
        {
            GThread *self = g_thread_self ();
            printf("VFS_FILE_TASK_QUERY_ABORT-THREAD = %#x\n", self );
            g_idle_add( ( GSourceFunc ) ptk_file_task_cancel, ptask );
        }
        */
        ret = FALSE;
        break;
    case VFS_FILE_TASK_ERROR:
        //printf("VFS_FILE_TASK_ERROR\n");
        g_mutex_lock( task->mutex );
        task->err_count++;
        //printf("    ptask->item_count = %d\n", task->current_item );

        if ( task->type == VFS_FILE_TASK_EXEC )
        {
            task->exec_is_error = TRUE;
            ret = FALSE;
        }
        else if ( xset_get_b( "task_err_any" ) ||
                    ( task->current_item < 2 && xset_get_b( "task_err_first" ) ) )
        {
//printf("    ABORT ON ERROR\n");
            ret = FALSE;
            ptask->aborted = TRUE;
        }
        ptask->progress_count = 50;  // trigger fast display

        g_mutex_unlock( task->mutex );
        break;
    default:
        break;
    }
    
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

void query_overwrite_response( GtkDialog *dlg, gint response, PtkFileTask* ptask )
{
    char* file_name;
    char* dir_name;

    switch ( response )
    {
    case RESPONSE_OVERWRITEALL:
        vfs_file_task_set_overwrite_mode( ptask->task, VFS_FILE_TASK_OVERWRITE_ALL );
        break;
    case RESPONSE_OVERWRITE:
        vfs_file_task_set_overwrite_mode( ptask->task, VFS_FILE_TASK_OVERWRITE );
        break;
    case RESPONSE_SKIPALL:
        vfs_file_task_set_overwrite_mode( ptask->task, VFS_FILE_TASK_SKIP_ALL );
        break;
    case RESPONSE_SKIP:
        vfs_file_task_set_overwrite_mode( ptask->task, VFS_FILE_TASK_SKIP );
        break;
    case RESPONSE_RENAME:
        vfs_file_task_set_overwrite_mode( ptask->task, VFS_FILE_TASK_RENAME );
        file_name = g_filename_from_utf8( gtk_entry_get_text( ptask->query_entry ),
                                          - 1, NULL, NULL, NULL );
        if ( file_name && ptask->task->current_dest )
        {
            dir_name = g_path_get_dirname( ptask->task->current_dest );
            *ptask->query_new_dest = g_build_filename( dir_name, file_name, NULL );
            g_free( file_name );
            g_free( dir_name );
        }
        break;
    case GTK_RESPONSE_DELETE_EVENT: // escape was pressed 
    case GTK_RESPONSE_CANCEL:
        ptask->task->state = VFS_FILE_TASK_ABORTED;
        //vfs_file_task_abort( ptask->task );
        break;
    }
    gtk_widget_destroy( GTK_WIDGET( dlg ) );

    if ( ptask->query_cond )
    {
        g_mutex_lock( ptask->task->mutex );
        ptask->query_ret = (response != GTK_RESPONSE_DELETE_EVENT)
                            && (response != GTK_RESPONSE_CANCEL);
        //g_cond_broadcast( ptask->query_cond );
        g_cond_signal( ptask->query_cond );
        g_mutex_unlock( ptask->task->mutex );
    }
    if ( ptask->restart_timeout )
    {
        ptask->timeout = g_timeout_add( 500,
                                ( GSourceFunc ) ptk_file_task_add_main,
                                ( gpointer ) ptask );        
    }
    ptask->progress_count = 50;
    ptask->progress_timer = g_timeout_add( 50,
                                        ( GSourceFunc ) on_progress_timer,
                                        ptask );
}

static void query_overwrite( PtkFileTask* ptask, char** new_dest )
{
//printf("query_overwrite ptask=%#x\n", ptask);
    const char* message = NULL;
    const char* question;
    const char* title;
    GtkWidget* dlg;
    GtkWidget* parent_win;

    int response;
    gboolean has_overwrite_btn = TRUE;
    char* udest_file;
    char* file_name;
    char* ufile_name;
    gboolean different_files, is_src_dir, is_dest_dir;
    struct stat64 src_stat, dest_stat;
    char* xmessage = NULL;

    different_files = ( 0 != g_strcmp0( ptask->task->current_file,
                                     ptask->task->current_dest ) );

    lstat64( ptask->task->current_file, &src_stat );
    lstat64( ptask->task->current_dest, &dest_stat );

    is_src_dir = !!S_ISDIR( dest_stat.st_mode );
    is_dest_dir = !!S_ISDIR( src_stat.st_mode );
    
    if ( different_files && is_dest_dir == is_src_dir )
    {
        if ( is_dest_dir )
        {
            /* Ask the user whether to overwrite dir content or not */
            question = _( "Overwrite this folder and its contents?" );
            title = _("Folder Exists");
        }
        else
        {
            /* Ask the user whether to overwrite the file or not */
            //question = _( "Overwrite this file?" );
            char buf[ 64 ];
            char* src_size;
            char* src_time;
            char* src_rel;
            char* src_rel_size;
            char* src_rel_time;
            if ( src_stat.st_size == dest_stat.st_size )
            {
                src_size = g_strdup( _("( same size )") );
                src_rel_size = NULL;
            }
            else
            {
                vfs_file_size_to_string( buf, src_stat.st_size );
                src_size = g_strdup_printf( _("%s\t( %llu bytes )"), buf, src_stat.st_size );
                if ( src_stat.st_size > dest_stat.st_size )
                    src_rel_size = _("larger");
                else
                    src_rel_size = _("smaller");
            }
            vfs_file_size_to_string( buf, dest_stat.st_size );
            char* dest_size = g_strdup_printf( _("%s\t( %llu bytes )"), buf, dest_stat.st_size );
            if ( src_stat.st_mtime == dest_stat.st_mtime )
            {
                src_time = g_strdup( _("( same time )\t") );
                src_rel_time = NULL;
            }
            else
            {
                strftime( buf, sizeof( buf ),
                          app_settings.date_format,
                          localtime( &src_stat.st_mtime ) );
                src_time = g_strdup( buf );
                if ( src_stat.st_mtime > dest_stat.st_mtime )
                    src_rel_time = _("newer");
                else
                    src_rel_time = _("older");
            }
            src_rel = g_strdup_printf( "%s%s%s%s%s",
                                        src_rel_time || src_rel_size ? "( " : "",
                                        src_rel_time ? src_rel_time : "",
                                        src_rel_time && src_rel_size ? ", " : "",
                                        src_rel_size ? src_rel_size : "",
                                        src_rel_time || src_rel_size ? " ) " : "" );
            strftime( buf, sizeof( buf ),
                      app_settings.date_format,
                      localtime( &dest_stat.st_mtime ) );
            char* dest_time = g_strdup( buf );
            
            message = xmessage = g_strdup_printf( _("Overwrite \"%%s\":\n\t%s\t%s\n\nwith %s\"%s\" ?\n\t%s\t%s"), dest_time, dest_size, src_rel, ptask->task->current_file, src_time, src_size );
            question = NULL;
            title = _("File Exists");
            g_free( src_size );
            g_free( dest_size );
            g_free( src_time );
            g_free( dest_time );
            g_free( src_rel );
        }
    }
    else
    { /* Rename is required */
        question = _( "Please choose a new filename:" );
        has_overwrite_btn = FALSE;
        title = _("Rename Required");
    }

    if ( different_files )
    {
        /* Ths first %s is a file name, and the second one represents followed message.
        ex: "/home/pcman/some_file" already exists.\n\nDo you want to overwrite existing file?" */
        if ( !message )
            message = _( "\"%s\" already exists.\n\n%s" );
    }
    else
    {
        /* Ths first %s is a file name, and the second one represents followed message. */
        message = _( "\"%s\"\n\nDestination and source are the same file.\n\n%s" );
    }


    udest_file = g_filename_display_name( ptask->task->current_dest );
    parent_win = ptask->progress_dlg ? ptask->progress_dlg :
                                                GTK_WIDGET( ptask->parent_window );

    dlg = gtk_message_dialog_new( GTK_WINDOW( parent_win ),
                                  GTK_DIALOG_MODAL,
                                  GTK_MESSAGE_QUESTION,
                                  GTK_BUTTONS_NONE,
                                  message,
                                  udest_file,
                                  question );
    g_free( udest_file );
    g_free( xmessage );
    gtk_window_set_title( GTK_WINDOW( dlg ), title );
    
    if ( has_overwrite_btn )
    {
        gtk_dialog_add_buttons ( GTK_DIALOG( dlg ),
                                 _( "_Overwrite" ), RESPONSE_OVERWRITE,
                                 _( "Overwrite _All" ), RESPONSE_OVERWRITEALL,
                                 NULL );
    }

    gtk_dialog_add_buttons ( GTK_DIALOG( dlg ),
                             _( "_Rename" ), RESPONSE_RENAME,
                             _( "_Skip" ), RESPONSE_SKIP,
                             _( "S_kip All" ), RESPONSE_SKIPALL,
                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                             NULL );
    file_name = g_path_get_basename( ptask->task->current_dest );
    ufile_name = g_filename_display_name( file_name );
    g_free( file_name );

    ptask->query_entry = ( GtkEntry* ) gtk_entry_new();
    g_object_set_data_full( G_OBJECT( ptask->query_entry ), "old_name",
                            ufile_name, g_free );
    g_signal_connect( G_OBJECT( ptask->query_entry ), "changed",
                      G_CALLBACK( on_file_name_entry_changed ), dlg );

    gtk_entry_set_text( ptask->query_entry, ufile_name );
	g_signal_connect (G_OBJECT (ptask->query_entry), "activate",
                      G_CALLBACK( enter_callback ), dlg );

    gtk_box_pack_start( GTK_BOX( GTK_DIALOG( dlg ) ->vbox ), GTK_WIDGET( ptask->query_entry ),
                        FALSE, TRUE, 4 );

    g_signal_connect( G_OBJECT( dlg ), "response",
                                G_CALLBACK( query_overwrite_response ), ptask );
                          
    gtk_widget_show_all( GTK_WIDGET( dlg ) );
    
    // can't run gtk_dialog_run here because it doesn't unlock a low level
    // mutex when run from inside the timer handler
    return;
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

