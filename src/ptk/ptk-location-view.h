/*
 * SpaceFM ptk-location-view.h
 * 
 * Copyright (C) 2014 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>
 * 
 * License: See COPYING file
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
gboolean ptk_bookmark_view_chdir( GtkTreeView* bookmark_view, const char* path );

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

void ptk_location_view_mount_network( PtkFileBrowser* file_browser,
                                      const char* url,
                                      gboolean new_tab,
                                      gboolean force_new_mount );
void mount_iso( PtkFileBrowser* file_browser, const char* path );
void ptk_location_view_dev_menu( GtkWidget* parent, PtkFileBrowser* file_browser, 
                                                            GtkWidget* menu );
char* ptk_location_view_create_mount_point( int mode, VFSVolume* vol,
                                    netmount_t* netmount, const char* path );
gboolean ptk_location_view_open_block( const char* block, gboolean new_tab );

G_END_DECLS

#endif
