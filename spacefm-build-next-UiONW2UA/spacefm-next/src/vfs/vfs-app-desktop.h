/*
*  C Interface: vfs-app-desktop
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef _VFS_APP_DESKTOP_H_
#define _VFS_APP_DESKTOP_H_

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _VFSAppDesktop VFSAppDesktop;

struct _VFSAppDesktop
{
    char* file_name;
    char* disp_name;
    char* comment;
    char* exec;
    char* icon_name;
    char* path;         // working dir
    char* full_path;    // path of desktop file
    gboolean terminal : 1;
    gboolean hidden : 1;
    gboolean startup : 1;

    /* <private> */
    int n_ref;
};

/*
* If file_name is not a full path, this function searches default paths
* for the desktop file.
*/
VFSAppDesktop* vfs_app_desktop_new( const char* file_name );

void vfs_app_desktop_ref( VFSAppDesktop* app );

void vfs_app_desktop_unref( gpointer data );

const char* vfs_app_desktop_get_name( VFSAppDesktop* app );

const char* vfs_app_desktop_get_disp_name( VFSAppDesktop* app );

const char* vfs_app_desktop_get_exec( VFSAppDesktop* app );

GdkPixbuf* vfs_app_desktop_get_icon( VFSAppDesktop* app, int size, gboolean use_fallback );

const char* vfs_app_desktop_get_icon_name( VFSAppDesktop* app );

gboolean vfs_app_desktop_open_multiple_files( VFSAppDesktop* app );

gboolean vfs_app_desktop_open_in_terminal( VFSAppDesktop* app );

gboolean vfs_app_desktop_is_hidden( VFSAppDesktop* app );

gboolean vfs_app_desktop_open_files( GdkScreen* screen,
                                     const char* working_dir,
                                     VFSAppDesktop* app,
                                     GList* file_paths,
                                     GError** err );
gboolean vfs_app_desktop_rename( char* desktop_file_path, char* new_name );   //sfm

G_END_DECLS

#endif
