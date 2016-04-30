/*
*  C Implementation: vfs-file-task
*
* Description: modified and redesigned for SpaceFM
*
*
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "vfs-file-task.h"

#include <unistd.h>
#include <fcntl.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <glib.h>
#include "glib-mem.h"
#include "glib-utils.h"
#include <glib/gi18n.h>

#include <stdio.h>
#include <stdlib.h> /* for mkstemp, realpath */
#include <string.h>
#include <errno.h>

#include "vfs-dir.h"
#include "settings.h"
#include <sys/wait.h> //MOD for exec
#include "main-window.h"
#include "desktop-window.h"
#include "vfs-volume.h"

#ifdef HAVE_BTRFS_CLONE
#include <sys/ioctl.h>
#include <btrfs/ioctl.h>
#endif

const mode_t chmod_flags[] =
    {
        S_IRUSR, S_IWUSR, S_IXUSR,
        S_IRGRP, S_IWGRP, S_IXGRP,
        S_IROTH, S_IWOTH, S_IXOTH,
        S_ISUID, S_ISGID, S_ISVTX
    };

/*
* void get_total_size_of_dir( const char* path, off_t* size )
* Recursively count total size of all files in the specified directory.
* If the path specified is a file, the size of the file is directly returned.
* cancel is used to cancel the operation. This function will check the value
* pointed by cancel in every iteration. If cancel is set to TRUE, the
* calculation is cancelled.
* NOTE: *size should be set to zero before calling this function.
*/
static void get_total_size_of_dir( VFSFileTask* task,
                                   const char* path,
                                   off_t* size,
                                   struct stat64* have_stat );
void vfs_file_task_error( VFSFileTask* task, int errnox, const char* action,
                                                            const char* target );
void vfs_file_task_exec_error( VFSFileTask* task, int errnox, char* action );
void add_task_dev( VFSFileTask* task, dev_t dev );
static gboolean should_abort( VFSFileTask* task );

void gx_free( gpointer x ) {}  // dummy free - test only

void append_add_log( VFSFileTask* task, const char* msg, gint msg_len )
{
    g_mutex_lock( task->mutex );
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark( task->add_log_buf, &iter, task->add_log_end );
    gtk_text_buffer_insert( task->add_log_buf, &iter, msg, msg_len );
    g_mutex_unlock( task->mutex );
}

static void call_state_callback( VFSFileTask* task,
                          VFSFileTaskState state )
{
    task->state = state;
    if ( task->state_cb )
    {
        if ( !task->state_cb( task, state, NULL, task->state_cb_data ) )
        {
            task->abort = TRUE;
            if ( task->type == VFS_FILE_TASK_EXEC && task->exec_cond )
            {
                // this is used only if exec task run in non-main loop thread
                g_mutex_lock( task->mutex );
                g_cond_broadcast( task->exec_cond );
                g_mutex_unlock( task->mutex );
            }
        }
        else
            task->state = VFS_FILE_TASK_RUNNING;
    }
}

static gboolean should_abort( VFSFileTask* task )
{
    if ( task->state_pause != VFS_FILE_TASK_RUNNING )
    {
        // paused or queued - suspend thread
        g_mutex_lock( task->mutex );
        g_timer_stop( task->timer );
        task->pause_cond = g_cond_new();
        g_cond_wait( task->pause_cond, task->mutex );
        // resume
        g_cond_free( task->pause_cond );
        task->pause_cond = NULL;
        task->last_elapsed = g_timer_elapsed( task->timer, NULL );
        task->last_progress = task->progress;
        task->last_speed = 0;
        g_timer_continue( task->timer );
        task->state_pause = VFS_FILE_TASK_RUNNING;
        g_mutex_unlock( task->mutex );
    }
    return task->abort;
}

char* vfs_file_task_get_unique_name( const char* dest_dir, const char* base_name,
                                                           const char* ext )
{   // returns NULL if all names used; otherwise newly allocated string
    struct stat64 dest_stat;
    char* new_name = g_strdup_printf( "%s%s%s", base_name,
                                      ext && ext[0] ? "." : "",
                                      ext ? ext : "" );
    char* new_dest_file = g_build_filename( dest_dir, new_name, NULL );
    g_free( new_name );
    uint n = 1;
    while ( n && lstat64( new_dest_file, &dest_stat ) == 0 )
    {
        g_free( new_dest_file );
        new_name = g_strdup_printf( "%s-%s%d%s%s", base_name,
                                      _("copy"),
                                      ++n,
                                      ext && ext[0] ? "." : "",
                                      ext ? ext : "" );
        new_dest_file = g_build_filename( dest_dir, new_name, NULL );
        g_free( new_name );
    }
    if ( n == 0 )
    {
        g_free( new_dest_file );
        return NULL;
    }
    return new_dest_file;
}

/*
* Check if the destination file exists.
* If the dest_file exists, let the user choose a new destination,
* skip/overwrite/auto-rename/all, pause, or cancel.
* The returned string is the new destination file chosen by the user
*/
static
gboolean check_overwrite( VFSFileTask* task,
                          const gchar* dest_file,
                          gboolean* dest_exists,
                          char** new_dest_file )
{
    struct stat64 dest_stat;
    const char* use_dest_file;
    char* new_dest;

    while ( 1 )
    {
        *new_dest_file = NULL;
        if ( task->overwrite_mode == VFS_FILE_TASK_OVERWRITE_ALL )
        {
            *dest_exists = !lstat64( dest_file, &dest_stat );
            if ( !g_strcmp0( task->current_file, task->current_dest ) )
            {
                // src and dest are same file - don't overwrite (truncates)
                // occurs if user pauses task and changes overwrite mode
                return FALSE;
            }
            return TRUE;
        }
        if ( task->overwrite_mode == VFS_FILE_TASK_SKIP_ALL )
        {
            *dest_exists = !lstat64( dest_file, &dest_stat );
            return !*dest_exists;
        }
        if ( task->overwrite_mode == VFS_FILE_TASK_AUTO_RENAME )
        {
            *dest_exists = !lstat64( dest_file, &dest_stat );
            if ( !*dest_exists )
                return !task->abort;
            
            // auto-rename
            char* ext;
            char* old_name = g_path_get_basename( dest_file );
            char* dest_dir = g_path_get_dirname( dest_file );
            char* base_name = get_name_extension( old_name, 
                                                  S_ISDIR( dest_stat.st_mode ),
                                                  &ext );
            g_free( old_name );
            *new_dest_file = vfs_file_task_get_unique_name( dest_dir, base_name, ext );
            *dest_exists = FALSE;
            g_free( dest_dir );
            g_free( base_name );
            g_free( ext );
            if ( *new_dest_file )
                return !task->abort;
            // else ran out of names - fall through to query user
        }

        *dest_exists = !lstat64( dest_file, &dest_stat );
        if ( !*dest_exists )
            return !task->abort;

        // dest exists - query user
        if ( !task->state_cb )  // failsafe
            return FALSE;
        use_dest_file = dest_file;
        do
        {
            // destination file exists
            *dest_exists = TRUE;
            task->state = VFS_FILE_TASK_QUERY_OVERWRITE;
            new_dest = NULL;
            
            // query user
            if ( !task->state_cb( task, VFS_FILE_TASK_QUERY_OVERWRITE,
                                   &new_dest, task->state_cb_data ) )
                // task->abort is actually set in query_overwrite_response
                // VFS_FILE_TASK_QUERY_OVERWRITE never returns FALSE
                task->abort = TRUE;
            task->state = VFS_FILE_TASK_RUNNING;

            // may pause here - user may change overwrite mode
            if ( should_abort( task ) )
            {
                g_free( new_dest );
                return FALSE;
            }
            
            if ( task->overwrite_mode != VFS_FILE_TASK_RENAME )
            {
                g_free( new_dest );
                new_dest = NULL;
                if( task->overwrite_mode == VFS_FILE_TASK_OVERWRITE ||
                            task->overwrite_mode == VFS_FILE_TASK_OVERWRITE_ALL )
                {
                    *dest_exists = !lstat64( dest_file, &dest_stat );
                    if ( !g_strcmp0( task->current_file, task->current_dest ) )
                    {
                        // src and dest are same file - don't overwrite (truncates)
                        // occurs if user pauses task and changes overwrite mode
                        return FALSE;
                    }
                    return TRUE;
                }
                else if ( task->overwrite_mode == VFS_FILE_TASK_AUTO_RENAME )
                    break;
                else
                    return FALSE;
            }
            // user renamed file or pressed Pause btn
            if ( new_dest )
                // user renamed file - test if new name exists
                use_dest_file = new_dest;
        } while ( lstat64( use_dest_file, &dest_stat ) != -1 );
        if ( new_dest )
        {
            // user renamed file to unique name
            *dest_exists = FALSE;
            *new_dest_file = new_dest;
            return !task->abort;
        }
    }
}

gboolean check_dest_in_src( VFSFileTask* task, const char* src_dir )
{
    char real_src_path[PATH_MAX];
    char real_dest_path[PATH_MAX];
    int len;

    if ( !( task->dest_dir && realpath( task->dest_dir, real_dest_path ) ) )
        return FALSE;
    if ( realpath( src_dir, real_src_path ) &&
                          g_str_has_prefix( real_dest_path, real_src_path ) &&
                          ( len = strlen( real_src_path ) ) &&
                          ( real_dest_path[len] == '/' || 
                            real_dest_path[len] == '\0' ) )
    {
        // source is contained in destination dir
        char* disp_src = g_filename_display_name( src_dir );
        char* disp_dest = g_filename_display_name( task->dest_dir );
        char* err = g_strdup_printf( _("Destination directory \"%1$s\" is contained in source \"%2$s\""), disp_dest, disp_src );
        append_add_log( task, err, -1 );
        g_free( err );
        g_free( disp_src );
        g_free( disp_dest );
        if ( task->state_cb )
            task->state_cb( task, VFS_FILE_TASK_ERROR,
                                        NULL, task->state_cb_data );
        task->state = VFS_FILE_TASK_RUNNING;
        return TRUE;
    }
    return FALSE;
}

