/*
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

/* turn on to debug GDK_THREADS_ENTER/GDK_THREADS_LEAVE related deadlocks */
#undef _DEBUG_THREAD

#include "private.h"

#include <gtk/gtk.h>
#include <glib.h>

#include <stdlib.h>
#include <string.h>

/* socket is used to keep single instance */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <signal.h>

#include <unistd.h> /* for getcwd */

#include <locale.h>

#include "main-window.h"

#include "vfs-file-info.h"
#include "vfs-mime-type.h"
#include "vfs-app-desktop.h"

#include "vfs-file-monitor.h"
#include "vfs-volume.h"
#include "vfs-thumbnail-loader.h"

#include "ptk-utils.h"
#include "ptk-app-chooser.h"
#include "ptk-file-properties.h"
#include "ptk-file-menu.h"

#include "find-files.h"
#include "pref-dialog.h"
#include "settings.h"

#include "desktop.h"
#include "cust-dialog.h"

//gboolean startup_mode = TRUE;  //MOD
//gboolean design_mode = TRUE;  //MOD

char* run_cmd = NULL;  //MOD

typedef enum{
    CMD_OPEN = 1,
    CMD_OPEN_TAB,
    CMD_REUSE_TAB,
    CMD_DAEMON_MODE,
    CMD_PREF,
    CMD_WALLPAPER,
    CMD_FIND_FILES,
    CMD_OPEN_PANEL1,
    CMD_OPEN_PANEL2,
    CMD_OPEN_PANEL3,
    CMD_OPEN_PANEL4,
    CMD_PANEL1,
    CMD_PANEL2,
    CMD_PANEL3,
    CMD_PANEL4,
    CMD_DESKTOP,
    CMD_NO_TABS,
    CMD_SOCKET_CMD,
    SOCKET_RESPONSE_OK,
    SOCKET_RESPONSE_ERROR,
    SOCKET_RESPONSE_DATA
}SocketEvent;

static gboolean folder_initialized = FALSE;
static gboolean desktop_or_deamon_initialized = FALSE;

static int sock;
GIOChannel* io_channel = NULL;

gboolean daemon_mode = FALSE;

static char* default_files[2] = {NULL, NULL};
static char** files = NULL;
static gboolean no_desktop = FALSE;
static gboolean old_show_desktop = FALSE;

static gboolean new_tab = TRUE;
static gboolean reuse_tab = FALSE;  //sfm
static gboolean no_tabs = FALSE;     //sfm
static gboolean new_window = FALSE;
static gboolean desktop_pref = FALSE;  //MOD
static gboolean desktop = FALSE;  //MOD
static gboolean profile = FALSE;  //MOD
static gboolean custom_dialog = FALSE;  //sfm
static gboolean socket_cmd = FALSE;     //sfm
static gboolean version_opt = FALSE;     //sfm
static gboolean sdebug = FALSE;         //sfm

static int show_pref = 0;
static int panel = -1;
static gboolean set_wallpaper = FALSE;

static gboolean find_files = FALSE;
static char* config_dir = NULL;

#ifdef HAVE_HAL
static char* mount = NULL;
static char* umount = NULL;
static char* eject = NULL;
#endif

static int n_pcmanfm_ref = 0;

static GOptionEntry opt_entries[] =
{
    { "new-tab", 't', 0, G_OPTION_ARG_NONE, &new_tab, N_("Open folders in new tab of last window (default)"), NULL },
    { "reuse-tab", 'r', 0, G_OPTION_ARG_NONE, &reuse_tab, N_("Open folder in current tab of last used window"), NULL },
    { "no-saved-tabs", 'n', 0, G_OPTION_ARG_NONE, &no_tabs, N_("Don't load saved tabs"), NULL },
    { "new-window", 'w', 0, G_OPTION_ARG_NONE, &new_window, N_("Open folders in new window"), NULL },
    { "panel", 'p', 0, G_OPTION_ARG_INT, &panel, N_("Open folders in panel 'P' (1-4)"), "P" },
    { "desktop", '\0', 0, G_OPTION_ARG_NONE, &desktop, N_("Launch desktop manager daemon"), NULL },
    { "desktop-pref", '\0', 0, G_OPTION_ARG_NONE, &desktop_pref, N_("Show desktop settings"), NULL },
    { "show-pref", '\0', 0, G_OPTION_ARG_INT, &show_pref, N_("Show Preferences ('N' is the Pref tab number)"), "N" },
    { "daemon-mode", 'd', 0, G_OPTION_ARG_NONE, &daemon_mode, N_("Run as a daemon"), NULL },
    { "config-dir", 'c', 0, G_OPTION_ARG_STRING, &config_dir, N_("Use DIR as configuration directory"), "DIR" },
    { "find-files", 'f', 0, G_OPTION_ARG_NONE, &find_files, N_("Show File Search"), NULL },
/*
    { "query-type", '\0', 0, G_OPTION_ARG_STRING, &query_type, N_("Query mime-type of the specified file."), NULL },
    { "query-default", '\0', 0, G_OPTION_ARG_STRING, &query_default, N_("Query default application of the specified mime-type."), NULL },
    { "set-default", '\0', 0, G_OPTION_ARG_STRING, &set_default, N_("Set default application of the specified mime-type."), NULL },
*/
#ifdef DESKTOP_INTEGRATION
    { "set-wallpaper", '\0', 0, G_OPTION_ARG_NONE, &set_wallpaper, N_("Set desktop wallpaper to FILE"), NULL },
#endif
    { "dialog", 'g', 0, G_OPTION_ARG_NONE, &custom_dialog, N_("Show a custom dialog (See -g help)"), NULL },
    { "socket-cmd", 's', 0, G_OPTION_ARG_NONE, &socket_cmd, N_("Send a socket command (See -s help)"), NULL },
    { "profile", '\0', 0, G_OPTION_ARG_STRING, &profile, N_("No function - for compatibility only"), "PROFILE" },
    { "no-desktop", '\0', 0, G_OPTION_ARG_NONE, &no_desktop, N_("No function - for compatibility only"), NULL },
    { "version", '\0', 0, G_OPTION_ARG_NONE, &version_opt, N_("Show version information"), NULL },

    { "sdebug", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &sdebug, NULL, NULL },

#ifdef HAVE_HAL
    /* hidden arguments used to mount volumes */
    { "mount", 'm', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &mount, NULL, NULL },
    { "umount", 'u', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &umount, NULL, NULL },
    { "eject", 'e', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &eject, NULL, NULL },
#endif
    {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &files, NULL, N_("[DIR | FILE | URL]...")},
    { NULL }
};

static gboolean single_instance_check();
static void single_instance_finalize();
static void get_socket_name( char* buf, int len );
static gboolean on_socket_event( GIOChannel* ioc, GIOCondition cond, gpointer data );

