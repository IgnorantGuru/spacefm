/*
 *      desktop.h
 *
 *      Copyright 2008 PCMan <pcman.tw@gmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 3 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#ifndef _DESKTOP_H_
#define _DESKTOP_H_

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include "desktop-window.h"


G_BEGIN_DECLS

void fm_turn_on_desktop_icons( gboolean transparent );
void fm_turn_off_desktop_icons();
void fm_desktop_update_thumbnails();
void fm_desktop_update_wallpaper( gboolean transparency_changed );
void fm_desktop_update_colors();
void fm_desktop_update_icons();
void fm_desktop_set_single_click( gboolean single_click );

G_END_DECLS

#endif
