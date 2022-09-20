/*
*  C Interface: ptkdirtreeview
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef _PTK_DIR_TREE_VIEW_H_
#define _PTK_DIR_TREE_VIEW_H_

#include <gtk/gtk.h>
#include "ptk-file-browser.h"

G_BEGIN_DECLS

/* Create a new dir tree view */
GtkWidget* ptk_dir_tree_view_new( PtkFileBrowser* browser,
                                  gboolean show_hidden );

gboolean ptk_dir_tree_view_chdir( GtkTreeView* dir_tree_view, const char* path );

/* Return a newly allocated string containing path of current selected dir. */
char* ptk_dir_tree_view_get_selected_dir( GtkTreeView* dir_tree_view );

void ptk_dir_tree_view_show_hidden_files( GtkTreeView* dir_tree_view,
                                          gboolean show_hidden );

char* ptk_dir_view_get_dir_path( GtkTreeModel* model, GtkTreeIter* it );

G_END_DECLS

#endif

