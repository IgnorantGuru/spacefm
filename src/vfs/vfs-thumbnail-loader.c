/*
 *      vfs-thumbnail-loader.c
 *
 *      Copyright 2008 PCMan <pcman.tw@gmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
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

#include "vfs-thumbnail-loader.h"
#include "glib-mem.h" /* for g_slice API */
#include "glib-utils.h" /* for g_mkdir_with_parents() */
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#if GLIB_CHECK_VERSION(2, 16, 0)
    #include "md5.h"    /* for thumbnails */
#endif

struct _VFSThumbnailLoader
{
    VFSDir* dir;
    GQueue* queue;
    VFSAsyncTask* task;
    guint idle_handler;
    GQueue* update_queue;
};

enum
{
    LOAD_BIG_THUMBNAIL,
    LOAD_SMALL_THUMBNAIL,
    N_LOAD_TYPES
};

typedef struct _ThumbnailRequest
{
    int n_requests[ N_LOAD_TYPES ];
    VFSFileInfo* file;
}
ThumbnailRequest;

static gpointer thumbnail_loader_thread( VFSAsyncTask* task, VFSThumbnailLoader* loader );
/* static void on_load_finish( VFSAsyncTask* task, gboolean is_cancelled, VFSThumbnailLoader* loader ); */
static void thumbnail_request_free( ThumbnailRequest* req );
static gboolean on_thumbnail_idle( VFSThumbnailLoader* loader );


VFSThumbnailLoader* vfs_thumbnail_loader_new( VFSDir* dir )
{
    VFSThumbnailLoader* loader = g_slice_new0( VFSThumbnailLoader );
    loader->dir = g_object_ref( dir );
    loader->queue = g_queue_new();
    loader->update_queue = g_queue_new();
    loader->task = vfs_async_task_new( (VFSAsyncFunc)thumbnail_loader_thread, loader );
    /* g_signal_connect( loader->task, "finish", G_CALLBACK(on_load_finish), loader ); */
    return loader;
}

void vfs_thumbnail_loader_free( VFSThumbnailLoader* loader )
{
    if( loader->idle_handler )
        g_source_remove( loader->idle_handler );

    /* g_signal_handlers_disconnect_by_func( loader->task, on_load_finish, loader ); */
    /* stop the running thread, if any. */
    vfs_async_task_cancel( loader->task );

    if( loader->idle_handler )
        g_source_remove( loader->idle_handler );

    g_object_unref( loader->task );

    if( loader->queue )
    {
        g_queue_foreach( loader->queue, (GFunc) thumbnail_request_free, NULL );
        g_queue_free( loader->queue );
    }
    if( loader->update_queue )
    {
        g_queue_foreach( loader->update_queue, (GFunc) vfs_file_info_unref, NULL );
        g_queue_free( loader->update_queue );
    }
    /* g_debug( "FREE THUMBNAIL LOADER" ); */

    /* prevent recursive unref called from vfs_dir_finalize */
    loader->dir->thumbnail_loader = NULL;
    g_object_unref( loader->dir );
}

#if 0  /* This is not used in the program. For debug only */
void on_load_finish( VFSAsyncTask* task, gboolean is_cancelled, VFSThumbnailLoader* loader )
{
    g_debug( "TASK FINISHED" );
}
#endif

void thumbnail_request_free( ThumbnailRequest* req )
{
    vfs_file_info_unref( req->file );
    g_slice_free( ThumbnailRequest, req );
    /* g_debug( "FREE REQUEST!" ); */
}

gboolean on_thumbnail_idle( VFSThumbnailLoader* loader )
{
    VFSFileInfo* file;
    /* g_debug( "ENTER ON_THUMBNAIL_IDLE" ); */
    vfs_async_task_lock( loader->task );

    while( ( file = (VFSFileInfo*)g_queue_pop_head(loader->update_queue) )  )
    {
        GDK_THREADS_ENTER();
        vfs_dir_emit_thumbnail_loaded( loader->dir, file );
        vfs_file_info_unref( file );
        GDK_THREADS_LEAVE();
    }

    loader->idle_handler = 0;

    vfs_async_task_unlock( loader->task );

    if( vfs_async_task_is_finished( loader->task ) )
    {
        /* g_debug( "FREE LOADER IN IDLE HANDLER" ); */
        loader->dir->thumbnail_loader = NULL;
        vfs_thumbnail_loader_free(loader);
    }
    /* g_debug( "LEAVE ON_THUMBNAIL_IDLE" ); */

    return FALSE;
}

