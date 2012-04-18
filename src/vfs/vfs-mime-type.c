/*
*  C Implementation: vfs-mime_type-type
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "vfs-mime-type.h"
#include "mime-action.h"
#include "vfs-file-monitor.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <gtk/gtk.h>

#include "glib-mem.h"

#include "vfs-utils.h"  /* for vfs_load_icon */

static GHashTable *mime_hash = NULL;
GStaticRWLock mime_hash_lock = G_STATIC_RW_LOCK_INIT; /* mutex lock is needed */

static guint reload_callback_id = 0;
static GList* reload_cb = NULL;

static int big_icon_size = 32, small_icon_size = 16;

static VFSFileMonitor** mime_caches_monitor = NULL;

static guint theme_change_notify = 0;

static void on_icon_theme_changed( GtkIconTheme *icon_theme,
                                   gpointer user_data );

typedef struct {
    GFreeFunc cb;
    gpointer user_data;
}VFSMimeReloadCbEnt;

static gboolean vfs_mime_type_reload( gpointer user_data )
{
    GList* l;
    /* FIXME: process mime database reloading properly. */
    /* Remove all items in the hash table */
    GDK_THREADS_ENTER();

    g_static_rw_lock_writer_lock( &mime_hash_lock );
    g_hash_table_foreach_remove ( mime_hash, ( GHRFunc ) gtk_true, NULL );
    g_static_rw_lock_writer_unlock( &mime_hash_lock );

    g_source_remove( reload_callback_id );
    reload_callback_id = 0;

    /* g_debug( "reload mime-types" ); */

    /* call all registered callbacks */
    for( l = reload_cb; l; l = l->next )
    {
        VFSMimeReloadCbEnt* ent = (VFSMimeReloadCbEnt*)l->data;
        ent->cb( ent->user_data );
    }
    GDK_THREADS_LEAVE();
    return FALSE;
}

static void on_mime_cache_changed( VFSFileMonitor* fm,
                                        VFSFileMonitorEvent event,
                                        const char* file_name,
                                        gpointer user_data )
{
    MimeCache* cache = (MimeCache*)user_data;
    switch( event )
    {
    case VFS_FILE_MONITOR_CREATE:
    case VFS_FILE_MONITOR_DELETE:
        /* NOTE: FAM sometimes generate incorrect "delete" notification for non-existent files.
         *  So if the cache is not loaded originally (the cache file is non-existent), we skip it. */
        if( ! cache->buffer )
            return;
    case VFS_FILE_MONITOR_CHANGE:
        mime_cache_reload( cache );
        /* g_debug( "reload cache: %s", file_name ); */
        if( 0 == reload_callback_id )
            reload_callback_id = g_idle_add( vfs_mime_type_reload, NULL );
    }
}

void vfs_mime_type_init()
{
    GtkIconTheme * theme;
    MimeCache** caches;
    int i, n_caches;
    VFSFileMonitor* fm;

    mime_type_init();

    /* install file alteration monitor for mime-cache */
    caches = mime_type_get_caches( &n_caches );
    mime_caches_monitor = g_new0( VFSFileMonitor*, n_caches );
    for( i = 0; i < n_caches; ++i )
    {
        //MOD NOTE1  check to see if path exists - otherwise it later tries to
        //  remove NULL fm with inotify which caused segfault
        if ( g_file_test( caches[i]->file_path, G_FILE_TEST_EXISTS ) )
            fm = vfs_file_monitor_add_file( caches[i]->file_path,
                                                on_mime_cache_changed, caches[i] );
        else
            fm = NULL;
        mime_caches_monitor[i] = fm;
    }
    mime_hash = g_hash_table_new_full( g_str_hash, g_str_equal,
                                       NULL, vfs_mime_type_unref );
    theme = gtk_icon_theme_get_default();
    theme_change_notify = g_signal_connect( theme, "changed",
                                            G_CALLBACK( on_icon_theme_changed ),
                                            NULL );
}

void vfs_mime_type_clean()
{
    GtkIconTheme * theme;
    MimeCache** caches;
    int i, n_caches;

    theme = gtk_icon_theme_get_default();
    g_signal_handler_disconnect( theme, theme_change_notify );

    /* remove file alteration monitor for mime-cache */
    caches = mime_type_get_caches( &n_caches );
    for( i = 0; i < n_caches; ++i )
    {
        if ( mime_caches_monitor[i] )  //MOD added if !NULL - see NOTE1 above
            vfs_file_monitor_remove( mime_caches_monitor[i],
                                        on_mime_cache_changed, caches[i] );
    }
    g_free( mime_caches_monitor );

    mime_type_finalize();

    g_hash_table_destroy( mime_hash );
}