static void init_folder();
static void init_daemon_or_desktop();
static void check_icon_theme();

static gboolean handle_parsed_commandline_args();

static FMMainWindow* create_main_window();
static void open_file( const char* path );

static GList* get_file_info_list( char** files );
static char* dup_to_absolute_file_path( char** file );
void receive_socket_command( int client, GString* args );  //sfm

gboolean on_socket_event( GIOChannel* ioc, GIOCondition cond, gpointer data )
{
    int client, r;
    socklen_t addr_len = 0;
    struct sockaddr_un client_addr ={ 0 };
    static char buf[ 1024 ];
    GString* args;
    char** file;
    SocketEvent cmd;

    if ( cond & G_IO_IN )
    {
        client = accept( g_io_channel_unix_get_fd( ioc ), (struct sockaddr *)&client_addr, &addr_len );
        if ( client != -1 )
        {
            args = g_string_new_len( NULL, 2048 );
            while( (r = read( client, buf, sizeof(buf) )) > 0 )
            {
                g_string_append_len( args, buf, r);
                if ( args->str[0] == CMD_SOCKET_CMD && args->len > 1 && 
                                            args->str[args->len - 2] == '\n' &&
                                            args->str[args->len - 1] == '\n' )
                    // because CMD_SOCKET_CMD doesn't immediately close the socket
                    // data is terminated by two linefeeds to prevent read blocking
                    break;
            }
            if ( args->str[0] == CMD_SOCKET_CMD )
                receive_socket_command( client, args );
            shutdown( client, 2 );
            close( client );

            new_tab = TRUE;
            panel = 0;
            reuse_tab = FALSE;
            no_tabs = FALSE;

            int argx = 0;
            if ( args->str[argx] == CMD_NO_TABS )
            {
                reuse_tab = FALSE;
                no_tabs = TRUE;
                argx++;  //another command follows CMD_NO_TABS
            }
            if ( args->str[argx] == CMD_REUSE_TAB )
            {
                reuse_tab = TRUE;
                new_tab = FALSE;
                argx++;  //another command follows CMD_REUSE_TAB
            }

            switch( args->str[argx] )
            {
            case CMD_PANEL1:
                panel = 1;
                break;
            case CMD_PANEL2:
                panel = 2;
                break;
            case CMD_PANEL3:
                panel = 3;
                break;
            case CMD_PANEL4:
                panel = 4;
                break;
            case CMD_OPEN:
                new_tab = FALSE;
                break;
            case CMD_OPEN_PANEL1:
                new_tab = FALSE;
                panel = 1;
                break;
            case CMD_OPEN_PANEL2:
                new_tab = FALSE;
                panel = 2;
                break;
            case CMD_OPEN_PANEL3:
                new_tab = FALSE;
                panel = 3;
                break;
            case CMD_OPEN_PANEL4:
                new_tab = FALSE;
                panel = 4;
                break;
            case CMD_DAEMON_MODE:
                daemon_mode = TRUE;
                g_string_free( args, TRUE );
                return TRUE;
            case CMD_DESKTOP:
                desktop = TRUE;
                break;
            case CMD_PREF:
                GDK_THREADS_ENTER();
                fm_edit_preference( NULL, (unsigned char)args->str[1] - 1 );
                GDK_THREADS_LEAVE();
                g_string_free( args, TRUE );
                return TRUE;
            case CMD_WALLPAPER:
                set_wallpaper = TRUE;
                break;
            case CMD_FIND_FILES:
                find_files = TRUE;
                break;
            case CMD_SOCKET_CMD:
                g_string_free( args, TRUE );
                return TRUE;
                break;
            }

            if( args->str[ argx + 1 ] )
                files = g_strsplit( args->str + argx + 1, "\n", 0 );
            else
                files = NULL;
            g_string_free( args, TRUE );

            GDK_THREADS_ENTER();

            if( files )
            {
                for( file = files; *file; ++file )
                {
                    if( ! **file )  /* remove empty string at tail */
                        *file = NULL;
                }
            }
            handle_parsed_commandline_args();
            app_settings.load_saved_tabs = TRUE;
            
            GDK_THREADS_LEAVE();
        }
    }

    return TRUE;
}

void get_socket_name_nogdk( char* buf, int len )
{
    char* dpy = g_strdup( g_getenv( "DISPLAY" ) );
    if ( dpy && !strcmp( dpy, ":0.0" ) )
    {
        // treat :0.0 as :0 to prevent multiple instances on screen 0
        g_free( dpy );
        dpy = g_strdup( ":0" );
    }
    g_snprintf( buf, len, "/tmp/.spacefm-socket%s-%s", dpy, g_get_user_name() );
    g_free( dpy );
}

void get_socket_name( char* buf, int len )
{
    char* dpy = gdk_get_display();
    if ( dpy && !strcmp( dpy, ":0.0" ) )
    {
        // treat :0.0 as :0 to prevent multiple instances on screen 0
        g_free( dpy );
        dpy = g_strdup( ":0" );
    }
    g_snprintf( buf, len, "/tmp/.spacefm-socket%s-%s", dpy, g_get_user_name() );
    g_free( dpy );
}