void update_file_display( const char* path )
{
    // for devices like nfs, emit created and flush to avoid a
    // blocking stat call in GUI thread during writes
    GDK_THREADS_ENTER();
    char* dir_path = g_path_get_dirname( path );
    VFSDir* vdir = vfs_dir_get_by_path_soft( dir_path );
    g_free( dir_path );
    if ( vdir && vdir->avoid_changes )
    {
        VFSFileInfo* file = vfs_file_info_new();
        vfs_file_info_get( file, path, NULL );
        vfs_dir_emit_file_created( vdir,
                            vfs_file_info_get_name( file ), TRUE );
        vfs_file_info_unref( file );
        vfs_dir_flush_notify_cache();
    }
    if ( vdir )
        g_object_unref( vdir );
    GDK_THREADS_LEAVE();
}

/*
void update_file_display( const char* path )
{
    // for devices like nfs, emit instant changed
    GDK_THREADS_ENTER();
    char* dir_path = g_path_get_dirname( path );
    VFSDir* vdir = vfs_dir_get_by_path_soft( dir_path );
    g_free( dir_path );
    if ( vdir && vdir->avoid_changes )
    {
        char* filename = g_path_get_basename( path );
        vfs_dir_emit_file_changed( vdir, filename, NULL, TRUE );
        g_free( filename );
    }
    if ( vdir )
        g_object_unref( vdir );
    GDK_THREADS_LEAVE();
}
*/

static int clone_file( int wfd, int rfd )
{
    int err;
#ifdef HAVE_BTRFS_CLONE
    err = ioctl( wfd, BTRFS_IOC_CLONE, rfd );
    if ( ! err ) {
        return 0;
    }
#endif
    return -1;
}

static gboolean
vfs_file_task_do_copy( VFSFileTask* task,
                       const char* src_file,
                       const char* dest_file )
{
    GDir * dir;
    const gchar* file_name;
    gchar* sub_src_file;
    gchar* sub_dest_file;
    struct stat64 file_stat;
    char buffer[ 4096 ];
    int rfd;
    int wfd;
    ssize_t rsize;
    char* new_dest_file = NULL;
    gboolean dest_exists;
    gboolean copy_fail = FALSE;
    int result;
    GError *error;

    if ( should_abort( task ) )
        return FALSE;
//printf("vfs_file_task_do_copy( %s, %s )\n", src_file, dest_file );
    g_mutex_lock( task->mutex );
    string_copy_free( &task->current_file, src_file );
    string_copy_free( &task->current_dest, dest_file );
    task->current_item++;
    g_mutex_unlock( task->mutex );

    if ( lstat64( src_file, &file_stat ) == -1 )
    {
        vfs_file_task_error( task, errno, _("Accessing"), src_file );
        return FALSE;
    }

    result = 0;
    if ( S_ISDIR( file_stat.st_mode ) )
    {
        if ( check_dest_in_src( task, src_file ) )
            goto _return_;

        if ( ! check_overwrite( task, dest_file,
                                &dest_exists, &new_dest_file ) )
            goto _return_;
        if ( new_dest_file )
        {
            dest_file = new_dest_file;
            g_mutex_lock( task->mutex );
            string_copy_free( &task->current_dest, dest_file );
            g_mutex_unlock( task->mutex );
        }
        
        if ( ! dest_exists )
            result = mkdir( dest_file, file_stat.st_mode | 0700 );

        if ( result == 0 )
        {
            struct utimbuf times;
            g_mutex_lock( task->mutex );
            task->progress += file_stat.st_size;
            g_mutex_unlock( task->mutex );

            error = NULL;
            dir = g_dir_open( src_file, 0, &error );
            if ( dir )
            {
                while ( (file_name = g_dir_read_name( dir )) )
                {
                    if ( should_abort( task ) )
                        break;
                    sub_src_file = g_build_filename( src_file, file_name, NULL );
                    sub_dest_file = g_build_filename( dest_file, file_name, NULL );
                    if ( !vfs_file_task_do_copy( task, sub_src_file, sub_dest_file )
                                                        && !copy_fail )
                        copy_fail = TRUE;
                    g_free(sub_dest_file );
                    g_free(sub_src_file );
                }
                g_dir_close( dir );
            }
            else if ( error )
            {
                char* msg = g_strdup_printf( "\n%s\n", error->message );
                g_error_free( error );
                vfs_file_task_exec_error( task, 0, msg );
                g_free( msg );
                copy_fail = TRUE;
                if ( should_abort( task ) )
                    goto _return_;
            }

            chmod( dest_file, file_stat.st_mode );
            times.actime = file_stat.st_atime;
            times.modtime = file_stat.st_mtime;
            utime( dest_file, &times );

            if ( task->avoid_changes )
                update_file_display( dest_file );

            /* Move files to different device: Need to delete source dir */
            if ( ( task->type == VFS_FILE_TASK_MOVE
                 || task->type == VFS_FILE_TASK_TRASH )
                 && !should_abort( task ) && !copy_fail )
            {
                if ( (result = rmdir( src_file )) )
                {
                    vfs_file_task_error( task, errno, _("Removing"), src_file );
                    copy_fail = TRUE;
                    if ( should_abort( task ) )
                        goto _return_;
                }
            }
        }
        else
        {
            vfs_file_task_error( task, errno, _("Creating Dir"), dest_file );
            copy_fail = TRUE;
        }
    }
    else if ( S_ISLNK( file_stat.st_mode ) )
    {
        if ( ( rfd = readlink( src_file, buffer, sizeof( buffer ) - 1 ) ) > 0 )
        {
            buffer[rfd] = '\0';  //MOD terminate buffer string
            if ( ! check_overwrite( task, dest_file,
                                    &dest_exists, &new_dest_file ) )
                goto _return_;

            if ( new_dest_file )
            {
                dest_file = new_dest_file;
                g_mutex_lock( task->mutex );
                string_copy_free( &task->current_dest, dest_file );
                g_mutex_unlock( task->mutex );
            }

            //MOD delete it first to prevent exists error
            if ( dest_exists )
            {
                result = unlink( dest_file );
                if ( result && errno != 2 /* no such file */ )
                {
                    vfs_file_task_error( task, errno, _("Removing"), dest_file );
                    goto _return_;
                }                
            }

            if ( ( wfd = symlink( buffer, dest_file ) ) == 0 )
            {
                //MOD don't chmod link because it changes target
                //chmod( dest_file, file_stat.st_mode );

                /* Move files to different device: Need to delete source files */
                if ( ( task->type == VFS_FILE_TASK_MOVE
                                            || task->type == VFS_FILE_TASK_TRASH )
                                            && !copy_fail )
                {
                    result = unlink( src_file );
                    if ( result )
                    {
                        vfs_file_task_error( task, errno, _("Removing"), src_file );
                        copy_fail = TRUE;
                    }
                }
                g_mutex_lock( task->mutex );
                task->progress += file_stat.st_size;
                g_mutex_unlock( task->mutex );
            }
            else
            {
                vfs_file_task_error( task, errno, _("Creating Link"), dest_file );
                copy_fail = TRUE;
            }
        }
        else
        {
            vfs_file_task_error( task, errno, _("Accessing"), src_file );
            copy_fail = TRUE;
        }
    }
    else
    {
        if ( ( rfd = open( src_file, O_RDONLY ) ) >= 0 )
        {
            if ( ! check_overwrite( task, dest_file,
                                    &dest_exists, &new_dest_file ) )
            {
                close( rfd );
                goto _return_;
            }

            if ( new_dest_file )
            {
                dest_file = new_dest_file;
                g_mutex_lock( task->mutex );
                string_copy_free( &task->current_dest, dest_file );
                g_mutex_unlock( task->mutex );
            }

            //MOD if dest is a symlink, delete it first to prevent overwriting target!
            if ( g_file_test( dest_file, G_FILE_TEST_IS_SYMLINK ) )
            {
                result = unlink( dest_file );
                if ( result )
                {
                    vfs_file_task_error( task, errno, _("Removing"), dest_file );
                    close( rfd );
                    goto _return_;
                }                
            }
            
            if ( ( wfd = creat( dest_file,
                                file_stat.st_mode | S_IWUSR ) ) >= 0 )
            {
                // sshfs becomes unresponsive with this, nfs is okay with it
                //if ( task->avoid_changes )
                //    emit_created( dest_file );
                struct utimbuf times;
                // Try to clone first, then fall back to copying
                if ( ! clone_file( wfd, rfd ) )
                {
                    g_mutex_lock( task->mutex );
                    task->progress += file_stat.st_size;
                    g_mutex_unlock( task->mutex );
                }
                else
                {
                    while ( ( rsize = read( rfd, buffer, sizeof( buffer ) ) ) > 0 )
                    {
                        if ( should_abort( task ) )
                        {
                            copy_fail = TRUE;
                            break;
                        }

                        if ( write( wfd, buffer, rsize ) > 0 )
                        {
                            g_mutex_lock( task->mutex );
                            task->progress += rsize;
                            g_mutex_unlock( task->mutex );
                        }
                        else
                        {
                            vfs_file_task_error( task, errno, _("Writing"), dest_file );
                            copy_fail = TRUE;
                            break;
                        }
                    }
                }
                close( wfd );
                if ( copy_fail )
                {
                    result = unlink( dest_file );
                    if ( result && errno != 2 /* no such file */ )
                    {
                        vfs_file_task_error( task, errno, _("Removing"), dest_file );
                        copy_fail = TRUE;
                    }
                }
                else
                {
                    //MOD don't chmod link
                    if ( ! g_file_test( dest_file, G_FILE_TEST_IS_SYMLINK ) )
                    {
                        chmod( dest_file, file_stat.st_mode );
                        times.actime = file_stat.st_atime;
                        times.modtime = file_stat.st_mtime;
                        utime( dest_file, &times );
                    }
                    if ( task->avoid_changes )
                        update_file_display( dest_file );
        
                    /* Move files to different device: Need to delete source files */
                    if ( (task->type == VFS_FILE_TASK_MOVE || task->type == VFS_FILE_TASK_TRASH)
                         && !should_abort( task ) )
                    {
                        result = unlink( src_file );
                        if ( result )
                        {
                            vfs_file_task_error( task, errno, _("Removing"), src_file );
                            copy_fail = TRUE;
                        }
                    }
                }
            }
            else
            {
                vfs_file_task_error( task, errno, _("Creating"), dest_file );
                copy_fail = TRUE;
            }
            close( rfd );
        }
        else
        {
            vfs_file_task_error( task, errno, _("Accessing"), src_file );
            copy_fail = TRUE;
        }
    }
    if ( new_dest_file )
        g_free( new_dest_file );
    if ( !copy_fail && task->error_first )
        task->error_first = FALSE;
    return !copy_fail;
_return_:

    if ( new_dest_file )
        g_free( new_dest_file );
    return FALSE;
}

