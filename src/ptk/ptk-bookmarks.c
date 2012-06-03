/*
*  C Implementation: ptkbookmarks
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "ptk-bookmarks.h"
#include "vfs-file-monitor.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "settings.h"

const char bookmarks_file_name[] = "bookmarks";
static PtkBookmarks bookmarks = {0};
static VFSFileMonitor* monitor = NULL;

typedef struct
{
    GFunc callback;
    gpointer user_data;
}
BookmarksCallback;

typedef struct unparsedLineO
{
    int lineNum;
    char *line;
} unparsedLine;

/**
 * malloc a unparsedLine structure
 *
 * @ret return the pointer of newly allocate structure
 */
static unparsedLine* unparsedLine_new() {
  unparsedLine* ret;
  ret = (unparsedLine*) malloc(sizeof(unparsedLine));
  if (ret!=NULL) {
    memset(ret,0,sizeof(unparsedLine));
  }
  return ret;
}

/**
 * free a unparsedLine structure
 * 
 * @param data the pointer of the structure to be free
 * @param user_data the pointer of a boolean value. If it's true, this
 *                  function will also free the data->line. If it's NULL or
 *                  false this function will only free the strcture.
 */
static void unparsedLine_delete( gpointer *data, gpointer *user_data) {
  unparsedLine *d;
  int *flag;
  d = (unparsedLine*)(data);
  flag = (int*)(user_data);
  if (d!=NULL) {
    if (flag && (*flag) && (d->line)) {
      free(d->line);
      d->line=NULL;
    }
    free(d);
  }
}

/* Notify all callbacks that the bookmarks are changed. */
static void ptk_bookmarks_notify()
{
    BookmarksCallback* cb;
    int i;
    /* Call the callback functions set by the user */
    if( bookmarks.callbacks && bookmarks.callbacks->len )
    {
        cb = (BookmarksCallback*)bookmarks.callbacks->data;
        for( i = 0; i < bookmarks.callbacks->len; ++i )
        {
            cb[i].callback( &bookmarks, cb[i].user_data );
        }
    }
}

static void load( const char* path )
{
    FILE* file;
    gchar* upath;
    gchar* uri;
    gchar* item;
    gchar* name;
    gchar* basename;
    char line_buf[1024];
    char *line=NULL;
    int lineNum=0;
    unparsedLine *unusedLine=NULL;
    gsize name_len, upath_len;

    file = fopen( path, "r" );
    if( file )
    {
        for(lineNum=0; fgets( line_buf, sizeof(line_buf), file ); lineNum++)
        {
            /* Every line is an URI containing no space charactetrs
               with its name appended (optional) */
            line = strdup(line_buf);
            uri = strtok( line, " \r\n" );
            if( ! uri || !*uri )
            {
                unusedLine = unparsedLine_new();
                unusedLine->lineNum = lineNum;
                unusedLine->line = strdup(line_buf);
                bookmarks.unparsedLines = g_list_append( 
                                          bookmarks.unparsedLines, unusedLine);
                free(line);
                line=NULL;
                continue;
            }
            path = g_filename_from_uri(uri, NULL, NULL);
            if( path )
            {
                upath = g_filename_to_utf8(path, -1, NULL, &upath_len, NULL);
                g_free( (gpointer) path );
                if( upath )
                {
                    name = strtok( NULL, "\r\n" );
                    if( name )
                    {
                        name_len = strlen( name );
                        basename = NULL;
                    }
                    else
                    {
                        name = basename = g_path_get_basename( upath );
                        name_len = strlen( basename );
                    }
                    item = ptk_bookmarks_item_new( name, name_len,
                                                   upath, upath_len );
                    bookmarks.list = g_list_append( bookmarks.list,
                                                    item );
                    g_free(upath);
                    g_free( basename );
                }
            }
            else if ( g_str_has_prefix( uri, "//" ) || strstr( uri, ":/" ) )
            {
                name = strtok( NULL, "\r\n" );
                if( name )
                {
                    name_len = strlen( name );
                    basename = NULL;
                }
                else
                {
                    name = basename = g_strdup( uri );
                    name_len = strlen( basename );
                }
                item = ptk_bookmarks_item_new( name, name_len,
                                               uri, strlen( uri ) );
                bookmarks.list = g_list_append( bookmarks.list,
                                                item );
                g_free( basename );
            }
            else
            {
                unusedLine = unparsedLine_new();
                unusedLine->lineNum = lineNum;
                unusedLine->line = strdup(line_buf);
                bookmarks.unparsedLines = g_list_append( 
                                          bookmarks.unparsedLines, unusedLine);
            }
            free(line);
            line=NULL;
        }
        fclose( file );
    }
}

