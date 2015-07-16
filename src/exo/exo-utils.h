/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>.
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

#ifndef __EXO_UTILS_H__
#define __EXO_UTILS_H__

#include <glib.h>

/* Taken from exo v0.10.2 (Debian package libexo-1-0), according to changelog
 * commit f455681554ca205ffe49bd616310b19f5f9f8ef1 Dec 27 13:50:21 2012 */

G_BEGIN_DECLS

void                    exo_noop        (void) G_GNUC_PURE;
gint                    exo_noop_one    (void) G_GNUC_PURE;
gint                    exo_noop_zero   (void) G_GNUC_PURE;
gpointer                exo_noop_null   (void) G_GNUC_PURE;
gboolean                exo_noop_true   (void) G_GNUC_PURE;
gboolean                exo_noop_false  (void) G_GNUC_PURE;


G_END_DECLS

#endif /* !__EXO_UTILS_H__ */