static void
vfs_file_task_copy( char* src_file, VFSFileTask* task )
{
    gchar * file_name;
    gchar* dest_file;

    file_name = g_path_get_basename( src_file );
    dest_file = g_build_filename( task->dest_dir, file_name, NULL );
    g_free(file_name );
    vfs_file_task_do_copy( task, src_file, dest_file );
    g_free(dest_file );
}

static int
vfs_file_task_do_move ( VFSFileTask* task,
                        const char* src_file,
                        const char* dest_file )  //MOD void to int
{
    gchar* new_dest_file = NULL;
    gboolean dest_exists;
    struct stat64 file_stat;
    GDir* dir;
    int result;
    GError* error;

    if ( should_abort( task ) )
        return 0;

    g_mutex_lock( task->mutex );
    string_copy_free( &task->current_file, src_file );
    string_copy_free( &task->current_dest, dest_file );
    task->current_item++;
    g_mutex_unlock( task->mutex );

    /* g_debug( "move \"%s\" to \"%s\"\n", src_file, dest_file ); */
    if ( lstat64( src_file, &file_stat ) == -1 )
    {
        vfs_file_task_error( task, errno, _("Accessing"), src_file );
        return 0;
    }

    if ( should_abort( task ) )
        return 0;

    if ( S_ISDIR( file_stat.st_mode ) && check_dest_in_src( task, src_file ) )
        return 0;

    if ( ! check_overwrite( task, dest_file,
           &dest_exists, &new_dest_file ) )
        return 0;

    if ( new_dest_file )
    {
        dest_file = new_dest_file;
        g_mutex_lock( task->mutex );
        string_copy_free( &task->current_dest, dest_file );
        g_mutex_unlock( task->mutex );
    }

    if ( S_ISDIR( file_stat.st_mode ) && 
                                g_file_test( dest_file, G_FILE_TEST_IS_DIR ) )
    {
        // moving a directory onto a directory that exists
        error = NULL;
        dir = g_dir_open( src_file, 0, &error );
        if ( dir )
        {
            const gchar* file_name;
            gchar* sub_src_file;
            gchar* sub_dest_file;
            while ( ( file_name = g_dir_read_name( dir ) ) )
            {
                if ( should_abort( task ) )
                    break;
                sub_src_file = g_build_filename( src_file, file_name, NULL );
                sub_dest_file = g_build_filename( dest_file, file_name, NULL );
                vfs_file_task_do_move( task, sub_src_file, sub_dest_file );
                g_free( sub_dest_file );
                g_free( sub_src_file );
            }
            g_dir_close( dir );
            // remove moved src dir if empty
            if ( !should_abort( task ) )
                rmdir( src_file );
        }
        else if ( error )
        {
            char* msg = g_strdup_printf( "\n%s\n", error->message );
            g_error_free( error );
            vfs_file_task_exec_error( task, 0, msg );
            g_free( msg );
        }
        return 0;
    }

    result = rename( src_file, dest_file );

    if ( result != 0 )
    {
        if ( result == -1 && errno == EXDEV )  //MOD Invalid cross-link device
            return 18;
        vfs_file_task_error( task, errno, _("Renaming"), src_file );
        if ( should_abort( task ) )
        {
            g_free( new_dest_file );
            return 0;
        }
    }
    //MOD don't chmod link
    else if ( ! g_file_test( dest_file, G_FILE_TEST_IS_SYMLINK ) )
        chmod( dest_file, file_stat.st_mode );
    
    g_mutex_lock( task->mutex );
    task->progress += file_stat.st_size;
    if ( task->error_first )
        task->error_first = FALSE;
    g_mutex_unlock( task->mutex );

    if ( new_dest_file )
        g_free( new_dest_file );
    return 0;
}


static void
vfs_file_task_move( char* src_file, VFSFileTask* task )
{
    struct stat64 src_stat;
    struct stat64 dest_stat;
    gchar* file_name;
    gchar* dest_file;
    GKeyFile* kf;   /* for trash info */
    int tmpfd = -1;

    if ( should_abort( task ) )
        return ;

    g_mutex_lock( task->mutex );
    string_copy_free( &task->current_file, src_file );
    g_mutex_unlock( task->mutex );

    file_name = g_path_get_basename( src_file );
    if( task->type == VFS_FILE_TASK_TRASH )
    {
        dest_file = g_strconcat( task->dest_dir, "/",  file_name, "XXXXXX", NULL );
        tmpfd = mkstemp( dest_file );
        if( tmpfd < 0 )
        {
            goto on_error;
        }
        g_debug( dest_file, NULL );
    }
    else
        dest_file = g_build_filename( task->dest_dir, file_name, NULL );

    g_free(file_name );

    if ( lstat64( src_file, &src_stat ) == 0
            && stat64( task->dest_dir, &dest_stat ) == 0 )
    {
        /* Not on the same device */
        if ( src_stat.st_dev != dest_stat.st_dev )
        {
            /* g_print("not on the same dev: %s\n", src_file); */
            vfs_file_task_do_copy( task, src_file, dest_file );
        }
        /*
        else if ( S_ISDIR( src_stat.st_mode ) && 
                                    g_file_test( dest_file, G_FILE_TEST_IS_DIR) )
        {
            // moving a directory onto a directory that exists
        }
        */
        else
        {
            /* g_print("on the same dev: %s\n", src_file); */
            if ( vfs_file_task_do_move( task, src_file, dest_file ) == EXDEV )  //MOD
            {
                //MOD Invalid cross-device link (st_dev not always accurate test)
                // so now redo move as copy
                vfs_file_task_do_copy( task, src_file, dest_file );
            }
        }
    }
    else
        vfs_file_task_error( task, errno, _("Accessing"), src_file );

on_error:
    if( tmpfd >= 0 )
    {
        close( tmpfd );
        unlink( dest_file );
    }
    g_free(dest_file );
}

static void
vfs_file_task_delete( char* src_file, VFSFileTask* task )
{
    GDir * dir;
    const gchar* file_name;
    gchar* sub_src_file;
    struct stat64 file_stat;
    int result;
    GError *error;

    if ( should_abort( task ) )
        return ;

    g_mutex_lock( task->mutex );
    string_copy_free( &task->current_file, src_file );
    task->current_item++;
    g_mutex_unlock( task->mutex );

    if ( lstat64( src_file, &file_stat ) == -1 )
    {
        vfs_file_task_error( task, errno, _("Accessing"), src_file );
        return;
    }

    if ( S_ISDIR( file_stat.st_mode ) )
    {
        error = NULL;
        dir = g_dir_open( src_file, 0, &error );
        if ( dir )
        {
            while ( (file_name = g_dir_read_name( dir )) )
            {
                if ( should_abort( task ) )
                    break;
                sub_src_file = g_build_filename( src_file, file_name, NULL );
                vfs_file_task_delete( sub_src_file, task );
                g_free(sub_src_file );
            }
            g_dir_close( dir );
        }
        else if ( error )
        {
            char* msg = g_strdup_printf( "\n%s\n", error->message );
            g_error_free( error );
            vfs_file_task_exec_error( task, 0, msg );
            g_free( msg );
        }

        if ( should_abort( task ) )
            return ;
        result = rmdir( src_file );
        if ( result != 0 )
        {
            vfs_file_task_error( task, errno, _("Removing"), src_file );
            return ;
        }
    }
    else
    {
        result = unlink( src_file );
        if ( result != 0 )
        {
            vfs_file_task_error( task, errno, _("Removing"), src_file );
            return ;
        }
    }
    g_mutex_lock( task->mutex );
    task->progress += file_stat.st_size;
    if ( task->error_first )
        task->error_first = FALSE;
    g_mutex_unlock( task->mutex );
}

static void
vfs_file_task_link( char* src_file, VFSFileTask* task )
{
    struct stat64 src_stat;
    int result;
    gchar* old_dest_file;
    gchar* dest_file;
    gchar* file_name;
    gboolean dest_exists;  //MOD
    char* new_dest_file = NULL;  //MOD

    if ( should_abort( task ) )
        return ;
    file_name = g_path_get_basename( src_file );
    old_dest_file = g_build_filename( task->dest_dir, file_name, NULL );
    g_free(file_name );
    dest_file = old_dest_file;
    
    //MOD  setup task for check overwrite
    if ( should_abort( task ) )
        return ;
    g_mutex_lock( task->mutex );
    string_copy_free( &task->current_file, src_file );
    string_copy_free( &task->current_dest, old_dest_file );
    task->current_item++;
    g_mutex_unlock( task->mutex );
    
    if ( stat64( src_file, &src_stat ) == -1 )
    {
        //MOD allow link to broken symlink
        if ( errno != 2 || ! g_file_test( src_file, G_FILE_TEST_IS_SYMLINK ) )  //MOD
        {
            vfs_file_task_error( task, errno, _("Accessing"), src_file );
            if ( should_abort( task ) )
                return ;
        }
    }

    /* FIXME: Check overwrite!! */  //MOD added check overwrite
    if ( ! check_overwrite( task, dest_file,
                              &dest_exists, &new_dest_file ) )
        return;

    if ( new_dest_file )
    {
        dest_file = new_dest_file;
        g_mutex_lock( task->mutex );
        string_copy_free( &task->current_dest, dest_file );
        g_mutex_unlock( task->mutex );
    }
    
    //MOD if dest exists, delete it first to prevent exists error
    if ( dest_exists )
    {
        result = unlink( dest_file );
        if ( result )
        {
            vfs_file_task_error( task, errno, _("Removing"), dest_file );
            return;
        }                
    }

    result = symlink( src_file, dest_file );
    if ( result )
    {
        vfs_file_task_error( task, errno, _("Creating Link"), dest_file );
        if ( should_abort( task ) )
            return ;
    }

    g_mutex_lock( task->mutex );
    task->progress += src_stat.st_size;
    if ( task->error_first )
        task->error_first = FALSE;
    g_mutex_unlock( task->mutex );

    if ( new_dest_file )
        g_free( new_dest_file );
    g_free( old_dest_file );
}