static void on_bookmark_file_changed( VFSFileMonitor* fm,
                                        VFSFileMonitorEvent event,
                                        const char* file_name,
                                        gpointer user_data )
{
    /* This callback is called from IO channel handler insode VFSFileMonotor. */
    GDK_THREADS_ENTER();

    g_list_foreach( bookmarks.list, (GFunc)g_free, NULL );
    g_list_free( bookmarks.list );
    bookmarks.list = 0;
    if (bookmarks.unparsedLines != NULL) {
      int flag=1;
      g_list_foreach( bookmarks.unparsedLines, (GFunc)unparsedLine_delete, &flag );
      g_list_free(bookmarks.unparsedLines);
      bookmarks.unparsedLines=NULL;
    }

    load( file_name );

    ptk_bookmarks_notify();
    GDK_THREADS_LEAVE();
}

/*
  Get a self-maintained list of bookmarks
  This is read from "~/.gtk-bookmarks".
*/
PtkBookmarks* ptk_bookmarks_get ()
{
    gchar* path;
    if( 0 == bookmarks.n_ref )
    {
        path = g_build_filename( xset_get_config_dir(), bookmarks_file_name, NULL );
        //path = g_build_filename( g_get_home_dir(), bookmarks_file_name, NULL );
        //monitor = vfs_file_monitor_add_file( path, on_bookmark_file_changed, NULL );
        load( path );
        g_free( path );
    }
    g_atomic_int_inc( &bookmarks.n_ref );
    return &bookmarks;
}

/*
  Replace the content of the bookmarks with new_list.
  PtkBookmarks will then owns new_list, and hence
  new_list shouldn't be freed after this function is called.
*/
void ptk_bookmarks_set ( GList* new_list )
{
    g_list_foreach( bookmarks.list, (GFunc)g_free, NULL );
    g_list_free( bookmarks.list );
    bookmarks.list = new_list;

    ptk_bookmarks_notify();
    ptk_bookmarks_save();
}

/* Insert an item into bookmarks */
void ptk_bookmarks_insert ( const char* name, const char* path, gint pos )
{
    char* item;
    item = ptk_bookmarks_item_new(name, strlen(name), path, strlen(path));
    bookmarks.list = g_list_insert( bookmarks.list,
                                    item, pos );
    ptk_bookmarks_notify();
    ptk_bookmarks_save();
}

/* Append an item into bookmarks */
void ptk_bookmarks_append ( const char* name, const char* path )
{
    char* item;
    item = ptk_bookmarks_item_new(name, strlen(name), path, strlen(path));
    bookmarks.list = g_list_append( bookmarks.list,
                                    item );
    ptk_bookmarks_notify();
    ptk_bookmarks_save();
}

/* find an item from bookmarks */
static GList* find_item( const char* path )
{
    GList* l;
    char* item;
    char* item_path;
    int len;

    for( l = bookmarks.list; l; l = l->next )
    {
        item = (char*)l->data;
        len = strlen( item );
        item_path = item + len + 1;
        if( 0 == strcmp( path, item_path ) )
            break;
    }
    return l;
}

void ptk_bookmarks_change( const char* path, const char* new_path )
{
    GList* l;
    char* item;
    char* item_path;
    int len;
    int pos = 0;
    
    for( l = bookmarks.list; l; l = l->next )
    {
        item = (char*)l->data;
        len = strlen( item );
        item_path = item + len + 1;
        if( 0 == strcmp( path, item_path ) )
            break;
        pos++;
    }
    if ( l )
    {
        char* name = g_strdup( item );
        ptk_bookmarks_remove( path );
        ptk_bookmarks_insert( name, new_path, pos );
        ptk_bookmarks_notify();
        ptk_bookmarks_save();
        g_free( name );
    }
}

void ptk_bookmarks_remove ( const char* path )
{
    GList* l;

    if( (l = find_item( path )) )
    {
        g_free( l->data );
        bookmarks.list = g_list_delete_link( bookmarks.list, l );

        ptk_bookmarks_notify();
        ptk_bookmarks_save();
    }
}

void ptk_bookmarks_rename ( const char* path, const char* new_name )
{
    GList* l;
    char* item;

    if( path && new_name && (l = find_item( path )) )
    {
        item = ptk_bookmarks_item_new(new_name, strlen(new_name),
                                      path, strlen(path));
        g_free( l->data );
        l->data = item;

        ptk_bookmarks_notify();
        ptk_bookmarks_save();
    }
}