gboolean single_instance_check()
{
    struct sockaddr_un addr;
    int addr_len;
    int ret;
    int reuse;

    if ( ( sock = socket( AF_UNIX, SOCK_STREAM, 0 ) ) == -1 )
    {
        ret = 1;
        goto _exit;
    }

    addr.sun_family = AF_UNIX;
    get_socket_name( addr.sun_path, sizeof( addr.sun_path ) );
#ifdef SUN_LEN
    addr_len = SUN_LEN( &addr );
#else

    addr_len = strlen( addr.sun_path ) + sizeof( addr.sun_family );
#endif

    /* try to connect to existing instance */
    if ( connect( sock, ( struct sockaddr* ) & addr, addr_len ) == 0 )
    {
        /* connected successfully */
        char** file;
        char cmd = CMD_OPEN_TAB;

        if ( no_tabs )
        {
            cmd = CMD_NO_TABS;
            write( sock, &cmd, sizeof(char) );
            // another command always follows CMD_NO_TABS
            cmd = CMD_OPEN_TAB;
        }
        if ( reuse_tab )
        {
            cmd = CMD_REUSE_TAB;
            write( sock, &cmd, sizeof(char) );
            // another command always follows CMD_REUSE_TAB
            cmd = CMD_OPEN;
        }
        
        if( daemon_mode )
            cmd = CMD_DAEMON_MODE;
        else if( desktop )
            cmd = CMD_DESKTOP;
        else if( new_window )
        {
            if ( panel > 0 && panel < 5 )
                cmd = CMD_OPEN_PANEL1 + panel - 1;
            else
                cmd = CMD_OPEN;
        }
        else if( show_pref > 0 )
            cmd = CMD_PREF;
        else if ( desktop_pref )  //MOD
        {
            cmd = CMD_PREF;
            show_pref = 3;
        }
        else if( set_wallpaper )
            cmd = CMD_WALLPAPER;
        else if( find_files )
            cmd = CMD_FIND_FILES;
        else if ( panel > 0 && panel < 5 )
            cmd = CMD_PANEL1 + panel - 1;            

        // open a new window if no file spec
        if ( cmd == CMD_OPEN_TAB && !files )
            cmd = CMD_OPEN;
            
        write( sock, &cmd, sizeof(char) );
        if( G_UNLIKELY( show_pref > 0 ) )
        {
            cmd = (unsigned char)show_pref;
            write( sock, &cmd, sizeof(char) );
        }
        else
        {
            if( files )
            {
                for( file = files; *file; ++file )
                {
                    char *real_path;

                    if ( ( *file[0] != '/' && strstr( *file, ":/" ) )
                                        || g_str_has_prefix( *file, "//" ) )
                        real_path = g_strdup( *file );
                    else
                    {
                        /* We send absolute paths because with different
                           $PWDs resolution would not work. */
                        real_path = dup_to_absolute_file_path( file );
                    }
                    write( sock, real_path, strlen( real_path ) );
                    g_free( real_path );
                    write( sock, "\n", 1 );
                }
            }
        }
        if ( config_dir )
            g_warning( _("Option --config-dir ignored - an instance is already running") );
        shutdown( sock, 2 );
        close( sock );
        ret = 0;
        goto _exit;
    }

    /* There is no existing server, and we are in the first instance. */
    unlink( addr.sun_path ); /* delete old socket file if it exists. */
    reuse = 1;
    ret = setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof( reuse ) );
    if ( bind( sock, ( struct sockaddr* ) & addr, addr_len ) == -1 )
    {
        ret = 1;
        goto _exit;
    }

    io_channel = g_io_channel_unix_new( sock );
    g_io_channel_set_encoding( io_channel, NULL, NULL );
    g_io_channel_set_buffered( io_channel, FALSE );

    g_io_add_watch( io_channel, G_IO_IN,
                    ( GIOFunc ) on_socket_event, NULL );

    if ( listen( sock, 5 ) == -1 )
    {
        ret = 1;
        goto _exit;
    }
    
    // custom config-dir
    if ( config_dir && strpbrk( config_dir, " $%\\()&#|:;?<>{}[]*\"'" ) )
    {
        g_warning( _("Option --config-dir contains invalid chars - cannot start") );
        ret = 1;
        goto _exit;
    }
    return TRUE;

_exit:

    gdk_notify_startup_complete();
    exit( ret );
}

void single_instance_finalize()
{
    char lock_file[ 256 ];

    shutdown( sock, 2 );
    g_io_channel_unref( io_channel );
    close( sock );

    get_socket_name( lock_file, sizeof( lock_file ) );
    unlink( lock_file );
}

void receive_socket_command( int client, GString* args )  //sfm
{
    char** argv;
    char** arg;
    char cmd;
    
    if ( args->str[1] )
    {
        if ( g_str_has_suffix( args->str, "\n\n" ) )
        {
            // remove empty strings at tail
            args->str[args->len - 1] = '\0';
            args->str[args->len - 2] = '\0';
        }
        argv = g_strsplit( args->str + 1, "\n", 0 );
    }
    else
        argv = NULL;

/*
    if ( argv )
    {
        printf( "receive:\n");
        for ( arg = argv; *arg; ++arg )
        {
            if ( ! **arg )  // skip empty string
            {
                printf( "    (skipped empty)\n");
                continue;
            }
            printf( "    %s\n", *arg );
        }
    }
*/
    // process command and get reply
    char* reply = NULL;
    gdk_threads_enter();
    cmd = main_window_socket_command( argv, &reply );
    gdk_threads_leave();
    g_strfreev( argv );
    
    // send response
    write( client, &cmd, sizeof(char) );  // send exit status
    if ( reply && reply[0] )
        write( client, reply, strlen( reply ) ); // send reply or error msg
    g_free( reply );
}

int send_socket_command( int argc, char* argv[], char** reply )   //sfm
{
    struct sockaddr_un addr;
    int addr_len;
    int ret;

    *reply = NULL;
    if ( argc < 3 )
    {
        fprintf( stderr, _("spacefm: --socket-cmd requires an argument\n") );
        return 1;
    }

    // create socket
    if ( ( sock = socket( AF_UNIX, SOCK_STREAM, 0 ) ) == -1 )
    {
        fprintf( stderr, _("spacefm: could not create socket\n") );
        return 1;
    }

    // open socket
    addr.sun_family = AF_UNIX;
    get_socket_name_nogdk( addr.sun_path, sizeof( addr.sun_path ) );
#ifdef SUN_LEN
    addr_len = SUN_LEN( &addr );
#else
    addr_len = strlen( addr.sun_path ) + sizeof( addr.sun_family );
#endif

    if ( connect( sock, ( struct sockaddr* ) & addr, addr_len ) != 0 )
    {
        fprintf( stderr, _("spacefm: could not connect to socket (not running? or DISPLAY not set?)\n") );
        return 1;
    }

    // send command
    char cmd = CMD_SOCKET_CMD;
    write( sock, &cmd, sizeof(char) );

    // send arguments
    int i;
    for ( i = 2; i < argc; i++ )
    {
        write( sock, argv[i], strlen( argv[i] ) );
        write( sock, "\n", 1 );
    }
    write( sock, "\n", 1 );
    
    // get response
    GString* sock_reply = g_string_new_len( NULL, 2048 );
    int r;
    static char buf[ 1024 ];
    
    while( ( r = read( sock, buf, sizeof( buf ) ) ) > 0 )
        g_string_append_len( sock_reply, buf, r);

    // close socket
    shutdown( sock, 2 );
    close( sock );

    // set reply
    if ( sock_reply->len != 0 )
    {
        *reply = g_strdup( sock_reply->str + 1 );
        ret = sock_reply->str[0];
    }
    else
    {
        fprintf( stderr, _("spacefm: invalid response from socket\n") );
        ret = 1;
    }
    g_string_free( sock_reply, TRUE );
    return ret;
}