VFSMimeType* vfs_mime_type_get_from_file_name( const char* ufile_name )
{
    const char * type;
    /* type = xdg_mime_get_mime_type_from_file_name( ufile_name ); */
    type = mime_type_get_by_filename( ufile_name, NULL );
    return vfs_mime_type_get_from_type( type );
}

VFSMimeType* vfs_mime_type_get_from_file( const char* file_path,
                                          const char* base_name,
                                          struct stat64* pstat )
{
    const char * type;
    type = mime_type_get_by_file( file_path, pstat, base_name );
    return vfs_mime_type_get_from_type( type );
}

VFSMimeType* vfs_mime_type_get_from_type( const char* type )
{
    VFSMimeType * mime_type;

    g_static_rw_lock_reader_lock( &mime_hash_lock );
    mime_type = g_hash_table_lookup( mime_hash, type );
    g_static_rw_lock_reader_unlock( &mime_hash_lock );

    if ( !mime_type )
    {
        mime_type = vfs_mime_type_new( type );
        g_static_rw_lock_writer_lock( &mime_hash_lock );
        g_hash_table_insert( mime_hash, mime_type->type, mime_type );
        g_static_rw_lock_writer_unlock( &mime_hash_lock );
    }
    vfs_mime_type_ref( mime_type );
    return mime_type;
}

VFSMimeType* vfs_mime_type_new( const char* type_name )
{
    VFSMimeType * mime_type = g_slice_new0( VFSMimeType );
    mime_type->type = g_strdup( type_name );
    mime_type->n_ref = 1;
    return mime_type;
}

void vfs_mime_type_ref( VFSMimeType* mime_type )
{
    g_atomic_int_inc(&mime_type->n_ref);
}

void vfs_mime_type_unref( gpointer mime_type_ )
{
    VFSMimeType* mime_type = (VFSMimeType*)mime_type_;
    if ( g_atomic_int_dec_and_test(&mime_type->n_ref) )
    {
        g_free( mime_type->type );
        if ( mime_type->big_icon )
            gdk_pixbuf_unref( mime_type->big_icon );
        if ( mime_type->small_icon )
            gdk_pixbuf_unref( mime_type->small_icon );
        /* g_strfreev( mime_type->actions ); */

        g_slice_free( VFSMimeType, mime_type );
    }
}