static void
vfs_file_task_chown_chmod( char* src_file, VFSFileTask* task )
{
    struct stat64 src_stat;
    int i;
    GDir* dir;
    gchar* sub_src_file;
    const gchar* file_name;
    mode_t new_mode;
    int result;
    GError* error;

    if( should_abort( task ) )
        return ;
    g_mutex_lock( task->mutex );
    string_copy_free( &task->current_file, src_file );
    task->current_item++;
    g_mutex_unlock( task->mutex );
    /* g_debug("chmod_chown: %s\n", src_file); */

    if ( lstat64( src_file, &src_stat ) == 0 )
    {
        /* chown */
        if ( task->uid != -1 || task->gid != -1 )
        {
            result = chown( src_file, task->uid, task->gid );
            if ( result != 0 )
            {
                vfs_file_task_error( task, errno, "chown", src_file );
                if ( should_abort( task ) )
                    return ;
            }
        }

        /* chmod */
        if ( task->chmod_actions )
        {
            new_mode = src_stat.st_mode;
            for ( i = 0; i < N_CHMOD_ACTIONS; ++i )
            {
                if ( task->chmod_actions[ i ] == 2 )            /* Don't change */
                    continue;
                if ( task->chmod_actions[ i ] == 0 )            /* Remove this bit */
                    new_mode &= ~chmod_flags[ i ];
                else  /* Add this bit */
                    new_mode |= chmod_flags[ i ];
            }
            if ( new_mode != src_stat.st_mode )
            {
                result = chmod( src_file, new_mode );
                if ( result != 0 )
                {
                    vfs_file_task_error( task, errno, "chmod", src_file );
                    if ( should_abort( task ) )
                        return ;
                }
            }
        }

        g_mutex_lock( task->mutex );
        task->progress += src_stat.st_size;
        g_mutex_unlock( task->mutex );

        if ( task->avoid_changes )
            update_file_display( src_file );

        if ( S_ISDIR( src_stat.st_mode ) && task->recursive )
        {
            error = NULL;
            dir = g_dir_open( src_file, 0, &error );
            if ( dir )
            {
                while ( (file_name = g_dir_read_name( dir )) )
                {
                    if ( should_abort( task ) )
                        break;
                    sub_src_file = g_build_filename( src_file, file_name, NULL );
                    vfs_file_task_chown_chmod( sub_src_file, task );
                    g_free(sub_src_file );
                }
                g_dir_close( dir );
            }
            else if ( error )
            {
                char* msg = g_strdup_printf( "\n%s\n", error->message );
                g_error_free( error );
                vfs_file_task_exec_error( task, 0, msg );
                g_free( msg );
                if ( should_abort( task ) )
                    return ;
            }
            else
            {
                vfs_file_task_error( task, errno, _("Accessing"), src_file );
                if ( should_abort( task ) )
                    return ;
            }
        }
    }
    if ( task->error_first )
        task->error_first = FALSE;
}

char* vfs_file_task_get_cpids( GPid pid )
{   // get child pids recursively as multi-line string
    char* nl;
    char* pids;
    char* pida;
    GPid pidi;
    char* stdout = NULL;
    char* cpids;
    char* gcpids;
    char* old_cpids;
    
    if ( !pid )
        return NULL;
        
    char* command = g_strdup_printf( "/bin/ps h --ppid %d -o pid", pid );
    gboolean ret = g_spawn_command_line_sync( command, &stdout, NULL, NULL, NULL );
    g_free(command );
    if ( ret && stdout && stdout[0] != '\0' && strchr( stdout, '\n' ) )
    {
        cpids = g_strdup( stdout );
        // get grand cpids recursively
        pids = stdout;
        while ( nl = strchr( pids, '\n' ) )
        {
            nl[0] = '\0';
            pida = g_strdup( pids );
            nl[0] = '\n';
            pids = nl + 1;
            pidi = atoi( pida );
            g_free(pida );
            if ( pidi )
            {
                if ( gcpids = vfs_file_task_get_cpids( pidi ) )
                {
                    old_cpids = cpids;
                    cpids = g_strdup_printf( "%s%s", old_cpids, gcpids );
                    g_free(old_cpids );
                    g_free(gcpids );
                }                
            }
        }
        g_free(stdout );
        //printf("vfs_file_task_get_cpids %d\n[\n%s]\n", pid, cpids );
        return cpids;
    }
    if ( stdout )
        g_free(stdout );
    return NULL;
}

void vfs_file_task_kill_cpids( char* cpids, int signal )
{
    char* nl;
    char* pids;
    char* pida;
    GPid pidi;
    
    if ( !signal || !cpids || cpids[0] == '\0' )
        return;
        
    pids = cpids;
    while ( nl = strchr( pids, '\n' ) )
    {
        nl[0] = '\0';
        pida = g_strdup( pids );
        nl[0] = '\n';
        pids = nl + 1;
        pidi = atoi( pida );
        g_free(pida );
        if ( pidi )
        {
            //printf("KILL_CPID %d %d\n", pidi, signal );
            kill( pidi, signal );
        }
    }
}

static void cb_exec_child_cleanup( GPid pid, gint status, char* tmp_file )
{   // delete tmp files after async task terminates
//printf("cb_exec_child_cleanup pid=%d status=%d file=%s\n", pid, status, tmp_file );
    g_spawn_close_pid( pid );
    if ( tmp_file )
    {
        unlink( tmp_file );
        g_free( tmp_file );
    }
    printf("async child finished  pid=%d\n", pid );
//printf("cb_exec_child_cleanup DONE\n", pid, status);
}

static void cb_exec_child_watch( GPid pid, gint status, VFSFileTask* task )
{
    gboolean bad_status = FALSE;
    g_spawn_close_pid( pid );
    task->exec_pid = 0;
    task->child_watch = 0;

    if ( status )
    {
        if ( WIFEXITED( status ) )
            task->exec_exit_status = WEXITSTATUS( status );
        else
        {
            bad_status = TRUE;
            task->exec_exit_status = -1;
        }
    }
    else
        task->exec_exit_status = 0;

    if ( !task->exec_keep_tmp )
    {
        if ( task->exec_script )
            unlink( task->exec_script );
    }
    printf("child finished  pid=%d exit_status=%d\n", pid,
                                    bad_status ? -1 : task->exec_exit_status );
    if ( !task->exec_exit_status && !bad_status )
    {
        if ( !task->custom_percent )
            task->percent = 100;
    }
    else
        call_state_callback( task, VFS_FILE_TASK_ERROR );
    
    if ( bad_status || ( !task->exec_channel_out && !task->exec_channel_err ) )
        call_state_callback( task, VFS_FILE_TASK_FINISH );
}

static gboolean cb_exec_out_watch( GIOChannel *channel, GIOCondition cond,
                              VFSFileTask* task )
{

/*
printf("cb_exec_out_watch %p\n", channel);
if ( cond & G_IO_IN )
    printf("    G_IO_IN\n");
if ( cond & G_IO_OUT )
    printf("    G_IO_OUT\n");
if ( cond & G_IO_PRI )
    printf("    G_IO_PRI\n");
if ( cond & G_IO_ERR )
    printf("    G_IO_ERR\n");
if ( cond & G_IO_HUP )
    printf("    G_IO_HUP\n");
if ( cond & G_IO_NVAL )
    printf("    G_IO_NVAL\n");

if ( !( cond & G_IO_NVAL ) )
{
    gint fd = g_io_channel_unix_get_fd( channel );
    printf("    fd=%d\n", fd);
    if ( fcntl(fd, F_GETFL) != -1 || errno != EBADF )
    {
        int flags = g_io_channel_get_flags( channel );
        if ( flags & G_IO_FLAG_IS_READABLE )
            printf( "    G_IO_FLAG_IS_READABLE\n");
    }
    else
        printf("    Invalid FD\n");
}
*/

    gchar *line;
    gsize  size;
    GtkTextIter iter;

    if ( ( cond & G_IO_NVAL ) ) 
    {
        g_io_channel_unref( channel );
        return FALSE;
    }
    else if ( !( cond & G_IO_IN ) )
    {
        if ( ( cond & G_IO_HUP ) )
            goto _unref_channel;
        else
            return TRUE;
    }
    else if ( !( fcntl( g_io_channel_unix_get_fd( channel ), F_GETFL ) != -1
                                                    || errno != EBADF ) )
    {
        // bad file descriptor - occurs with stop on fast output
        goto _unref_channel;
    }

    //GError *error = NULL;
    gchar buf[2048];
    if ( g_io_channel_read_chars( channel, buf, sizeof( buf ), &size, NULL ) ==
                                                G_IO_STATUS_NORMAL && size > 0 )
    {
        //gtk_text_buffer_get_iter_at_mark( task->exec_err_buf, &iter,
        //                                                    task->exec_mark_end );
        if ( task->exec_type == VFS_EXEC_UDISKS
                                    && task->exec_show_error //prevent progress_cb opening taskmanager
                                    && g_strstr_len( buf, size, "ount failed:" ) )
        {
            // bug in udisks - exit status not set
            if ( size > 81 && !strncmp( buf, "Mount failed: Error mounting: mount exited with exit code 1: helper failed with:\n", 81 ) )  //cleanup output - useless line
                //gtk_text_buffer_insert( task->exec_err_buf, &iter, buf + 81, size - 81 );
                append_add_log( task, buf + 81, size - 81 );
            else
                //gtk_text_buffer_insert( task->exec_err_buf, &iter, buf, size );
                append_add_log( task, buf, size );
            call_state_callback( task, VFS_FILE_TASK_ERROR );
        }
        else  // no error
            //gtk_text_buffer_insert( task->exec_err_buf, &iter, buf, size );
            append_add_log( task, buf, size );
        //task->err_count++;   //notify of new output - does not indicate error for exec
    }
    else
        printf("cb_exec_out_watch: g_io_channel_read_chars != G_IO_STATUS_NORMAL\n");

/*
    // this hangs ????
    GError *error = NULL;
    gchar* buf;
    printf("g_io_channel_read_to_end\n");
    if ( g_io_channel_read_to_end( channel, &buf, &size, &error ) ==
                                                        G_IO_STATUS_NORMAL )
    {
        gtk_text_buffer_get_iter_at_mark( task->exec_err_buf, &iter,
                                                            task->exec_mark_end );
        gtk_text_buffer_insert( task->exec_err_buf, &iter, buf, size );
        g_free(buf );
        task->err_count++;
        task->ticks = 10000;
    }
    else
        printf("cb_exec_out_watch: g_io_channel_read_to_end != G_IO_STATUS_NORMAL\n");
    printf("g_io_channel_read_to_end DONE\n");
*/

/*
    // this works except that it blocks when a linefeed is not in buffer
    //   eg echo -n aaaa  unless NONBLOCK set
    if ( g_io_channel_read_line( channel, &line, &size, NULL, NULL ) ==
                                                    G_IO_STATUS_NORMAL )
    {
        //printf("    line=%s", line );
        gtk_text_buffer_get_iter_at_mark( task->exec_err_buf, &iter,
                                                            task->exec_mark_end );
        if ( task->exec_type == VFS_EXEC_UDISKS && strstr( line, "ount failed:" ) )
        {
            // bug in udisks - exit status not set
            call_state_callback( task, VFS_FILE_TASK_ERROR );
            if ( !strstr( line, "helper failed with:" ) ) //cleanup udisks output
                gtk_text_buffer_insert( task->exec_err_buf, &iter, line, -1 );
        }
        else
            gtk_text_buffer_insert( task->exec_err_buf, &iter, line, -1 );

        g_free(line );
        task->err_count++; //signal new output
        task->ticks = 10000;
    }
    else
        printf("cb_exec_out_watch: g_io_channel_read_line != G_IO_STATUS_NORMAL\n");
*/

    // don't enable this or last lines are lost
    //if ( ( cond & G_IO_HUP ) )  // put here in case both G_IO_IN and G_IO_HUP
    //    goto _unref_channel;

    return TRUE;
    
_unref_channel:
    g_io_channel_unref( channel );
    if ( channel == task->exec_channel_out )
        task->exec_channel_out = 0;
    else if ( channel == task->exec_channel_err )
        task->exec_channel_err = 0;
    if ( !task->exec_channel_out && !task->exec_channel_err && !task->exec_pid )
        call_state_callback( task, VFS_FILE_TASK_FINISH );
    return FALSE;
}