void show_socket_help()
{
    printf( "%s\n", _("SpaceFM socket commands permit external processes (such as command scripts)") );
    printf( "%s\n", _("to read and set GUI property values and execute methods inside running SpaceFM") );
    printf( "%s\n", _("windows.  To handle events see View|Auto Run in the main menu bar.") );

    printf( "\n%s\n", _("Usage:") );
    printf( "    spacefm --socket-cmd|-s METHOD [OPTIONS] [ARGUMENT...]\n" );
    printf( "%s\n", _("Example:") );
    printf( "    spacefm -s set window_size 800x600\n" );

    printf( "\n%s\n", _("METHODS\n-------") );
    printf( "spacefm -s set PROPERTY [VALUE...]\n" );
    printf( "    %s\n", _("Sets a property") );

    printf( "\nspacefm -s get PROPERTY\n" );
    printf( "    %s\n", _("Gets a property") );

    printf( "\nspacefm -s set-task TASKID TASKPROPERTY [VALUE...]\n" );
    printf( "    %s\n", _("Sets a task property") );

    printf( "\nspacefm -s get-task TASKID TASKPROPERTY\n" );
    printf( "    %s\n", _("Gets a task property") );

    printf( "\nspacefm -s emit-key KEYCODE [MODIFIER]\n" );
    printf( "    %s\n", _("Activates a menu item by emitting its shortcut key") );

    printf( "\nspacefm -s show-menu MENUNAME\n" );
    printf( "    %s\n", _("Shows custom submenu named MENUNAME as a popup menu") );

    printf( "\nspacefm -s add-event EVENT COMMAND ...\n" );
    printf( "    %s\n", _("Add asynchronous handler COMMAND to EVENT") );

    printf( "\nspacefm -s replace-event EVENT COMMAND ...\n" );
    printf( "    %s\n", _("Add synchronous handler COMMAND to EVENT, replacing default handler") );

    printf( "\nspacefm -s remove-event EVENT COMMAND ...\n" );
    printf( "    %s\n", _("Remove handler COMMAND from EVENT") );

    printf( "\nspacefm -s help|--help\n" );
    printf( "    %s\n", _("Shows this help reference.  (Also see manual link below.)") );

    printf( "\n%s\n", _("OPTIONS\n-------") );
    printf( "%s\n", _("Add options after METHOD to specify a specific window, panel, and/or tab.") );
    printf( "%s\n", _("Otherwise the current tab of the current panel in the last window is used.") );

    printf( "\n--window WINDOWID\n" );
    printf( "    %s spacefm -s set --window 0x104ca80 window_size 800x600\n", _("Specify window.  eg:") );
    printf( "--panel PANEL\n" );
    printf( "    %s spacefm -s set --panel 2 bookmarks_visible true\n", _("Specify panel 1-4.  eg:") );
    printf( "--tab TAB\n" );
    printf( "    %s spacefm -s set selected_filenames --tab 3 fstab\n", _("Specify tab 1-...  eg:") );

    printf( "\n%s\n", _("PROPERTIES\n----------") );
    printf( "%s\n", _("Set properties with METHOD 'set', or get the value with 'get'.") );

    printf( "\nwindow_size                     eg '800x600'\n" );
    printf( "window_position                 eg '100x50'\n" );
    printf( "window_maximized                1|true|yes|0|false|no\n" );
    printf( "window_fullscreen               1|true|yes|0|false|no\n" );
    printf( "screen_size                     eg '1024x768'  (read-only)\n" );
    printf( "window_vslider_top              eg '100'\n" );
    printf( "window_vslider_bottom           eg '100'\n" );
    printf( "window_hslider                  eg '100'\n" );
    printf( "window_tslider                  eg '100'\n" );
    printf( "focused_panel                   1|2|3|4|prev|next|hide\n" );
    printf( "focused_pane                    filelist|devices|bookmarks|dirtree|pathbar\n" );
    printf( "current_tab                     1|2|...|prev|next|close\n" );
    printf( "bookmarks_visible               1|true|yes|0|false|no\n" );
    printf( "dirtree_visible                 1|true|yes|0|false|no\n" );
    printf( "toolbar_visible                 1|true|yes|0|false|no\n" );
    printf( "sidetoolbar_visible             1|true|yes|0|false|no\n" );
    printf( "hidden_files_visible            1|true|yes|0|false|no\n" );
    printf( "panel1_visible                  1|true|yes|0|false|no\n" );
    printf( "panel2_visible                  1|true|yes|0|false|no\n" );
    printf( "panel3_visible                  1|true|yes|0|false|no\n" );
    printf( "panel4_visible                  1|true|yes|0|false|no\n" );
    printf( "panel_hslider_top               eg '100'\n" );
    printf( "panel_hslider_bottom            eg '100'\n" );
    printf( "panel_vslider                   eg '100'\n" );
    printf( "column_width                    name|size|type|permission|owner|modified WIDTH\n" );
    printf( "statusbar_text                  %s\n", _("eg 'Current Status: Example'") );
    printf( "pathbar_text                    [TEXT [SELSTART [SELEND]]]\n" );
    printf( "current_dir                     %s\n", _("DIR            eg '/etc'") );
    printf( "selected_filenames              %s\n", _("[FILENAME ...]") );
    printf( "selected_pattern                %s\n", _("[PATTERN]      eg '*.jpg'") );
    printf( "clipboard_text                  %s\n", _("eg 'Some\\nlines\\nof text'") );
    printf( "clipboard_primary_text          %s\n", _("eg 'Some\\nlines\\nof text'") );
    printf( "clipboard_from_file             %s\n", _("eg '~/copy-file-contents-to-clipboard.txt'") );
    printf( "clipboard_primary_from_file     %s\n", _("eg '~/copy-file-contents-to-clipboard.txt'") );
    printf( "clipboard_copy_files            %s\n", _("FILE ...  Files copied to clipboard") );
    printf( "clipboard_cut_files             %s\n", _("FILE ...  Files cut to clipboard") );
    printf( "edit_file                       %s\n", _("FILE        Open FILE in user's text editor") );
    printf( "run_in_terminal                 %s\n", _("COMMAND...  Run COMMAND in user's terminal") );

    printf( "\n%s\n", _("TASK PROPERTIES\n---------------") );
    printf( "status                          %s\n", _("contents of Status task column  (read-only)") );
    printf( "icon                            %s\n", _("eg 'gtk-open'") );
    printf( "count                           %s\n", _("text to show in Count task column") );
    printf( "folder                          %s\n", _("text to show in Folder task column") );
    printf( "item                            %s\n", _("text to show in Item task column") );
    printf( "to                              %s\n", _("text to show in To task column") );
    printf( "progress                        %s\n", _("Progress percent (1..100) or '' to pulse") );
    printf( "total                           %s\n", _("text to show in Total task column") );
    printf( "curspeed                        %s\n", _("text to show in Current task column") );
    printf( "curremain                       %s\n", _("text to show in CRemain task column") );
    printf( "avgspeed                        %s\n", _("text to show in Average task column") );
    printf( "avgremain                       %s\n", _("text to show in Remain task column") );
    printf( "elapsed                         %s\n", _("contents of Elapsed task column (read-only)") );
    printf( "started                         %s\n", _("contents of Started task column (read-only)") );
    printf( "queue_state                     run|pause|queue|stop\n" );
    printf( "popup_handler                   %s\n", _("COMMAND  command to show a custom task dialog\n") );

    printf( "\n%s\n", _("EVENTS\n------") );
    printf( "evt_start                       %s\n", _("Instance start        %e") );
    printf( "evt_exit                        %s\n", _("Instance exit         %e") );
    printf( "evt_win_new                     %s\n", _("Window new            %e %w %p %t") );
    printf( "evt_win_focus                   %s\n", _("Window focus          %e %w %p %t") );
    printf( "evt_win_move                    %s\n", _("Window move/resize    %e %w %p %t") );
    printf( "evt_win_click                   %s\n", _("Mouse click           %e %w %p %t %b %m %f") );
    printf( "evt_win_key                     %s\n", _("Window keypress       %e %w %p %t %k %m") );
    printf( "evt_win_close                   %s\n", _("Window close          %e %w %p %t") );
    printf( "evt_pnl_focus                   %s\n", _("Panel focus           %e %w %p %t") );
    printf( "evt_pnl_show                    %s\n", _("Panel show/hide       %e %w %p %t %f %v") );
    printf( "evt_pnl_sel                     %s\n", _("Selection changed     %e %w %p %t") );
    printf( "evt_tab_new                     %s\n", _("Tab new               %e %w %p %t") );
    printf( "evt_tab_focus                   %s\n", _("Tab focus             %e %w %p %t") );
    printf( "evt_tab_close                   %s\n", _("Tab close             %e %w %p %t") );
    printf( "evt_device                      %s\n", _("Device change         %e %f %v") );

    printf( "\n%s\n", _("Event COMMAND Substitution Variables:") );
    printf( "    %%e   %s\n", _("event name (evt_start|evt_exit|...)") );
    printf( "    %%w   %s\n", _("window ID") );
    printf( "    %%p   %s\n", _("panel number (1-4)") );
    printf( "    %%t   %s\n", _("tab number (1-...)") );
    printf( "    %%b   %s\n", _("mouse button (0=double 1=left 2=middle 3=right ...") );
    printf( "    %%k   %s\n", _("key code  (eg 0x63)") );
    printf( "    %%m   %s\n", _("modifier key (eg 0x4  used with clicks and keypresses)") );
    printf( "    %%f   %s\n", _("focus element (panelN|filelist|devices|bookmarks|dirtree|pathbar)") );
    printf( "    %%v   %s\n", _("focus element is visible (0 or 1, or device state change)") );

    printf( "\n%s:\n\n", _("Examples") );

    printf( "    window_size=\"$(spacefm -s get window_size)\"\n" );
    printf( "    spacefm -s set window_size 1024x768\n" );
    printf( "    spacefm -s set column_width name 100\n" );
    printf( "    spacefm -s set-task $fm_my_task progress 25\n" );
    printf( "    spacefm -r /etc; sleep 0.3; spacefm -s set selected_filenames fstab hosts\n" );
    printf( "    spacefm -s set clipboard_copy_files /etc/fstab /etc/hosts\n" );
    printf( "    spacefm -s emit-key 0xffbe 0   # press F1 to show Help\n" );
    printf( "    spacefm -s show-menu --window $fm_my_window \"Custom Menu\"\n" );
    printf( "    spacefm -s add-event evt_pnl_sel 'spacefm -s set statusbar_text \"$fm_file\"'\n\n" );
    
    printf( "    #!/bin/bash\n" );
    printf( "    eval copied_files=\"$(spacefm -s get clipboard_copy_files)\"\n" );
    printf( "    echo \"%s:\"\n", _("These files have been copied to the clipboard") );
    printf( "    i=0\n" );
    printf( "    while [ \"${copied_files[i]}\" != \"\" ]; do\n" );
    printf( "        echo \"    ${copied_files[i]}\"\n" );
    printf( "        (( i++ ))\n" );
    printf( "    done\n" );
    printf( "    if (( i != 0 )); then\n" );
    printf( "        echo \"MD5SUMS:\"\n" );
    printf( "        md5sum \"${copied_files[@]}\"\n" );
    printf( "    fi\n" );

    printf( "\n%s\n    http://ignorantguru.github.com/spacefm/spacefm-manual-en.html#sockets\n", _("For full documentation and examples see the SpaceFM User's Manual:") );
}