static void ptk_bookmarks_save_item( GList* l, FILE* file )
{
    gchar* item;
    const gchar* upath;
    char* uri;
    char* path;

    item = (char*)l->data;
    upath = ptk_bookmarks_item_get_path( item );
    path = g_filename_from_utf8( upath, -1, NULL, NULL, NULL );

    if( path )
    {
        uri = g_filename_to_uri( path, NULL, NULL );
        if( uri )
        {
            fprintf( file, "%s %s\n", uri, item );
            g_free( uri );
        }
        else if ( g_str_has_prefix( path, "//" ) || strstr( path, ":/" ) )
            fprintf( file, "%s %s\n", path, item );
        g_free( path );
    }
}

void ptk_bookmarks_save ()
{
    FILE* file;
    gchar* path;
    GList* l;
    GList* ul;
    
    int lineNum=0;

    path = g_build_filename( xset_get_config_dir(), bookmarks_file_name, NULL );
    //path = g_build_filename( g_get_home_dir(), bookmarks_file_name, NULL );
    file = fopen( path, "w" );
    g_free( path );

    if( file )
    {
        lineNum=0;
        ul = bookmarks.unparsedLines;
        for( l = bookmarks.list; l; l = l->next )
        {
            while (ul != NULL && ul->data != NULL 
                && ((unparsedLine*)ul->data)->lineNum==lineNum) {
              fputs(((unparsedLine*)ul->data)->line,file);
              lineNum++;
              ul = g_list_next(ul);
            }
            ptk_bookmarks_save_item( l, file );
            lineNum++;
        }
        while (ul != NULL && ul->data != NULL) {
            fputs(((unparsedLine*)ul->data)->line,file);
            lineNum++;
            ul = g_list_next(ul);
        }
        fclose( file );
    }
}

/* Add a callback which gets called when the content of bookmarks changed */
void ptk_bookmarks_add_callback ( GFunc callback, gpointer user_data )
{
    BookmarksCallback cb;
    cb.callback = callback;
    cb.user_data = user_data;
    if( !bookmarks.callbacks )
    {
        bookmarks.callbacks = g_array_new (FALSE, FALSE, sizeof(BookmarksCallback));
    }
    bookmarks.callbacks = g_array_append_val( bookmarks.callbacks, cb );
}

/* Remove a callback added by ptk_bookmarks_add_callback */
void ptk_bookmarks_remove_callback ( GFunc callback, gpointer user_data )
{
    BookmarksCallback* cb = (BookmarksCallback*)bookmarks.callbacks->data;
    int i;
    if( bookmarks.callbacks )
    {
        for(i = 0; i < bookmarks.callbacks->len; ++i )
        {
            if( cb[i].callback == callback && cb[i].user_data == user_data )
            {
                bookmarks.callbacks = g_array_remove_index_fast ( bookmarks.callbacks, i );
                break;
            }
        }
    }
}

void ptk_bookmarks_unref ()
{
    if( g_atomic_int_dec_and_test(&bookmarks.n_ref) )
    {
        //vfs_file_monitor_remove( monitor, on_bookmark_file_changed, NULL );
        monitor = NULL;

        bookmarks.n_ref = 0;
        if( bookmarks.list )
        {
            g_list_foreach( bookmarks.list, (GFunc)g_free, NULL );
            g_list_free( bookmarks.list );
            bookmarks.list = NULL;
        }

        if( bookmarks.callbacks )
        {
            g_array_free(bookmarks.callbacks, TRUE);
            bookmarks.callbacks = NULL;
        }
    }
}

/*
* Create a new bookmark item.
* name: displayed name of the bookmark item.
* name_len: length of name;
* upath: dir path of the bookmark item encoded in UTF-8.
* upath_len: length of upath;
* Returned value is a newly allocated string.
*/
gchar* ptk_bookmarks_item_new( const gchar* name, gsize name_len,
                               const gchar* upath, gsize upath_len )
{
    char* buf;
    ++name_len; /* include terminating null */
    ++upath_len; /* include terminating null */
    buf = g_new( char, name_len + upath_len );
    memcpy( buf, name, name_len );
    memcpy( buf + name_len, upath, upath_len );
    return buf;
}

/*
* item: bookmark item
* Returned value: dir path of the bookmark item. (int utf-8)
*/
const gchar* ptk_bookmarks_item_get_path( const gchar* item )
{
    int name_len = strlen(item);
    return (item + name_len + 1);
}

