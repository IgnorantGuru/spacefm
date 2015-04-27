/*-
 * Copyright (c) 2004-2006 os-cillation e.K.
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#ifndef __EXO_GTK_EXTENSIONS_H__
#define __EXO_GTK_EXTENSIONS_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

void      exo_gtk_object_destroy_later                (GtkObject      *object);

void      exo_gtk_file_chooser_add_thumbnail_preview  (GtkFileChooser *chooser);

void      exo_gtk_url_about_dialog_hook               (GtkAboutDialog *about_dialog,
                                                       const gchar    *link,
                                                       gpointer        user_data);

G_END_DECLS

#endif /* !__EXO_GTK_EXTENSIONS_H__ */