char* get_sha256sum( char* path )
{
    char* sha256sum = g_find_program_in_path( "/usr/bin/sha256sum" );
    if ( !sha256sum )
    {
        g_warning( _("Please install /usr/bin/sha256sum so I can improve your security while running root commands\n") );
        return NULL;
    }
    
    char* stdout;
    char* sum;
    char* cmd = g_strdup_printf( "%s %s", sha256sum, path );
    if ( g_spawn_command_line_sync( cmd, &stdout, NULL, NULL, NULL ) )
    {
        sum = g_strndup( stdout, 64 );
        g_free( stdout );
        if ( strlen( sum ) != 64 )
        {
            g_free( sum );
            sum = NULL;
        }
    }
    g_free( cmd );
    return sum;    
}

void vfs_file_task_exec_error( VFSFileTask* task, int errnox, char* action )
{
    char* msg;

    if ( errnox )
        msg = g_strdup_printf( "%s\n%s\n", action, g_strerror( errnox ) );
    else
        msg = g_strdup_printf( "%s\n", action );

    append_add_log( task, msg, -1 );
    g_free( msg );
    call_state_callback( task, VFS_FILE_TASK_ERROR );
}

static void vfs_file_task_exec( char* src_file, VFSFileTask* task )
{
    // this function is now thread safe but is not currently run in
    // another thread because gio adds watches to main loop thread anyway
    char* su = NULL;
    char* gsu = NULL;
    char* str;
    const char* tmp;
    char* hex8;
    char* hexname;
    int result;
    FILE* file;
    char* terminal = NULL;
    char** terminalv = NULL;
    const char* value;
    char* sum_script = NULL;
    GtkWidget* parent = NULL;
    gboolean success;
    int i;
    char buf[ PATH_MAX + 1 ];

//printf("vfs_file_task_exec\n");
//task->exec_keep_tmp = TRUE;

    g_mutex_lock( task->mutex );
    value = task->current_dest;  // variable value temp storage
    task->current_dest = NULL;

    if ( task->exec_browser )
        parent = gtk_widget_get_toplevel( task->exec_browser );
    else if ( task->exec_desktop )
        parent = gtk_widget_get_toplevel( task->exec_desktop );

    task->state = VFS_FILE_TASK_RUNNING;
    string_copy_free( &task->current_file, src_file );
    task->total_size = 0;
    task->percent = 0;
    g_mutex_unlock( task->mutex );
    
    if ( should_abort( task ) )
        return;

    // need su?
    if ( task->exec_as_user )
    {
        if ( geteuid() == 0 && !strcmp( task->exec_as_user, "root" ) )
        {
            // already root so no su
            g_free( task->exec_as_user );
            task->exec_as_user = NULL;
        }
        else
        {
            // get su programs
            su = get_valid_su();
            if ( !su )
            {
                str = _("Please configure a valid Terminal SU command in View|Preferences|Advanced");
                g_warning ( str, NULL );
                // do not use xset_msg_dialog if non-main thread
                //vfs_file_task_exec_error( task, 0, str );
                xset_msg_dialog( parent, GTK_MESSAGE_ERROR,
                            _("Terminal SU Not Available"), NULL, 0, str, NULL, NULL );
                goto _exit_with_error_lean;
            }
            gsu = get_valid_gsu();
            if ( !gsu )
            {
                str = _("Please configure a valid Graphical SU command in View|Preferences|Advanced");
                g_warning ( str, NULL );
                // do not use xset_msg_dialog if non-main thread
                //vfs_file_task_exec_error( task, 0, str );
                xset_msg_dialog( parent, GTK_MESSAGE_ERROR,
                            _("Graphical SU Not Available"), NULL, 0, str, NULL, NULL );
                goto _exit_with_error_lean;
            }
        }
    }
    
    // make tmpdir
    if ( geteuid() != 0 && task->exec_as_user && !strcmp( task->exec_as_user, "root" ) )
        tmp = xset_get_shared_tmp_dir();
    else
        tmp = xset_get_user_tmp_dir();
 
    if ( !tmp || ( tmp && !g_file_test( tmp, G_FILE_TEST_IS_DIR ) ) )
    {
        str = _("Cannot create temporary directory");
        g_warning ( str, NULL );
        // do not use xset_msg_dialog if non-main thread
        //vfs_file_task_exec_error( task, 0, str );
        xset_msg_dialog( parent, GTK_MESSAGE_ERROR,
                                _("Error"), NULL, 0, str, NULL, NULL );
        goto _exit_with_error_lean;
    }
    
    // get terminal if needed
    if ( !task->exec_terminal && task->exec_as_user )
    {
        if ( !strcmp( gsu, "/bin/su" ) || !strcmp( gsu, "/usr/bin/sudo" ) )
        {
            // using a non-GUI gsu so run in terminal
            if ( su )
                g_free( su );
            su = strdup( gsu );
            task->exec_terminal = TRUE;
        }
    }
    if ( task->exec_terminal )
    {
        // get terminal
        str = g_strdup( xset_get_s( "main_terminal" ) );
        g_strstrip( str );
        terminalv = g_strsplit( str, " ", 0 );
        g_free( str );
        if ( terminalv && terminalv[0] && terminalv[0][0] != '\0' )
            terminal = g_find_program_in_path( terminalv[0] );
        if ( !( terminal && terminal[0] == '/' ) )
        {
            str = _("Please set a valid terminal program in View|Preferences|Advanced");
            g_warning ( str, NULL );
            // do not use xset_msg_dialog if non-main thread
            //vfs_file_task_exec_error( task, 0, str );
            xset_msg_dialog( parent, GTK_MESSAGE_ERROR,
                            _("Terminal Not Available"), NULL, 0, str, NULL, NULL );
            goto _exit_with_error_lean;
        }
        // resolve x-terminal-emulator link (may be recursive link)
        else if ( strstr( terminal, "x-terminal-emulator" ) &&
                                        realpath( terminal, buf ) != NULL )
        {
            g_free( terminal );
            g_free( terminalv[0] );
            terminal = g_strdup( buf );
            terminalv[0] = g_strdup( buf );
        }
    }

    // Build exec script
    if ( !task->exec_direct )
    {
        // get script name
        do
        {
            if ( task->exec_script )
                g_free( task->exec_script );
            hex8 = randhex8();
            hexname = g_strdup_printf( "%s-tmp.sh", hex8 );
            task->exec_script = g_build_filename( tmp, hexname, NULL );
            g_free( hexname );
            g_free( hex8 );
        }
        while ( g_file_test( task->exec_script, G_FILE_TEST_EXISTS ) ); 

        // open file
        file = fopen( task->exec_script, "w" );
        if ( !file ) goto _exit_with_error;

        // build - header
        result = fprintf( file, "#!%s\n#\n# Temporary SpaceFM exec script - it is safe to delete this file\n\n", BASHPATH );
        if ( result < 0 ) goto _exit_with_error;
        
        // build - exports
        if ( task->exec_export && ( task->exec_browser || task->exec_desktop ) )
        {
            if ( task->exec_browser )
                success = main_write_exports( task, value, file );
            else
#ifdef DESKTOP_INTEGRATION
                success = desktop_write_exports( task, value, file );
#else
                success = FALSE;
#endif
            if ( !success ) goto _exit_with_error;
        }
        else
        {
            if ( task->exec_export && !task->exec_browser && !task->exec_desktop )
            {
                task->exec_export = FALSE;
                g_warning( "exec_export set without exec_browser/exec_desktop" );
            }
        }

        // build - run
        result = fprintf( file, "# run\n\nif [ \"$1\" == \"run\" ]; then\n\n" );
        if ( result < 0 ) goto _exit_with_error;
        
        // build - write root settings
        if ( task->exec_write_root && geteuid() != 0 )
        {
            const char* this_user = g_get_user_name();
            if ( this_user && this_user[0] != '\0' )
            {
                char* root_set_path= g_strdup_printf(
                                "%s/spacefm/%s-as-root", SYSCONFDIR, this_user );
                write_root_settings( file, root_set_path );
                g_free( root_set_path );
                //g_free( this_user );  DON'T
            }
            else
            {
                char* root_set_path= g_strdup_printf(
                                "%s/spacefm/%d-as-root", SYSCONFDIR, geteuid() );
                write_root_settings( file, root_set_path );
                g_free( root_set_path );
            }
        }

        // build - export vars
        if ( task->exec_export )
            result = fprintf( file, "export fm_import='source %s'\n", task->exec_script );
        else
            result = fprintf( file, "export fm_import=''\n" );
        if ( result < 0 ) goto _exit_with_error;
        result = fprintf( file, "export fm_source='%s'\n\n", task->exec_script );
        if ( result < 0 ) goto _exit_with_error;

        // build - trap rm
        /* These terminals provide no option to start a new instance, child
         * exit occurs immediately so can't delete tmp files.  So keep files
         * and let trap delete on exit.

         * These terminals will not work properly with Run As Task.
         * ! WHEN CHANGING THIS LIST, also see similar checks in pref-dialog.c
         * and ptk-location-view.c.
        
         * Note for konsole:  if you create a link to it and execute the
         * link, it will start a new instance (might also work for lxterminal?)
         * http://www.linuxjournal.com/content/start-and-control-konsole-dbus
         * 
         * gnome-terminal removed --disable-factory option as of 3.10
         * https://github.com/IgnorantGuru/spacefm/issues/428
        */
        if ( !task->exec_keep_tmp && terminal && 
                                ( strstr( terminal, "lxterminal" ) ||
                                  strstr( terminal, "urxvtc" ) ||    // sure no option avail?
                                  strstr( terminal, "konsole" ) ||
                                  strstr( terminal, "gnome-terminal" ) ) )
                                  /* when changing this list adjust also
                                   * ptk-location-view.c:ptk_location_view_mount_network()
                                   * pref-dialog.c Line ~777 */
        {
            result = fprintf( file, "trap \"rm -f %s; exit\" EXIT SIGINT SIGTERM SIGQUIT SIGHUP\n\n",
                                                    task->exec_script );
            if ( result < 0 ) goto _exit_with_error;
            task->exec_keep_tmp = TRUE;
        }
        else if ( !task->exec_keep_tmp && geteuid() != 0 && task->exec_as_user
                                        && !strcmp( task->exec_as_user, "root" ) )
        {
            // run as root command, clean up
            result = fprintf( file, "trap \"rm -f %s; exit\" EXIT SIGINT SIGTERM SIGQUIT SIGHUP\n\n",
                                                    task->exec_script );
            if ( result < 0 ) goto _exit_with_error;
        }
        
        // build - command
        printf("\nTASK_COMMAND(%p)=%s\n", task->exec_ptask, task->exec_command );
        result = fprintf( file, "%s\nfm_err=$?\n", task->exec_command );
        if ( result < 0 ) goto _exit_with_error;

        // build - press enter to close
        if ( terminal && task->exec_keep_terminal )
        {
            if ( geteuid() == 0 || 
                    ( task->exec_as_user && !strcmp( task->exec_as_user, "root" ) ) )
                result = fprintf( file, "\necho\necho -n '%s: '\nread s",
                                        "[ Finished ]  Press Enter to close" );
            else 
            {         
                result = fprintf( file, "\necho\necho -n '%s: '\nread s\nif [ \"$s\" = 's' ]; then\n    if [ \"$(whoami)\" = \"root\" ]; then\n        echo\n        echo '[ %s ]'\n    fi\n    echo\n    %s\nfi\n\n", "[ Finished ]  Press Enter to close or s + Enter for a shell", "You are ROOT", BASHPATH );
            }
            if ( result < 0 ) goto _exit_with_error;
        }

        result = fprintf( file, "\nexit $fm_err\nfi\n" );
        if ( result < 0 ) goto _exit_with_error;
        
        // close file
        result = fclose( file );
        file = NULL;
        if ( result ) goto _exit_with_error;
        
        // set permissions
        if ( task->exec_as_user && strcmp( task->exec_as_user, "root" ) )
            // run as a non-root user
            chmod( task->exec_script,
                            S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH );    
        else
            // run as self or as root
            chmod( task->exec_script, S_IRWXU );
        
        // use checksum
        if ( geteuid() != 0 && ( task->exec_as_user || task->exec_checksum ) )
            sum_script = get_sha256sum( task->exec_script );
    }

    task->percent = 50;

    // Spawn
    GPid pid;
    gchar *argv[35];
    gint out, err;
    int a = 0;
    char* use_su;
    gboolean single_arg = FALSE;
    char* auth = NULL;
    
    if ( terminal )
    {
        // terminal
        argv[a++] = terminal;

        // terminal options - add <=9 options
        for ( i = 0; terminalv[i]; i++ )
        {
            if ( i == 0 || a > 9 || terminalv[i][0] == '\0' )
                g_free( terminalv[i] );
            else
                argv[a++] = terminalv[i];  // steal
        }
        g_free( terminalv );  // all strings freed or stolen
        terminalv = NULL;

        // automatic terminal options
        if ( strstr( terminal, "roxterm" ) )
        {
            argv[a++] = g_strdup_printf( "--disable-sm" );
            argv[a++] = g_strdup_printf( "--separate" );
        }
        else if ( strstr( terminal, "lilyterm" ) )
            argv[a++] = g_strdup_printf( "--separate" );
        else if ( strstr( terminal, "xfce4-terminal" )
                                || g_str_has_suffix( terminal, "/terminal" ) )
            argv[a++] = g_strdup_printf( "--disable-server" );

        // add option to execute command in terminal
        if ( strstr( terminal, "xfce4-terminal" ) 
                                || strstr( terminal, "gnome-terminal" )
                                || strstr( terminal, "terminator" )
                                || g_str_has_suffix( terminal, "/terminal" ) ) // xfce
            argv[a++] = g_strdup( "-x" );
        else if ( strstr( terminal, "sakura" ) )
        {
            argv[a++] = g_strdup( "-x" );
            single_arg = TRUE;
        }
        else
            /* Note: Some terminals accept multiple arguments to -e, whereas
             * others needs the entire command quoted and passed as a single
             * argument to -e.  SpaceFM uses spacefm-auth to run commands,
             * so only a single argument is ever used as the command. */
            argv[a++] = g_strdup( "-e" );
        
        use_su = su;
    }
    else
        use_su = gsu;

    if ( task->exec_as_user )
    {
        // su
        argv[a++] = g_strdup( use_su );
        if ( strcmp( task->exec_as_user, "root" ) )
        {
            if ( !strcmp( use_su, "/usr/bin/su-to-root" ) )
                argv[a++] = g_strdup( "-p" );
            else if ( strcmp( use_su, "/bin/su" ) )
                argv[a++] = g_strdup( "-u" );
            argv[a++] = g_strdup( task->exec_as_user );
        }

        if ( !strcmp( use_su, "/usr/bin/gksu" ) || !strcmp( use_su, "/usr/bin/gksudo" ) )
        {
            // gksu*
            argv[a++] = g_strdup( "-g" );
            argv[a++] = g_strdup( "-D" );
            argv[a++] = g_strdup( "SpaceFM Command" );
            single_arg = TRUE;
        }
        else if ( strstr( use_su, "kdesu" ) )
        {
            // kdesu kdesudo
            argv[a++] = g_strdup( "-d" );
            argv[a++] = g_strdup( "-c" );
            single_arg = TRUE;
        }
        else if ( !strcmp( use_su, "/bin/su" ) )
        {
            // /bin/su
            argv[a++] = g_strdup( "-s" );
            argv[a++] = g_strdup( BASHPATH );  //shell spec
            argv[a++] = g_strdup( "-c" );
            single_arg = TRUE;
        }
        else if ( !strcmp( use_su, "/usr/bin/gnomesu" )
                                        || !strcmp( use_su, "/usr/bin/xdg-su" ) )
        {
            // gnomesu
            argv[a++] = g_strdup( "-c" );
            single_arg = TRUE;
        }
        else if ( !strcmp( use_su, "/usr/bin/su-to-root" ) )
        {
            // su-to-root
            if ( !terminal )
                argv[a++] = g_strdup( "-X" );  // command is a X11 program
            argv[a++] = g_strdup( "-c" );
            single_arg = TRUE;
        }
    }

    if ( sum_script )
    {
        // spacefm-auth exists?
        auth = g_find_program_in_path( "spacefm-auth" );
        if ( !auth )
        {
            g_free( sum_script );
            sum_script = NULL;
            g_warning( _("spacefm-auth not found in path - this reduces your security") );
        }
    }
    
    if ( sum_script && auth )
    {
        // spacefm-auth
        if ( single_arg )
        {
            argv[a++] = g_strdup_printf( "%s %s%s %s %s",
                                BASHPATH,
                                auth,
                                !g_strcmp0( task->exec_as_user, "root" ) ?
                                                                " root" : "",
                                task->exec_script,
                                sum_script );
            g_free( auth );
        }
        else
        {
            argv[a++] = g_strdup( BASHPATH );
            argv[a++] = auth;
            if ( !g_strcmp0( task->exec_as_user, "root" ) )
                argv[a++] = g_strdup( "root" );
            argv[a++] = g_strdup( task->exec_script );
            argv[a++] = g_strdup( sum_script );
        }
        g_free( sum_script );
    }
    else if ( task->exec_direct )
    {
        // add direct args - not currently used
        if ( single_arg )
        {
            argv[a++] = g_strjoinv( " ", &task->exec_argv[0] );
            for ( i = 0; i < 7; i++ )
            {
                if ( !task->exec_argv[i] )
                    break;
                g_free( task->exec_argv[i] );
            }
        }
        else
        {
            for ( i = 0; i < 7; i++ )
            {
                if ( !task->exec_argv[i] )
                    break;
                argv[a++] = task->exec_argv[i];
            }
        }
    }
    else
    {
        if ( single_arg )
        {
            argv[a++] = g_strdup_printf( "%s %s run", BASHPATH, task->exec_script );
        }
        else
        {
            argv[a++] = g_strdup( BASHPATH );
            argv[a++] = g_strdup( task->exec_script );
            argv[a++] = g_strdup( "run" );
        }
    }

    argv[a++] = NULL;
    if ( su )
        g_free( su );
    if ( gsu )
        g_free( gsu );

    printf( "SPAWN=" );
    i = 0;
    while ( argv[i] )
    {
        printf( "%s%s", i == 0 ? "" : "  ", argv[i] );
        i++;
    }
    printf( "\n" );

    char* first_arg = g_strdup( argv[0] );
    if ( task->exec_sync )
    {
        result = g_spawn_async_with_pipes( task->dest_dir, argv, NULL,
                                    G_SPAWN_DO_NOT_REAP_CHILD, NULL,
                                    NULL, &pid, NULL, &out, &err, NULL );
    }
    else
    {
        result = g_spawn_async_with_pipes( task->dest_dir, argv, NULL,
                                    G_SPAWN_DO_NOT_REAP_CHILD, NULL,
                                    NULL, &pid, NULL, NULL, NULL, NULL );
    }

    if( !result )
    {
        printf("    result=%d ( %s )\n", errno, g_strerror( errno ));
        if ( !task->exec_keep_tmp && task->exec_sync )
        {
            if ( task->exec_script )
                unlink( task->exec_script );
        }
        str = g_strdup_printf( _("Error executing '%s'\nSee stdout (run spacefm in a terminal) for debug info"), first_arg );
        g_free( first_arg );
        vfs_file_task_exec_error( task, errno, str );
        g_free( str );
        call_state_callback( task, VFS_FILE_TASK_FINISH );
        return;
    }
    else
        printf( "    pid = %d\n", pid );
    g_free( first_arg );

    if ( !task->exec_sync )
    {
        // catch termination to waitpid and delete tmp if needed
        // task can be destroyed while this watch is still active
        g_child_watch_add( pid, (GChildWatchFunc)cb_exec_child_cleanup,
                             !task->exec_keep_tmp && !task->exec_direct &&
                             task->exec_script ?
                                g_strdup( task->exec_script ) : NULL );
        call_state_callback( task, VFS_FILE_TASK_FINISH );
        return;
    }
    
    task->exec_pid = pid;

    // catch termination (always is run in the main loop thread)
    task->child_watch = g_child_watch_add( pid,
                                    (GChildWatchFunc)cb_exec_child_watch, task );

    // create channels for output
    fcntl( out, F_SETFL,O_NONBLOCK );
    fcntl( err, F_SETFL,O_NONBLOCK );
    task->exec_channel_out = g_io_channel_unix_new( out );
    task->exec_channel_err = g_io_channel_unix_new( err );
    g_io_channel_set_close_on_unref( task->exec_channel_out, TRUE );
    g_io_channel_set_close_on_unref( task->exec_channel_err, TRUE );

    // Add watches to channels
    // These are run in the main loop thread so use G_PRIORITY_LOW to not
    // interfere with g_idle_add in vfs-dir/vfs-async-task etc 
    // "Use this for very low priority background tasks. It is not used within
    // GLib or GTK+." 
    g_io_add_watch_full( task->exec_channel_out, G_PRIORITY_LOW,
                        G_IO_IN	| G_IO_HUP | G_IO_NVAL | G_IO_ERR, //want ERR?
                        (GIOFunc)cb_exec_out_watch, task, NULL );
    g_io_add_watch_full( task->exec_channel_err, G_PRIORITY_LOW,
                        G_IO_IN | G_IO_HUP | G_IO_NVAL | G_IO_ERR, //want ERR?
                        (GIOFunc)cb_exec_out_watch, task, NULL );

    // running
    task->state = VFS_FILE_TASK_RUNNING;

/* enable if this function is not in main loop thread
    // wait
    task->exec_cond = g_cond_new();
    g_mutex_lock( task->mutex );
    g_cond_wait( task->exec_cond, task->mutex );
    g_cond_free( task->exec_cond );
    task->exec_cond = NULL;
    g_source_remove( child_watch );
    g_mutex_unlock( task->mutex );
*/
//printf("vfs_file_task_exec DONE\n");
    return;  // exit thread

// out and err can/should be closed too?

_exit_with_error:
    vfs_file_task_exec_error( task, errno, _("Error writing temporary file") );
    if ( file )
        fclose( file );
    if ( !task->exec_keep_tmp )
    {
        if ( task->exec_script )
            unlink( task->exec_script );
    }
_exit_with_error_lean:
    g_strfreev( terminalv );
    g_free( terminal );
    g_free( su );
    g_free( gsu );
    call_state_callback( task, VFS_FILE_TASK_FINISH );
//printf("vfs_file_task_exec DONE ERROR\n");
}

