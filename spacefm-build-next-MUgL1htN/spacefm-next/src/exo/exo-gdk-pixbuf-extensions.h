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

#ifndef __EXO_GDK_PIXBUF_EXTENSIONS_H__
#define __EXO_GDK_PIXBUF_EXTENSIONS_H__

#include <gdk/gdk.h>

/* Taken from exo v0.10.2 (Debian package libexo-1-0), according to changelog
 * commit f455681554ca205ffe49bd616310b19f5f9f8ef1 Dec 27 13:50:21 2012 */

G_BEGIN_DECLS

GdkPixbuf *exo_gdk_pixbuf_scale_down                (GdkPixbuf       *source,
                                                     gboolean         preserve_aspect_ratio,
                                                     gint             dest_width,
                                                     gint             dest_height) G_GNUC_WARN_UNUSED_RESULT;

GdkPixbuf *exo_gdk_pixbuf_colorize                  (const GdkPixbuf *source,
                                                     const GdkColor  *color) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

GdkPixbuf *exo_gdk_pixbuf_spotlight                 (const GdkPixbuf *source) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

GdkPixbuf *exo_gdk_pixbuf_frame                     (const GdkPixbuf *source,
                                                     const GdkPixbuf *frame,
                                                     gint             left_offset,
                                                     gint             top_offset,
                                                     gint             right_offset,
                                                     gint             bottom_offset) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS

#endif /* !__EXO_GDK_PIXBUF_EXTENSIONS_H__ */