GdkPixbuf* vfs_mime_type_get_icon( VFSMimeType* mime_type, gboolean big )
{
    GdkPixbuf * icon = NULL;
    const char* sep;
    char icon_name[ 100 ];
    GtkIconTheme *icon_theme;
    int size;

    if ( big )
    {
        if ( G_LIKELY( mime_type->big_icon ) )     /* big icon */
            return gdk_pixbuf_ref( mime_type->big_icon );
        size = big_icon_size;
    }
    else    /* small icon */
    {
        if ( G_LIKELY( mime_type->small_icon ) )
            return gdk_pixbuf_ref( mime_type->small_icon );
        size = small_icon_size;
    }

    icon_theme = gtk_icon_theme_get_default ();

    if ( G_UNLIKELY( 0 == strcmp( mime_type->type, XDG_MIME_TYPE_DIRECTORY ) ) )
    {
        icon = vfs_load_icon ( icon_theme, "folder", size );
        if( G_UNLIKELY( !icon) )
            icon = vfs_load_icon ( icon_theme, "gnome-fs-directory", size );
        if( G_UNLIKELY( !icon) )
            icon = vfs_load_icon ( icon_theme, "gtk-directory", size );
        if ( big )
            mime_type->big_icon = icon;
        else
            mime_type->small_icon = icon;
        return icon ? gdk_pixbuf_ref( icon ) : NULL;
    }

    sep = strchr( mime_type->type, '/' );
    if ( sep )
    {
        /* convert mime-type foo/bar to foo-bar */
        strcpy( icon_name, mime_type->type );
        icon_name[ (sep - mime_type->type) ] = '-';
        /* is there an icon named foo-bar? */
        icon = vfs_load_icon ( icon_theme, icon_name, size );
        if ( ! icon )
        {
            /* maybe we can find a legacy icon named gnome-mime-foo-bar */
            strcpy( icon_name, "gnome-mime-" );
            strncat( icon_name, mime_type->type, ( sep - mime_type->type ) );
            strcat( icon_name, "-" );
            strcat( icon_name, sep + 1 );
            icon = vfs_load_icon ( icon_theme, icon_name, size );
        }
        // hack for x-xz-compressed-tar missing icon
        if ( !icon && strstr( mime_type->type, "compressed" ) )
        {
            icon = vfs_load_icon ( icon_theme, "application-x-archive", size );
            if ( !icon )
                icon = vfs_load_icon ( icon_theme, "gnome-mime-application-x-archive",
                                                                            size );
        }
        /* try gnome-mime-foo */
        if ( G_UNLIKELY( ! icon ) )
        {
            icon_name[ 11 ] = '\0'; /* strlen("gnome-mime-") = 11 */
            strncat( icon_name, mime_type->type, ( sep - mime_type->type ) );
            icon = vfs_load_icon ( icon_theme, icon_name, size );
        }
        /* try foo-x-generic */
        if ( G_UNLIKELY( ! icon ) )
        {
            strncpy( icon_name, mime_type->type, ( sep - mime_type->type ) );
            icon_name[ (sep - mime_type->type) ] = '\0';
            strcat( icon_name, "-x-generic" );
            icon = vfs_load_icon ( icon_theme, icon_name, size );
        }
    }

    if( G_UNLIKELY( !icon ) )
    {
        /* prevent endless recursion of XDG_MIME_TYPE_UNKNOWN */
        if( G_LIKELY( strcmp(mime_type->type, XDG_MIME_TYPE_UNKNOWN) ) )
        {
            /* FIXME: fallback to icon of parent mime-type */
            VFSMimeType* unknown;
            unknown = vfs_mime_type_get_from_type( XDG_MIME_TYPE_UNKNOWN );
            icon = vfs_mime_type_get_icon( unknown, big );
            vfs_mime_type_unref( unknown );
        }
        else /* unknown */
        {
            icon = vfs_load_icon ( icon_theme, "unknown", size );
        }
    }

    if ( big )
        mime_type->big_icon = icon;
    else
        mime_type->small_icon = icon;
    return icon ? gdk_pixbuf_ref( icon ) : NULL;
}

static void free_cached_icons ( gpointer key,
                                gpointer value,
                                gpointer user_data )
{
    VFSMimeType * mime_type = ( VFSMimeType* ) value;
    gboolean big = GPOINTER_TO_INT( user_data );
    if ( big )
    {
        if ( mime_type->big_icon )
        {
            gdk_pixbuf_unref( mime_type->big_icon );
            mime_type->big_icon = NULL;
        }
    }
    else
    {
        if ( mime_type->small_icon )
        {
            gdk_pixbuf_unref( mime_type->small_icon );
            mime_type->small_icon = NULL;
        }
    }
}

void vfs_mime_type_set_icon_size( int big, int small )
{
    g_static_rw_lock_writer_lock( &mime_hash_lock );
    if ( big != big_icon_size )
    {
        big_icon_size = big;
        /* Unload old cached icons */
        g_hash_table_foreach( mime_hash,
                              free_cached_icons,
                              GINT_TO_POINTER( 1 ) );
    }
    if ( small != small_icon_size )
    {
        small_icon_size = small;
        /* Unload old cached icons */
        g_hash_table_foreach( mime_hash,
                              free_cached_icons,
                              GINT_TO_POINTER( 0 ) );
    }
    g_static_rw_lock_writer_unlock( &mime_hash_lock );
}

void vfs_mime_type_get_icon_size( int* big, int* small )
{
    if ( big )
        * big = big_icon_size;
    if ( small )
        * small = small_icon_size;
}

const char* vfs_mime_type_get_type( VFSMimeType* mime_type )
{
    return mime_type->type;
}

/* Get human-readable description of mime type */
const char* vfs_mime_type_get_description( VFSMimeType* mime_type )
{
    if ( G_UNLIKELY( ! mime_type->description ) )
    {
        mime_type->description = mime_type_get_desc( mime_type->type, NULL );
        /* FIXME: should handle this better */
        if ( G_UNLIKELY( ! mime_type->description || ! *mime_type->description ) )
        {
            g_warning( "mime-type %s has no desc", mime_type->type );
            mime_type->description = mime_type_get_desc( XDG_MIME_TYPE_UNKNOWN, NULL );
        }
    }
    return mime_type->description;
}