gboolean on_size_timeout( VFSFileTask* task )
{
    if ( !task->abort )
        task->state = VFS_FILE_TASK_SIZE_TIMEOUT;
    return FALSE;
}

static gpointer vfs_file_task_thread ( VFSFileTask* task )
//void * vfs_file_task_thread ( void * ptr )
{
    GList * l;
    struct stat64 file_stat;
    dev_t dest_dev = 0;
    off64_t size;
    GFunc funcs[] = {( GFunc ) vfs_file_task_move,
                     ( GFunc ) vfs_file_task_copy,
                     ( GFunc ) vfs_file_task_move,  /* trash */
                     ( GFunc ) vfs_file_task_delete,
                     ( GFunc ) vfs_file_task_link,
                     ( GFunc ) vfs_file_task_chown_chmod,
                     ( GFunc ) vfs_file_task_exec};
//VFSFileTask* task = (VFSFileTask*)ptr;
    if ( task->type < VFS_FILE_TASK_MOVE
            || task->type >= VFS_FILE_TASK_LAST )
        goto _exit_thread;

    g_mutex_lock( task->mutex );
    task->state = VFS_FILE_TASK_RUNNING;
    string_copy_free( &task->current_file,
                        task->src_paths ? (char*)task->src_paths->data : NULL );
    task->total_size = 0;

    if( task->type == VFS_FILE_TASK_TRASH )
    {
        task->dest_dir = g_build_filename( vfs_get_trash_dir(), "files", NULL );
        /* Create the trash dir if it doesn't exist */
        if( ! g_file_test( task->dest_dir, G_FILE_TEST_EXISTS ) )
            g_mkdir_with_parents( task->dest_dir, 0700 );
    }
    g_mutex_unlock( task->mutex );

    if ( task->abort )
        goto _exit_thread;

    /* Calculate total size of all files */
    guint size_timeout = 0;
    if ( task->recursive )
    {
        // start timer to limit the amount of time to spend on this - can be 
        // VERY slow for network filesystems
        size_timeout = g_timeout_add_seconds( 5,
                                       ( GSourceFunc ) on_size_timeout, task );
        for ( l = task->src_paths; l; l = l->next )
        {
            if ( lstat64( (char*)l->data, &file_stat ) == -1 )
            {
                // don't report error here since its reported later
                //vfs_file_task_error( task, errno, _("Accessing"), (char*)l->data );
            }
            else
            {
                size = 0;
                get_total_size_of_dir( task, ( char* ) l->data, &size, &file_stat );
                g_mutex_lock( task->mutex );
                task->total_size += size;
                g_mutex_unlock( task->mutex );
            }
            if ( task->abort )
                goto _exit_thread;
            if ( task->state == VFS_FILE_TASK_SIZE_TIMEOUT )
                break;
        }
    }
    else if ( task->type != VFS_FILE_TASK_EXEC )
    {
        // start timer to limit the amount of time to spend on this - can be 
        // VERY slow for network filesystems
        size_timeout = g_timeout_add_seconds( 5,
                                       ( GSourceFunc ) on_size_timeout, task );
        if ( task->type != VFS_FILE_TASK_CHMOD_CHOWN )
        {
            if ( !( task->dest_dir && stat64( task->dest_dir, &file_stat ) == 0 ) )
            {
                vfs_file_task_error( task, errno, _("Accessing"), task->dest_dir );
                task->abort = TRUE;
                goto _exit_thread;
            }
            dest_dev = file_stat.st_dev;
        }

        for ( l = task->src_paths; l; l = l->next )
        {
            if ( lstat64( ( char* ) l->data, &file_stat ) == -1 )
            {
                // don't report error here since it's reported later
                //vfs_file_task_error( task, errno, _("Accessing"), ( char* ) l->data );
            }
            else
            {
                /*
                // sfm why does this code change task->recursive back and forth?
                if ( S_ISLNK( file_stat.st_mode ) )      // Don't do deep copy for symlinks
                    task->recursive = FALSE;
                else if ( task->type == VFS_FILE_TASK_MOVE || task->type == VFS_FILE_TASK_TRASH )
                    task->recursive = ( file_stat.st_dev != dest_dev );
                else
                    task->recursive = FALSE;
                if ( task->recursive )
                */
                if ( ( task->type == VFS_FILE_TASK_MOVE ||
                                                task->type == VFS_FILE_TASK_TRASH )
                                                && file_stat.st_dev != dest_dev )
                {
                    // recursive size
                    size = 0;
                    get_total_size_of_dir( task, ( char* ) l->data, &size, &file_stat );
                    g_mutex_lock( task->mutex );
                    task->total_size += size;
                    g_mutex_unlock( task->mutex );
                }
                else
                {
                    g_mutex_lock( task->mutex );
                    task->total_size += file_stat.st_size;
                    g_mutex_unlock( task->mutex );
                }
            }
            if ( task->abort )
                goto _exit_thread;
            if ( task->state == VFS_FILE_TASK_SIZE_TIMEOUT )
                break;
        }
    }

    if ( task->dest_dir && stat64( task->dest_dir, &file_stat ) != -1 )
        add_task_dev( task, file_stat.st_dev );

    if ( task->abort )
        goto _exit_thread;

    // cancel the timer
    if ( size_timeout )
        g_source_remove_by_user_data( task );

    if ( task->state_pause == VFS_FILE_TASK_QUEUE )
    {
        if ( task->state != VFS_FILE_TASK_SIZE_TIMEOUT && xset_get_b( "task_q_smart" ) )
        {
            // make queue exception for smaller tasks
            off64_t exlimit;
            if ( task->type == VFS_FILE_TASK_MOVE || 
                                                task->type == VFS_FILE_TASK_COPY )
                exlimit = 10485760;     // 10M
            else if ( task->type == VFS_FILE_TASK_DELETE || 
                                                task->type == VFS_FILE_TASK_TRASH )            
                exlimit = 5368709120;   // 5G
            else
                exlimit = 0;            // always exception for other types
            if ( !exlimit || task->total_size < exlimit )
                task->state_pause = VFS_FILE_TASK_RUNNING;
        }
        // device list is populated so signal queue start
        task->queue_start = TRUE;
    }
    
    if ( task->state == VFS_FILE_TASK_SIZE_TIMEOUT )
    {
        append_add_log( task, _("Timed out calculating total size\n"), -1 );
        task->total_size = 0;
    }
    task->state = VFS_FILE_TASK_RUNNING;
    if ( should_abort( task ) )
        goto _exit_thread;

    g_list_foreach( task->src_paths,
                    funcs[ task->type ],
                    task );

_exit_thread:
    task->state = VFS_FILE_TASK_RUNNING;
    if ( size_timeout )
        g_source_remove_by_user_data( task );
    if ( task->state_cb )
    {
        call_state_callback( task, VFS_FILE_TASK_FINISH );
    }
/*    else
    {
        // FIXME: Will this cause any problem?
        task->thread = NULL;
        vfs_file_task_free( task );
    }
*/
    return NULL;
}

