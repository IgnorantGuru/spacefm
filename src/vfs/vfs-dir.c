/*
*  C Implementation: vfs-dir
*
* Description: Object used to present a directory
*
*
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // euidaccess
#endif

#include "vfs-dir.h"
#include "vfs-thumbnail-loader.h"
#include "glib-mem.h"

#include <glib/gi18n.h>
#include <string.h>

#include <fcntl.h>  /* for open() */

#if defined (__GLIBC__)
#include <malloc.h> /* for malloc_trim */
#endif

#include <unistd.h> /* for read */
#include "vfs-volume.h"


static void vfs_dir_class_init( VFSDirClass* klass );
static void vfs_dir_init( VFSDir* dir );
static void vfs_dir_finalize( GObject *obj );
static void vfs_dir_set_property ( GObject *obj,
                                   guint prop_id,
                                   const GValue *value,
                                   GParamSpec *pspec );
static void vfs_dir_get_property ( GObject *obj,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec );

char* gethidden( const char* path );  //MOD added
gboolean ishidden( const char* hidden, const char* file_name );  //MOD added

/* constructor is private */
static VFSDir* vfs_dir_new( const char* path );

static void vfs_dir_load( VFSDir* dir );
static gpointer vfs_dir_load_thread( VFSAsyncTask* task, VFSDir* dir );

static void vfs_dir_monitor_callback( VFSFileMonitor* fm,
                                      VFSFileMonitorEvent event,
                                      const char* file_name,
                                      gpointer user_data );

#if 0
static gpointer load_thumbnail_thread( gpointer user_data );
#endif

static void on_mime_type_reload( gpointer user_data );


static void update_changed_files( gpointer key, gpointer data,
                                  gpointer user_data );
static gboolean notify_file_change( gpointer user_data );
static gboolean update_file_info( VFSDir* dir, VFSFileInfo* file );

#if 0
static gboolean is_dir_desktop( const char* path );
#endif

static void on_list_task_finished( VFSAsyncTask* task, gboolean is_cancelled, VFSDir* dir );

enum {
    FILE_CREATED_SIGNAL = 0,
    FILE_DELETED_SIGNAL,
    FILE_CHANGED_SIGNAL,
    THUMBNAIL_LOADED_SIGNAL,
    FILE_LISTED_SIGNAL,
    N_SIGNALS
};

static guint signals[ N_SIGNALS ] = { 0 };
static GObjectClass *parent_class = NULL;

static GHashTable* dir_hash = NULL;
static GList* mime_cb = NULL;
static guint change_notify_timeout = 0;
static guint theme_change_notify = 0;

static char* desktop_dir = NULL;
static char* home_trash_dir = NULL;
static size_t home_trash_dir_len = 0;

static gboolean is_desktop_set = FALSE;

GType vfs_dir_get_type()
{
    static GType type = G_TYPE_INVALID;
    if ( G_UNLIKELY ( type == G_TYPE_INVALID ) )
    {
        static const GTypeInfo info =
            {
                sizeof ( VFSDirClass ),
                NULL,
                NULL,
                ( GClassInitFunc ) vfs_dir_class_init,
                NULL,
                NULL,
                sizeof ( VFSDir ),
                0,
                ( GInstanceInitFunc ) vfs_dir_init,
                NULL,
            };
        type = g_type_register_static ( G_TYPE_OBJECT, "VFSDir", &info, 0 );
    }
    return type;
}

