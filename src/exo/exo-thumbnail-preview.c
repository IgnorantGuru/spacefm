/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2015 OmegaPhil <OmegaPhil@startmail.com>
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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

/* Taken from exo v0.10.2 (Debian package libexo-1-0), according to changelog
 * commit f455681554ca205ffe49bd616310b19f5f9f8ef1 Dec 27 13:50:21 2012 */

#include "exo-gdk-pixbuf-extensions.h"
#include "exo-private.h"
#include "exo-thumbnail-preview.h"
#include "exo-utils.h"
#include "exo-string.h"
#include "vfs-thumbnail-loader.h"

// From exo-thumbnail.h
/**
 * ExoThumbnailSize:
 * @EXO_THUMBNAIL_SIZE_NORMAL : normal sized thumbnails (up to 128px).
 * @EXO_THUMBNAIL_SIZE_LARGE  : large sized thumbnails.
 *
 * Thumbnail sizes used by the thumbnail database.
 **/
typedef enum /*< skip >*/
{
    EXO_THUMBNAIL_SIZE_NORMAL = 128,
    EXO_THUMBNAIL_SIZE_LARGE  = 256,
} ExoThumbnailSize;


static void exo_thumbnail_preview_style_set (GtkWidget           *ebox,
                                             GtkStyle            *previous_style,
                                             ExoThumbnailPreview *thumbnail_preview);



struct _ExoThumbnailPreviewClass
{
    GtkFrameClass __parent__;
};

struct _ExoThumbnailPreview
{
    GtkFrame   __parent__;
    GtkWidget *image;
    GtkWidget *name_label;
    GtkWidget *size_label;
    GtkWidget *ebox;  // Required to competently set the widget background
                      // colour...
};



G_DEFINE_TYPE (ExoThumbnailPreview, exo_thumbnail_preview, GTK_TYPE_FRAME)



static void
exo_thumbnail_preview_class_init (ExoThumbnailPreviewClass *klass)
{
}



