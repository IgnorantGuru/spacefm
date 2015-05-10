/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __EXO_THUMBNAIL_PREVIEW_H__
#define __EXO_THUMBNAIL_PREVIEW_H__

// TODO: No config here
//#include <exo/exo-config.h>

/* Taken from exo v0.10.2 (Debian package libexo-1-0), according to changelog
 * commit f455681554ca205ffe49bd616310b19f5f9f8ef1 Dec 27 13:50:21 2012 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _ExoThumbnailPreviewClass ExoThumbnailPreviewClass;
typedef struct _ExoThumbnailPreview      ExoThumbnailPreview;

#define EXO_TYPE_THUMBNAIL_PREVIEW            (exo_thumbnail_preview_get_type ())
#define EXO_THUMBNAIL_PREVIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EXO_TYPE_THUMBNAIL_PREVIEW, ExoThumbnailPreview))
#define EXO_THUMBNAIL_PREVIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EXO_TYPE_THUMBNAIL_PREVIEW, ExoThumbnailPreviewClass))
#define EXO_IS_THUMBNAIL_PREVIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EXO_TYPE_THUMBNAIL_PREVIEW))
#define EXO_IS_THUMBNAIL_PREVIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EXO_TYPE_THUMBNAIL_PREVIEW))
#define EXO_THUMBNAIL_PREVIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EXO_TYPE_THUMBNAIL_PREVIEW, ExoThumbnailPreviewClass))

G_GNUC_INTERNAL GType      exo_thumbnail_preview_get_type  (void) G_GNUC_CONST;

G_GNUC_INTERNAL GtkWidget *_exo_thumbnail_preview_new      (void) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

G_GNUC_INTERNAL void       _exo_thumbnail_preview_set_uri  (ExoThumbnailPreview *thumbnail_preview,
                                                            const gchar         *uri);

G_END_DECLS

#endif /* !__EXO_THUMBNAIL_PREVIEW_H__ */
