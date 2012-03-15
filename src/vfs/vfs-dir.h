/*
*  C Interface: vfs-dir
*
* Description: Object used to present a directory
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef _VFS_DIR_H_
#define _VFS_DIR_H_

#include <glib.h>
#include <glib-object.h>

#include "vfs-file-monitor.h"
#include "vfs-file-info.h"
#include "vfs-async-task.h"

G_BEGIN_DECLS

#define VFS_TYPE_DIR             (vfs_dir_get_type())
#define VFS_DIR(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),  VFS_TYPE_DIR, VFSDir))
#define VFS_DIR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass),  VFS_TYPE_DIR, VFSDirClass))
#define VFS_IS_DIR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VFS_TYPE_DIR))
#define VFS_IS_DIR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass),  VFS_TYPE_DIR))
#define VFS_DIR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj),  VFS_TYPE_DIR, VFSDirClass))

typedef struct _VFSDir VFSDir;
typedef struct _VFSDirClass VFSDirClass;

struct _VFSDir
{
    GObject parent;

    char* path;
    char* disp_path;
    GList* file_list;
    int n_files;

    union {
        int flags;
        struct {
            gboolean is_home : 1;
            gboolean is_desktop : 1;
            gboolean is_trash : 1;
            gboolean is_mount_point : 1;
            gboolean is_remote : 1;
            gboolean is_virtual : 1;
        };
    };

    /*<private>*/
    VFSFileMonitor* monitor;
    GMutex* mutex;  /* Used to guard file_list */
    VFSAsyncTask* task;
    gboolean file_listed : 1;
    gboolean load_complete : 1;
    gboolean cancel: 1;
    gboolean show_hidden : 1;

    struct _VFSThumbnailLoader* thumbnail_loader;

    GSList* changed_files;
    GSList* created_files;  //MOD
    glong xhidden_count;  //MOD
};

struct _VFSDirClass
{
    GObjectClass parent;
    /* Default signal handlers */
    void ( *file_created ) ( VFSDir* dir, VFSFileInfo* file );
    void ( *file_deleted ) ( VFSDir* dir, VFSFileInfo* file );
    void ( *file_changed ) ( VFSDir* dir, VFSFileInfo* file );
    void ( *thumbnail_loaded ) ( VFSDir* dir, VFSFileInfo* file );
    void ( *file_listed ) ( VFSDir* dir );
    void ( *load_complete ) ( VFSDir* dir );
    /*  void (*need_reload) ( VFSDir* dir ); */
    /*  void (*update_mime) ( VFSDir* dir ); */
};

typedef void ( *VFSDirStateCallback ) ( VFSDir* dir, int state, gpointer user_data );

GType vfs_dir_get_type ( void );

VFSDir* vfs_dir_get_by_path( const char* path );

gboolean vfs_dir_is_loading( VFSDir* dir );
void vfs_dir_cancel_load( VFSDir* dir );
gboolean vfs_dir_is_file_listed( VFSDir* dir );

void vfs_dir_unload_thumbnails( VFSDir* dir, gboolean is_big );

/* emit signals */
void vfs_dir_emit_file_created( VFSDir* dir, const char* file_name, VFSFileInfo* file );
void vfs_dir_emit_file_deleted( VFSDir* dir, const char* file_name, VFSFileInfo* file );
void vfs_dir_emit_file_changed( VFSDir* dir, const char* file_name, VFSFileInfo* file );
void vfs_dir_emit_thumbnail_loaded( VFSDir* dir, VFSFileInfo* file );

/* get the path of desktop dir */
const char* vfs_get_desktop_dir();

gboolean vfs_dir_add_hidden( const char* path, const char* file_name );  //MOD added

/* Get the path of user's trash dir under home dir.
 * NOTE:
 * According to the spec, there are many legal trash dirs on the system
 * located at various places. However, because that spec is poor and try
 * very hard to make simple things more complicated, we only support
 * home trash dir instead. They are good at making things complicated and
 * hard to implement. This time, they did it again.
 */
const char* vfs_get_trash_dir();

/* call function "func" for every VFSDir instances */
void vfs_dir_foreach( GHFunc func, gpointer user_data );

G_END_DECLS

#endif
