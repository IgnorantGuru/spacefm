/*
*  C Implementation: vfs-execute
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "vfs-execute.h"

/* FIXME: Startup notification may cause problems */
#define SN_API_NOT_YET_FROZEN
#include <libsn/sn-launcher.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>

#include <string.h>
#include <stdlib.h>

#include <time.h>


gboolean vfs_exec( const char* work_dir,
                   char** argv, char** envp,
                   const char* disp_name,
                   GSpawnFlags flags,
                   GError **err )
{
    return vfs_exec_on_screen( gdk_screen_get_default(), work_dir,
                               argv, envp, disp_name, flags, err );
}

static gboolean sn_timeout( gpointer user_data )
{
    SnLauncherContext * ctx = ( SnLauncherContext* ) user_data;
    gdk_threads_enter();
    /* FIXME: startup notification, is this correct? */
    sn_launcher_context_complete ( ctx );
    sn_launcher_context_unref ( ctx );
    gdk_threads_leave();
    return FALSE;
}

/* This function is taken from the code of thunar, written by Benedikt Meurer <benny@xfce.org> */
static gint
tvsn_get_active_workspace_number ( GdkScreen *screen )
{
    GdkWindow * root;
    gulong bytes_after_ret = 0;
    gulong nitems_ret = 0;
    guint *prop_ret = NULL;
    Atom _NET_CURRENT_DESKTOP;
    Atom _WIN_WORKSPACE;
    Atom type_ret = None;
    gint format_ret;
    gint ws_num = 0;

    gdk_error_trap_push ();

    root = gdk_screen_get_root_window ( screen );

    /* determine the X atom values */
    _NET_CURRENT_DESKTOP = XInternAtom ( GDK_WINDOW_XDISPLAY ( root ), "_NET_CURRENT_DESKTOP", False );
    _WIN_WORKSPACE = XInternAtom ( GDK_WINDOW_XDISPLAY ( root ), "_WIN_WORKSPACE", False );

    if ( XGetWindowProperty ( GDK_WINDOW_XDISPLAY ( root ), GDK_WINDOW_XWINDOW ( root ),
                              _NET_CURRENT_DESKTOP, 0, 32, False, XA_CARDINAL,
                              &type_ret, &format_ret, &nitems_ret, &bytes_after_ret,
                              ( gpointer ) & prop_ret ) != Success )
    {
        if ( XGetWindowProperty ( GDK_WINDOW_XDISPLAY ( root ), GDK_WINDOW_XWINDOW ( root ),
                                  _WIN_WORKSPACE, 0, 32, False, XA_CARDINAL,
                                  &type_ret, &format_ret, &nitems_ret, &bytes_after_ret,
                                  ( gpointer ) & prop_ret ) != Success )
        {
            if ( G_UNLIKELY ( prop_ret != NULL ) )
            {
                XFree ( prop_ret );
                prop_ret = NULL;
            }
        }
    }

    if ( G_LIKELY ( prop_ret != NULL ) )
    {
        if ( G_LIKELY ( type_ret != None && format_ret != 0 ) )
            ws_num = *prop_ret;
        XFree ( prop_ret );
    }

    gdk_error_trap_pop ();

    return ws_num;
}

gboolean vfs_exec_on_screen( GdkScreen* screen,
                             const char* work_dir,
                             char** argv, char** envp,
                             const char* disp_name,
                             GSpawnFlags flags,
                             GError **err )
{
    SnLauncherContext * ctx = NULL;
    SnDisplay* display;
    gboolean ret;
    GSpawnChildSetupFunc setup_func = NULL;
    extern char **environ;
    char** new_env = envp;
    int i, n_env = 0;
    char* display_name;
    int display_index = -1, startup_id_index = -1;

    if ( ! envp )
        envp = environ;

    n_env = g_strv_length(envp);

    new_env = g_new0( char*, n_env + 4 );
    for ( i = 0; i < n_env; ++i )
    {
        /* g_debug( "old envp[%d] = \"%s\"" , i, envp[i]); */
        if ( 0 == strncmp( envp[ i ], "DISPLAY=", 8 ) )
            display_index = i;
        else
        {
            if ( 0 == strncmp( envp[ i ], "DESKTOP_STARTUP_ID=", 19 ) )
                startup_id_index = i;
            new_env[i] = g_strdup( envp[ i ] );
        }
    }

    display = sn_display_new ( GDK_SCREEN_XDISPLAY ( screen ),
                               ( SnDisplayErrorTrapPush ) gdk_error_trap_push,
                               ( SnDisplayErrorTrapPush ) gdk_error_trap_pop );
    if ( G_LIKELY ( display ) )
    {
        if ( !disp_name )
            disp_name = argv[ 0 ];

        ctx = sn_launcher_context_new( display, gdk_screen_get_number( screen ) );

        sn_launcher_context_set_description( ctx, disp_name );
        sn_launcher_context_set_name( ctx, g_get_prgname() );
        sn_launcher_context_set_binary_name( ctx, argv[ 0 ] );

        sn_launcher_context_set_workspace ( ctx, tvsn_get_active_workspace_number( screen ) );

        /* FIXME: I don't think this is correct, other people seem to use CurrentTime here.
                  However, using CurrentTime causes problems, so I so it like this.
                  Maybe this is incorrect, but it works, so, who cares?
        */
        /* time( &cur_time ); */
        sn_launcher_context_initiate( ctx, g_get_prgname(),
                                      argv[ 0 ], gtk_get_current_event_time() /*cur_time*/ );

        setup_func = (GSpawnChildSetupFunc) sn_launcher_context_setup_child_process;
        if( startup_id_index >= 0 )
            g_free( new_env[i] );
        else
            startup_id_index = i++;
        new_env[ startup_id_index ] = g_strconcat( "DESKTOP_STARTUP_ID=",
                                      sn_launcher_context_get_startup_id ( ctx ), NULL );
    }

    /* This is taken from gdk_spawn_on_screen */
    display_name = gdk_screen_make_display_name ( screen );
    if ( display_index >= 0 )
        new_env[ display_index ] = g_strconcat( "DISPLAY=", display_name, NULL );
    else
        new_env[ i++ ] = g_strconcat( "DISPLAY=", display_name, NULL );

    g_free( display_name );
    new_env[ i ] = NULL;

    ret = g_spawn_async( work_dir,
                         argv,  new_env,
                         flags,
                         NULL, NULL,
                         NULL, err );

    /* for debugging */
#if 0
    g_debug( "debug vfs_execute_on_screen(): flags: %d, display_index=%d", flags, display_index );
    for( i = 0; argv[i]; ++i ) {
        g_debug( "argv[%d] = \"%s\"" , i, argv[i] );
    }
    for( i = 0; i < n_env /*new_env[i]*/; ++i ) {
        g_debug( "new_env[%d] = \"%s\"" , i, new_env[i] );
    }
    if( ret )
        g_debug( "the program was executed without error" );
    else
        g_debug( "launch failed: %s", (*err)->message );
#endif

    g_strfreev( new_env );

    if ( G_LIKELY ( ctx ) )
    {
        if ( G_LIKELY ( ret ) )
            g_timeout_add ( 20 * 1000, sn_timeout, ctx );
        else
        {
            sn_launcher_context_complete ( ctx );
            sn_launcher_context_unref ( ctx );
        }
    }

    if ( G_LIKELY ( display ) )
        sn_display_unref ( display );

    return ret;
}

