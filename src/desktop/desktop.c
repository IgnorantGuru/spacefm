/*
 *      main.c - desktop manager of pcmanfm
 *
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "desktop.h"

#ifdef DESKTOP_INTEGRATION

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
//#include "fm-desktop.h"

#include "vfs-file-info.h"
#include "vfs-mime-type.h"
//#include "vfs-app-desktop.h"

#include "vfs-file-monitor.h"
#include "vfs-volume.h"
#include "vfs-thumbnail-loader.h"
#include "vfs-dir.h"

#include "desktop-window.h"

#include "settings.h"

static GtkWindowGroup* group = NULL;

static GdkFilterReturn on_rootwin_event ( GdkXEvent *xevent, GdkEvent *event, gpointer data );

static GtkWidget **desktops = NULL;
static gint n_screens = 0;

static guint theme_change_notify = 0;

static void on_icon_theme_changed( GtkIconTheme* theme, gpointer data )
{
	/* reload icons of desktop windows */
    int i;
    for ( i = 0; i < n_screens; i++ )
        desktop_window_reload_icons( (DesktopWindow*)desktops[ i ] );
}

/*
#include <glib-object.h>   // for g_signal_connect
void on_size_changed( GdkScreen *screen, GtkWidget* w )
{
    printf( "screen size changed  %d, %d\n", gdk_screen_get_width( screen ),
                                             gdk_screen_get_height( screen ) );
}
*/

void fm_turn_on_desktop_icons(gboolean transparent)
{
    GdkDisplay * gdpy;
    gint i;
    int big = 0;

    gdpy = gdk_display_get_default();
#if GTK_CHECK_VERSION (3, 0, 0)
    if( ! GDK_IS_X11_DISPLAY( gdpy ) )
        return;
#endif

    if( ! group )
        group = gtk_window_group_new();

    theme_change_notify = g_signal_connect( gtk_icon_theme_get_default(), "changed",
                                    G_CALLBACK(on_icon_theme_changed), NULL );

    vfs_mime_type_get_icon_size( &big, NULL );

    n_screens = gdk_display_get_n_screens( gdpy );
    desktops = g_new( GtkWidget *, n_screens );
    for ( i = 0; i < n_screens; i++ )
    {
        desktops[ i ] = desktop_window_new(transparent);
        //printf("added desktop window %p to screen %d on display %p (%s)\n",
        //                  desktops[ i ], i, gdpy, g_getenv( "DISPLAY" ) );
        ((DesktopWindow*)desktops[ i ])->screen_index = i;
        desktop_window_set_icon_size( (DesktopWindow*)desktops[ i ], big );
        desktop_window_set_single_click( (DesktopWindow*)desktops[ i ],
                                            app_settings.desk_single_click );

        gtk_window_set_role( GTK_WINDOW( desktops[i] ), "desktop_manager" );

        gtk_widget_realize( desktops[ i ] );  /* without this, setting wallpaper won't work */
        gtk_widget_show_all( desktops[ i ] );
        
        // move desktop window to 0,0 so background in window aligns with
        // root window background for transparent panel
        // re https://github.com/IgnorantGuru/spacefm/issues/248
        gtk_window_move( GTK_WINDOW( desktops[ i ] ), 0, 0 );
        
        gdk_window_lower( gtk_widget_get_window(desktops[ i ]) );

        gtk_window_group_add_window( GTK_WINDOW_GROUP(group), GTK_WINDOW( desktops[i] ) );
        
        /*   this doesn't work when size is changed via xrandr?
        // temp detect screen size change
        g_signal_connect( gtk_widget_get_screen( GTK_WIDGET( desktops[ i ] ) ),
                            "size-changed", G_CALLBACK( on_size_changed ),
                            desktops[ i ] );
        */
    }
    fm_desktop_update_colors();
    fm_desktop_update_wallpaper( FALSE );
}

void fm_turn_off_desktop_icons()
{
    int i;

    if( theme_change_notify )
    {
        g_signal_handler_disconnect( gtk_icon_theme_get_default(), theme_change_notify );
        theme_change_notify = 0;
    }

    for ( i = 0; i < n_screens; i++ )
    {
        gtk_widget_destroy( desktops[ i ] );
        /* gtk_window_group_remove_window() */
    }
    g_free( desktops );

//    if ( busy_cursor > 0 )
//        g_source_remove( busy_cursor );
    g_object_unref( group );
    group = NULL;
}

void fm_desktop_update_thumbnails()
{
    /* FIXME: thumbnail on desktop cannot be turned off. */
}

void fm_desktop_update_wallpaper( gboolean transparency_changed )
{
    DWBgType type;
    GdkPixbuf* pix;
    int i;
    
    // is desktop manager running?
    if ( !n_screens || !desktops )
        return;

    if( transparency_changed )
    {
        fm_turn_off_desktop_icons();
        fm_turn_on_desktop_icons( type == DW_BG_TRANSPARENT );
        return;
    }

    if( app_settings.show_wallpaper && app_settings.wallpaper )
    {
        switch( app_settings.wallpaper_mode )
        {
        case WPM_FULL:
            type = DW_BG_FULL;
            break;
        case WPM_ZOOM:
            type = DW_BG_ZOOM;
            break;
        case WPM_CENTER:
            type = DW_BG_CENTER;
            break;
        case WPM_TILE:
            type = DW_BG_TILE;
            break;
        case WPM_TRANSPARENT:
            type = DW_BG_TRANSPARENT;
            break;
        case WPM_STRETCH:
        default:
            type = DW_BG_STRETCH;
        }
        pix = gdk_pixbuf_new_from_file( app_settings.wallpaper, NULL );
    }
    else
    {
        type = DW_BG_COLOR;
        pix = NULL;
    }

    for ( i = 0; i < n_screens; i++ )
        desktop_window_set_background( DESKTOP_WINDOW(desktops[ i ]), pix, type );

    if( pix )
        g_object_unref( pix );
}

void fm_desktop_update_colors()
{
    int i;
    for ( i = 0; i < n_screens; i++ )
    {
        desktop_window_set_bg_color( DESKTOP_WINDOW(desktops[ i ]), &app_settings.desktop_bg1 );
        desktop_window_set_text_color( DESKTOP_WINDOW(desktops[ i ]), &app_settings.desktop_text, &app_settings.desktop_shadow );
    }
}

void fm_desktop_update_icons()
{
    int i;
    int big = 0;

    vfs_mime_type_get_icon_size( &big, NULL );

    for ( i = 0; i < n_screens; i++ )
        desktop_window_set_icon_size( (DesktopWindow*)desktops[ i ], big );
}

void fm_desktop_set_single_click( gboolean single_click )
{
    int i;
    for ( i = 0; i < n_screens; i++ )
        desktop_window_set_single_click( (DesktopWindow*)desktops[ i ], single_click );
}

#else /* ! DESKTOP_INTEGRATION */

/* dummy implementations */
void fm_turn_on_desktop_icons( gboolean transparent ) { }
void fm_turn_off_desktop_icons() { }
void fm_desktop_update_thumbnails() { }
void fm_desktop_update_wallpaper( gboolean transparency_changed ) { }
void fm_desktop_update_colors() { }
void fm_desktop_update_icons() { }
void fm_desktop_set_single_click( gboolean single_click ) { }
#endif

