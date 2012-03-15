/*
*  C Interface: vfs-execute
*
* Description: 
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef _VFS_EXECUTE_H_
#define _VFS_EXECUTE_H_

#include <glib.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

#define VFS_EXEC_DEFAULT_FLAGS  (G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL)

gboolean vfs_exec( const char* work_dir,
                   char** argv, char** envp,
                   const char* disp_name,
                   GSpawnFlags flags,
                   GError **err );

gboolean vfs_exec_on_screen( GdkScreen* screen,
                             const char* work_dir,
                             char** argv, char** envp,
                             const char* disp_name,
                             GSpawnFlags flags,
                             GError **err );

G_END_DECLS

#endif
