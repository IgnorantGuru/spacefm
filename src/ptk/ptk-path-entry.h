/*
*  C Interface: ptk-path-entry
*
* Description: A custom entry widget with auto-completion
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef _PTK_PATH_ENTRY_
#define _PTK_PATH_ENTRY_

#include <gtk/gtk.h>
#include "ptk-file-browser.h"

G_BEGIN_DECLS

typedef struct
{
    PtkFileBrowser* browser;
    guint seek_timer;
} EntryData;

GtkWidget* ptk_path_entry_new( PtkFileBrowser* file_browser );
void ptk_path_entry_help( GtkWidget* widget, GtkWidget* parent );

G_END_DECLS

#endif
