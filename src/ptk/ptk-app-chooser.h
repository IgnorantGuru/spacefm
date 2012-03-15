/*
*  C Interface: appchooserdlg
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef _APP_CHOOSER_DLG_H_
#define _APP_CHOOSER_DLG_H_

#include <gtk/gtk.h>
#include "vfs-mime-type.h"

G_BEGIN_DECLS

/* Let the user choose a application */
gchar* ptk_choose_app_for_mime_type( GtkWindow* parent,
                                    VFSMimeType* mime_type );

/* Let the user choose a application */
GtkWidget* app_chooser_dialog_new( GtkWindow* parent, VFSMimeType* mime_type );

/*
* Return selected application in a ``newly allocated'' string.
* Returned string is the file name of the *.desktop file or a command line.
* These two can be separated by check if the returned string is ended
* with ".desktop".
*/
gchar* app_chooser_dialog_get_selected_app( GtkWidget* dlg );

/*
* Check if the user set the selected app default handler.
*/
gboolean app_chooser_dialog_get_set_default( GtkWidget* dlg );


void
on_notebook_switch_page                (GtkNotebook     *notebook,
                                        GtkNotebookPage *page,
                                        guint            page_num,
                                        gpointer         user_data);

void
on_browse_btn_clicked                  (GtkButton       *button,
                                        gpointer         user_data);


G_END_DECLS

#endif

