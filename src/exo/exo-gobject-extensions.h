/*-
 * Copyright (c) 2004 os-cillation e.K.
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

#ifndef __EXO_GOBJECT_EXTENSIONS_H__
#define __EXO_GOBJECT_EXTENSIONS_H__

#include <glib-object.h>

/* Taken from exo v0.10.2 (Debian package libexo-1-0), according to changelog
 * commit f455681554ca205ffe49bd616310b19f5f9f8ef1 Dec 27 13:50:21 2012 */

G_BEGIN_DECLS

gboolean  exo_g_value_transform_negate  (const GValue *src_value,
                                         GValue       *dst_value);

G_END_DECLS

#endif /* !__EXO_GOBJECT_EXTENSIONS_H__ */