FMMainWindow* create_main_window()
{
    FMMainWindow * main_window = FM_MAIN_WINDOW(fm_main_window_new ());
    gtk_window_set_default_size( GTK_WINDOW( main_window ),
                                 app_settings.width, app_settings.height );
    if ( app_settings.maximized )
    {
        gtk_window_maximize( GTK_WINDOW( main_window ) );
    }
    gtk_widget_show ( GTK_WIDGET( main_window ) );
    return main_window;
}
/*
void check_icon_theme()
{
    GtkSettings * settings;
    char* theme;
    const char* title = N_( "GTK+ icon theme is not properly set" );
    const char* error_msg =
        N_( "<big><b>%s</b></big>\n\n"
            "This usually means you don't have an XSETTINGS manager running.  "
            "Desktop environment like GNOME or XFCE automatically execute their "
            "XSETTING managers like gnome-settings-daemon or xfce-mcs-manager.\n\n"
            "<b>If you don't use these desktop environments, "
            "you have two choices:\n"
            "1. run an XSETTINGS manager, or\n"
            "2. simply specify an icon theme in ~/.gtkrc-2.0.</b>\n"
            "For example to use the Tango icon theme add a line:\n"
            "<i><b>gtk-icon-theme-name=\"Tango\"</b></i> in your ~/.gtkrc-2.0. (create it if no such file)\n\n"
            "<b>NOTICE: The icon theme you choose should be compatible with GNOME, "
            "or the file icons cannot be displayed correctly.</b>  "
            "Due to the differences in icon naming of GNOME and KDE, KDE themes cannot be used.  "
            "Currently there is no standard for this, but it will be solved by freedesktop.org in the future." );
    settings = gtk_settings_get_default();
    g_object_get( settings, "gtk-icon-theme-name", &theme, NULL );

    // No icon theme available
    if ( !theme || !*theme || 0 == strcmp( theme, "hicolor" ) )
    {
        GtkWidget * dlg;
        dlg = gtk_message_dialog_new_with_markup( NULL,
                                                  GTK_DIALOG_MODAL,
                                                  GTK_MESSAGE_ERROR,
                                                  GTK_BUTTONS_OK,
                                                  _( error_msg ), _( title ) );
        gtk_window_set_title( GTK_WINDOW( dlg ), _( title ) );
        gtk_dialog_run( GTK_DIALOG( dlg ) );
        gtk_widget_destroy( dlg );
    }
    g_free( theme );
}
*/
#ifdef _DEBUG_THREAD