/*
* Join two string vector containing app lists to generate a new one.
* Duplicated app will be removed.
*/
char** vfs_mime_type_join_actions( char** list1, gsize len1,
                                   char** list2, gsize len2 )
{
    gchar **ret = NULL;
    int i, j, k;

    if ( len1 > 0 || len2 > 0 )
        ret = g_new0( char*, len1 + len2 + 1 );
    for ( i = 0; i < len1; ++i )
    {
        ret[ i ] = g_strdup( list1[ i ] );
    }
    for ( j = 0, k = 0; j < len2; ++j )
    {
        for ( i = 0; i < len1; ++i )
        {
            if ( 0 == strcmp( ret[ i ], list2[ j ] ) )
                break;
        }
        if ( i >= len1 )
        {
            ret[ len1 + k ] = g_strdup( list2[ j ] );
            ++k;
        }
    }
    return ret;
}

char** vfs_mime_type_get_actions( VFSMimeType* mime_type )
{
    return (char**)mime_type_get_actions( mime_type->type );
}

char* vfs_mime_type_get_default_action( VFSMimeType* mime_type )
{
    char* def = (char*)mime_type_get_default_action( mime_type->type );
    /* FIXME:
     * If default app is not set, choose one from all availble actions.
     * Is there any better way to do this?
     * Should we put this fallback handling here, or at API of higher level?
     */
    if( ! def )
    {
        char** actions = mime_type_get_actions( mime_type->type );
        if( actions )
        {
            def = g_strdup( actions[0] );
            g_strfreev( actions );
        }
    }
    return def;
}

/*
* Set default app.desktop for specified file.
* app can be the name of the desktop file or a command line.
*/
void vfs_mime_type_set_default_action( VFSMimeType* mime_type,
                                       const char* desktop_id )
{
    char* cust_desktop = NULL;
/*
    if( ! g_str_has_suffix( desktop_id, ".desktop" ) )
        return;
*/
    vfs_mime_type_add_action( mime_type, desktop_id, &cust_desktop );
    if( cust_desktop )
        desktop_id = cust_desktop;
    mime_type_set_default_action( mime_type->type, desktop_id );

    g_free( cust_desktop );
}

/* If user-custom desktop file is created, it's returned in custom_desktop. */
void vfs_mime_type_add_action( VFSMimeType* mime_type,
                               const char* desktop_id,
                               char** custom_desktop )
{
    //MOD  don't create custom desktop file if desktop_id is not a command
    if ( !g_str_has_suffix( desktop_id, ".desktop" ) )
        mime_type_add_action( mime_type->type, desktop_id, custom_desktop );
}

/*
* char** vfs_mime_type_get_all_known_apps():
*
* Get all app.desktop files for all mime types.
* The returned string array contains a list of *.desktop file names,
* and should be freed when no longer needed.
*/

#if 0
static void hash_to_strv ( gpointer key, gpointer value, gpointer user_data )
{
    char***all_apps = ( char*** ) user_data;
    **all_apps = ( char* ) key;
    ++*all_apps;
}
#endif

void on_icon_theme_changed( GtkIconTheme *icon_theme,
                            gpointer user_data )
{
    /* reload_mime_icons */
    g_static_rw_lock_writer_lock( &mime_hash_lock );

    g_hash_table_foreach( mime_hash,
                          free_cached_icons,
                          GINT_TO_POINTER( 1 ) );
    g_hash_table_foreach( mime_hash,
                          free_cached_icons,
                          GINT_TO_POINTER( 0 ) );

    g_static_rw_lock_writer_unlock( &mime_hash_lock );
}

GList* vfs_mime_type_add_reload_cb( GFreeFunc cb, gpointer user_data )
{
    VFSMimeReloadCbEnt* ent = g_slice_new( VFSMimeReloadCbEnt );
    ent->cb = cb;
    ent->user_data = user_data;
    reload_cb = g_list_append( reload_cb, ent );
    return g_list_last( reload_cb );
}

void vfs_mime_type_remove_reload_cb( GList* cb )
{
    g_slice_free( VFSMimeReloadCbEnt, cb->data );
    reload_cb = g_list_delete_link( reload_cb, cb );
}