gpointer thumbnail_loader_thread( VFSAsyncTask* task, VFSThumbnailLoader* loader )
{
    ThumbnailRequest* req;
    int i;
    gboolean load_big, need_update;

    while( G_LIKELY( ! vfs_async_task_is_cancelled(task) ))
    {
        vfs_async_task_lock( task );
        req = (ThumbnailRequest*)g_queue_pop_head( loader->queue );
        vfs_async_task_unlock( task );
        if( G_UNLIKELY( ! req ) )
            break;
        /* g_debug("pop: %s", req->file->name); */

        /* Only we have the reference. That means, no body is using the file */
        if( req->file->n_ref == 1 )
        {
            thumbnail_request_free( req );
            continue;
        }

        need_update = FALSE;
        for ( i = 0; i < 2; ++i )
        {
            if ( 0 == req->n_requests[ i ] )
                continue;

            load_big = ( i == LOAD_BIG_THUMBNAIL );
            if ( ! vfs_file_info_is_thumbnail_loaded( req->file, load_big ) )
            {
                char* full_path;
                full_path = g_build_filename( loader->dir->path,
                                              vfs_file_info_get_name( req->file ),
                                              NULL );
                vfs_file_info_load_thumbnail( req->file, full_path, load_big );
                g_free( full_path );
                /*  Slow donwn for debugging.
                g_debug( "DELAY!!" );
                g_usleep(G_USEC_PER_SEC/2);
                */

                /* g_debug( "thumbnail loaded: %s", req->file ); */
            }
            need_update = TRUE;
        }

        if( ! vfs_async_task_is_cancelled(task) && need_update )
        {
            vfs_async_task_lock( task );
            g_queue_push_tail( loader->update_queue, vfs_file_info_ref(req->file) );
            if( 0 == loader->idle_handler)
                loader->idle_handler = g_idle_add_full( G_PRIORITY_LOW, (GSourceFunc) on_thumbnail_idle, loader, NULL );
            vfs_async_task_unlock( task );
        }
        /* g_debug( "NEED_UPDATE: %d", need_update ); */
        thumbnail_request_free( req );
    }

    if( vfs_async_task_is_cancelled(task) )
    {
        /* g_debug( "THREAD CANCELLED!!!" ); */
        vfs_async_task_lock( task );
        if( loader->idle_handler)
        {
            g_source_remove( loader->idle_handler );
            loader->idle_handler = 0;
        }
        vfs_async_task_unlock( task );
    }
    else
    {
        if( 0 == loader->idle_handler)
        {
            /* g_debug( "ADD IDLE HANDLER BEFORE THREAD ENDING" ); */
            loader->idle_handler = g_idle_add_full( G_PRIORITY_LOW, (GSourceFunc) on_thumbnail_idle, loader, NULL );
        }
    }
    /* g_debug("THREAD ENDED!");  */
    return NULL;
}

void vfs_thumbnail_loader_request( VFSDir* dir, VFSFileInfo* file, gboolean is_big )
{
    VFSThumbnailLoader* loader;
    ThumbnailRequest* req;
    gboolean new_task = FALSE;
    GList* l;

    /* g_debug( "request thumbnail: %s, is_big: %d", file->name, is_big ); */
    if( G_UNLIKELY( ! dir->thumbnail_loader ) )
    {
        dir->thumbnail_loader = vfs_thumbnail_loader_new( dir );
        new_task = TRUE;
    }

    loader = dir->thumbnail_loader;

    if( G_UNLIKELY( ! loader->task ) )
    {
        loader->task = vfs_async_task_new( (VFSAsyncFunc)thumbnail_loader_thread, loader );
        new_task = TRUE;
    }
    vfs_async_task_lock( loader->task );

    /* Check if the request is already scheduled */
    for( l = loader->queue->head; l; l = l->next )
    {
        req = (ThumbnailRequest*)l->data;
        /* If file with the same name is already in our queue */
        if( req->file == file || 0 == strcmp( req->file->name, file->name ) )
            break;
    }
    if( l )
    {
        req = (ThumbnailRequest*)l->data;
    }
    else
    {
        req = g_slice_new0( ThumbnailRequest );
        req->file = vfs_file_info_ref(file);
        g_queue_push_tail( dir->thumbnail_loader->queue, req );
    }

    ++req->n_requests[ is_big ? LOAD_BIG_THUMBNAIL : LOAD_SMALL_THUMBNAIL ];

    vfs_async_task_unlock( loader->task );

    if( new_task )
        vfs_async_task_execute( loader->task );
}

void vfs_thumbnail_loader_cancel_all_requests( VFSDir* dir, gboolean is_big )
{
    GList* l;
    VFSThumbnailLoader* loader;
    ThumbnailRequest* req;

    if( G_UNLIKELY( (loader=dir->thumbnail_loader) ) )
    {
        vfs_async_task_lock( loader->task );
        /* g_debug( "TRY TO CANCEL REQUESTS!!" ); */
        for( l = loader->queue->head; l;  )
        {
            req = (ThumbnailRequest*)l->data;
            --req->n_requests[ is_big ? LOAD_BIG_THUMBNAIL : LOAD_SMALL_THUMBNAIL ];

            if( req->n_requests[0]  <= 0 && req->n_requests[1] <= 0 )   /* nobody needs this */
            {
                GList* next = l->next;
                g_queue_delete_link( loader->queue, l );
                l = next;
            }
            else
                l = l->next;
        }

        if( g_queue_get_length( loader->queue ) == 0 )
        {
            /* g_debug( "FREE LOADER IN vfs_thumbnail_loader_cancel_all_requests!" ); */
            vfs_async_task_unlock( loader->task );
            loader->dir->thumbnail_loader = NULL;
            vfs_thumbnail_loader_free( loader );
            return;
        }
        vfs_async_task_unlock( loader->task );
    }
}