G_LOCK_DEFINE(gdk_lock);
void debug_gdk_threads_enter (const char* message)
{
    g_debug( "Thread %p tries to get GDK lock: %s", g_thread_self (), message );
    G_LOCK(gdk_lock);
    g_debug( "Thread %p got GDK lock: %s", g_thread_self (), message );
}

static void _debug_gdk_threads_enter ()
{
    debug_gdk_threads_enter( "called from GTK+ internal" );
}

void debug_gdk_threads_leave( const char* message )
{
    g_debug( "Thread %p tries to release GDK lock: %s", g_thread_self (), message );
    G_LOCK(gdk_lock);
    g_debug( "Thread %p released GDK lock: %s", g_thread_self (), message );
}

static void _debug_gdk_threads_leave()
{
    debug_gdk_threads_leave( "called from GTK+ internal" );
}
#endif

void init_folder()
{
    if( G_LIKELY(folder_initialized) )
        return;

    app_settings.bookmarks = ptk_bookmarks_get();

    vfs_volume_init();
    vfs_thumbnail_init();

    vfs_mime_type_set_icon_size( app_settings.big_icon_size,
                                 app_settings.small_icon_size );
    vfs_file_info_set_thumbnail_size( app_settings.big_icon_size,
                                      app_settings.small_icon_size );

    //check_icon_theme();  //sfm seems to run okay without gtk theme
    folder_initialized = TRUE;
}

static void init_daemon_or_desktop()
{
    if( desktop )
        fm_turn_on_desktop_icons();
}

#ifdef HAVE_HAL

/* FIXME: Currently, this cannot be supported without HAL */

static int handle_mount( char** argv )
{
    gboolean success;
    vfs_volume_init();
    if( mount )
        success = vfs_volume_mount_by_udi( mount, NULL );
    else if( umount )
        success = vfs_volume_umount_by_udi( umount, NULL );
    else /* if( eject ) */
        success = vfs_volume_eject_by_udi( eject, NULL );
    vfs_volume_finalize();
    return success ? 0 : 1;
}
#endif

GList* get_file_info_list( char** file_paths )
{
    GList* file_list = NULL;
    char** file;
    VFSFileInfo* fi;

    for( file = file_paths; *file; ++file )
    {
        fi = vfs_file_info_new();
        if( vfs_file_info_get( fi, *file, NULL ) )
            file_list = g_list_append( file_list, fi );
        else
            vfs_file_info_unref( fi );
    }

    return file_list;
}

gboolean delayed_popup( GtkWidget* popup )
{
    GDK_THREADS_ENTER();

    gtk_menu_popup( GTK_MENU( popup ), NULL, NULL,
                    NULL, NULL, 0, gtk_get_current_event_time() );

    GDK_THREADS_LEAVE();

    return FALSE;
}

static void init_desktop_or_daemon()
{
    init_folder();

    signal( SIGPIPE, SIG_IGN );
    signal( SIGHUP, (void*)gtk_main_quit );
    signal( SIGINT, (void*)gtk_main_quit );
    signal( SIGTERM, (void*)gtk_main_quit );

    if( desktop )
        fm_turn_on_desktop_icons();
    desktop_or_deamon_initialized = TRUE;
}

char* dup_to_absolute_file_path(char** file)
{
    char* file_path, *real_path, *cwd_path;
    const size_t cwd_size = PATH_MAX;

    if( g_str_has_prefix( *file, "file:" ) ) /* It's a URI */
    {
        file_path = g_filename_from_uri( *file, NULL, NULL );
        g_free( *file );
        *file = file_path;
    }
    else
        file_path = *file;

    cwd_path = malloc( cwd_size );
    if( cwd_path )
    {
        getcwd( cwd_path, cwd_size );
    }

    real_path = vfs_file_resolve_path( cwd_path, file_path );
    free( cwd_path );
    cwd_path = NULL;

    return real_path; /* To free with g_free */
}

static void open_in_tab( FMMainWindow** main_window, const char* real_path )
{
    XSet* set;
    int p;
    // create main window if needed
    if( G_UNLIKELY( !*main_window ) )
    {
        // initialize things required by folder view
        if( G_UNLIKELY( ! daemon_mode ) )
            init_folder();

        // preload panel?
        if ( panel > 0 && panel < 5 )
            // user specified panel
            p = panel;
        else
        {
            // use first visible panel
            for ( p = 1; p < 5; p++ )
            {
                if ( xset_get_b_panel( p, "show" ) )
                    break;
            }
        }
        if ( p == 5 )
            p = 1;  // no panels were visible (unlikely)

        // set panel to load real_path on window creation
        set = xset_get_panel( p, "show" );
        set->ob1 = g_strdup( real_path );
        set->b = XSET_B_TRUE;

        // create new window
        *main_window = create_main_window();
    }
    else
    {
        // existing window
        gboolean tab_added = FALSE;
        if ( panel > 0 && panel < 5 )
        {
            // change to user-specified panel
            if ( !gtk_notebook_get_n_pages(
                            GTK_NOTEBOOK( (*main_window)->panel[panel-1] ) ) )
            {
                // set panel to load real_path on panel load
                set = xset_get_panel( panel, "show" );
                set->ob1 = g_strdup( real_path );
                tab_added = TRUE;
                set->b = XSET_B_TRUE;
                show_panels_all_windows( NULL, *main_window );
            }
            else if ( !gtk_widget_get_visible( (*main_window)->panel[panel-1] ) )
            {
                // show panel
                set = xset_get_panel( panel, "show" );
                set->b = XSET_B_TRUE;
                show_panels_all_windows( NULL, *main_window );
            }
            (*main_window)->curpanel = panel;
            (*main_window)->notebook = (*main_window)->panel[panel-1];
        }
        if ( !tab_added )
        {
            if ( reuse_tab )
            {
                main_window_open_path_in_current_tab( *main_window,
                                                        real_path );
                reuse_tab = FALSE;
            }
            else
                fm_main_window_add_new_tab( *main_window, real_path );
        }
    }
    gtk_window_present( GTK_WINDOW( *main_window ) );
}

