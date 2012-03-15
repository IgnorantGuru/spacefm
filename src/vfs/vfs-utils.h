/*
 *      vfs-utils.h
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

#ifndef _VFS_UTILS_H_
#define _VFS_UTILS_H_

#include <gtk/gtk.h>

GdkPixbuf* vfs_load_icon( GtkIconTheme* theme, const char* icon_name, int size );

/* execute programs with sudo */
gboolean vfs_sudo_cmd_sync( const char* cwd, char* argv[],
                                               int* exit_status,
                                               char** pstdout, char** pstderr, GError** err );  //MOD

gboolean vfs_sudo_cmd_async( const char* cwd, char* argv[], GError** err );  //MOD


#endif
