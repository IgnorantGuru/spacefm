/*
*  C Interface: vfs-mime_type-type
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING mimele that comes with this distribution
*
*/

#ifndef _VFS_MIME_TYPE_H_
#define _VFS_MIME_TYPE_H_

#include <gdk/gdk.h>
#include "mime-type.h"

G_BEGIN_DECLS

typedef struct _VFSMimeType VFSMimeType;

struct _VFSMimeType
{
    char* type; /* mime_type-type string */
    char* description;  /* description of the mimele type */
    GdkPixbuf* big_icon;
    GdkPixbuf* small_icon;
    /*<private>*/
    int n_ref;
    /*  FIXME: Don't cache mime actions
    char** actions;
    */
};

void vfs_mime_type_init();

void vfs_mime_type_clean();

/* file name used in this API should be encoded in UTF-8 */
VFSMimeType* vfs_mime_type_get_from_file_name( const char* ufile_name );

VFSMimeType* vfs_mime_type_get_from_file( const char* file_path,  /* Should be on-disk encoding */
                                          const char* base_name,  /* Should be in UTF-8 */
                                          struct stat* pstat );   /* Can be NULL */

VFSMimeType* vfs_mime_type_get_from_type( const char* type );

VFSMimeType* vfs_mime_type_new( const char* type_name );
void vfs_mime_type_ref( VFSMimeType* mime_type );
void vfs_mime_type_unref( gpointer mime_type_ );

GdkPixbuf* vfs_mime_type_get_icon( VFSMimeType* mime_type, gboolean big );

void vfs_mime_type_set_icon_size( int big, int small );
void vfs_mime_type_get_icon_size( int* big, int* small );

/* Get mime-type string */
const char* vfs_mime_type_get_type( VFSMimeType* mime_type );

/* Get human-readable description of mime-type */
const char* vfs_mime_type_get_description( VFSMimeType* mime_type );

/*
 * Get available actions (applications) for this mime-type
 * returned vector should be freed with g_strfreev when not needed.
*/
char** vfs_mime_type_get_actions( VFSMimeType* mime_type );

/* returned string should be freed with g_strfreev when not needed. */
char* vfs_mime_type_get_default_action( VFSMimeType* mime_type );

void vfs_mime_type_set_default_action( VFSMimeType* mime_type,
                                       const char* desktop_id );

/* If user-custom desktop file is created, it's returned in custom_desktop. */
void vfs_mime_type_add_action( VFSMimeType* mime_type,
                               const char* desktop_id,
                               char** custom_desktop );

char** vfs_mime_type_get_all_known_apps();

char** vfs_mime_type_join_actions( char** list1, gsize len1,
                                   char** list2, gsize len2 );

GList* vfs_mime_type_add_reload_cb( GFreeFunc cb, gpointer user_data );

void vfs_mime_type_remove_reload_cb( GList* cb );

G_END_DECLS

#endif