gboolean handle_parsed_commandline_args()
{
    FMMainWindow * main_window = NULL;
    char** file;
    gboolean ret = TRUE;
    XSet* set;
    int p;

    app_settings.load_saved_tabs = !no_tabs;
    
    // If no files are specified, open home dir by defualt.
    if( G_LIKELY( ! files ) )
    {
        files = default_files;
        //files[0] = (char*)g_get_home_dir();
    }

    // get the last active window on this desktop, if available
    if( new_tab || reuse_tab )
    {
        //main_window = fm_main_window_get_last_active();
        main_window = fm_main_window_get_on_current_desktop();
    }

    if ( desktop_pref )  //MOD
        show_pref = 3;

    if( show_pref > 0 ) /* show preferences dialog */
    {
        /* We should initialize desktop support here.
         * Otherwise, if the user turn on the desktop support
         * in the pref dialog, the newly loaded desktop will be uninitialized.
         */
        //init_desktop_or_daemon();
        fm_edit_preference( GTK_WINDOW( main_window ), show_pref - 1 );
        show_pref = 0;
    }
    else if( find_files ) /* find files */
    {
        init_folder();
        fm_find_files( (const char**)files );
        find_files = FALSE;
    }
#ifdef DESKTOP_INTEGRATION
    else if( set_wallpaper ) /* change wallpaper */
    {
        set_wallpaper = FALSE;
        char* file = files ? files[0] : NULL;
        char* path;
        if( ! file )
            return FALSE;

        if( g_str_has_prefix( file, "file:" ) )  /* URI */
        {
            path = g_filename_from_uri( file, NULL, NULL );
            g_free( file );
            file = path;
        }
        else
            file = g_strdup( file );

        if( g_file_test( file, G_FILE_TEST_IS_REGULAR ) )
        {
            g_free( app_settings.wallpaper );
            app_settings.wallpaper = file;
            app_settings.show_wallpaper = TRUE;
            if ( xset_autosave_timer )
            {
                g_source_remove( xset_autosave_timer );
                xset_autosave_timer = 0;
            }
            char* err_msg = save_settings( NULL );
            if ( err_msg )
                printf( _("spacefm: Error: Unable to save session\n       %s\n"), err_msg );
            if( desktop && app_settings.show_wallpaper )
            {
                if( desktop_or_deamon_initialized )
                    fm_desktop_update_wallpaper();
            }
        }
        else
            g_free( file );

        ret = ( daemon_mode || ( desktop && desktop_or_deamon_initialized)  );
        goto out;
    }
#endif
    else /* open files/folders */
    {
        if( (daemon_mode || desktop) && ! desktop_or_deamon_initialized )
        {
            init_desktop_or_daemon();
        }
        else if ( files != default_files )
        {
            /* open files passed in command line arguments */
            ret = FALSE;
            for( file = files; *file; ++file )
            {
                char *real_path;

                if( ! **file )  /* skip empty string */
                    continue;

                real_path = dup_to_absolute_file_path( file );

                if( g_file_test( real_path, G_FILE_TEST_IS_DIR ) )
                {
                    open_in_tab( &main_window, real_path );
                    ret = TRUE;
                }
                else if ( g_file_test( real_path, G_FILE_TEST_EXISTS ) )
                    open_file( real_path );
                else if ( ( *file[0] != '/' && strstr( *file, ":/" ) )
                                        || g_str_has_prefix( *file, "//" ) )
                {
                    if ( main_window )
                        main_window_open_network( main_window, *file, TRUE );
                    else
                    {
                        open_in_tab( &main_window, "/" );
                        main_window_open_network( main_window, *file, FALSE );
                    }
                    ret = TRUE;
                    gtk_window_present( GTK_WINDOW( main_window ) );
                }    
                else
                {
                    char* err_msg = g_strdup_printf( "%s:\n\n%s", _( "File doesn't exist" ),
                                                        real_path );
                    ptk_show_error( NULL, _("Error"), err_msg );
                    g_free( err_msg );
                }
                g_free( real_path );
            }
        }
        else
        {
            // no files specified, just create window with default tabs
            if( G_UNLIKELY( ! main_window ) )
            {
                // initialize things required by folder view
                if( G_UNLIKELY( ! daemon_mode ) )
                    init_folder();
                main_window = create_main_window();
            }
            else
                gtk_window_present( GTK_WINDOW( main_window ) );
            if ( panel > 0 && panel < 5 )
            {
                // user specified a panel with no file, let's show the panel
                if ( !gtk_widget_get_visible( main_window->panel[panel-1] ) )
                {
                    // show panel
                    set = xset_get_panel( panel, "show" );
                    set->b = XSET_B_TRUE;
                    show_panels_all_windows( NULL, main_window );
                }
                focus_panel( NULL, (gpointer)main_window, 2 );
            }
        }
    }

out:
    if( files != default_files )
        g_strfreev( files );

    files = NULL;
    return ret;
}

void tmp_clean()
{
    char* cmd = g_strdup_printf( "rm -rf %s", xset_get_user_tmp_dir() );
    g_spawn_command_line_async( cmd, NULL );
    g_free( cmd );
}

