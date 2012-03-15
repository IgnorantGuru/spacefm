/*
 * Guifications - The end all, be all, toaster popup plugin
 * Copyright (C) 2003-2004 Gary Kramlich
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
  2005.07.22 Modified by Hong Jen Yee (PCMan)
  This piece of code detecting working area is got from Guifications, a plug-in for Gaim.
*/

# include <gdk/gdk.h>
# include <gdk/gdkx.h>
# include <X11/Xlib.h>
# include <X11/Xutil.h>
# include <X11/Xatom.h>

gboolean
gf_display_get_workarea(GdkScreen* g_screen, GdkRectangle *rect) {
	Atom xa_desktops, xa_current, xa_workarea, xa_type;
	Display *x_display;
	Window x_root;
	guint32 desktops = 0, current = 0;
	gulong *workareas, len, fill;
	guchar *data;
	gint format;

	GdkDisplay *g_display;
	Screen *x_screen;

	/* get the gdk display */
	g_display = gdk_display_get_default();
	if(!g_display)
		return FALSE;

	/* get the x display from the gdk display */
	x_display = gdk_x11_display_get_xdisplay(g_display);
	if(!x_display)
		return FALSE;

	/* get the x screen from the gdk screen */
	x_screen = gdk_x11_screen_get_xscreen(g_screen);
	if(!x_screen)
		return FALSE;

	/* get the root window from the screen */
	x_root = XRootWindowOfScreen(x_screen);

	/* find the _NET_NUMBER_OF_DESKTOPS atom */
	xa_desktops = XInternAtom(x_display, "_NET_NUMBER_OF_DESKTOPS", True);
	if(xa_desktops == None)
		return FALSE;

	/* get the number of desktops */
	if(XGetWindowProperty(x_display, x_root, xa_desktops, 0, 1, False,
						  XA_CARDINAL, &xa_type, &format, &len, &fill,
						  &data) != Success)
	{
		return FALSE;
	}

	if(!data)
		return FALSE;

	desktops = *(guint32 *)data;
	XFree(data);

	/* find the _NET_CURRENT_DESKTOP atom */
	xa_current = XInternAtom(x_display, "_NET_CURRENT_DESKTOP", True);
	if(xa_current == None)
		return FALSE;

	/* get the current desktop */
	if(XGetWindowProperty(x_display, x_root, xa_current, 0, 1, False,
						  XA_CARDINAL, &xa_type, &format, &len, &fill,
						  &data) != Success)
	{
		return FALSE;
	}

	if(!data)
		return FALSE;

	current = *(guint32 *)data;
	XFree(data);

	/* find the _NET_WORKAREA atom */
	xa_workarea = XInternAtom(x_display, "_NET_WORKAREA", True);
	if(xa_workarea == None)
		return FALSE;

	if(XGetWindowProperty(x_display, x_root, xa_workarea, 0, (glong)(4 * 32),
						  False, AnyPropertyType, &xa_type, &format, &len,
						  &fill, &data) != Success)
	{
		return FALSE;
	}

	/* make sure the type and format are good */
	if(xa_type == None || format == 0)
		return FALSE;

	/* make sure we don't have any leftovers */
	if(fill)
		return FALSE;

	/* make sure len divides evenly by 4 */
	if(len % 4)
		return FALSE;

	/* it's good, lets use it */
	workareas = (gulong *)(guint32 *)data;

	rect->x = (guint32)workareas[current * 4];
	rect->y = (guint32)workareas[current * 4 + 1];
	rect->width = (guint32)workareas[current * 4 + 2];
	rect->height = (guint32)workareas[current * 4 + 3];

	/* clean up our memory */
	XFree(data);

	return TRUE;
}

void get_working_area( GdkScreen* screen, GdkRectangle* area )
{
	if( !gf_display_get_workarea(screen, area) )
	{
		area->x = 0;
		area->y = 0;
		area->width = gdk_screen_width();
		area->height = gdk_screen_height();
	}
}