static void
exo_thumbnail_preview_init (ExoThumbnailPreview *thumbnail_preview)
{
    GtkWidget *button;
    GtkWidget *label;
    GtkWidget *ebox;
    GtkWidget *vbox;
    GtkWidget *box;

    gtk_frame_set_shadow_type (GTK_FRAME (thumbnail_preview), GTK_SHADOW_IN);
    gtk_widget_set_sensitive (GTK_WIDGET (thumbnail_preview), FALSE);

    ebox = gtk_event_box_new ();

    /* IgnorantGuru wants the background of the thumbnail widget to be grey/
     * black depending on the theme, rather than the default white, which is
     * associated with GTK_STATE_NORMAL
     * Note that this event box is what defines the colour, not thumbnail_preview,
     * so it must be accessible for later setting */
    gtk_widget_modify_bg (ebox, GTK_STATE_NORMAL,
                          &gtk_widget_get_style (ebox)->base[GTK_STATE_INSENSITIVE]);
    thumbnail_preview->ebox = ebox;

    g_signal_connect (G_OBJECT (ebox), "style-set", G_CALLBACK (exo_thumbnail_preview_style_set), thumbnail_preview);
    gtk_container_add (GTK_CONTAINER (thumbnail_preview), ebox);
    gtk_widget_show (ebox);

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (ebox), vbox);
    gtk_widget_show (vbox);

    button = gtk_button_new ();
    g_signal_connect (G_OBJECT (button), "button-press-event", G_CALLBACK (exo_noop_true), NULL);
    g_signal_connect (G_OBJECT (button), "button-release-event", G_CALLBACK (exo_noop_true), NULL);
    g_signal_connect (G_OBJECT (button), "enter-notify-event", G_CALLBACK (exo_noop_true), NULL);
    g_signal_connect (G_OBJECT (button), "leave-notify-event", G_CALLBACK (exo_noop_true), NULL);
    gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
    gtk_widget_show (button);

    label = gtk_label_new (_("Preview"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
    gtk_container_add (GTK_CONTAINER (button), label);
    gtk_widget_show (label);

    box = gtk_vbox_new (FALSE, 2);
    gtk_container_set_border_width (GTK_CONTAINER (box), 2);
    gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 0);
    gtk_widget_show (box);

    thumbnail_preview->image = gtk_image_new_from_stock (GTK_STOCK_MISSING_IMAGE, GTK_ICON_SIZE_DIALOG);
    gtk_widget_set_size_request (thumbnail_preview->image, EXO_THUMBNAIL_SIZE_NORMAL + 2 * 12, EXO_THUMBNAIL_SIZE_NORMAL + 2 * 12);
    gtk_image_set_pixel_size (GTK_IMAGE (thumbnail_preview->image), EXO_THUMBNAIL_SIZE_NORMAL / 2);
    gtk_box_pack_start (GTK_BOX (box), thumbnail_preview->image, FALSE, FALSE, 0);
    gtk_widget_show (thumbnail_preview->image);

    thumbnail_preview->name_label = gtk_label_new (_("No file selected"));
    gtk_label_set_justify (GTK_LABEL (thumbnail_preview->name_label), GTK_JUSTIFY_CENTER);
    gtk_label_set_ellipsize (GTK_LABEL (thumbnail_preview->name_label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_box_pack_start (GTK_BOX (box), thumbnail_preview->name_label, FALSE, FALSE, 0);
    gtk_widget_show (thumbnail_preview->name_label);

    thumbnail_preview->size_label = gtk_label_new ("");
    gtk_box_pack_start (GTK_BOX (box), thumbnail_preview->size_label, FALSE, FALSE, 0);
    gtk_widget_show (thumbnail_preview->size_label);
}



static void
exo_thumbnail_preview_style_set (GtkWidget           *ebox,
                                 GtkStyle            *previous_style,
                                 ExoThumbnailPreview *thumbnail_preview)
{
    _exo_return_if_fail (EXO_IS_THUMBNAIL_PREVIEW (thumbnail_preview));
    _exo_return_if_fail (GTK_IS_EVENT_BOX (ebox));

        /* Check if the ebox is already realized */
//sfm-gtk3 - note that as we target v2.18, can't just move to gtk_widget_get_realized
#if GTK_CHECK_VERSION (3, 0, 0)
    if (gtk_widget_get_realized (ebox))
#else
    if (GTK_WIDGET_REALIZED (ebox))
#endif
    {
        /* Set background color (using the base color) */
        /* IgnorantGuru wants the background of the thumbnail widget to be grey/
         * black depending on the theme, rather than the default white, which is
         * associated with GTK_STATE_NORMAL */
        g_signal_handlers_block_by_func (G_OBJECT (ebox),
                                         exo_thumbnail_preview_style_set,
                                         thumbnail_preview);
        GtkStyle *style = gtk_widget_get_style(GTK_WIDGET (ebox));
        gtk_widget_modify_bg (ebox, GTK_STATE_NORMAL,
                              &style->base[GTK_STATE_INSENSITIVE]);
        g_signal_handlers_unblock_by_func (G_OBJECT (ebox),
                                           exo_thumbnail_preview_style_set,
                                           thumbnail_preview);
    }
}



/**
 * _exo_thumbnail_preview_new:
 *
 * Allocates a new #ExoThumbnailPreview instance.
 *
 * Returns: the newly allocated #ExoThumbnailPreview.
 **/
GtkWidget*
_exo_thumbnail_preview_new (void)
{
    return g_object_new (EXO_TYPE_THUMBNAIL_PREVIEW, NULL);
}



/**
 * _exo_thumbnail_preview_set_uri:
 * @thumbnail_preview : an #ExoThumbnailPreview.
 * @uri               : the new URI for which to show a preview or %NULL.
 *
 * Updates the @thumbnail_preview to display a preview of the specified @uri.
 **/
void
_exo_thumbnail_preview_set_uri (ExoThumbnailPreview *thumbnail_preview,
                                const gchar         *uri)
{
    struct stat statb;
    GdkPixbuf  *thumbnail;
    gchar      *icon_name = NULL;
    gchar      *size_name = NULL;
    gchar      *displayname;
    gchar      *filename;
    gchar      *slash;

    _exo_return_if_fail (EXO_IS_THUMBNAIL_PREVIEW (thumbnail_preview));

    /* Check if we have an URI to preview */
    if (G_UNLIKELY (uri == NULL))
    {
        /* The preview widget is insensitive if we don't have an URI */
        gtk_widget_set_sensitive (GTK_WIDGET (thumbnail_preview), FALSE);
        gtk_image_set_from_stock (GTK_IMAGE (thumbnail_preview->image), GTK_STOCK_MISSING_IMAGE, GTK_ICON_SIZE_DIALOG);
        gtk_label_set_text (GTK_LABEL (thumbnail_preview->name_label), _("No file selected"));
    }
    else
    {
        /* Make the preview widget appear sensitive */
        gtk_widget_set_sensitive (GTK_WIDGET (thumbnail_preview), TRUE);
        gtk_widget_modify_bg (thumbnail_preview->ebox, GTK_STATE_NORMAL,
                              &gtk_widget_get_style (thumbnail_preview->ebox)->base[GTK_STATE_INSENSITIVE]);

        /* Check if we have a local file here */
        filename = g_filename_from_uri (uri, NULL, NULL);
        if (G_LIKELY (filename != NULL))
        {
            /* Try to stat the file */
            if (stat (filename, &statb) == 0)
            {
                /* Icon and size label depends on the mode */
                if (S_ISBLK (statb.st_mode))
                {
                    icon_name = g_strdup ("gnome-fs-blockdev");
                    size_name = g_strdup (_("Block Device"));
                }
                else if (S_ISCHR (statb.st_mode))
                {
                    icon_name = g_strdup ("gnome-fs-chardev");
                    size_name = g_strdup (_("Character Device"));
                }
                else if (S_ISDIR (statb.st_mode))
                {
                    icon_name = g_strdup ("gnome-fs-directory");
                    size_name = g_strdup (_("Folder"));
                }
                else if (S_ISFIFO (statb.st_mode))
                {
                    icon_name = g_strdup ("gnome-fs-fifo");
                    size_name = g_strdup (_("FIFO"));
                }
                else if (S_ISSOCK (statb.st_mode))
                {
                    icon_name = g_strdup ("gnome-fs-socket");
                    size_name = g_strdup (_("Socket"));
                }
                else if (S_ISREG (statb.st_mode))
                {
                    if (G_UNLIKELY ((gulong) statb.st_size > 1024ul * 1024ul * 1024ul))
                        size_name = g_strdup_printf ("%0.1f GB", statb.st_size / (1024.0 * 1024.0 * 1024.0));
                    else if ((gulong) statb.st_size > 1024ul * 1024ul)
                        size_name = g_strdup_printf ("%0.1f MB", statb.st_size / (1024.0 * 1024.0));
                    else if ((gulong) statb.st_size > 1024ul)
                        size_name = g_strdup_printf ("%0.1f kB", statb.st_size / 1024.0);
                    else
                        size_name = g_strdup_printf ("%lu B", (gulong) statb.st_size);
                }
            }

            /* Determine the basename from the filename */
            displayname = g_filename_display_basename (filename);
        }
        else
        {
            /* Determine the basename from the URI */
            slash = strrchr (uri, '/');
            if (G_LIKELY (!exo_str_is_empty (slash)))
                displayname = g_filename_display_name (slash + 1);
            else
                displayname = g_filename_display_name (uri);
        }

        /* Check if we have an icon-name */
        if (G_UNLIKELY (icon_name != NULL))
        {
            /* Setup the named icon then */
            gtk_image_set_from_icon_name (GTK_IMAGE (thumbnail_preview->image), icon_name, GTK_ICON_SIZE_DIALOG);
            g_free (icon_name);
        }
        else
        {
            /* Try to load a thumbnail for the URI */
            //thumbnail = _exo_thumbnail_get_for_uri (uri, EXO_THUMBNAIL_SIZE_NORMAL, NULL);
            thumbnail = vfs_thumbnail_load_for_uri (uri, EXO_THUMBNAIL_SIZE_NORMAL, 0);
            if (thumbnail == NULL && G_LIKELY (filename != NULL))
            {
                /* But we can try to generate a thumbnail */
                //thumbnail = _exo_thumbnail_get_for_file (filename, EXO_THUMBNAIL_SIZE_NORMAL, NULL);
                thumbnail = vfs_thumbnail_load_for_file (filename, EXO_THUMBNAIL_SIZE_NORMAL, 0);
            }

            /* Check if we have a thumbnail */
            if (G_LIKELY (thumbnail != NULL))
            {
                /* Setup the thumbnail for the image */
                gtk_image_set_from_pixbuf (GTK_IMAGE (thumbnail_preview->image), thumbnail);
                g_object_unref (G_OBJECT (thumbnail));
            }
            else
            {
                /* No thumbnail, cannot display anything useful then */
                gtk_image_set_from_stock (GTK_IMAGE (thumbnail_preview->image), GTK_STOCK_MISSING_IMAGE, GTK_ICON_SIZE_DIALOG);
            }
        }

        /* Setup the name label */
        gtk_label_set_text (GTK_LABEL (thumbnail_preview->name_label), displayname);

        /* Cleanup */
        g_free (displayname);
        g_free (filename);
    }

    /* Setup the new size label */
    gtk_label_set_text (GTK_LABEL (thumbnail_preview->size_label), (size_name != NULL) ? size_name : "");
    g_free (size_name);
}


#define __EXO_THUMBNAIL_PREVIEW_C__
