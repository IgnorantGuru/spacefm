/*
*  C Interface: PtkLocationView
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef  _PTK_LOCATION_VIEW_H_
#define  _PTK_LOCATION_VIEW_H_

#include <gtk/gtk.h>
#include <sys/types.h>

#include "vfs-volume.h"

G_BEGIN_DECLS

/* Create a new location view */
GtkWidget* ptk_location_view_new( PtkFileBrowser* file_browser );
GtkWidget* ptk_bookmark_view_new( PtkFileBrowser* file_browser );

gboolean ptk_location_view_chdir( GtkTreeView* location_view, const char* path );

char* ptk_location_view_get_selected_dir( GtkTreeView* location_view );
char* ptk_bookmark_view_get_selected_dir( GtkTreeView* bookmark_view );
char* ptk_bookmark_view_get_selected_name( GtkTreeView* bookmark_view );

gboolean ptk_location_view_is_item_bookmark( GtkTreeView* location_view,
                                             GtkTreeIter* it );

void ptk_location_view_rename_selected_bookmark( GtkTreeView* location_view );

gboolean ptk_location_view_is_item_volume(  GtkTreeView* location_view, GtkTreeIter* it );

VFSVolume* ptk_location_view_get_volume(  GtkTreeView* location_view, GtkTreeIter* it );

void ptk_location_view_show_trash_can( gboolean show );

void ptk_location_view_on_action( GtkWidget* view, XSet* set );
VFSVolume* ptk_location_view_get_selected_vol( GtkTreeView* location_view );
void update_volume_icons();
void update_bookmark_icons();
void on_bookmark_remove( GtkMenuItem* item, PtkFileBrowser* file_browser );
void on_bookmark_rename( GtkMenuItem* item, PtkFileBrowser* file_browser );
void on_bookmark_edit( GtkMenuItem* item, PtkFileBrowser* file_browser );
void on_bookmark_open( GtkMenuItem* item, PtkFileBrowser* file_browser );
void on_bookmark_open_tab( GtkMenuItem* item, PtkFileBrowser* file_browser );

void mount_network( PtkFileBrowser* file_browser, const char* url );
void mount_iso( PtkFileBrowser* file_browser, const char* path );

G_END_DECLS

#endif
