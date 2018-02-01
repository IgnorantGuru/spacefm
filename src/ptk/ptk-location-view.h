/*
 * SpaceFM ptk-location-view.h
 * 
 * Copyright (C) 2015 IgnorantGuru <ignorantguru@gmx.com>
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

// Location View
GtkWidget* ptk_location_view_new( PtkFileBrowser* file_browser );
gboolean ptk_location_view_chdir( GtkTreeView* location_view, const char* path );
char* ptk_location_view_get_selected_dir( GtkTreeView* location_view );
gboolean ptk_location_view_is_item_volume(  GtkTreeView* location_view, GtkTreeIter* it );
VFSVolume* ptk_location_view_get_volume(  GtkTreeView* location_view, GtkTreeIter* it );
void ptk_location_view_show_trash_can( gboolean show );
void ptk_location_view_on_action( GtkWidget* view, XSet* set );
VFSVolume* ptk_location_view_get_selected_vol( GtkTreeView* location_view );
void update_volume_icons();
void ptk_location_view_mount_network( PtkFileBrowser* file_browser,
                                      const char* url,
                                      gboolean new_tab,
                                      gboolean force_new_mount );
void mount_iso( PtkFileBrowser* file_browser, const char* path );
void ptk_location_view_dev_menu( GtkWidget* parent, DesktopWindow* desktop,
                                            PtkFileBrowser* file_browser,
                                            GtkWidget* menu );
char* ptk_location_view_create_mount_point( int mode, VFSVolume* vol,
                                    netmount_t* netmount, const char* path );
char* ptk_location_view_get_mount_point_dir( const char* name );
void ptk_location_view_clean_mount_points();
gboolean ptk_location_view_open_block( const char* block, gboolean new_tab );


// Bookmark View
GtkWidget* ptk_bookmark_view_new( PtkFileBrowser* file_browser );
gboolean ptk_bookmark_view_chdir( GtkTreeView* view,
                                  PtkFileBrowser* file_browser,
                                  gboolean recurse );
void ptk_bookmark_view_add_bookmark( GtkMenuItem *menuitem,
                                     PtkFileBrowser* file_browser,
                                     const char* url );
char* ptk_bookmark_view_get_selected_dir( GtkTreeView* view );
void ptk_bookmark_view_update_icons( GtkIconTheme* icon_theme,
                                     PtkFileBrowser* file_browser );
void ptk_bookmark_view_xset_changed( GtkTreeView* view,
                    PtkFileBrowser* file_browser, const char* changed_name );
XSet* ptk_bookmark_view_get_first_bookmark( XSet* book_set );
void ptk_bookmark_view_import_gtk( const char* path, XSet* book_set );
void ptk_bookmark_view_on_open_reverse( GtkMenuItem* item,
                                        PtkFileBrowser* file_browser );


G_END_DECLS

#endif