/*
* source_files sould be a newly allocated list, and it will be
* freed after file operation has been completed
*/
VFSFileTask* vfs_task_new ( VFSFileTaskType type,
                            GList* src_files,
                            const char* dest_dir )
{
    VFSFileTask * task;
    task = g_slice_new0( VFSFileTask );

    task->type = type;
    task->src_paths = src_files;
    task->dest_dir = g_strdup( dest_dir );
    task->current_file = NULL;
    task->current_dest = NULL;
    
    task->recursive = ( task->type == VFS_FILE_TASK_COPY ||
                        task->type == VFS_FILE_TASK_DELETE );

    task->err_count = 0;
    task->abort = FALSE;
    task->error_first = TRUE;
    task->custom_percent = FALSE;
    
    task->exec_type = VFS_EXEC_NORMAL;
    task->exec_action = NULL;
    task->exec_command = NULL;
    task->exec_sync = TRUE;
    task->exec_popup = FALSE;
    task->exec_show_output = FALSE;
    task->exec_show_error = FALSE;
    task->exec_terminal = FALSE;
    task->exec_keep_terminal = FALSE;
    task->exec_export = FALSE;
    task->exec_direct = FALSE;
    task->exec_as_user = NULL;
    task->exec_icon = NULL;
    task->exec_script = NULL;
    task->exec_keep_tmp = FALSE;
    task->exec_browser = NULL;
    task->exec_desktop = NULL;
    task->exec_pid = 0;
    task->child_watch = 0;
    task->exec_is_error = FALSE;
    task->exec_scroll_lock = FALSE;
    task->exec_write_root = FALSE;
    task->exec_checksum = FALSE;
    task->exec_set = NULL;
    task->exec_cond = NULL;
    task->exec_ptask = NULL;
    
    task->pause_cond = NULL;
    task->state_pause = VFS_FILE_TASK_RUNNING;
    task->queue_start = FALSE;
    task->devs = NULL;
    
    task->mutex = g_mutex_new();
    
    GtkTextIter iter;
    task->add_log_buf = gtk_text_buffer_new( NULL );
    task->add_log_end = gtk_text_mark_new( NULL, FALSE );
    gtk_text_buffer_get_end_iter( task->add_log_buf, &iter);
    gtk_text_buffer_add_mark( task->add_log_buf, task->add_log_end, &iter );
    
    task->start_time = time( NULL );
    task->last_speed = 0;
    task->last_progress = 0;
    task->current_item = 0;
    task->timer = g_timer_new();
    task->last_elapsed = 0;
    return task;
}