int main ( int argc, char *argv[] )
{
    gboolean run = FALSE;
    GError* err = NULL;
    
#ifdef ENABLE_NLS
    bindtextdomain ( GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR );
    bind_textdomain_codeset ( GETTEXT_PACKAGE, "UTF-8" );
    textdomain ( GETTEXT_PACKAGE );
#endif

    // separate instance options
    if ( argc > 1 )
    {
        // dialog mode?
        if ( !strcmp( argv[1], "-g" ) || !strcmp( argv[1], "--dialog" ) )
        {
            g_thread_init( NULL );
            gdk_threads_init ();
            /* initialize the file alteration monitor */
            if( G_UNLIKELY( ! vfs_file_monitor_init() ) )
            {
#ifdef USE_INOTIFY
                ptk_show_error( NULL, _("Error"), _("Error: Unable to initialize inotify file change monitor.\n\nDo you have an inotify-capable kernel?") );
#else
                ptk_show_error( NULL, _("Error"), _("Error: Unable to establish connection with FAM.\n\nDo you have \"FAM\" or \"Gamin\" installed and running?") );
#endif
                vfs_file_monitor_clean();
                return 1;
            }
            gtk_init (&argc, &argv);
            int ret = custom_dialog_init( argc, argv );
            if ( ret != 0 )
            {
                vfs_file_monitor_clean();
                return ret == -1 ? 0 : ret;
            }
            gtk_main();
            vfs_file_monitor_clean();
            return 0;
        }

        // socket_command?
        if ( !strcmp( argv[1], "-s" ) || !strcmp( argv[1], "--socket-cmd" ) )
        {
#ifdef ENABLE_NLS
            // initialize gettext since gtk_init is not run here
            setlocale( LC_ALL, "" );
            bindtextdomain( GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR );
            textdomain( GETTEXT_PACKAGE );
#endif
            if ( argv[2] && ( !strcmp( argv[2], "help" ) || 
                                                !strcmp( argv[2], "--help" ) ) )
            {
                show_socket_help();
                return 0;
            }
            char* reply = NULL;
            int ret = send_socket_command( argc, argv, &reply );
            if ( reply && reply[0] )
                fprintf( ret ? stderr : stdout, "%s", reply );
            g_free( reply );
            return ret;
        }
    }

    /* initialize GTK+ and parse the command line arguments */
    if( G_UNLIKELY( ! gtk_init_with_args( &argc, &argv, "", opt_entries, GETTEXT_PACKAGE, &err ) ) )
    {
        printf( "spacefm: %s\n", err->message );
        g_error_free( err );
        return 1;
    }
    
    // dialog mode with other options?
    if ( custom_dialog )
    {
        fprintf( stderr, "spacefm: %s\n", _("--dialog must be first option") );
        return 1;
    }
    
    // socket command with other options?
    if ( socket_cmd )
    {
        fprintf( stderr, "spacefm: %s\n", _("--socket-cmd must be first option") );
        return 1;
    }
    
    // --version
    if ( version_opt )
    {
        printf( "spacefm %s\n", VERSION );
#if GTK_CHECK_VERSION (3, 0, 0)
        printf( "GTK3 " );
#else
        printf( "GTK2 " );
#endif
#ifdef HAVE_HAL
        printf( "HAL " );
#else
        printf( "UDEV " );
#endif
#ifdef USE_INOTIFY
        printf( "INOTIFY " );
#else
        printf( "FAM " );
#endif
#ifdef DESKTOP_INTEGRATION
        printf( "DESKTOP " );
#endif
#ifdef HAVE_SN
        printf( "SNOTIFY " );
#endif
        printf( "\n" );
        return 0;
    }
    
    /* Initialize multithreading  //sfm moved below parse arguments
         No matter we use threads or not, it's safer to initialize this earlier. */
#ifdef _DEBUG_THREAD
    gdk_threads_set_lock_functions(_debug_gdk_threads_enter, _debug_gdk_threads_leave);
#endif
    g_thread_init( NULL );
    gdk_threads_init ();

#if HAVE_HAL
    /* If the user wants to mount/umount/eject a device */
    if( G_UNLIKELY( mount || umount || eject ) )
        return handle_mount( argv );
#endif

    /* ensure that there is only one instance of spacefm.
         if there is an existing instance, command line arguments
         will be passed to the existing instance, and exit() will be called here.  */
    single_instance_check();

    /* initialize the file alteration monitor */
    if( G_UNLIKELY( ! vfs_file_monitor_init() ) )
    {
#ifdef USE_INOTIFY
        ptk_show_error( NULL, _("Error"), _("Error: Unable to initialize inotify file change monitor.\n\nDo you have an inotify-capable kernel?") );
#else
        ptk_show_error( NULL, _("Error"), _("Error: Unable to establish connection with FAM.\n\nDo you have \"FAM\" or \"Gamin\" installed and running?") );
#endif
        vfs_file_monitor_clean();
        //free_settings();
        return 1;
    }

    /* check if the filename encoding is UTF-8 */
    vfs_file_info_set_utf8_filename( g_get_filename_charsets( NULL ) );

    /* Initialize our mime-type system */
    vfs_mime_type_init();

    load_settings( config_dir );    /* load config file */  //MOD was before vfs_file_monitor_init

    app_settings.sdebug = sdebug;
    
/*
    // temporarily turn off desktop if needed
    if( G_LIKELY( no_desktop ) )
    {
        // No matter what the value of show_desktop is, we don't showdesktop icons
        // if --no-desktop argument is passed by the users.
        old_show_desktop = app_settings.show_desktop;
        // This config value will be restored before saving config files, if needed.
        app_settings.show_desktop = FALSE;
    }
*/
    /* If we reach this point, we are the first instance.
     * Subsequent processes will exit() inside single_instance_check and won't reach here.
     */

    main_window_event( NULL, NULL, "evt_start", 0, 0, NULL, 0, 0, 0, FALSE );

    /* handle the parsed result of command line args */
    run = handle_parsed_commandline_args();
    app_settings.load_saved_tabs = TRUE;
 
    if( run )   /* run the main loop */
        gtk_main();

    main_window_event( NULL, NULL, "evt_exit", 0, 0, NULL, 0, 0, 0, FALSE );

    single_instance_finalize();

    if( desktop && desktop_or_deamon_initialized )  // desktop was app_settings.show_desktop
        fm_turn_off_desktop_icons();

/*
    if( no_desktop )    // desktop icons is temporarily supressed
    {
        if( old_show_desktop )  // restore original settings
        {
            old_show_desktop = app_settings.show_desktop;
            app_settings.show_desktop = TRUE;
        }
    }
*/

/*
    if( run && xset_get_b( "main_save_exit" ) )
    {
        char* err_msg = save_settings();
        if ( err_msg )
            printf( "spacefm: Error: Unable to save session\n       %s\n", err_msg );
    }
*/
    vfs_volume_finalize();
    vfs_mime_type_clean();
    vfs_file_monitor_clean();
    tmp_clean();
    free_settings();

    return 0;
}

void open_file( const char* path )
{
    GError * err;
    char *msg, *error_msg;
    VFSFileInfo* file;
    VFSMimeType* mime_type;
    gboolean opened;
    char* app_name;

    file = vfs_file_info_new();
    vfs_file_info_get( file, path, NULL );
    mime_type = vfs_file_info_get_mime_type( file );
    opened = FALSE;
    err = NULL;

    app_name = vfs_mime_type_get_default_action( mime_type );
    if ( app_name )
    {
        opened = vfs_file_info_open_file( file, path, &err );
        g_free( app_name );
    }
    else
    {
        VFSAppDesktop* app;
        GList* files;

        app_name = (char *) ptk_choose_app_for_mime_type( NULL, mime_type, FALSE );
        if ( app_name )
        {
            app = vfs_app_desktop_new( app_name );
            if ( ! vfs_app_desktop_get_exec( app ) )
                app->exec = g_strdup( app_name ); /* This is a command line */
            files = g_list_prepend( NULL, (gpointer) path );
            opened = vfs_app_desktop_open_files( gdk_screen_get_default(),
                                                 NULL, app, files, &err );
            g_free( files->data );
            g_list_free( files );
            vfs_app_desktop_unref( app );
            g_free( app_name );
        }
        else
            opened = TRUE;
    }

    if ( !opened )
    {
        char * disp_path;
        if ( err && err->message )
        {
            error_msg = err->message;
        }
        else
            error_msg = _( "Don't know how to open the file" );
        disp_path = g_filename_display_name( path );
        msg = g_strdup_printf( _( "Unable to open file:\n\"%s\"\n%s" ), disp_path, error_msg );
        g_free( disp_path );
        ptk_show_error( NULL, _("Error"), msg );
        g_free( msg );
        if ( err )
            g_error_free( err );
    }
    vfs_mime_type_unref( mime_type );
    vfs_file_info_unref( file );
}

/* After opening any window/dialog/tool, this should be called. */
void pcmanfm_ref()
{
    ++n_pcmanfm_ref;
}

/* After closing any window/dialog/tool, this should be called.
 * If the last window is closed and we are not a deamon, pcmanfm will quit.
 */
gboolean pcmanfm_unref()
{
    --n_pcmanfm_ref;
    if( 0 == n_pcmanfm_ref && ! daemon_mode && !desktop )
        gtk_main_quit();
    return FALSE;
}
