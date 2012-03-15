/*
*  C Interface: vfs-monitor
*
* Description: File alteration monitor
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/*
  FIXME: VFSFileMonitor can support at most 1024 monitored files.
         This is caused by the limit of FAM/gamin itself.
         Maybe using inotify directly can solve this?
*/

#ifndef _VFS_FILE_MONITOR_H_
#define _VFS_FILE_MONITOR_H_

#include <glib.h>

#ifdef USE_INOTIFY
#include <unistd.h>
#include "linux-inotify.h"
#include "inotify-syscalls.h"
#else /* Use FAM|gamin */
#include <fam.h>
#endif

G_BEGIN_DECLS

#ifdef USE_INOTIFY
typedef enum{
  VFS_FILE_MONITOR_CREATE,
  VFS_FILE_MONITOR_DELETE,
  VFS_FILE_MONITOR_CHANGE
}VFSFileMonitorEvent;
#else
typedef enum{
  VFS_FILE_MONITOR_CREATE = FAMCreated,
  VFS_FILE_MONITOR_DELETE = FAMDeleted,
  VFS_FILE_MONITOR_CHANGE = FAMChanged
}VFSFileMonitorEvent;
#endif

typedef struct _VFSFileMonitor VFSFileMonitor;

struct _VFSFileMonitor{
  gchar* path;
  /*<private>*/
  int n_ref;
#ifdef USE_INOTIFY
  int wd;
#else
  FAMRequest request;
#endif
  GArray* callbacks;
};

/* Callback function which will be called when monitored events happen
 *  NOTE: GDK_THREADS_ENTER and GDK_THREADS_LEAVE might be needed
 *  if gtk+ APIs are called in this callback, since the callback is called from
 *  IO channel handler.
 */
typedef void (*VFSFileMonitorCallback)( VFSFileMonitor* fm,
                                        VFSFileMonitorEvent event,
                                        const char* file_name,
                                        gpointer user_data );

/*
* Init monitor:
* Establish connection with gamin/fam.
*/
gboolean vfs_file_monitor_init();

/*
* Monitor changes of a file or directory.
*
* Parameters:
* path: the file/dir to be monitored
* cb: callback function to be called when file event happens.
* user_data: user data to be passed to callback function.
*/
VFSFileMonitor* vfs_file_monitor_add( char* path,
                                      gboolean is_dir,
                                      VFSFileMonitorCallback cb,
                                      gpointer user_data );

/*
* Monitor changes of a file.
*
* Parameters:
* path: the file/dir to be monitored
* cb: callback function to be called when file event happens.
* user_data: user data to be passed to callback function.
*/
#define vfs_file_monitor_add_file( path, cb, user_data )  \
                    vfs_file_monitor_add(path, FALSE, cb, user_data )

/*
* Monitor changes of a directory.
*
* Parameters:
* path: the file/dir to be monitored
* cb: callback function to be called when file event happens.
* user_data: user data to be passed to callback function.
*/
#define vfs_file_monitor_add_dir( path, cb, user_data )  \
                    vfs_file_monitor_add(path, TRUE, cb, user_data )

/*
 * Remove previously installed monitor.
 */
void vfs_file_monitor_remove( VFSFileMonitor* fm,
                              VFSFileMonitorCallback cb,
                              gpointer user_data );

/*
 * Clearn up and shutdown file alteration monitor.
 */
void vfs_file_monitor_clean();

G_END_DECLS

#endif