void vfs_dir_class_init( VFSDirClass* klass )
{
    GObjectClass * object_class;

    object_class = ( GObjectClass * ) klass;
    parent_class = g_type_class_peek_parent ( klass );

    object_class->set_property = vfs_dir_set_property;
    object_class->get_property = vfs_dir_get_property;
    object_class->finalize = vfs_dir_finalize;

    /* signals */
//    klass->file_created = on_vfs_dir_file_created;
//    klass->file_deleted = on_vfs_dir_file_deleted;
//    klass->file_changed = on_vfs_dir_file_changed;

    /*
    * file-created is emitted when there is a new file created in the dir.
    * The param is VFSFileInfo of the newly created file.
    */
    signals[ FILE_CREATED_SIGNAL ] =
        g_signal_new ( "file-created",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( VFSDirClass, file_created ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1, G_TYPE_POINTER );

    /*
    * file-deleted is emitted when there is a file deleted in the dir.
    * The param is VFSFileInfo of the newly created file.
    */
    signals[ FILE_DELETED_SIGNAL ] =
        g_signal_new ( "file-deleted",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( VFSDirClass, file_deleted ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1, G_TYPE_POINTER );

    /*
    * file-changed is emitted when there is a file changed in the dir.
    * The param is VFSFileInfo of the newly created file.
    */
    signals[ FILE_CHANGED_SIGNAL ] =
        g_signal_new ( "file-changed",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( VFSDirClass, file_changed ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1, G_TYPE_POINTER );

    signals[ THUMBNAIL_LOADED_SIGNAL ] =
        g_signal_new ( "thumbnail-loaded",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( VFSDirClass, thumbnail_loaded ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1, G_TYPE_POINTER );

    signals[ FILE_LISTED_SIGNAL ] =
        g_signal_new ( "file-listed",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( VFSDirClass, file_listed ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__BOOLEAN,
                       G_TYPE_NONE, 1, G_TYPE_BOOLEAN );

    /* FIXME: Is there better way to do this? */
    if( G_UNLIKELY( ! is_desktop_set ) )
        vfs_get_desktop_dir();

    if( ! home_trash_dir )
        vfs_get_trash_dir();
}

/* constructor */
void vfs_dir_init( VFSDir* dir )
{
    dir->mutex = g_mutex_new();
}

/* destructor */

void vfs_dir_finalize( GObject *obj )
{
    VFSDir * dir = VFS_DIR( obj );
//printf("vfs_dir_finalize: dir=%p  %s\n", dir, dir->path );
    do{}
    while( g_source_remove_by_user_data( dir ) );
    if ( dir->path )
    {
        if( G_LIKELY( dir_hash ) )
        {
            g_hash_table_remove( dir_hash, dir->path );

            /* There is no VFSDir instance */
            if ( 0 == g_hash_table_size( dir_hash ) )
            {
                g_hash_table_destroy( dir_hash );
                dir_hash = NULL;

                vfs_mime_type_remove_reload_cb( mime_cb );
                mime_cb = NULL;

                if( change_notify_timeout )
                {
                    g_source_remove( change_notify_timeout );
                    change_notify_timeout = 0;
                }

                g_signal_handler_disconnect( gtk_icon_theme_get_default(),
                                                                theme_change_notify );
                theme_change_notify = 0;
            }
        }
        g_free( dir->path );
        g_free( dir->disp_path );
        g_free( dir->device_info );
        dir->path = dir->disp_path = dir->device_info = NULL;
    }
    if ( dir->monitor )
    {
        vfs_file_monitor_remove( dir->monitor,
                                 vfs_dir_monitor_callback,
                                 dir );
        dir->monitor = NULL;
    }
    if( G_UNLIKELY( dir->task ) )
    {
        g_signal_handlers_disconnect_by_func( dir->task, on_list_task_finished, dir );
        GDK_THREADS_LEAVE();
        vfs_async_task_cancel( dir->task );
        GDK_THREADS_ENTER();
        g_object_unref( dir->task );
        dir->task = NULL;
    }
    /* g_debug( "dir->thumbnail_loader: %p", dir->thumbnail_loader ); */
    if( G_UNLIKELY( dir->thumbnail_loader ) )
    {
        /* g_debug( "FREE THUMBNAIL LOADER IN VFSDIR" ); */
//printf("vfs_thumbnail_loader_free@vfs_dir_finalize %s loader=%p\n", dir->path, dir->thumbnail_loader );
        vfs_thumbnail_loader_free( dir->thumbnail_loader );
        dir->thumbnail_loader = NULL;
    }

    if ( dir->file_list )
    {
        g_list_foreach( dir->file_list, ( GFunc ) vfs_file_info_unref, NULL );
        g_list_free( dir->file_list );
        dir->file_list = NULL;
        dir->n_files = 0;
    }

    if( dir->changed_files )
    {
        g_slist_foreach( dir->changed_files, (GFunc)vfs_file_info_unref, NULL );
        g_slist_free( dir->changed_files );
        dir->changed_files = NULL;
    }

    if( dir->changed_files_delayed )
    {
        g_slist_foreach( dir->changed_files_delayed,
                                            (GFunc)vfs_file_info_unref, NULL );
        g_slist_free( dir->changed_files_delayed );
        dir->changed_files_delayed = NULL;
    }

    if( dir->created_files )
    {
        g_slist_foreach( dir->created_files, (GFunc)g_free, NULL );
        g_slist_free( dir->created_files );
        dir->created_files = NULL;
    }

    if ( dir->delayed_timer )
        g_timer_destroy( dir->delayed_timer );
    
    g_mutex_free( dir->mutex );
    dir->mutex = NULL;
    G_OBJECT_CLASS( parent_class ) ->finalize( obj );
}

void vfs_dir_get_property ( GObject *obj,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec )
{}

void vfs_dir_set_property ( GObject *obj,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec )
{}

static GList* vfs_dir_find_file( VFSDir* dir, const char* file_name, VFSFileInfo* file )
{
    GList * l;
    VFSFileInfo* file2;
    for ( l = dir->file_list; l; l = l->next )
    {
        file2 = ( VFSFileInfo* ) l->data;
        if( G_UNLIKELY( file == file2 ) )
            return l;
        if ( file2->name && 0 == strcmp( file2->name, file_name ) )
        {
            return l;
        }
    }
    return NULL;
}

void vfs_dir_get_deep_size( VFSAsyncTask* task,
                            const char* path,
                            off64_t* size,
                            struct stat64* have_stat,
                            gboolean top )
{   // see also vfs-file-task.c:get_total_size_of_dir()
    GDir * dir;
    const char* name;
    char* full_path;
    struct stat64 file_stat;

    if ( vfs_async_task_is_cancelled( task ) )
        return;

    if ( have_stat )
        file_stat = *have_stat;
    else if ( lstat64( path, &file_stat ) == -1 )
        return;

    *size += file_stat.st_size;

    // Don't follow symlinks
    if ( S_ISLNK( file_stat.st_mode ) || !S_ISDIR( file_stat.st_mode ) )
        return;

    dev_t st_dev = file_stat.st_dev;
    
    dir = g_dir_open( path, 0, NULL );
    if ( dir )
    {
        while ( (name = g_dir_read_name( dir )) )
        {
            if ( vfs_async_task_is_cancelled( task ) )
                break;
            full_path = g_build_filename( path, name, NULL );
            if ( full_path && lstat64( full_path, &file_stat ) != -1 &&
                                        file_stat.st_dev == st_dev )
            {
                if ( S_ISDIR( file_stat.st_mode ) )
                    vfs_dir_get_deep_size( task, full_path, size, &file_stat,
                                                                    FALSE );
                else
                    *size += file_stat.st_size;
            }
            g_free(full_path );
        }
        g_dir_close( dir );
    }
    else if ( top )
        // access error
        *size = 0;
}

/* signal handlers */
void vfs_dir_emit_file_created( VFSDir* dir, const char* file_name, gboolean force )
{
    GList* l;

    // Ignore avoid_changes for creation of files
    //if ( !force && dir->avoid_changes )
    //    return;

    if ( G_UNLIKELY( 0 == strcmp(file_name, dir->path) ) )
    {
        // Special Case: The directory itself was created?
        return;
    }

    dir->created_files = g_slist_append( dir->created_files, g_strdup( file_name ) );
    if ( 0 == change_notify_timeout )
    {
        change_notify_timeout = g_timeout_add_full( G_PRIORITY_LOW,
                                                    200,
                                                    notify_file_change,
                                                    NULL, NULL );
    }
}

void vfs_dir_emit_file_deleted( VFSDir* dir, const char* file_name, VFSFileInfo* file )
{
    GList* l;
    VFSFileInfo* file_found;

    if( G_UNLIKELY( 0 == strcmp(file_name, dir->path) ) )
    {
        /* Special Case: The directory itself was deleted... */
        file = NULL;
        /* clear the whole list */
        g_mutex_lock( dir->mutex );
        g_list_foreach( dir->file_list, (GFunc)vfs_file_info_unref, NULL );
        g_list_free( dir->file_list );
        dir->file_list = NULL;
        g_mutex_unlock( dir->mutex );

        g_signal_emit( dir, signals[ FILE_DELETED_SIGNAL ], 0, file );
        return;
    }

    g_mutex_lock( dir->mutex );
    l = vfs_dir_find_file( dir, file_name, file );
    if ( G_LIKELY( l ) )
    {
        file_found = vfs_file_info_ref( ( VFSFileInfo* ) l->data );
        if( !g_slist_find( dir->changed_files, file_found ) )
        {
            dir->changed_files = g_slist_prepend( dir->changed_files, file_found );
            if ( 0 == change_notify_timeout )
            {
                change_notify_timeout = g_timeout_add_full( G_PRIORITY_LOW,
                                                            200,
                                                            notify_file_change,
                                                            NULL, NULL );
            }
        }
        else
            vfs_file_info_unref( file_found );
    }
    g_mutex_unlock( dir->mutex );
}

void vfs_dir_emit_file_changed( VFSDir* dir, const char* file_name,
                                        VFSFileInfo* file, gboolean force )
{
    GList* l;
//printf("vfs_dir_emit_file_changed dir=%s file_name=%s avoid=%s\n", dir->path, file_name, dir->avoid_changes ? "TRUE" : "FALSE" );

    if ( !force && dir->avoid_changes )
        return;

    if ( G_UNLIKELY( 0 == strcmp(file_name, dir->path) ) )
    {
        // Special Case: The directory itself was changed
        g_signal_emit( dir, signals[ FILE_CHANGED_SIGNAL ], 0, NULL );
        return;
    }

    g_mutex_lock( dir->mutex );

    l = vfs_dir_find_file( dir, file_name, file );
    if ( G_LIKELY( l ) )
    {
        file = vfs_file_info_ref( ( VFSFileInfo* ) l->data );

        if ( app_settings.show_thumbnail && (
#ifdef HAVE_FFMPEG
             vfs_file_info_is_video( file ) ||
#endif
             ( file->size < app_settings.max_thumb_size &&
               vfs_file_info_is_image( file ) ) ) )
        {
            /* image or video file - add it to delayed to prevent thumbnail
             * being reloaded rapidly - caused high cpu and icon flashing,
             * and to ensure data has been written for image files.
             * 
             * This adds the file to changed_files_delayed instead of
             * changed_files.  changed_files_delayed is processed after a
             * several second delay as long as no image/video files change again
             * in that period, otherwise the timer is reset. */
            if ( !g_slist_find( dir->changed_files_delayed, file ) )
                dir->changed_files_delayed = g_slist_prepend(
                                            dir->changed_files_delayed, file );    
            // reset timer
            if ( !dir->delayed_timer )
                dir->delayed_timer = g_timer_new();
            else
                g_timer_reset( dir->delayed_timer );
            if ( 0 == change_notify_timeout )
            {
                change_notify_timeout = g_timeout_add_full( G_PRIORITY_LOW,
                                                            500,
                                                            notify_file_change,
                                                            NULL, NULL );
            }
        }
        else if( !g_slist_find( dir->changed_files, file ) )
        {
            if ( force )
            {
                dir->changed_files = g_slist_prepend( dir->changed_files, file );
                if ( 0 == change_notify_timeout )
                {
                    change_notify_timeout = g_timeout_add_full( G_PRIORITY_LOW,
                                                                100,
                                                                notify_file_change,
                                                                NULL, NULL );
                }
            }
            else if( G_LIKELY( update_file_info( dir, file ) ) ) // update file info the first time
            {
                dir->changed_files = g_slist_prepend( dir->changed_files, file );
                if ( 0 == change_notify_timeout )
                {
                    change_notify_timeout = g_timeout_add_full( G_PRIORITY_LOW,
                                                                500,
                                                                notify_file_change,
                                                                NULL, NULL );
                }
                g_signal_emit( dir, signals[ FILE_CHANGED_SIGNAL ], 0, file );
            }
        }
        else
            // already in changed_files queue
            vfs_file_info_unref( file );
    }

    g_mutex_unlock( dir->mutex );
}

void vfs_dir_emit_thumbnail_loaded( VFSDir* dir, VFSFileInfo* file )
{
    GList* l;
    if ( file )
    {
        g_mutex_lock( dir->mutex );
        l = vfs_dir_find_file( dir, file->name, file );
        if( l )
        {
            g_assert( file == (VFSFileInfo*)l->data );
            file = vfs_file_info_ref( (VFSFileInfo*)l->data );
        }
        else
            file = NULL;
        g_mutex_unlock( dir->mutex );
    }
    else
        g_signal_emit( dir, signals[ THUMBNAIL_LOADED_SIGNAL ], 0, NULL );

    if ( G_LIKELY( file ) )
    {
        g_signal_emit( dir, signals[ THUMBNAIL_LOADED_SIGNAL ], 0, file );
        vfs_file_info_unref( file );
    }
}

/* methods */

VFSDir* vfs_dir_new( const char* path )
{
    VFSDir * dir;
    dir = ( VFSDir* ) g_object_new( VFS_TYPE_DIR, NULL );
    dir->path = g_strdup( path );
    dir->device_info = NULL;
    
#ifdef HAVE_HAL
    dir->avoid_changes = FALSE;
#else
    dir->avoid_changes = vfs_volume_dir_avoid_changes( path, &dir->device_info );
#endif
//printf("vfs_dir_new %s  avoid_changes=%s\n", dir->path, dir->avoid_changes ? "TRUE" : "FALSE" );
    dir->suppress_thumbnail_reload = FALSE;
    return dir;
}

void on_list_task_finished( VFSAsyncTask* task, gboolean is_cancelled, VFSDir* dir )
{
    g_object_unref( dir->task );
    dir->task = NULL;
    dir->load_status = DIR_LOADING_FINISHED;
    GDK_THREADS_ENTER();  // here or vfs-async-task.c:on_idle() ?
    g_signal_emit( dir, signals[FILE_LISTED_SIGNAL], 0, is_cancelled );
    GDK_THREADS_LEAVE();
    dir->file_listed = 1;
}

static gboolean is_dir_trash( const char* path )
{
/* FIXME: Temporarily disable trash support since it's not finished */
#if 0
    /* FIXME: Only support home trash now */
    if( strncmp( path, home_trash_dir, home_trash_dir_len ) == 0 )
    {
        if( strcmp( path + home_trash_dir_len, "/files" ) == 0 )
        {
            return TRUE;
        }
    }
#endif

    return FALSE;
}

static gboolean is_dir_mount_point( const char* path )
{
    return FALSE;   /* FIXME: not implemented */
}

static gboolean is_dir_remote( const char* path )
{
    return FALSE;   /* FIXME: not implemented */
}

static gboolean is_dir_virtual( const char* path )
{
    return FALSE;   /* FIXME: not implemented */
}

char* gethidden( const char* path )  //MOD added
{
    // Read .hidden into string
    char* hidden_path = g_build_filename( path, ".hidden", NULL );     

    // test access first because open() on missing file may cause
    // long delay on nfs
    int acc;
#if defined(HAVE_EUIDACCESS)
    acc = euidaccess( hidden_path, R_OK );
#elif defined(HAVE_EACCESS)
    acc = eaccess( hidden_path, R_OK );
#else
    acc = 0;
#endif
    if ( acc != 0 )
    {       
        g_free( hidden_path );
        return NULL;
    }

    int fd = open( hidden_path, O_RDONLY );
    g_free( hidden_path );
    if ( fd != -1 )
    {
        struct stat s;   // skip stat64
        if ( G_LIKELY( fstat( fd, &s ) != -1 ) )
        {
            char* buf = g_malloc( s.st_size + 1 );
            if( (s.st_size = read( fd, buf, s.st_size )) != -1 )
            {
                buf[ s.st_size ] = 0;
                close( fd );
                return buf;
            }
            else
                g_free( buf );
        }
        close( fd );
    }
    return NULL;
}

gboolean ishidden( const char* hidden, const char* file_name )  //MOD added
{   // assumes hidden,file_name != NULL
    char* str;
    char c;
    str = strstr( hidden, file_name );
    while ( str )
    {
        if ( str == hidden )
        {
            // file_name is at start of buffer
            c = hidden[ strlen( file_name ) ];
            if ( c == '\n' || c == '\0' )
                return TRUE;
        }
        else
        {
            c = str[ strlen( file_name ) ];
            if ( str[-1] == '\n' && ( c == '\n' || c == '\0' ) )
                return TRUE;
        }
        str = strstr( ++str, file_name );
    }
    return FALSE;
}

gboolean vfs_dir_add_hidden( const char* path, const char* file_name )
{
    gboolean ret = TRUE;
    char* hidden = gethidden( path );

    if ( !( hidden && ishidden( hidden, file_name ) ) )
    {
        char* buf = g_strdup_printf( "%s\n", file_name );
        char* file_path = g_build_filename( path, ".hidden", NULL );
        int fd = open( file_path, O_WRONLY | O_CREAT | O_APPEND, 0644 );
        g_free( file_path );
        
        if ( fd != -1 )
        {
            if ( write( fd, buf, strlen( buf ) ) == -1 )
                ret = FALSE;
            close( fd );
        }
        else
            ret = FALSE;
    
        g_free( buf );
    }
    
    if ( hidden )
        g_free( hidden );
    return ret;
}

void vfs_dir_load( VFSDir* dir )
{
    if ( G_LIKELY(dir->path) )
    {
        dir->disp_path = g_filename_display_name( dir->path );
        dir->flags = 0;

        /* FIXME: We should check access here! */

        if( G_UNLIKELY( strcmp( dir->path, vfs_get_desktop_dir() ) == 0 ) )
            dir->is_desktop = TRUE;
        else if( G_UNLIKELY( strcmp( dir->path, g_get_home_dir() ) == 0 ) )
            dir->is_home = TRUE;
        if( G_UNLIKELY(is_dir_trash(dir->path)) )
        {
//            g_free( dir->disp_path );
//            dir->disp_path = g_strdup( _("Trash") );
            dir->is_trash = TRUE;
        }
        if( G_UNLIKELY( is_dir_mount_point(dir->path)) )
            dir->is_mount_point = TRUE;
        if( G_UNLIKELY( is_dir_remote(dir->path)) )
            dir->is_remote = TRUE;
        if( G_UNLIKELY( is_dir_virtual(dir->path)) )
            dir->is_virtual = TRUE;

        dir->task = vfs_async_task_new( (VFSAsyncFunc)vfs_dir_load_thread, dir );
        g_signal_connect( dir->task, "finish", G_CALLBACK(on_list_task_finished), dir );
        vfs_async_task_execute( dir->task );
    }
}

#if 0
gboolean is_dir_desktop( const char* path )
{
    return (desktop_dir && 0 == strcmp(path, desktop_dir));
}
#endif

static gint files_compare( gconstpointer a, gconstpointer b )
{
    VFSFileInfo* file_a = (VFSFileInfo*)a;
    VFSFileInfo* file_b = (VFSFileInfo*)b;
    return strcmp( file_a->collate_icase_key, file_b->collate_icase_key );
}

gpointer vfs_dir_load_thread(  VFSAsyncTask* task, VFSDir* dir )
{
    const gchar * file_name;
    char* full_path;
    GDir* dir_content;
    VFSFileInfo* file;
    struct stat64 file_stat;
    char* hidden = NULL;
    VFSMimeType* mime_type;
    GList* l;
    GList* file_list_copy = NULL;
//printf("vfs_dir_load_thread: %s   [thread %p]\n", dir->path, g_thread_self() );
    dir->file_listed = 0;
    dir->load_status = DIR_LOADING_FILES;
    dir->xhidden_count = 0;
    if ( !dir->path )
        return NULL;
    
    /* Install file alteration monitor */
    dir->monitor = vfs_file_monitor_add_dir( dir->path,
                                         vfs_dir_monitor_callback,
                                         dir );

    // populate file list
    if ( dir_content = g_dir_open( dir->path, 0, NULL ) )
    {
        GKeyFile* kf;

        if( G_UNLIKELY(dir->is_trash) )
            kf = g_key_file_new();

        // MOD  dir contains .hidden file?
        hidden = gethidden( dir->path );

        while ( ! vfs_async_task_is_cancelled( dir->task )
                    && ( file_name = g_dir_read_name( dir_content ) ) )
        {
            full_path = g_build_filename( dir->path, file_name, NULL );
            if ( !full_path )
                continue;

            //MOD ignore if in .hidden
            if ( hidden && ishidden( hidden, file_name ) )
            {
                dir->xhidden_count++;
                continue;
            }
            /* FIXME: Is locking GDK needed here? */
            /* GDK_THREADS_ENTER(); */
            file = vfs_file_info_new();
            if ( G_LIKELY( vfs_file_info_get( file, full_path, file_name,
                                                                FALSE ) ) )
            {
                g_mutex_lock( dir->mutex );
//printf("BG vfs_dir_load_thread: new file: %s\n", full_path );

                /* FIXME: load info, too when new file is added to trash dir */
                if( G_UNLIKELY( dir->is_trash ) ) /* load info of trashed files */
                {
                    gboolean info_loaded;
                    char* info = g_strconcat( home_trash_dir, "/info/", file_name, ".trashinfo", NULL );

                    info_loaded = g_key_file_load_from_file( kf, info, 0, NULL );
                    g_free( info );
                    if( info_loaded )
                    {
                        char* ori_path = g_key_file_get_string( kf, "Trash Info", "Path", NULL );
                        if( ori_path )
                        {
                            /* Thanks to the stupid freedesktop.org spec, the filename is encoded
                             * like a URL, which is insane. This add nothing more than overhead. */
                            char* fake_uri = g_strconcat( "file://", ori_path, NULL );
                            g_free( ori_path );
                            ori_path = g_filename_from_uri( fake_uri, NULL, NULL );
                            /* g_debug( ori_path ); */

                            if( file->disp_name && file->disp_name != file->name )
                                g_free( file->disp_name );
                            file->disp_name = g_filename_display_basename( ori_path );
                            g_free( ori_path );
                        }
                    }
                }

                dir->file_list = g_list_prepend( dir->file_list, file );
                // extra ref for list copy
                vfs_file_info_ref( file );
                file_list_copy = g_list_prepend( file_list_copy, file );
                g_mutex_unlock( dir->mutex );
                ++dir->n_files;
            }
            else
                vfs_file_info_unref( file );
            /* GDK_THREADS_LEAVE(); */
            g_free( full_path );
        }
        g_dir_close( dir_content );
        g_free( hidden );

        if( G_UNLIKELY(dir->is_trash) )
            g_key_file_free( kf );
    }
    else
        return NULL;

    if ( vfs_async_task_is_cancelled( dir->task ) )
    {
        // remove list copy
        g_list_foreach( file_list_copy, (GFunc)vfs_file_info_unref, NULL );
        g_list_free( file_list_copy );
        return NULL;
    }
    
    // signal load status change
    dir->load_status = DIR_LOADING_TYPES;
    GDK_THREADS_ENTER();
    g_signal_emit( dir, signals[FILE_LISTED_SIGNAL], 0,
                                    vfs_async_task_is_cancelled( dir->task ) );
    GDK_THREADS_LEAVE();

    // rough sort list for smooth display
    file_list_copy= g_list_sort( file_list_copy, (GCompareFunc)files_compare );
    
    // get file mime types and load icons
    for ( l = file_list_copy; l; l = l->next )
    {
        file = (VFSFileInfo*)l->data;
        // if n_ref == 1, only we have the reference. That means, nobody
        // is using the file.
        if ( file->n_ref == 1 || file->mime_type )
        {
            if ( !( S_ISDIR( file->mode ) ||
                    ( file->mime_type && S_ISLNK( file->mode ) &&
                      !strcmp( vfs_mime_type_get_type( file->mime_type ),
                                            XDG_MIME_TYPE_DIRECTORY ) ) ) )
            {
                // only leave subdirs or subdir links in the list copy
                vfs_file_info_unref( file );
                l->data = NULL;
            }
            continue;
        }

        if ( full_path = g_build_filename( dir->path, file->name, NULL ) )
        {
            /* convert VFSFileInfo to struct stat
               In current implementation, only st_mode is used in
               mime-type detection, so let's save some CPU cycles
               and don't copy unused fields. 
               see vfs-file-info.c:vfs_file_info_reload_mime_type() */
            file_stat.st_mode = file->mode;
            mime_type = vfs_mime_type_get_from_file( full_path,
                                                           file->disp_name,
                                                           &file_stat );
            if ( file->mime_type )
            {
                // could have been loaded in another thread while we were
                // loading
                if ( mime_type )
                    vfs_mime_type_unref( mime_type );
            }
            else if ( mime_type )
                file->mime_type = mime_type;
            else
                file->mime_type = vfs_mime_type_get_from_type(
                                                    XDG_MIME_TYPE_UNKNOWN );
            // Special processing for desktop folder
            vfs_file_info_load_special_info( file, full_path );
            g_free( full_path );
        }
        else
            file->mime_type = vfs_mime_type_get_from_type(
                                                    XDG_MIME_TYPE_UNKNOWN );
        if ( vfs_async_task_is_cancelled( dir->task ) )
            break;
        if ( file->n_ref > 1 )
        {
            GDK_THREADS_ENTER();
            // use thumbnail-loaded signal since is conditional on fast_update
            if ( !vfs_async_task_is_cancelled( dir->task ) )
                g_signal_emit( dir, signals[ THUMBNAIL_LOADED_SIGNAL ], 0, file );
            // these cause flashing in icon view
            //vfs_dir_emit_file_changed( dir, file->name, file, FALSE );
            //g_signal_emit( dir, signals[FILE_CHANGED_SIGNAL], 0, file );
            GDK_THREADS_LEAVE();
        }
        // only leave subdirs in the list copy
        vfs_file_info_unref( file );
        l->data = NULL;
    }
        
    // signal load status change
    if ( !vfs_async_task_is_cancelled( dir->task ) )
    {
        dir->load_status = DIR_LOADING_SIZES;
        GDK_THREADS_ENTER();
        g_signal_emit( dir, signals[FILE_LISTED_SIGNAL], 0,
                                vfs_async_task_is_cancelled( dir->task ) );
        GDK_THREADS_LEAVE();
    }

    // get deep subdir sizes, or if cancel just unref files
    off64_t size;
    for ( l = file_list_copy; l; l = l->next )
    {
        if ( !l->data )
            continue;
        file = (VFSFileInfo*)l->data;
        
        if ( !vfs_async_task_is_cancelled( dir->task ) &&
                        !dir->avoid_changes && file->n_ref > 1 &&
              ( full_path = g_build_filename( dir->path, file->name, NULL ) ) )
        {
            // see also vfs-thumbnail-loader.c:thumbnail_loader_thread()
            if ( strcmp( full_path, "/mnt" ) &&
                            strcmp( full_path, "/proc" ) &&
                            strcmp( full_path, "/sys" ) &&
                            !vfs_volume_dir_avoid_changes( full_path, NULL ) &&
                            stat64( full_path, &file_stat ) != -1 &&
                            S_ISDIR( file_stat.st_mode ) )
            {
                size = 0;
                vfs_dir_get_deep_size( dir->task, full_path, &size, &file_stat,
                                                                        TRUE );
                if ( !vfs_async_task_is_cancelled( dir->task ) )
                {
                    file->size = size;
                    g_free( file->disp_size );
                    file->disp_size = NULL;  // recalculate
                    if ( file->n_ref > 1 )
                    {
                        GDK_THREADS_ENTER();
                        // use thumbnail-loaded signal since is conditional on
                        // fast_update
                        if ( !vfs_async_task_is_cancelled( dir->task ) )
                            g_signal_emit( dir, signals[ THUMBNAIL_LOADED_SIGNAL ],
                                                                0, file );
                        // these cause flashing in icon view
                        //vfs_dir_emit_file_changed( dir, file->name, file, FALSE );
                        //g_signal_emit( dir, signals[FILE_CHANGED_SIGNAL], 0,
                        //                                                file );
                        GDK_THREADS_LEAVE();
                    }
                }
            }
            g_free( full_path );
        }
        vfs_file_info_unref( file );
        l->data = NULL;
    }
    g_list_free( file_list_copy );
    return NULL;
}

gboolean vfs_dir_is_loading( VFSDir* dir )
{
    return dir->task ? TRUE : FALSE;
}

gboolean vfs_dir_is_file_listed( VFSDir* dir )
{
    return dir->file_listed;
}

#if 0
void vfs_dir_cancel_load( VFSDir* dir )
{
    dir->cancel = TRUE;
    if ( dir->task )
    {
        vfs_async_task_cancel( dir->task );
        /* don't do g_object_unref on task here since this is done in the handler of "finish" signal.
         * FIXME: should probably unref or not set task = NULL, but this code
         * is currently unused */
        dir->task = NULL;
    }
}
#endif

gboolean update_file_info( VFSDir* dir, VFSFileInfo* file )
{
    char* full_path;
    char* file_name;
    gboolean ret = FALSE;
    /* gboolean is_desktop = is_dir_desktop(dir->path); */

    /* FIXME: Dirty hack: steal the string to prevent memory allocation */
    file_name = file->name;
    if( file->name == file->disp_name )
        file->disp_name = NULL;
    file->name = NULL;

    full_path = g_build_filename( dir->path, file_name, NULL );
    if ( G_LIKELY( full_path ) )
    {
        if( G_LIKELY( vfs_file_info_get( file, full_path, file_name, TRUE ) ) )
        {
            ret = TRUE;
            /* if( G_UNLIKELY(is_desktop) ) */
            vfs_file_info_load_special_info( file, full_path );
        }
        else /* The file doesn't exist */
        {
            GList* l;
            l = g_list_find( dir->file_list, file );
            if( G_UNLIKELY(l) )
            {
                dir->file_list = g_list_delete_link( dir->file_list, l );
                --dir->n_files;
                if ( file )
                {
                    g_signal_emit( dir, signals[ FILE_DELETED_SIGNAL ], 0, file );
                    vfs_file_info_unref( file );
                }
            }
            ret = FALSE;
        }
        g_free( full_path );
    }
    g_free( file_name );
    return ret;
}


void update_changed_files( gpointer key, gpointer data, gpointer user_data )
{
    VFSDir* dir = (VFSDir*)data;
    GSList* l;
    VFSFileInfo* file;

    if ( dir->changed_files || dir->changed_files_delayed )
    {
        g_mutex_lock( dir->mutex );
        
        if ( dir->changed_files_delayed && dir->delayed_timer &&
                  g_timer_elapsed( dir->delayed_timer, NULL ) >= 1.0 /*sec*/ )
        {
            // add expired delayed queue onto end of normal queue
            dir->changed_files = g_slist_concat( dir->changed_files,
                                        dir->changed_files_delayed );
            dir->changed_files_delayed = NULL;
            g_timer_destroy( dir->delayed_timer );
            dir->delayed_timer = NULL;
        }
        else if ( dir->changed_files_delayed )
        {
            // delayed queue hasn't expired so just update file info for queued
            for ( l = dir->changed_files_delayed; l; l = l->next )
            {
                file = vfs_file_info_ref( ( VFSFileInfo* ) l->data );
                if ( update_file_info( dir, file ) )
                {
                    // suppress thumbnail reload but emit changed signal
                    dir->suppress_thumbnail_reload = TRUE;
                    g_signal_emit( dir, signals[ FILE_CHANGED_SIGNAL ], 0, file );
                    dir->suppress_thumbnail_reload = FALSE;
                    vfs_file_info_unref( file );
                }
                // else was deleted, signaled, and unrefed in update_file_info
            }
        }
        
        for ( l = dir->changed_files; l; l = l->next )
        {
            file = vfs_file_info_ref( ( VFSFileInfo* ) l->data );
            if ( update_file_info( dir, file ) )
            {
                g_signal_emit( dir, signals[ FILE_CHANGED_SIGNAL ], 0, file );
                vfs_file_info_unref( file );
            }
            // else was deleted, signaled, and unrefed in update_file_info
        }
        g_slist_free( dir->changed_files );
        dir->changed_files = NULL;
        g_mutex_unlock( dir->mutex );
    }
}

void update_created_files( gpointer key, gpointer data, gpointer user_data )
{
    VFSDir* dir = (VFSDir*)data;
    GSList* l;
    char* full_path;
    VFSFileInfo* file;
    GList* ll;

    if ( dir->created_files )
    {
        g_mutex_lock( dir->mutex );
        for ( l = dir->created_files; l; l = l->next )
        {
            if ( !( ll = vfs_dir_find_file( dir, (char*)l->data, NULL ) ) )
            {
                // file is not in dir file_list
                full_path = g_build_filename( dir->path, (char*)l->data, NULL );
                file = vfs_file_info_new();
                if ( vfs_file_info_get( file, full_path, NULL, TRUE ) )
                {
                    // add new file to dir file_list
                    vfs_file_info_load_special_info( file, full_path );
                    dir->file_list = g_list_prepend( dir->file_list,
                                                    vfs_file_info_ref( file ) );
                    ++dir->n_files;
                    g_signal_emit( dir, signals[ FILE_CREATED_SIGNAL ], 0, file );
                }
                // else file doesn't exist in filesystem
                vfs_file_info_unref( file );
                g_free( full_path );
            }
            else
            {
                // file already exists in dir file_list
                file = vfs_file_info_ref( (VFSFileInfo*)ll->data );
                if ( update_file_info( dir, file ) )
                {
                    g_signal_emit( dir, signals[ FILE_CHANGED_SIGNAL ], 0, file );
                    vfs_file_info_unref( file );
                }
                // else was deleted, signaled, and unrefed in update_file_info
            }
            g_free( (char*)l->data );  // free file_name string
        }
        g_slist_free( dir->created_files );
        dir->created_files = NULL;
        g_mutex_unlock( dir->mutex );
    }
}

void has_changed_files_delayed( gpointer key, gpointer data,
                                                    gpointer has_delayed )
{
    if ( !*(gboolean*)has_delayed && ((VFSDir*)data)->changed_files_delayed )
        *(gboolean*)has_delayed = TRUE;
}

gboolean notify_file_change( gpointer user_data )
    
{
    GDK_THREADS_ENTER();
    g_hash_table_foreach( dir_hash, update_changed_files, NULL );
    g_hash_table_foreach( dir_hash, update_created_files, NULL );
    GDK_THREADS_LEAVE();
    /* remove the timeout */
    if ( change_notify_timeout )
        g_source_remove( change_notify_timeout );
    change_notify_timeout = 0;

    // if any dirs still have changed_files_delayed add a change_notify_timeout
    gboolean has_delayed = FALSE;
    g_hash_table_foreach( dir_hash, has_changed_files_delayed, &has_delayed );
    if ( has_delayed )
        change_notify_timeout = g_timeout_add_full( G_PRIORITY_LOW,
                                                    500,
                                                    notify_file_change,
                                                    NULL, NULL );
    return FALSE;
}

void vfs_dir_flush_notify_cache()
{
    if ( change_notify_timeout )
        g_source_remove( change_notify_timeout );
    change_notify_timeout = 0;
    g_hash_table_foreach( dir_hash, update_changed_files, NULL );
    g_hash_table_foreach( dir_hash, update_created_files, NULL );
}

/* Callback function which will be called when monitored events happen */
void vfs_dir_monitor_callback( VFSFileMonitor* fm,
                               VFSFileMonitorEvent event,
                               const char* file_name,
                               gpointer user_data )
{
    VFSDir* dir = ( VFSDir* ) user_data;
    GDK_THREADS_ENTER();

    switch ( event )
    {
    case VFS_FILE_MONITOR_CREATE:
        vfs_dir_emit_file_created( dir, file_name, FALSE );
        break;
    case VFS_FILE_MONITOR_DELETE:
        vfs_dir_emit_file_deleted( dir, file_name, NULL );
        break;
    case VFS_FILE_MONITOR_CHANGE:
        vfs_dir_emit_file_changed( dir, file_name, NULL, FALSE );
        break;
    default:
        g_warning("Error: unrecognized file monitor signal!");
    }
    GDK_THREADS_LEAVE();
}

/* called on every VFSDir when icon theme got changed */
static void reload_icons( const char* path, VFSDir* dir, gpointer user_data )
{
    GList* l;
    for( l = dir->file_list; l; l = l->next )
    {
        VFSFileInfo* fi = (VFSFileInfo*)l->data;
        /* It's a desktop entry file */
        if( fi->flags & VFS_FILE_INFO_DESKTOP_ENTRY )
        {
            char* file_path = g_build_filename( path, fi->name,NULL );
            if( fi->big_thumbnail )
            {
                g_object_unref( fi->big_thumbnail );
                fi->big_thumbnail = NULL;
            }
            if( fi->small_thumbnail )
            {
                g_object_unref( fi->small_thumbnail );
                fi->small_thumbnail = NULL;
            }
            vfs_file_info_load_special_info( fi, file_path );
            g_free( file_path );
        }
    }
}

static void on_theme_changed( GtkIconTheme *icon_theme, gpointer user_data )
{
    g_hash_table_foreach( dir_hash, (GHFunc)reload_icons, NULL );
}

VFSDir* vfs_dir_get_by_path_soft( const char* path )
{
    if ( G_UNLIKELY( !dir_hash || !path ) )
        return NULL;

    VFSDir * dir = g_hash_table_lookup( dir_hash, path );
    if ( dir )
        g_object_ref( dir );
    return dir;
}
    
VFSDir* vfs_dir_get_by_path( const char* path )
{
    VFSDir * dir = NULL;

    g_return_val_if_fail( G_UNLIKELY(path), NULL );

    if ( G_UNLIKELY( ! dir_hash ) )
    {
        dir_hash = g_hash_table_new_full( g_str_hash, g_str_equal, NULL, NULL );
        if( 0 == theme_change_notify )
            theme_change_notify = g_signal_connect( gtk_icon_theme_get_default(), "changed",
                                                                        G_CALLBACK( on_theme_changed ), NULL );
    }
    else
    {
        dir = g_hash_table_lookup( dir_hash, path );
    }

    if( G_UNLIKELY( !mime_cb ) )
        mime_cb = vfs_mime_type_add_reload_cb( on_mime_type_reload, NULL );

//printf("\nvfs_dir_get_by_path: %s   ", path);
    if ( dir )
    {
//printf("  (ref++)\n");
        g_object_ref( dir );
    }
    else
    {
//printf("  (dir_load)\n");
        dir = vfs_dir_new( path );
        vfs_dir_load( dir );  /* asynchronous operation */
        g_hash_table_insert( dir_hash, (gpointer)dir->path, (gpointer)dir );
    }
    return dir;
}

static void reload_mime_type( char* key, VFSDir* dir, gpointer user_data )
{
    GList* l;
    VFSFileInfo* file;
    char* full_path;

    if( G_UNLIKELY( ! dir || ! dir->file_list ) )
        return;
    g_mutex_lock( dir->mutex );
    for( l = dir->file_list; l; l = l->next )
    {
        file = (VFSFileInfo*)l->data;
        full_path = g_build_filename( dir->path,
                                      vfs_file_info_get_name( file ), NULL );
        vfs_file_info_reload_mime_type( file, full_path );
        /* g_debug( "reload %s", full_path ); */
        g_free( full_path );
    }

    for( l = dir->file_list; l; l = l->next )
    {
        file = (VFSFileInfo*)l->data;
        g_signal_emit( dir, signals[FILE_CHANGED_SIGNAL], 0, file );
    }
    g_mutex_unlock( dir->mutex );
}

static void on_mime_type_reload( gpointer user_data )
{
    if( ! dir_hash )
        return;
    /* g_debug( "reload mime-type" ); */
    g_hash_table_foreach( dir_hash, (GHFunc)reload_mime_type, NULL );
}

/* Thanks to the freedesktop.org, things are much more complicated now... */
const char* vfs_get_desktop_dir()
{
    char* def;

    if( G_LIKELY(is_desktop_set) )
        return desktop_dir;

/* glib provides API for this since ver. 2.14, but I think my implementation is better. */
#if GLIB_CHECK_VERSION( 2, 14, 0 ) && 0  /* Delete && 0 to use the one provided by glib */
    desktop_dir = g_get_user_special_dir( G_USER_DIRECTORY_DESKTOP );
#else
    def = g_build_filename( g_get_user_config_dir(), "user-dirs.dirs", NULL );
    if( def )
    {
        int fd = open( def, O_RDONLY );
        g_free( def );
        if( G_LIKELY( fd != -1 ) )
        {
            struct stat s;   // skip stat64
            if( G_LIKELY( fstat( fd, &s ) != -1 ) )
            {
                char* buf = g_malloc( s.st_size + 1 );
                if( (s.st_size = read( fd, buf, s.st_size )) != -1 )
                {
                    char* line;
                    buf[ s.st_size ] = 0;
                    line = strstr( buf, "XDG_DESKTOP_DIR=" );
                    if( G_LIKELY( line ) )
                    {
                        char* eol;
                        line += 16;
                        if( G_LIKELY( ( eol = strchr( line, '\n' ) ) ) )
                            *eol = '\0';
                        line = g_shell_unquote( line, NULL );
                        if( g_str_has_prefix(line, "$HOME") )
                        {
                            desktop_dir = g_build_filename( g_get_home_dir(), line + 5, NULL );
                            g_free( line );
                        }
                        else
                            desktop_dir = line;
                    }
                }
                g_free( buf );
            }
            close( fd );
        }
    }

    if( ! desktop_dir )
        desktop_dir = g_build_filename( g_get_home_dir(), "Desktop", NULL );
#endif

#if 0
    /* FIXME: what should we do if the user has no desktop dir? */
    if( ! g_file_test( desktop_dir, G_FILE_TEST_IS_DIR ) )
    {
        g_free( desktop_dir );
        desktop_dir = NULL;
    }
#endif
    is_desktop_set = TRUE;
    return desktop_dir;
}

const char* vfs_get_trash_dir()
{
    if( G_UNLIKELY( ! home_trash_dir ) )
    {
        home_trash_dir = g_build_filename( g_get_user_data_dir(), "Trash", NULL );
        home_trash_dir_len = strlen( home_trash_dir );
    }
    return home_trash_dir;
}

void vfs_dir_foreach( GHFunc func, gpointer user_data )
{
    if( ! dir_hash )
        return;
    /* g_debug( "reload mime-type" ); */
    g_hash_table_foreach( dir_hash, (GHFunc)func, user_data );
}

void vfs_dir_unload_thumbnails( VFSDir* dir, gboolean is_big )
{
    GList* l;
    VFSFileInfo* file;
    char* file_path;

    g_mutex_lock( dir->mutex );
    if( is_big )
    {
        for( l = dir->file_list; l; l = l->next )
        {
            file = (VFSFileInfo*)l->data;
            if( file->big_thumbnail )
            {
                g_object_unref( file->big_thumbnail );
                file->big_thumbnail = NULL;
            }
            /* This is a desktop entry file, so the icon needs reload
                 FIXME: This is not a good way to do things, but there is no better way now.  */
            if( file->flags & VFS_FILE_INFO_DESKTOP_ENTRY )
            {
                file_path = g_build_filename( dir->path, file->name, NULL );
                vfs_file_info_load_special_info( file, file_path );
                g_free( file_path );
            }
        }
    }
    else
    {
        for( l = dir->file_list; l; l = l->next )
        {
            file = (VFSFileInfo*)l->data;
            if( file->small_thumbnail )
            {
                g_object_unref( file->small_thumbnail );
                file->small_thumbnail = NULL;
            }
            /* This is a desktop entry file, so the icon needs reload
                 FIXME: This is not a good way to do things, but there is no better way now.  */
            if( file->flags & VFS_FILE_INFO_DESKTOP_ENTRY )
            {
                file_path = g_build_filename( dir->path, file->name, NULL );
                vfs_file_info_load_special_info( file, file_path );
                g_free( file_path );
            }
        }
    }
    g_mutex_unlock( dir->mutex );

    /* Ensuring free space at the end of the heap is freed to the OS,
     * mainly to deal with the possibility thousands of large thumbnails
     * have been freed but the memory not actually released by SpaceFM */
#if defined (__GLIBC__)
	malloc_trim(0);
#endif
}

//sfm added mime change timer
guint mime_change_timer = 0;
VFSDir* mime_dir = NULL;

gboolean on_mime_change_timer( gpointer user_data )
{
    //printf("MIME-UPDATE on_timer\n" );
    char* cmd = g_strdup_printf( "update-mime-database %s/mime",
                                                    g_get_user_data_dir() );
    g_spawn_command_line_async( cmd, NULL );
    g_free( cmd );
    cmd = g_strdup_printf( "update-desktop-database %s/applications",
                                                    g_get_user_data_dir() );
    g_spawn_command_line_async( cmd, NULL );
    g_free( cmd );
    g_source_remove( mime_change_timer );
    mime_change_timer = 0;
    return FALSE;
}

void mime_change( gpointer user_data )
{
    if ( mime_change_timer )
    {
        // timer is already running, so ignore request
        //printf("MIME-UPDATE already set\n" );
        return;
    }
    if ( mime_dir )
    {
        // update mime database in 2 seconds 
        mime_change_timer = g_timeout_add_seconds( 2,
                                    ( GSourceFunc ) on_mime_change_timer, NULL );
        //printf("MIME-UPDATE timer started\n" );
    }
}

void vfs_dir_monitor_mime()
{
    // start watching for changes
    if ( mime_dir )
        return;
    char* path = g_build_filename( g_get_user_data_dir(), "mime/packages", NULL );
    if ( g_file_test( path, G_FILE_TEST_IS_DIR ) )
    {
        mime_dir = vfs_dir_get_by_path( path );
        if ( mime_dir )
        {
            g_signal_connect( mime_dir, "file-listed", G_CALLBACK( mime_change ), NULL );
            g_signal_connect( mime_dir, "file-created", G_CALLBACK( mime_change ), NULL );
            g_signal_connect( mime_dir, "file-deleted", G_CALLBACK( mime_change ), NULL );
            g_signal_connect( mime_dir, "file-changed", G_CALLBACK( mime_change ), NULL );
        }
        //printf("MIME-UPDATE watch started\n" );
    }
    g_free( path );
}

