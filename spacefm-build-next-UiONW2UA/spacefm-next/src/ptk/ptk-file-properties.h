/*                                                                                  
*  C Interface: file_properties
*
* Description: 
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef _FILE_PROPERTIES_H_
#define _FILE_PROPERTIES_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

GtkWidget* file_properties_dlg_new( GtkWindow* parent,
                                    const char* dir_path,
                                    GList* sel_files, int page );

void on_filePropertiesDlg_response          (GtkDialog       *dialog,
                                             gint             response_id,
                                             gpointer         user_data);

G_END_DECLS

#endif