/* Set some actions for chmod, this array will be copied
* and stored in VFSFileTask */
void vfs_file_task_set_chmod( VFSFileTask* task,
                              guchar* chmod_actions )
{
    task->chmod_actions = g_slice_alloc( sizeof( guchar ) * N_CHMOD_ACTIONS );
    memcpy( ( void* ) task->chmod_actions, ( void* ) chmod_actions,
            sizeof( guchar ) * N_CHMOD_ACTIONS );
}

void vfs_file_task_set_chown( VFSFileTask* task,
                              uid_t uid, gid_t gid )
{
    task->uid = uid;
    task->gid = gid;
}

void vfs_file_task_run ( VFSFileTask* task )
{
    if ( task->type != VFS_FILE_TASK_EXEC )
    {
#ifdef HAVE_HAL
        task->avoid_changes = FALSE;
#else
        if ( task->type == VFS_FILE_TASK_CHMOD_CHOWN && task->src_paths->data )
        {
            char* dir = g_path_get_dirname( (char*)task->src_paths->data );
            task->avoid_changes = vfs_volume_dir_avoid_changes( dir );
            g_free( dir );
        }
        else
            task->avoid_changes = vfs_volume_dir_avoid_changes( task->dest_dir );
#endif
        task->thread = g_thread_create( ( GThreadFunc ) vfs_file_task_thread,
                                        task, TRUE, NULL );
        //task->thread = g_thread_create_full( ( GThreadFunc ) vfs_file_task_thread,
        //                    task, 0, TRUE, TRUE, G_THREAD_PRIORITY_NORMAL, NULL );
        //pthread_t tid;
        //task->thread = pthread_create( &tid, NULL, vfs_file_task_thread, task );
    }
    else
    {
        // don't use another thread for exec since gio adds watches to main
        // loop thread anyway
        task->thread = NULL;
        vfs_file_task_exec( (char*)task->src_paths->data, task );
    }
}

void vfs_file_task_try_abort ( VFSFileTask* task )
{
    task->abort = TRUE;
    if ( task->pause_cond )
    {
        g_mutex_lock( task->mutex );
        g_cond_broadcast( task->pause_cond );
        task->last_elapsed = g_timer_elapsed( task->timer, NULL );
        task->last_progress = task->progress;
        task->last_speed = 0;
        g_mutex_unlock( task->mutex );
    }
    else
    {
        g_mutex_lock( task->mutex );
        task->last_elapsed = g_timer_elapsed( task->timer, NULL );
        task->last_progress = task->progress;
        task->last_speed = 0;
        g_mutex_unlock( task->mutex );
    }
    task->state_pause = VFS_FILE_TASK_RUNNING;
}

void vfs_file_task_abort ( VFSFileTask* task )
{
    task->abort = TRUE;
    /* Called from another thread */
    if ( task->thread && g_thread_self() != task->thread
                                            && task->type != VFS_FILE_TASK_EXEC )
    {
        g_thread_join( task->thread );
        task->thread = NULL;
    }
}

void vfs_file_task_free ( VFSFileTask* task )
{
    if ( task->src_paths )
    {
        g_list_foreach( task->src_paths, ( GFunc ) g_free, NULL );
        g_list_free( task->src_paths );
    }
    g_free( task->dest_dir );
    g_free( task->current_file );
    g_free( task->current_dest );
    g_slist_free( task->devs );

    if ( task->chmod_actions )
        g_slice_free1( sizeof( guchar ) * N_CHMOD_ACTIONS,
                       task->chmod_actions );

    if ( task->exec_action )
        g_free(task->exec_action );
    if ( task->exec_as_user )
        g_free(task->exec_as_user );
    if ( task->exec_command )
        g_free(task->exec_command );
    if ( task->exec_script )
        g_free(task->exec_script );

    g_mutex_free( task->mutex );
    
    gtk_text_buffer_set_text( task->add_log_buf, "", -1 );
    g_object_unref( task->add_log_buf );

    g_timer_destroy( task->timer );
    
    g_slice_free( VFSFileTask, task );
}

void add_task_dev( VFSFileTask* task, dev_t dev )
{
    dev_t parent = 0;
    if ( !g_slist_find( task->devs, GUINT_TO_POINTER( dev ) ) )
    {
#ifndef HAVE_HAL
        parent = get_device_parent( dev );
#endif
//printf("add_task_dev %d:%d\n", major(dev), minor(dev) );
        g_mutex_lock( task->mutex );
        task->devs = g_slist_append( task->devs, GUINT_TO_POINTER( dev ) );
        if ( parent && !g_slist_find( task->devs, GUINT_TO_POINTER( parent ) ) )
        {
//printf("add_task_dev PARENT %d:%d\n", major(parent), minor(parent) );
            task->devs = g_slist_append( task->devs, GUINT_TO_POINTER( parent ) );
        }
        g_mutex_unlock( task->mutex );
    }
}

/*
* void get_total_size_of_dir( const char* path, off_t* size )
* Recursively count total size of all files in the specified directory.
* If the path specified is a file, the size of the file is directly returned.
* cancel is used to cancel the operation. This function will check the value
* pointed by cancel in every iteration. If cancel is set to TRUE, the
* calculation is cancelled.
* NOTE: *size should be set to zero before calling this function.
*/
void get_total_size_of_dir( VFSFileTask* task,
                            const char* path,
                            off64_t* size,
                            struct stat64* have_stat )
{
    GDir * dir;
    const char* name;
    char* full_path;
    struct stat64 file_stat;

    if ( task->abort )
        return;

    if ( have_stat )
        file_stat = *have_stat;
    else if ( lstat64( path, &file_stat ) == -1 )
        return;

    *size += file_stat.st_size;

    // remember device for smart queue
    if ( !task->devs )
        add_task_dev( task, file_stat.st_dev );
    else if ( (uint)file_stat.st_dev != GPOINTER_TO_UINT( task->devs->data ) )
        add_task_dev( task, file_stat.st_dev );

    // Don't follow symlinks
    if ( S_ISLNK( file_stat.st_mode ) || !S_ISDIR( file_stat.st_mode ) )
        return;

    dir = g_dir_open( path, 0, NULL );
    if ( dir )
    {
        while ( (name = g_dir_read_name( dir )) )
        {
            if ( task->state == VFS_FILE_TASK_SIZE_TIMEOUT || task->abort )
                break;
            full_path = g_build_filename( path, name, NULL );
            if ( lstat64( full_path, &file_stat ) != -1 )
            {
                if ( S_ISDIR( file_stat.st_mode ) )
                    get_total_size_of_dir( task, full_path, size, &file_stat );
                else
                    *size += file_stat.st_size;
            }
            g_free(full_path );
        }
        g_dir_close( dir );
    }
}

void vfs_file_task_set_recursive( VFSFileTask* task, gboolean recursive )
{
    task->recursive = recursive;
}

void vfs_file_task_set_overwrite_mode( VFSFileTask* task,
                                       VFSFileTaskOverwriteMode mode )
{
    task->overwrite_mode = mode;
}

void vfs_file_task_set_progress_callback( VFSFileTask* task,
                                          VFSFileTaskProgressCallback cb,
                                          gpointer user_data )
{
    task->progress_cb = cb;
    task->progress_cb_data = user_data;
}

void vfs_file_task_set_state_callback( VFSFileTask* task,
                                       VFSFileTaskStateCallback cb,
                                       gpointer user_data )
{
    task->state_cb = cb;
    task->state_cb_data = user_data;
}

void vfs_file_task_error( VFSFileTask* task, int errnox, const char* action,
                                                            const char* target )
{
    task->error = errnox;
    char* msg = g_strdup_printf( _("\n%s %s\nError: %s\n"), action, target,
                                                        g_strerror( errnox ) );
    append_add_log( task, msg, -1 );
    g_free( msg );
    call_state_callback( task, VFS_FILE_TASK_ERROR );
}

