/*-
 * Copyright (c) 2004-2006 os-cillation e.K.
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
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

//sfm-gtk3
#include <gtk/gtk.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "exo-gtk-extensions.h"
#include "exo-private.h"
#include "exo-thumbnail-preview.h"

/* Taken from exo v0.10.2 (Debian package libexo-1-0), according to changelog
 * commit f455681554ca205ffe49bd616310b19f5f9f8ef1 Dec 27 13:50:21 2012 */

/**
 * SECTION: exo-gtk-extensions
 * @title: Extensions to Gtk+
 * @short_description: Miscelleanous extensions to the Gtk+ library
 * @include: exo/exo.h
 *
 * Various additional functions to the core API provided by the Gtk+ library.
 *
 * For example, exo_gtk_file_chooser_add_thumbnail_preview() is a
 * convenience method to add a thumbnail based preview widget to a
 * #GtkFileChooser, which will display a preview of the selected file if
 * either a thumbnail is available or a thumbnail could be generated using
 * the GdkPixbuf library.
 **/


static void
update_preview (GtkFileChooser      *chooser,
                ExoThumbnailPreview *thumbnail_preview)
{
    gchar *uri;

    _exo_return_if_fail (EXO_IS_THUMBNAIL_PREVIEW (thumbnail_preview));
    _exo_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

    /* Update the URI for the preview */
    uri = gtk_file_chooser_get_preview_uri (chooser);
    if (G_UNLIKELY (uri == NULL))
    {
        /* Gee, why is there a get_preview_uri() method if
        * it doesn't work in several cases? did anybody ever
        * test this method prior to committing it?
        */
        uri = gtk_file_chooser_get_uri (chooser);
    }

    /* This code is still ran when the file chooser is apparently not ready to
     * provide either the preview URI or the real file URI - however I can't
     * ignore this as sometimes it is genuine (there are no files in the
     * directory and therefore the preview needs to be reset) - therefore
     * letting it through */
    _exo_thumbnail_preview_set_uri (thumbnail_preview, uri);
    g_free (uri);

    /* Indicating to GTK that we can successfully preview this file (since the
     * filter is on Image Files we should be able to deal with everything) */
    gtk_file_chooser_set_preview_widget_active (chooser, TRUE);
}



/**
 * exo_gtk_file_chooser_add_thumbnail_preview:
 * @chooser : a #GtkFileChooser.
 *
 * This is a convenience function that adds a preview widget to the @chooser,
 * which displays thumbnails for the selected filenames using the thumbnail
 * database. The preview widget is also able to generate thumbnails for all
 * image formats supported by #GdkPixbuf.
 *
 * Use this function whenever you display a #GtkFileChooser to ask the user
 * to select an image file from the file system.
 *
 * The preview widget also supports URIs other than file:-URIs to a certain
 * degree, but this support is rather limited currently, so you may want to
 * use gtk_file_chooser_set_local_only() to ensure that the user can only
 * select files from the local file system.
 *
 * When @chooser is configured to select multiple image files - using the
 * gtk_file_chooser_set_select_multiple() method - the behaviour of the
 * preview widget is currently undefined, in that it is not defined for
 * which of the selected files the preview will be displayed.
 *
 * Since: 0.3.1.9
 **/
void
exo_gtk_file_chooser_add_thumbnail_preview (GtkFileChooser *chooser)
{
    GtkWidget *thumbnail_preview;

    g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

    /* Add the preview to the file chooser */
    thumbnail_preview = _exo_thumbnail_preview_new ();
    gtk_file_chooser_set_preview_widget (chooser, thumbnail_preview);
    gtk_file_chooser_set_preview_widget_active (chooser, TRUE);
    gtk_file_chooser_set_use_preview_label (chooser, FALSE);
    gtk_widget_show (thumbnail_preview);

    /* Update the preview as necessary. Note that the 'update-preview' signal
     * only fires after the initial image load happens, and forcing an update
     * right now is too early, the preview URI and file URI come back NULL - the
     * only signal that seems to do the job is 'selection-changed' */
    g_signal_connect (G_OBJECT (chooser), "selection-changed",
                      G_CALLBACK (update_preview), thumbnail_preview);

    /* Initially update the preview, in case the file chooser is already set up.
     * Keeping this here inspite the above comment as this is supposed to be
     * generic code, shouldn't be tied to the specific circumstances of the icon
     * chooser dialog */
    update_preview (chooser, EXO_THUMBNAIL_PREVIEW (thumbnail_preview));
}


#define __EXO_GTK_EXTENSIONS_C__