static GdkPixbuf* _vfs_thumbnail_load( const char* file_path, const char* uri,
                                                                          int size, time_t mtime )
{
#if GLIB_CHECK_VERSION(2, 16, 0)
    GChecksum *cs;
#else
    md5_state_t md5_state;
    md5_byte_t md5[ 16 ];
#endif
    char file_name[ 40 ];
    char* thumbnail_file;
    char mtime_str[ 32 ];
    const char* thumb_mtime;
    int i, w, h;
    struct stat statbuf;
    GdkPixbuf* thumbnail, *result = NULL;

    if ( !gdk_pixbuf_get_file_info( file_path, &w, &h ) )
        return NULL;   /* image format cannot be recognized */

    /* If the image itself is very small, we should load it directly */
    if ( w <= 128 && h <= 128 )
    {
        if( w <= size && h <= size )
            return gdk_pixbuf_new_from_file( file_path, NULL );
        return gdk_pixbuf_new_from_file_at_size( file_path, size, size, NULL );
    }

#if GLIB_CHECK_VERSION(2, 16, 0)
    cs = g_checksum_new(G_CHECKSUM_MD5);
    g_checksum_update(cs, uri, strlen(uri));
    memcpy( file_name, g_checksum_get_string(cs), 32 );
    g_checksum_free(cs);
#else
    md5_init( &md5_state );
    md5_append( &md5_state, ( md5_byte_t * ) uri, strlen( uri ) );
    md5_finish( &md5_state, md5 );

    for ( i = 0; i < 16; ++i )
        sprintf( ( file_name + i * 2 ), "%02x", md5[ i ] );
#endif
    strcpy( ( file_name + 32 ), ".png" );

    thumbnail_file = g_build_filename( g_get_home_dir(),
                                       ".thumbnails/normal",
                                       file_name, NULL );

    if( G_UNLIKELY( 0 == mtime ) )
    {
        if( stat( file_path, &statbuf ) != -1 )
            mtime = statbuf.st_mtime;
    }

    /* load existing thumbnail */
    thumbnail = gdk_pixbuf_new_from_file( thumbnail_file, NULL );
    if ( !thumbnail ||
            !( thumb_mtime = gdk_pixbuf_get_option( thumbnail, "tEXt::Thumb::MTime" ) ) ||
            atol( thumb_mtime ) != mtime )
    {
        if( thumbnail )
            g_object_unref( thumbnail );
        /* create new thumbnail */
        thumbnail = gdk_pixbuf_new_from_file_at_size( file_path, 128, 128, NULL );
        if ( thumbnail )
        {
            thumbnail = gdk_pixbuf_apply_embedded_orientation( thumbnail );
            sprintf( mtime_str, "%lu", mtime );
            gdk_pixbuf_save( thumbnail, thumbnail_file, "png", NULL,
                             "tEXt::Thumb::URI", uri, "tEXt::Thumb::MTime", mtime_str, NULL );
            chmod( thumbnail_file, 0600 );  /* only the owner can read it. */
        }
    }

    if ( thumbnail )
    {
        w = gdk_pixbuf_get_width( thumbnail );
        h = gdk_pixbuf_get_height( thumbnail );

        if ( w > h )
        {
            h = h * size / w;
            w = size;
        }
        else if ( h > w )
        {
            w = w * size / h;
            h = size;
        }
        else
        {
            w = h = size;
        }
        if ( w > 0 && h > 0 )
            result = gdk_pixbuf_scale_simple(
                         thumbnail,
                         w, h, GDK_INTERP_BILINEAR );
        g_object_unref( thumbnail );
    }

    g_free( thumbnail_file );
    return result;
}

GdkPixbuf* vfs_thumbnail_load_for_uri(  const char* uri, int size, time_t mtime )
{
    GdkPixbuf* ret;
    char* file = g_filename_from_uri( uri, NULL, NULL );
    ret = _vfs_thumbnail_load( file, uri, size, mtime );
    g_free( file );
    return ret;
}

GdkPixbuf* vfs_thumbnail_load_for_file( const char* file, int size, time_t mtime )
{
    GdkPixbuf* ret;
    char* uri = g_filename_to_uri( file, NULL, NULL );
    ret = _vfs_thumbnail_load( file, uri, size, mtime );
    g_free( uri );
    return ret;
}

/* Ensure the thumbnail dirs exist and have proper file permission. */
void vfs_thumbnail_init()
{
    char* dir;
    dir = g_build_filename( g_get_home_dir(), ".thumbnails/normal", NULL );

    if( G_LIKELY( g_file_test( dir, G_FILE_TEST_IS_DIR ) ) )
        chmod( dir, 0700 );
    else
        g_mkdir_with_parents( dir, 0700 );

    g_free( dir );
}
