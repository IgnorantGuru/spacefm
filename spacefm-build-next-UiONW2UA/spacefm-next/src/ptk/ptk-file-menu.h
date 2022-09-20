/*
*  C Interface: ptk-file-menu
*
* Description: Popup menu for files
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef _PTK_FILE_MENU_H_
#define _PTK_FILE_MENU_H_

#include <gtk/gtk.h>
#include "ptk-file-browser.h"
#include "vfs-file-info.h"
#include "desktop-window.h"

G_BEGIN_DECLS

/* sel_files is a list containing VFSFileInfo structures
  * The list will be freed in this function, so the caller mustn't
  * free the list after calling this function.
  */
  
typedef struct _PtkFileMenu PtkFileMenu;
struct _PtkFileMenu
{
    PtkFileBrowser* browser;
    DesktopWindow* desktop;
    char* cwd;
    char* file_path;
    VFSFileInfo* info;
    GList* sel_files;
    GtkAccelGroup *accel_group;
};


GtkWidget* ptk_file_menu_new( DesktopWindow* desktop, PtkFileBrowser* browser,
                                const char* file_path, VFSFileInfo* info,
                                const char* cwd, GList* sel_files );

void ptk_file_menu_add_panel_view_menu( PtkFileBrowser* browser,
                                        GtkWidget* menu,
                                        GtkAccelGroup* accel_group );

void on_popup_open_in_new_tab_here( GtkMenuItem *menuitem,
                                        PtkFileMenu* data );

void ptk_file_menu_action( DesktopWindow* desktop, PtkFileBrowser* browser,
                                                            char* setname );

void on_popup_sortby( GtkMenuItem *menuitem, PtkFileBrowser* file_browser, int order );
void on_popup_list_detailed( GtkMenuItem *menuitem, PtkFileBrowser* browser );
void on_popup_list_icons( GtkMenuItem *menuitem, PtkFileBrowser* browser );
void on_popup_list_compact( GtkMenuItem *menuitem, PtkFileBrowser* browser );
void on_popup_list_large( GtkMenuItem *menuitem, PtkFileBrowser* browser );
void on_popup_rubber( GtkMenuItem *menuitem, PtkFileBrowser* file_browser );

G_END_DECLS

#endif

