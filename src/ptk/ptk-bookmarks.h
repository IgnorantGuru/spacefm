/*
*  C Interface: ptkbookmarks
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef _PTK_BOOKMARKS_H_
#define _PTK_BOOKMARKS_H_

#include <glib.h>

G_BEGIN_DECLS

/*
  NOTE:
  All paths used in these APIs are all encoded in UTF-8.
  The format of all items in this list is as follows.
  name, '\0', path in utf-8, '\0'
  Two strings are continuously stored in the same buffer
  with a separating NULL character.
*/

typedef struct _PtkBookmarks{
  GList* list; /* Read-only */
  GList* unparsedLines; /* Read-only */
  /* <private> */
  GArray* callbacks;
  gint n_ref;
}PtkBookmarks;

/*
  Get a self-maintained list of bookmarks
  This is read from "~/.gtk-bookmarks".
*/
PtkBookmarks* ptk_bookmarks_get ();

/*
  Replace the content of the bookmarks with new_list.
  PtkBookmarks will then owns new_list, and hence
  new_list shouldn't be freed after this function is called.
*/
void ptk_bookmarks_set ( GList* new_list );

/* Insert an item into bookmarks */
void ptk_bookmarks_insert ( const char* name, const char* path, gint pos );

/* Append an item into bookmarks */
void ptk_bookmarks_append ( const char* name, const char* path );

/* Remove an item from bookmarks */
void ptk_bookmarks_remove ( const char* path );

void ptk_bookmarks_rename ( const char* path, const char* new_name );

void ptk_bookmarks_change( const char* path, const char* new_path );

/* Save the content of the bookmarks to "~/.gtk-bookmarks" */
void ptk_bookmarks_save ();

/* Add a callback which gets called when the content of bookmarks changed */
void ptk_bookmarks_add_callback ( GFunc callback, gpointer user_data );

/* Remove a callback added by ptk_bookmarks_add_callback */
void ptk_bookmarks_remove_callback ( GFunc callback, gpointer user_data );

void ptk_bookmarks_unref ();

/*
* Create a new bookmark item.
* name: displayed name of the bookmark item.
* name_len: length of name;
* upath: dir path of the bookmark item encoded in UTF-8.
* upath_len: length of upath;
* Returned value is a newly allocated string.
*/
gchar* ptk_bookmarks_item_new( const gchar* name, gsize name_len,
                               const gchar* upath, gsize upath_len );

/*
* item: bookmark item
* Returned value: dir path of the bookmark item. (int utf-8)
*/
const gchar* ptk_bookmarks_item_get_path( const gchar* item );

G_END_DECLS

#endif

