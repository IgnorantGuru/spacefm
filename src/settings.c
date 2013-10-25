/*
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // euidaccess
#endif

#include "settings.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include "glib-utils.h" /* for g_mkdir_with_parents() */
#include <glib/gi18n.h>

#include <gtk/gtk.h>
#include "gtk2-compat.h"

#include "desktop.h"
#include <gdk/gdkkeysyms.h>
#include "ptk-utils.h"
#include <errno.h>
#include <fcntl.h>
#include "main-window.h"
#include "vfs-app-desktop.h"
#include "item-prop.h"
#include "ptk-app-chooser.h"
#include "vfs-utils.h" /* for vfs_load_icon */
#include "ptk-location-view.h"


/* Dirty hack: check whether we are under LXDE or not */
#define is_under_LXDE()     (g_getenv( "_LXSESSION_PID" ) != NULL)

AppSettings app_settings = {0};
/* const gboolean singleInstance_default = TRUE; */
const gboolean show_hidden_files_default = FALSE;
const gboolean show_side_pane_default = TRUE;
const int side_pane_mode_default = PTK_FB_SIDE_PANE_BOOKMARKS;
const gboolean show_thumbnail_default = FALSE;
const int max_thumb_size_default = 4 << 20;
const int big_icon_size_default = 48;
const int small_icon_size_default = 22;
const int tool_icon_size_default = 0;
const gboolean single_click_default = FALSE;
const gboolean show_location_bar_default = TRUE;

/* FIXME: temporarily disable trash since it's not finished */
const gboolean use_trash_can_default = FALSE;
//const int open_bookmark_method_default = 1;
const int view_mode_default = PTK_FB_ICON_VIEW;
const int sort_order_default = PTK_FB_SORT_BY_NAME;
const int sort_type_default = GTK_SORT_ASCENDING;

//gboolean show_desktop_default = FALSE;
const gboolean show_wallpaper_default = FALSE;
const WallpaperMode wallpaper_mode_default=WPM_STRETCH;
const GdkColor desktop_bg1_default={0, 4656, 4125, 12014};
const GdkColor desktop_bg2_default={0};   // unused?
const GdkColor desktop_text_default={0, 65535, 65535, 65535};
const GdkColor desktop_shadow_default={0, 11822, 13364, 13878};
const int desktop_sort_by_default = DW_SORT_CUSTOM;
const int desktop_sort_type_default = GTK_SORT_ASCENDING;
const gboolean show_wm_menu_default = FALSE;
const gboolean desk_single_click_default = FALSE;
const gboolean desk_open_mime_default = FALSE;
const int margin_top_default = 12;
const int margin_left_default = 6;
const int margin_right_default = 6;
const int margin_bottom_default = 12;
const int margin_pad_default = 6;

/* Default values of interface settings */
const gboolean always_show_tabs_default = TRUE;
const gboolean hide_close_tab_buttons_default = FALSE;
const gboolean hide_side_pane_buttons_default = FALSE;
//const gboolean hide_folder_content_border_default = FALSE;

// MOD settings
void xset_write( FILE* file );
void xset_parse( char* line );
void read_root_settings();
void xset_defaults();
const gboolean use_si_prefix_default = FALSE;
GList* xsets = NULL;
GList* keysets = NULL;
XSet* set_clipboard = NULL;
gboolean clipboard_is_cut;
XSet* set_last;
char* settings_config_dir = NULL;
char* settings_tmp_dir = NULL;
char* settings_shared_tmp_dir = NULL;
char* settings_user_tmp_dir = NULL;
XSetContext* xset_context = NULL;

// delayed session saving
guint xset_autosave_timer = 0;
gboolean xset_autosave_request = FALSE;

typedef void ( *SettingsParseFunc ) ( char* line );

static void color_from_str( GdkColor* ret, const char* value );
static void save_color( FILE* file, const char* name,
                 GdkColor* color );
void xset_free_all();
void xset_custom_delete( XSet* set, gboolean delete_next );
void xset_default_keys();
char* clean_label( const char* menu_label, gboolean kill_special, gboolean convert_amp );
char* xset_color_dialog( GtkWidget* parent, char* title, char* defcolor );
GtkWidget* xset_design_additem( GtkWidget* menu, char* label, gchar* stock_icon,
                                                            int job, XSet* set );
gboolean xset_design_cb( GtkWidget* item, GdkEventButton * event, XSet* set );
gboolean on_autosave_timer( gpointer main_window );
const char* icon_stock_to_id( const char* name );
void xset_custom_activate( GtkWidget* item, XSet* set );

const char* user_manual_url = "http://ignorantguru.github.io/spacefm/spacefm-manual-en.html";
const char* homepage = "http://ignorantguru.github.io/spacefm/"; //also in aboutdlg.ui

const char* enter_command_line = N_("Enter program or bash command line:\n\nUse:\n\t%%F\tselected files  or  %%f first selected file\n\t%%N\tselected filenames  or  %%n first selected filename\n\t%%d\tcurrent directory\n\t%%v\tselected device (eg /dev/sda1)\n\t%%m\tdevice mount point (eg /media/dvd);  %%l device label\n\t%%b\tselected bookmark\n\t%%t\tselected task directory;  %%p task pid\n\t%%a\tmenu item value\n\t$fm_panel, $fm_tab, $fm_command, etc");

const char* icon_desc = N_("Enter an icon name, icon file path, or stock item name:\n\nNot all icons may work due to various issues.");

const char* enter_menu_name = N_("Enter menu item name:\n\nPrecede a character with an underscore (_) to underline that character as a shortcut key if desired.");
const char* enter_menu_name_new = N_("Enter new menu item name:\n\nPrecede a character with an underscore (_) to underline that character as a shortcut key if desired.\n\nTIP: To change this menu item later, right-click on the menu item to open the design menu.");


static void parse_general_settings( char* line )
{
    char * sep = strstr( line, "=" );
    char* name;
    char* value;
    if ( !sep )
        return ;
    name = line;
    value = sep + 1;
    *sep = '\0';
    if ( 0 == strcmp( name, "encoding" ) )
        strcpy( app_settings.encoding, value );
    //else if ( 0 == strcmp( name, "show_hidden_files" ) )
    //    app_settings.show_hidden_files = atoi( value );
    //else if ( 0 == strcmp( name, "show_side_pane" ) )
    //    app_settings.show_side_pane = atoi( value );
    //else if ( 0 == strcmp( name, "side_pane_mode" ) )
    //    app_settings.side_pane_mode = atoi( value );
    else if ( 0 == strcmp( name, "show_thumbnail" ) )
        app_settings.show_thumbnail = atoi( value );
    else if ( 0 == strcmp( name, "max_thumb_size" ) )
        app_settings.max_thumb_size = atoi( value ) << 10;
    else if ( 0 == strcmp( name, "big_icon_size" ) )
    {
        app_settings.big_icon_size = atoi( value );
        if( app_settings.big_icon_size <= 0 || app_settings.big_icon_size > 128 )
            app_settings.big_icon_size = big_icon_size_default;
    }
    else if ( 0 == strcmp( name, "small_icon_size" ) )
    {
        app_settings.small_icon_size = atoi( value );
        if( app_settings.small_icon_size <= 0 || app_settings.small_icon_size > 128 )
            app_settings.small_icon_size = small_icon_size_default;
    }
    else if ( 0 == strcmp( name, "tool_icon_size" ) )
    {
        app_settings.tool_icon_size = atoi( value );
        if( app_settings.tool_icon_size < 0 || 
                            app_settings.tool_icon_size > GTK_ICON_SIZE_DIALOG )
            app_settings.tool_icon_size = tool_icon_size_default;
    }
    /* FIXME: temporarily disable trash since it's not finished */
#if 0
    else if ( 0 == strcmp( name, "use_trash_can" ) )
        app_settings.use_trash_can = atoi(value);
#endif
    else if ( 0 == strcmp( name, "single_click" ) )
        app_settings.single_click = atoi(value);
    //else if ( 0 == strcmp( name, "view_mode" ) )
    //    app_settings.view_mode = atoi( value );
    else if ( 0 == strcmp( name, "sort_order" ) )
        app_settings.sort_order = atoi( value );
    else if ( 0 == strcmp( name, "sort_type" ) )
        app_settings.sort_type = atoi( value );
    else if ( 0 == strcmp( name, "open_bookmark_method" ) )
        //app_settings.open_bookmark_method = atoi( value );
        xset_set_b( "book_newtab", atoi( value ) != 1 ); //sfm backwards compat
/*
    else if ( 0 == strcmp( name, "iconTheme" ) )
    {
        if ( value && *value )
            app_settings.iconTheme = strdup( value );
    }
*/
    else if ( 0 == strcmp( name, "terminal" ) ) //MOD backwards compat
    {
        if ( value && *value )
            xset_set( "main_terminal", "s", value );
            //app_settings.terminal = strdup( value );
    }
    else if ( 0 == strcmp( name, "use_si_prefix" ) )
        app_settings.use_si_prefix = atoi( value );
    else if ( 0 == strcmp( name, "no_execute" ) )
        app_settings.no_execute = atoi( value );  //MOD
    else if ( 0 == strcmp( name, "home_folder" ) )
    {
        // backwards compat
        if ( value && *value )
            xset_set( "go_set_default", "s", value );
    }
    else if ( 0 == strcmp( name, "no_confirm" ) )
        app_settings.no_confirm = atoi( value );  //MOD
    /*
    else if ( 0 == strcmp( name, "singleInstance" ) )
        app_settings.singleInstance = atoi( value );
    */
/*    else if ( 0 == strcmp( name, "show_location_bar" ) )
        app_settings.show_location_bar = atoi( value );
*/
}

static void color_from_str( GdkColor* ret, const char* value )
{
    sscanf( value, "%hu,%hu,%hu",
            &ret->red, &ret->green, &ret->blue );
}

static void save_color( FILE* file, const char* name, GdkColor* color )
{
    fprintf( file, "%s=%d,%d,%d\n", name,
             color->red, color->green, color->blue );
}

static void parse_window_state( char* line )
{
    char * sep = strstr( line, "=" );
    char* name;
    char* value;
    int v;
    if ( !sep )
        return ;
    name = line;
    value = sep + 1;
    *sep = '\0';
    //if ( 0 == strcmp( name, "splitter_pos" ) )
    //{
    //    v = atoi( value );
    //    app_settings.splitter_pos = ( v > 0 ? v : 160 );
    //}
    if ( 0 == strcmp( name, "width" ) )
    {
        v = atoi( value );
        app_settings.width = ( v > 0 ? v : 640 );
    }
    if ( 0 == strcmp( name, "height" ) )
    {
        v = atoi( value );
        app_settings.height = ( v > 0 ? v : 480 );
    }
    if ( 0 == strcmp( name, "maximized" ) )
    {
        app_settings.maximized = atoi( value );
    }
}

static void parse_desktop_settings( char* line )
{
    char * sep = strstr( line, "=" );
    char* name;
    char* value;
    if ( !sep )
        return ;
    name = line;
    value = sep + 1;
    *sep = '\0';
    //if ( 0 == strcmp( name, "show_desktop" ) )
    //    app_settings.show_desktop = atoi( value );
    if ( 0 == strcmp( name, "show_wallpaper" ) )
        app_settings.show_wallpaper = atoi( value );
    else if ( 0 == strcmp( name, "wallpaper" ) )
        app_settings.wallpaper = g_strdup( value );
    else if ( 0 == strcmp( name, "wallpaper_mode" ) )
        app_settings.wallpaper_mode = atoi( value );
    else if ( 0 == strcmp( name, "bg1" ) )
        color_from_str( &app_settings.desktop_bg1, value );
    else if ( 0 == strcmp( name, "bg2" ) )
        color_from_str( &app_settings.desktop_bg2, value );
    else if ( 0 == strcmp( name, "text" ) )
        color_from_str( &app_settings.desktop_text, value );
    else if ( 0 == strcmp( name, "shadow" ) )
        color_from_str( &app_settings.desktop_shadow, value );
    else if ( 0 == strcmp( name, "font" ) )
        app_settings.desk_font = pango_font_description_from_string( value );
    else if ( 0 == strcmp( name, "sort_by" ) )
        app_settings.desktop_sort_by = atoi( value );
    else if ( 0 == strcmp( name, "sort_type" ) )
        app_settings.desktop_sort_type = atoi( value );
    else if ( 0 == strcmp( name, "show_wm_menu" ) )
        app_settings.show_wm_menu = atoi( value );
    else if ( 0 == strcmp( name, "desk_single_click" ) )
        app_settings.desk_single_click = atoi( value );
    else if ( 0 == strcmp( name, "desk_open_mime" ) )
        app_settings.desk_open_mime = atoi( value );
    else if ( 0 == strcmp( name, "margin_top" ) )
        app_settings.margin_top = atoi( value );
    else if ( 0 == strcmp( name, "margin_left" ) )
        app_settings.margin_left = atoi( value );
    else if ( 0 == strcmp( name, "margin_right" ) )
        app_settings.margin_right = atoi( value );
    else if ( 0 == strcmp( name, "margin_bottom" ) )
        app_settings.margin_bottom = atoi( value );
    else if ( 0 == strcmp( name, "margin_pad" ) )
        app_settings.margin_pad = atoi( value );
}

static void parse_interface_settings( char* line )
{
    char * sep = strstr( line, "=" );
    char* name;
    char* value;
    if ( !sep )
        return ;
    name = line;
    value = sep + 1;
    *sep = '\0';
    if ( 0 == strcmp( name, "always_show_tabs" ) )
        app_settings.always_show_tabs = atoi( value );
    else if ( 0 == strcmp( name, "show_close_tab_buttons" ) )
        app_settings.hide_close_tab_buttons = !atoi( value );
    //else if ( 0 == strcmp( name, "hide_side_pane_buttons" ) )
    //    app_settings.hide_side_pane_buttons = atoi( value );
    //else if ( 0 == strcmp( name, "hide_folder_content_border" ) )
    //    app_settings.hide_folder_content_border = atoi( value );
}

static void parse_conf( char* line )
{
    char * sep = strstr( line, "=" );
    char* name;
    char* value;
    if ( !sep )
        return ;
    name = line;
    value = sep + 1;
    *sep = '\0';
    char* sname = g_strstrip( name );
    
    if ( !strcmp( sname, "tmp_dir" ) )
    {
        settings_tmp_dir = g_strdup( g_strstrip( value ) );
        if ( settings_tmp_dir && settings_tmp_dir[0] == '\0' )
        {
            g_free( settings_tmp_dir );
            settings_tmp_dir = NULL;
        }
        else if ( settings_tmp_dir && strpbrk( settings_tmp_dir, " $%\\()&#|:;?<>{}[]*\"'" ) )
        {
            g_free( settings_tmp_dir );
            settings_tmp_dir = NULL;
            g_warning( _("custom tmp_dir contains invalid chars - reverting to /tmp") );
        }
        else if ( settings_tmp_dir && !g_file_test( settings_tmp_dir, G_FILE_TEST_IS_DIR ) )
        {
            g_free( settings_tmp_dir );
            settings_tmp_dir = NULL;
            g_warning( _("custom tmp_dir does not exist - reverting to /tmp") );
        }
    }
}

void load_conf()
{
    // load spacefm.conf
    char line[ 2048 ];
    FILE* file = fopen( "/etc/spacefm/spacefm.conf", "r" );
    if ( file )
    {
        while ( fgets( line, sizeof( line ), file ) )
            parse_conf( line );
        fclose( file );
    }
    
    // set tmp dirs
    if ( !settings_tmp_dir )
        settings_tmp_dir = g_strdup( "/tmp" );
}        

void swap_menu_label( const char* set_name, const char* old_name,
                                                        const char* new_name )
{   // changes default menu label for older config files
    XSet* set;

    if ( set = xset_is( set_name ) )
    {
        if ( set->menu_label && !strcmp( set->menu_label, old_name ) )
        {
            // menu label has not been changed by user - change default
            g_free( set->menu_label );
            set->menu_label = g_strdup( new_name );
            set->in_terminal = XSET_B_UNSET;
        }
    }
}

void load_settings( char* config_dir )
{
    FILE * file;
    gchar* path = NULL;
    char line[ 2048 ];
    char* section_name;
    SettingsParseFunc func = NULL;
    XSet* set;
    char* str;
    
    xset_cmd_history = NULL;
    app_settings.load_saved_tabs = TRUE;
    if ( config_dir )
        settings_config_dir = config_dir;
    else
        settings_config_dir = g_build_filename( g_get_user_config_dir(), "spacefm", NULL );
    /* set default value */

    /* General */
    /* app_settings.show_desktop = show_desktop_default; */
    app_settings.show_wallpaper = show_wallpaper_default;
    app_settings.wallpaper = NULL;
    app_settings.desktop_bg1 = desktop_bg1_default;
    app_settings.desktop_bg2 = desktop_bg2_default;
    app_settings.desktop_text = desktop_text_default;
    app_settings.desk_font = NULL;
    app_settings.desktop_sort_by = desktop_sort_by_default;
    app_settings.desktop_sort_type = desktop_sort_type_default;
    app_settings.show_wm_menu = show_wm_menu_default;
    app_settings.desk_single_click = desk_single_click_default;
    app_settings.desk_open_mime = desk_open_mime_default;
    app_settings.margin_top = margin_top_default;
    app_settings.margin_left = margin_left_default;
    app_settings.margin_right = margin_right_default;
    app_settings.margin_bottom = margin_bottom_default;
    app_settings.margin_pad = margin_pad_default;
    
    app_settings.encoding[ 0 ] = '\0';
    //app_settings.show_hidden_files = show_hidden_files_default;
    //app_settings.show_side_pane = show_side_pane_default;
    //app_settings.side_pane_mode = side_pane_mode_default;
    app_settings.show_thumbnail = show_thumbnail_default;
    app_settings.max_thumb_size = max_thumb_size_default;
    app_settings.big_icon_size = big_icon_size_default;
    app_settings.small_icon_size = small_icon_size_default;
    app_settings.tool_icon_size = tool_icon_size_default;
    app_settings.use_trash_can = use_trash_can_default;
    //app_settings.view_mode = view_mode_default;
    //app_settings.open_bookmark_method = open_bookmark_method_default;
    /* app_settings.iconTheme = NULL; */
    //app_settings.terminal = NULL;
    app_settings.use_si_prefix = use_si_prefix_default;
    //app_settings.show_location_bar = show_location_bar_default;
    //app_settings.home_folder = NULL;   //MOD
    app_settings.no_execute = TRUE;   //MOD
    app_settings.no_confirm = FALSE;   //MOD
    app_settings.date_format = NULL;   //MOD
    
    /* Interface */
    app_settings.always_show_tabs = always_show_tabs_default;
    app_settings.hide_close_tab_buttons = hide_close_tab_buttons_default;
    //app_settings.hide_side_pane_buttons = hide_side_pane_buttons_default;
    //app_settings.hide_folder_content_border = hide_folder_content_border_default;

    /* Window State */
    //app_settings.splitter_pos = 160;
    app_settings.width = 640;
    app_settings.height = 480;

    // MOD extra settings
    xset_defaults();

    /* load settings */   //MOD
    /* Dirty hacks for LXDE */
    /*
    if( is_under_LXDE() )
    {
        show_desktop_default = app_settings.show_desktop = TRUE;   // show the desktop by default
    }
    */

    // set tmp dirs
    if ( !settings_tmp_dir )
        settings_tmp_dir = g_strdup( "/tmp" );
        
    // shared tmp
    settings_shared_tmp_dir = g_build_filename( settings_tmp_dir, "spacefm.tmp", NULL );
    if ( geteuid() == 0 )
    {
        if ( !g_file_test( settings_shared_tmp_dir, G_FILE_TEST_EXISTS ) )
            g_mkdir_with_parents( settings_shared_tmp_dir,
                                        S_IRWXU | S_IRWXG | S_IRWXO | S_ISVTX );
        chown( settings_shared_tmp_dir, 0, 0 );
        chmod( settings_shared_tmp_dir, S_IRWXU | S_IRWXG | S_IRWXO | S_ISVTX );
    }

    // copy /etc/xdg/spacefm
    if ( !g_file_test( settings_config_dir, G_FILE_TEST_EXISTS ) 
                && g_file_test( "/etc/xdg/spacefm", G_FILE_TEST_IS_DIR ) )
    {
        char* command = g_strdup_printf( "cp -r /etc/xdg/spacefm '%s'",
                                                        settings_config_dir );
        printf( "COMMAND=%s\n", command );
        g_spawn_command_line_sync( command, NULL, NULL, NULL, NULL );
        g_free( command );
        chmod( settings_config_dir, S_IRWXU );
    }
    
    // load session
    int x = 0;
    do
    {
        if ( path )
            g_free ( path );
        path = NULL;
        switch ( x )
        {
            case 0:
                path = g_build_filename( settings_config_dir, "session", NULL );
                break;
            case 1:
                path = g_build_filename( settings_config_dir, "session-last", NULL );
                break;
            case 2:
                path = g_build_filename( settings_config_dir, "session-prior", NULL );
                break;
            case 3:
                path = g_build_filename( settings_config_dir, "main.lxde", NULL );
                break;
            case 4:
                path = g_build_filename( settings_config_dir, "main", NULL );
                break;
            case 5:
                path = g_build_filename( g_get_user_config_dir(), "pcmanfm",
                                                                "main.lxde", NULL );
                break;
            case 6:
                path = g_build_filename( g_get_user_config_dir(), "pcmanfm",
                                                                "main", NULL );
                break;
            default:
                path = NULL;
        }
        x++;
    } while ( path && !g_file_test( path, G_FILE_TEST_EXISTS ) );
    
    if ( x == 1 )
    {
        // copy session to session-last
        char* last = g_build_filename( settings_config_dir, "session-last", NULL );
        char* prior = g_build_filename( settings_config_dir, "session-prior", NULL );
        if ( g_file_test( last, G_FILE_TEST_EXISTS ) )
        {
            unlink( prior );
            rename( last, prior );
        }
        xset_copy_file( path, last );
        chmod( last, S_IRUSR | S_IWUSR );
        g_free( last );
        g_free( prior );
    }
    
    if ( path )
    {
        file = fopen( path, "r" );
        g_free( path );
    }
    else
        file = NULL;
    if ( file )
    {
        while ( fgets( line, sizeof( line ), file ) )
        {
            strtok( line, "\r\n" );
            if ( ! line[ 0 ] )
                continue;
            if ( line[ 0 ] == '[' )
            {
                section_name = strtok( line, "]" );
                if ( 0 == strcmp( line + 1, "General" ) )
                    func = &parse_general_settings;
                else if ( 0 == strcmp( line + 1, "Window" ) )
                    func = &parse_window_state;
                else if ( 0 == strcmp( line + 1, "Interface" ) )
                    func = &parse_interface_settings;
                else if ( 0 == strcmp( line + 1, "Desktop" ) )
                    func = &parse_desktop_settings;
                else if ( 0 == strcmp( line + 1, "MOD" ) )  //MOD
                    func = &xset_parse;
                else
                    func = NULL;
                continue;
            }
            if ( func )
                ( *func ) ( line );
        }
        fclose( file );
    }

    if ( app_settings.encoding[ 0 ] )
    {
        setenv( "G_FILENAME_ENCODING", app_settings.encoding, 1 );
    }

    //sfm margin limits
    if ( app_settings.margin_top < 0 || app_settings.margin_top > 999 )
        app_settings.margin_top = margin_top_default;
    if ( app_settings.margin_left < 0 || app_settings.margin_left > 999 )
        app_settings.margin_left = margin_left_default;
    if ( app_settings.margin_right < 0 || app_settings.margin_right > 999 )
        app_settings.margin_right = margin_right_default;
    if ( app_settings.margin_bottom < 0 || app_settings.margin_bottom > 999 )
        app_settings.margin_bottom = margin_bottom_default;
    if ( app_settings.margin_pad < 0 || app_settings.margin_pad > 999 )
        app_settings.margin_pad = margin_pad_default;

    //MOD turn off fullscreen
    xset_set_b( "main_full", FALSE );
    
    //MOD date_format
    app_settings.date_format = g_strdup( xset_get_s( "date_format" ) );
    if ( !app_settings.date_format || app_settings.date_format[0] == '\0' )
    {
        if ( app_settings.date_format )
            g_free( app_settings.date_format );
        app_settings.date_format = g_strdup_printf( "%%Y-%%m-%%d %%H:%%M" );
        xset_set( "date_format", "s", "%Y-%m-%d %H:%M" );
    }
    
    //MOD su and gsu command discovery (sets default)
    char* set_su = get_valid_su();
    if ( set_su )
        g_free( set_su );
    set_su = get_valid_gsu();
    if ( set_su )
        g_free( set_su );

    //MOD terminal discovery
    int i;
    char* term;
    char* terminal = xset_get_s( "main_terminal" );
    if ( !terminal || terminal[0] == '\0' )
    {
        for ( i = 0; i < G_N_ELEMENTS( terminal_programs ); i++ )
        {
            if ( term = g_find_program_in_path( terminal_programs[i] ) )
            {
                xset_set( "main_terminal", "s", terminal_programs[i] );
                xset_set_b( "main_terminal", TRUE );  // discovery
                g_free( term );
                break;
            }
        }
    }

    //MOD editor discovery
    char* app_name = xset_get_s( "editor" );
    if ( !app_name || app_name[0] == '\0' )
    {
        VFSMimeType* mime_type = vfs_mime_type_get_from_type( "text/plain" );
        if ( mime_type )
        {
            app_name = vfs_mime_type_get_default_action( mime_type );
            vfs_mime_type_unref( mime_type );
            //int app_len = strlen( app_name );
            //if ( app_len > 8 && !strcmp( app_name + app_len - 8, ".desktop" ) )
            if ( app_name )
            {
                VFSAppDesktop* app = vfs_app_desktop_new( app_name );
                if ( app )
                {
                    if ( app->exec )
                        xset_set( "editor", "s", app->exec );
                    vfs_app_desktop_unref( app );
                }
            }
        }
    }
    
    // get root-protected settings
    read_root_settings();

    // set default keys
    xset_default_keys();
    
    /* Load bookmarks */
    /* Don't load bookmarks here since we won't use it in some cases */
    /* app_settings.bookmarks = ptk_bookmarks_get(); */
    
    // cache event handlers
    evt_win_focus = xset_get( "evt_win_focus" );
    evt_win_move = xset_get( "evt_win_move" );
    evt_win_click = xset_get( "evt_win_click" );
    evt_win_key = xset_get( "evt_win_key" );
    evt_win_close = xset_get( "evt_win_close" );
    evt_pnl_show = xset_get( "evt_pnl_show" );
    evt_pnl_focus = xset_get( "evt_pnl_focus" );
    evt_pnl_sel = xset_get( "evt_pnl_sel" );
    evt_tab_new = xset_get( "evt_tab_new" );
    evt_tab_focus = xset_get( "evt_tab_focus" );
    evt_tab_close = xset_get( "evt_tab_close" );
    evt_device = xset_get( "evt_device" );

    // config conversions
    int ver = xset_get_int( "config_version", "s" );
    if ( ver == 0 )
        return;
    if ( ver < 3 ) // < 0.5.3
    {
        set = xset_get( "toolbar_left" );
        if ( set->menu_label && !strcmp( set->menu_label, "_Left" ) )
        {
            g_free( set->menu_label );
            set->menu_label = g_strdup( "_Left Toolbar" );
            set->in_terminal = XSET_B_UNSET;
        }
        set = xset_get( "toolbar_right" );
        if ( set->menu_label && !strcmp( set->menu_label, "_Right" ) )
        {
            g_free( set->menu_label );
            set->menu_label = g_strdup( "_Right Toolbar" );
            set->in_terminal = XSET_B_UNSET;
        }
        set = xset_get( "toolbar_side" );
        if ( set->menu_label && !strcmp( set->menu_label, "_Side" ) )
        {
            g_free( set->menu_label );
            set->menu_label = g_strdup( "_Side Toolbar" );
            set->in_terminal = XSET_B_UNSET;
        }
        for ( i = 1; i < 5; i++ )
        {
            set = xset_get_panel( i, "show_sidebar" );
            if ( set->menu_label && !strcmp( set->menu_label, "_Sidebar" ) )
            {
                g_free( set->menu_label );
                set->menu_label = g_strdup( "_Side Toolbar" );
                set->in_terminal = XSET_B_UNSET;
            }
        }
        set = xset_get( "focus_path_bar" );
        if ( set->menu_label && !strcmp( set->menu_label, "_Smart Bar" ) )
        {
            g_free( set->menu_label );
            set->menu_label = g_strdup( "_Smartbar" );
            set->in_terminal = XSET_B_UNSET;
        }
    }
    if ( ver < 4 ) // < 0.5.4
    {
        set = xset_get( "task_err_first" );
        if ( set->menu_label && !strcmp( set->menu_label, "Stop On _First" ) )
        {
            g_free( set->menu_label );
            set->menu_label = g_strdup( "Stop If _First" );
            set->in_terminal = XSET_B_UNSET;
        }
    }
    if ( ver < 6 ) // < 0.6.3
    {
        set = xset_get( "dev_show_internal_drives" );
        if ( set->menu_label && !strcmp( set->menu_label, "Show _Internal Drives" ) )
        {
            g_free( set->menu_label );
            set->menu_label = g_strdup( "_Internal Drives" );
            set->in_terminal = XSET_B_UNSET;
        }
        set = xset_get( "dev_show_partition_tables" );
        if ( set->menu_label && !strcmp( set->menu_label, "Show _Partition Tables" ) )
        {
            g_free( set->menu_label );
            set->menu_label = g_strdup( "_Partition Tables" );
            set->in_terminal = XSET_B_UNSET;
        }
        set = xset_get( "dev_ignore_udisks_hide" );
        if ( set->menu_label && !strcmp( set->menu_label, "Ignore Udisks _Hide Policy" ) )
        {
            g_free( set->menu_label );
            set->menu_label = g_strdup( "Ignore _Hide Policy" );
            set->in_terminal = XSET_B_UNSET;
        }
        set = xset_get( "dev_show_hide_volumes" );
        if ( set->menu_label && !strcmp( set->menu_label, "Show _Volumes..." ) )
        {
            g_free( set->menu_label );
            set->menu_label = g_strdup( "_Volumes..." );
            set->in_terminal = XSET_B_UNSET;
        }
        set = xset_get( "dev_show_empty" );  //new
        if ( set->b == XSET_B_UNSET )
            set->b = XSET_B_TRUE;
    }
    if ( ver < 7 ) // < 0.7.0
    {
        // custom separators ->next xset have invalid prev
        XSet* set_next;
        GList* l;

        for ( l = xsets; l; l = l->next )
        {
            set = l->data;
            if ( !set->lock && set->menu_style == XSET_MENU_SEP && set->next )
            {
                set_next = xset_get( set->next );
                if ( set_next->prev )
                    g_free( set_next->prev );
                set_next->prev = g_strdup( set->name );
            }
        }
    }
    if ( ver < 8 ) // < 0.7.2
    {
        if ( app_settings.small_icon_size == 20 )
            app_settings.small_icon_size = 22;
        if ( app_settings.big_icon_size == 20 )
            app_settings.big_icon_size = 22;
    }
    if ( ver < 9 ) // < 0.7.3
    {
        for ( i = 1; i < 5; i++ )
        {
            set = xset_get_panel( i, "show_toolbox" );
            if ( set->menu_label && !strcmp( set->menu_label, "_Toolbox" ) )
            {
                g_free( set->menu_label );
                set->menu_label = g_strdup( "_Toolbar" );
                set->in_terminal = XSET_B_UNSET;
            }
        }
        set = xset_get( "focus_path_bar" );
        if ( set->menu_label && !strcmp( set->menu_label, "_Smartbar" ) )
        {
            g_free( set->menu_label );
            set->menu_label = g_strdup( "_Path Bar" );
            set->in_terminal = XSET_B_UNSET;
        }
        set = xset_get( "path_help" );
        if ( set->menu_label && !strcmp( set->menu_label, "_Smartbar Help" ) )
        {
            g_free( set->menu_label );
            set->menu_label = g_strdup( "_Path Bar Help" );
            set->in_terminal = XSET_B_UNSET;
        }
    }
    if ( ver < 10 ) // < 0.7.5
    {
        set = xset_get( "dev_ignore_udisks_hide" );
        if ( set->menu_label && !strcmp( set->menu_label, "Ignore Udisks _Hide" ) )
        {
            g_free( set->menu_label );
            set->menu_label = g_strdup( _("Ignore _Hide Policy") );
            set->in_terminal = XSET_B_UNSET;
        }
        set = xset_get( "dev_ignore_udisks_nopolicy" );
        if ( set->menu_label && !strcmp( set->menu_label, "Ignore Udisks _No Policy" ) )
        {
            g_free( set->menu_label );
            set->menu_label = g_strdup( _("Ignore _No Policy") );
            set->in_terminal = XSET_B_UNSET;
        }
    }
    if ( ver < 11 ) // < 0.7.7+
    {
        set = xset_get( "main_faq" );
        if ( set->menu_label && !strcmp( set->menu_label, "How do I... (_FAQ)" ) )
        {
            g_free( set->menu_label );
            set->menu_label = g_strdup( _("_FAQ") );
            set->in_terminal = XSET_B_UNSET;
        }
    }
    if ( ver < 15 ) // < 0.8.1
    {
        set = xset_get( "task_stop" );
        if ( set->menu_label && !strcmp( set->menu_label, "_Stop Task" ) )
        {
            g_free( set->menu_label );
            set->menu_label = g_strdup( _("_Stop") );
            set->in_terminal = XSET_B_UNSET;
        }
        set = xset_get( "task_stop_all" );
        g_free( set->menu_label );
        set->menu_label = g_strdup( _("_Stop") );
        set->in_terminal = XSET_B_UNSET;

        set = xset_get( "task_show_manager" );
        if ( set->menu_label && !strcmp( set->menu_label, "_Show Manager" ) )
        {
            g_free( set->menu_label );
            set->menu_label = g_strdup( _("Show _Manager") );
            set->in_terminal = XSET_B_UNSET;
        }
        set = xset_get( "task_hide_manager" );
        if ( set->menu_label && !strcmp( set->menu_label, "_Auto-Hide Manager" ) )
        {
            g_free( set->menu_label );
            set->menu_label = g_strdup( _("Auto-_Hide Manager") );
            set->in_terminal = XSET_B_UNSET;
        }
        set = xset_get( "task_errors" );
        if ( set->menu_label && !strcmp( set->menu_label, "_Errors" ) )
        {
            g_free( set->menu_label );
            set->menu_label = g_strdup( _("Err_ors") );
            set->in_terminal = XSET_B_UNSET;
        }
        set = xset_get( "task_col_curest" );
        if ( set->menu_label && !strcmp( set->menu_label, "Current Esti_mate" ) )
        {
            g_free( set->menu_label );
            set->menu_label = g_strdup( _("Current Re_main") );
            set->in_terminal = XSET_B_UNSET;
        }
        set = xset_get( "task_col_avgest" );
        if ( set->menu_label && !strcmp( set->menu_label, "A_verage Estimate" ) )
        {
            g_free( set->menu_label );
            set->menu_label = g_strdup( _("A_verage Remain") );
            set->in_terminal = XSET_B_UNSET;
        }
        set = xset_get( "task_col_path" );
        if ( set->menu_label && !strcmp( set->menu_label, "_Path" ) )
        {
            g_free( set->menu_label );
            set->menu_label = g_strdup( _("_Folder") );
            set->in_terminal = XSET_B_UNSET;
        }
        set = xset_get( "dev_root_mount" );
        if ( !g_strcmp0( set->icon, "gtk-add" ) )
        {
            string_copy_free( &set->icon, "drive-removable-media" );
            set->keep_terminal = XSET_B_UNSET;
        }
        set = xset_get( "iso_mount" );
        if ( !g_strcmp0( set->icon, "gtk-cdrom" ) )
        {
            string_copy_free( &set->icon, "drive-removable-media" );
            set->keep_terminal = XSET_B_UNSET;
        }
        set = xset_get( "stool_mount" );
        if ( !g_strcmp0( set->icon, "gtk-add" ) )
        {
            string_copy_free( &set->icon, "drive-removable-media" );
            set->keep_terminal = XSET_B_UNSET;
        }
        set = xset_get( "dev_menu_mount" );
        if ( !g_strcmp0( set->icon, "gtk-add" ) )
        {
            string_copy_free( &set->icon, "drive-removable-media" );
            set->keep_terminal = XSET_B_UNSET;
        }
        set = xset_get( "task_pop_detail" );
        if ( !g_strcmp0( set->menu_label, "_Detailed Status" ) )
        {
            string_copy_free( &set->menu_label, _("_Detailed Stats") );
            set->in_terminal = XSET_B_UNSET;
        }
        
        if ( app_settings.small_icon_size == 20 )
            app_settings.small_icon_size = 22;
        if ( app_settings.big_icon_size == 20 )
            app_settings.big_icon_size = 22;
    }
    if ( ver < 16 ) // < 0.8.3
    {
        swap_menu_label( "dev_menu_remove", "Remo_ve", _("Remo_ve / Eject") );
    }
    if ( ver < 18 ) // < 0.8.7+
    {
        app_settings.desk_single_click = app_settings.single_click;
    }
    if ( ver < 23 ) // < 0.9.0
    {
        // Note: this is the last config version which should require menu_label
        //       changes of this nature due to defaults no longer being saved
        //       in session file
        swap_menu_label( "plug_copy", "_Copy", _("_Import") );
        swap_menu_label( "main_tasks", "_Tasks", _("_Task Manager") );

        // for rename dialog
        swap_menu_label( "move_filename", "_Filename", _("F_ilename") );
        swap_menu_label( "move_type", "_Type", _("Typ_e") );
        swap_menu_label( "move_target", "_Target", _("Ta_rget") );
        swap_menu_label( "move_template", "_Template", _("Te_mplate") );
        swap_menu_label( "move_dlg_help", "T_ips", _("_Help") );
    }
}


char* save_settings( gpointer main_window_ptr )
{
    FILE * file;
    gchar* path;
    int result, p, pages, g;
    char* err_msg = NULL;
    XSet* set;
    PtkFileBrowser* file_browser;
    char* tabs;
    char* old_tabs;
    FMMainWindow* main_window;
//printf("save_settings\n");

    xset_set( "config_version", "s", "23" );  // 0.9.0

    // save tabs
    gboolean save_tabs = xset_get_b( "main_save_tabs" );
    if ( main_window_ptr && save_tabs )
    {
        main_window = (FMMainWindow*)main_window_ptr;
        for ( p = 1; p < 5; p++ )
        {
            set = xset_get_panel( p, "show" );            
            pages = gtk_notebook_get_n_pages( GTK_NOTEBOOK( main_window->panel[p-1] ) );
            if ( pages )  // panel was shown
            {
                if ( set->s )
                {
                    g_free( set->s );
                    set->s = NULL;
                }
                tabs = g_strdup( "" );
                for ( g = 0; g < pages; g++ )
                {
                    file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                        GTK_NOTEBOOK( main_window->panel[p-1] ), g ) );
                    old_tabs = tabs;
                    tabs = g_strdup_printf( "%s///%s", old_tabs,
                                        ptk_file_browser_get_cwd( file_browser ) );
                    g_free( old_tabs );
                }
                if ( tabs[0] != '\0' )
                    set->s = tabs;
                else
                    g_free( tabs );

                // save current tab
                if ( set->x )
                    g_free( set->x );
                set->x = g_strdup_printf( "%d", gtk_notebook_get_current_page(
                                        GTK_NOTEBOOK( main_window->panel[p-1] ) ) );
            }
        }
    }
    else if ( !save_tabs )
    {
        // clear saved tabs
        for ( p = 1; p < 5; p++ )
        {
            set = xset_get_panel( p, "show" );            
            if ( set->s )
            {
                g_free( set->s );
                set->s = NULL;
            }
            if ( set->x )
            {
                g_free( set->x );
                set->x = NULL;
            }
        }
    }
    
    /* save settings */
    if ( ! g_file_test( settings_config_dir, G_FILE_TEST_EXISTS ) )
        g_mkdir_with_parents( settings_config_dir, 0700 );

    if ( ! g_file_test( settings_config_dir, G_FILE_TEST_EXISTS ) )
        goto _save_error;
        
    path = g_build_filename( settings_config_dir, "session.tmp", NULL );

    /* Dirty hacks for LXDE */
    file = fopen( path, "w" );

    if ( file )
    {
        /* General */
        result = fputs( _("# SpaceFM Session File\n\n# THIS FILE IS NOT DESIGNED TO BE EDITED - it will be read and OVERWRITTEN\n\n# If you delete all session* files, SpaceFM will be reset to factory defaults.\n\n"), file );
        if ( result < 0 )
            goto _save_error;
        fputs( "[General]\n", file );
        /*
        if ( app_settings.singleInstance != singleInstance_default )
            fprintf( file, "singleInstance=%d\n", !!app_settings.singleInstance );
        */
        if ( app_settings.encoding[ 0 ] )
            fprintf( file, "encoding=%s\n", app_settings.encoding );
        //if ( app_settings.show_hidden_files != show_hidden_files_default )
        //    fprintf( file, "show_hidden_files=%d\n", !!app_settings.show_hidden_files );
        //if ( app_settings.show_side_pane != show_side_pane_default )
        //    fprintf( file, "show_side_pane=%d\n", app_settings.show_side_pane );
        //if ( app_settings.side_pane_mode != side_pane_mode_default )
        //    fprintf( file, "side_pane_mode=%d\n", app_settings.side_pane_mode );
        if ( app_settings.show_thumbnail != show_thumbnail_default )
            fprintf( file, "show_thumbnail=%d\n", !!app_settings.show_thumbnail );
        if ( app_settings.max_thumb_size != max_thumb_size_default )
            fprintf( file, "max_thumb_size=%d\n", app_settings.max_thumb_size >> 10 );
        if ( app_settings.big_icon_size != big_icon_size_default )
            fprintf( file, "big_icon_size=%d\n", app_settings.big_icon_size );
        if ( app_settings.small_icon_size != small_icon_size_default )
            fprintf( file, "small_icon_size=%d\n", app_settings.small_icon_size );
        if ( app_settings.tool_icon_size != tool_icon_size_default )
            fprintf( file, "tool_icon_size=%d\n", app_settings.tool_icon_size );
        /* FIXME: temporarily disable trash since it's not finished */
#if 0
        if ( app_settings.use_trash_can != use_trash_can_default )
            fprintf( file, "use_trash_can=%d\n", app_settings.use_trash_can );
#endif
        if ( app_settings.single_click != single_click_default )
            fprintf( file, "single_click=%d\n", app_settings.single_click );
        //if ( app_settings.view_mode != view_mode_default )
        //    fprintf( file, "view_mode=%d\n", app_settings.view_mode );
        if ( app_settings.sort_order != sort_order_default )
            fprintf( file, "sort_order=%d\n", app_settings.sort_order );
        if ( app_settings.sort_type != sort_type_default )
            fprintf( file, "sort_type=%d\n", app_settings.sort_type );
        //if ( app_settings.open_bookmark_method != open_bookmark_method_default )
        //    fprintf( file, "open_bookmark_method=%d\n", app_settings.open_bookmark_method );
        /*
        if ( app_settings.iconTheme )
            fprintf( file, "iconTheme=%s\n", app_settings.iconTheme );
        */
        //if ( app_settings.terminal )
        //    fprintf( file, "terminal=%s\n", app_settings.terminal );
        if ( app_settings.use_si_prefix != use_si_prefix_default )
            fprintf( file, "use_si_prefix=%d\n", !!app_settings.use_si_prefix );
//        if ( app_settings.show_location_bar != show_location_bar_default )
//            fprintf( file, "show_location_bar=%d\n", app_settings.show_location_bar );
/*        if ( app_settings.home_folder )
            fprintf( file, "home_folder=%s\n", app_settings.home_folder );  //MOD
*/        if ( !app_settings.no_execute )
            fprintf( file, "no_execute=%d\n", !!app_settings.no_execute );  //MOD
        if ( app_settings.no_confirm )
            fprintf( file, "no_confirm=%d\n", !!app_settings.no_confirm );  //MOD

        fputs( "\n[Window]\n", file );
        fprintf( file, "width=%d\n", app_settings.width );
        fprintf( file, "height=%d\n", app_settings.height );
        //fprintf( file, "splitter_pos=%d\n", app_settings.splitter_pos );
        fprintf( file, "maximized=%d\n", app_settings.maximized );

        /* Desktop */
        fputs( "\n[Desktop]\n", file );
        //if ( app_settings.show_desktop != show_desktop_default )
        //    fprintf( file, "show_desktop=%d\n", !!app_settings.show_desktop );
        if ( app_settings.show_wallpaper != show_wallpaper_default )
            fprintf( file, "show_wallpaper=%d\n", !!app_settings.show_wallpaper );
        if ( app_settings.wallpaper && app_settings.wallpaper[ 0 ] )
            fprintf( file, "wallpaper=%s\n", app_settings.wallpaper );
        if ( app_settings.wallpaper_mode != wallpaper_mode_default )
            fprintf( file, "wallpaper_mode=%d\n", app_settings.wallpaper_mode );
        if ( app_settings.desktop_sort_by != desktop_sort_by_default )
            fprintf( file, "sort_by=%d\n", app_settings.desktop_sort_by );
        if ( app_settings.desktop_sort_type != desktop_sort_type_default )
            fprintf( file, "sort_type=%d\n", app_settings.desktop_sort_type );
        if ( app_settings.show_wm_menu != show_wm_menu_default )
            fprintf( file, "show_wm_menu=%d\n", app_settings.show_wm_menu );
        if ( app_settings.desk_single_click != desk_single_click_default )
            fprintf( file, "desk_single_click=%d\n", app_settings.desk_single_click );
        if ( app_settings.desk_open_mime != desk_open_mime_default )
            fprintf( file, "desk_open_mime=%d\n", app_settings.desk_open_mime );
        
        // always save these colors in case defaults change
        //if ( ! gdk_color_equal( &app_settings.desktop_bg1,
        //       &desktop_bg1_default ) )
            save_color( file, "bg1",
                        &app_settings.desktop_bg1 );
        //if ( ! gdk_color_equal( &app_settings.desktop_bg2,
        //       &desktop_bg2_default ) )
            save_color( file, "bg2",
                        &app_settings.desktop_bg2 );
        //if ( ! gdk_color_equal( &app_settings.desktop_text,
        //       &desktop_text_default ) )
            save_color( file, "text",
                        &app_settings.desktop_text );
        //if ( ! gdk_color_equal( &app_settings.desktop_shadow,
        //       &desktop_shadow_default ) )
            save_color( file, "shadow",
                        &app_settings.desktop_shadow );
                        
        if ( app_settings.desk_font )
        {
            char* fontname = pango_font_description_to_string(
                                                    app_settings.desk_font );
            if ( fontname )
                fprintf( file, "font=%s\n", fontname );
            g_free( fontname );
        }
        if ( app_settings.margin_top != margin_top_default )
            fprintf( file, "margin_top=%d\n", app_settings.margin_top );
        if ( app_settings.margin_left != margin_left_default )
            fprintf( file, "margin_left=%d\n", app_settings.margin_left );
        if ( app_settings.margin_right != margin_right_default )
            fprintf( file, "margin_right=%d\n", app_settings.margin_right );
        if ( app_settings.margin_bottom != margin_bottom_default )
            fprintf( file, "margin_bottom=%d\n", app_settings.margin_bottom );
        if ( app_settings.margin_pad != margin_pad_default )
            fprintf( file, "margin_pad=%d\n", app_settings.margin_pad );

        /* Interface */
        fputs( "\n[Interface]\n", file );
        if ( app_settings.always_show_tabs != always_show_tabs_default )
            fprintf( file, "always_show_tabs=%d\n", app_settings.always_show_tabs );
        if ( app_settings.hide_close_tab_buttons != hide_close_tab_buttons_default )
            fprintf( file, "show_close_tab_buttons=%d\n", !app_settings.hide_close_tab_buttons );
        //if ( app_settings.hide_side_pane_buttons != hide_side_pane_buttons_default )
        //    fprintf( file, "hide_side_pane_buttons=%d\n", app_settings.hide_side_pane_buttons );
        //if ( app_settings.hide_folder_content_border != hide_folder_content_border_default )
        //    fprintf( file, "hide_folder_content_border=%d\n", app_settings.hide_folder_content_border );

        // MOD extra settings
        fputs( "\n[MOD]\n", file );
        xset_write( file );
        
        result = fputs( "\n", file );
        if ( result < 0 )
            goto _save_error;
        result = fclose( file );
        if ( result )
            goto _save_error;
    }
    else
        goto _save_error;
        
    // move
    char* session = g_build_filename( settings_config_dir, "session", NULL );
    unlink( session );
    if ( g_file_test( session, G_FILE_TEST_EXISTS ) )
        goto _save_error;
    result = rename( path, session );
    g_free( path );
    if ( result == -1 )
        goto _save_error;
    if ( !g_file_test( session, G_FILE_TEST_EXISTS ) )
        goto _save_error;
    g_free( session );

    /* Save bookmarks */
    ptk_bookmarks_save();
    return NULL;    

_save_error:
    if ( errno )
    {
        err_msg = (char*)g_strerror( errno );
        if ( err_msg )
            err_msg = g_strdup( err_msg );
    }
    if ( !err_msg )
        err_msg = g_strdup_printf( _("Error saving file") );
    ptk_bookmarks_save();
    return err_msg;
}

void free_settings()
{
/*
    if ( app_settings.iconTheme )
        g_free( app_settings.iconTheme );
*/
    //g_free( app_settings.terminal );
    g_free( app_settings.wallpaper );

    ptk_bookmarks_unref();

    if ( xset_cmd_history )
    {
        g_list_foreach( xset_cmd_history, (GFunc)g_free, NULL );
        g_list_free( xset_cmd_history );
        xset_cmd_history = NULL;
    }

    xset_free_all();
}

const char* xset_get_config_dir()
{
    return settings_config_dir;
}

const char* xset_get_tmp_dir()
{
    return settings_tmp_dir;
}

const char* xset_get_shared_tmp_dir()
{
    if ( !g_file_test( settings_shared_tmp_dir, G_FILE_TEST_EXISTS ) )
    {
        g_mkdir_with_parents( settings_shared_tmp_dir,
                                        S_IRWXU | S_IRWXG | S_IRWXO | S_ISVTX );
        chmod( settings_shared_tmp_dir,
                                    S_IRWXU | S_IRWXG | S_IRWXO | S_ISVTX );
    }
    return settings_shared_tmp_dir;
}

const char* xset_get_user_tmp_dir()
{
    if ( settings_user_tmp_dir && 
                    g_file_test( settings_user_tmp_dir, G_FILE_TEST_EXISTS ) )
        return settings_user_tmp_dir;

    char* rand;
    char* name;
    int count = 0;
    int ret;
    do
    {
        g_free( settings_user_tmp_dir );
        rand = randhex8();
        name =  g_strdup_printf( "spacefm-%s-%s.tmp", g_get_user_name(), rand );
        g_free( rand );
        settings_user_tmp_dir = g_build_filename( settings_tmp_dir, name, NULL );
        g_free( name );
        count++;
    } while ( count < 1000 && ( ret = mkdir( settings_user_tmp_dir,
                        S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH ) != 0 ) );
    if ( ret != 0 )
    {
        g_free( settings_user_tmp_dir );
        settings_user_tmp_dir = NULL;
        g_warning( "Unable to create temporary directory in %s", settings_tmp_dir );
    }
    return settings_user_tmp_dir;
}

static gboolean idle_save_settings( gpointer main_window )
{
    //printf("AUTOSAVE *** idle_save_settings\n" );
    char* err_msg = save_settings( main_window );
    if ( err_msg )
    {
        printf( _("SpaceFM Error: Unable to autosave session file ( %s )\n"), err_msg );
        g_free( err_msg );
    }
    return FALSE;
}

static void auto_save_start( gpointer main_window, gboolean delay )
{
    //printf("AUTOSAVE auto_save_start\n" );
    if ( !delay )
    {
        g_idle_add( ( GSourceFunc ) idle_save_settings, main_window );
        xset_autosave_request = FALSE;
    }
    else
        xset_autosave_request = TRUE;
    if ( !xset_autosave_timer )
    {
        xset_autosave_timer = g_timeout_add_seconds( 10,
                            ( GSourceFunc ) on_autosave_timer, main_window );
        //printf("AUTOSAVE timer started\n" );
    }
}

gboolean on_autosave_timer( gpointer main_window )
{
    //printf("AUTOSAVE timeout\n" );
    if ( xset_autosave_timer )
    {
        g_source_remove( xset_autosave_timer );
        xset_autosave_timer = 0;
    }
    if ( xset_autosave_request )
        auto_save_start( main_window, FALSE );
    return FALSE;
}

void xset_autosave( PtkFileBrowser* file_browser, gboolean force, gboolean delay )
{
    if ( xset_autosave_timer && !force )
    {
        // autosave timer is running, so request save on timeout to prevent
        // saving too frequently, unless force
        xset_autosave_request = TRUE;
        //printf("AUTOSAVE request\n" );
    }
    else
    {
        if ( xset_autosave_timer && force )
        {
            g_source_remove( xset_autosave_timer );
            xset_autosave_timer = 0;
        }
        /* if ( force )
            printf("AUTOSAVE force\n" );
        else if ( delay )
            printf("AUTOSAVE delay\n" );
        else
            printf("AUTOSAVE normal\n" ); */
        auto_save_start( file_browser ? file_browser->main_window : NULL,
                                                            !force && delay );
    }
}

void xset_autosave_cancel()
{
    //printf("AUTOSAVE cancel\n" );
    xset_autosave_request = FALSE;
    if ( xset_autosave_timer )
    {
        g_source_remove( xset_autosave_timer );
        xset_autosave_timer = 0;
    }
}


/////////////////////////////////////////////////////////////////////////////
//MOD extra settings below


char* get_valid_su()  // may return NULL
{
    char* set_su = NULL;
    int i;
    if ( xset_get_s( "su_command" ) )
        set_su = g_strdup( xset_get_s( "su_command" ) );

    if ( set_su )
    {
        // is set_su valid su command?
        for ( i = 0; i < G_N_ELEMENTS( su_commands ); i++ )
        {
            if ( !strcmp( su_commands[i], set_su ) )
                break;
        }
        if ( i == G_N_ELEMENTS( su_commands ) )
        {
            // invalid
            g_free( set_su );
            set_su = NULL;
        }
    }
    if ( !set_su )
    {
        // discovery
        for ( i = 0; i < G_N_ELEMENTS( su_commands ); i++ )
        {
            if ( set_su = g_find_program_in_path( su_commands[i] ) )
                break;
        }
        if ( !set_su )
            set_su = g_strdup( su_commands[0] );
        xset_set( "su_command", "s", set_su );
    }
    char* str = g_find_program_in_path( set_su );
    g_free( set_su );
    return str;
}

char* get_valid_gsu()  // may return NULL
{
    int i;
    char* set_gsu = NULL;
    char* custom_gsu = NULL;
    if ( xset_get_s( "gsu_command" ) )
        set_gsu = g_strdup( xset_get_s( "gsu_command" ) );
#ifdef PREFERABLE_SUDO_PROG
    custom_gsu = g_find_program_in_path( PREFERABLE_SUDO_PROG );
#endif
    if ( custom_gsu )
    {
        if ( !set_gsu || set_gsu[0] == '\0' )
        {
            xset_set( "gsu_command", "s", custom_gsu );
            if ( set_gsu )
                g_free( set_gsu );
            set_gsu = g_strdup( custom_gsu );
        }
    }
    if ( set_gsu )
    {
        if ( !custom_gsu || strcmp( custom_gsu, set_gsu ) )
        {
            // is set_gsu valid gsu command?
            for ( i = 0; i < G_N_ELEMENTS( gsu_commands ); i++ )
            {
                if ( !strcmp( gsu_commands[i], set_gsu ) )
                    break;
            }
            if ( i == G_N_ELEMENTS( gsu_commands ) )
            {
                // invalid
                g_free( set_gsu );
                set_gsu = NULL;
            }
        }
    }
    if ( !set_gsu )
    {
        // discovery
        for ( i = 0; i < G_N_ELEMENTS( gsu_commands ); i++ )
        {
            // don't automatically select gksudo
            if ( strcmp( gsu_commands[i], "/usr/bin/gksudo" ) )
            {
                if ( set_gsu = g_find_program_in_path( gsu_commands[i] ) )
                    break;
            }
        }
        if ( !set_gsu )
            set_gsu = g_strdup( gsu_commands[0] );
        xset_set( "gsu_command", "s", set_gsu );
    }
    
    if ( custom_gsu )
        g_free( custom_gsu );
    char* str = g_find_program_in_path( set_gsu );
    
    if ( !str && !strcmp( set_gsu, "/usr/bin/kdesu" ) )
    {
        // kdesu may be in libexec path
        char* stdout;
        if ( g_spawn_command_line_sync( "kde4-config --path libexec",
                                            &stdout, NULL, NULL, NULL ) 
                                            && stdout && stdout[0] != '\0' )
        {
            if ( str = strchr( stdout, '\n' ) )
               str[0] = '\0';
            str = g_build_filename( stdout, "kdesu", NULL );
            g_free( stdout );
            if ( !g_file_test( str, G_FILE_TEST_EXISTS ) )
            {
                g_free( str );
                str = NULL;
            }
        }
    }
    g_free( set_gsu );
    return str;
}

char* randhex8()
{
    char hex[9];
    uint n;

    n = rand();
    sprintf(hex, "%08x", n);
    return g_strdup( hex );
}

char* replace_line_subs( const char* line )
{
    char* old_s;
    char* s;
    int i;
    const char* perc[] = { "%f", "%F", "%n", "%N", "%d", "%D", "%v", "%l", "%m", "%y", "%b", "%t", "%p", "%a" };
    const char* var[] =
    {
        "\"${fm_file}\"",
        "\"${fm_files[@]}\"",
        "\"${fm_filename}\"",
        "\"${fm_filenames[@]}\"",
        "\"${fm_pwd}\"",
        "\"${fm_pwd}\"",
        "\"${fm_device}\"",
        "\"${fm_device_label}\"",
        "\"${fm_device_mount_point}\"",
        "\"${fm_device_fstype}\"",
        "\"${fm_bookmark}\"",
        "\"${fm_task_pwd}\"",
        "\"${fm_task_pid}\"",
        "\"${fm_value}\""
    };

    s = g_strdup( line );
    int num = G_N_ELEMENTS( perc );
    for ( i = 0; i < num; i++ )
    {
        if ( strstr( line, perc[i] ) )
        {
            old_s = s;
            s = replace_string( old_s, perc[i], var[i], FALSE );
            g_free( old_s );
        }
    }
    return s;
}

char* replace_desktop_subs( const char* line )
{
    char* old_s;
    char* s;
    int i;
    const char* perc[] = { "%f", "%F", "%u", "%U", "%d", "%D" };
    const char* var[] =
    {
        "\"${fm_file}\"",
        "\"${fm_files[@]}\"",
        "\"${fm_file}\"",
        "\"${fm_files[@]}\"",
        "\"${fm_pwd}\"",
        "\"${fm_pwd}\""
    };

    s = g_strdup( line );
    int num = G_N_ELEMENTS( perc );
    for ( i = 0; i < num; i++ )
    {
        if ( strstr( line, perc[i] ) )
        {
            old_s = s;
            s = replace_string( old_s, perc[i], var[i], FALSE );
            g_free( old_s );
        }
    }
    return s;
}

gboolean is_alphanum( char* str )
{
    char* ptr = str;
    while ( ptr[0] != '\0' )
    {
        if ( !g_ascii_isalnum( ptr[0] ) )
            return FALSE;
        ptr++;
    }
    return TRUE;
}

char* get_name_extension( char* full_name, gboolean is_dir, char** ext )
{
    char* dot;
    char* str;
    char* final_ext;
    char* full;

    full = g_strdup( full_name );
    // get last dot
    if ( is_dir || !( dot = strrchr( full, '.' ) ) || dot == full )
    {
        // dir or no dots or one dot first
        *ext = NULL;
        return full;
    }
    dot[0] = '\0';
    final_ext = dot + 1;
    // get previous dot
    dot = strrchr( full, '.' );
    uint final_ext_len = strlen( final_ext );
    if ( dot && !strcmp( dot + 1, "tar" ) && final_ext_len < 11 && final_ext_len )
    {
        // double extension
        final_ext[-1] = '.';
        *ext = g_strdup( dot + 1 );
        dot[0] = '\0';
        str = g_strdup( full );
        g_free( full );
        return str;
    }
    // single extension, one or more dots
    if ( final_ext_len < 11 && final_ext[0] )
    {
        *ext = g_strdup( final_ext );
        str = g_strdup( full );
        g_free( full );
        return str;
    }
    else
    {
        // extension too long, probably part of name
        final_ext[-1] = '.';
        *ext = NULL;
        return full;
    }
}

/*
char* get_name_extension( char* full_name, gboolean is_dir, char** ext )
{
    char* dot = strchr( full_name, '.' );
    if ( !dot || is_dir )
    {
        *ext = NULL;
        return g_strdup( full_name );
    }
    char* name = NULL;
    char* old_name;
    char* old_extension;
    char* segment;
    char* extension = NULL;
    char* seg_start = full_name;
    while ( seg_start )
    {
        if ( dot )
            segment = g_strndup( seg_start, dot - seg_start );
        else
            segment = g_strdup( seg_start );
        if ( ( seg_start == full_name || g_utf8_strlen( segment, -1 ) > 5
                                            || !is_alphanum( segment ) )
                        && !( seg_start != full_name && !strcmp( segment, "desktop" ) ) )
        {
            // segment and thus all prior segments are part of name
            old_name = name;
            //printf("part of name\n");
            if ( !extension )
            {
                if ( !old_name )
                    name = g_strdup( segment );
                else
                    name = g_strdup_printf( "%s.%s", old_name, segment );
                //printf("\told_name=%s\n\tsegment=%s\n\tname=%s\n", old_name, segment, name );
            }
            else
            {
                name = g_strdup_printf( "%s.%s.%s", old_name, extension, segment );
                //printf("\told_name=%s\n\text=%s\n\tsegment=%s\n\tname=%s\n", old_name, extension, segment, name );
                g_free( extension );
                extension = NULL;
            }
            g_free( old_name );
        }
        else
        {
            // segment is part of extension
            //printf("part of extension\n");
            if ( !extension )
            {
                extension = g_strdup( segment );
                //printf ("\tsegment=%s\n\text=%s\n", segment, extension );
            }
            else
            {
                old_extension = extension;
                extension = g_strdup_printf( "%s.%s", old_extension, segment );
                //printf ("\told_extension=%s\n\tsegment=%s\n\text=%s\n", old_extension, segment, extension );
                g_free( old_extension );
            }
        }
        g_free( segment );
        if ( dot )
        {
            seg_start = ++dot;
            dot = strchr( seg_start, '.' );
        }
        else
            seg_start = NULL;
    }
    *ext = extension;
    return name;
}
*/

void xset_free_all()
{
    XSet* set;
    GList* l;

    for ( l = xsets; l; l = l->next )
    {
        set = l->data;
        if ( set->ob2_data && g_str_has_prefix( set->name, "evt_" ) )
        {
            g_list_foreach( (GList*)set->ob2_data, (GFunc)g_free, NULL );
            g_list_free( (GList*)set->ob2_data );
        }
        if ( set->name )
            g_free( set->name );
        if ( set->s )
            g_free( set->s );
        if ( set->x )
            g_free( set->x );
        if ( set->y )
            g_free( set->y );
        if ( set->z )
            g_free( set->z );
        if ( set->menu_label )
            g_free( set->menu_label );
        if ( set->shared_key )
            g_free( set->shared_key );
        if ( set->icon )
            g_free( set->icon );
        if ( set->desc )
            g_free( set->desc );
        if ( set->title )
            g_free( set->title );
        if ( set->next )
            g_free( set->next );
        if ( set->parent )
            g_free( set->parent );
        if ( set->child )
            g_free( set->child );
        if ( set->prev )
            g_free( set->prev );
        if ( set->line )
            g_free( set->line );
        if ( set->plugin )
        {
            if ( set->plug_dir )
                g_free( set->plug_dir );
            if ( set->plug_name )
                g_free( set->plug_name );
        }
        g_slice_free( XSet, set );
    }
    g_list_free( xsets );
    xsets = NULL;
    set_last = NULL;
    
    if ( xset_context )
    {
        xset_context_new();
        g_slice_free( XSetContext, xset_context );
        xset_context = NULL;
    }
}

void xset_free( XSet* set )
{
    if ( set->name )
        g_free( set->name );
    if ( set->s )
        g_free( set->s );
    if ( set->x )
        g_free( set->x );
    if ( set->y )
        g_free( set->y );
    if ( set->z )
        g_free( set->z );
    if ( set->menu_label )
        g_free( set->menu_label );
    if ( set->shared_key )
        g_free( set->shared_key );
    if ( set->icon )
        g_free( set->icon );
    if ( set->desc )
        g_free( set->desc );
    if ( set->title )
        g_free( set->title );
    if ( set->next )
        g_free( set->next );
    if ( set->parent )
        g_free( set->parent );
    if ( set->child )
        g_free( set->child );
    if ( set->prev )
        g_free( set->prev );
    if ( set->line )
        g_free( set->line );
    if ( set->plugin )
    {
        if ( set->plug_dir )
            g_free( set->plug_dir );
        if ( set->plug_name )
            g_free( set->plug_name );
    }
    xsets = g_list_remove( xsets, set );
    g_slice_free( XSet, set );
    set_last = NULL;
}

XSet* xset_new( const char* name )
{
    XSet* set = g_slice_new( XSet );
    set->name = g_strdup( name );

    set->b = XSET_B_UNSET;
    set->s = NULL;
    set->x = NULL;
    set->y = NULL;
    set->z = NULL;
    set->disable = FALSE;
    set->menu_label = NULL;
    set->menu_style = XSET_MENU_NORMAL;
    set->cb_func = NULL;
    set->cb_data = NULL;
    set->ob1 = NULL;
    set->ob1_data = NULL;
    set->ob2 = NULL;
    set->ob2_data = NULL;
    set->key = 0;
    set->keymod = 0;
    set->shared_key = NULL;
    set->icon = NULL;
    set->desc = NULL;
    set->title = NULL;
    set->next = NULL;
    set->context = NULL;
    set->tool = XSET_B_UNSET;
    set->lock = TRUE;
    set->plugin = FALSE;
    
    // custom ( !lock )
    set->prev = NULL;
    set->parent = NULL;
    set->child = NULL;
    set->line = NULL;
    set->task = XSET_B_UNSET;
    set->task_pop = XSET_B_UNSET;
    set->task_err = XSET_B_UNSET;
    set->task_out = XSET_B_UNSET;
    set->in_terminal = XSET_B_UNSET;
    set->keep_terminal = XSET_B_UNSET;
    set->scroll_lock = XSET_B_UNSET;
    set->opener = 0;
    return set;
}

XSet* xset_get( const char* name )
{
    GList* l;

    if ( !name )
        return NULL;
    
    for ( l = xsets; l; l = l->next )
    {
        if ( !strcmp( name, ((XSet*)l->data)->name ) )
        {
            // existing xset
            return l->data;
        }
    }

    // add new
    xsets = g_list_prepend( xsets, xset_new( name ) );
    return xsets->data;    
}

XSet* xset_get_panel( int panel, const char* name )
{
    XSet* set;
    char* fullname = g_strdup_printf( "panel%d_%s", panel, name );
    set = xset_get( fullname );
    g_free( fullname );
    return set;
}

XSet* xset_get_panel_mode( int panel, const char* name, char mode )
{
    XSet* set;
    char* fullname = g_strdup_printf( "panel%d_%s%d", panel, name, mode );
    set = xset_get( fullname );
    g_free( fullname );
    return set;
}

char* xset_get_s( const char* name )
{
    XSet* set = xset_get( name );
    if ( set )
        return set->s;
    else
        return NULL;
}

char* xset_get_s_panel( int panel, const char* name )
{
    char* fullname = g_strdup_printf( "panel%d_%s", panel, name );
    char* s = xset_get_s( fullname );
    g_free( fullname );
    return s;
}

gboolean xset_get_b( const char* name )
{
    XSet* set = xset_get( name );
    return ( set->b == XSET_B_TRUE );
}

gboolean xset_get_b_panel( int panel, const char* name )
{
    char* fullname = g_strdup_printf( "panel%d_%s", panel, name );
    gboolean b = xset_get_b( fullname );
    g_free( fullname );
    return b;
}

gboolean xset_get_b_panel_mode( int panel, const char* name, char mode )
{
    char* fullname = g_strdup_printf( "panel%d_%s%d", panel, name, mode );
    gboolean b = xset_get_b( fullname );
    g_free( fullname );
    return b;
}

gboolean xset_get_b_set( XSet* set )
{
    return ( set->b == XSET_B_TRUE );
}

XSet* xset_is( const char* name )
{
    XSet* set;
    GList* l;

    if ( !name )
        return NULL;
    
    for ( l = xsets; l; l = l->next )
    {
        if ( !strcmp( name, ((XSet*)l->data)->name ) )
        {
            // existing xset
            return l->data;
        }
    }
    return NULL;
}

XSet* xset_set_b( const char* name, gboolean bval )
{
    XSet* set = xset_get( name );

    if ( bval )
        set->b = XSET_B_TRUE;
    else
        set->b = XSET_B_FALSE;
    return set;
}

XSet* xset_set_b_panel( int panel, const char* name, gboolean bval )
{
    char* fullname = g_strdup_printf( "panel%d_%s", panel, name );
    XSet* set = xset_set_b( fullname, bval );
    g_free( fullname );
    return set;
}

XSet* xset_set_b_panel_mode( int panel, const char* name, char mode,
                                                            gboolean bval )
{
    char* fullname = g_strdup_printf( "panel%d_%s%d", panel, name, mode );
    XSet* set = xset_set_b( fullname, bval );
    g_free( fullname );
    return set;
}

gboolean xset_get_bool( const char* name, const char* var )
{
    XSet* set = xset_get( name );
    if ( !strcmp( var, "b" ) )
        return ( set->b == XSET_B_TRUE );
    if ( !strcmp( var, "disable" ) )
        return set->disable;
    if ( !strcmp( var, "style" ) )
        return !!set->menu_style;

    char* varstring = NULL;
    if ( !strcmp( var, "x" ) )
        varstring = set->x;
    else if ( !strcmp( var, "y" ) )
        varstring = set->y;
    else if ( !strcmp( var, "z" ) )
        varstring = set->z;
    else if ( !strcmp( var, "s" ) )
        varstring = set->s;
    else if ( !strcmp( var, "desc" ) )
        varstring = set->desc;
    else if ( !strcmp( var, "title" ) )
        varstring = set->title;
    else if ( !strcmp( var, "lbl" ) )
        varstring = set->menu_label;
    else if ( !set->lock )
    {
        if ( !strcmp( var, "task" ) )
            return ( set->task == XSET_B_TRUE );
        if ( !strcmp( var, "task_pop" ) )
            return ( set->task_pop == XSET_B_TRUE );
        if ( !strcmp( var, "task-err" ) )
            return ( set->task_err == XSET_B_TRUE );
        if ( !strcmp( var, "task_out" ) )
            return ( set->task_out == XSET_B_TRUE );
    }
    if ( !varstring )
        return FALSE;
    return !!atoi( varstring );
}

gboolean xset_get_bool_panel( int panel, const char* name, const char* var )
{
    char* fullname = g_strdup_printf( "panel%d_%s", panel, name );
    gboolean bool = xset_get_bool( fullname, var );
    g_free( fullname );
    return bool;
}

int xset_get_int( const char* name, const char* var )
{
    XSet* set = xset_get( name );
    char* varstring = NULL;
    if ( !strcmp( var, "key" ) )
        return set->key;
    else if ( !strcmp( var, "keymod" ) )
        return set->keymod;
    else if ( !strcmp( var, "x" ) )
        varstring = set->x;
    else if ( !strcmp( var, "y" ) )
        varstring = set->y;
    else if ( !strcmp( var, "z" ) )
        varstring = set->z;
    else if ( !strcmp( var, "s" ) )
        varstring = set->s;
    if ( !varstring )
        return 0;
    return atoi( varstring );
}

int xset_get_int_panel( int panel, const char* name, const char* var )
{
    char* fullname = g_strdup_printf( "panel%d_%s", panel, name );
    int i = xset_get_int( fullname, var );
    g_free( fullname );
    return i;
}

static void xset_write_set( FILE* file, XSet* set )
{
    if ( set->plugin )
        return;
    if ( set->s )
        fprintf( file, "%s-s=%s\n", set->name, set->s );
    if ( set->x )
        fprintf( file, "%s-x=%s\n", set->name, set->x );
    if ( set->y )
        fprintf( file, "%s-y=%s\n", set->name, set->y );
    if ( set->z )
        fprintf( file, "%s-z=%s\n", set->name, set->z );
    if ( set->key )
        fprintf( file, "%s-key=%d\n", set->name, set->key );
    if ( set->keymod )
        fprintf( file, "%s-keymod=%d\n", set->name, set->keymod );
    // menu label
    if ( set->menu_label )
    {
        if ( set->lock )
        {
            // built-in
            if ( set->in_terminal == XSET_B_TRUE && set->menu_label[0] )
                // only save lbl if menu_label was customized
                fprintf( file, "%s-lbl=%s\n", set->name, set->menu_label );
        }
        else
            // custom
            fprintf( file, "%s-label=%s\n", set->name, set->menu_label );
    }
    // icon
    if ( set->lock )
    {
        // built-in            
        if ( set->keep_terminal == XSET_B_TRUE )
            // only save icn if icon was customized
            fprintf( file, "%s-icn=%s\n", set->name, set->icon ? set->icon : "" );
    }
    else if ( set->icon )
        // custom
        fprintf( file, "%s-icon=%s\n", set->name, set->icon );
    if ( set->next )
        fprintf( file, "%s-next=%s\n", set->name, set->next );
    if ( set->child )
        fprintf( file, "%s-child=%s\n", set->name, set->child );
    if ( set->context )
        fprintf( file, "%s-cxt=%s\n", set->name, set->context );
    if ( set->b != XSET_B_UNSET )
        fprintf( file, "%s-b=%d\n", set->name, set->b );
    if ( set->tool != XSET_B_UNSET )
        fprintf( file, "%s-tool=%d\n", set->name, set->tool );
    if ( !set->lock )
    {
        if ( set->menu_style )
            fprintf( file, "%s-style=%d\n", set->name, set->menu_style );
        if ( set->desc )
            fprintf( file, "%s-desc=%s\n", set->name, set->desc );
        if ( set->title )
            fprintf( file, "%s-title=%s\n", set->name, set->title );
        if ( set->prev )
            fprintf( file, "%s-prev=%s\n", set->name, set->prev );
        if ( set->parent )
            fprintf( file, "%s-parent=%s\n", set->name, set->parent );
        if ( set->line )
            fprintf( file, "%s-line=%s\n", set->name, set->line );
        if ( set->task != XSET_B_UNSET )
            fprintf( file, "%s-task=%d\n", set->name, set->task );
        if ( set->task_pop != XSET_B_UNSET )
            fprintf( file, "%s-task_pop=%d\n", set->name, set->task_pop );
        if ( set->task_err != XSET_B_UNSET )
            fprintf( file, "%s-task_err=%d\n", set->name, set->task_err );
        if ( set->task_out != XSET_B_UNSET )
            fprintf( file, "%s-task_out=%d\n", set->name, set->task_out );
        if ( set->in_terminal != XSET_B_UNSET )
            fprintf( file, "%s-term=%d\n", set->name, set->in_terminal );
        if ( set->keep_terminal != XSET_B_UNSET )
            fprintf( file, "%s-keep=%d\n", set->name, set->keep_terminal );
        if ( set->scroll_lock != XSET_B_UNSET )
            fprintf( file, "%s-scroll=%d\n", set->name, set->scroll_lock );
        if ( set->opener != 0 )
            fprintf( file, "%s-op=%d\n", set->name, set->opener );
    }
}

void xset_write( FILE* file )
{
    GList* l;
    
    if ( !file )
        return;

    for ( l = g_list_last( xsets ); l; l = l->prev )
        xset_write_set( file, (XSet*)l->data );
}

void xset_parse( char* line )
{
    char* sep = strchr( line, '=' );
    char* name;
    char* value;
    if ( !sep )
        return ;
    name = line;
    value = sep + 1;
    *sep = '\0';
    sep = strchr( name, '-' );
    if ( !sep )
        return ;
    char* var = sep + 1;
    *sep = '\0';

    if ( !strncmp( name, "cstm_", 5 ) )
    {
        // custom
        if ( !strcmp( set_last->name, name ) )
            xset_set_set( set_last, var, value );
        else
        {
            set_last = xset_set( name, var, value );
            if ( set_last->lock )
                set_last->lock = FALSE;
        }
    }
    else
    {
        // normal
        if ( !strcmp( set_last->name, name ) )
            xset_set_set( set_last, var, value );
        else
            set_last = xset_set( name, var, value );
    }
}

XSet* xset_set_cb( const char* name, void (*cb_func) (), gpointer cb_data )
{
    XSet* set = xset_get( name );
    set->cb_func = cb_func;
    set->cb_data = cb_data;
    return set;
}

XSet* xset_set_cb_panel( int panel, const char* name, void (*cb_func) (), gpointer cb_data )
{
    char* fullname = g_strdup_printf( "panel%d_%s", panel, name );
    XSet* set = xset_set_cb( fullname, cb_func, cb_data );
    g_free( fullname );
    return set;
}

XSet* xset_set_ob1_int( XSet* set, const char* ob1, int ob1_int )
{
    if ( set->ob1 )
        g_free( set->ob1 );
    set->ob1 = g_strdup( ob1 );
    set->ob1_data = GINT_TO_POINTER( ob1_int );
    return set;
}

XSet* xset_set_ob1( XSet* set, const char* ob1, gpointer ob1_data )
{
    if ( set->ob1 )
        g_free( set->ob1 );
    set->ob1 = g_strdup( ob1 );
    set->ob1_data = ob1_data;
    return set;
}

XSet* xset_set_ob2( XSet* set, const char* ob2, gpointer ob2_data )
{
    if ( set->ob2 )
        g_free( set->ob2 );
    set->ob2 = g_strdup( ob2 );
    set->ob2_data = ob2_data;
    return set;
}

XSet* xset_set_set( XSet* set, const char* var, const char* value )
{
    if ( !set )
        return NULL;

    if ( !strcmp( var, "s" ) )
    {
        if ( set->s )
            g_free( set->s );
        set->s = g_strdup( value );
    }
    else if ( !strcmp( var, "b" ) )
    {
        if ( !strcmp( value, "1" ) )
            set->b = XSET_B_TRUE;
        else
            set->b = XSET_B_FALSE;
    }
    else if ( !strcmp( var, "x" ) )
    {
        if ( set->x )
            g_free( set->x );
        set->x = g_strdup( value );
    }
    else if ( !strcmp( var, "y" ) )
    {
        if ( set->y )
            g_free( set->y );
        set->y = g_strdup( value );
    }
    else if ( !strcmp( var, "z" ) )
    {
        if ( set->z )
            g_free( set->z );
        set->z = g_strdup( value );
    }
    else if ( !strcmp( var, "key" ) )
        set->key = atoi( value );
    else if ( !strcmp( var, "keymod" ) )
        set->keymod = atoi( value );
    else if ( !strcmp( var, "style" ) )
    {
        set->menu_style = atoi( value );
    }
    else if ( !strcmp( var, "desc" ) )
    {
        if ( set->desc )
            g_free( set->desc );
        set->desc = g_strdup( value );
    }
    else if ( !strcmp( var, "title" ) )
    {
        if ( set->title )
            g_free( set->title );
        set->title = g_strdup( value );
    }
    else if ( !strcmp( var, "lbl" ) )
    {
        // lbl is only used >= 0.9.0 for changed lock default menu_label
        if ( set->menu_label )
            g_free( set->menu_label );
        set->menu_label = g_strdup( value );
        if ( set->lock )
            // indicate that menu label is not default and should be saved
            set->in_terminal = XSET_B_TRUE;
    }
    else if ( !strcmp( var, "icn" ) )
    {
        // icn is only used >= 0.9.0 for changed lock default icon
        if ( set->icon )
            g_free( set->icon );
        set->icon = g_strdup( value );
        if ( set->lock )
            // indicate that icon is not default and should be saved
            set->keep_terminal = XSET_B_TRUE;
    }
    else if ( !strcmp( var, "label" ) )
    {
        // pre-0.9.0 menu_label or >= 0.9.0 custom item label
        // only save if custom or not default label
        if ( !set->lock || g_strcmp0( set->menu_label, value ) )
        {
            if ( set->menu_label )
                g_free( set->menu_label );
            set->menu_label = g_strdup( value );
            if ( set->lock )
                // indicate that menu label is not default and should be saved
                set->in_terminal = XSET_B_TRUE;
        }
    }
    else if ( !strcmp( var, "icon" ) )
    {
        // pre-0.9.0 icon or >= 0.9.0 custom item icon
        // only save if custom or not default icon
        // also check that stock name doesn't match
        if ( !set->lock || ( g_strcmp0( set->icon, value ) &&
                             ( !icon_stock_to_id( value ) ||
                               !icon_stock_to_id( set->icon ) ||
                               g_strcmp0( icon_stock_to_id( value ),
                                        icon_stock_to_id( set->icon ) ) ) ) )
        {
            if ( set->icon )
                g_free( set->icon );
            set->icon = g_strdup( value );
            if ( set->lock )
            {
                // indicate that icon is not default and should be saved
                set->keep_terminal = XSET_B_TRUE;
            }
        }
    }
    else if ( !strcmp( var, "shared_key" ) )
    {
        if ( set->shared_key )
            g_free( set->shared_key );
        set->shared_key = g_strdup( value );
    }
    else if ( !strcmp( var, "next" ) )
    {
        if ( set->next )
            g_free( set->next );
        set->next = g_strdup( value );
    }
    else if ( !strcmp( var, "prev" ) )
    {
        if ( set->prev )
            g_free( set->prev );
        set->prev = g_strdup( value );
    }
    else if ( !strcmp( var, "parent" ) )
    {
        if ( set->parent )
            g_free( set->parent );
        set->parent = g_strdup( value );
    }
    else if ( !strcmp( var, "child" ) )
    {
        if ( set->child )
            g_free( set->child );
        set->child = g_strdup( value );
    }
    else if ( !strcmp( var, "cxt" ) )
    {
        if ( set->context )
            g_free( set->context );
        set->context = g_strdup( value );
    }
    else if ( !strcmp( var, "line" ) )
    {
        if ( set->line )
            g_free( set->line );
        set->line = g_strdup( value );
    }
    else if ( !strcmp( var, "tool" ) )
    {
        set->tool = atoi( value );
    }
    else if ( !strcmp( var, "task" ) )
    {
        if ( !strcmp( value, "1" ) )
            set->task = XSET_B_TRUE;
        else
            set->task = XSET_B_UNSET;
    }
    else if ( !strcmp( var, "task_pop" ) )
    {
        if ( !strcmp( value, "1" ) )
            set->task_pop = XSET_B_TRUE;
        else
            set->task_pop = XSET_B_UNSET;
    }
    else if ( !strcmp( var, "task_err" ) )
    {
        if ( !strcmp( value, "1" ) )
            set->task_err = XSET_B_TRUE;
        else
            set->task_err = XSET_B_UNSET;
    }
    else if ( !strcmp( var, "task_out" ) )
    {
        if ( !strcmp( value, "1" ) )
            set->task_out = XSET_B_TRUE;
        else
            set->task_out = XSET_B_UNSET;
    }
    else if ( !strcmp( var, "term" ) )
    {
        if ( !strcmp( value, "1" ) )
            set->in_terminal = XSET_B_TRUE;
        else
            set->in_terminal = XSET_B_UNSET;
    }
    else if ( !strcmp( var, "keep" ) )
    {
        if ( !strcmp( value, "1" ) )
            set->keep_terminal = XSET_B_TRUE;
        else
            set->keep_terminal = XSET_B_UNSET;
    }
    else if ( !strcmp( var, "scroll" ) )
    {
        if ( !strcmp( value, "1" ) )
            set->scroll_lock = XSET_B_TRUE;
        else
            set->scroll_lock = XSET_B_UNSET;
    }
    else if ( !strcmp( var, "disable" ) )
    {
        if ( !strcmp( value, "1" ) )
            set->disable = TRUE;
        else
            set->disable = FALSE;
    }
    else if ( !strcmp( var, "op" ) )
        set->opener = atoi( value );
    return set;
}

XSet* xset_set( const char* name, const char* var, const char* value )
{
    XSet* set = xset_get( name );
    if ( !set->lock || ( strcmp( var, "style" ) && strcmp( var, "desc" )
                        && strcmp( var, "title" ) && strcmp( var, "shared_key" ) ) )
        return xset_set_set( set, var, value );
    else
        return set;
}

XSet* xset_set_panel( int panel, const char* name, const char* var, const char* value )
{
    XSet* set;
    char* fullname = g_strdup_printf( "panel%d_%s", panel, name );
    set = xset_set( fullname, var, value );
    g_free( fullname );
    return set;
}

XSet* xset_set_panel_mode( int panel, const char* name, char mode,
                                      const char* var, const char* value )
{
    XSet* set;
    char* fullname = g_strdup_printf( "panel%d_%s%d", panel, name, mode );
    set = xset_set( fullname, var, value );
    g_free( fullname );
    return set;
}

XSet* xset_find_menu( const char* menu_name )
{
    XSet* set;
    GList* l;
    char* str;
    
    char* name = clean_label( menu_name, TRUE, FALSE );
    for ( l = xsets; l; l = l->next )
    {
        set = l->data;
        if ( !set->lock && set->menu_style == XSET_MENU_SUBMENU && set->child )
        {
            str = clean_label( set->menu_label, TRUE, FALSE );
            if ( !g_strcmp0( str, name ) )
            {
                g_free( str );
                g_free( name );
                return set;
            }
            g_free( str );
        }
    }
    g_free( name );
    return NULL;
}

gboolean xset_opener( DesktopWindow* desktop, PtkFileBrowser* file_browser,
                                                            char job )
{   // find an opener for job
    XSet* set, *mset, *open_all_set, *tset, *open_all_tset;
    GList* l, *ll;
    XSetContext* context = NULL;
    int context_action;
    gboolean found = FALSE;
    char pinned;

    for ( l = xsets; l; l = l->next )
    {
        if ( !((XSet*)l->data)->lock && ((XSet*)l->data)->opener == job &&
                                !((XSet*)l->data)->tool &&
                    ((XSet*)l->data)->menu_style != XSET_MENU_SUBMENU &&
                    ((XSet*)l->data)->menu_style != XSET_MENU_SEP )
        {
            if ( ((XSet*)l->data)->desc && 
                        !strcmp( ((XSet*)l->data)->desc, "@plugin@mirror@" ) )
            {
                // is a plugin mirror
                mset = (XSet*)l->data;
                set = xset_is( mset->shared_key );
                if ( !set )
                    continue;
            }
            else if ( ((XSet*)l->data)->plugin && ((XSet*)l->data)->shared_key )
            {
                // plugin with mirror - ignore to use mirror's context only
                continue;
            }
            else
                set = mset = (XSet*)l->data;
            
            if ( !context )
            {
                if ( !( context = xset_context_new() ) )
                    return FALSE;
                if ( file_browser )
                    main_context_fill( file_browser, context );
#ifdef DESKTOP_INTEGRATION
                else if ( desktop )
                    desktop_context_fill( desktop, context );
#endif
                else
                    return FALSE;

                if ( !context->valid )
                    return FALSE;

                // get mime type open_all_type set
                char* open_all_name = g_strdup( context->var[CONTEXT_MIME] );
                if ( !open_all_name )
                    open_all_name = g_strdup( "" );
                char* str = open_all_name;
                open_all_name = replace_string( str, "-", "_", FALSE );
                g_free( str );
                str = replace_string( open_all_name, " ", "", FALSE );
                g_free( open_all_name );
                open_all_name = g_strdup_printf( "open_all_type_%s", str );
                g_free( str );
                open_all_set = xset_is( open_all_name );
                g_free( open_all_name );                
            }

            // test context
            if ( mset->context )
            {
                context_action = xset_context_test( context, mset->context,
                                                             FALSE );
                if ( context_action == CONTEXT_HIDE ||
                                            context_action == CONTEXT_DISABLE )
                    continue;
            }
            
            // valid custom type?
            int cmd_type = atoi( set->x );
            if ( cmd_type != XSET_CMD_APP && cmd_type != XSET_CMD_LINE &&
                 cmd_type != XSET_CMD_SCRIPT )
                continue;

            // is set pinned to open_all_type for pre-context?
            pinned = 0;
            for ( ll = xsets; ll && !pinned; ll = ll->next )
            {
                if ( ((XSet*)ll->data)->next && 
                                g_str_has_prefix( ((XSet*)ll->data)->name,
                                                        "open_all_type_" ) )
                {
                    tset = open_all_tset = (XSet*)ll->data;
                    while ( tset->next )
                    {
                        if ( !strcmp( set->name, tset->next ) )
                        {
                            // found pinned to open_all_type
                            if ( open_all_tset == open_all_set )
                                // correct mime type
                                pinned = 2;
                            else
                                // wrong mime type
                                pinned = 1;
                            break;
                        }
                        tset = xset_is( tset->next );
                    }
                }
            }
            if ( pinned == 1 )
                continue;

            // valid
            found = TRUE;
            set->browser = file_browser;
            set->desktop = desktop;
            xset_menu_cb( NULL, set );  // also does custom activate
        }
    }
    return found;
}

void write_root_saver( FILE* file, const char* path, const char* name,
                                            const char* var, const char* value )
{
    if ( !value )
        return;
    
    char* save = g_strdup_printf( "%s-%s=%s", name, var, value );
    char* qsave = bash_quote( save );
    fprintf( file, "echo %s >> \"%s\"\n", qsave, path );
    g_free( save );
    g_free( qsave );
}

gboolean write_root_settings( FILE* file, const char* path )
{
    GList* l;
    XSet* set;

    if ( !file )
        return FALSE;

    fprintf( file, "\n# save root settings\nmkdir -p /etc/spacefm\necho -e '# SpaceFM As-Root Session File\\n\\n# THIS FILE IS NOT DESIGNED TO BE EDITED\\n\\n' > '%s'\n", path );

    for ( l = xsets ; l; l = l->next )
    {
        set = l->data;
        if ( set )
        {
            if ( !strcmp( set->name, "root_editor" ) 
                    || !strcmp( set->name, "dev_back_fsarc" )
                    || !strcmp( set->name, "dev_back_part" )
                    || !strcmp( set->name, "dev_rest_file" )
                    || !strcmp( set->name, "dev_root_check" )
                    || !strcmp( set->name, "dev_root_mount" )
                    || !strcmp( set->name, "dev_root_unmount" )
                    || !strcmp( set->name, "main_terminal" )
                    || !strncmp( set->name, "dev_fmt_", 8 )
                    || !strncmp( set->name, "label_cmd_", 8 ) )
            {
                write_root_saver( file, path, set->name, "s", set->s );
                write_root_saver( file, path, set->name, "x", set->x );
                write_root_saver( file, path, set->name, "y", set->y );
                if ( set->b != XSET_B_UNSET )
                    fprintf( file, "echo '%s-b=%d' >> \"%s\"\n",
                                                        set->name, set->b, path );
            }
        }
    }
    
    // create spacefm.conf
    if ( !g_file_test( "/etc/spacefm/spacefm.conf", G_FILE_TEST_EXISTS ) )
    {
        fprintf( file, "echo \"# spacefm.conf\" >> /etc/spacefm/spacefm.conf\n" );
        fprintf( file, "echo >> /etc/spacefm/spacefm.conf\n" );
        fprintf( file, "echo \"# tmp_dir should be a root-protected user-writable dir like /tmp\" >> /etc/spacefm/spacefm.conf\n" );
        fprintf( file, "echo \"# tmp_dir must NOT contain spaces or special chars - keep it simple\" >> /etc/spacefm/spacefm.conf\n" );
        fprintf( file, "echo \"# tmp_dir=/tmp\" >> /etc/spacefm/spacefm.conf\n" );
    }

    fprintf( file, "chmod -R go-w+rX /etc/spacefm\n\n" );
    return TRUE;
}

void read_root_settings()
{
    GList* l;
    XSet* set;
    FILE* file;
    char line[ 2048 ];
    
    if ( geteuid() == 0 )
        return;
    
    char* root_set_path= g_strdup_printf(
                    "/etc/spacefm/%s-as-root", g_get_user_name() );
    if ( !g_file_test( root_set_path, G_FILE_TEST_EXISTS ) )
    {
        g_free( root_set_path );
        root_set_path= g_strdup_printf(
                                    "/etc/spacefm/%d-as-root", geteuid() );
    }
    
    file = fopen( root_set_path, "r" );

    if ( !file )
    {
        if ( g_file_test( root_set_path, G_FILE_TEST_EXISTS ) )
            g_warning( _("Error reading root settings from /etc/spacefm/  Commands run as root may present a security risk") );
        else
            g_warning( _("No root settings found in /etc/spacefm/  Setting a root editor in Preferences should remove this warning on startup.   Otherwise commands run as root may present a security risk.") );
        g_free( root_set_path );
        return;
    }
    g_free( root_set_path );
    
    // clear settings
    for ( l = xsets ; l; l = l->next )
    {
        set = l->data;
        if ( set )
        {
            if ( !strcmp( set->name, "root_editor" ) 
                    || !strcmp( set->name, "dev_back_fsarc" )
                    || !strcmp( set->name, "dev_back_part" )
                    || !strcmp( set->name, "dev_rest_file" )
                    || !strcmp( set->name, "dev_root_check" )
                    || !strcmp( set->name, "dev_root_mount" )
                    || !strcmp( set->name, "dev_root_unmount" )
                    || !strncmp( set->name, "dev_fmt_", 8 )
                    || !strncmp( set->name, "label_cmd_", 8 ) )
            {
                if ( set->s ) { g_free( set->s ); set->s = NULL; }
                if ( set->x ) { g_free( set->x ); set->x = NULL; }
                if ( set->y ) { g_free( set->y ); set->y = NULL; }
                set->b = XSET_B_UNSET;
            }
        }
    }

    while ( fgets( line, sizeof( line ), file ) )
    {
        strtok( line, "\r\n" );
        if ( !line[ 0 ] )
            continue;
        xset_parse( line );
    }
    fclose( file );
}

void write_src_functions( FILE* file )
{
    fputs( "\nfm_randhex4()  # generate a four digit random hex number\n{\n    fm_rand1=$RANDOM\n    fm_rand2=$RANDOM\n    (( fm_rand = fm_rand1 + fm_rand2 ))\n    let \"fm_rand \%= 65536\"\n    fm_randhex=`printf \"\%04X\" $fm_rand | tr A-Z a-z`\n    if [ \"$fm_randhex\" = \"\" ]; then\n        fm_randhex=$RANDOM  # failsafe\n    fi\n}\n\nfm_new_tmp()\n{\n    fm_randhex4\n    fm_tmp1=\"$fm_tmp_dir/$$-$fm_randhex.tmp\"\n    fm_count1=0\n    while ! mkdir \"$fm_tmp1\" 2>/dev/null; do\n        fm_randhex4\n        fm_tmp1=\"$fm_tmp_dir/$$-$fm_randhex.tmp\"\n        if (( fm_count1++ > 1000 )); then\n            echo 'spacefm: error creating temporary directory' 1>&2\n          unset fm_tmp1 fm_randhex fm_count1\n            echo \"\"\n            return 1\n        fi\n    done\n    echo \"$fm_tmp1\"\n    unset fm_tmp1 fm_randhex fm_count1\n}\n\nfm_edit()\n{\n    spacefm -s set edit_file \"$1\"\n}\n\n", file );
}

XSetContext* xset_context_new()
{
    int i;
    if ( !xset_context )
    {
        xset_context = g_slice_new0( XSetContext );
        xset_context->valid = FALSE;
        for ( i = 0; i < G_N_ELEMENTS( xset_context->var ); i++ )
            xset_context->var[i] = NULL;
    }
    else
    {
        xset_context->valid = FALSE;
        for ( i = 0; i < G_N_ELEMENTS( xset_context->var ); i++ )
        {
            if ( xset_context->var[i] )
                g_free( xset_context->var[i] );
            xset_context->var[i] = NULL;
        }        
    }
    return xset_context;
}

const char* icon_stock_to_id( const char* name )
{
    if ( !name )
        return NULL;
    else if ( !strncmp( name, "gtk-", 4 ) )
        return name;
    else if ( !strncmp( name, "GTK_STOCK_", 10 ) )
    {
        const char* icontail = name + 10;
        const char* stockid;
        if ( !strcmp( icontail, "ABOUT" ) )
            stockid = GTK_STOCK_ABOUT;
        else if ( !strcmp( icontail, "ADD" ) )
            stockid = GTK_STOCK_ADD;
        else if ( !strcmp( icontail, "APPLY" ) )
            stockid = GTK_STOCK_APPLY;
        else if ( !strcmp( icontail, "BOLD" ) )
            stockid = GTK_STOCK_BOLD;
        else if ( !strcmp( icontail, "CANCEL" ) )
            stockid = GTK_STOCK_CANCEL;
        else if ( !strcmp( icontail, "CDROM" ) )
            stockid = GTK_STOCK_CDROM;
        else if ( !strcmp( icontail, "CLEAR" ) )
            stockid = GTK_STOCK_CLEAR;
        else if ( !strcmp( icontail, "CLOSE" ) )
            stockid = GTK_STOCK_CLOSE;
        else if ( !strcmp( icontail, "CONVERT" ) )
            stockid = GTK_STOCK_CONVERT;
        else if ( !strcmp( icontail, "CONNECT" ) )
            stockid = GTK_STOCK_CONNECT;
        else if ( !strcmp( icontail, "COPY" ) )
            stockid = GTK_STOCK_COPY;
        else if ( !strcmp( icontail, "CUT" ) )
            stockid = GTK_STOCK_CUT;
        else if ( !strcmp( icontail, "DELETE" ) )
            stockid = GTK_STOCK_DELETE;
        else if ( !strcmp( icontail, "DIALOG_ERROR" ) )
            stockid = GTK_STOCK_DIALOG_ERROR;
        else if ( !strcmp( icontail, "DIALOG_INFO" ) )
            stockid = GTK_STOCK_DIALOG_INFO;
        else if ( !strcmp( icontail, "DIALOG_QUESTION" ) )
            stockid = GTK_STOCK_DIALOG_QUESTION;
        else if ( !strcmp( icontail, "DIALOG_WARNING" ) )
            stockid = GTK_STOCK_DIALOG_WARNING;
        else if ( !strcmp( icontail, "DIRECTORY" ) )
            stockid = GTK_STOCK_DIRECTORY;
        else if ( !strcmp( icontail, "DISCARD" ) )
            stockid = GTK_STOCK_DISCARD;
        else if ( !strcmp( icontail, "DISCONNECT" ) )
            stockid = GTK_STOCK_DISCONNECT;
        else if ( !strcmp( icontail, "DND" ) )
            stockid = GTK_STOCK_DND;
        else if ( !strcmp( icontail, "DND_MULTIPLE" ) )
            stockid = GTK_STOCK_DND_MULTIPLE;
        else if ( !strcmp( icontail, "EDIT" ) )
            stockid = GTK_STOCK_EDIT;
        else if ( !strcmp( icontail, "EXECUTE" ) )
            stockid = GTK_STOCK_EXECUTE;
        else if ( !strcmp( icontail, "FILE" ) )
            stockid = GTK_STOCK_FILE;
        else if ( !strcmp( icontail, "FIND" ) )
            stockid = GTK_STOCK_FIND;
        else if ( !strcmp( icontail, "FIND_AND_REPLACE" ) )
            stockid = GTK_STOCK_FIND_AND_REPLACE;
        else if ( !strcmp( icontail, "FLOPPY" ) )
            stockid = GTK_STOCK_FLOPPY;
        else if ( !strcmp( icontail, "FULLSCREEN" ) )
            stockid = GTK_STOCK_FULLSCREEN;
        else if ( !strcmp( icontail, "GOTO_BOTTOM" ) )
            stockid = GTK_STOCK_GOTO_BOTTOM;
        else if ( !strcmp( icontail, "GOTO_FIRST" ) )
            stockid = GTK_STOCK_GOTO_FIRST;
        else if ( !strcmp( icontail, "GOTO_LAST" ) )
            stockid = GTK_STOCK_GOTO_LAST;
        else if ( !strcmp( icontail, "GOTO_TOP" ) )
            stockid = GTK_STOCK_GOTO_TOP;
        else if ( !strcmp( icontail, "GO_BACK" ) )
            stockid = GTK_STOCK_GO_BACK;
        else if ( !strcmp( icontail, "GO_DOWN" ) )
            stockid = GTK_STOCK_GO_DOWN;
        else if ( !strcmp( icontail, "GO_FORWARD" ) )
            stockid = GTK_STOCK_GO_FORWARD;
        else if ( !strcmp( icontail, "GO_UP" ) )
            stockid = GTK_STOCK_GO_UP;
        else if ( !strcmp( icontail, "HARDDISK" ) )
            stockid = GTK_STOCK_HARDDISK;
        else if ( !strcmp( icontail, "HELP" ) )
            stockid = GTK_STOCK_HELP;
        else if ( !strcmp( icontail, "HOME" ) )
            stockid = GTK_STOCK_HOME;
        else if ( !strcmp( icontail, "INDENT" ) )
            stockid = GTK_STOCK_INDENT;
        else if ( !strcmp( icontail, "INDEX" ) )
            stockid = GTK_STOCK_INDEX;
        else if ( !strcmp( icontail, "INFO" ) )
            stockid = GTK_STOCK_INFO;
        else if ( !strcmp( icontail, "ITALIC" ) )
            stockid = GTK_STOCK_ITALIC;
        else if ( !strcmp( icontail, "JUMP_TO" ) )
            stockid = GTK_STOCK_JUMP_TO;
        else if ( !strcmp( icontail, "MEDIA_FORWARD" ) )
            stockid = GTK_STOCK_MEDIA_FORWARD;
        else if ( !strcmp( icontail, "MEDIA_NEXT" ) )
            stockid = GTK_STOCK_MEDIA_NEXT;
        else if ( !strcmp( icontail, "MEDIA_PAUSE" ) )
            stockid = GTK_STOCK_MEDIA_PAUSE;
        else if ( !strcmp( icontail, "MEDIA_PLAY" ) )
            stockid = GTK_STOCK_MEDIA_PLAY;
        else if ( !strcmp( icontail, "MEDIA_PREVIOUS" ) )
            stockid = GTK_STOCK_MEDIA_PREVIOUS;
        else if ( !strcmp( icontail, "MEDIA_RECORD" ) )
            stockid = GTK_STOCK_MEDIA_RECORD;
        else if ( !strcmp( icontail, "MEDIA_REWIND" ) )
            stockid = GTK_STOCK_MEDIA_REWIND;
        else if ( !strcmp( icontail, "MEDIA_STOP" ) )
            stockid = GTK_STOCK_MEDIA_STOP;
        else if ( !strcmp( icontail, "NETWORK" ) )
            stockid = GTK_STOCK_NETWORK;
        else if ( !strcmp( icontail, "NEW" ) )
            stockid = GTK_STOCK_NEW;
        else if ( !strcmp( icontail, "NO" ) )
            stockid = GTK_STOCK_NO;
        else if ( !strcmp( icontail, "OK" ) )
            stockid = GTK_STOCK_OK;
        else if ( !strcmp( icontail, "OPEN" ) )
            stockid = GTK_STOCK_OPEN;
        else if ( !strcmp( icontail, "PAGE_SETUP" ) )
            stockid = GTK_STOCK_PAGE_SETUP;
        else if ( !strcmp( icontail, "PASTE" ) )
            stockid = GTK_STOCK_PASTE;
        else if ( !strcmp( icontail, "PREFERENCES" ) )
            stockid = GTK_STOCK_PREFERENCES;
        else if ( !strcmp( icontail, "PRINT" ) )
            stockid = GTK_STOCK_PRINT;
        else if ( !strcmp( icontail, "PROPERTIES" ) )
            stockid = GTK_STOCK_PROPERTIES;
        else if ( !strcmp( icontail, "QUIT" ) )
            stockid = GTK_STOCK_QUIT;
        else if ( !strcmp( icontail, "REDO" ) )
            stockid = GTK_STOCK_REDO;
        else if ( !strcmp( icontail, "REFRESH" ) )
            stockid = GTK_STOCK_REFRESH;
        else if ( !strcmp( icontail, "REMOVE" ) )
            stockid = GTK_STOCK_REMOVE;
        else if ( !strcmp( icontail, "REVERT_TO_SAVED" ) )
            stockid = GTK_STOCK_REVERT_TO_SAVED;
        else if ( !strcmp( icontail, "SAVE" ) )
            stockid = GTK_STOCK_SAVE;
        else if ( !strcmp( icontail, "SAVE_AS" ) )
            stockid = GTK_STOCK_SAVE_AS;
        else if ( !strcmp( icontail, "SELECT_ALL" ) )
            stockid = GTK_STOCK_SELECT_ALL;
        else if ( !strcmp( icontail, "SELECT_COLOR" ) )
            stockid = GTK_STOCK_SELECT_COLOR;
        else if ( !strcmp( icontail, "SELECT_FONT" ) )
            stockid = GTK_STOCK_SELECT_FONT;
        else if ( !strcmp( icontail, "SORT_ASCENDING" ) )
            stockid = GTK_STOCK_SORT_ASCENDING;
        else if ( !strcmp( icontail, "SORT_DESCENDING" ) )
            stockid = GTK_STOCK_SORT_DESCENDING;
        else if ( !strcmp( icontail, "SPELL_CHECK" ) )
            stockid = GTK_STOCK_SPELL_CHECK;
        else if ( !strcmp( icontail, "STOP" ) )
            stockid = GTK_STOCK_STOP;
        else if ( !strcmp( icontail, "STRIKETHROUGH" ) )
            stockid = GTK_STOCK_STRIKETHROUGH;
        else if ( !strcmp( icontail, "UNDELETE" ) )
            stockid = GTK_STOCK_UNDELETE;
        else if ( !strcmp( icontail, "UNDERLINE" ) )
            stockid = GTK_STOCK_UNDERLINE;
        else if ( !strcmp( icontail, "UNDO" ) )
            stockid = GTK_STOCK_UNDO;
        else if ( !strcmp( icontail, "UNINDENT" ) )
            stockid = GTK_STOCK_UNINDENT;
        else if ( !strcmp( icontail, "YES" ) )
            stockid = GTK_STOCK_YES;
        else if ( !strcmp( icontail, "ZOOM_100" ) )
            stockid = GTK_STOCK_ZOOM_100;
        else if ( !strcmp( icontail, "ZOOM_FIT" ) )
            stockid = GTK_STOCK_ZOOM_FIT;
        else if ( !strcmp( icontail, "ZOOM_IN" ) )
            stockid = GTK_STOCK_ZOOM_IN;
        else if ( !strcmp( icontail, "ZOOM_OUT" ) )
            stockid = GTK_STOCK_ZOOM_OUT;
        else if ( !strcmp( icontail, "DIALOG_AUTHENTICATION" ) )
            stockid = GTK_STOCK_DIALOG_AUTHENTICATION;
        else
            stockid = NULL;
        return stockid;
    }
    return NULL;
}

GtkWidget* xset_get_image( const char* icon, int icon_size )
{
/*
    GTK_ICON_SIZE_MENU,
    GTK_ICON_SIZE_SMALL_TOOLBAR,
    GTK_ICON_SIZE_LARGE_TOOLBAR,
    GTK_ICON_SIZE_BUTTON,
    GTK_ICON_SIZE_DND,
    GTK_ICON_SIZE_DIALOG
*/
    GtkWidget* image = NULL;
    const char* stockid;

    if ( !( icon && icon[0] ) )
        return NULL;
    if ( !icon_size )
        icon_size = GTK_ICON_SIZE_MENU;

    if ( stockid = icon_stock_to_id( icon ) )
        image = gtk_image_new_from_stock( stockid, icon_size );
    else if ( icon[0] == '/' )
        image = gtk_image_new_from_file( icon );
    else
        image = gtk_image_new_from_icon_name( icon, icon_size );
    return image;
}

void xset_add_menu( DesktopWindow* desktop, PtkFileBrowser* file_browser,
                    GtkWidget* menu, GtkAccelGroup *accel_group, char* elements )
{
    char* space;
    XSet* set;
    
    if ( !elements )
        return;

    while ( elements[0] == ' ' )
        elements++;

    while ( elements && elements[0] != '\0' )
    {
        space = strchr( elements, ' ' );
        if ( space )
            space[0] = '\0';
        set = xset_get( elements );
        if ( space )
            space[0] = ' ';
        elements = space;
        xset_add_menuitem( desktop, file_browser, menu, accel_group, set );
        if ( elements )
        {
            while ( elements[0] == ' ' )
                elements++;
        }
    }
}

GtkWidget* xset_new_menuitem( const char* label, const char* icon )
{
    GtkWidget* image = NULL;
    GtkWidget* item;
    
    if ( !icon || icon[0] == '\0' )
        return gtk_image_menu_item_new_with_mnemonic( label );
    image = xset_get_image( icon, GTK_ICON_SIZE_MENU );
    if ( !image )
        return gtk_image_menu_item_new_with_mnemonic( label );
    item = gtk_image_menu_item_new_with_mnemonic( label );
    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM(item), image );
    return item;
}

/*
gboolean xset_design_event_cb (GtkWidget *widget, GdkEvent  *gdk_event , XSet* set)
{
    GdkEventButton* event = (GdkEventButton*)gdk_event;
printf("xset_design_event_cb type=%d button=%d state=%d\n", event->type, event->button, event->state );
    if ( event->type == 10 )
    {
        //return TRUE;
    }
    else if ( event->type == GDK_BUTTON_PRESS )
    {
//        printf("    button_press\n");
        if ( ((GdkEventButton*)event)->state & GDK_CONTROL_MASK
                                || ((GdkEventButton*)event)->button == 2 )
        {
            printf("    DESIGN\n");
            return TRUE;
        }
    }
    return FALSE;  // pass through    
}
*/

/*
void xset_design_activate_item( GtkMenuItem* item, XSet* set )
{
    if ( design_mode )
    {
        g_signal_stop_emission_by_name( item, "activate" );
        //g_signal_stop_emission_by_name( item, "activate-item" );
        printf("design_mode_activate\n");
    }
}
*/

GtkWidget* xset_add_menuitem( DesktopWindow* desktop, PtkFileBrowser* file_browser,
                                    GtkWidget* menu, GtkAccelGroup *accel_group,
                                    XSet* set )
{
    GtkWidget* item = NULL;
    GtkWidget* submenu;
    XSet* keyset;
    XSet* set_next;
    char* icon_name = NULL;
    char* context = NULL;
    int context_action = CONTEXT_SHOW;
    XSet* mset;
    char* icon_file = NULL;
//printf("xset_add_menuitem %s\n", set->name );

    // plugin?
    mset = xset_get_plugin_mirror( set );
    if ( set->plugin && set->shared_key )
    {
        icon_name = mset->icon;
        context = mset->context;
    }
    if ( !icon_name )
        icon_name = set->icon;
    if ( !icon_name )
    {
        if ( set->plugin )
            icon_file = g_build_filename( set->plug_dir, set->plug_name, "icon",
                                                                        NULL );
        else
            icon_file = g_build_filename( settings_config_dir, "scripts",
                                                        set->name, "icon", NULL );
        if ( !g_file_test( icon_file, G_FILE_TEST_EXISTS ) )
        {
            g_free( icon_file );
            icon_file = NULL;
        }
        else
            icon_name = icon_file;
    }
    if ( !context )
        context = set->context;

    // context?
    if ( context && !set->tool && xset_context && xset_context->valid && 
                                                !xset_get_b( "context_dlg" ) )
        context_action = xset_context_test( xset_context, context, set->disable );
    
    if ( context_action != CONTEXT_HIDE )
    {
        if ( set->tool && set->menu_style != XSET_MENU_SUBMENU )
        {
            //item = xset_new_menuitem( set->menu_label, icon_name );
        }
        else if ( set->menu_style )
        {
            if ( set->menu_style == XSET_MENU_CHECK &&
                        !( !set->lock && set->x &&
                           ( atoi( set->x ) > XSET_CMD_SCRIPT ) ) ) // app or book
            {
                item = gtk_check_menu_item_new_with_mnemonic( set->menu_label );
                gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( item ),
                                                        mset->b == XSET_B_TRUE );
            }
            else if ( set->menu_style == XSET_MENU_RADIO )
            {
                XSet* set_radio;
                if ( set->ob2_data )
                    set_radio = (XSet*)set->ob2_data;
                else
                    set_radio = set;
                item = gtk_radio_menu_item_new_with_mnemonic( 
                                                    (GSList*)set_radio->ob2_data,
                                                    set->menu_label );
                set_radio->ob2_data = (gpointer)gtk_radio_menu_item_get_group(
                                                    GTK_RADIO_MENU_ITEM( item ) );
                gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( item ),
                                                        mset->b == XSET_B_TRUE );
            }
            else if ( set->menu_style == XSET_MENU_SUBMENU )
            {
                submenu = gtk_menu_new();
                item = xset_new_menuitem( set->menu_label, icon_name );
                gtk_menu_item_set_submenu( GTK_MENU_ITEM( item ), submenu );
                g_signal_connect( submenu, "key-press-event",
                                  G_CALLBACK( xset_menu_keypress ), NULL );
                if ( set->lock )
                    xset_add_menu( desktop, file_browser, submenu, accel_group, set->desc );
                else if ( set->child )
                {
                    set_next = xset_get( set->child );
                    xset_add_menuitem( desktop, file_browser, submenu, accel_group, set_next );
                    GList* l = gtk_container_get_children( GTK_CONTAINER( submenu ) );
                    if ( l )
                        g_list_free( l );
                    else
                    {
                        // Nothing was added to the menu (all items likely have
                        // invisible context) so destroy (hide) - issue #215
                        gtk_widget_destroy( item );
                        goto _next_item;
                    }
                }
            }
            else if ( set->menu_style == XSET_MENU_SEP )
            {
                item = gtk_separator_menu_item_new();
            }
        }
        if ( !item )
        {
            if ( !set->lock && set->z &&
                        ( !( set->menu_label && set->menu_label[0] )
                          || !( icon_name && icon_name[0] ) ) &&
                                    set->x && atoi( set->x ) == 2 &&
                                    g_str_has_suffix( set->z, ".desktop" ) )
            {
                // Application - get name and/or icon
                const char* menu_label = set->menu_label;
                VFSAppDesktop* app = vfs_app_desktop_new( set->z );
                if ( app )
                {
                    if ( !( menu_label && menu_label[0] ) )
                        menu_label = vfs_app_desktop_get_disp_name( app );
                    if ( !( icon_name && icon_name[0] ) )
                        icon_name = (char*)vfs_app_desktop_get_icon_name( app );
                }
                item = xset_new_menuitem( menu_label && menu_label[0] ?
                                            menu_label : set->z,
                                            icon_name );
                if ( app )
                    vfs_app_desktop_unref( app );
            }
            else if ( !set->lock && set->z &&
                                    !( icon_name && icon_name[0] ) &&
                                    set->x && atoi( set->x ) == 3 )
            {
                // Bookmark - get default icon
                item = xset_new_menuitem( 
                            set->menu_label && set->menu_label[0] ?
                            set->menu_label : set->z, NULL );
                GtkWidget* folder_image = NULL;
                XSet* book_icon_set = xset_get( "book_icon" );
                if ( book_icon_set->icon )
                    folder_image = xset_get_image( book_icon_set->icon,
                                                        GTK_ICON_SIZE_MENU );
                if ( !folder_image )
                    folder_image = xset_get_image( "gtk-directory",
                                                        GTK_ICON_SIZE_MENU );
                if ( folder_image )
                    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM( item ),
                                                        folder_image );
            }
            else if ( !set->lock && !( set->menu_label && set->menu_label[0] )
                        && set->z && set->x && atoi( set->x ) > 1 )
                // An app or bookmark with no name
                item = xset_new_menuitem( set->z, icon_name );
            else
                item = xset_new_menuitem( set->menu_label, icon_name );
        }
            
        if ( set->tool == XSET_B_TRUE )
        {
            char* ml = g_strdup_printf( "%s *",
                            gtk_menu_item_get_label( GTK_MENU_ITEM( item ) ) );
            gtk_menu_item_set_label( GTK_MENU_ITEM( item ), ml );
            g_free( ml );
        }
        
        set->desktop = desktop;
        set->browser = file_browser;
        g_object_set_data( G_OBJECT( item ), "menu", menu );
        g_object_set_data( G_OBJECT( item ), "set", set );

        if ( set->ob1 )
            g_object_set_data( G_OBJECT( item ), set->ob1, set->ob1_data );
        if ( set->menu_style != XSET_MENU_RADIO && set->ob2 )
            g_object_set_data( G_OBJECT( item ), set->ob2, set->ob2_data );

        if ( set->menu_style < XSET_MENU_SUBMENU )
        {
            // activate callback
            if ( !set->cb_func || set->menu_style )
            {
                // use xset menu callback
                //if ( !design_mode )
                //{
                    g_signal_connect( item, "activate", G_CALLBACK( xset_menu_cb ), set );
                    g_object_set_data( G_OBJECT(item), "cb_func", set->cb_func );
                    g_object_set_data( G_OBJECT(item), "cb_data", set->cb_data );
                //}
            }
            else if ( set->cb_func )
            {
                // use custom callback directly
                //if ( !design_mode )
                    g_signal_connect( item, "activate", G_CALLBACK( set->cb_func ), set->cb_data );
            }
        
            // key accel
            if ( set->shared_key )
                keyset = xset_get( set->shared_key );
            else
                keyset = set;
            if ( keyset->key > 0 && accel_group )
                gtk_widget_add_accelerator( item, "activate", accel_group,
                                        keyset->key, keyset->keymod, GTK_ACCEL_VISIBLE);
        }
        // design mode callback
        g_signal_connect( item, "button-press-event", G_CALLBACK( xset_design_cb ), set );
        g_signal_connect( item, "button-release-event", G_CALLBACK( xset_design_cb ), set );

        gtk_widget_set_sensitive( item, context_action != CONTEXT_DISABLE &&
                                                                !set->disable );
        gtk_menu_shell_append( GTK_MENU_SHELL(menu), item );
    }

_next_item:
    if ( icon_file )
        g_free( icon_file );
        
    // next item
    if ( set->next )
    {
        set_next = xset_get( set->next );
        xset_add_menuitem( desktop, file_browser, menu, accel_group, set_next );
    }
    return item;
}

char* xset_custom_get_script( XSet* set, gboolean create )
{
    char* path;
    char* old_path;
    char* cscript;

    if ( strncmp( set->name, "cstm_", 5 ) || ( create && set->plugin ) )
        return NULL;

    if ( create )
    {
        path = g_build_filename( settings_config_dir, "scripts", set->name, NULL );
        if ( !g_file_test( path, G_FILE_TEST_EXISTS ) )
        {
            g_mkdir_with_parents( path, 0700 );
            chmod( path, 0700 );
        }
        g_free( path );
    }

    if ( set->plugin )
    {
        path = g_build_filename( set->plug_dir, set->plug_name, "exec.sh", NULL );
    }
    else
    {
        path = g_build_filename( settings_config_dir, "scripts", set->name,
                                                                "exec.sh", NULL );
    }

    if ( !g_file_test( path, G_FILE_TEST_EXISTS ) )
    {
        // backwards compatible < 0.7.0
        if ( set->plugin )
        {
            cscript = g_strdup_printf( "%s.sh", set->plug_name );
            old_path = g_build_filename( set->plug_dir, "files", cscript, NULL );
            g_free( cscript );
            g_free( path );
            return old_path;
        }
        else
        {
            cscript = g_strdup_printf( "%s.sh", set->name );
            old_path = g_build_filename( settings_config_dir, "scripts",
                                                            cscript, NULL );
        }
        g_free( cscript );
        if ( g_file_test( old_path, G_FILE_TEST_EXISTS ) )
        {
            // copy old location to new
            char* new_dir = g_build_filename( settings_config_dir, "scripts",
                                                                set->name, NULL );
            if ( !g_file_test( new_dir, G_FILE_TEST_EXISTS ) )
            {
                g_mkdir_with_parents( new_dir, 0700 );
                chmod( new_dir, 0700 );
            }
            g_free( new_dir );
            xset_copy_file( old_path, path );
            chmod( path, S_IRUSR | S_IWUSR | S_IXUSR );
        }
        g_free( old_path );
    }

    if ( create && !g_file_test( path, G_FILE_TEST_EXISTS ) )
    {
        old_path = g_build_filename( settings_config_dir, "scripts",
                                                        "default-script", NULL );
        if ( g_file_test( old_path, G_FILE_TEST_EXISTS ) )
            xset_copy_file( old_path, path );
        else
        {
            FILE* file;
            int i;
            const char* script_default_head = "#!/bin/bash\n$fm_import    # import file manager variables (scroll down for info)\n#\n# Enter your commands here:     ( then save this file )\n";
            const char* script_default_tail = "exit $?\n# Example variables available for use: (imported by $fm_import)\n# These variables represent the state of the file manager when command is run.\n# These variables can also be used in command lines and in the Path Bar.\n\n# \"${fm_files[@]}\"          selected files              ( same as %F )\n# \"$fm_file\"                first selected file         ( same as %f )\n# \"${fm_files[2]}\"          third selected file\n\n# \"${fm_filenames[@]}\"      selected filenames          ( same as %N )\n# \"$fm_filename\"            first selected filename     ( same as %n )\n\n# \"$fm_pwd\"                 current directory           ( same as %d )\n# \"${fm_pwd_tab[4]}\"        current directory of tab 4\n# $fm_panel                 current panel number (1-4)\n# $fm_tab                   current tab number\n\n# \"${fm_panel3_files[@]}\"   selected files in panel 3\n# \"${fm_pwd_panel[3]}\"      current directory in panel 3\n# \"${fm_pwd_panel3_tab[2]}\" current directory in panel 3 tab 2\n# ${fm_tab_panel[3]}        current tab number in panel 3\n\n# \"${fm_desktop_files[@]}\"  selected files on desktop (when run from desktop)\n# \"$fm_desktop_pwd\"         desktop directory (eg '/home/user/Desktop')\n\n# \"$fm_device\"              selected device (eg /dev/sr0)  ( same as %v )\n# \"$fm_device_udi\"          device ID\n# \"$fm_device_mount_point\"  device mount point if mounted (eg /media/dvd) (%m)\n# \"$fm_device_label\"        device volume label            ( same as %l )\n# \"$fm_device_fstype\"       device fs_type (eg vfat)\n# \"$fm_device_size\"         device volume size in bytes\n# \"$fm_device_display_name\" device display name\n# \"$fm_device_icon\"         icon currently shown for this device\n# $fm_device_is_mounted     device is mounted (0=no or 1=yes)\n# $fm_device_is_optical     device is an optical drive (0 or 1)\n# $fm_device_is_table       a partition table (usually a whole device)\n# $fm_device_is_floppy      device is a floppy drive (0 or 1)\n# $fm_device_is_removable   device appears to be removable (0 or 1)\n# $fm_device_is_audiocd     optical device contains an audio CD (0 or 1)\n# $fm_device_is_dvd         optical device contains a DVD (0 or 1)\n# $fm_device_is_blank       device contains blank media (0 or 1)\n# $fm_device_is_mountable   device APPEARS to be mountable (0 or 1)\n# $fm_device_nopolicy       policy_noauto set (no automount) (0 or 1)\n\n# \"$fm_panel3_device\"       panel 3 selected device (eg /dev/sdd1)\n# \"$fm_panel3_device_udi\"   panel 3 device ID\n# ...                       (all these are the same as above for each panel)\n\n# \"fm_bookmark\"             selected bookmark directory     ( same as %b )\n# \"fm_panel3_bookmark\"      panel 3 selected bookmark directory\n\n# \"fm_task_type\"            currently SELECTED task type (eg 'run','copy')\n# \"fm_task_name\"            selected task name (custom menu item name)\n# \"fm_task_pwd\"             selected task working directory ( same as %t )\n# \"fm_task_pid\"             selected task pid               ( same as %p )\n# \"fm_task_command\"         selected task command\n# \"fm_task_id\"              selected task id\n# \"fm_task_window\"          selected task window id\n\n# \"$fm_command\"             current command\n# \"$fm_value\"               menu item value             ( same as %a )\n# \"$fm_user\"                original user who ran this command\n# \"$fm_my_task\"             current task's id  (see 'spacefm -s help')\n# \"$fm_my_window\"           current task's window id\n# \"$fm_cmd_name\"            menu name of current command\n# \"$fm_cmd_dir\"             command files directory (for read only)\n# \"$fm_cmd_data\"            command data directory (must create)\n#                                 To create:   mkdir -p \"$fm_cmd_data\"\n# \"$fm_plugin_dir\"          top plugin directory\n# tmp=\"$(fm_new_tmp)\"       makes new temp directory (destroy when done)\n#                                 To destroy:  rm -rf \"$tmp\"\n# fm_edit \"FILE\"            open FILE in user's configured editor\n\n# $fm_import                command to import above variables (this\n#                           variable is exported so you can use it in any\n#                           script run from this script)\n\n\n# Script Example 1:\n\n#   # show MD5 sums of selected files\n#   md5sum \"${fm_files[@]}\"\n\n\n# Script Example 2:\n\n#   # Show a confirmation dialog using SpaceFM Dialog:\n#   # http://ignorantguru.github.io/spacefm/spacefm-manual-en.html#dialog\n#   # Use QUOTED eval to read variables output by SpaceFM Dialog:\n#   eval \"`spacefm -g --label \"Are you sure?\" --button yes --button no`\"\n#   if [[ \"$dialog_pressed\" == \"button1\" ]]; then\n#       echo \"User pressed Yes - take some action\"\n#   else\n#       echo \"User did NOT press Yes - abort\"\n#   fi\n\n\n# Script Example 3:\n\n#   # Build list of filenames in panel 4:\n#   i=0\n#   for f in \"${fm_panel4_files[@]}\"; do\n#       panel4_names[$i]=\"$(basename \"$f\")\"\n#       (( i++ ))\n#   done\n#   echo \"${panel4_names[@]}\"\n\n\n# Script Example 4:\n\n#   # Copy selected files to panel 2\n#      # make sure panel 2 is visible ?\n#      # and files are selected ?\n#      # and current panel isn't 2 ?\n#   if [ \"${fm_pwd_panel[2]}\" != \"\" ] \\\n#               && [ \"${fm_files[0]}\" != \"\" ] \\\n#               && [ \"$fm_panel\" != 2 ]; then\n#       cp \"${fm_files[@]}\" \"${fm_pwd_panel[2]}\"\n#   else\n#       echo \"Can't copy to panel 2\"\n#       exit 1    # shows error if 'Popup Error' enabled\n#   fi\n\n\n# Script Example 5:\n\n#   # Keep current time in task manager list Item column\n#   # See http://ignorantguru.github.io/spacefm/spacefm-manual-en.html#sockets\n#   while (( 1 )); do\n#       sleep 0.7\n#       spacefm -s set-task $fm_my_task item \"$(date)\"\n#   done\n\n\n# Bash Scripting Guide:  http://www.tldp.org/LDP/abs/html/index.html\n\n# NOTE: Additional variables or examples may be available in future versions.\n#       To see the latest list, create a new command script or see:\n#       http://ignorantguru.github.io/spacefm/spacefm-manual-en.html#exvar\n\n";
            file = fopen( path, "w" );

            if ( file )
            {
                // write default script
                fputs( script_default_head, file );
                for ( i = 0; i < 14; i++ )
                    fputs( "\n", file );
                fputs( script_default_tail, file );                
                fclose( file );
            }
        }
        chmod( path, S_IRUSR | S_IWUSR | S_IXUSR );
        g_free( old_path );
    }
    return path;
}

char* xset_custom_get_help( XSet* set )
{
    char* dir;
    char* path;
    
    if ( !set || ( set && strncmp( set->name, "cstm_", 5 ) ) )
        return NULL;

    if ( set->plugin )
        dir = g_build_filename( set->plug_dir, set->plug_name, NULL );
    else
    {
        dir = g_build_filename( settings_config_dir, "scripts", set->name, NULL );
        if ( !g_file_test( dir, G_FILE_TEST_EXISTS ) )
        {
            g_mkdir_with_parents( dir, 0700 );
            chmod( dir, 0700 );
        }
    }
    
    char* names[] = { "README", "readme", "README.TXT", "README.txt", "readme.txt", "README.MKD", "README.mkd", "readme.mkd" };
    int i;
    for ( i = 0; i < G_N_ELEMENTS( names ); ++i )
    {
        path = g_build_filename( dir, names[i], NULL );
        if ( g_file_test( path, G_FILE_TEST_EXISTS ) )
            break;
        g_free( path );
        path = NULL;
    }
    
    if ( !path )
    {
        if ( set->plugin )
        {
            g_free( dir );
            return NULL;
        }

        // create
        path = g_build_filename( dir, names[0], NULL );
        FILE* file = fopen( path, "w" );
        if ( file )
        {
            // write default readme
            fputs( "README\n------\n\nFill this text file with detailed information about this command.  For\ncontext-sensitive help within SpaceFM, this file must be named README,\nREADME.txt, or README.mkd.\n\nIf you plan to distribute this command as a plugin, the following information\nis recommended:\n\n\nCommand Name:\n\nRelease Version and Date:\n\nPlugin Homepage or Download Link:\n\nAuthor's Contact Information or Feedback Instructions:\n\nDependencies or Requirements:\n\nDescription:\n\nInstructions For Use:\n\nCopyright and License Information:\n\n    Copyright (C) YEAR AUTHOR <EMAIL>\n\n    This program is free software: you can redistribute it and/or modify\n    it under the terms of the GNU General Public License as published by\n    the Free Software Foundation, either version 2 of the License, or\n    (at your option) any later version.\n\n    This program is distributed in the hope that it will be useful,\n    but WITHOUT ANY WARRANTY; without even the implied warranty of\n    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n    GNU General Public License for more details.\n\n    You should have received a copy of the GNU General Public License\n    along with this program.  If not, see <http://www.gnu.org/licenses/>.\n\n", file );
            fclose( file );
        }
        chmod( path, S_IRUSR | S_IWUSR );
    }
    
    g_free( dir );
    if ( g_file_test( path, G_FILE_TEST_EXISTS ) )
        return path;
    g_free( path );
    return NULL;
}

gboolean xset_copy_file( char* src, char* dest )
{   // overwrites!
    int inF, ouF, bytes;
    char line[ 1024 ];

    if ( !g_file_test( src, G_FILE_TEST_EXISTS ) )
        return FALSE;

    if ( ( inF = open( src, O_RDONLY ) ) == -1 )
        return FALSE;
     
    unlink( dest );
    if ( ( ouF = open( dest, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR ) ) == -1 )
    {
        close(inF);            
        return FALSE;
    }
    
    while ( ( bytes = read( inF, line, sizeof( line ) ) ) > 0 )
    {
        if ( write(ouF, line, bytes) <= 0 )
        {
            close(inF);
            close(ouF);
            unlink( dest );
            return FALSE;
        }
    }
    close(inF);
    close(ouF);
    return TRUE;
}

char* xset_custom_new_name()
{
    char* hex8 = NULL;
    char* setname;
    char* path1;
    char* path2;
    char* path3;
    char* str;

    do
    {
        if ( hex8 )
            g_free( hex8 );
        hex8 = randhex8();
        setname = g_strdup_printf( "cstm_%s", hex8 );
        if ( xset_is( setname ) )
        {
            g_free( setname );
            setname = NULL;
        }
        else
        {
            path1 = g_build_filename( settings_config_dir, "scripts", setname, NULL );
            str = g_strdup_printf( "%s.sh", setname );  //backwards compat < 0.7.0
            path2 = g_build_filename( settings_config_dir, "scripts", str, NULL );
            g_free( str );
            path3 = g_build_filename( settings_config_dir, "plugin-data", setname, NULL );
            if ( g_file_test( path1, G_FILE_TEST_EXISTS )
                                || g_file_test( path2, G_FILE_TEST_EXISTS )
                                || g_file_test( path3, G_FILE_TEST_EXISTS ) )
            {
                g_free( setname );
                setname = NULL;
            }
            g_free( path1 );
            g_free( path2 );
            g_free( path3 );
        }
    }
    while ( !setname ); 
    return setname;
}

void xset_custom_copy_files( XSet* src, XSet* dest )
{
    char* cscript;
    char* path_src;
    char* path_dest;
    char* command = NULL;
    char* stdout = NULL;
    char* stderr = NULL;
    char* msg;
    gboolean ret;
    gint exit_status;
    
//printf("xset_custom_copy_files( %s, %s )\n", src->name, dest->name );

    // copy command dir
    
    // do this for backwards compat - will copy old script
    cscript = xset_custom_get_script( src, FALSE );
    if ( cscript )
        g_free( cscript );

    if ( src->plugin )
        path_src = g_build_filename( src->plug_dir, src->plug_name, NULL );
    else
        path_src = g_build_filename( settings_config_dir, "scripts", src->name, NULL );
//printf("    path_src=%s\n", path_src );

    if ( !g_file_test( path_src, G_FILE_TEST_EXISTS ) )
    {
//printf("    path_src !EXISTS\n");
        if ( !src->plugin )
        {
            command = NULL;
        }
        else
        {
            // plugin backwards compat ?
            cscript = xset_custom_get_script( src, FALSE );
            if ( cscript && g_file_test( cscript, G_FILE_TEST_EXISTS ) )
            {
                path_dest = g_build_filename( settings_config_dir, "scripts",
                                                                dest->name, NULL );
                g_mkdir_with_parents( path_dest, 0700 );
                chmod( path_dest, 0700 );
                g_free( path_dest );
                path_dest = g_build_filename( settings_config_dir, "scripts",
                                                        dest->name, "exec.sh", NULL );
                command = g_strdup_printf( "cp %s %s", cscript, path_dest );
                g_free( cscript );
            }
            else
            {
                if ( cscript )
                    g_free( cscript );
                command = NULL;
            }            
        }
    }
    else
    {
//printf("    path_src EXISTS\n");
        path_dest = g_build_filename( settings_config_dir, "scripts", NULL );
        g_mkdir_with_parents( path_dest, 0700 );
        chmod( path_dest, 0700 );
        g_free( path_dest );
        path_dest = g_build_filename( settings_config_dir, "scripts", dest->name, NULL );
        command = g_strdup_printf( "cp -a %s %s", path_src, path_dest );
    }
    g_free( path_src );

    if ( command )
    {
//printf("    path_dest=%s\n", path_dest );
        printf( "COMMAND=%s\n", command );
        ret = g_spawn_command_line_sync( command, &stdout, &stderr, &exit_status, NULL );
        g_free( command );
        printf( "%s%s", stdout, stderr );

        if ( !ret || ( exit_status && WIFEXITED( exit_status ) ) )
        {
            msg = g_strdup_printf( _("An error occured copying command files\n\n%s"),
                                                                stderr ? stderr : "" );
            xset_msg_dialog( NULL, GTK_MESSAGE_ERROR, _("Copy Command Error"), NULL,
                                                                0, msg, NULL, NULL );
            g_free( msg );
        }
        if ( stderr )
            g_free( stderr );
        if ( stdout )
            g_free( stdout );
        stderr = stdout = NULL;
        command = g_strdup_printf( "chmod -R go-rwx %s", path_dest );
        printf( "COMMAND=%s\n", command );
        g_spawn_command_line_sync( command, NULL, NULL, NULL, NULL );
        g_free( command );
        g_free( path_dest );
    }
    
    // copy data dir
    XSet* mset = xset_get_plugin_mirror( src );
    path_src = g_build_filename( settings_config_dir, "plugin-data",
                                                        mset->name, NULL );
    if ( g_file_test( path_src, G_FILE_TEST_IS_DIR ) )
    {
        path_dest = g_build_filename( settings_config_dir, "plugin-data",
                                                            dest->name, NULL );
        command = g_strdup_printf( "cp -a %s %s", path_src, path_dest );
        g_free( path_src );
        stderr = stdout = NULL;
        printf( "COMMAND=%s\n", command );
        ret = g_spawn_command_line_sync( command, &stdout, &stderr, &exit_status, NULL );
        g_free( command );
        printf( "%s%s", stdout, stderr );
        if ( !ret || ( exit_status && WIFEXITED( exit_status ) ) )
        {
            msg = g_strdup_printf( _("An error occured copying command data files\n\n%s"),
                                                                stderr ? stderr : "" );
            xset_msg_dialog( NULL, GTK_MESSAGE_ERROR, _("Copy Command Error"), NULL,
                                                            0, msg, NULL, NULL );
            g_free( msg );
        }
        if ( stderr )
            g_free( stderr );
        if ( stdout )
            g_free( stdout );
        stderr = stdout = NULL;
        command = g_strdup_printf( "chmod -R go-rwx %s", path_dest );
        g_free( path_dest );
        printf( "COMMAND=%s\n", command );
        g_spawn_command_line_sync( command, NULL, NULL, NULL, NULL );
        g_free( command );
    }
}

XSet* xset_custom_copy( XSet* set, gboolean copy_next )
{
//printf("\nxset_custom_copy( %s, %d )\n", set->name, copy_next );
    XSet* mset = set;
    if ( set->plugin && set->shared_key )
        mset = xset_get_plugin_mirror( set );
    
    XSet* newset = xset_custom_new();
    newset->menu_label = g_strdup( set->menu_label );
    newset->s = g_strdup( set->s );
    newset->x = g_strdup( set->x );
    newset->y = g_strdup( set->y );
    newset->z = g_strdup( set->z );
    newset->desc = g_strdup( set->desc );
    newset->title = g_strdup( set->title );
    newset->b = set->b;
    newset->menu_style = set->menu_style;
    newset->context = g_strdup( mset->context );
    newset->line = g_strdup( set->line );

    newset->task = mset->task;
    newset->task_pop = mset->task_pop;
    newset->task_err = mset->task_err;
    newset->task_out = mset->task_out;
    newset->in_terminal = mset->in_terminal;
    newset->keep_terminal = mset->keep_terminal;
    newset->scroll_lock = mset->scroll_lock;

    if ( !mset->icon && set->plugin )
        newset->icon = g_strdup( set->icon );
    else
        newset->icon = g_strdup( mset->icon );

    xset_custom_copy_files( set, newset );
    if ( set->tool )
    {
        newset->tool = XSET_B_TRUE;
        if ( !newset->icon )
            newset->icon = g_strdup_printf( "gtk-execute" );
    }

    if ( set->menu_style == XSET_MENU_SUBMENU && set->child )
    {
        XSet* set_child = xset_get( set->child );
//printf("    copy submenu %s\n", set_child->name );
        XSet* newchild = xset_custom_copy( set_child, TRUE );
        newset->child = g_strdup( newchild->name );
        newchild->parent = g_strdup( newset->name );
    }
    
    if ( copy_next && set->next )
    {
        XSet* set_next = xset_get( set->next );
//printf("    copy next %s\n", set_next->name );
        XSet* newnext = xset_custom_copy( set_next, TRUE );
        newnext->prev = g_strdup( newset->name );
        newset->next = g_strdup( newnext->name );        
    }
    return newset;
}

void clean_plugin_mirrors()
{   // remove plugin mirrors for non-existent plugins
    GList* l;
    XSet* set;
    gboolean redo = TRUE;

    while ( redo )
    {
        redo = FALSE;
        for ( l = xsets; l; l = l->next )
        {
            if ( ((XSet*)l->data)->desc && 
                        !strcmp( ((XSet*)l->data)->desc, "@plugin@mirror@" ) )
            {
                set = (XSet*)l->data;
                if ( !set->shared_key )
                {
                    xset_free( set );
                    redo = TRUE;
                    break;
                }
                else if ( !xset_is( set->shared_key ) )
                {
                    xset_free( set );
                    redo = TRUE;
                    break;
                }
            }
        }
    }
    
    // remove plugin-data for non-existent xsets
    const char* name;
    char* command;
    char* stdout;
    char* stderr;
    GDir* dir;
    char* path = g_build_filename( settings_config_dir, "plugin-data", NULL );
_redo:
    dir = g_dir_open( path, 0, NULL );
    if ( dir )
    {
        while ( name = g_dir_read_name( dir ) )
        {
            if ( strlen( name ) == 13 && g_str_has_prefix( name, "cstm_" )
                                                            && !xset_is( name ) )
            {
                g_dir_close( dir );
                command = g_strdup_printf( "rm -rf %s/%s", path, name );
                stderr = stdout = NULL;
                printf( "COMMAND=%s\n", command );
                g_spawn_command_line_sync( command, NULL, NULL, NULL, NULL );
                g_free( command );
                if ( stderr )
                    g_free( stderr );
                if ( stdout )
                    g_free( stdout );
                goto _redo;
            }
        }
        g_dir_close( dir );
    }
    g_free( path );
}

void xset_set_plugin_mirror( XSet* pset )
{
    XSet* set;
    GList* l;
    
    for ( l = xsets; l; l = l->next )
    {
        if ( ((XSet*)l->data)->desc && 
                    !strcmp( ((XSet*)l->data)->desc, "@plugin@mirror@" ) )
        {
            set = (XSet*)l->data;
            if ( set->parent && set->child )
            {
                if ( !strcmp( set->child, pset->plug_name ) &&
                                    !strcmp( set->parent, pset->plug_dir ) )
                {
                    if ( set->shared_key )
                        g_free( set->shared_key );
                    set->shared_key = g_strdup( pset->name );
                    if ( pset->shared_key )
                        g_free( pset->shared_key );
                    pset->shared_key = g_strdup( set->name );
                    return;
                }
            }
        }

    }
}
    
XSet* xset_get_plugin_mirror( XSet* set )
{
    // plugin mirrors are custom xsets that save the user's key, icon
    // and run prefs for the plugin, if any
    if ( !set->plugin )
        return set;
    if ( set->shared_key )
        return xset_get( set->shared_key );
    
    XSet* newset = xset_custom_new();
    newset->desc = g_strdup( "@plugin@mirror@" );
    newset->parent = g_strdup( set->plug_dir );
    newset->child = g_strdup( set->plug_name );
    newset->shared_key = g_strdup( set->name );  // this will not be saved
    newset->task = set->task;
    newset->task_pop = set->task_pop;
    newset->task_err = set->task_err;
    newset->task_out = set->task_out;
    newset->in_terminal = set->in_terminal;
    newset->keep_terminal = set->keep_terminal;
    newset->scroll_lock = set->scroll_lock;
    newset->context = g_strdup( set->context );
    newset->opener = set->opener;
    newset->b = set->b;
    newset->s = g_strdup( set->s );
    set->shared_key = g_strdup( newset->name );    
    return newset;
}

gint compare_plugin_sets( XSet* a, XSet* b )
{
    return g_utf8_collate( a->menu_label, b->menu_label );
}

GList* xset_get_plugins( gboolean included )
{   // return list of plugin sets (included or not ) sorted by menu_label    
    GList* l;
    GList* plugins = NULL;
    XSet* set;
    
    for ( l = xsets; l; l = l->next )
    {
        if ( ((XSet*)l->data)->plugin && ((XSet*)l->data)->plugin_top && 
                                                    ((XSet*)l->data)->plug_dir )
        {
            set = (XSet*)l->data;
            if ( strstr( set->plug_dir, "/included/" ) )
            {
                if ( included )
                    plugins = g_list_prepend( plugins, l->data );
            }
            else if ( !included )
                plugins = g_list_prepend( plugins, l->data );
        }
    }
    plugins = g_list_sort( plugins, (GCompareFunc)compare_plugin_sets );
    return plugins;
}

XSet* xset_get_by_plug_name( const char* plug_dir, const char* plug_name )
{
    GList* l;
    if ( !plug_name )
        return NULL;

    for ( l = xsets; l; l = l->next )
    {
        if ( ((XSet*)l->data)->plugin
                && !strcmp( plug_name, ((XSet*)l->data)->plug_name ) 
                && !strcmp( plug_dir, ((XSet*)l->data)->plug_dir ) )
            return l->data;
    }

    // add new
    XSet* set = xset_new( xset_custom_new_name() );
    set->plug_dir = g_strdup( plug_dir );
    set->plug_name = g_strdup( plug_name );
    set->plugin = TRUE;
    set->lock = FALSE;
    xsets = g_list_append( xsets, set );
    return set;
}

void xset_parse_plugin( const char* plug_dir, char* line )
{
    char* sep = strchr( line, '=' );
    char* name;
    char* value;
    XSet* set;
    XSet* set2;

    if ( !sep )
        return ;
    name = line;
    value = sep + 1;
    *sep = '\0';
    sep = strchr( name, '-' );
    if ( !sep )
        return ;
    char* var = sep + 1;
    *sep = '\0';

    if ( !strncmp( name, "cstm_", 5 ) )
    {
        set = xset_get_by_plug_name( plug_dir, name );
        xset_set_set( set, var, value );
        // map plug names to new set names
        if ( set->prev && !strcmp( var, "prev" ) )
        {
            if ( !strncmp( set->prev, "cstm_", 5 ) )
            {
                set2 = xset_get_by_plug_name( plug_dir, set->prev );
                g_free( set->prev );
                set->prev = g_strdup( set2->name );
            }
            else
            {
                g_free( set->prev );
                set->prev = NULL;
            }
        }
        else if ( set->next && !strcmp( var, "next" ) )
        {
            if ( !strncmp( set->next, "cstm_", 5 ) )
            {
                set2 = xset_get_by_plug_name( plug_dir, set->next );
                g_free( set->next );
                set->next = g_strdup( set2->name );
            }
            else
            {
                g_free( set->next );
                set->next = NULL;
            }
        }
        else if ( set->parent && !strcmp( var, "parent" ) )
        {
            if ( !strncmp( set->parent, "cstm_", 5 ) )
            {
                set2 = xset_get_by_plug_name( plug_dir, set->parent );
                g_free( set->parent );
                set->parent = g_strdup( set2->name );
            }
            else
            {
                g_free( set->parent );
                set->parent = NULL;
            }
        }
        else if ( set->child && !strcmp( var, "child" ) )
        {
            if ( !strncmp( set->child, "cstm_", 5 ) )
            {
                set2 = xset_get_by_plug_name( plug_dir, set->child );
                g_free( set->child );
                set->child = g_strdup( set2->name );
            }
            else
            {
                g_free( set->child );
                set->child = NULL;
            }
        }
    }
}

XSet* xset_import_plugin( const char* plug_dir )
{
    char line[ 2048 ];
    char* section_name;
    gboolean func;
    GList* l;
    XSet* set;

    // clear all existing plugin sets with this plug_dir
    // ( keep the mirrors to retain user prefs )
    gboolean redo = TRUE;
    while ( redo )
    {
        redo = FALSE;
        for ( l = xsets; l; l = l->next )
        {
            if ( ((XSet*)l->data)->plugin
                                && !strcmp( plug_dir, ((XSet*)l->data)->plug_dir ) )
            {
                xset_free( (XSet*)l->data );
                redo = TRUE;  // search list from start again due to changed list
                break;
            }
        }
    }
    
    // read plugin file into xsets
    gboolean plugin_good = FALSE;
    char* plugin = g_build_filename( plug_dir, "plugin", NULL );    
    FILE* file = fopen( plugin, "r" );
    if ( !file )
    {
        g_warning( _("Error reading plugin file %s"), plugin );
        return NULL;
    }
    
    while ( fgets( line, sizeof( line ), file ) )
    {
        strtok( line, "\r\n" );
        if ( ! line[ 0 ] )
            continue;
        if ( line[ 0 ] == '[' )
        {
            section_name = strtok( line, "]" );
            if ( 0 == strcmp( line + 1, "Plugin" ) )
                func = TRUE;
            else
                func = FALSE;
            continue;
        }
        if ( func )
        {
            xset_parse_plugin( plug_dir, line );
            if ( !plugin_good )
                plugin_good = TRUE;
        }
    }
    fclose( file );

    // clean plugin sets, set type
    gboolean top = TRUE;
    XSet* rset = NULL;
    for ( l = xsets; l; l = l->next )
    {
        if ( ((XSet*)l->data)->plugin
                            && !strcmp( plug_dir, ((XSet*)l->data)->plug_dir ) )
        {
            set = (XSet*)l->data;
            set->key = set->keymod = set->tool = 0;
            xset_set_plugin_mirror( set );
            if ( set->plugin_top = top )
            {
                top = FALSE;
                rset = set;
            }
        }
    }
    return plugin_good ? rset : NULL;
}

typedef struct _PluginData
{
    FMMainWindow* main_window;
    char* plug_dir;
    XSet* set;
    int job;
}PluginData;

void on_install_plugin_cb( VFSFileTask* task, PluginData* plugin_data )
{
    XSet* set;
    char* msg;
//printf("on_install_plugin_cb\n");
    if ( plugin_data->job == 2 ) // uninstall
    {
        if ( !g_file_test( plugin_data->plug_dir, G_FILE_TEST_EXISTS ) )
        {
            xset_custom_delete( plugin_data->set, FALSE );
            clean_plugin_mirrors();
            //main_window_on_plugins_change( NULL );
        }
    }
    else
    {
        char* plugin = g_build_filename( plugin_data->plug_dir, "plugin", NULL );
        if ( g_file_test( plugin, G_FILE_TEST_EXISTS ) )
        {
            set = xset_import_plugin( plugin_data->plug_dir );
            if ( !set )
            {
                msg = g_strdup_printf( _("The imported plugin folder does not contain a valid plugin.\n\n(%s/)"), plugin_data->plug_dir );
                xset_msg_dialog( GTK_WIDGET( plugin_data->main_window ), GTK_MESSAGE_ERROR, "Invalid Plugin",
                                                    NULL, 0, msg, NULL, NULL );
                g_free( msg );
            }
            else if ( plugin_data->job == 1 )
            {
                // copy
                set->plugin_top = FALSE;  // don't show tmp plugin in Plugins menu
                if ( plugin_data->set )
                {
                    // paste after insert_set (plugin_data->set)
                    XSet* newset = xset_custom_copy( set, FALSE );
                    newset->prev = g_strdup( plugin_data->set->name );
                    newset->next = plugin_data->set->next;  //steal
                    if ( plugin_data->set->next )
                    {
                        XSet* set_next = xset_get( plugin_data->set->next );
                        g_free( set_next->prev );
                        set_next->prev = g_strdup( newset->name );
                    }
                    plugin_data->set->next = g_strdup( newset->name );
                    if ( plugin_data->set->tool )
                    {
                        newset->tool = XSET_B_TRUE;
                        if ( !newset->icon )
                            newset->icon = g_strdup_printf( "gtk-execute" );
                    }
                    else
                        newset->tool = XSET_B_UNSET;
                }
                else
                {
                    // place on design clipboard
                    set_clipboard = set;
                    clipboard_is_cut = FALSE;
                    if ( xset_get_b( "plug_cverb" ) )
                    {
                        char* label = clean_label( set->menu_label, FALSE, FALSE );
                        if ( geteuid() == 0 )
                            msg = g_strdup_printf( _("The '%s' plugin has been copied to the design clipboard.  Use View|Design Mode to paste it into a menu.\n\nBecause it has not been installed, this plugin will not appear in the Plugins menu."), label );
                        else
                            msg = g_strdup_printf( _("The '%s' plugin has been copied to the design clipboard.  Use View|Design Mode to paste it into a menu.\n\nBecause it has not been installed, this plugin will not appear in the Plugins menu, and its contents are not protected by root (once pasted it will be saved with normal ownership).\n\nIf this plugin contains su commands or will be run as root, installing it to and running it only from the Plugins menu is recommended to improve your system security."), label );
                        g_free( label );
                        GDK_THREADS_ENTER(); // due to dialog run causes low level thread lock
                        xset_msg_dialog( GTK_WIDGET( plugin_data->main_window ),
                                                            0, "Copy Plugin",
                                                            NULL, 0, msg, NULL, NULL );
                        GDK_THREADS_LEAVE();
                        g_free( msg );
                    }
                }
            }
        }
        g_free( plugin );
    }
    g_free( plugin_data->plug_dir );
    g_slice_free( PluginData, plugin_data );
}

void xset_remove_plugin( GtkWidget* parent, PtkFileBrowser* file_browser, XSet* set )
{
    char* msg;
    
    if ( !file_browser || !set || !set->plugin_top || !set->plug_dir )
        return;

    if ( strstr( set->plug_dir, "/included/" ) )
        return;   // failsafe - don't allow removal of included
        
    if ( !app_settings.no_confirm )
    {
        char* label = clean_label( set->menu_label, FALSE, FALSE );
        msg = g_strdup_printf( _("Uninstall the '%s' plugin?\n\n( %s )"), label,
                                                                set->plug_dir );
        g_free( label );
        if ( xset_msg_dialog( parent, GTK_MESSAGE_WARNING, _("Uninstall Plugin"),
                    NULL, GTK_BUTTONS_YES_NO, msg, NULL, NULL ) != GTK_RESPONSE_YES )
        {
            g_free( msg );
            return;
        }
        g_free( msg );
    }
    PtkFileTask* task = ptk_file_exec_new( _("Uninstall Plugin"), NULL, parent,
                                                        file_browser->task_view );

    char* plug_dir_q = bash_quote( set->plug_dir );

    task->task->exec_command = g_strdup_printf( "rm -rf %s", plug_dir_q );
    g_free( plug_dir_q );
    task->task->exec_sync = TRUE;
    task->task->exec_popup = FALSE;
    task->task->exec_show_output = FALSE;
    task->task->exec_show_error = TRUE;
    task->task->exec_export = FALSE;
    task->task->exec_as_user = g_strdup( "root" );
    
    PluginData* plugin_data = g_slice_new0( PluginData );
    plugin_data->main_window = NULL;
    plugin_data->plug_dir = g_strdup( set->plug_dir );
    plugin_data->set = set;
    plugin_data->job = 2;
    task->complete_notify = (GFunc)on_install_plugin_cb;
    task->user_data = plugin_data;

    ptk_file_task_run( task );
}

void install_plugin_file( gpointer main_win, const char* path,
                    const char* plug_dir, int type, int job, XSet* insert_set )
{
    char* wget;
    char* file_path;
    char* file_path_q;
    char* own;
    char* rem = g_strdup( "" );
    char* compression = "z";
    
    FMMainWindow* main_window = (FMMainWindow*)main_win;
    // task
    PtkFileTask* task = ptk_file_exec_new( _("Install Plugin"), NULL,
                                main_win ? GTK_WIDGET( main_window ) : NULL,
                                main_win ? main_window->task_view : NULL );

    char* plug_dir_q = bash_quote( plug_dir );

    if ( type == 0 )
    {
        // file
        wget = g_strdup( "" );
        if ( g_str_has_suffix( path, ".tar.xz" ) )
            compression = "J";
        file_path_q = bash_quote( path );
    }
    else
    {
        // url
        if ( g_str_has_suffix( path, ".tar.xz" ) )
        {
            file_path = g_build_filename( plug_dir, "plugin-tmp.tar.xz", NULL );
            compression = "J";            
        }
        else
            file_path = g_build_filename( plug_dir, "plugin-tmp.tar.gz", NULL );
        file_path_q = bash_quote( file_path );
        g_free( file_path );
        char* url_q = bash_quote( path );
        wget = g_strdup_printf( "&& wget --tries=1 --connect-timeout=30 -O %s %s ",
                                                            file_path_q, url_q );
        g_free( url_q );
        g_free( rem );
        rem = g_strdup_printf( "; rm -f %s", file_path_q );
    }

    if ( job == 0 )
    {
        // install
        own = g_strdup_printf( "chown -R root:root %s && chmod -R go+rX-w %s",
                                                    plug_dir_q, plug_dir_q );
        task->task->exec_as_user = g_strdup( "root" );
    }
    else
    {
        // copy
        own = g_strdup_printf( "chmod -R go+rX-w %s", plug_dir_q );
    }
        
    task->task->exec_command = g_strdup_printf( "rm -rf %s ; mkdir -p %s && cd %s %s&& tar --exclude='/*' --keep-old-files -x%sf %s ; err=$?; if [ $err -ne 0 ] || [ ! -e plugin ]; then rm -rf %s ; echo 'Error installing plugin (invalid plugin file?)'; exit 1 ; fi ; %s %s",
                                plug_dir_q, plug_dir_q, plug_dir_q,
                                wget, compression, file_path_q, plug_dir_q, own, rem );
    g_free( plug_dir_q );
    g_free( file_path_q );
    g_free( own );
    g_free( rem );
    task->task->exec_sync = TRUE;
    task->task->exec_popup = FALSE;
    task->task->exec_show_output = FALSE;
    task->task->exec_show_error = TRUE;
    task->task->exec_export = FALSE;
    
    PluginData* plugin_data = g_slice_new0( PluginData );
    plugin_data->main_window = main_window;
    plugin_data->plug_dir = g_strdup( plug_dir );
    plugin_data->job = job;
    plugin_data->set = insert_set;
    task->complete_notify = (GFunc)on_install_plugin_cb;
    task->user_data = plugin_data;

    ptk_file_task_run( task );
}

gboolean xset_custom_export_files( XSet* set, char* plug_dir )
{
    char* cscript;
    char* path_src;
    char* path_dest;
    char* command;
    char* stdout = NULL;
    char* stderr = NULL;

    // do this for backwards compat - will copy old script
    cscript = xset_custom_get_script( set, FALSE );
    if ( cscript )
        g_free( cscript );

    if ( set->plugin )
    {
        path_src = g_build_filename( set->plug_dir, set->plug_name, NULL );
        path_dest = g_build_filename( plug_dir, set->plug_name, NULL );
    }
    else
    {
        path_src = g_build_filename( settings_config_dir, "scripts", set->name, NULL );
        path_dest = g_build_filename( plug_dir, set->name, NULL );
    }

    if ( !g_file_test( path_src, G_FILE_TEST_EXISTS ) )
    {
        g_mkdir_with_parents( path_dest, 0755 );
        if ( !g_file_test( path_dest, G_FILE_TEST_EXISTS ) )
        {
            g_free( path_src );
            g_free( path_dest );
            return FALSE;
        }
        chmod( path_dest, 0755 );
        if ( !set->plugin )
        {
            g_free( path_src );
            g_free( path_dest );
            return TRUE;
        }
        // plugin backwards compat ?
        cscript = xset_custom_get_script( set, FALSE );
        if ( cscript && g_file_test( cscript, G_FILE_TEST_EXISTS ) )
        {
            command = g_strdup_printf( "cp -a %s %s/", cscript, path_dest );
            g_free( cscript );
        }
        else
        {
            if ( cscript )
                g_free( cscript );
            g_free( path_src );
            g_free( path_dest );
            return TRUE;
        }            
    }
    else
    {
        command = g_strdup_printf( "cp -a %s %s", path_src, path_dest );
    }
    g_free( path_src );
    g_free( path_dest );
    printf( "COMMAND=%s\n", command );
    gboolean ret = g_spawn_command_line_sync( command, NULL, NULL, NULL, NULL );
    g_free( command );
    if ( stderr )
        g_free( stderr );
    if ( stdout )
        g_free( stdout );

    return ret;
}

gboolean xset_custom_export_write( FILE* file, XSet* set, char* plug_dir )
{   // recursively write set, submenu sets, and next sets
    xset_write_set( file, set );
    if ( !xset_custom_export_files( set, plug_dir ) )
        return FALSE;
    if ( set->menu_style == XSET_MENU_SUBMENU && set->child )
    {
        if ( !xset_custom_export_write( file, xset_get( set->child ), plug_dir ) )
            return FALSE;
    }
    if ( set->next )
    {
        if ( !xset_custom_export_write( file, xset_get( set->next ), plug_dir ) )
            return FALSE;
    }
    return TRUE;
}

void xset_custom_export( GtkWidget* parent, PtkFileBrowser* file_browser,
                                                                    XSet* set )
{
    char* deffolder;
    char* deffile;
    char* s1;
    char* s2;

    if ( !file_browser )
        return;
        
    // get new plugin filename    
    XSet* save = xset_get( "plug_ifile" );
    if ( save->s )  //&& g_file_test( save->s, G_FILE_TEST_IS_DIR )
        deffolder = save->s;
    else
    {
        if ( !( deffolder = xset_get_s( "go_set_default" ) ) )
            deffolder = "/";
    }
    if ( !set->plugin )
    {
        s1 = clean_label( set->menu_label, TRUE, FALSE );
        s2 = plain_ascii_name( s1 );
        if ( s2[0] == '\0' )
        {
            g_free( s2 );
            s2 = g_strdup( "Plugin" );
        }
        deffile = g_strdup_printf( "%s.spacefm-plugin.tar.gz", s2 );
        g_free( s1 );
        g_free( s2 );
    }
    else
    {
        s1 = g_path_get_basename( set->plug_dir );
        deffile = g_strdup_printf( "%s.spacefm-plugin.tar.gz", s1 );
        g_free( s1 );        
    }
    char* path = xset_file_dialog( parent, GTK_FILE_CHOOSER_ACTION_SAVE,
                                _("Save As Plugin File"), deffolder, deffile );
    g_free( deffile );
    if ( !path )
        return;
    if ( save->s )
        g_free( save->s );
    save->s = g_path_get_dirname( path );
    
    // get or create tmp plugin dir
    char* plug_dir = NULL;
    char* hex8;
    if ( !set->plugin )
    {
        s1 = (char*)xset_get_user_tmp_dir();
        if ( !s1 )
            goto _export_error;
        while ( !plug_dir || g_file_test( plug_dir, G_FILE_TEST_EXISTS ) )
        {
            hex8 = randhex8();
            if ( plug_dir )
                g_free( plug_dir );
            plug_dir = g_build_filename( s1, hex8, NULL );
            g_free( hex8 );
        }
        g_mkdir_with_parents( plug_dir, 0700 );
        chmod( plug_dir, 0700 );

        // Create plugin file
        s1 = g_build_filename( plug_dir, "plugin", NULL );
        FILE* file = fopen( s1, "w" );
        g_free( s1 );
        if ( !file )
            goto _rmtmp_error;
        int result = fputs( "# SpaceFM Plugin File\n\n# THIS FILE IS NOT DESIGNED TO BE EDITED\n\n", file );
        if ( result < 0 )
        {
            fclose( file );
            goto _rmtmp_error;
        }
        fputs( "[Plugin]\n", file );
        xset_write_set( file, xset_get( "config_version" ) );
        
        char* s_prev = set->prev;
        char* s_next = set->next;
        char* s_parent = set->parent;
        set->prev = set->next = set->parent = NULL;
        xset_write_set( file, set );
        set->prev = s_prev;
        set->next = s_next;
        set->parent = s_parent;
        
        if ( !xset_custom_export_files( set, plug_dir ) )
            goto _rmtmp_error;            
        if ( set->menu_style == XSET_MENU_SUBMENU && set->child )
        {
            if ( !xset_custom_export_write( file, xset_get( set->child ), plug_dir ) )
                goto _rmtmp_error;            
        }
        result = fputs( "\n", file );
        fclose( file );
        if ( result < 0 )
            goto _rmtmp_error;
    }
    else
        plug_dir = g_strdup( set->plug_dir );
    
    // tar and delete tmp files
    // task
    PtkFileTask* task = ptk_file_exec_new( _("Export Plugin"), plug_dir, parent,
                                            file_browser->task_view );
    char* plug_dir_q = bash_quote( plug_dir );
    char* path_q = bash_quote( path );
    if ( !set->plugin )
        task->task->exec_command = g_strdup_printf( "tar --numeric-owner -czf %s * ; err=$? ; rm -rf %s ; if [ $err -ne 0 ]; then rm -f %s; fi; exit $err", path_q, plug_dir_q, path_q );
    else
        task->task->exec_command = g_strdup_printf( "tar --numeric-owner -czf %s * ; err=$? ; if [ $err -ne 0 ]; then rm -f %s; fi; exit $err", path_q, path_q );
    g_free( plug_dir_q );
    g_free( path_q );
    task->task->exec_sync = TRUE;
    task->task->exec_popup = FALSE;
    task->task->exec_show_output = FALSE;
    task->task->exec_show_error = TRUE;
    task->task->exec_export = FALSE;
    task->task->exec_browser = file_browser;
    ptk_file_task_run( task );          

    g_free( path );
    g_free( plug_dir );
    return;
    
_rmtmp_error:
    if ( !set->plugin )
    {
        s2 = bash_quote( plug_dir );
        s1 = g_strdup_printf( "rm -rf %s", s2 );
        g_spawn_command_line_sync( s1, NULL, NULL, NULL, NULL );
        g_free( s1 );
        g_free( s2 );
    }
_export_error:
    g_free( plug_dir );
    g_free( path );    
    xset_msg_dialog( parent, GTK_MESSAGE_ERROR, _("Export Error"), NULL, 0,
                            _("Unable to create temporary files"), NULL, NULL );
}

static void open_spec( PtkFileBrowser* file_browser, const char* url,
                                                gboolean in_new_tab )
{
    gboolean new_window = FALSE;
    if ( !file_browser )
    {
        FMMainWindow* main_window = fm_main_window_get_on_current_desktop();
        if ( !main_window )
        {
            main_window = FM_MAIN_WINDOW(fm_main_window_new());
            gtk_window_set_default_size( GTK_WINDOW( main_window ),
                                         app_settings.width,
                                         app_settings.height );
            gtk_widget_show( GTK_WIDGET(main_window) );
            new_window = !xset_get_b( "main_save_tabs" );
        }
        file_browser = PTK_FILE_BROWSER( 
                    fm_main_window_get_current_file_browser( main_window ) );
        gtk_window_present( GTK_WINDOW( main_window ) );
    }
    gboolean new_tab = !new_window && in_new_tab;

    if ( ( url[0] != '/' && strstr( url, ":/" ) ) ||
                                            g_str_has_prefix( url, "//" ) )
    {
        // network
        if ( file_browser )
            mount_network( file_browser, url, new_tab );
        else
            open_in_prog( url );
    }
    else if ( g_file_test( url, G_FILE_TEST_IS_DIR ) )
    {
        // dir
        if ( file_browser )
            ptk_file_browser_emit_open( file_browser, url,
                                        new_tab ?
                                            PTK_OPEN_NEW_TAB : PTK_OPEN_DIR );
        else
            open_in_prog( url );
    }
    else if ( g_file_test( url, G_FILE_TEST_EXISTS ) )
    {
        // file - open dir and select file
        char* dir = g_path_get_dirname( url );
        if ( dir && g_file_test( dir, G_FILE_TEST_IS_DIR ) )
        {
            if ( file_browser )
            {
                if ( !new_tab && !strcmp( dir,
                                ptk_file_browser_get_cwd( file_browser ) ) )
                {
                    ptk_file_browser_select_file( file_browser, url );
                    gtk_widget_grab_focus( GTK_WIDGET(
                                            file_browser->folder_view ) );
                }
                else
                {
                    file_browser->select_path = strdup( url );
                    ptk_file_browser_emit_open( file_browser, dir,
                                                new_tab ?
                                            PTK_OPEN_NEW_TAB : PTK_OPEN_DIR );
                    if ( new_tab )
                    {
                        FMMainWindow* main_window_last =
                                    (FMMainWindow*)file_browser->main_window;
                        file_browser = main_window_last ? PTK_FILE_BROWSER( 
                                    fm_main_window_get_current_file_browser(
                                                        main_window_last ) ) :
                                    NULL;
                        if ( file_browser )
                        {
                            // select path in new browser
                            file_browser->select_path = strdup( url );
                            // usually this is not ready but try anyway
                            ptk_file_browser_select_file( file_browser, url );
                        }
                    }
                }
            }
            else
                open_in_prog( dir );
        }
        g_free( dir );
    }
}

void xset_custom_activate( GtkWidget* item, XSet* set )
{
    GtkWidget* parent;
    GtkWidget* task_view = NULL;
    const char* cwd;
    char* command;
    char* value = NULL;
    XSet* mset;

    // plugin?
    mset = xset_get_plugin_mirror( set );

    if ( set->browser )
    {
        parent = GTK_WIDGET( set->browser );
        task_view = set->browser->task_view;
        cwd = ptk_file_browser_get_cwd( set->browser );
    }
    else
    {
        parent = GTK_WIDGET( set->desktop );
        cwd = vfs_get_desktop_dir();
    }
    
    // name
    if ( !set->plugin &&
            !( !set->lock && set->x && atoi( set->x ) > 1 /*app or bookmark*/) )
    {
        if ( !( set->menu_label && set->menu_label[0] )
                || ( set->menu_label && !strcmp( set->menu_label, _("New _Command") ) ) )
        {
            if ( !xset_text_dialog( parent, _("Change Menu Name"), NULL, FALSE, _(enter_menu_name_new),
                                            NULL, set->menu_label, &set->menu_label,
                                            NULL, FALSE, "#designmode-designmenu-new" ) )
                return;
        }
    }
    
    // variable value
    if ( set->menu_style == XSET_MENU_CHECK )
        value = g_strdup_printf( "%d", mset->b == XSET_B_TRUE ? 1 : 0 );
    else if ( set->menu_style == XSET_MENU_STRING )
        value = g_strdup( mset->s );
    else
        value = g_strdup( set->menu_label );

    // command
    gboolean app_no_sync = FALSE;
    if ( !set->x )
        set->x = g_strdup( "0" );
    int command_type = atoi( set->x );
    if ( command_type == 0 )
    {
        // line
        if ( !set->line || set->line[0] == '\0' )
        {
            xset_item_prop_dlg( xset_context, set, 2 );
            return;
            /*
            if ( !xset_text_dialog( parent, _("Set Command Line"), NULL, TRUE,
                            _(enter_command_line), NULL, set->line, &set->line,
                                                NULL, FALSE,
                                                "#designmode-command-line" )
                             || !set->line || set->line[0] == '\0' )
                return;
            */
        }
        command = replace_line_subs( set->line );
        char* str = replace_string( command, "\\n", "\n", FALSE );
        g_free( command );
        command = replace_string( str, "\\t", "\t", FALSE );
        g_free( str );
    }
    else if ( command_type == 1 )
    {
        // script
        command = xset_custom_get_script( set, FALSE );
        if ( !command )
            return;
    }
    else if ( command_type == 2 )
    {
        // app or executable
        if ( !( set->z && set->z[0] ) )
        {
            xset_item_prop_dlg( xset_context, set, 0 );
            return;
        }
        else if ( g_str_has_suffix( set->z, ".desktop" ) )
        {
            VFSAppDesktop* app = vfs_app_desktop_new( set->z );
            if ( app && app->exec && app->exec[0] != '\0' )
            {
                // get file list
                GList* sel_files;
                GdkScreen* screen;
                if ( set->browser )
                {
                    sel_files = ptk_file_browser_get_selected_files(
                                                            set->browser );
                    screen = gtk_widget_get_screen(
                                                GTK_WIDGET( set->browser ) );
                }
#ifdef DESKTOP_INTEGRATION
                else if ( set->desktop )
                {
                    sel_files = desktop_window_get_selected_files(
                                                            set->desktop );
                    screen = gtk_widget_get_screen(
                                                GTK_WIDGET( set->desktop ) );
                }
#endif
                else
                {
                    sel_files = NULL;
                    cwd = "/";
                    screen = gdk_screen_get_default();
                }
                GList* file_paths = NULL;
                GList* l;
                char* path;
                for ( l = sel_files; l; l = l->next )
                {
                    file_paths = g_list_prepend( file_paths, g_build_filename(
                                            cwd, vfs_file_info_get_name(
                                            (VFSFileInfo*)l->data ), NULL ) );                    
                }
                file_paths = g_list_reverse( file_paths );
                
                // open in app
                GError* err = NULL;
                if ( !vfs_app_desktop_open_files( screen, cwd, app, file_paths,
                                                                    &err ) )
                {
                    ptk_show_error( parent ? GTK_WINDOW( parent ) : NULL,
                                    _("Error"),
                                    err->message );
                    g_error_free( err );
                }
                if ( sel_files )
                {
                    g_list_foreach( sel_files, (GFunc)vfs_file_info_unref,
                                                                    NULL );
                    g_list_free( sel_files );
                }
                if ( file_paths )
                {
                    g_list_foreach( file_paths, (GFunc)g_free, NULL );
                    g_list_free( file_paths );
                }
            }
            if ( app )
                vfs_app_desktop_unref( app );
            return;
        }
        else
        {
            command = bash_quote( set->z );
            app_no_sync = TRUE;
        }
    }
    else if ( command_type == 3 )
    {
        // Bookmark
        if ( !( set->z && set->z[0] ) )
        {
            xset_item_prop_dlg( xset_context, set, 0 );
            return;
        }
        if ( g_file_test( set->z, G_FILE_TEST_EXISTS ) )
            open_spec( set->browser, set->z,
                                set->desktop || xset_get_b( "book_newtab" ) );
        else
        {
            // parse semi-colon separated list
            char* sep;
            char* specs = set->z;
            char* url;
            while ( specs && specs[0] )
            {
                if ( sep = strchr( specs, ';' ) )
                    sep[0] = '\0';
                url = g_strdup( specs );
                url = g_strstrip( url );
                if ( url[0] )
                    open_spec( set->browser, url, TRUE );
                g_free( url );
                if ( sep )
                {
                    sep[0] = ';';
                    specs = sep + 1;
                }
                else
                    specs = NULL;
            }
        }
        return;
    }
    else
        return;
    
    // task
    char* task_name = clean_label( set->menu_label, FALSE, FALSE );
    PtkFileTask* task = ptk_file_exec_new( task_name, cwd, parent,
                                                            task_view );
    g_free( task_name );
    // don't free cwd!
    task->task->exec_browser = set->browser;
    task->task->exec_desktop = set->desktop;
    task->task->exec_command = command;
    task->task->exec_set = set;
    
    if ( set->y && set->y[0] != '\0' )
        task->task->exec_as_user = g_strdup( set->y );

    if ( set->plugin && set->shared_key && mset->icon )
        task->task->exec_icon = g_strdup( mset->icon );
    if ( !task->task->exec_icon && set->icon )
        task->task->exec_icon = g_strdup( set->icon );

    task->task->current_dest = value;  // temp storage
    task->task->exec_terminal = ( mset->in_terminal == XSET_B_TRUE );
    task->task->exec_keep_terminal = ( mset->keep_terminal == XSET_B_TRUE );
    task->task->exec_sync = !app_no_sync && ( mset->task == XSET_B_TRUE );
    task->task->exec_popup = ( mset->task_pop == XSET_B_TRUE );
    task->task->exec_show_output = ( mset->task_out == XSET_B_TRUE );
    task->task->exec_show_error = ( mset->task_err == XSET_B_TRUE );
    task->task->exec_scroll_lock = ( mset->scroll_lock == XSET_B_TRUE );
    task->task->exec_export = TRUE;
//task->task->exec_keep_tmp = TRUE;

    ptk_file_task_run( task );
}

void xset_custom_delete( XSet* set, gboolean delete_next )
{
    char* cscript;
    char* path1;
    char* path2;
    char* path3;
    char* command;
    char* stdout = NULL;
    char* stderr = NULL;
    
    if ( set->menu_style == XSET_MENU_SUBMENU && set->child )
    {
        XSet* set_child = xset_get( set->child );
        xset_custom_delete( set_child, TRUE );
    }
    
    if ( delete_next && set->next )
    {
        XSet* set_next = xset_get( set->next );
        xset_custom_delete( set_next, TRUE );
    }
    
    if ( set == set_clipboard )
        set_clipboard = NULL;

    cscript = g_strdup_printf( "%s.sh", set->name );  //backwards compat
    path1 = g_build_filename( settings_config_dir, "scripts", cscript, NULL );
    path2 = g_build_filename( settings_config_dir, "scripts", set->name, NULL );
    path3 = g_build_filename( settings_config_dir, "plugin-data", set->name, NULL );
    command = g_strdup_printf( "rm -rf %s %s %s", path1, path2, path3 );
    g_free( path1 );
    g_free( path2 );
    g_free( path3 );
    g_free( cscript );
    printf( "COMMAND=%s\n", command );
    g_spawn_command_line_sync( command, NULL, NULL, NULL, NULL );
    g_free( command );
    if ( stderr )
        g_free( stderr );
    if ( stdout )
        g_free( stdout );

    xset_free( set );
}

void xset_custom_remove( XSet* set )
{
    XSet* set_prev;
    XSet* set_next;
    XSet* set_parent;
    XSet* set_child;

/*
printf("xset_custom_remove %s (%s)\n", set->name, set->menu_label );
printf("    set->parent = %s\n", set->parent );
printf("    set->prev = %s\n", set->prev );
printf("    set->next = %s\n", set->next );
*/
    if ( set->prev )
    {
        set_prev = xset_get( set->prev );
        //printf("        set->prev = %s (%s)\n", set_prev->name, set_prev->menu_label );
        if ( set_prev->next )
            g_free( set_prev->next );
        if ( set->next )
            set_prev->next = g_strdup( set->next );
        else
            set_prev->next = NULL;
    }
    if ( set->next )
    {
        set_next = xset_get( set->next );
        if ( set_next->prev )
            g_free( set_next->prev );
        if ( set->prev )
            set_next->prev = g_strdup( set->prev );
        else
        {
            set_next->prev = NULL;
            if ( set->parent )
            {
                set_parent = xset_get( set->parent );
                if ( set_parent->child )
                    g_free( set_parent->child );
                set_parent->child = g_strdup( set_next->name );
                set_next->parent = g_strdup( set->parent );
            }
        }
    }
    if ( !set->prev && !set->next && set->parent )
    {
        set_parent = xset_get( set->parent );
        set_child = xset_custom_new();
        if ( set_parent->child )
            g_free( set_parent->child );
        set_parent->child = g_strdup( set_child->name );
        set_child->parent = g_strdup( set->parent );
        set_child->menu_label = g_strdup( _("New _Command") );
    }
}

gboolean xset_clipboard_in_set( XSet* set )
{   // look upward to see if clipboard is in set's tree
    if ( !set_clipboard || set->lock )
        return FALSE;
    if ( set == set_clipboard )
        return TRUE;
        
    if ( set->parent )
    {
        XSet* set_parent = xset_get( set->parent );
        if ( xset_clipboard_in_set( set_parent ) )
            return TRUE;
    }

    if ( set->prev )
    {
        XSet* set_prev = xset_get( set->prev );
        while ( set_prev )
        {
            if ( set_prev->parent )
            {
                XSet* set_prev_parent = xset_get( set_prev->parent );
                if ( xset_clipboard_in_set( set_prev_parent ) )
                    return TRUE;
                set_prev = NULL;
            }
            else if ( set_prev->prev )
                set_prev = xset_get( set_prev->prev );
            else
                set_prev = NULL;
        }
    }
    return FALSE;
}

XSet* xset_custom_new()
{
    char* setname;
    XSet* set;
    
    setname = xset_custom_new_name();
 
    // create set
    set = xset_get( setname );
    g_free( setname );
    set->lock = FALSE;
    set->keep_terminal = XSET_B_TRUE;
    set->task = XSET_B_TRUE;
    set->task_err = XSET_B_TRUE;
    set->task_out = XSET_B_TRUE;
    set->x = g_strdup_printf( "0" );
    return set;
}

gboolean have_x_access( const char* path )
{
#if defined(HAVE_EUIDACCESS)
    return ( euidaccess( path, R_OK | X_OK ) == 0 );
#elif defined(HAVE_EACCESS)
    return ( eaccess( path, R_OK | X_OK ) == 0 );
#else
    struct stat results;  

    stat( path, &results );
    if ( ( results.st_mode & S_IXOTH ) )
        return TRUE;    
    if ( ( results.st_mode & S_IXUSR ) && ( geteuid() == results.st_uid ) )
        return TRUE;
    if ( ( results.st_mode & S_IXGRP ) && ( getegid() == results.st_gid ) )
        return TRUE;
    return FALSE;
#endif
}

gboolean have_rw_access( const char* path )
{
    if ( !path )
        return FALSE;
#if defined(HAVE_EUIDACCESS)
    return ( euidaccess( path, R_OK | W_OK ) == 0 );
#elif defined(HAVE_EACCESS)
    return ( eaccess( path, R_OK | W_OK ) == 0 );
#else
    struct stat results;  

    stat( path, &results );
    if ( ( results.st_mode & S_IROTH ) && ( results.st_mode & S_IWOTH ) )
        return TRUE;    
    if ( ( results.st_mode & S_IRUSR ) && ( results.st_mode & S_IWUSR ) 
                                    && ( geteuid() == results.st_uid ) )
        return TRUE;
    if ( ( results.st_mode & S_IRGRP ) && ( results.st_mode & S_IWGRP ) 
                                    && ( getegid() == results.st_gid ) )
        return TRUE;
    return FALSE;
#endif
}

gboolean dir_has_files( const char* path )
{
    GDir* dir;
    gboolean ret = FALSE;
    
    if ( !( path && g_file_test( path, G_FILE_TEST_IS_DIR ) ) )
        return FALSE;
        
    dir = g_dir_open( path, 0, NULL );
    if ( dir )
    {
        if ( g_dir_read_name( dir ) )
            ret = TRUE;
        g_dir_close( dir );
    }
    return ret;
}

void xset_edit( GtkWidget* parent, const char* path, gboolean force_root, gboolean no_root )
{
    gboolean as_root = FALSE;
    gboolean terminal;
    char* editor;
    char* quoted_path;
    GtkWidget* dlgparent = NULL;
    if ( !path )
        return;
    if ( force_root && no_root )
        return;
        
    if ( parent )
        dlgparent = gtk_widget_get_toplevel( GTK_WIDGET( parent ) );
    
    if ( geteuid() != 0 && !force_root && ( no_root || have_rw_access( path ) ) )
    {
        editor = xset_get_s( "editor" );
        if ( !editor || editor[0] == '\0' )
        {
            ptk_show_error( GTK_WINDOW( dlgparent ), _("Editor Not Set"),
                            _("Please set your editor in View|Preferences|Advanced") );
            return;
        }
        terminal = xset_get_b( "editor" );
    }
    else
    {
        editor = xset_get_s( "root_editor" );
        if ( !editor || editor[0] == '\0' )
        {
            ptk_show_error( GTK_WINDOW( dlgparent ), _("Root Editor Not Set"),
                            _("Please set root's editor in View|Preferences|Advanced") );
            return;
        }
        as_root = TRUE;
        terminal = xset_get_b( "root_editor" );
    }
    // replacements
    quoted_path = bash_quote( path );
    if ( strstr( editor, "%f" ) )
        editor = replace_string( editor, "%f", quoted_path, FALSE );
    else if ( strstr( editor, "%F" ) )
        editor = replace_string( editor, "%F", quoted_path, FALSE );
    else if ( strstr( editor, "%u" ) )
        editor = replace_string( editor, "%u", quoted_path, FALSE );
    else if ( strstr( editor, "%U" ) )
        editor = replace_string( editor, "%U", quoted_path, FALSE );
    else
        editor = g_strdup_printf( "%s %s", editor, quoted_path );
    g_free( quoted_path );

    // task
    char* task_name = g_strdup_printf( _("Edit %s"), path );
    char* cwd = g_path_get_dirname( path );
    PtkFileTask* task = ptk_file_exec_new( task_name, cwd, dlgparent, NULL );
    g_free( task_name );
    g_free( cwd );
    task->task->exec_command = editor;
    task->task->exec_sync = FALSE;
    task->task->exec_terminal = terminal;
    if ( as_root )
        task->task->exec_as_user = g_strdup_printf( "root" );
    ptk_file_task_run( task );
}

void xset_open_url( GtkWidget* parent, const char* url )
{
    const char* browser;
    char* command = NULL;

    if ( !url )
        url = homepage;
        
    browser = xset_get_s( "main_help_browser" );
    if ( browser )
    {
        if ( strstr( browser, "%u" ) )
            command = replace_string( browser, "%u", url, TRUE );
        else
            command = g_strdup_printf( "%s '%s'", browser, url );
    }
    else
    {
        browser = g_getenv( "BROWSER" );
        if ( browser && browser[0] != '\0' )
            command = g_strdup_printf( "%s '%s'", browser, url );
        else
        {
            int ii = 0;
            char* program;
            if ( g_str_has_prefix( url, "file://" )
                                            || g_str_has_prefix( url, "/" ) )
                ii = 3;  // xdg,gnome,exo-open use editor for html files so skip at start
            char* programs[] = { "xdg-open", "gnome-open", "exo-open", "firefox", "iceweasel", "arora", "konqueror", "opera", "epiphany", "midori", "chrome", "xdg-open", "gnome-open", "exo-open" };
            int i;
            for(  i = ii; i < G_N_ELEMENTS(programs); ++i)
            {
                if ( ( program = g_find_program_in_path( programs[i] ) ) )
                {
                    command = g_strdup_printf( "%s '%s'", program, url );
                    g_free( program );
                    break;
                }
            }
        }
    }
    
    if ( !command )
    {
        XSet* set = xset_get( "main_help_browser" );
        if ( !xset_text_dialog( parent, set->title, NULL, TRUE, 
                        set->desc, NULL, set->s, &set->s, NULL, FALSE, NULL ) || !set->s )
            return;
        xset_open_url( parent, url );
        return;
    }
    
    // task
    PtkFileTask* task = ptk_file_exec_new( "Open URL", "/", parent, NULL );
    task->task->exec_command = command;
    task->task->exec_sync = FALSE;
    task->task->exec_export = FALSE;
    ptk_file_task_run( task );
}

char* xset_get_manual_url()
{
    char* path;
    char* url;
    url = xset_get_s( "main_help_url" );
    if ( url )
    {
        if ( url[0] == '/' )
            return g_strdup_printf( "file://%s", url );
        return g_strdup( url );
    }
    
    // get user's locale
    const char* locale = NULL;
    const char* const * langs = g_get_language_names();
    char* dot = strchr( langs[0], '.' );
    if( dot )
        locale = g_strndup( langs[0], (size_t)(dot - langs[0]) );
    else
        locale = langs[0];
    if ( !locale || locale[0] == '\0' )
        locale = "en";
    char* l = g_strdup( locale );
    char* ll = strchr( l, '_' );
    if ( ll )
        ll[0] = '\0';

    // get potential filenames
    GList* names = NULL;
    if ( locale && locale[0] != '\0' )
        names = g_list_append( names, g_strdup_printf( "spacefm-manual-%s.html",
                                                                    locale ) );
    if ( l && l[0] != '\0' && g_strcmp0( l, locale ) )
        names = g_list_append( names, g_strdup_printf( "spacefm-manual-%s.html", l ) );
    if ( g_strcmp0( l, "en" ) )
        names = g_list_append( names, g_strdup( "spacefm-manual-en.html" ) );
    names = g_list_append( names, g_strdup( "spacefm-manual.html" ) );
    g_free( l );

    // get potential locations
    GList* locations = NULL;
    if ( HTMLDIR )
        locations = g_list_append( locations, g_strdup( HTMLDIR ) );
    if ( DATADIR )
        locations = g_list_append( locations, g_build_filename( DATADIR,
                                                            "spacefm", NULL ) );
    const gchar* const * dir = g_get_system_data_dirs();
    for( ; *dir; ++dir )
    {
        path = g_build_filename( *dir, "spacefm", NULL );
        if ( !g_list_find_custom( locations, path, (GCompareFunc)g_strcmp0 ) )
            locations = g_list_append( locations, path );
        else
            g_free( path );
    }
    if ( !g_list_find_custom( locations, "/usr/local/share/spacefm",
                                                    (GCompareFunc)g_strcmp0 ) )
        locations = g_list_append( locations, g_strdup( "/usr/local/share/spacefm" ) );
    if ( !g_list_find_custom( locations, "/usr/share/spacefm",
                                                    (GCompareFunc)g_strcmp0 ) )
        locations = g_list_append( locations, g_strdup( "/usr/share/spacefm" ) );
    
    GList* loc;
    GList* ln;
    for ( loc = locations; loc && !url; loc = loc->next )
    {
        for ( ln = names; ln; ln = ln->next )
        {
            path = g_build_filename( (char*)loc->data, (char*)ln->data, NULL );
            if ( g_file_test( path, G_FILE_TEST_EXISTS ) )
            {
                url = path;
                break;
            }
            g_free( path );
        }
    }
    g_list_foreach( names, (GFunc)g_free, NULL );
    g_list_foreach( locations, (GFunc)g_free, NULL );
    g_list_free( names );
    g_list_free( locations );
    
    if ( !url )
        return NULL;
    
    path = g_strdup_printf( "file://%s", url );
    g_free( url );
    return path;
}

void xset_show_help( GtkWidget* parent, XSet* set, const char* anchor )
{
    GtkWidget* dlgparent = NULL;
    char* url;
    char* manual = NULL;
    
    if ( parent )
        dlgparent = parent;
    else if ( set )
        dlgparent = set->browser ? GTK_WIDGET( set->browser ) : GTK_WIDGET( set->desktop );

    if ( !set || ( set && set->lock ) )
    {
        manual = xset_get_manual_url();
        if ( !manual )
        {
            if ( xset_msg_dialog( dlgparent, GTK_MESSAGE_QUESTION,
                                                _("User's Manual Not Found"), NULL,
                                                GTK_BUTTONS_YES_NO,
                                                _("Read the user's manual online?\n\nThe local copy of the SpaceFM user's manual was not found.  Click Yes to read it online, or click No and then set the correct location in Help|Options|Manual Location."), NULL, NULL ) != GTK_RESPONSE_YES )
                return;
            manual = g_strdup( user_manual_url );
            xset_set( "main_help_url", "s", manual );
        }
    }
    
    if ( set )
    {
        if ( set->lock )
        {
            // built-in command
            if ( set->line )
            {
                url = g_strdup_printf( "%s%s", manual, set->line );
                xset_open_url( dlgparent, url );
                g_free( url );
            }
            else
            {
                g_free( manual );
                return;
            }
        }
        else
        {
            // custom command or plugin
            url = xset_custom_get_help( set );
            if ( !url )
            {
                if ( set->plugin )
                    xset_msg_dialog( dlgparent, 0, _("Help Not Available"), NULL, 0, _("This plugin does not include a README file."), NULL, NULL );
                else
                    xset_msg_dialog( dlgparent, 0, _("Creation Failed"), NULL, 0, _("An error occured creating a README file for this command."), NULL, NULL );
            }
            xset_edit( dlgparent, url, FALSE, TRUE );
            g_free( url );
            return;
        }
    }
    else if ( anchor )
    {
        url = g_strdup_printf( "%s%s", manual, anchor );
        xset_open_url( dlgparent, url );
        g_free( url );
    }
    else
        // just show the manual
        xset_open_url( dlgparent, manual );

    if ( manual )
        g_free( manual );

    if ( !xset_get_b( "main_help" ) )
    {
        xset_msg_dialog( dlgparent, 0, _("Manual Opened ?"), NULL, 0, _("The SpaceFM user's manual should have opened in your browser.  If it didn't open, or if you would like to use a different browser, set your browser in Help|Options|Browser.\n\nThis message will not repeat."), NULL, NULL );
        xset_set_b( "main_help", TRUE );
    }
}

char* xset_get_keyname( XSet* set, int key_val, int key_mod )
{
    int keyval, keymod;
    if ( set )
    {
        keyval = set->key;
        keymod = set->keymod;
    }
    else
    {
        keyval = key_val;
        keymod = key_mod;
    }
    if ( keyval <= 0 )
        return g_strdup( _("( none )") );
    char* mod = g_strdup( gdk_keyval_name( keyval ) );
    if ( mod && mod[0] && !mod[1] && g_ascii_isalpha( mod[0] ) )
        mod[0] = g_ascii_toupper( mod[0] );
    else if ( !mod )
        mod = g_strdup( "NA" );
    char* str;
    if ( keymod )
    {
        if ( keymod & GDK_SUPER_MASK )
        {
            str = mod;
            mod = g_strdup_printf( "Super+%s", str );
            g_free( str );
        }
        if ( keymod & GDK_HYPER_MASK )
        {
            str = mod;
            mod = g_strdup_printf( "Hyper+%s", str );
            g_free( str );
        }
        if ( keymod & GDK_META_MASK )
        {
            str = mod;
            mod = g_strdup_printf( "Meta+%s", str );
            g_free( str );
        }
        if ( keymod & GDK_MOD1_MASK )
        {
            str = mod;
            mod = g_strdup_printf( "Alt+%s", str );
            g_free( str );
        }
        if ( keymod & GDK_CONTROL_MASK )
        {
            str = mod;
            mod = g_strdup_printf( "Ctrl+%s", str );
            g_free( str );
        }
        if ( keymod & GDK_SHIFT_MASK )
        {
            str = mod;
            mod = g_strdup_printf( "Shift+%s", str );
            g_free( str );
        }
    }
    return mod;
}

gboolean on_set_key_keypress( GtkWidget *widget, GdkEventKey *event,
                                                            GtkWidget* dlg )
{
    GList* l;
    int* newkey = (int*)g_object_get_data( G_OBJECT(dlg), "newkey" );
    int* newkeymod = (int*)g_object_get_data( G_OBJECT(dlg), "newkeymod" );
    GtkWidget* btn = (GtkWidget*)g_object_get_data( G_OBJECT(dlg), "btn" );
    XSet* set = (XSet*)g_object_get_data( G_OBJECT(dlg), "set" );
    XSet* set2;
    XSet* keyset = NULL;
    char* keyname;
    
    int keymod = ( event->state & ( GDK_SHIFT_MASK | GDK_CONTROL_MASK |
                 GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK ) );

    
    if ( !event->keyval ) // || ( event->keyval < 1000 && !keymod ) )
    {
        *newkey = 0;
        *newkeymod = 0;
        gtk_widget_set_sensitive( btn, FALSE );
        gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG( dlg ), NULL );
        return TRUE;
    }
    
    gtk_widget_set_sensitive( btn, TRUE );

    if ( *newkey != 0 && keymod == 0 )
    {
        if ( event->keyval == GDK_KEY_Return || 
                                          event->keyval == GDK_KEY_KP_Enter )
        {
            // user pressed Enter after selecting a key, so click Set
            gtk_button_clicked( GTK_BUTTON( btn ) );
            return TRUE;
        }
        else if ( event->keyval == GDK_KEY_Escape && *newkey == GDK_KEY_Escape )
        {
            // user pressed Escape twice so click Unset
            GtkWidget* btn_unset = (GtkWidget*)g_object_get_data( G_OBJECT(dlg),
                                                                "btn_unset" );
            gtk_button_clicked( GTK_BUTTON( btn_unset ) );
            return TRUE;
        }
    }

    *newkey = 0;
    *newkeymod = 0;
    if ( set->shared_key )
        keyset = xset_get( set->shared_key );
            
    for ( l = xsets; l; l = l->next )
    {
        set2 = l->data;
        if ( set2 && set2 != set && set2->key > 0 && set2->key == event->keyval
                                    && set2->keymod == keymod && set2 != keyset )
        {
            char* name;
            if ( set2->desc && !strcmp( set2->desc, "@plugin@mirror@" )
                                                        && set2->shared_key )
            {
                // set2 is plugin mirror
                XSet* rset = xset_get( set2->shared_key );
                if ( rset->menu_label )
                    name = clean_label( rset->menu_label, FALSE, FALSE );
                else
                    name = g_strdup( "( no name )" );
            }
            else if ( set2->menu_label )
                name = clean_label( set2->menu_label, FALSE, FALSE );
            else
                name = g_strdup( "( no name )" );

            keyname = xset_get_keyname( NULL, event->keyval, keymod );
            gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG( dlg ), _("\t%s\n\tKeycode: %#4x  Modifier: %#x\n\n%s is already assigned to '%s'.\n\nPress a different key or click Set to replace the current key assignment."), keyname,
                                        event->keyval, keymod, keyname, name );
            g_free( name );
            g_free( keyname );
            *newkey = event->keyval;
            *newkeymod = keymod;
            return TRUE;
        }
    }
    keyname = xset_get_keyname( NULL, event->keyval, keymod );
    gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG( dlg ),
                                    _("\t%s\n\tKeycode: %#4x  Modifier: %#x"),
                                    keyname, event->keyval, keymod );
    g_free( keyname );
    *newkey = event->keyval;
    *newkeymod = keymod;
    return TRUE;
}

void xset_set_key( GtkWidget* parent, XSet* set )
{
    char* name;
    char* keymsg;
    XSet* keyset;
    int newkey = 0, newkeymod = 0;
    GtkWidget* dlgparent = NULL;

    if ( set->menu_label )
        name = clean_label( set->menu_label, FALSE, TRUE );
    else if ( g_str_has_prefix( set->name, "open_all_type_" ) )
    {
        keyset = xset_get( "open_all" );
        name = clean_label( keyset->menu_label, FALSE, TRUE );
        if ( set->shared_key )
            g_free( set->shared_key );
        set->shared_key = g_strdup( "open_all" );
    }
    else
        name = g_strdup( "( no name )" );
    keymsg = g_strdup_printf( _("Press your key combination for menu item '%s' then click Set.  To remove the current key assignment, click Unset."), name );
    g_free( name );
    if ( parent )
        dlgparent = gtk_widget_get_toplevel( parent );
    GtkWidget* dlg = gtk_message_dialog_new_with_markup( GTK_WINDOW( dlgparent ),
                                  GTK_DIALOG_MODAL,
                                  GTK_MESSAGE_QUESTION,
                                  GTK_BUTTONS_NONE,
                                  keymsg, NULL );
    xset_set_window_icon( GTK_WINDOW( dlg ) );

    GtkWidget* btn_cancel = gtk_button_new_from_stock( GTK_STOCK_CANCEL );
    gtk_button_set_label( GTK_BUTTON( btn_cancel ), _("Cancel") );
    gtk_button_set_image( GTK_BUTTON( btn_cancel ), xset_get_image( "GTK_STOCK_CANCEL",
                                                    GTK_ICON_SIZE_BUTTON ) );
    gtk_dialog_add_action_widget( GTK_DIALOG( dlg ), btn_cancel, GTK_RESPONSE_CANCEL);

    GtkWidget* btn_unset = gtk_button_new_from_stock( GTK_STOCK_NO );
    gtk_button_set_label( GTK_BUTTON( btn_unset ), _("Unset") );
    gtk_button_set_image( GTK_BUTTON( btn_unset ), xset_get_image( "GTK_STOCK_REMOVE",
                                                    GTK_ICON_SIZE_BUTTON ) );
    gtk_dialog_add_action_widget( GTK_DIALOG( dlg ), btn_unset, GTK_RESPONSE_NO);

    if ( set->shared_key )
        keyset = xset_get( set->shared_key );
    else
        keyset = set;
    if ( keyset->key <= 0 )
        gtk_widget_set_sensitive( btn_unset, FALSE );

    GtkWidget* btn = gtk_button_new_from_stock( GTK_STOCK_APPLY );
    gtk_button_set_label( GTK_BUTTON( btn ), _("Set") );
    gtk_button_set_image( GTK_BUTTON( btn ), xset_get_image( "GTK_STOCK_YES",
                                                    GTK_ICON_SIZE_BUTTON ) );
    gtk_dialog_add_action_widget( GTK_DIALOG( dlg ), btn, GTK_RESPONSE_OK);
    gtk_widget_set_sensitive( btn, FALSE );
    
    g_object_set_data( G_OBJECT(dlg), "set", set );
    g_object_set_data( G_OBJECT(dlg), "newkey", &newkey );
    g_object_set_data( G_OBJECT(dlg), "newkeymod", &newkeymod );
    g_object_set_data( G_OBJECT(dlg), "btn", btn );
    g_object_set_data( G_OBJECT(dlg), "btn_unset", btn_unset );
    g_signal_connect ( dlg, "key_press_event",
                               G_CALLBACK ( on_set_key_keypress ), dlg );
    gtk_widget_show_all( dlg );
    gtk_window_set_title( GTK_WINDOW( dlg ), _("Set Key") );
    
    int response = gtk_dialog_run( GTK_DIALOG( dlg ) );
    gtk_widget_destroy( dlg );
    if ( response == GTK_RESPONSE_OK || response == GTK_RESPONSE_NO )
    {
        if ( response == GTK_RESPONSE_OK && ( newkey || newkeymod ) )
        {
            // clear duplicate key assignments
            GList* l;
            XSet* set2;
            for ( l = xsets; l; l = l->next )
            {
                set2 = l->data;
                if ( set2 && set2->key > 0 && set2->key == newkey
                                            && set2->keymod == newkeymod )
                {
                    set2->key = 0;
                    set2->keymod = 0;
                }
            }
        }
        else if ( response == GTK_RESPONSE_NO )
        {
            newkey = -1;  // unset
            newkeymod = 0;
        }
        // plugin? set shared_key to mirror if not
        if ( set->plugin && !set->shared_key )
            xset_get_plugin_mirror( set );
        // set new key
        if ( set->shared_key )
            keyset = xset_get( set->shared_key );
        else
            keyset = set;
        keyset->key = newkey;
        keyset->keymod = newkeymod;
    }
}

void xset_design_job( GtkWidget* item, XSet* set )
{
    char* keymsg;
    GtkWidget* vbox;
    int newkey = 0, newkeymod = 0;
    XSet* keyset;
    XSet* newset;
    XSet* mset;
    XSet* childset;
    XSet* set_next;
    char* msg;
    int response;
    char* folder;
    char* file;
    char* custom_file;
    char* cscript;
    char* name;
    char* prog;
    char* command;
    int buttons;
    GtkWidget* dlgparent = NULL;
    GtkWidget* dlg;
    GtkClipboard* clip;
    GtkWidget* parent = NULL;
    
    if ( set->browser )
        parent = gtk_widget_get_toplevel( GTK_WIDGET( set->browser ) );
    else if ( set->desktop )
        parent = gtk_widget_get_toplevel( GTK_WIDGET( set->desktop ) );
    
    int job = GPOINTER_TO_INT( g_object_get_data( G_OBJECT(item), "job" ) );

//printf("activate job %d %s\n", job, set->name);    
    switch ( job ) {
    case XSET_JOB_KEY:
        xset_set_key( parent, set );
        break;
    case XSET_JOB_ICON:
        mset = xset_get_plugin_mirror( set );
        char* old_icon = g_strdup( mset->icon );
        xset_text_dialog( parent, _("Change Icon"), NULL, FALSE, _(icon_desc), NULL,
                                                        mset->icon, &mset->icon,
                                                                NULL, FALSE, "#designmode-designmenu-icon" );
        if ( set->lock && set->keep_terminal == XSET_B_UNSET &&
                                        g_strcmp0( old_icon, mset->icon ) )
        {
            // built-in icon has been changed from default, save it
            set->keep_terminal = XSET_B_TRUE;
        }
        g_free( old_icon );
        break;
    case XSET_JOB_LABEL:
        /*  unused - note that this does not accommodate in_terminal indicator
        if ( g_str_has_prefix( set->name, "open_all_type_" ) )
            keyset = xset_get( "open_all" );
        else
            keyset = set;
        xset_text_dialog( parent, _("Change Menu Name"), NULL, FALSE, _(enter_menu_name),
                                        NULL, keyset->menu_label, &keyset->menu_label,
                                        NULL, FALSE, "#designmode-designmenu-name" );
        */
        break;
    case XSET_JOB_EDIT:
        if ( atoi( set->x ) == 0 )
        {
            // line
            xset_text_dialog( parent, _("Edit Command Line"), NULL, TRUE, 
                                    _(enter_command_line), NULL, set->line, &set->line,
                                    NULL, FALSE, "#designmode-command-line" );
        }
        else if ( atoi( set->x ) == 1 )
        {
            // script
            cscript = xset_custom_get_script( set, !set->plugin );
            if ( !cscript )
                break;
            xset_edit( parent, cscript, FALSE, TRUE );
            g_free( cscript );
        }
        else if ( atoi( set->x ) == 2 )
        {
            // custom
            if ( !set->z || !g_file_test( set->z, G_FILE_TEST_EXISTS ) )
                goto _XSET_JOB_CUSTOM;
            if ( mime_type_is_text_file( set->z, NULL ) )
                xset_edit( parent, set->z, FALSE, TRUE );
            else
                goto _XSET_JOB_CUSTOM;
        }
        break;
    case XSET_JOB_EDIT_ROOT:
        if ( atoi( set->x ) == 1 )
        {
            // script
            cscript = xset_custom_get_script( set, !set->plugin );
            if ( !cscript )
                break;
            xset_edit( parent, cscript, TRUE, FALSE );
            g_free( cscript );
        }
        else if ( atoi( set->x ) == 2 )
        {
            // custom
            if ( !set->z || !g_file_test( set->z, G_FILE_TEST_EXISTS ) )
                goto _XSET_JOB_CUSTOM;
            if ( mime_type_is_text_file( set->z, NULL ) )
                xset_edit( parent, set->z, TRUE, FALSE );
            else
                goto _XSET_JOB_CUSTOM;
        }
        break;
    case XSET_JOB_COPYNAME:
        clip = gtk_clipboard_get( GDK_SELECTION_CLIPBOARD );
        if ( atoi( set->x ) == 0 )
        {
            // line
            gtk_clipboard_set_text ( clip, set->line , -1 );
        }
        else if ( atoi( set->x ) == 1 )
        {
            // script
            cscript = xset_custom_get_script( set, TRUE );
            if ( !cscript )
                break;
            gtk_clipboard_set_text ( clip, cscript , -1 );
            g_free( cscript );
        }
        else if ( atoi( set->x ) == 2 )
        {
            // custom
            gtk_clipboard_set_text ( clip, set->z , -1 );
        }
        break;
    case XSET_JOB_LINE:
        if ( xset_text_dialog( parent, _("Edit Command Line"), NULL, TRUE, 
                                _(enter_command_line), NULL, set->line, &set->line,
                                NULL, FALSE, "#designmode-command-line" ) )
            xset_set_set( set, "x", "0" );
        break;
    case XSET_JOB_SCRIPT:
        xset_set_set( set, "x", "1" ); 
        cscript = xset_custom_get_script( set, TRUE );
        if ( !cscript )
            break;
        xset_edit( parent, cscript, FALSE, FALSE );
        g_free( cscript );
        break;
    case XSET_JOB_CUSTOM:
        _XSET_JOB_CUSTOM:
        if ( set->z && set->z[0] != '\0' )
        {
            folder = g_path_get_dirname( set->z );
            file = g_path_get_basename( set->z );
        }
        else
        {
            folder = g_strdup_printf( "/usr/bin" );
            file = NULL;
        }
        if ( custom_file = xset_file_dialog( parent, GTK_FILE_CHOOSER_ACTION_OPEN,
                            _("Choose Custom Executable"), folder, file ) )
        {
            xset_set_set( set, "x", "2" ); 
            xset_set_set( set, "z", custom_file );
            g_free( custom_file );
        }
        g_free( file );
        g_free( folder );
        break;
    case XSET_JOB_USER:
        if ( !set->plugin )
            xset_text_dialog( parent, _("Run As User"), NULL, FALSE, _("Run this command as username:\n\n( Leave blank for current user )"), NULL, set->y, &set->y, NULL, FALSE, "#designmode-command-user" );    
        break;
    case XSET_JOB_BOOKMARK:
    case XSET_JOB_APP:
    case XSET_JOB_COMMAND:
        if ( g_str_has_prefix( set->name, "open_all_type_" ) )
        {
            name = set->name + 14;
            msg = g_strdup_printf( _("You are adding a custom command to the Default menu item.  This item will automatically have a pre-context - it will only appear when the MIME type of the first selected file matches the current type '%s'.\n\nAdd commands or menus here which you only want to appear for this one MIME type."), name[0] == '\0' ? "(none)" : name );
            if ( xset_msg_dialog( parent, 0, _("New Context Command"), NULL,
                            GTK_BUTTONS_OK_CANCEL, msg, NULL, NULL ) != GTK_RESPONSE_OK )
            {
                g_free( msg );
                break;
            }
            g_free( msg );
        }
        if ( job == XSET_JOB_COMMAND )
        {
            name = g_strdup_printf( _("New _Command") );
            if ( !xset_text_dialog( parent, _("Set Menu Name"), NULL, FALSE, _(enter_menu_name_new),
                                                            NULL, name, &name,
                                                            NULL, FALSE,
                                                            "#designmode-designmenu-new" ) )
            {
                g_free( name );
                break;
            }
            file = NULL;
        }
        else if ( job == XSET_JOB_APP )
        {
            VFSMimeType* mime_type = vfs_mime_type_get_from_type( 
                    xset_context &&
                    xset_context->var[CONTEXT_MIME] &&
                    xset_context->var[CONTEXT_MIME][0] ?
                    xset_context->var[CONTEXT_MIME] : XDG_MIME_TYPE_UNKNOWN );
            file = (char*)ptk_choose_app_for_mime_type(
                            GTK_WINDOW( parent ),
                            mime_type, TRUE, FALSE, FALSE, FALSE );
            vfs_mime_type_unref( mime_type );            
            if ( !( file && file[0] ) )
            {
                g_free( file );
                break;
            }
            name = NULL;
        }
        else if ( job == XSET_JOB_BOOKMARK )
        {
            if ( set->browser )
                folder = (char*)ptk_file_browser_get_cwd( set->browser );
            else
                folder = NULL;
            file = xset_file_dialog( parent,
                                GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                _("Choose Folder"), folder, NULL );
            if ( !( file && file[0] ) )
            {
                g_free( file );
                break;
            }
            name = g_path_get_basename( file );
        }
        newset = xset_custom_new();
        newset->z = file;
        newset->prev = g_strdup( set->name );
        newset->next = set->next;
        if ( set->next )
        {
            set_next = xset_get( set->next );
            if ( set_next->prev )
                g_free( set_next->prev );
            set_next->prev = g_strdup( newset->name );
        }
        set->next = g_strdup( newset->name );
        newset->menu_label = name;
        if ( set->tool )
        {
            newset->tool = XSET_B_TRUE;
            if ( job == XSET_JOB_COMMAND )
                newset->icon = g_strdup( "gtk-execute" );
        }
        newset->browser = set->browser;
        newset->desktop = set->desktop;
        if ( job == XSET_JOB_COMMAND )
            xset_item_prop_dlg( xset_context, newset, 2 );
        else if ( job == XSET_JOB_APP )
        {
            g_free( newset->x );
            newset->x = g_strdup( "2" ); // app
        }
        else if ( job == XSET_JOB_BOOKMARK )
        {
            g_free( newset->x );
            newset->x = g_strdup( "3" ); // bookmark
        }
        break;
    case XSET_JOB_SUBMENU:
        if ( g_str_has_prefix( set->name, "open_all_type_" ) )
        {
            name = set->name + 14;
            msg = g_strdup_printf( _("You are adding a custom submenu to the Default menu item.  This item will automatically have a pre-context - it will only appear when the MIME type of the first selected file matches the current type '%s'.\n\nAdd commands or menus here which you only want to appear for this one MIME type."), name[0] == '\0' ? _("(none)") : name );
            if ( xset_msg_dialog( parent, 0, "New Context Submenu", NULL, GTK_BUTTONS_OK_CANCEL, msg, NULL, NULL ) != GTK_RESPONSE_OK )
            {
                g_free( msg );
                break;
            }
            g_free( msg );
        }
        name = NULL;
        if ( !xset_text_dialog( parent, _("Set Submenu Name"), NULL, FALSE, _("Enter submenu name:\n\nPrecede a character with an underscore (_) to underline that character as a shortcut key if desired."), NULL, _("New _Submenu"), &name, NULL, FALSE, "#designmode-designmenu-name" ) || !name )
            break;
        newset = xset_custom_new();
        newset->prev = g_strdup( set->name );
        newset->next = set->next;
        if ( set->next )
        {
            set_next = xset_get( set->next );
            if ( set_next->prev )
                g_free( set_next->prev );
            set_next->prev = g_strdup( newset->name );
        }
        set->next = g_strdup( newset->name );
        newset->menu_label = name;
        newset->menu_style = XSET_MENU_SUBMENU;
        if ( set->tool )
        {
            newset->tool = XSET_B_TRUE;
            newset->icon = g_strdup_printf( "gtk-execute" );
        }
        if ( newset->tool )
            newset->icon = g_strdup_printf( "gtk-execute" );
        childset = xset_custom_new();
        newset->child = g_strdup( childset->name );
        childset->parent = g_strdup( newset->name );
        childset->menu_label = g_strdup_printf( _("New _Command") );
        if ( set->tool )
        {
            childset->tool = XSET_B_TRUE;
            childset->icon = g_strdup_printf( "gtk-execute" );
        }
        break;
    case XSET_JOB_SEP:
        newset = xset_custom_new();
        newset->prev = g_strdup( set->name );
        newset->next = set->next;
        if ( set->next )
        {
            set_next = xset_get( set->next );
            if ( set_next->prev )
                g_free( set_next->prev );
            set_next->prev = g_strdup( newset->name );
        }
        set->next = g_strdup( newset->name );
        newset->menu_style = XSET_MENU_SEP;
        if ( set->tool )
            newset->tool = XSET_B_TRUE;
        break;
    case XSET_JOB_IMPORT_FILE:
    case XSET_JOB_IMPORT_URL:
        if ( job == XSET_JOB_IMPORT_FILE )
        {
            // get file path
            XSet* save = xset_get( "plug_ifile" );
            if ( save->s )  //&& g_file_test( save->s, G_FILE_TEST_IS_DIR )
                folder = save->s;
            else
            {
                if ( !( folder = xset_get_s( "go_set_default" ) ) )
                    folder = "/";
            }
            file = xset_file_dialog( GTK_WIDGET( parent ),
                                            GTK_FILE_CHOOSER_ACTION_OPEN,
                                            _("Choose Plugin File"),
                                            folder, NULL );
            if ( !file )
                break;
            if ( save->s )
                g_free( save->s );
            save->s = g_path_get_dirname( file );
        }
        else
        {
            // Get URL
            file = NULL;
            if ( !xset_text_dialog( GTK_WIDGET( parent ), _("Enter Plugin URL"), NULL, FALSE, _("Enter SpaceFM Plugin URL:\n\n(wget will be used to download the plugin file)"), NULL, NULL, &file, NULL, FALSE, "#designmode-designmenu-import" ) || !file || file[0] == '\0' )
                break;
        }
        // Make Plugin Dir
        const char* user_tmp = xset_get_user_tmp_dir();
        if ( !user_tmp )
        {
            xset_msg_dialog( GTK_WIDGET( parent ), GTK_MESSAGE_ERROR,
                                _("Error Creating Temp Directory"), NULL, 0, 
                                _("Unable to create temporary directory"), NULL,
                                NULL );
            g_free( file );
            break;
        }
        char* hex8;
        folder = NULL;
        while ( !folder || ( folder && g_file_test( folder,
                                                    G_FILE_TEST_EXISTS ) ) )
        {
            hex8 = randhex8();
            if ( folder )
                g_free( folder );
            folder = g_build_filename( user_tmp, hex8, NULL );
            g_free( hex8 );
        }
        install_plugin_file( set->browser ? set->browser->main_window : NULL,
                             file, folder,
                             job == XSET_JOB_IMPORT_FILE ? 0 : 1, 1, set );                             
        g_free( file );
        g_free( folder );
        break;
    case XSET_JOB_CUT:
        set_clipboard = set;
        clipboard_is_cut = TRUE;
        break;
    case XSET_JOB_COPY:
        set_clipboard = set;
        clipboard_is_cut = FALSE;
        break;
    case XSET_JOB_PASTE:
        if ( !set_clipboard )
            break;
        if ( clipboard_is_cut )
        {
            xset_custom_remove( set_clipboard );
            set_clipboard->prev = g_strdup( set->name );
            set_clipboard->next = set->next;  //swap string
            if ( set->next )
            {
                set_next = xset_get( set->next );
                if ( set_next->prev )
                    g_free( set_next->prev );
                set_next->prev = g_strdup( set_clipboard->name );
            }
            set->next = g_strdup( set_clipboard->name );
            if ( set->tool )
            {
                set_clipboard->tool = XSET_B_TRUE;
                if ( !set_clipboard->icon )
                    set_clipboard->icon = g_strdup_printf( "gtk-execute" );
            }
            else
                set_clipboard->tool = XSET_B_UNSET;
            set_clipboard = NULL;
        }
        else
        {
            newset = xset_custom_copy( set_clipboard, FALSE );
            newset->prev = g_strdup( set->name );
            newset->next = set->next;
            if ( set->next )
            {
                set_next = xset_get( set->next );
                if ( set_next->prev )
                    g_free( set_next->prev );
                set_next->prev = g_strdup( newset->name );
            }
            set->next = g_strdup( newset->name );
            if ( set->tool )
            {
                newset->tool = XSET_B_TRUE;
                if ( !newset->icon )
                    newset->icon = g_strdup_printf( "gtk-execute" );
            }
            else
                newset->tool = XSET_B_UNSET;
        }
        break;
    case XSET_JOB_REMOVE:
        if ( set->plugin )
        {
            xset_remove_plugin( parent, set->browser, set );
            break;
        }
        if ( set->menu_label )
        {
            name = clean_label( set->menu_label, FALSE, FALSE );
        }
        else
            name = g_strdup( _("( no name )") );
        if ( set->child && set->menu_style == XSET_MENU_SUBMENU )
        {
            msg = g_strdup_printf( _("Permanently remove the '%s' SUBMENU AND ALL ITEMS WITHIN IT?\n\nThis action will delete all settings and files associated with these items."), name );
            buttons = GTK_BUTTONS_YES_NO;
        }
        else
        {
            msg = g_strdup_printf( _("Permanently remove the '%s' item?\n\nThis action will delete all settings and files associated with this item."), name );
            buttons = GTK_BUTTONS_OK_CANCEL;
        }
        g_free( name );
        if ( set->menu_style != XSET_MENU_SEP && !app_settings.no_confirm )
        {
            if ( parent )
                dlgparent = gtk_widget_get_toplevel( parent );
            dlg = gtk_message_dialog_new( GTK_WINDOW( dlgparent ),
                                          GTK_DIALOG_MODAL,
                                          GTK_MESSAGE_WARNING,
                                          buttons,
                                          msg, NULL );
            xset_set_window_icon( GTK_WINDOW( dlg ) );
            gtk_window_set_title( GTK_WINDOW( dlg ), _("Confirm Remove") );
            gtk_widget_show_all( dlg );
            response = gtk_dialog_run( GTK_DIALOG( dlg ) );
            gtk_widget_destroy( dlg );
            if ( response == GTK_RESPONSE_CANCEL )
                break;
        }
        g_free( msg );

        xset_custom_remove( set );
        xset_custom_delete( set, FALSE );
        break;
    case XSET_JOB_EXPORT:
        if ( !set->lock )
            xset_custom_export( parent, set->browser, set );
        break;
    case XSET_JOB_NORMAL:
        set->menu_style = XSET_MENU_NORMAL;
        break;
    case XSET_JOB_CHECK:
        set->menu_style = XSET_MENU_CHECK;
        break;
    case XSET_JOB_CONFIRM:
        if ( !set->desc )
            set->desc = g_strdup( _("Are you sure?") );
        if ( xset_text_dialog( parent, _("Dialog Message"), NULL, TRUE, _("Enter the message to be displayed in this dialog:\n\nUse:\n\t\\n\tnewline\n\t\\t\ttab"), NULL, set->desc, &set->desc, NULL, FALSE, "#designmode-style-input" ) )
            set->menu_style = XSET_MENU_CONFIRM;
        break;
    case XSET_JOB_DIALOG:
        if ( xset_text_dialog( parent, _("Dialog Message"), NULL, TRUE, _("Enter the message to be displayed in this dialog:\n\nUse:\n\t\\n\tnewline\n\t\\t\ttab"), NULL, set->desc, &set->desc, NULL, FALSE, "#designmode-style-input" ) )
            set->menu_style = XSET_MENU_STRING;
        break;
    case XSET_JOB_MESSAGE:
        xset_text_dialog( parent, _("Dialog Message"), NULL, TRUE, _("Enter the message to be displayed in this dialog:\n\nUse:\n\t\\n\tnewline\n\t\\t\ttab"), NULL, set->desc, &set->desc, NULL, FALSE, "#designmode-style-message" );
        break;
    case XSET_JOB_PROP:
        xset_item_prop_dlg( xset_context, set, 0 );
        break;
    case XSET_JOB_PROP_CMD:
        xset_item_prop_dlg( xset_context, set, 2 );
        break;
    case XSET_JOB_IGNORE_CONTEXT:
        xset_set_b( "context_dlg", !xset_get_b( "context_dlg" ) );
        break;
    case XSET_JOB_HELP:
        if ( parent )
            dlgparent = gtk_widget_get_toplevel( parent );
        xset_show_help( dlgparent, set, NULL );
        break;
    case XSET_JOB_BROWSE_FILES:
        if ( set->plugin )
        {
            folder = g_build_filename( set->plug_dir, "files", NULL );
            if ( !g_file_test( folder, G_FILE_TEST_EXISTS ) )
            {
                g_free( folder );
                folder = g_build_filename( set->plug_dir, set->plug_name, NULL );
            }
        }
        else
        {
            cscript = xset_custom_get_script( set, FALSE );  //backwards compat copy
            if ( cscript )
                g_free( cscript );
            folder = g_build_filename( settings_config_dir, "scripts",
                                                            set->name, NULL );
        }
        if ( !g_file_test( folder, G_FILE_TEST_EXISTS ) && !set->plugin )
        {
            g_mkdir_with_parents( folder, 0700 );
            chmod( folder, 0700 );
        }
        
        if ( set->browser )
        {
            ptk_file_browser_emit_open( set->browser, folder, PTK_OPEN_DIR );
        }
        else if ( set->desktop )
        {
            prog = g_find_program_in_path( g_get_prgname() );
            if ( !prog )
                prog = g_strdup( g_get_prgname() );
            if ( !prog )
                prog = g_strdup( "spacefm" );
            
            command = g_strdup_printf( "%s %s", prog, folder );
            g_spawn_command_line_sync( command, NULL, NULL, NULL, NULL );
            g_free( prog );
            g_free( command );
            g_free( folder );
        }
        break;
    case XSET_JOB_BROWSE_DATA:
        if ( set->plugin )
        {
            mset = xset_get_plugin_mirror( set );
            folder = g_build_filename( settings_config_dir, "plugin-data",
                                                        mset->name, NULL );
        }
        else
            folder = g_build_filename( settings_config_dir, "plugin-data",
                                                        set->name, NULL );
        if ( !g_file_test( folder, G_FILE_TEST_EXISTS ) )
        {
            g_mkdir_with_parents( folder, 0700 );
            chmod( folder, 0700 );
        }
        
        if ( set->browser )
        {
            ptk_file_browser_emit_open( set->browser, folder, PTK_OPEN_DIR );
        }
        else if ( set->desktop )
        {
            prog = g_find_program_in_path( g_get_prgname() );
            if ( !prog )
                prog = g_strdup( g_get_prgname() );
            if ( !prog )
                prog = g_strdup( "spacefm" );
            
            command = g_strdup_printf( "%s %s", prog, folder );
            g_spawn_command_line_sync( command, NULL, NULL, NULL, NULL );
            g_free( prog );
            g_free( command );
            g_free( folder );
        }
        break;
    case XSET_JOB_BROWSE_PLUGIN:
        if ( set->plugin && set->plug_dir )
        {
            if ( set->browser )
            {
                ptk_file_browser_emit_open( set->browser, set->plug_dir, PTK_OPEN_DIR );
            }
            else if ( set->desktop )  // should never happen in current version
            {
                prog = g_find_program_in_path( g_get_prgname() );
                if ( !prog )
                    prog = g_strdup( g_get_prgname() );
                if ( !prog )
                    prog = g_strdup( "spacefm" );
                
                command = g_strdup_printf( "%s %s", prog, set->plug_dir );
                g_spawn_command_line_sync( command, NULL, NULL, NULL, NULL );
                g_free( prog );
                g_free( command );
            }
        }
        break;
    case XSET_JOB_TERM:
        mset = xset_get_plugin_mirror( set );
        if ( mset->in_terminal == XSET_B_TRUE )
            mset->in_terminal = XSET_B_UNSET;
        else
        {
            mset->in_terminal = XSET_B_TRUE;
            mset->task = XSET_B_FALSE;
        }
        break;
    case XSET_JOB_KEEP:
        mset = xset_get_plugin_mirror( set );
        if ( mset->keep_terminal == XSET_B_TRUE )
            mset->keep_terminal = XSET_B_UNSET;
        else
            mset->keep_terminal = XSET_B_TRUE;
        break;
    case XSET_JOB_TASK:
        mset = xset_get_plugin_mirror( set );
        if ( mset->task == XSET_B_TRUE )
            mset->task = XSET_B_UNSET;
        else
            mset->task = XSET_B_TRUE;
        break;
    case XSET_JOB_POP:
        mset = xset_get_plugin_mirror( set );
        if ( mset->task_pop == XSET_B_TRUE )
            mset->task_pop = XSET_B_UNSET;
        else
            mset->task_pop = XSET_B_TRUE;
        break;
    case XSET_JOB_ERR:
        mset = xset_get_plugin_mirror( set );
        if ( mset->task_err == XSET_B_TRUE )
            mset->task_err = XSET_B_UNSET;
        else
            mset->task_err = XSET_B_TRUE;
        break;
    case XSET_JOB_OUT:
        mset = xset_get_plugin_mirror( set );
        if ( mset->task_out == XSET_B_TRUE )
            mset->task_out = XSET_B_UNSET;
        else
            mset->task_out = XSET_B_TRUE;
        break;
    case XSET_JOB_SCROLL:
        mset = xset_get_plugin_mirror( set );
        if ( mset->scroll_lock == XSET_B_TRUE )
            mset->scroll_lock = XSET_B_UNSET;
        else
            mset->scroll_lock = XSET_B_TRUE;
        break;
    case XSET_JOB_SHOW:
        if ( gtk_check_menu_item_get_active( GTK_CHECK_MENU_ITEM( item ) ) )
            set->tool = XSET_B_TRUE;
        else
            set->tool = XSET_B_FALSE;
        break;
    }

    //if ( set->plugin )
    //    main_window_on_plugins_change( NULL );

    // autosave
    xset_autosave( set->browser, FALSE, FALSE );
}

void on_design_radio_toggled( GtkCheckMenuItem* item, XSet* set )
{
     if ( gtk_check_menu_item_get_active( GTK_CHECK_MENU_ITEM( item ) ) )
        xset_design_job( GTK_WIDGET( item ), set );
}

gboolean xset_job_is_valid( XSet* set, int job )
{
    gboolean no_remove = FALSE;
    gboolean toolexecsub = FALSE;
    gboolean no_paste = FALSE;
    gboolean open_all = FALSE;
    XSet* sett;

    if ( !set )
        return FALSE;

    if ( set->plugin )
    {
        if ( !set->plug_dir )
            return FALSE;
        if ( !set->plugin_top || strstr( set->plug_dir, "/included/" ) )
            no_remove = TRUE;
    }
    
    // only first level custom tool submenu is executable 
    if ( set->tool && !set->lock && set->menu_style == XSET_MENU_SUBMENU )
    {
        sett = set;
        while ( sett->prev )
        {
            sett = xset_get( sett->prev );
            if ( sett->lock )
            {
                toolexecsub = TRUE;
                break;
            }
        }
        if ( !toolexecsub && sett->parent )
        {
            sett = xset_get( sett->parent );
            if ( sett->lock )
                toolexecsub = TRUE;
        }
    }

    if ( set == set_clipboard )
    {
        if ( clipboard_is_cut )
            // don't allow cut paste to self
            no_paste = TRUE;
    }
    else if ( set_clipboard && set_clipboard->menu_style == XSET_MENU_SUBMENU )
        // don't allow paste of submenu to self or below
        no_paste = xset_clipboard_in_set( set );
    
    // control open_all item
    if ( g_str_has_prefix( set->name, "open_all_type_" ) )
        open_all = TRUE;

    switch ( job ) {
    case XSET_JOB_KEY:
        return ( set->menu_style < XSET_MENU_SUBMENU || toolexecsub );
    case XSET_JOB_ICON:
        return ( ( set->menu_style == XSET_MENU_NORMAL 
                                        || set->menu_style == XSET_MENU_STRING 
                                        || set->menu_style == XSET_MENU_FONTDLG
                                        || set->menu_style == XSET_MENU_COLORDLG
                                        || set->menu_style == XSET_MENU_SUBMENU
                                        || set->tool ) && !open_all );
    case XSET_JOB_EDIT:
        return ( !set->lock && (
                        set->menu_style < XSET_MENU_SUBMENU || toolexecsub ) );
    case XSET_JOB_COMMAND:
        return !set->plugin;
    case XSET_JOB_CUT:
        return ( !set->lock && !set->plugin );
    case XSET_JOB_COPY:
         return !set->lock;
    case XSET_JOB_PASTE:
        return ( set_clipboard && !no_paste && !set->plugin 
                        && !( set->tool && set_clipboard->menu_style ==
                                                    XSET_MENU_SUBMENU ) );
    case XSET_JOB_REMOVE:
        return ( !set->lock && !no_remove );
    //case XSET_JOB_CONTEXT:
    //    return ( xset_context && xset_context->valid && !open_all );
    case XSET_JOB_PROP:
    case XSET_JOB_PROP_CMD:
        return TRUE;
    case XSET_JOB_HELP:
        return ( !set->lock || ( set->lock && set->line ) );
    }
    return FALSE;
}

gboolean xset_design_menu_keypress( GtkWidget* widget, GdkEventKey* event,
                                                                XSet* set )
{
    int job = -1;

    GtkWidget* item = gtk_menu_shell_get_selected_item( GTK_MENU_SHELL( widget ) );
    if ( !item )
        return FALSE;
    
    int keymod = ( event->state & ( GDK_SHIFT_MASK | GDK_CONTROL_MASK |
                 GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK ) );
    
    if ( keymod == 0 )
    {        
        if ( event->keyval == GDK_KEY_F1 )
        {
            char* help = NULL;
            job = GPOINTER_TO_INT( g_object_get_data( G_OBJECT(item), "job" ) );
            switch ( job ) {
            case XSET_JOB_KEY:
                help = "#designmode-designmenu-key";
                break;
            case XSET_JOB_ICON:
                help = "#designmode-designmenu-icon";
                break;
            case XSET_JOB_LABEL:
                help = "#designmode-designmenu-name";
                break;
            case XSET_JOB_EDIT:
                help = "#designmode-command-edit";
                break;
            case XSET_JOB_EDIT_ROOT:
                help = "#designmode-command-edit";
                break;
            case XSET_JOB_COPYNAME:
                help = "#designmode-command-copy";
                break;
            case XSET_JOB_LINE:
                help = "#designmode-command-line";
                break;
            case XSET_JOB_SCRIPT:
                help = "#designmode-command-script";
                break;
            case XSET_JOB_CUSTOM:
                help = "#designmode-command-custom";
                break;
            case XSET_JOB_USER:
                help = "#designmode-command-user";
                break;
            case XSET_JOB_COMMAND:
                help = "#designmode-designmenu-new";
                break;
            case XSET_JOB_SUBMENU:
                help = "#designmode-designmenu-submenu";
                break;
            case XSET_JOB_SEP:
                help = "#designmode-designmenu-separator";
                break;
            case XSET_JOB_IMPORT_FILE:
            case XSET_JOB_IMPORT_URL:
                help = "#designmode-designmenu-import";
                break;
            case XSET_JOB_CUT:
                help = "#designmode-designmenu-cut";
                break;
            case XSET_JOB_COPY:
                help = "#designmode-designmenu-copy";
                break;
            case XSET_JOB_PASTE:
                help = "#designmode-designmenu-paste";
                break;
            case XSET_JOB_REMOVE:
                help = "#designmode-designmenu-remove";
                break;
            case XSET_JOB_EXPORT:
                help = "#designmode-designmenu-export";
                break;
            case XSET_JOB_BOOKMARK:
                help = "#designmode-designmenu-bookmark";
                break;
            case XSET_JOB_APP:
                help = "#designmode-designmenu-app";
                break;
            case XSET_JOB_NORMAL:
                help = "#designmode-style-normal";
                break;
            case XSET_JOB_CHECK:
                help = "#designmode-style-checkbox";
                break;
            case XSET_JOB_CONFIRM:
                help = "#designmode-style-confirm";
                break;
            case XSET_JOB_DIALOG:
                help = "#designmode-style-input";
                break;
            case XSET_JOB_MESSAGE:
                help = "#designmode-style-message";
                break;
            //case XSET_JOB_CONTEXT:
            //    help = "#designmode-props-context";
            //    break;
            case XSET_JOB_IGNORE_CONTEXT:
                help = "#designmode-props-ignorecontext";
                break;
            case XSET_JOB_HELP:
                help = "#designmode-designmenu-help";
                break;
            case XSET_JOB_HELP_STYLE:
                help = "#designmode-style";
                break;
            case XSET_JOB_HELP_NEW:
                help = "#designmode-designmenu-bookmark";
                break;
            case XSET_JOB_PROP:
                help = "#designmode-props";
                break;
            case XSET_JOB_HELP_BROWSE:
                help = "#designmode-command-browse";
                break;
            case XSET_JOB_BROWSE_FILES:
                help = "#designmode-command-browse-files";
                break;
            case XSET_JOB_BROWSE_DATA:
                help = "#designmode-command-browse-data";
                break;
            case XSET_JOB_BROWSE_PLUGIN:
                help = "#designmode-command-browse-plugin";
                break;
            case XSET_JOB_TERM:
                help = "#designmode-command-terminal";
                break;
            case XSET_JOB_KEEP:
                help = "#designmode-command-keep";
                break;
            case XSET_JOB_TASK:
                help = "#designmode-command-task";
                break;
            case XSET_JOB_POP:
                help = "#designmode-command-popup";
                break;
            case XSET_JOB_ERR:
                help = "#designmode-command-poperr";
                break;
            case XSET_JOB_OUT:
                help = "#designmode-command-popout";
                break;
            case XSET_JOB_SCROLL:
                help = "#designmode-command-scroll";
                break;
            case XSET_JOB_SHOW:
                help = "#designmode-designmenu-show";
                break;
            }
            if ( !help )
                help = "#designmode";
            gtk_menu_shell_deactivate( GTK_MENU_SHELL( widget ) );
            xset_show_help( NULL, NULL, help );
            return TRUE;
        }
        else if ( event->keyval == GDK_KEY_F3 )
            job = XSET_JOB_PROP;
        else if ( event->keyval == GDK_KEY_F4 )
            job = XSET_JOB_PROP_CMD;
        else if ( event->keyval == GDK_KEY_Delete )
            job = XSET_JOB_REMOVE;
        else if ( event->keyval == GDK_KEY_Insert )
            job = XSET_JOB_COMMAND;
    }
    else if ( keymod == GDK_CONTROL_MASK )
    {
        if ( event->keyval == GDK_KEY_c )
            job = XSET_JOB_COPY;
        else if ( event->keyval == GDK_KEY_x )
            job = XSET_JOB_CUT;
        else if ( event->keyval == GDK_KEY_v )
            job = XSET_JOB_PASTE;
        else if ( event->keyval == GDK_KEY_e )
        {
            if ( set->lock )
            {
                return FALSE;
            }
            else
                job = XSET_JOB_EDIT;
        }
        else if ( event->keyval == GDK_KEY_k )
            job = XSET_JOB_KEY;
        else if ( event->keyval == GDK_KEY_i )
            job = XSET_JOB_ICON;
    }
    if ( job != -1 )
    {
        if ( xset_job_is_valid( set, job ) )
        {
            gtk_menu_shell_deactivate( GTK_MENU_SHELL( widget ) );
            g_object_set_data( G_OBJECT( item ), "job", GINT_TO_POINTER( job ) );
            xset_design_job( item, set );
            return TRUE;
        }
    }
    return FALSE;
}

void xset_design_destroy( GtkWidget* item, GtkWidget* design_menu )
{
//printf( "xset_design_destroy\n");
    // close design_menu if menu deactivated
    gtk_widget_set_sensitive( item, TRUE );
    gtk_menu_shell_deactivate( GTK_MENU_SHELL( design_menu ) );
}

void on_menu_hide(GtkWidget *widget, GtkWidget* design_menu )
{
    gtk_widget_set_sensitive( widget, TRUE );
    gtk_menu_shell_deactivate( GTK_MENU_SHELL( design_menu ) );
}

static void set_check_menu_item_block( GtkWidget* item )
{
    g_signal_handlers_block_matched( item, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                                xset_design_job, NULL );
    gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( item ), TRUE );
    g_signal_handlers_unblock_matched( item, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                                xset_design_job, NULL );
}

GtkWidget* xset_design_additem( GtkWidget* menu, char* label, gchar* stock_icon,
                                                            int job, XSet* set )
{
    GtkWidget* item;
    if ( stock_icon )
    {
        if ( !strcmp( stock_icon, "@check" ) )
            item = gtk_check_menu_item_new_with_mnemonic( label );
        else
        {
            item = gtk_image_menu_item_new_with_mnemonic( label );
            GtkWidget* image = gtk_image_new_from_stock( stock_icon,
                                                    GTK_ICON_SIZE_MENU );
            if ( image )
                gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM( item ), 
                                                    image );
        }
    }
    else
        item = gtk_menu_item_new_with_mnemonic( label );

    g_object_set_data( G_OBJECT(item), "job", GINT_TO_POINTER( job ) );
    gtk_container_add ( GTK_CONTAINER ( menu ), item );
    g_signal_connect( item, "activate", G_CALLBACK( xset_design_job ), set );
    return item;
}

static void xset_design_show_menu( GtkWidget* menu, XSet* set, guint button, guint32 time )
{
    GtkWidget* newitem;
    GtkWidget* newitem2;
    GtkWidget* newitem3;
    GtkWidget* newitem4;
    GtkWidget* submenu;
    GtkWidget* submenu2;
    GSList* radio_group;
    char* label;
    char* path;
    gboolean no_remove = FALSE;
    gboolean toolexecsub = FALSE;
    gboolean toolshow = FALSE;
    gboolean no_paste = FALSE;
    gboolean open_all = FALSE;
    XSet* sett;
    XSet* mset;

    if ( set->plugin && set->shared_key )
        mset = xset_get_plugin_mirror( set );
    else
        mset = set;
        
    if ( set->plugin )
    {
        if ( set->plug_dir )
        {
            if ( !set->plugin_top || strstr( set->plug_dir, "/included/" ) )
                no_remove = TRUE;
        }
        else
            no_remove = TRUE;
    }
    
    // only first level custom tool submenu is executable
    if ( set->tool && !set->lock && set->menu_style == XSET_MENU_SUBMENU )
    {
        sett = set;
        while ( sett->prev )
        {
            sett = xset_get( sett->prev );
            if ( sett->lock )
            {
                toolexecsub = TRUE;
                break;
            }
        }
        if ( !toolexecsub && sett->parent )
        {
            sett = xset_get( sett->parent );
            if ( sett->lock )
                toolexecsub = TRUE;
        }
    }

    // show "Show"?
    if ( set->tool )
    {
        if ( set->lock )
            toolshow = TRUE;
        else
        {
            sett = set;
            while ( sett->prev )
            {
                sett = xset_get( sett->prev );
                if ( sett->lock )
                {
                    toolshow = TRUE;
                    break;
                }
            }
        }
    }

    if ( set == set_clipboard )
    {
        if ( clipboard_is_cut )
            // don't allow cut paste to self
            no_paste = TRUE;
    }
    else if ( set_clipboard && set_clipboard->menu_style == XSET_MENU_SUBMENU )
        // don't allow paste of submenu to self or below
        no_paste = xset_clipboard_in_set( set );
    
    // control open_all item
    if ( g_str_has_prefix( set->name, "open_all_type_" ) )
        open_all = TRUE;
    
    GtkWidget* design_menu = gtk_menu_new();
    GtkAccelGroup* accel_group = gtk_accel_group_new();

    if ( toolshow )
    {
        // Show Tool
        newitem = gtk_check_menu_item_new_with_mnemonic( _("S_how") );
        gtk_container_add ( GTK_CONTAINER ( design_menu ), newitem );
        g_object_set_data( G_OBJECT(newitem), "job", GINT_TO_POINTER( XSET_JOB_SHOW ) );
        gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( newitem ),
                                                ( set->tool == XSET_B_TRUE ) );
        g_signal_connect( newitem, "activate", G_CALLBACK( xset_design_job ), set );

        // Separator
        gtk_container_add ( GTK_CONTAINER (design_menu ), gtk_separator_menu_item_new() );
    }

#if 0  // to prevent comment showing in pot file
    // Name
    /*
    newitem = xset_design_additem( design_menu, _("_Name"),
                                GTK_STOCK_PROPERTIES, XSET_JOB_LABEL, set );
    gtk_widget_set_sensitive( newitem, ( set->menu_style <= XSET_MENU_SUBMENU
                                            && !set->plugin ) );
    */
    
    /*
    // Icon
    newitem = xset_design_additem( design_menu, _("_Icon"),
                                    GTK_STOCK_PROPERTIES, XSET_JOB_ICON, set );
    gtk_widget_set_sensitive( newitem, ( set->menu_style == XSET_MENU_NORMAL 
                                        || set->menu_style == XSET_MENU_STRING 
                                        || set->menu_style == XSET_MENU_FONTDLG
                                        || set->menu_style == XSET_MENU_COLORDLG
                                        || set->menu_style == XSET_MENU_SUBMENU
                                        || set->tool ) && !open_all );
    gtk_widget_add_accelerator( newitem, "activate", accel_group,
                            GDK_KEY_i, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

    //// Style submenu
    newitem = gtk_image_menu_item_new_with_mnemonic( _("_Style") );
    submenu = gtk_menu_new();
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( newitem ), submenu );
    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM( newitem ), 
          gtk_image_new_from_stock( GTK_STOCK_ITALIC, GTK_ICON_SIZE_MENU ) );
    gtk_container_add ( GTK_CONTAINER ( design_menu ), newitem );
    gtk_widget_set_sensitive( newitem, ( !set->plugin && !set->lock &&
                                    set->menu_style < XSET_MENU_SUBMENU ) ||
                            ( xset_context && xset_context->valid && !open_all ) );
    g_object_set_data( G_OBJECT( newitem ), "job", GINT_TO_POINTER( XSET_JOB_HELP_STYLE ) );
    g_signal_connect( submenu, "key_press_event",
                      G_CALLBACK( xset_design_menu_keypress ), set );

    // Normal
    radio_group = NULL;
    newitem = gtk_radio_menu_item_new_with_mnemonic( radio_group, _("_Normal") );
    radio_group = gtk_radio_menu_item_get_group( GTK_RADIO_MENU_ITEM( newitem ) );
    gtk_container_add ( GTK_CONTAINER ( submenu ), newitem );
    g_object_set_data( G_OBJECT(newitem), "job", GINT_TO_POINTER( XSET_JOB_NORMAL ) );
    gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( newitem ),
                                    ( set->menu_style == XSET_MENU_NORMAL ) );
    gtk_widget_set_sensitive( newitem, !set->plugin && !set->lock &&
                                    set->menu_style < XSET_MENU_SUBMENU );
    
    // Check
    newitem2 = gtk_radio_menu_item_new_with_mnemonic( radio_group, _("_Checkbox") );
    radio_group = gtk_radio_menu_item_get_group( GTK_RADIO_MENU_ITEM( newitem2 ) );
    gtk_container_add ( GTK_CONTAINER ( submenu ), newitem2 );
    g_object_set_data( G_OBJECT(newitem2), "job", GINT_TO_POINTER( XSET_JOB_CHECK ) );
    gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( newitem2 ),
                                        ( set->menu_style == XSET_MENU_CHECK ) );
    gtk_widget_set_sensitive( newitem2, !set->plugin && !set->lock &&
                                    set->menu_style < XSET_MENU_SUBMENU );

    // Confirmation
    newitem3 = gtk_radio_menu_item_new_with_mnemonic( radio_group, _("Con_firmation") );
    radio_group = gtk_radio_menu_item_get_group( GTK_RADIO_MENU_ITEM( newitem3 ) );
    gtk_container_add ( GTK_CONTAINER ( submenu ), newitem3 );
    g_object_set_data( G_OBJECT(newitem3), "job", GINT_TO_POINTER( XSET_JOB_CONFIRM ) );
    gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( newitem3 ),
                                    ( set->menu_style == XSET_MENU_CONFIRM ) );
    gtk_widget_set_sensitive( newitem3, !set->plugin && !set->lock &&
                                    set->menu_style < XSET_MENU_SUBMENU );

    // Dialog
    newitem4 = gtk_radio_menu_item_new_with_mnemonic( radio_group, _("_Input") );
    radio_group = gtk_radio_menu_item_get_group( GTK_RADIO_MENU_ITEM( newitem4 ) );
    gtk_container_add ( GTK_CONTAINER ( submenu ), newitem4 );
    g_object_set_data( G_OBJECT(newitem4), "job", GINT_TO_POINTER( XSET_JOB_DIALOG ) );
    gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( newitem4 ),
                                        ( set->menu_style == XSET_MENU_STRING ) );
    gtk_widget_set_sensitive( newitem4, !set->plugin && !set->lock &&
                                    set->menu_style < XSET_MENU_SUBMENU );

    g_signal_connect( newitem, "toggled", G_CALLBACK( on_design_radio_toggled ), set );
    g_signal_connect( newitem2, "toggled", G_CALLBACK( on_design_radio_toggled ), set );
    g_signal_connect( newitem3, "toggled", G_CALLBACK( on_design_radio_toggled ), set );
    g_signal_connect( newitem4, "toggled", G_CALLBACK( on_design_radio_toggled ), set );

    // Description
    newitem = xset_design_additem( submenu, _("_Message"),
                                GTK_STOCK_EDIT, XSET_JOB_MESSAGE, set );
    gtk_widget_set_sensitive( newitem, ( set->menu_style == XSET_MENU_STRING
                                      || set->menu_style == XSET_MENU_CONFIRM )
                                            && !set->plugin && !set->lock );

    // Separator
    gtk_container_add ( GTK_CONTAINER ( submenu ), gtk_separator_menu_item_new() );

    // Context
    newitem = xset_design_additem( submenu, _("Con_text"),
                                GTK_STOCK_EDIT, XSET_JOB_CONTEXT, set );
    gtk_widget_set_sensitive( newitem, xset_context && xset_context->valid
                                                            && !open_all );
    gtk_widget_add_accelerator( newitem, "activate", accel_group,
                            GDK_KEY_F3, 0, GTK_ACCEL_VISIBLE);

    newitem = xset_design_additem( submenu, _("Ign_ore Context (global)"),
                                "@check", XSET_JOB_IGNORE_CONTEXT, set );
    if ( xset_get_b( "context_dlg" ) )
        set_check_menu_item_block( newitem );

    //// Command submenu
    newitem = gtk_image_menu_item_new_with_mnemonic( _("C_ommand") );
    submenu = gtk_menu_new();
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( newitem ), submenu );
    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM( newitem ), 
          gtk_image_new_from_stock( GTK_STOCK_EXECUTE, GTK_ICON_SIZE_MENU ) );
    gtk_container_add ( GTK_CONTAINER ( design_menu ), newitem );
    gtk_widget_set_sensitive( newitem, !set->lock && (
                        set->menu_style < XSET_MENU_SUBMENU || toolexecsub ) ); 
    g_object_set_data( G_OBJECT( newitem ), "job", GINT_TO_POINTER( XSET_JOB_HELP_COMMAND ) );
    g_signal_connect( submenu, "key_press_event",
                      G_CALLBACK( xset_design_menu_keypress ), set );

    // Edit
    newitem = xset_design_additem( submenu, _("_Edit"),
                                GTK_STOCK_EDIT, XSET_JOB_EDIT, set );
    gtk_widget_add_accelerator( newitem, "activate", accel_group,
                            GDK_KEY_F4, 0, GTK_ACCEL_VISIBLE);
    if ( !set->lock && geteuid() != 0 && atoi( set->x ) != 0 )
    {
        gboolean edit_as_root = TRUE;
        if ( atoi( set->x ) == 2 )
        {
            // custom
            if ( !set->z || !g_file_test( set->z, G_FILE_TEST_EXISTS ) )
                edit_as_root = FALSE;
            else if ( !mime_type_is_text_file( set->z, NULL ) )
                edit_as_root = FALSE;
        }
        if ( set->plugin && set->plug_dir && strstr( set->plug_dir, "/included/" ) )
            edit_as_root = FALSE;
        if ( edit_as_root )
            newitem = xset_design_additem( submenu, _("E_dit As Root"),
                        GTK_STOCK_DIALOG_WARNING, XSET_JOB_EDIT_ROOT, set );                
    }

    // Copy (Script)
    newitem = xset_design_additem( submenu, _("_Copy"),
                                                    GTK_STOCK_COPY,
                                                    XSET_JOB_COPYNAME,
                                                    set );

    //// Browse submenu
    newitem = gtk_image_menu_item_new_with_mnemonic( _("_Browse") );
    submenu2 = gtk_menu_new();
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( newitem ), submenu2 );
    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM( newitem ), 
          gtk_image_new_from_stock( GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU ) );
    gtk_container_add ( GTK_CONTAINER ( submenu ), newitem );
    gtk_widget_set_sensitive( newitem, !set->lock ); 
    g_object_set_data( G_OBJECT( newitem ), "job", GINT_TO_POINTER( XSET_JOB_HELP_BROWSE ) );
    g_signal_connect( submenu2, "key_press_event",
                      G_CALLBACK( xset_design_menu_keypress ), set );

        if ( set->plugin )
            path = g_build_filename( set->plug_dir, set->plug_name, NULL );
        else
            path = g_build_filename( settings_config_dir, "scripts",
                                                        set->name, NULL );
        if ( dir_has_files( path ) )
            newitem = xset_design_additem( submenu2, _("_Files"),
                                    NULL, XSET_JOB_BROWSE_FILES, set );
        else
            newitem = xset_design_additem( submenu2, _("_Files (none)"),
                                    NULL, XSET_JOB_BROWSE_FILES, set );
        g_free( path );

        if ( set->plugin )
        {
            mset = xset_get_plugin_mirror( set );
            path = g_build_filename( settings_config_dir, "plugin-data",
                                                        mset->name, NULL );
        }
        else
            path = g_build_filename( settings_config_dir, "plugin-data",
                                                        set->name, NULL );
        if ( dir_has_files( path ) )
            newitem = xset_design_additem( submenu2, _("_Data"),
                                    NULL, XSET_JOB_BROWSE_DATA, set );
        else
            newitem = xset_design_additem( submenu2, _("_Data (none)"),
                                    NULL, XSET_JOB_BROWSE_DATA, set );
        g_free( path );

        newitem = xset_design_additem( submenu2, _("_Plugin"),
                                    NULL, XSET_JOB_BROWSE_PLUGIN, set );
        gtk_widget_set_sensitive( newitem, set->plugin ); 

    // Separator
    gtk_container_add ( GTK_CONTAINER ( submenu ), gtk_separator_menu_item_new() );

    radio_group = NULL;
    if ( !set->lock && !set->x )
        set->x = g_strdup_printf("0");
    if ( set->x )
    {
        // Line
        newitem = gtk_radio_menu_item_new_with_mnemonic( radio_group, _("_Line") );
        radio_group = gtk_radio_menu_item_get_group( GTK_RADIO_MENU_ITEM( newitem ) );
        gtk_container_add ( GTK_CONTAINER ( submenu ), newitem );
        g_object_set_data( G_OBJECT(newitem), "job", GINT_TO_POINTER( XSET_JOB_LINE ) );
        gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( newitem ),
                                                        ( atoi( set->x ) == 0 ) );
        gtk_widget_set_sensitive( newitem, !set->plugin );

        // Script
        newitem2 = gtk_radio_menu_item_new_with_mnemonic( radio_group, _("_Script") );
        radio_group = gtk_radio_menu_item_get_group( GTK_RADIO_MENU_ITEM( newitem2 ) );
        gtk_container_add ( GTK_CONTAINER ( submenu ), newitem2 );
        g_object_set_data( G_OBJECT(newitem2), "job", GINT_TO_POINTER( XSET_JOB_SCRIPT ) );
        gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( newitem2 ),
                                                        ( atoi( set->x ) == 1 ) );
        gtk_widget_set_sensitive( newitem2, !set->plugin );

        // Custom
        if ( ( atoi( set->x ) == 2 ) && ( set->z && set->z[0] != '\0' ) )
        {
            char* s = g_path_get_basename( set->z );
            if ( strlen( s ) < 20 )
                label = g_strdup_printf( _("Custo_m (%s)"), s );
            else
                label = g_strdup( _("Custo_m (...)") );
            g_free( s );
        }
        else
            label = g_strdup_printf( _("Custo_m") );
        newitem3 = gtk_radio_menu_item_new_with_mnemonic( radio_group, label );
        radio_group = gtk_radio_menu_item_get_group( GTK_RADIO_MENU_ITEM( newitem3 ) );
        g_free( label );
        gtk_container_add ( GTK_CONTAINER ( submenu ), newitem3 );
        g_object_set_data( G_OBJECT(newitem3), "job", GINT_TO_POINTER( XSET_JOB_CUSTOM ) );
        gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( newitem3 ),
                                                        ( atoi( set->x ) == 2 ) );
        gtk_widget_set_sensitive( newitem3, !set->plugin );

        g_signal_connect( newitem, "toggled", G_CALLBACK( on_design_radio_toggled ), set );
        g_signal_connect( newitem2, "toggled", G_CALLBACK( on_design_radio_toggled ), set );
        g_signal_connect( newitem3, "toggled", G_CALLBACK( on_design_radio_toggled ), set );
    }
    
    // Separator
    gtk_container_add ( GTK_CONTAINER ( submenu ), gtk_separator_menu_item_new() );

    // Run In Terminal
    newitem = xset_design_additem( submenu, _("Run _In Terminal"),
                                                    "@check", XSET_JOB_TERM, set );
    if ( mset->in_terminal == XSET_B_TRUE )
        set_check_menu_item_block( newitem );

    // Keep Terminal
    newitem = xset_design_additem( submenu, _("_Keep Terminal"),
                                                    "@check", XSET_JOB_KEEP, set );
    if ( mset->keep_terminal == XSET_B_TRUE )
        set_check_menu_item_block( newitem );
    gtk_widget_set_sensitive( newitem, ( mset->in_terminal == XSET_B_TRUE ) );

    // Separator
    gtk_container_add ( GTK_CONTAINER ( submenu ), gtk_separator_menu_item_new() );

    // Run As User
    if ( set->y && set->y[0] != '\0' && strlen( set->y ) < 20 )
    {
        label = g_strdup_printf( _("Run As _User... (%s)"), set->y );
        newitem = xset_design_additem( submenu, label,
                                GTK_STOCK_DIALOG_WARNING, XSET_JOB_USER, set );
        g_free( label );
    }
    else
        newitem = xset_design_additem( submenu, _("Run As _User..."),
                                                    NULL, XSET_JOB_USER, set );
    gtk_widget_set_sensitive( newitem, !set->plugin );

    // Separator
    gtk_container_add ( GTK_CONTAINER ( submenu ), gtk_separator_menu_item_new() );

    // Run As Task
    newitem = xset_design_additem( submenu, _("Run As _Task"),
                                                    "@check", XSET_JOB_TASK, set );
    if ( mset->task == XSET_B_TRUE )
        set_check_menu_item_block( newitem );

    // Popup Task
    newitem = xset_design_additem( submenu, _("_Popup Task"),
                                                    "@check", XSET_JOB_POP, set );
    if ( mset->task_pop == XSET_B_TRUE )
        set_check_menu_item_block( newitem );
    gtk_widget_set_sensitive( newitem, ( mset->task == XSET_B_TRUE ) );

    // Popup Error
    newitem = xset_design_additem( submenu, _("Popup E_rror"),
                                                    "@check", XSET_JOB_ERR, set );
    if ( mset->task_err == XSET_B_TRUE )
        set_check_menu_item_block( newitem );
    gtk_widget_set_sensitive( newitem, ( mset->task == XSET_B_TRUE ) );

    // Popup Output
    newitem = xset_design_additem( submenu, _("Popup _Output"),
                                                    "@check", XSET_JOB_OUT, set );
    if ( mset->task_out == XSET_B_TRUE )
        set_check_menu_item_block( newitem );
    gtk_widget_set_sensitive( newitem, ( mset->task == XSET_B_TRUE ) );

    // Scroll
    newitem = xset_design_additem( submenu, _("_Scroll"),
                                                    "@check", XSET_JOB_SCROLL, set );
    if ( mset->scroll_lock != XSET_B_TRUE )
        set_check_menu_item_block( newitem );
    gtk_widget_set_sensitive( newitem, ( mset->task == XSET_B_TRUE ) );
    */

    // Separator
    //gtk_container_add ( GTK_CONTAINER ( design_menu ), gtk_separator_menu_item_new() );

#endif

    // Cut
    newitem = xset_design_additem( design_menu, _("Cu_t"),
                                GTK_STOCK_CUT, XSET_JOB_CUT, set );
    gtk_widget_set_sensitive( newitem, !set->lock && !set->plugin );
    gtk_widget_add_accelerator( newitem, "activate", accel_group,
                            GDK_KEY_x, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

    // Copy
    newitem = xset_design_additem( design_menu, _("_Copy"),
                                GTK_STOCK_COPY, XSET_JOB_COPY, set );
    gtk_widget_set_sensitive( newitem, !set->lock );
    gtk_widget_add_accelerator( newitem, "activate", accel_group,
                            GDK_KEY_c, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

    // Paste
    newitem = xset_design_additem( design_menu, _("_Paste"),
                                GTK_STOCK_PASTE, XSET_JOB_PASTE, set );
    gtk_widget_set_sensitive( newitem, set_clipboard && !no_paste
                                                        && !set->plugin 
                        && !( set->tool && set_clipboard->menu_style ==
                                                    XSET_MENU_SUBMENU ) );
    gtk_widget_add_accelerator( newitem, "activate", accel_group,
                            GDK_KEY_v, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

    // Remove
    newitem = xset_design_additem( design_menu, _("_Remove"),
                                GTK_STOCK_REMOVE, XSET_JOB_REMOVE, set );
    gtk_widget_set_sensitive( newitem, !set->lock && !no_remove );
    gtk_widget_add_accelerator( newitem, "activate", accel_group,
                            GDK_KEY_Delete, 0, GTK_ACCEL_VISIBLE);

    // Export
    newitem = xset_design_additem( design_menu, _("E_xport"),
                                GTK_STOCK_SAVE, XSET_JOB_EXPORT, set );
    gtk_widget_set_sensitive( newitem, !set->lock
                                    && set->menu_style < XSET_MENU_SEP );

    //// New submenu
    newitem = gtk_image_menu_item_new_with_mnemonic( _("_New") );
    submenu = gtk_menu_new();
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( newitem ), submenu );
    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM( newitem ), 
          gtk_image_new_from_stock( GTK_STOCK_ADD, GTK_ICON_SIZE_MENU ) );
    gtk_container_add ( GTK_CONTAINER ( design_menu ), newitem );
    gtk_widget_set_sensitive( newitem, !set->plugin );
    g_object_set_data( G_OBJECT( newitem ), "job",
                                    GINT_TO_POINTER( XSET_JOB_HELP_NEW ) );
    g_signal_connect( submenu, "key_press_event",
                      G_CALLBACK( xset_design_menu_keypress ), set );

    // New > Bookmark
    newitem = xset_design_additem( submenu, _("_Bookmark"),
                                NULL, XSET_JOB_BOOKMARK, set );

    // New > Application
    newitem = xset_design_additem( submenu, _("_Application"),
                                NULL, XSET_JOB_APP, set );

    // New > Command
    newitem = xset_design_additem( submenu, _("_Command"),
                                NULL, XSET_JOB_COMMAND, set );
    gtk_widget_add_accelerator( newitem, "activate", accel_group,
                            GDK_KEY_Insert, 0, GTK_ACCEL_VISIBLE);

    // New > Submenu
    newitem = xset_design_additem( submenu, _("Sub_menu"),
                                NULL, XSET_JOB_SUBMENU, set );

    // New > Separator
    newitem = xset_design_additem( submenu, _("S_eparator"),
                                NULL, XSET_JOB_SEP, set );

    // New > Import >
    newitem = gtk_image_menu_item_new_with_mnemonic( _("_Import") );
    submenu2 = gtk_menu_new();
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( newitem ), submenu2 );
    //gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM( newitem ), 
    //       gtk_image_new_from_stock( GTK_STOCK_ADD, GTK_ICON_SIZE_MENU ) );
    gtk_container_add ( GTK_CONTAINER ( submenu ), newitem );
    gtk_widget_set_sensitive( newitem, !set->plugin );
    g_object_set_data( G_OBJECT( newitem ), "job",
                                    GINT_TO_POINTER( XSET_JOB_IMPORT_FILE ) );
    g_signal_connect( submenu2, "key_press_event",
                      G_CALLBACK( xset_design_menu_keypress ), set );

        newitem = xset_design_additem( submenu2, _("_File"),
                                    NULL, XSET_JOB_IMPORT_FILE, set );

        newitem = xset_design_additem( submenu2, _("_URL"),
                                    NULL, XSET_JOB_IMPORT_URL, set );

    // Separator
    gtk_container_add ( GTK_CONTAINER ( design_menu ),
                                            gtk_separator_menu_item_new() );

    // Help
    newitem = xset_design_additem( design_menu, _("_Help"),
                                GTK_STOCK_HELP, XSET_JOB_HELP, set );
    gtk_widget_set_sensitive( newitem, !set->lock || ( set->lock && set->line ) );
    gtk_widget_add_accelerator( newitem, "activate", accel_group,
                            GDK_KEY_F1, 0, GTK_ACCEL_VISIBLE);

    // Key
    newitem = xset_design_additem( design_menu, _("_Key Shortcut"),
                                    GTK_STOCK_PROPERTIES, XSET_JOB_KEY, set );
    gtk_widget_set_sensitive( newitem, ( set->menu_style < XSET_MENU_SUBMENU
                                        || toolexecsub ) );
    gtk_widget_add_accelerator( newitem, "activate", accel_group,
                            GDK_KEY_k, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

    // Properties
    newitem = xset_design_additem( design_menu, _("_Properties"),
                                GTK_STOCK_PROPERTIES, XSET_JOB_PROP, set );
    gtk_widget_add_accelerator( newitem, "activate", accel_group,
                            GDK_KEY_F3, 0, GTK_ACCEL_VISIBLE);


    // show menu
    gtk_widget_show_all( GTK_WIDGET( design_menu ) );
    gtk_menu_popup( GTK_MENU( design_menu ), GTK_WIDGET( menu ), NULL, NULL,
                                                    NULL, button, time );
    gtk_widget_set_sensitive( GTK_WIDGET( menu ), FALSE );
    
    g_signal_connect( menu, "hide", G_CALLBACK( on_menu_hide ),
                                                            design_menu );
    //g_signal_connect( menu, "deactivate",  //doesn't work for menubar
    //                  G_CALLBACK( xset_design_destroy), design_menu );
    g_signal_connect( design_menu, "selection-done",
                      G_CALLBACK( gtk_widget_destroy ), NULL );
    g_signal_connect( design_menu, "key_press_event",
                      G_CALLBACK( xset_design_menu_keypress ), set );
}

gboolean xset_design_cb( GtkWidget* item, GdkEventButton* event, XSet* set )
{
    int job = -1;
        
//printf("xset_design_cb\n");
        
    GtkWidget* menu = (GtkWidget*)g_object_get_data( G_OBJECT(item), "menu" );
    int keymod = ( event->state & ( GDK_SHIFT_MASK | GDK_CONTROL_MASK |
                 GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK ) );

    if ( event->type == GDK_BUTTON_RELEASE )
    {
        if ( event->button == 1 && keymod == 0 )
        {
            // user released left button - due to an apparent gtk bug, activate
            // doesn't always fire on this event so handle it ourselves
            // see also ptk-file-menu.c on_app_button_press()
            // test: gtk2 Crux theme with touchpad on Edit|Copy To|Location
            // https://github.com/IgnorantGuru/spacefm/issues/31
            // https://github.com/IgnorantGuru/spacefm/issues/228
            if ( set && set->tool )
            {
                // is in a toolbar config menu - show the design menu
                xset_design_show_menu( menu, set, event->button, event->time );
            }
            else
            {
                if ( menu )
                    gtk_menu_shell_deactivate( GTK_MENU_SHELL( menu ) );
                gtk_menu_item_activate( GTK_MENU_ITEM( item ) );
            }
            return TRUE;
        }
        return FALSE;
    }
    else if ( event->type != GDK_BUTTON_PRESS )
        return FALSE;

    if ( event->button == 1 || event->button == 3 )
    {
        // left or right click
        if ( keymod == 0 )
        {
            // no modifier
            if ( event->button == 3 )
            {
                // right
                xset_design_show_menu( menu, set, event->button, event->time );
                return TRUE;
            }
        }
        else if ( keymod == GDK_CONTROL_MASK )
        {
            // ctrl
            job = XSET_JOB_COPY;
        }
        else if ( keymod == GDK_MOD1_MASK )
        {
            // alt
            job = XSET_JOB_CUT;
        }
        else if ( keymod == GDK_SHIFT_MASK )
        {
            // shift
            job = XSET_JOB_PASTE;
        }
        else if ( keymod == ( GDK_CONTROL_MASK | GDK_SHIFT_MASK ) )
        {
            // ctrl + shift
            job = XSET_JOB_COMMAND;
        }
    }
    else if ( event->button == 2 )
    {
        // middle click
        if ( keymod == 0 )
        {
            // no modifier
            if ( set->lock )
            {
                xset_design_show_menu( menu, set, event->button, event->time );
                return TRUE;
            }
            else
                job = XSET_JOB_PROP_CMD;
        }
        else if ( keymod == GDK_CONTROL_MASK )
        {
            // ctrl
            job = XSET_JOB_KEY;
        }
        else if ( keymod == GDK_MOD1_MASK )
        {
            // alt
            job = XSET_JOB_HELP;
        }
        else if ( keymod == GDK_SHIFT_MASK )
        {
            // shift
            job = XSET_JOB_ICON;
        }
        else if ( keymod == ( GDK_CONTROL_MASK | GDK_SHIFT_MASK ) )
        {
            // ctrl + shift
            job = XSET_JOB_REMOVE;
        }        
        else if ( keymod == ( GDK_CONTROL_MASK | GDK_MOD1_MASK ) )
        {
            // ctrl + alt
            job = XSET_JOB_PROP;
        }        
    }

    if ( job != -1 )
    {
        if ( xset_job_is_valid( set, job ) )
        {
            if ( menu )
                gtk_menu_shell_deactivate( GTK_MENU_SHELL( menu ) );
            g_object_set_data( G_OBJECT( item ), "job", GINT_TO_POINTER( job ) );
            xset_design_job( item, set );
        }
        else
            xset_design_show_menu( menu, set, event->button, event->time );
        return TRUE;
    }
    return FALSE;  // TRUE won't stop activate on button-press (will on release)
}

gboolean xset_menu_keypress( GtkWidget* widget, GdkEventKey* event,
                                                            gpointer user_data )
{
    int job = -1;
    XSet* set;

    GtkWidget* item = gtk_menu_shell_get_selected_item( GTK_MENU_SHELL( widget ) );
    if ( item )
    {
        set = g_object_get_data( G_OBJECT( item ), "set" );
        if ( !set )
            return FALSE;
    }
    else
        return FALSE;
    
    int keymod = ( event->state & ( GDK_SHIFT_MASK | GDK_CONTROL_MASK |
                 GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK ) );
    
    if ( keymod == 0 )
    {        
        if ( event->keyval == GDK_KEY_F1 )
        {
            job = XSET_JOB_HELP;
        }
        else if ( event->keyval == GDK_KEY_F2 )
        {
            xset_design_show_menu( widget, set, 0, event->time );
            return TRUE;
        }
        else if ( event->keyval == GDK_KEY_F3 )
            job = XSET_JOB_PROP;
        else if ( event->keyval == GDK_KEY_F4 )
            job = XSET_JOB_PROP_CMD;
        else if ( event->keyval == GDK_KEY_Delete )
            job = XSET_JOB_REMOVE;
        else if ( event->keyval == GDK_KEY_Insert )
            job = XSET_JOB_COMMAND;
    }
    else if ( keymod == GDK_CONTROL_MASK )
    {
        if ( event->keyval == GDK_KEY_c )
            job = XSET_JOB_COPY;
        else if ( event->keyval == GDK_KEY_x )
            job = XSET_JOB_CUT;
        else if ( event->keyval == GDK_KEY_v )
            job = XSET_JOB_PASTE;
        else if ( event->keyval == GDK_KEY_e )
        {
            if ( set->lock )
            {
                xset_design_show_menu( widget, set, 0, event->time );
                return TRUE;
            }
            else
                job = XSET_JOB_PROP_CMD;
        }
        else if ( event->keyval == GDK_KEY_k )
            job = XSET_JOB_KEY;
        else if ( event->keyval == GDK_KEY_i )
            job = XSET_JOB_ICON;
    }
    if ( job != -1 )
    {
        if ( xset_job_is_valid( set, job ) )
        {
            gtk_menu_shell_deactivate( GTK_MENU_SHELL( widget ) );
            g_object_set_data( G_OBJECT( item ), "job", GINT_TO_POINTER( job ) );
            xset_design_job( item, set );
        }
        else
            xset_design_show_menu( widget, set, 0, event->time );
        return TRUE;
    }
    return FALSE;
}

void xset_menu_cb( GtkWidget* item, XSet* set )
{
    GtkWidget* parent;
    void (*cb_func) () = NULL;
    gpointer cb_data = NULL;
    char* title;
    XSet* mset;  // mirror set or set
    XSet* rset;  // real set

    if ( item )
    {
        if ( set->lock && set->menu_style == XSET_MENU_RADIO &&
                    GTK_IS_CHECK_MENU_ITEM( item ) &&
                    !gtk_check_menu_item_get_active(
                                            GTK_CHECK_MENU_ITEM( item ) ) )
            return;
        cb_func = (void *)g_object_get_data( G_OBJECT(item), "cb_func" );
        cb_data = g_object_get_data( G_OBJECT(item), "cb_data" );
    }
    
    if ( set->tool )
    {
        // get current browser for toolbar button
        FMMainWindow* main_window = fm_main_window_get_last_active();
        if ( main_window )
            set->browser = PTK_FILE_BROWSER( 
                    fm_main_window_get_current_file_browser( main_window ) );
        else
            set->browser = NULL;
        set->desktop = NULL;
    }
    
    parent = set->browser ? GTK_WIDGET( set->browser ) : GTK_WIDGET( set->desktop );

    if ( set->plugin )
    {
        // set is plugin
        mset = xset_get_plugin_mirror( set );
        rset = set;
    }
    else if ( !set->lock && set->desc && !strcmp( set->desc, "@plugin@mirror@" )
                                                            && set->shared_key )
    {
        // set is plugin mirror
        mset = set;
        rset = xset_get( set->shared_key );
        rset->browser = set->browser;
        rset->desktop = set->desktop;
    }
    else
    {
        mset = set;
        rset = set;
    }
    
    if ( !rset->menu_style )
    {
        if ( cb_func )
            (*cb_func) ( item, cb_data );    
        else if ( !rset->lock )
            xset_custom_activate( item, rset );
    }
    else if ( rset->menu_style == XSET_MENU_CHECK )
    {
        if ( mset->b == XSET_B_TRUE )
            mset->b = XSET_B_FALSE;
        else
            mset->b = XSET_B_TRUE;
        if ( cb_func )
            (*cb_func) ( item, cb_data );
        else if ( !rset->lock )
            xset_custom_activate( item, rset );
    }
    else if ( rset->menu_style == XSET_MENU_STRING ||
              rset->menu_style == XSET_MENU_CONFIRM )
    {
        char* msg;
        char* help;
        char* default_str = NULL;
        if ( rset->title && rset->lock )
            title = g_strdup( rset->title );
        else
            title = clean_label( rset->menu_label, FALSE, FALSE );
        if ( rset->lock )
        {
            msg = rset->desc;
            default_str = rset->z;
            help = set->line;
        }
        else
        {
            char* newline = g_strdup_printf( "\n" );
            char* tab = g_strdup_printf( "\t" );
            char* msg1 = replace_string( rset->desc, "\\n", newline, FALSE );
            msg = replace_string( msg1, "\\t", tab, FALSE );
            g_free( msg1 );
            g_free( newline );
            g_free( tab );
            help = set->name;
        }
        if ( rset->menu_style == XSET_MENU_CONFIRM )
        {
            if ( xset_msg_dialog( parent, GTK_MESSAGE_QUESTION, title, NULL,
                                            GTK_BUTTONS_OK_CANCEL,
                                            msg, NULL, help ) == GTK_RESPONSE_OK )
            {
                if ( cb_func )
                    (*cb_func) ( item, cb_data );
                else if ( !set->lock )
                    xset_custom_activate( item, rset );
            }
        }
        else if ( xset_text_dialog( parent, title, NULL, TRUE, msg, NULL, mset->s,
                                                        &mset->s, default_str,
                                                        FALSE, help ) )
        {
            if ( cb_func )
                (*cb_func) ( item, cb_data );
            else if ( !set->lock )
                xset_custom_activate( item, rset );
        }
        if ( !rset->lock )
            g_free( msg );
        g_free( title );
    }
    else if ( rset->menu_style == XSET_MENU_RADIO )
    {
        if ( mset->b != XSET_B_TRUE )
            mset->b = XSET_B_TRUE;
        if ( cb_func )
            (*cb_func) ( item, cb_data );
        else if ( !rset->lock )
            xset_custom_activate( item, rset );
    }
    else if ( rset->menu_style == XSET_MENU_FONTDLG )
    {
        char* fontname = xset_font_dialog( parent, rset->title, rset->desc, rset->s );
        if ( fontname )
        {
            if ( fontname[0] != '\0' )
            {
                xset_set_set( rset, "s", fontname );
            }
            else
            {
                if ( rset->s )
                    g_free( rset->s );
                rset->s = NULL;
            }            
            g_free( fontname );
            if ( cb_func )
                (*cb_func) ( item, cb_data );
        }
    }
    else if ( rset->menu_style == XSET_MENU_FILEDLG )
    {
        // test purpose only
        char* file = xset_file_dialog( parent, GTK_FILE_CHOOSER_ACTION_SAVE,
                        rset->title, rset->s, "foobar.xyz" );
        //printf("file=%s\n", file );
    }
    else if ( rset->menu_style == XSET_MENU_ICON )
    {
        if ( xset_text_dialog( parent, rset->title ? rset->title : _("Set Icon"),
                                NULL, FALSE,
                                rset->desc? rset->desc : _(icon_desc), NULL,
                                rset->icon, &rset->icon,
                                NULL, FALSE, NULL ) )
        {
            if ( rset->lock )
                rset->keep_terminal = TRUE;  // trigger save of changed icon
            if ( cb_func )
                (*cb_func) ( item, cb_data );
        }
    }
    else if ( rset->menu_style == XSET_MENU_COLORDLG )
    {
        char* scolor = xset_color_dialog( parent, rset->title, rset->s );
        if ( rset->s )
            g_free( rset->s );
        rset->s = scolor;
        if ( cb_func )
            (*cb_func) ( item, cb_data );
    }
    else if ( cb_func )
        (*cb_func) ( item, cb_data );    
    else if ( !set->lock )
        xset_custom_activate( item, rset );

    if ( rset->menu_style )
        xset_autosave( rset->browser, FALSE, FALSE );
}

int xset_msg_dialog( GtkWidget* parent, int action, const char* title, GtkWidget* image,
                                int buttons, const char* msg1, const char* msg2,
                                const char* help )
{   
    /* action=
    GTK_MESSAGE_INFO,
    GTK_MESSAGE_WARNING,
    GTK_MESSAGE_QUESTION,
    GTK_MESSAGE_ERROR
    */
    GtkWidget* dlgparent = NULL;
    
    if ( parent )
        dlgparent = gtk_widget_get_toplevel( parent );

    if ( !buttons )
        buttons = GTK_BUTTONS_OK;
    if ( action == 0 )
        action = GTK_MESSAGE_INFO;
        
    GtkWidget* dlg = gtk_message_dialog_new( GTK_WINDOW( dlgparent ),
                                              GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                              action,
                                              buttons,
                                              msg1, NULL );
    if ( action == GTK_MESSAGE_INFO )
        xset_set_window_icon( GTK_WINDOW( dlg ) );
    gtk_window_set_role( GTK_WINDOW( dlg ), "msg_dialog" );

    if ( msg2 )
        gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG( dlg ), msg2, NULL );
    if ( image )
        gtk_message_dialog_set_image( GTK_MESSAGE_DIALOG( dlg ), image );
    if ( title )
        gtk_window_set_title( GTK_WINDOW( dlg ), title );

    if ( help )
    {
        GtkWidget* btn_help = gtk_button_new_with_mnemonic( _("_Help") );
        gtk_dialog_add_action_widget( GTK_DIALOG( dlg ), btn_help, GTK_RESPONSE_HELP );
        gtk_button_set_image( GTK_BUTTON( btn_help ), xset_get_image( "GTK_STOCK_HELP",
                                                            GTK_ICON_SIZE_BUTTON ) );
        gtk_button_set_focus_on_click( GTK_BUTTON( btn_help ), FALSE );
        gtk_widget_set_can_focus( btn_help, FALSE );
    }

    gtk_widget_show_all( dlg );
    int response;
    while ( response = gtk_dialog_run( GTK_DIALOG( dlg ) ) )
    {
        if ( response == GTK_RESPONSE_HELP )
        {
            // btn_help clicked
            if ( help )
            {
                if ( help[0] == '#' )
                {
                    // as anchor
                    xset_show_help( dlg, NULL, help );                    
                }
                else if ( xset_is( help ) )
                {
                    // as set name
                    xset_show_help( dlg, xset_get( help ), NULL );
                }
            }
        }
        else
            break;
    }
    gtk_widget_destroy( dlg );
    return response;
}

void on_multi_input_insert( GtkTextBuffer *buf )
{   // remove linefeeds from pasted text
    GtkTextIter iter, siter;
    //gboolean changed = FALSE;
    int x;
    
    // buffer contains linefeeds?
    gtk_text_buffer_get_start_iter( buf, &siter );
    gtk_text_buffer_get_end_iter( buf, &iter );
    char* all = gtk_text_buffer_get_text( buf, &siter, &iter, FALSE );
    if ( !strchr( all, '\n' ) )
    {
        g_free( all );
        return;
    }
    g_free( all );

    // delete selected text that was pasted over
    if ( gtk_text_buffer_get_selection_bounds( buf, &siter, &iter ) )
        gtk_text_buffer_delete( buf, &siter, &iter );
    
    GtkTextMark* insert = gtk_text_buffer_get_insert( buf );
    gtk_text_buffer_get_iter_at_mark( buf, &iter, insert );
    gtk_text_buffer_get_start_iter( buf, &siter );
    char* b = gtk_text_buffer_get_text( buf, &siter, &iter, FALSE );
    gtk_text_buffer_get_end_iter( buf, &siter );
    char* a = gtk_text_buffer_get_text( buf, &iter, &siter, FALSE );
    
    if ( strchr( b, '\n' ) )
    {
        x = 0;
        while ( b[x] != '\0' )
        {
            if ( b[x] == '\n' )
                b[x] = ' ';
            x++;
        }
    }
    if ( strchr( a, '\n' ) )
    {
        x = 0;
        while ( a[x] != '\0' )
        {
            if ( a[x] == '\n' )
                a[x] = ' ';
            x++;
        }
    }

    g_signal_handlers_block_matched( buf, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                 on_multi_input_insert, NULL );
                                                                  
    gtk_text_buffer_set_text( buf, b, -1 );
    gtk_text_buffer_get_end_iter( buf, &iter );
    GtkTextMark* mark = gtk_text_buffer_create_mark( buf, NULL, &iter, TRUE );
    gtk_text_buffer_get_end_iter( buf, &iter );
    gtk_text_buffer_insert( buf, &iter, a, -1 );
    gtk_text_buffer_get_iter_at_mark( buf, &iter, mark );
    gtk_text_buffer_place_cursor( buf, &iter );
    
    g_signal_handlers_unblock_matched( buf, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                 on_multi_input_insert, NULL );
    g_free( a );
    g_free( b );
}

void on_multi_input_font_change( GtkMenuItem* item, GtkTextView *input )
{
    char* fontname = xset_get_s( "input_font" );
    if ( fontname )
    {
        PangoFontDescription* font_desc = pango_font_description_from_string( fontname );
        gtk_widget_modify_font( GTK_WIDGET( input ), font_desc );
        pango_font_description_free( font_desc );
    }
    else
        gtk_widget_modify_font( GTK_WIDGET( input ), NULL );
}

void on_multi_input_popup( GtkTextView *input, GtkMenu *menu, gpointer user_data )
{
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    XSet* set = xset_get( "sep_multi" );
    set->menu_style = XSET_MENU_SEP;
    set->browser = NULL;
    set->desktop = NULL;
    xset_add_menuitem( NULL, NULL, GTK_WIDGET( menu ), accel_group, set );
    set = xset_set_cb( "input_font", on_multi_input_font_change, input );
    set->browser = NULL;
    set->desktop = NULL;
    xset_add_menuitem( NULL, NULL, GTK_WIDGET( menu ), accel_group, set );
    gtk_widget_show_all( GTK_WIDGET( menu ) );
    g_signal_connect( G_OBJECT( menu ), "key-press-event",
                      G_CALLBACK( xset_menu_keypress ), NULL );
}

char* multi_input_get_text( GtkWidget* input )
{   // returns a new allocated string or NULL if input is empty
    GtkTextIter iter, siter;

    if ( !GTK_IS_TEXT_VIEW( input ) )
        return NULL;
        
    GtkTextBuffer* buf = gtk_text_view_get_buffer( GTK_TEXT_VIEW( input ) );
    gtk_text_buffer_get_start_iter( buf, &siter );
    gtk_text_buffer_get_end_iter( buf, &iter );
    char* ret = gtk_text_buffer_get_text( buf, &siter, &iter, FALSE );
    if ( ret && ret[0] == '\0' )
    {
        g_free( ret );
        ret = NULL;
    }
    return ret;
}

void multi_input_select_region( GtkWidget* input, int start, int end )
{
    GtkTextIter iter, siter;

    if ( start < 0 || !GTK_IS_TEXT_VIEW( input ) )
        return;
        
    GtkTextBuffer* buf = gtk_text_view_get_buffer( GTK_TEXT_VIEW( input ) );

    gtk_text_buffer_get_iter_at_offset( buf, &siter, start );
    
    if ( end < 0 )
        gtk_text_buffer_get_end_iter( buf, &iter );
    else
        gtk_text_buffer_get_iter_at_offset( buf, &iter, end );
    
    gtk_text_buffer_select_range( buf, &iter, &siter );
}

GtkTextView* multi_input_new( GtkScrolledWindow* scrolled, const char* text,
                                                            gboolean def_font )
{
    GtkTextIter iter;

    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( scrolled ),
                                     GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    GtkTextView* input = GTK_TEXT_VIEW( gtk_text_view_new() );
    // ubuntu shows input too small so use mininum height
    gtk_widget_set_size_request( GTK_WIDGET( input ), -1, 50 );
    gtk_widget_set_size_request( GTK_WIDGET( scrolled ), -1, 50 );

    gtk_container_add ( GTK_CONTAINER ( scrolled ), GTK_WIDGET( input ) );
    GtkTextBuffer* buf = gtk_text_view_get_buffer( input );
    gtk_text_view_set_wrap_mode( input, GTK_WRAP_CHAR );  //GTK_WRAP_WORD_CHAR

    if ( text )
        gtk_text_buffer_set_text( buf, text, -1 );
    gtk_text_buffer_get_end_iter( buf, &iter );
    gtk_text_buffer_place_cursor( buf, &iter );
    GtkTextMark* insert = gtk_text_buffer_get_insert( buf );
    gtk_text_view_scroll_to_mark( input, insert, 0.0, FALSE, 0, 0 );
    gtk_text_view_set_accepts_tab( input, FALSE );

    g_signal_connect_after( G_OBJECT( buf ), "insert-text",
                          G_CALLBACK( on_multi_input_insert ), NULL );
    if ( def_font )
    {
        on_multi_input_font_change( NULL, input );
        g_signal_connect_after( G_OBJECT( input ), "populate-popup",
                            G_CALLBACK( on_multi_input_popup ), NULL );
    }
    return input;
}

static gboolean on_input_keypress ( GtkWidget *widget, GdkEventKey *event, GtkWidget* dlg )
{
    if ( event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter )
    {
        gtk_dialog_response( GTK_DIALOG( dlg ), GTK_RESPONSE_OK );
        return TRUE;
    }
    return FALSE;
}

gboolean xset_text_dialog( GtkWidget* parent, const char* title, GtkWidget* image,
                            gboolean large, const char* msg1, const char* msg2,
                            const char* defstring, char** answer, const char* defreset,
                            gboolean edit_care, const char* help )
{   
    GtkTextIter iter, siter;
    int width, height;
    GtkWidget* dlgparent = NULL;

    if ( parent )
         dlgparent = gtk_widget_get_toplevel( parent );
    GtkWidget* dlg = gtk_message_dialog_new( GTK_WINDOW( dlgparent ),
                                  GTK_DIALOG_MODAL,
                                  GTK_MESSAGE_QUESTION,
                                  GTK_BUTTONS_NONE,
                                  msg1, NULL );
    xset_set_window_icon( GTK_WINDOW( dlg ) );
    gtk_window_set_role( GTK_WINDOW( dlg ), "text_dialog" );

    if ( large )
    {
        width = xset_get_int( "text_dlg", "s" );
        height = xset_get_int( "text_dlg", "z" );
        if ( width && height )
            gtk_window_set_default_size( GTK_WINDOW( dlg ), width, height );
        else
            gtk_window_set_default_size( GTK_WINDOW( dlg ), 600, 400 );
            //gtk_widget_set_size_request( GTK_WIDGET( dlg ), 600, 400 );
    }
    else
    {
        width = xset_get_int( "text_dlg", "x" );
        height = xset_get_int( "text_dlg", "y" );
        if ( width && height )
            gtk_window_set_default_size( GTK_WINDOW( dlg ), width, -1 );
        else
            gtk_window_set_default_size( GTK_WINDOW( dlg ), 500, -1 );
            //gtk_widget_set_size_request( GTK_WIDGET( dlg ), 500, 300 );
    }
    
    gtk_window_set_resizable( GTK_WINDOW( dlg ), TRUE );

    if ( msg2 )
        gtk_message_dialog_format_secondary_text( GTK_MESSAGE_DIALOG( dlg ), msg2, NULL );
    if ( image )
        gtk_message_dialog_set_image( GTK_MESSAGE_DIALOG( dlg ), image );

    // input view
    GtkScrolledWindow* scroll_input = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new(
                                                                    NULL, NULL ) );
    GtkTextView* input = multi_input_new( scroll_input, defstring, TRUE );
    GtkTextBuffer* buf = gtk_text_view_get_buffer( input );

    gtk_box_pack_start( GTK_BOX( gtk_dialog_get_content_area ( GTK_DIALOG( dlg ) ) ), GTK_WIDGET( scroll_input ),
                                                                TRUE, TRUE, 4 );

    g_signal_connect( G_OBJECT( input ), "key-press-event",
                          G_CALLBACK( on_input_keypress ), dlg );


    // buttons
    GtkWidget* btn_edit;
    GtkWidget* btn_help = NULL;
    GtkWidget* btn_default = NULL;
    if ( help )
    {
        btn_help = gtk_button_new_with_mnemonic( _("_Help") );
        gtk_dialog_add_action_widget( GTK_DIALOG( dlg ), btn_help, GTK_RESPONSE_HELP );
        gtk_button_set_image( GTK_BUTTON( btn_help ), xset_get_image( "GTK_STOCK_HELP",
                                                            GTK_ICON_SIZE_BUTTON ) );
        gtk_button_set_focus_on_click( GTK_BUTTON( btn_help ), FALSE );
    }

    if ( edit_care )
    {
        btn_edit = gtk_toggle_button_new_with_mnemonic( _("_Edit") );
        gtk_dialog_add_action_widget( GTK_DIALOG( dlg ), btn_edit, GTK_RESPONSE_YES );
        gtk_button_set_image( GTK_BUTTON( btn_edit ), xset_get_image( "GTK_STOCK_DIALOG_WARNING",
                                                            GTK_ICON_SIZE_BUTTON ) );
        gtk_button_set_focus_on_click( GTK_BUTTON( btn_edit ), FALSE );
        gtk_text_view_set_editable( input, FALSE );
    }

    if ( defreset )
    {
        btn_default = gtk_button_new_with_mnemonic( _("_Default") );
        gtk_dialog_add_action_widget( GTK_DIALOG( dlg ), btn_default, GTK_RESPONSE_NO );
        gtk_button_set_image( GTK_BUTTON( btn_default ), 
                                        xset_get_image( "GTK_STOCK_REVERT_TO_SAVED",
                                        GTK_ICON_SIZE_BUTTON ) );
        gtk_button_set_focus_on_click( GTK_BUTTON( btn_default ), FALSE );
    }

    GtkWidget* btn_cancel = gtk_button_new_from_stock( GTK_STOCK_CANCEL );
    gtk_dialog_add_action_widget( GTK_DIALOG( dlg ), btn_cancel, GTK_RESPONSE_CANCEL);

    GtkWidget* btn_ok = gtk_button_new_from_stock( GTK_STOCK_OK );
    gtk_dialog_add_action_widget( GTK_DIALOG( dlg ), btn_ok, GTK_RESPONSE_OK);

    // show
    gtk_widget_show_all( dlg );

    if ( title )
        gtk_window_set_title( GTK_WINDOW( dlg ), title );
    if ( edit_care )
    {
        gtk_widget_grab_focus( btn_ok );
        if ( btn_default )
            gtk_widget_set_sensitive( btn_default, FALSE );
    }
    
    char* ans;
    char* trim_ans;
    int response;
    gboolean ret = FALSE;
    while ( response = gtk_dialog_run( GTK_DIALOG( dlg ) ) )
    {
        if ( response == GTK_RESPONSE_OK )
        {
            gtk_text_buffer_get_start_iter( buf, &siter );
            gtk_text_buffer_get_end_iter( buf, &iter );
            ans = gtk_text_buffer_get_text( buf, &siter, &iter, FALSE );
            if ( strchr( ans, '\n' ) )
            {
                ptk_show_error( GTK_WINDOW( dlgparent ), _("Error"),
                                _("Your input is invalid because it contains linefeeds") );
            }
            else
            {
                if ( *answer )
                    g_free( *answer );
                trim_ans = g_strdup( ans );
                trim_ans = g_strstrip( trim_ans );
                if ( ans && trim_ans[0] != '\0' )
                    *answer = g_filename_from_utf8( trim_ans, -1, NULL, NULL, NULL );
                else
                    *answer = NULL;
                if ( ans )
                {
                    g_free( trim_ans );
                    g_free( ans );
                }
                ret = TRUE;
                break;
            }
        }
        else if ( response == GTK_RESPONSE_YES )
        {
            // btn_edit clicked
            gtk_text_view_set_editable( input,
                                        gtk_toggle_button_get_active( 
                                        GTK_TOGGLE_BUTTON( btn_edit ) ) );
            if ( btn_default )
                gtk_widget_set_sensitive( btn_default,
                                        gtk_toggle_button_get_active( 
                                        GTK_TOGGLE_BUTTON( btn_edit ) ) );
        }
        else if ( response == GTK_RESPONSE_NO )
        {
            // btn_default clicked
            gtk_text_buffer_set_text( buf, defreset, -1 );
        }
        else if ( response == GTK_RESPONSE_HELP )
        {
            // btn_help clicked
            if ( help )
            {
                if ( help[0] == '#' )
                {
                    // as anchor
                    xset_show_help( dlg, NULL, help );                    
                }
                else if ( xset_is( help ) )
                {
                    // as set name
                    xset_show_help( dlg, xset_get( help ), NULL );
                }
            }
        }
        else if ( response == GTK_RESPONSE_CANCEL
                                    || response == GTK_RESPONSE_DELETE_EVENT )
            break;
    }
    GtkAllocation allocation;
    gtk_widget_get_allocation( GTK_WIDGET( dlg ), &allocation );
    width = allocation.width;
    height = allocation.height;
    if ( width && height )
    {
        char* str = g_strdup_printf( "%d", width );
        if ( large )
            xset_set( "text_dlg", "s", str );
        else
            xset_set( "text_dlg", "x", str );
        g_free( str );
        str = g_strdup_printf( "%d", height );
        if ( large )
            xset_set( "text_dlg", "z", str );
        else
            xset_set( "text_dlg", "y", str );
        g_free( str );
    }
    gtk_widget_destroy( dlg );
    return ret;
}

/*
gboolean xset_text_dialog_old( GtkWidget* parent, gint width, char* title, char* message,
                                            char* defstring, char** answer )
{   
    GtkWidget* dlgparent = gtk_widget_get_toplevel( parent );
    GtkWidget* dlg = gtk_message_dialog_new( GTK_WINDOW( dlgparent ),
                                  GTK_DIALOG_MODAL,
                                  GTK_MESSAGE_QUESTION,
                                  GTK_BUTTONS_OK_CANCEL,
                                  message );
    //gtk_window_set_resizable( GTK_WINDOW( dlg ), TRUE );
    gtk_widget_set_size_request( GTK_WINDOW( dlg ), width, -1 );
  
    GtkEntry* entry = ( GtkEntry* ) gtk_entry_new();
    if ( defstring )
        gtk_entry_set_text( entry, defstring );

    gtk_box_pack_start( GTK_BOX( GTK_DIALOG( dlg ) ->vbox ), GTK_WIDGET( entry ),
                        TRUE, TRUE, 4 );
    g_signal_connect (G_OBJECT (entry), "activate",
                      G_CALLBACK( xset_text_dialog_activate ), dlg );
    gtk_widget_show_all( dlg );

    gtk_editable_select_region( GTK_EDITABLE(entry), 0, 0 );
    gtk_editable_set_position( GTK_EDITABLE(entry), -1 );
    if ( title )
        gtk_window_set_title( GTK_WINDOW( dlg ), title );
    
    int response = gtk_dialog_run( GTK_DIALOG( dlg ) );
    if ( response == GTK_RESPONSE_OK )
    {
        if ( *answer )
            g_free( *answer );
        *answer = g_filename_from_utf8( gtk_entry_get_text( entry ),
                                              - 1, NULL, NULL, NULL );
        gtk_widget_destroy( dlg );
        return TRUE;
    }
    gtk_widget_destroy( dlg );
    return FALSE;
}

void xset_text_dialog_activate( GtkEntry* entry, GtkDialog* dlg )
{
    // User pressed enter in dialog
    gtk_dialog_response( dlg, GTK_RESPONSE_OK );
}
*/

static gboolean on_fontdlg_keypress ( GtkWidget *widget, GdkEventKey *event,
                                                                GtkWidget* dlg )
{
    if ( event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter )
    {
        gtk_dialog_response( GTK_DIALOG( dlg ), GTK_RESPONSE_YES );
        return TRUE;
    }
    return FALSE;
}

char* xset_font_dialog( GtkWidget* parent, const char* title,
                                    const char* preview, const char* deffont )
{
    char* fontname = NULL;
    GtkWidget* image;
    
    GtkWidget* dlg = gtk_font_selection_dialog_new( title );
    xset_set_window_icon( GTK_WINDOW( dlg ) );
    gtk_window_set_role( GTK_WINDOW( dlg ), "font_dialog" );

    if ( deffont )
        gtk_font_selection_dialog_set_font_name( GTK_FONT_SELECTION_DIALOG( dlg ),
                                                                        deffont );
    if ( title )
        gtk_window_set_title( GTK_WINDOW( dlg ), title );
    if ( preview )
        gtk_font_selection_dialog_set_preview_text( GTK_FONT_SELECTION_DIALOG( dlg ),
                                                                        preview );

        
    int width = xset_get_int( "font_dlg", "x" );
    int height = xset_get_int( "font_dlg", "y" );
    if ( width && height )
        gtk_window_set_default_size( GTK_WINDOW( dlg ), width, height );

    // add default button, rename OK
    GtkButton* btn = GTK_BUTTON( gtk_button_new_from_stock( GTK_STOCK_YES ) );
    gtk_dialog_add_action_widget( GTK_DIALOG(dlg), GTK_WIDGET( btn ),
                                                            GTK_RESPONSE_YES);
    gtk_widget_show( GTK_WIDGET( btn ) );
    GtkButton* ok = GTK_BUTTON( gtk_font_selection_dialog_get_ok_button( 
                                            GTK_FONT_SELECTION_DIALOG( dlg ) ) );
    gtk_button_set_label( ok, _("_Default") );
    image = xset_get_image( "GTK_STOCK_YES", GTK_ICON_SIZE_BUTTON );
    gtk_button_set_image( ok, image );
    gtk_button_set_label( btn, _("_OK") );
    image = xset_get_image( "GTK_STOCK_OK", GTK_ICON_SIZE_BUTTON );
    gtk_button_set_image( btn, image );

    g_signal_connect( G_OBJECT( dlg ), "key-press-event",
                          G_CALLBACK( on_fontdlg_keypress ), dlg );

    gint response = gtk_dialog_run(GTK_DIALOG(dlg));

    GtkAllocation allocation;
    gtk_widget_get_allocation( GTK_WIDGET( dlg ), &allocation );
    width = allocation.width;
    height = allocation.height;
    if ( width && height )
    {
        char* str = g_strdup_printf( "%d", width );
        xset_set( "font_dlg", "x", str );
        g_free( str );
        str = g_strdup_printf( "%d", height );
        xset_set( "font_dlg", "y", str );
        g_free( str );
    }

    if( response == GTK_RESPONSE_YES )
    {
        char* fontname2 = gtk_font_selection_dialog_get_font_name( 
                                                GTK_FONT_SELECTION_DIALOG( dlg ) );
        char* trim_fontname = g_strstrip( fontname2 );
        fontname = g_strdup( trim_fontname );
        g_free( fontname2 );
    }
    else if( response == GTK_RESPONSE_OK )
    {
        // default font
        fontname = g_strdup( "" );
    }
    
    gtk_widget_destroy( dlg );
    return fontname;
}

char* xset_file_dialog( GtkWidget* parent, GtkFileChooserAction action,
                        const char* title, const char* deffolder, const char* deffile )
{
    char* path;
/*  Actions:
 *      GTK_FILE_CHOOSER_ACTION_OPEN
 *      GTK_FILE_CHOOSER_ACTION_SAVE
 *      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
 *      GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER  */
    GtkWidget* dlgparent = gtk_widget_get_toplevel( parent );
    GtkWidget* dlg = gtk_file_chooser_dialog_new( title,
                                   GTK_WINDOW( dlgparent ), action,
                                   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                   GTK_STOCK_OK, GTK_RESPONSE_OK, NULL );
    //gtk_file_chooser_set_action( GTK_FILE_CHOOSER(dlg), GTK_FILE_CHOOSER_ACTION_SAVE );
    gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER(dlg), TRUE );
    xset_set_window_icon( GTK_WINDOW( dlg ) );
    gtk_window_set_role( GTK_WINDOW( dlg ), "file_dialog" );

    if ( deffolder )
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dlg), deffolder );
    else
    {
        path = xset_get_s( "go_set_default" );
        if ( path && path[0] != '\0' )
            gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dlg), path );
        else
        {
            path = (char*)g_get_home_dir();
            gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dlg), path );
        }
    }
    if ( deffile )
    {
        if ( action == GTK_FILE_CHOOSER_ACTION_SAVE
                    || action == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER )
            gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER(dlg), deffile );
        else
        {
            path = g_build_filename( deffolder, deffile, NULL );
            gtk_file_chooser_set_filename( GTK_FILE_CHOOSER (dlg), path );
            g_free( path );
        }
    }

    int width = xset_get_int( "file_dlg", "x" );
    int height = xset_get_int( "file_dlg", "y" );
    if ( width && height )
    {
        // filechooser won't honor default size or size request ?
        gtk_widget_show_all( dlg );
        gtk_window_set_position( GTK_WINDOW( dlg ), GTK_WIN_POS_CENTER_ALWAYS );
        gtk_window_resize( GTK_WINDOW( dlg ), width, height );
        while( gtk_events_pending() )
            gtk_main_iteration();
        gtk_window_set_position( GTK_WINDOW( dlg ), GTK_WIN_POS_CENTER );
    }
    
    gint response = gtk_dialog_run(GTK_DIALOG(dlg));

    GtkAllocation allocation;
    gtk_widget_get_allocation( GTK_WIDGET( dlg ), &allocation );
    width = allocation.width;
    height = allocation.height;
    if ( width && height )
    {
        char* str = g_strdup_printf( "%d", width );
        xset_set( "file_dlg", "x", str );
        g_free( str );
        str = g_strdup_printf( "%d", height );
        xset_set( "file_dlg", "y", str );
        g_free( str );
    }

    if( response == GTK_RESPONSE_OK )
    {
        char* dest = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER ( dlg ) );
        gtk_widget_destroy( dlg );
        return dest;
    }
    gtk_widget_destroy( dlg );
    return NULL;
}

char* xset_color_dialog( GtkWidget* parent, char* title, char* defcolor )
{
    GdkColor color;
    char* scolor = NULL;
    GtkWidget* dlgparent = gtk_widget_get_toplevel( parent );
    GtkWidget* dlg = gtk_color_selection_dialog_new( title );
    GtkWidget* color_sel;
    GtkWidget* help_button;

    g_object_get ( G_OBJECT ( dlg ), "help-button", &help_button, NULL);

    gtk_button_set_label( GTK_BUTTON( help_button ), _("_Unset") );
    gtk_button_set_image( GTK_BUTTON( help_button ), xset_get_image( "GTK_STOCK_REMOVE", GTK_ICON_SIZE_BUTTON ) );

    xset_set_window_icon( GTK_WINDOW( dlg ) );
    gtk_window_set_role( GTK_WINDOW( dlg ), "color_dialog" );

    if ( defcolor && defcolor[0] != '\0' )
    {
        if ( gdk_color_parse( defcolor, &color ) )
        {
            //printf( "        gdk_color_to_string = %s\n", gdk_color_to_string( &color ) );
            color_sel = gtk_color_selection_dialog_get_color_selection( 
                                            GTK_COLOR_SELECTION_DIALOG( dlg ) );
            gtk_color_selection_set_current_color( GTK_COLOR_SELECTION( color_sel ),
                                                                        &color );
        }
    }

    gtk_widget_show_all( dlg );
    gint response = gtk_dialog_run( GTK_DIALOG( dlg ) );

    if ( response == GTK_RESPONSE_OK )
    {
        // color_sel must be set directly before get_current_color
        color_sel = gtk_color_selection_dialog_get_color_selection( 
                                            GTK_COLOR_SELECTION_DIALOG( dlg ) );
        gtk_color_selection_get_current_color( GTK_COLOR_SELECTION( color_sel ),
                                            &color );
        scolor = gdk_color_to_string( &color );
    }
    else if ( response == GTK_RESPONSE_HELP )
        scolor = NULL;
    else  // cancel, delete_event
        scolor = g_strdup( defcolor );
    gtk_widget_destroy( dlg );
    return scolor;
}

GtkWidget* xset_add_toolitem( GtkWidget* parent, PtkFileBrowser* file_browser,
                        GtkWidget* toolbar, int icon_size, XSet* set )
{
    GtkWidget* image = NULL;
    GtkWidget* btn;
    XSet* set_next;

    if ( set->tool == XSET_B_TRUE )
    {
        // button
        const char* icon_name = set->icon;
        if ( !set->menu_style || set->menu_style == XSET_MENU_STRING )
        {
            if ( !set->lock && set->z &&
                        ( !( set->menu_label && set->menu_label[0] )
                          || !( icon_name && icon_name[0] ) ) &&
                                    set->x && atoi( set->x ) == 2 &&
                                    g_str_has_suffix( set->z, ".desktop" ) )
            {
                // Application - get name and/or icon
                const char* menu_label = set->menu_label;
                VFSAppDesktop* app = vfs_app_desktop_new( set->z );
                if ( app )
                {
                    if ( !( menu_label && menu_label[0] ) )
                        menu_label = vfs_app_desktop_get_disp_name( app );
                    if ( !( icon_name && icon_name[0] ) )
                        icon_name = (char*)vfs_app_desktop_get_icon_name( app );
                }
                image = xset_get_image( icon_name, icon_size );
                btn = GTK_WIDGET( gtk_tool_button_new( image,
                                            menu_label && menu_label[0] ?
                                                menu_label : set->z ) );
                if ( app )
                    vfs_app_desktop_unref( app );
            }
            else if ( !set->lock && set->z &&
                                    !( icon_name && icon_name[0] ) &&
                                    set->x && atoi( set->x ) == 3 )
            {
                // Bookmark - get default icon
                if ( icon_name && icon_name[0] )
                    image = xset_get_image( icon_name, icon_size );
                else
                {
                    image = NULL;
                    XSet* book_icon_set = xset_get( "book_icon" );
                    if ( book_icon_set->icon )
                        image = xset_get_image( book_icon_set->icon,
                                                            icon_size );
                    if ( !image )
                        image = xset_get_image( "gtk-directory",
                                                            icon_size );
                }
                btn = GTK_WIDGET( gtk_tool_button_new( image,
                                        set->menu_label && set->menu_label[0] ?
                                            set->menu_label : set->z ) );
            }
            else if ( !set->lock && !( set->menu_label && set->menu_label[0] )
                        && set->z && set->x && atoi( set->x ) > 1 )
            {
                // An app or bookmark with no name
                image = xset_get_image( set->icon, icon_size );
                btn = GTK_WIDGET( gtk_tool_button_new( image, set->z ) );
            }
            else
            {
                image = xset_get_image( set->icon, icon_size );
                btn = GTK_WIDGET( gtk_tool_button_new( image, set->menu_label ) );
            }
            // pass btn back to add_toolbar caller
            set->ob2_data = btn;
            // ob2 in use ?
            if ( set->ob2 )
            {
                g_free( set->ob2 );
                set->ob2 = NULL;
                g_warning( "add_toolbar_item style normal set->ob2 != NULL" );
            }
        }
        else if ( set->menu_style == XSET_MENU_CHECK )
        {
            image = xset_get_image( set->icon, icon_size );
            btn = GTK_WIDGET( gtk_toggle_tool_button_new() );
            gtk_tool_button_set_icon_widget( GTK_TOOL_BUTTON( btn ), image );
            gtk_tool_button_set_label( GTK_TOOL_BUTTON( btn ), set->menu_label );
            gtk_toggle_tool_button_set_active( GTK_TOGGLE_TOOL_BUTTON( btn ),
                                                        xset_get_b_set( set ) );
            // pass btn back to add_toolbar caller
            set->ob2_data = btn;
            // ob2 in use ?
            if ( set->ob2 )
            {
                g_free( set->ob2 );
                set->ob2 = NULL;
                g_warning( "add_toolbar_item style check set->ob2 != NULL" );
            }
        }
        else if ( set->menu_style == XSET_MENU_SUBMENU )
        {
            image = xset_get_image( set->icon, icon_size );
            btn = GTK_WIDGET( gtk_menu_tool_button_new( image, set->menu_label ) );
            if ( set->lock )
            {
                // Create initial menu for btn
                gtk_menu_tool_button_set_menu( GTK_MENU_TOOL_BUTTON( btn ),
                                                                gtk_menu_new() );
                // pass btn back to add_toolbar caller
                set->ob2_data = btn;
                // ob2 in use ?
                if ( set->ob2 )
                {
                    g_free( set->ob2 );
                    set->ob2 = NULL;
                    g_warning( "add_toolbar_item style submenu set->ob2 != NULL" );
                }
            }
            else if ( set->child )
            {
                GtkWidget* submenu = gtk_menu_new();
                GtkAccelGroup* accel_group = gtk_accel_group_new();
                XSet* set_child = xset_get( set->child );
                xset_add_menuitem( NULL, file_browser, submenu, accel_group, set_child );
                gtk_menu_tool_button_set_menu( GTK_MENU_TOOL_BUTTON( btn ), submenu );
                gtk_widget_show_all( submenu );
            }
        }
        else if ( set-> menu_style == XSET_MENU_SEP )
        {
            btn = GTK_WIDGET( gtk_separator_tool_item_new() );
            gtk_separator_tool_item_set_draw( GTK_SEPARATOR_TOOL_ITEM( btn ), TRUE ); 
        }
        else
            return NULL;

        g_object_set_data( G_OBJECT( btn ), "toolbar", toolbar );

        if ( set->ob1 )
            g_object_set_data( G_OBJECT( btn ), set->ob1, set->ob1_data );
        if ( set->ob2 )
            g_object_set_data( G_OBJECT( btn ), set->ob2, set->ob2_data );

        set->browser = file_browser;
        
        // callback
        if ( set->menu_style <= XSET_MENU_SUBMENU )
        {
            if ( set->lock )
            {
                if ( set->cb_func )
                {
                    if ( set->menu_style == XSET_MENU_CHECK )
                        g_signal_connect( btn, "toggled", G_CALLBACK( set->cb_func ),
                                                                        set->cb_data);
                    else
                        g_signal_connect( btn, "clicked", G_CALLBACK( set->cb_func ),
                                                                        set->cb_data);
                }
            }
            else
            {   // cb_func is always unset for !lock????  this can be trimmed?
                set->cb_func = NULL;
                set->cb_data = NULL;
                if ( !set->cb_func || set->menu_style )
                {
                    // use xset menu callback
                    if ( set->menu_style == XSET_MENU_CHECK )
                        g_signal_connect( btn, "toggled", G_CALLBACK( xset_menu_cb ),
                                                                                set );
                    else
                        g_signal_connect( btn, "clicked", G_CALLBACK( xset_menu_cb ),
                                                                                set );
                    g_object_set_data( G_OBJECT(btn), "cb_func", set->cb_func );
                    g_object_set_data( G_OBJECT(btn), "cb_data", set->cb_data );
                }
                else if ( set->cb_func )
                {
                    // use custom callback directly
                    if ( set->menu_style == XSET_MENU_CHECK )
                        g_signal_connect( btn, "toggled", G_CALLBACK( set->cb_func ),
                                                                            set->cb_data );
                    else
                        g_signal_connect( btn, "clicked", G_CALLBACK( set->cb_func ),
                                                                            set->cb_data );
                }
            }
        }        

        // tooltip
        //if ( set->x )
        //    gtk_tool_item_set_tooltip( GTK_TOOL_ITEM( btn ), tooltips, set->x, NULL);

        gtk_toolbar_insert( GTK_TOOLBAR( toolbar ), GTK_TOOL_ITEM( btn ), -1 );
    }
    
    // next toolitem
    if ( set->next )
    {
        set_next = xset_get( set->next );
        xset_add_toolitem( parent, file_browser, toolbar, icon_size, set_next );
    }

    return btn;
}

void xset_add_toolbar( GtkWidget* parent, PtkFileBrowser* file_browser,
            GtkWidget* toolbar, const char* elements )
{
    char* space;
    XSet* set;
    
    if ( !elements )
        return;

    GtkIconSize icon_size = gtk_toolbar_get_icon_size( GTK_TOOLBAR( toolbar ) );
        
    while ( elements[0] == ' ' )
        elements++;

    while ( elements && elements[0] != '\0' )
    {
        space = strchr( elements, ' ' );
        if ( space )
            space[0] = '\0';
        set = xset_get( elements );
        if ( space )
            space[0] = ' ';
        elements = space;
        xset_add_toolitem( parent, file_browser, toolbar, icon_size, set );
        if ( elements )
        {
            while ( elements[0] == ' ' )
                elements++;
        }
    }
}

void open_in_prog( const char* path )
{
    char* prog = g_find_program_in_path( g_get_prgname() );
    if ( !prog )
        prog = g_strdup( g_get_prgname() );
    if ( !prog )
        prog = g_strdup( "spacefm" );
    char* qpath = bash_quote( path );
    char* line = g_strdup_printf( "%s %s", prog, qpath );
    g_spawn_command_line_async( line, NULL );
    g_free( qpath );
    g_free( line );
    g_free( prog );
}

void xset_set_window_icon( GtkWindow* win )
{
    char* name;
    XSet* set = xset_get( "main_icon" );
    if ( set->icon )
        name = set->icon;
    else if ( geteuid() == 0 )
        name = "spacefm-root";
    else
        name = "spacefm";
    GtkIconTheme* theme = gtk_icon_theme_get_default();
    if ( !theme )
        return;
    GdkPixbuf* icon = gtk_icon_theme_load_icon( theme, name, 48, 0, NULL );
    if ( icon )
    {
        gtk_window_set_icon( GTK_WINDOW( win ), icon );
        g_object_unref( icon );
    }
}

char *replace_string( const char* orig, const char* str, const char* replace,
                                                                gboolean quote )
{   // replace all occurrences of str in orig with replace, optionally quoting
    char* rep;
    const char* cur;
    char* result = NULL;
    char* old_result;
    char* s;

    if ( !orig || !( s = strstr( orig, str ) ) )
        return g_strdup( orig );  // str not in orig
    
    if ( !replace )
    {
        if ( quote )
            rep = g_strdup( "''" );
        else
            rep = g_strdup( "" );
    }
    else if ( quote )
        rep = g_strdup_printf( "'%s'", replace );
    else
        rep = g_strdup( replace );

    cur = orig;
    do
    {
        if ( result )
        {
            old_result = result;
            result = g_strdup_printf( "%s%s%s", old_result,
                                            g_strndup( cur, s - cur ), rep );
            g_free( old_result );
        }
        else
            result = g_strdup_printf( "%s%s", g_strndup( cur, s - cur ), rep );
        cur = s + strlen( str );
        s = strstr( cur, str );
    } while ( s );
    old_result = result;
    result = g_strdup_printf( "%s%s", old_result, cur );
    g_free( old_result );
    g_free( rep );
    return result;
    
/*
    // replace first occur of str in orig with rep
    char* buffer;
    char* buffer2;
    char *p;
    char* rep_good;

    if ( !( p = strstr( orig, str ) ) )
        return g_strdup( orig );  // str not in orig
    if ( !rep )
        rep_good = g_strdup_printf( "" );
    else
        rep_good = g_strdup( rep );
    buffer = g_strndup( orig, p - orig );
    if ( quote )
        buffer2 = g_strdup_printf( "%s'%s'%s", buffer, rep_good, p + strlen( str ) );
    else
        buffer2 = g_strdup_printf( "%s%s%s", buffer, rep_good, p + strlen( str ) );
    g_free( buffer );
    g_free( rep_good );
    return buffer2;
*/
}

char* bash_quote( const char* str )
{  
    if ( !str )
        return g_strdup( "''" );
    char* s1 = replace_string( str, "'", "'\\''", FALSE );
    char* s2 = g_strdup_printf( "'%s'", s1 );
    g_free( s1 );
    return s2;
}

char* plain_ascii_name( const char* orig_name )
{
    if ( !orig_name )
        return g_strdup( "" );
    char* orig = g_strdup( orig_name );
    g_strcanon( orig, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_", ' ' );
    char* s = replace_string( orig, " ", "", FALSE );
    g_free( orig );
    return s;
}

char* clean_label( const char* menu_label, gboolean kill_special, gboolean escape )
{
    char* s1;
    char* s2;
    s1 = replace_string( menu_label, "_", "", FALSE );
    if ( kill_special )
    {
        s2 = replace_string( s1, "&", "", FALSE );
        g_free( s1 );
        s1 = replace_string( s2, " ", "-", FALSE );
        g_free( s2 );
    }
    else if ( escape )
    {
        s2 = g_markup_escape_text( s1, -1 );
        g_free( s1 );
        s1 = s2;
    }
    return s1;
}

void string_copy_free( char** s, const char* src )
{
    char* discard = *s;
    *s = g_strdup( src );
    g_free( discard );
}

char* unescape( const char* t )
{
    if ( !t )
        return NULL;
    
    char* s = g_strdup( t );

    int i = 0, j = 0;    
    while ( t[i] )
    {
        switch ( t[i] )
        {
        case '\\':
            switch( t[++i] )
            {
            case 'n':
                s[j] = '\n';
                break;
            case 't':
                s[j] = '\t';
                break;                
            case '\\':
                s[j] = '\\';
                break;
            case '\"':
                s[j] = '\"';
                break;
            default:
                // copy
                s[j++] = '\\';
                s[j] = t[i];
            }
            break;            
        default:
            s[j] = t[i];
        }
        ++i;
        ++j;
    }
    s[j] = t[i];  // null char
    return s;
}

void xset_defaults()
{
    XSet* set;
    
    srand( (unsigned int)time( 0 ) + getpid() );

    // set_last must be set (to anything)
    set_last = xset_get( "separator" );
    set_last->menu_style = XSET_MENU_SEP;

    // toolbars
    set = xset_get( "sep_tool1" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_tool2" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_tool3" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_tool4" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_set( "toolbar_left", "lbl", _("_Left Toolbar") );
    xset_set_set( set, "desc", "tool_device tool_book tool_dirtree tool_newtab tool_newtabhere tool_back tool_backmenu tool_forward tool_forwardmenu tool_up tool_home tool_default tool_refresh" );
    set->menu_style = XSET_MENU_SUBMENU;

    set = xset_set( "toolbar_right", "lbl", _("_Right Toolbar") );
    xset_set_set( set, "desc", "rtool_device rtool_book rtool_dirtree rtool_newtab rtool_newtabhere rtool_back rtool_backmenu rtool_forward rtool_forwardmenu rtool_up rtool_refresh rtool_home rtool_default" );
    set->menu_style = XSET_MENU_SUBMENU;

    set = xset_set( "toolbar_side", "lbl", _("_Side Toolbar") );
    xset_set_set( set, "desc", "stool_device stool_book stool_dirtree stool_newtab stool_newtabhere stool_back stool_backmenu stool_forward stool_forwardmenu stool_up stool_refresh stool_home stool_default" );  // stool_mount stool_mountopen stool_eject
    set->menu_style = XSET_MENU_SUBMENU;

    set = xset_set( "toolbar_hide", "lbl", _("_Hide") );
    xset_set_set( set, "shared_key", "panel1_show_toolbox" );

    set = xset_set( "toolbar_hide_side", "lbl", _("_Hide") );
    xset_set_set( set, "shared_key", "panel1_show_sidebar" );

    set = xset_set( "toolbar_config", "lbl", _("Configure Toolbar") );
    xset_set_set( set, "icn", "gtk-properties" );
    set->tool = XSET_B_TRUE;

    set = xset_set( "toolbar_help", "lbl", _("H_elp") );
    xset_set_set( set, "icn", "gtk-help" );

    // toolitems left
    set = xset_set( "tool_dirtree", "lbl", _("Tree") );
    xset_set_set( set, "icn", "gtk-directory" );
    set->menu_style = XSET_MENU_CHECK;
    set->tool = XSET_B_TRUE;
    xset_set_set( set, "shared_key", "panel1_show_dirtree" );

    set = xset_set( "tool_book", "lbl", _("Bookmarks") );
    xset_set_set( set, "icn", "gtk-jump-to" );
    set->menu_style = XSET_MENU_CHECK;
    set->tool = XSET_B_TRUE;
    xset_set_set( set, "shared_key", "panel1_show_book" );

    set = xset_set( "tool_device", "lbl", _("Devices") );
    xset_set_set( set, "icn", "gtk-harddisk" );
    set->menu_style = XSET_MENU_CHECK;
    set->tool = XSET_B_TRUE;
    xset_set_set( set, "shared_key", "panel1_show_devmon" );

    set = xset_set( "tool_newtab", "lbl", _("New Tab") );
    xset_set_set( set, "icn", "gtk-add" );
    set->tool = XSET_B_FALSE;
    xset_set_set( set, "shared_key", "tab_new" );

    set = xset_set( "tool_newtabhere", "lbl", _("New Tab Here") );
    xset_set_set( set, "icn", "gtk-add" );
    set->tool = XSET_B_TRUE;
    xset_set_set( set, "shared_key", "tab_new_here" );

    set = xset_set( "tool_back", "lbl", _("Back") );
    xset_set_set( set, "icn", "gtk-go-back" );
    set->tool = XSET_B_FALSE;
    xset_set_set( set, "shared_key", "go_back" );

    set = xset_set( "tool_backmenu", "lbl", _("Back Menu") );
    xset_set_set( set, "icn", "gtk-go-back" );
    set->menu_style = XSET_MENU_SUBMENU;
    set->tool = XSET_B_TRUE;

    set = xset_set( "tool_forward", "lbl", _("Forward") );
    xset_set_set( set, "icn", "gtk-go-forward" );
    set->tool = XSET_B_FALSE;
    xset_set_set( set, "shared_key", "go_forward" );

    set = xset_set( "tool_forwardmenu", "lbl", _("Forward Menu") );
    xset_set_set( set, "icn", "gtk-go-forward" );
    set->menu_style = XSET_MENU_SUBMENU;
    set->tool = XSET_B_TRUE;

    set = xset_set( "tool_up", "lbl", _("Up") );
    xset_set_set( set, "icn", "gtk-go-up" );
    set->tool = XSET_B_TRUE;
    xset_set_set( set, "shared_key", "go_up" );

    set = xset_set( "tool_refresh", "lbl", _("Refresh") );
    xset_set_set( set, "icn", "gtk-refresh" );
    set->tool = XSET_B_FALSE;
    xset_set_set( set, "shared_key", "view_refresh" );

    set = xset_set( "tool_home", "lbl", _("Home") );
    xset_set_set( set, "icn", "gtk-home" );
    set->tool = XSET_B_FALSE;
    xset_set_set( set, "shared_key", "go_home" );

    set = xset_set( "tool_default", "lbl", _("Default") );
    xset_set_set( set, "icn", "gtk-home" );
    set->tool = XSET_B_TRUE;
    xset_set_set( set, "shared_key", "go_default" );


    // toolitems right
    set = xset_set( "rtool_dirtree", "lbl", _("Tree") );
    xset_set_set( set, "icn", "gtk-directory" );
    set->menu_style = XSET_MENU_CHECK;
    set->tool = XSET_B_FALSE;
    xset_set_set( set, "shared_key", "panel1_show_dirtree" );

    set = xset_set( "rtool_book", "lbl", _("Bookmarks") );
    xset_set_set( set, "icn", "gtk-jump-to" );
    set->menu_style = XSET_MENU_CHECK;
    set->tool = XSET_B_FALSE;
    xset_set_set( set, "shared_key", "panel1_show_book" );

    set = xset_set( "rtool_device", "lbl", _("Devices") );
    xset_set_set( set, "icn", "gtk-harddisk" );
    set->menu_style = XSET_MENU_CHECK;
    set->tool = XSET_B_FALSE;
    xset_set_set( set, "shared_key", "panel1_show_devmon" );

    set = xset_set( "rtool_newtab", "lbl", _("New Tab") );
    xset_set_set( set, "icn", "gtk-add" );
    set->tool = XSET_B_FALSE;
    xset_set_set( set, "shared_key", "tab_new" );

    set = xset_set( "rtool_newtabhere", "lbl", _("New Tab Here") );
    xset_set_set( set, "icn", "gtk-add" );
    set->tool = XSET_B_FALSE;
    xset_set_set( set, "shared_key", "tab_new_here" );

    set = xset_set( "rtool_back", "lbl", _("Back") );
    xset_set_set( set, "icn", "gtk-go-back" );
    set->tool = XSET_B_FALSE;
    xset_set_set( set, "shared_key", "go_back" );

    set = xset_set( "rtool_backmenu", "lbl", _("Back Menu") );
    xset_set_set( set, "icn", "gtk-go-back" );
    set->menu_style = XSET_MENU_SUBMENU;
    set->tool = XSET_B_FALSE;

    set = xset_set( "rtool_forward", "lbl", _("Forward") );
    xset_set_set( set, "icn", "gtk-go-forward" );
    set->tool = XSET_B_FALSE;
    xset_set_set( set, "shared_key", "go_forward" );

    set = xset_set( "rtool_forwardmenu", "lbl", _("Forward Menu") );
    xset_set_set( set, "icn", "gtk-go-forward" );
    set->menu_style = XSET_MENU_SUBMENU;
    set->tool = XSET_B_FALSE;

    set = xset_set( "rtool_up", "lbl", _("Up") );
    xset_set_set( set, "icn", "gtk-go-up" );
    set->tool = XSET_B_FALSE;
    xset_set_set( set, "shared_key", "go_up" );

    set = xset_set( "rtool_refresh", "lbl", _("Refresh") );
    xset_set_set( set, "icn", "gtk-refresh" );
    set->tool = XSET_B_TRUE;
    xset_set_set( set, "shared_key", "view_refresh" );

    set = xset_set( "rtool_home", "lbl", _("Home") );
    xset_set_set( set, "icn", "gtk-home" );
    set->tool = XSET_B_FALSE;
    xset_set_set( set, "shared_key", "go_home" );

    set = xset_set( "rtool_default", "lbl", _("Default") );
    xset_set_set( set, "icn", "gtk-home" );
    set->tool = XSET_B_FALSE;
    xset_set_set( set, "shared_key", "go_default" );

    // toolitems side
    set = xset_set( "stool_dirtree", "lbl", _("Tree") );
    xset_set_set( set, "icn", "gtk-directory" );
    set->menu_style = XSET_MENU_CHECK;
    set->tool = XSET_B_TRUE;
    xset_set_set( set, "shared_key", "panel1_show_dirtree" );

    set = xset_set( "stool_book", "lbl", _("Bookmarks") );
    xset_set_set( set, "icn", "gtk-jump-to" );
    set->menu_style = XSET_MENU_CHECK;
    set->tool = XSET_B_TRUE;
    xset_set_set( set, "shared_key", "panel1_show_book" );

    set = xset_set( "stool_device", "lbl", _("Devices") );
    xset_set_set( set, "icn", "gtk-harddisk" );
    set->menu_style = XSET_MENU_CHECK;
    set->tool = XSET_B_TRUE;
    xset_set_set( set, "shared_key", "panel1_show_devmon" );

    set = xset_set( "stool_newtab", "lbl", _("New Tab") );
    xset_set_set( set, "icn", "gtk-add" );
    set->tool = XSET_B_FALSE;
    xset_set_set( set, "shared_key", "tab_new" );

    set = xset_set( "stool_newtabhere", "lbl", _("New Tab Here") );
    xset_set_set( set, "icn", "gtk-add" );
    set->tool = XSET_B_FALSE;
    xset_set_set( set, "shared_key", "tab_new_here" );

    set = xset_set( "stool_back", "lbl", _("Back") );
    xset_set_set( set, "icn", "gtk-go-back" );
    set->tool = XSET_B_FALSE;
    xset_set_set( set, "shared_key", "go_back" );

    set = xset_set( "stool_backmenu", "lbl", _("Back Menu") );
    xset_set_set( set, "icn", "gtk-go-back" );
    set->menu_style = XSET_MENU_SUBMENU;
    set->tool = XSET_B_FALSE;

    set = xset_set( "stool_forward", "lbl", _("Forward") );
    xset_set_set( set, "icn", "gtk-go-forward" );
    set->tool = XSET_B_FALSE;
    xset_set_set( set, "shared_key", "go_forward" );

    set = xset_set( "stool_forwardmenu", "lbl", _("Forward Menu") );
    xset_set_set( set, "icn", "gtk-go-forward" );
    set->menu_style = XSET_MENU_SUBMENU;
    set->tool = XSET_B_FALSE;

    set = xset_set( "stool_up", "lbl", _("Up") );
    xset_set_set( set, "icn", "gtk-go-up" );
    set->tool = XSET_B_FALSE;
    xset_set_set( set, "shared_key", "go_up" );

    set = xset_set( "stool_refresh", "lbl", _("Refresh") );
    xset_set_set( set, "icn", "gtk-refresh" );
    set->tool = XSET_B_FALSE;
    xset_set_set( set, "shared_key", "view_refresh" );

    set = xset_set( "stool_home", "lbl", _("Home") );
    xset_set_set( set, "icn", "gtk-home" );
    set->tool = XSET_B_FALSE;

    set = xset_set( "stool_default", "lbl", _("Default") );
    xset_set_set( set, "icn", "gtk-home" );
    set->tool = XSET_B_FALSE;
    xset_set_set( set, "shared_key", "go_default" );

    set = xset_set( "stool_mount", "lbl", _("Mount") );//not added
    xset_set_set( set, "icn", "drive-removable-media" );
    set->tool = XSET_B_FALSE;

    set = xset_set( "stool_mountopen", "lbl", _("Mount & Open") );//not added
    xset_set_set( set, "icn", "gtk-open" );
    set->tool = XSET_B_TRUE;

    set = xset_set( "stool_eject", "lbl", _("Remove") );//not added
    xset_set_set( set, "icn", "gtk-disconnect" );
    set->tool = XSET_B_TRUE;

    // dev menu
    set = xset_get( "sep_dm1" );
    set->menu_style = XSET_MENU_SEP;
        
    set = xset_get( "sep_dm2" );
    set->menu_style = XSET_MENU_SEP;
        
    set = xset_get( "sep_dm3" );
    set->menu_style = XSET_MENU_SEP;
        
    set = xset_get( "sep_dm4" );
    set->menu_style = XSET_MENU_SEP;
        
    set = xset_get( "sep_dm5" );
    set->menu_style = XSET_MENU_SEP;
        
    set = xset_set( "dev_menu_remove", "lbl", _("Remo_ve / Eject") );
    xset_set_set( set, "icn", "gtk-disconnect" );
    set->line = g_strdup( "#devices-menu-remove" );
    
    set = xset_set( "dev_menu_unmount", "lbl", _("_Unmount") );
    xset_set_set( set, "icn", "gtk-remove" );
    set->line = g_strdup( "#devices-menu-unmount" );
    
    set = xset_set( "dev_menu_reload", "lbl", _("Re_load") );
    xset_set_set( set, "icn", "gtk-disconnect" );
    set->line = g_strdup( "#devices-menu-reload" );
    
    set = xset_set( "dev_menu_sync", "lbl", _("_Sync") );
    xset_set_set( set, "icn", "gtk-save" );
    set->line = g_strdup( "#devices-menu-sync" );
   
    set = xset_set( "dev_menu_open", "lbl", _("_Open") );
    xset_set_set( set, "icn", "gtk-open" );
    set->line = g_strdup( "#devices-menu-open" );
   
    set = xset_set( "dev_menu_tab", "lbl", C_("Devices|Open|", "_Tab") );
    xset_set_set( set, "icn", "gtk-add" );
    set->line = g_strdup( "#devices-menu-tab" );
   
    set = xset_set( "dev_menu_mount", "lbl", _("_Mount") );
    xset_set_set( set, "icn", "drive-removable-media" );
    set->line = g_strdup( "#devices-menu-mount" );
   
    set = xset_set( "dev_menu_remount", "lbl", _("Re_/mount") );
    xset_set_set( set, "icn", "gtk-redo" );
    set->line = g_strdup( "#devices-menu-remount" );
   
    set = xset_set( "dev_menu_mark", "lbl", _("_Bookmark") );
    xset_set_set( set, "icn", "gtk-add" );
    set->line = g_strdup( "#devices-menu-bookmark" );

    set = xset_get( "sep_mr1" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_mr2" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_mr3" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_set( "dev_menu_root", "lbl", _("_Root") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "dev_root_unmount dev_root_mount sep_mr1 dev_root_label sep_mr2 dev_root_check dev_menu_format dev_menu_backup dev_menu_restore sep_mr3 dev_root_fstab dev_root_udevil" );
    xset_set_set( set, "icn", "gtk-dialog-warning" );
    set->line = g_strdup( "#devices-root" );
    
    set = xset_set( "dev_root_mount", "lbl", _("_Mount") );
    xset_set_set( set, "icn", "drive-removable-media" );
    xset_set_set( set, "z", "/usr/bin/udevil mount -o %o %v" );
    set->line = g_strdup( "#devices-root-mount" );
   
    set = xset_set( "dev_root_unmount", "lbl", _("_Unmount") );
    xset_set_set( set, "icn", "gtk-remove" );
    xset_set_set( set, "z", "/usr/bin/udevil umount %v" );
    set->line = g_strdup( "#devices-root-unmount" );
   
    set = xset_set( "dev_root_label", "lbl", _("_Label") );
    xset_set_set( set, "icn", "gtk-edit" );
    set->line = g_strdup( "#devices-root-label" );

        // set->y contains current remove label command
        // set->z contains current set label command
        // set->desc contains default remove label command
        // set->title contains default set label command
        
        // ext2,3,4
        set = xset_get( "label_cmd_ext" );
        xset_set_set( set, "desc", "/sbin/tune2fs -L %l %v" );
        xset_set_set( set, "title", set->desc );
        set->line = g_strdup( "#devices-root-label" );

        set = xset_get( "label_cmd_vfat" );
        xset_set_set( set, "desc",  "mlabel -c -i %v ::" );
        xset_set_set( set, "title", "mlabel -i %v ::%l" );
        set->line = g_strdup( "#devices-root-label" );

        set = xset_get( "label_cmd_msdos" );
        xset_set_set( set, "desc",  "mlabel -c -i %v ::" );
        xset_set_set( set, "title", "mlabel -i %v ::%l" );
        set->line = g_strdup( "#devices-root-label" );

        set = xset_get( "label_cmd_fat16" );
        xset_set_set( set, "desc",  "mlabel -c -i %v ::" );
        xset_set_set( set, "title", "mlabel -i %v ::%l" );
        set->line = g_strdup( "#devices-root-label" );

        set = xset_get( "label_cmd_fat32" );
        xset_set_set( set, "desc",  "mlabel -c -i %v ::" );
        xset_set_set( set, "title", "mlabel -i %v ::%l" );
        set->line = g_strdup( "#devices-root-label" );

        set = xset_get( "label_cmd_ntfs" );
        xset_set_set( set, "desc", "/sbin/ntfslabel -f %v %l" );
        xset_set_set( set, "title", set->desc );
        set->line = g_strdup( "#devices-root-label" );

        set = xset_get( "label_cmd_btrfs" );
        xset_set_set( set, "desc", "/sbin/btrfs filesystem label %v %l" );
        xset_set_set( set, "title", set->desc );
        set->line = g_strdup( "#devices-root-label" );

        set = xset_get( "label_cmd_reiserfs" );
        xset_set_set( set, "desc", "/sbin/reiserfstune -l %l %v" );
        xset_set_set( set, "title", set->desc );
        set->line = g_strdup( "#devices-root-label" );

    set = xset_set( "dev_root_check", "lbl", _("_Check") );
    xset_set_set( set, "desc", "/sbin/fsck %v" );
    set->line = g_strdup( "#devices-root-check" );
       
    set = xset_set( "dev_root_fstab", "lbl", _("_Edit fstab") );
    xset_set_set( set, "icn", "gtk-edit" );
    set->line = g_strdup( "#devices-root-fstab" );
       
    set = xset_set( "dev_root_udevil", "lbl", _("Edit u_devil.conf") );
    xset_set_set( set, "icn", "gtk-edit" );
    set->line = g_strdup( "#devices-root-udevil" );
       
    set = xset_set( "dev_menu_format", "lbl", _("_Format") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "dev_fmt_vfat dev_fmt_ntfs dev_fmt_ext2 dev_fmt_ext3 dev_fmt_ext4 dev_fmt_btrfs dev_fmt_reis dev_fmt_reis4 dev_fmt_swap dev_fmt_zero dev_fmt_urand" );
    set->line = g_strdup( "#devices-root-format" );

        set = xset_set( "dev_fmt_vfat", "lbl", "_vfat" );
        xset_set_set( set, "desc", "vfat" );
        xset_set_set( set, "title", "/sbin/mkfs -t vfat %v" );
        set->line = g_strdup( "#devices-root-format" );

        set = xset_set( "dev_fmt_ntfs", "lbl", "_ntfs" );
        xset_set_set( set, "desc", "ntfs" );
        xset_set_set( set, "title", "/sbin/mkfs -t ntfs %v" );
        set->line = g_strdup( "#devices-root-format" );

        set = xset_set( "dev_fmt_ext2", "lbl", "ext_2" );
        xset_set_set( set, "desc", "ext2" );
        xset_set_set( set, "title", "/sbin/mkfs -t ext2 %v" );
        set->line = g_strdup( "#devices-root-format" );

        set = xset_set( "dev_fmt_ext3", "lbl", "ext_3" );
        xset_set_set( set, "desc", "ext3" );
        xset_set_set( set, "title", "/sbin/mkfs -t ext3 %v" );
        set->line = g_strdup( "#devices-root-format" );

        set = xset_set( "dev_fmt_ext4", "lbl", "ext_4" );
        xset_set_set( set, "desc", "ext4" );
        xset_set_set( set, "title", "/sbin/mkfs -t ext4 %v" );
        set->line = g_strdup( "#devices-root-format" );

        set = xset_set( "dev_fmt_btrfs", "lbl", "_btrfs" );
        xset_set_set( set, "desc", "btrfs" );
        xset_set_set( set, "title", "/sbin/mkfs -t btrfs %v" );
        set->line = g_strdup( "#devices-root-format" );

        set = xset_set( "dev_fmt_reis", "lbl", "_reiserfs" );
        xset_set_set( set, "desc", "reiserfs" );
        xset_set_set( set, "title", "/sbin/mkfs -t reiserfs %v" );
        set->line = g_strdup( "#devices-root-format" );

        set = xset_set( "dev_fmt_reis4", "lbl", "r_eiser4" );
        xset_set_set( set, "desc", "reiser4" );
        xset_set_set( set, "title", "/sbin/mkfs -t reiser4 %v" );
        set->line = g_strdup( "#devices-root-format" );

        set = xset_set( "dev_fmt_swap", "lbl", "_swap" );
        xset_set_set( set, "desc", "swap" );
        xset_set_set( set, "title", "/sbin/mkswap %v" );
        set->line = g_strdup( "#devices-root-format" );

        set = xset_set( "dev_fmt_zero", "lbl", "_zero" );
        xset_set_set( set, "desc", "zero" );
        xset_set_set( set, "title", "dd if=/dev/zero of=%v" );
        set->line = g_strdup( "#devices-root-format" );

        set = xset_set( "dev_fmt_urand", "lbl", "_urandom" );
        xset_set_set( set, "desc", "urandom" );
        xset_set_set( set, "title", "dd if=/dev/urandom of=%v" );
        set->line = g_strdup( "#devices-root-format" );

    set = xset_set( "dev_menu_backup", "lbl", _("_Backup") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "dev_back_fsarc dev_back_part dev_back_mbr" );
    set->line = g_strdup( "#devices-root-fsarc" );

        set = xset_set( "dev_back_fsarc", "lbl", "_FSArchiver" );
        xset_set_set( set, "desc", "FSArchiver" );
        xset_set_set( set, "title", "/usr/sbin/fsarchiver -vo -z 7 savefs %s %v" );
        set->line = g_strdup( "#devices-root-fsarc" );

        set = xset_set( "dev_back_part", "lbl", "_Partimage" );
        xset_set_set( set, "desc", "Partimage" );
        xset_set_set( set, "title", "/usr/sbin/partimage -dbo -V 4050 save %v %s" );
        set->line = g_strdup( "#devices-root-parti" );

        set = xset_set( "dev_back_mbr", "lbl", "_MBR" );
        xset_set_set( set, "desc", "MBR" );
        set->line = g_strdup( "#devices-root-mbr" );

    set = xset_get( "sep_mr4" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_set( "dev_menu_restore", "lbl", _("_Restore") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "dev_rest_file sep_mr4 dev_rest_info" );
    set->line = g_strdup( "#devices-root-resfile" );

        set = xset_set( "dev_rest_file", "lbl", _("_From File") );
        xset_set_set( set, "desc", "/usr/sbin/fsarchiver -v restfs %s id=0,dest=%v" );
        xset_set_set( set, "title", "/usr/sbin/partimage -b restore %v %s" );
        set->line = g_strdup( "#devices-root-resfile" );

        set = xset_set( "dev_rest_info", "lbl", _("File _Info") );
        set->line = g_strdup( "#devices-root-resinfo" );
    
    set = xset_set( "dev_prop", "lbl", _("_Properties") );
    set->line = g_strdup( "#devices-menu-properties" );

    set = xset_set( "dev_menu_settings", "lbl", _("Setti_ngs") );
    xset_set_set( set, "icn", "gtk-properties" );
    set->menu_style = XSET_MENU_SUBMENU;
    set->line = g_strdup( "#devices-settings" );

    // dev settings
    set = xset_set( "dev_show", "lbl", _("S_how") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "dev_show_internal_drives dev_show_empty dev_show_partition_tables dev_show_net dev_show_file dev_ignore_udisks_hide dev_show_hide_volumes dev_dispname" );
    set->line = g_strdup( "#devices-settings-internal" );

        set = xset_set( "dev_show_internal_drives", "lbl", _("_Internal Drives") );
        set->menu_style = XSET_MENU_CHECK;
        set->b = geteuid() == 0 ? XSET_B_TRUE : XSET_B_FALSE;
        set->line = g_strdup( "#devices-settings-internal" );

        set = xset_set( "dev_show_empty", "lbl", _("_Empty Drives") );
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_TRUE;  //geteuid() == 0 ? XSET_B_TRUE : XSET_B_UNSET;
        set->line = g_strdup( "#devices-settings-empty" );

        set = xset_set( "dev_show_partition_tables", "lbl", _("_Partition Tables") );
        set->menu_style = XSET_MENU_CHECK;
        set->line = g_strdup( "#devices-settings-table" );

        set = xset_set( "dev_show_net", "lbl", _("Mounted _Networks") );
        set->menu_style = XSET_MENU_CHECK;
        set->line = g_strdup( "#devices-settings-net" );
        set->b = XSET_B_TRUE;

        set = xset_set( "dev_show_file", "lbl", _("Mounted _Files") );
        set->menu_style = XSET_MENU_CHECK;
        set->line = g_strdup( "#devices-settings-files" );
        set->b = XSET_B_TRUE;

        set = xset_set( "dev_show_hide_volumes", "lbl", _("_Volumes...") );
        xset_set_set( set, "title", _("Show/Hide Volumes") );
        xset_set_set( set, "desc", _("To force showing or hiding of some volumes, overriding other settings, you can specify the devices, volume labels, or device IDs in the space-separated list below.\n\nExample:  +/dev/sdd1 -Label With Space +ata-OCZ-part4\nThis would cause /dev/sdd1 and the OCZ device to be shown, and the volume with label \"Label With Space\" to be hidden.\n\nThere must be a space between entries and a plus or minus sign directly before each item.  This list is case-sensitive.\n\n") );
        set->line = g_strdup( "#devices-settings-vol" );

        set = xset_set( "dev_ignore_udisks_hide", "lbl", _("Ignore _Hide Policy") );
        set->menu_style = XSET_MENU_CHECK;
        set->line = g_strdup( "#devices-settings-hide" );

        set = xset_set( "dev_dispname", "lbl", _("_Display Name") );
        set->menu_style = XSET_MENU_STRING;
        xset_set_set( set, "title", _("Set Display Name Format") );
        xset_set_set( set, "desc", _("Enter device display name format:\n\nUse:\n\t%%v\tdevice filename (eg sdd1)\n\t%%s\ttotal size (eg 800G)\n\t%%t\tfstype (eg ext4)\n\t%%l\tvolume label (eg Label or [no media])\n\t%%m\tmount point if mounted, or ---\n\t%%i\tdevice ID\n") );
        xset_set_set( set, "s", "%v %s %l %m" );
        xset_set_set( set, "z", "%v %s %l %m" );
        xset_set_set( set, "icn", "gtk-edit" );
        set->line = g_strdup( "#devices-settings-name" );

    set = xset_set( "dev_menu_auto", "lbl", _("_Auto Mount") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "dev_automount_optical dev_automount_removable dev_ignore_udisks_nopolicy dev_automount_volumes dev_auto_open dev_unmount_quit" );
    set->line = g_strdup( "#devices-settings-optical" );

        set = xset_set( "dev_automount_optical", "lbl", _("Mount _Optical") );
        set->b = geteuid() == 0 ? XSET_B_FALSE : XSET_B_TRUE;
        set->menu_style = XSET_MENU_CHECK;
        set->line = g_strdup( "#devices-settings-optical" );

        set = xset_set( "dev_automount_removable", "lbl", _("_Mount Removable") );
        set->b = geteuid() == 0 ? XSET_B_FALSE : XSET_B_TRUE;
        set->menu_style = XSET_MENU_CHECK;
        set->line = g_strdup( "#devices-settings-remove" );

        set = xset_set( "dev_auto_open", "lbl", _("Open _Tab") );
        set->b = XSET_B_TRUE;
        set->menu_style = XSET_MENU_CHECK;
        set->line = g_strdup( "#devices-settings-tab" );

        set = xset_set( "dev_unmount_quit", "lbl", _("_Unmount On Exit") );
        set->b = XSET_B_UNSET;
        set->menu_style = XSET_MENU_CHECK;
        set->line = g_strdup( "#devices-settings-exit" );

        set = xset_get( "sep_ar1" );
        set->menu_style = XSET_MENU_SEP;

    set = xset_set( "dev_exec", "lbl", _("Auto _Run") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "dev_exec_fs dev_exec_audio dev_exec_video sep_ar1 dev_exec_insert dev_exec_unmount dev_exec_remove" );
    xset_set_set( set, "icn", "gtk-execute" );
    set->line = g_strdup( "#devices-settings-runm" );

        set = xset_set( "dev_exec_fs", "lbl", _("On _Mount") );
        set->menu_style = XSET_MENU_STRING;
        xset_set_set( set, "title", _("Auto Run On Mount") );
        xset_set_set( set, "desc", _("Enter program or bash command line to be run automatically after a removable drive or data disc is auto-mounted:\n\nUse:\n\t%%v\tdevice (eg /dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)") );
        set->line = g_strdup( "#devices-settings-runm" );

        set = xset_set( "dev_exec_audio", "lbl", _("On _Audio CD") );
        set->menu_style = XSET_MENU_STRING;
        xset_set_set( set, "title", _("Auto Run On Audio CD") );
        xset_set_set( set, "desc", _("Enter program or bash command line to be run automatically when an audio CD is inserted in a qualified device:\n\nUse:\n\t%%v\tdevice (eg /dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)") );
        set->line = g_strdup( "#devices-settings-runa" );

        set = xset_set( "dev_exec_video", "lbl", _("On _Video DVD") );
        set->menu_style = XSET_MENU_STRING;
        xset_set_set( set, "title", _("Auto Run On Video DVD") );
        xset_set_set( set, "desc", _("Enter program or bash command line to be run automatically when a video DVD is auto-mounted:\n\nUse:\n\t%%v\tdevice (eg /dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)") );
        set->line = g_strdup( "#devices-settings-runv" );

        set = xset_set( "dev_exec_insert", "lbl", _("On _Insert") );
        set->menu_style = XSET_MENU_STRING;
        xset_set_set( set, "title", _("Auto Run On Insert") );
        xset_set_set( set, "desc", _("Enter program or bash command line to be run automatically when any device is inserted:\n\nUse:\n\t%%v\tdevice added (eg /dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)") );
        set->line = g_strdup( "#devices-settings-runi" );

        set = xset_set( "dev_exec_unmount", "lbl", _("On _Unmount") );
        set->menu_style = XSET_MENU_STRING;
        xset_set_set( set, "title", _("Auto Run On Unmount") );
        xset_set_set( set, "desc", _("Enter program or bash command line to be run automatically when any device is unmounted by any means:\n\nUse:\n\t%%v\tdevice unmounted (eg /dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)") );
        set->line = g_strdup( "#devices-settings-runu" );

        set = xset_set( "dev_exec_remove", "lbl", _("On _Remove") );
        set->menu_style = XSET_MENU_STRING;
        xset_set_set( set, "title", _("Auto Run On Remove") );
        xset_set_set( set, "desc", _("Enter program or bash command line to be run automatically when any device is removed (ejection of media does not qualify):\n\nUse:\n\t%%v\tdevice removed (eg /dev/sda1)\n\t%%l\tdevice label\n\t%%m\tdevice mount point (eg /media/disk)") );
        set->line = g_strdup( "#devices-settings-runr" );

    set = xset_set( "dev_ignore_udisks_nopolicy", "lbl", _("Ignore _No Policy") );
    set->menu_style = XSET_MENU_CHECK;
    set->line = g_strdup( "#devices-settings-nopolicy" );

    set = xset_set( "dev_automount_volumes", "lbl", _("Mount _Volumes...") );
    xset_set_set( set, "title", _("Auto-Mount Volumes") );
    xset_set_set( set, "desc", _("To force or prevent automounting of some volumes, overriding other settings, you can specify the devices, volume labels, or device IDs in the space-separated list below.\n\nExample:  +/dev/sdd1 -Label With Space +ata-OCZ-part4\nThis would cause /dev/sdd1 and the OCZ device to be auto-mounted when detected, and the volume with label \"Label With Space\" to be ignored.\n\nThere must be a space between entries and a plus or minus sign directly before each item.  This list is case-sensitive.\n\n") );
    set->line = g_strdup( "#devices-settings-mvol" );

    set = xset_set( "dev_mount_options", "lbl", _("_Mount Options") );
    xset_set_set( set, "desc", _("Enter your comma- or space-separated list of default mount options below (to be used for all mounts).\n\nIn addition to regular options, you can also specify options to be added or removed for a specific filesystem type by using the form OPTION+FSTYPE or OPTION-FSTYPE.\n\nExample:  nosuid, sync+vfat, sync+ntfs, noatime, noatime-ext4\nThis will add nosuid and noatime for all filesystem types, add sync for vfat and ntfs only, and remove noatime for ext4.\n\nNote: Some options, such as nosuid, may be added by the mount program even if you don't include them.  Options in fstab take precedence.  pmount ignores options set here.") );
    set->menu_style = XSET_MENU_STRING;
    xset_set_set( set, "title", _("Default Mount Options") );
    xset_set_set( set, "s", "noexec, nosuid, noatime" );
    xset_set_set( set, "z", "noexec, nosuid, noatime" );
    xset_set_set( set, "icn", "gtk-edit" );
    set->line = g_strdup( "#devices-settings-opts" );

    set = xset_set( "dev_remount_options", "z", "noexec, nosuid, noatime" );
    set->menu_style = XSET_MENU_STRING;
    xset_set_set( set, "title", _("Re/mount With Options") );
    xset_set_set( set, "desc", _("Device will be (re)mounted using the options below.\n\nIn addition to regular options, you can also specify options to be added or removed for a specific filesystem type by using the form OPTION+FSTYPE or OPTION-FSTYPE.\n\nExample:  nosuid, sync+vfat, sync+ntfs, noatime, noatime-ext4\nThis will add nosuid and noatime for all filesystem types, add sync for vfat and ntfs only, and remove noatime for ext4.\n\nNote: Some options, such as nosuid, may be added by the mount program even if you don't include them.  Options in fstab take precedence.  pmount ignores options set here.") );
    xset_set_set( set, "s", "noexec, nosuid, noatime" );
    set->line = g_strdup( "#devices-menu-remount" );

    set = xset_set( "dev_mount_cmd", "lbl", _("Mount _Command") );
    xset_set_set( set, "desc", _("Enter the command to mount a device:\n\nUse:\n\t%%v\tdevice file ( eg /dev/sda5 )\n\t%%o\tvolume-specific mount options\n\nudevil:\t/usr/bin/udevil mount -o %%o %%v\npmount:\t/usr/bin/pmount %%v\nUdisks2:\t/usr/bin/udisksctl mount -b %%v -o %%o\nUdisks1:\t/usr/bin/udisks --mount %%v --mount-options %%o\n\nLeave blank for auto-detection.") );
    set->menu_style = XSET_MENU_STRING;
    xset_set_set( set, "title", _("Mount Command") );
    xset_set_set( set, "icn", "gtk-edit" );
    set->line = g_strdup( "#devices-settings-mcmd" );

    set = xset_set( "dev_unmount_cmd", "lbl", _("_Unmount Command") );
    xset_set_set( set, "desc", _("Enter the command to unmount a device:\n\nUse:\n\t%%v\tdevice file ( eg /dev/sda5 )\n\nudevil:\t/usr/bin/udevil umount %%v\npmount:\t/usr/bin/pumount %%v\nUdisks1:\t/usr/bin/udisks --unmount %%v\nUdisks2:\t/usr/bin/udisksctl unmount -b %%v\n\nLeave blank for auto-detection.") );
    set->menu_style = XSET_MENU_STRING;
    xset_set_set( set, "title", _("Unmount Command") );
    xset_set_set( set, "icn", "gtk-edit" );
    set->line = g_strdup( "#devices-settings-ucmd" );

    // dev icons
    set = xset_get( "sep_i1" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_i2" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_i3" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_i4" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_set( "dev_icon", "lbl", _("_Icon") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "dev_icon_internal_mounted dev_icon_internal_unmounted sep_i1 dev_icon_remove_mounted dev_icon_remove_unmounted sep_i2 dev_icon_optical_mounted dev_icon_optical_media dev_icon_optical_nomedia dev_icon_audiocd sep_i3 dev_icon_floppy_mounted dev_icon_floppy_unmounted sep_i4 dev_icon_network dev_icon_file" );
    set->line = g_strdup( "#devices-settings-icon" );

        set = xset_set( "dev_icon_audiocd", "lbl", _("Audio CD") );
        set->menu_style = XSET_MENU_ICON;
        xset_set_set( set, "icn", "gtk-cdrom" );
        set->line = g_strdup( "" );
        set->line = g_strdup( "#devices-settings-icon" );

        set = xset_set( "dev_icon_optical_mounted", "lbl", _("Optical Mounted") );
        set->menu_style = XSET_MENU_ICON;
        xset_set_set( set, "icn", "gtk-cdrom" );
        set->line = g_strdup( "#devices-settings-icon" );

        set = xset_set( "dev_icon_optical_media", "lbl", _("Optical Has Media") );
        set->menu_style = XSET_MENU_ICON;
        xset_set_set( set, "icn", "gtk-cdrom" );
        set->line = g_strdup( "#devices-settings-icon" );

        set = xset_set( "dev_icon_optical_nomedia", "lbl", _("Optical No Media") );
        set->menu_style = XSET_MENU_ICON;
        xset_set_set( set, "icn", "gtk-cdrom" );
        set->line = g_strdup( "#devices-settings-icon" );

        set = xset_set( "dev_icon_floppy_mounted", "lbl", _("Floppy Mounted") );
        set->menu_style = XSET_MENU_ICON;
        xset_set_set( set, "icn", "gtk-floppy" );
        set->line = g_strdup( "#devices-settings-icon" );

        set = xset_set( "dev_icon_floppy_unmounted", "lbl", _("Floppy Unmounted") );
        set->menu_style = XSET_MENU_ICON;
        xset_set_set( set, "icn", "gtk-floppy" );
        set->line = g_strdup( "#devices-settings-icon" );

        set = xset_set( "dev_icon_remove_mounted", "lbl", _("Removable Mounted") );
        set->menu_style = XSET_MENU_ICON;
        xset_set_set( set, "icn", "gtk-harddisk" );
        set->line = g_strdup( "#devices-settings-icon" );

        set = xset_set( "dev_icon_remove_unmounted", "lbl", _("Removable Unmounted") );
        set->menu_style = XSET_MENU_ICON;
        xset_set_set( set, "icn", "gtk-harddisk" );
        set->line = g_strdup( "#devices-settings-icon" );

        set = xset_set( "dev_icon_internal_mounted", "lbl", _("Internal Mounted") );
        set->menu_style = XSET_MENU_ICON;
        xset_set_set( set, "icn", "gtk-harddisk" );
        set->line = g_strdup( "#devices-settings-icon" );

        set = xset_set( "dev_icon_internal_unmounted", "lbl", _("Internal Unmounted") );
        set->menu_style = XSET_MENU_ICON;
        xset_set_set( set, "icn", "gtk-harddisk" );
        set->line = g_strdup( "#devices-settings-icon" );

        set = xset_set( "dev_icon_network", "lbl", _("Mounted Network") );
        set->menu_style = XSET_MENU_ICON;
        xset_set_set( set, "icn", "gtk-network" );
        set->line = g_strdup( "#devices-settings-icon" );

        set = xset_set( "dev_icon_file", "lbl", _("Mounted File") );
        set->menu_style = XSET_MENU_ICON;
        xset_set_set( set, "icn", "gtk-file" );
        set->line = g_strdup( "#devices-settings-icon" );

    // Bookmark list
    set = xset_get( "sep_bk1" );
    set->menu_style = XSET_MENU_SEP;
        
    set = xset_get( "sep_bk2" );
    set->menu_style = XSET_MENU_SEP;
        
    set = xset_set( "book_new", "lbl", _("_New") );
    xset_set_set( set, "icn", "gtk-new" );

    set = xset_set( "book_rename", "lbl", _("_Rename") );
    xset_set_set( set, "icn", "gtk-edit" );

    set = xset_set( "book_edit", "lbl", _("_Edit") );
    xset_set_set( set, "icn", "gtk-edit" );
    
    set = xset_set( "book_remove", "lbl", _("Re_move") );
    xset_set_set( set, "icn", "gtk-remove" );
    
    set = xset_set( "book_open", "lbl", _("_Open") );
    xset_set_set( set, "icn", "gtk-open" );

    set = xset_set( "book_tab", "lbl", C_("Bookmarks|Open|", "_Tab") );
    xset_set_set( set, "icn", "gtk-add" );

    set = xset_set( "book_settings", "lbl", _("_Settings") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "icn", "gtk-properties" );

    set = xset_set( "book_icon", "lbl", _("_Icon") );
    set->menu_style = XSET_MENU_ICON;
    // do not set a default icon for book_icon
    
    // Rename/Move Dialog
    set = xset_set( "move_name", "lbl", _("_Name") );
    set->menu_style = XSET_MENU_CHECK;
    
    set = xset_set( "move_filename", "lbl", _("F_ilename") );
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;
    
    set = xset_set( "move_parent", "lbl", _("_Parent") );
    set->menu_style = XSET_MENU_CHECK;
    
    set = xset_set( "move_path", "lbl", _("P_ath") );
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;
    
    set = xset_set( "move_type", "lbl", _("Typ_e") );
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set( "move_target", "lbl", _("Ta_rget") );
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set( "move_template", "lbl", _("Te_mplate") );
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set( "move_option", "lbl", _("_Option") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "move_copy move_link move_copyt move_linkt move_as_root" );

        set = xset_set( "move_copy", "lbl", _("_Copy") );
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_TRUE;
    
        set = xset_set( "move_link", "lbl", _("_Link") );
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_TRUE;
    
        set = xset_set( "move_copyt", "lbl", _("Copy _Target") );
        set->menu_style = XSET_MENU_CHECK;
    
        set = xset_set( "move_linkt", "lbl", _("Lin_k Target") );
        set->menu_style = XSET_MENU_CHECK;
    
        set = xset_set( "move_as_root", "lbl", _("_As Root") );
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_TRUE;
    
    set = xset_set( "move_dlg_font", "lbl", _("_Font") );
    set->menu_style = XSET_MENU_FONTDLG;
    xset_set_set( set, "icn", "gtk-select-font" );
    xset_set_set( set, "title", _("Move Dialog Font") );
    xset_set_set( set, "desc", _("/home/user/Example Filename.ext") );

    set = xset_set( "move_dlg_help", "lbl", _("_Help") );
    xset_set_set( set, "icn", "gtk-help" );

    set = xset_set( "move_dlg_confirm_create", "lbl", _("_Confirm Create") );
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;
 
    // status bar
    set = xset_get( "sep_bar1" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_set( "status_border", "lbl", _("Highlight _Bar") );
    xset_set_set( set, "title", _("Status Bar Highlight Color") );
    xset_set_set( set, "icn", "GTK_STOCK_SELECT_COLOR" );
    set->menu_style = XSET_MENU_COLORDLG;

    set = xset_set( "status_text", "lbl", _("Highlight _Text") );
    xset_set_set( set, "title", _("Status Bar Text Highlight Color") );
    xset_set_set( set, "icn", "GTK_STOCK_SELECT_COLOR" );
    set->menu_style = XSET_MENU_COLORDLG;

    set = xset_set( "status_middle", "lbl", _("_Middle Click") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "status_name status_path status_info status_hide" );

        set = xset_set( "status_name", "lbl", _("Copy _Name") );
        set->menu_style = XSET_MENU_RADIO;

        set = xset_set( "status_path", "lbl", _("Copy _Path") );
        set->menu_style = XSET_MENU_RADIO;

        set = xset_set( "status_info", "lbl", _("File _Info") );
        set->menu_style = XSET_MENU_RADIO;
        set->b = XSET_B_TRUE;
        
        set = xset_set( "status_hide", "lbl", _("_Hide Panel") );
        set->menu_style = XSET_MENU_RADIO;


    // MAIN WINDOW MENUS
    
    // File
    set = xset_get( "sep_f1" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_f2" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_f3" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_set( "main_new_window", "lbl", _("New _Window") );
    xset_set_set( set, "icn", "spacefm" );

    set = xset_set( "main_root_window", "lbl", _("R_oot Window") );
    xset_set_set( set, "icn", "gtk-dialog-warning" );

    set = xset_set( "main_search", "lbl", _("_File Search") );
    xset_set_set( set, "icn", "gtk-find" );

    set = xset_set( "main_terminal", "lbl", _("_Terminal") );
    set->b = XSET_B_UNSET;  // discovery notification

    set = xset_set( "main_root_terminal", "lbl", _("_Root Terminal") );
    xset_set_set( set, "icn", "gtk-dialog-warning" );
    
    set = xset_set( "main_save_session", "lbl", _("_Save Session") );
    xset_set_set( set, "icn", "gtk-save" );

    set = xset_set( "main_save_tabs", "lbl", _("Save Ta_bs") );
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set( "main_exit", "lbl", _("E_xit") );
    xset_set_set( set, "icn", "gtk-quit" );

    // View
    set = xset_get( "sep_v1" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_v2" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_v3" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_v4" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_v5" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_v6" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_v7" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_v8" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_v9" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_set( "panel1_show", "lbl", _("Panel _1") );
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set( "panel2_show", "lbl", _("Panel _2") );
    set->menu_style = XSET_MENU_CHECK;

    set = xset_set( "panel3_show", "lbl", _("Panel _3") );
    set->menu_style = XSET_MENU_CHECK;

    set = xset_set( "panel4_show", "lbl", _("Panel _4") );
    set->menu_style = XSET_MENU_CHECK;

    set = xset_set( "main_pbar", "lbl", _("Panel _Bar") );
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set( "main_focus_panel", "lbl", _("_Go") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "panel_prev panel_next panel_hide panel_1 panel_2 panel_3 panel_4" );
    xset_set_set( set, "icn", "gtk-go-forward" );

        xset_set( "panel_prev", "lbl", _("_Prev") );
        xset_set( "panel_next", "lbl", _("_Next") );
        /*
        xset_set( "panel_left", "lbl", _("_Left") );
        xset_set( "panel_right", "lbl", _("_Right") );
        xset_set( "panel_top", "lbl", _("_Top") );
        xset_set( "panel_bottom", "lbl", _("_Bottom") );
        */
        xset_set( "panel_hide", "lbl", _("_Hide") );
        xset_set( "panel_1", "lbl", _("Panel _1") );
        xset_set( "panel_2", "lbl", _("Panel _2") );
        xset_set( "panel_3", "lbl", _("Panel _3") );
        xset_set( "panel_4", "lbl", _("Panel _4") );

    set = xset_set( "main_auto", "lbl", _("_Events") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "auto_inst auto_win auto_pnl auto_tab evt_device" );
    xset_set_set( set, "icn", "gtk-execute" );
    set->line = g_strdup( "#sockets-menu" );
    
        set = xset_set( "auto_inst", "lbl", _("_Instance") );
        set->menu_style = XSET_MENU_SUBMENU;
        xset_set_set( set, "desc", "evt_start evt_exit" );
        set->line = g_strdup( "#sockets-menu" );

            set = xset_set( "evt_start", "lbl", _("_Startup") );
            set->menu_style = XSET_MENU_STRING;
            xset_set_set( set, "title", _("Set Instance Startup Command") );
            xset_set_set( set, "desc", _("Enter program or bash command line to be run automatically when a SpaceFM instance starts:\n\nUse:\n\t%%e\tevent type  (evt_start)\n") );
            set->line = g_strdup( "#sockets-events-start" );

            set = xset_set( "evt_exit", "lbl", _("_Exit") );
            set->menu_style = XSET_MENU_STRING;
            xset_set_set( set, "title", _("Set Instance Exit Command") );
            xset_set_set( set, "desc", _("Enter program or bash command line to be run automatically when a SpaceFM instance exits:\n\nUse:\n\t%%e\tevent type  (evt_exit)\n") );
            set->line = g_strdup( "#sockets-events-exit" );

        set = xset_set( "auto_win", "lbl", _("_Window") );
        set->menu_style = XSET_MENU_SUBMENU;
        xset_set_set( set, "desc", "evt_win_new evt_win_focus evt_win_move evt_win_click evt_win_key evt_win_close" );
        set->line = g_strdup( "#sockets-menu" );

            set = xset_set( "evt_win_new", "lbl", C_("View|Events|Window|", "_New") );
            set->menu_style = XSET_MENU_STRING;
            xset_set_set( set, "title", _("Set New Window Command") );
            xset_set_set( set, "desc", _("Enter program or bash command line to be run automatically whenever a new SpaceFM window is opened:\n\nUse:\n\t%%e\tevent type  (evt_win_new)\n\t%%w\twindow id  (see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg $fm_pwd, etc) can be used in this command.") );
            set->line = g_strdup( "#sockets-events-winnew" );

            set = xset_set( "evt_win_focus", "lbl", _("_Focus") );
            set->menu_style = XSET_MENU_STRING;
            xset_set_set( set, "title", _("Set Window Focus Command") );
            xset_set_set( set, "desc", _("Enter program or bash command line to be run automatically whenever a SpaceFM window gets focus:\n\nUse:\n\t%%e\tevent type  (evt_win_focus)\n\t%%w\twindow id  (see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg $fm_pwd, etc) can be used in this command.") );
            set->line = g_strdup( "#sockets-events-winfoc" );

            set = xset_set( "evt_win_move", "lbl", _("_Move/Resize") );
            set->menu_style = XSET_MENU_STRING;
            xset_set_set( set, "title", _("Set Window Move/Resize Command") );
            xset_set_set( set, "desc", _("Enter program or bash command line to be run automatically whenever a SpaceFM window is moved or resized:\n\nUse:\n\t%%e\tevent type  (evt_win_move)\n\t%%w\twindow id  (see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg $fm_pwd, etc) can be used in this command.\n\nNote: This command may be run multiple times during resize.") );
            set->line = g_strdup( "#sockets-events-winmov" );

            set = xset_set( "evt_win_click", "lbl", _("_Click") );
            set->menu_style = XSET_MENU_STRING;
            xset_set_set( set, "title", _("Set Click Command") );
            xset_set_set( set, "desc", _("Enter program or bash command line to be run automatically whenever the mouse is clicked:\n\nUse:\n\t%%e\tevent type  (evt_win_click)\n\t%%w\twindow id  (see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\t%%b\tbutton  (mouse button pressed)\n\t%%m\tmodifier  (modifier keys)\n\t%%f\tfocus  (element which received the click)\n\nExported bash variables (eg $fm_pwd, etc) can be used in this command when no asterisk prefix is used.\n\nPrefix your command with an asterisk (*) and conditionally return exit status 0 to inhibit the default handler.  For example:\n*if [ \"%%b\" != \"2\" ]; then exit 1; fi; spacefm -g --label \"\\nMiddle button was clicked in %%f\" --button ok &") );
            set->line = g_strdup( "#sockets-events-winclk" );

            set = xset_set( "evt_win_key", "lbl", _("_Keypress") );
            set->menu_style = XSET_MENU_STRING;
            xset_set_set( set, "title", _("Set Window Keypress Command") );
            xset_set_set( set, "desc", _("Enter program or bash command line to be run automatically whenever a key is pressed:\n\nUse:\n\t%%e\tevent type  (evt_win_key)\n\t%%w\twindow id  (see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\t%%k\tkey code  (key pressed)\n\t%%m\tmodifier  (modifier keys)\n\nExported bash variables (eg $fm_pwd, etc) can be used in this command when no asterisk prefix is used.\n\nPrefix your command with an asterisk (*) and conditionally return exit status 0 to inhibit the default handler.  For example:\n*if [ \"%%k\" != \"0xffc5\" ]; then exit 1; fi; spacefm -g --label \"\\nKey F8 was pressed.\" --button ok &") );
            set->line = g_strdup( "#sockets-events-winkey" );

            set = xset_set( "evt_win_close", "lbl", _("Cl_ose") );
            set->menu_style = XSET_MENU_STRING;
            xset_set_set( set, "title", _("Set Window Close Command") );
            xset_set_set( set, "desc", _("Enter program or bash command line to be run automatically whenever a SpaceFM window is closed:\n\nUse:\n\t%%e\tevent type  (evt_win_close)\n\t%%w\twindow id  (see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg $fm_pwd, etc) can be used in this command.") );
            set->line = g_strdup( "#sockets-events-wincls" );

        set = xset_set( "auto_pnl", "lbl", _("_Panel") );
        set->menu_style = XSET_MENU_SUBMENU;
        xset_set_set( set, "desc", "evt_pnl_focus evt_pnl_show evt_pnl_sel" );
        set->line = g_strdup( "#sockets-menu" );

            set = xset_set( "evt_pnl_focus", "lbl", _("_Focus") );
            set->menu_style = XSET_MENU_STRING;
            xset_set_set( set, "title", _("Set Panel Focus Command") );
            xset_set_set( set, "desc", _("Enter program or bash command line to be run automatically whenever a panel gets focus:\n\nUse:\n\t%%e\tevent type  (evt_pnl_focus)\n\t%%w\twindow id  (see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg $fm_pwd, etc) can be used in this command.") );
            set->line = g_strdup( "#sockets-events-pnlfoc" );

            set = xset_set( "evt_pnl_show", "lbl", C_("View|Events|Panel|", "_Show") );
            set->menu_style = XSET_MENU_STRING;
            xset_set_set( set, "title", _("Set Panel Show Command") );
            xset_set_set( set, "desc", _("Enter program or bash command line to be run automatically whenever a panel or panel element is shown or hidden:\n\nUse:\n\t%%e\tevent type  (evt_pnl_show)\n\t%%w\twindow id  (see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\t%%f\tfocus  (element shown or hidden)\n\t%%v\tvisible  (1 or 0)\n\nExported bash variables (eg $fm_pwd, etc) can be used in this command.") );
            set->line = g_strdup( "#sockets-events-pnlshw" );

            set = xset_set( "evt_pnl_sel", "lbl", _("S_elect") );
            set->menu_style = XSET_MENU_STRING;
            xset_set_set( set, "title", _("Set Panel Select Command") );
            xset_set_set( set, "desc", _("Enter program or bash command line to be run automatically whenever the file selection changes:\n\nUse:\n\t%%e\tevent type  (evt_pnl_sel)\n\t%%w\twindow id  (see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg $fm_pwd, etc) can be used in this command.\n\nPrefix your command with an asterisk (*) and conditionally return exit status 0 to inhibit the default handler.") );
            set->line = g_strdup( "#sockets-events-pnlsel" );

        set = xset_set( "auto_tab", "lbl", C_("View|Events|", "_Tab") );
        set->menu_style = XSET_MENU_SUBMENU;
        xset_set_set( set, "desc", "evt_tab_new evt_tab_focus evt_tab_close" );
        set->line = g_strdup( "#sockets-menu" );

            set = xset_set( "evt_tab_new", "lbl", _("_New") );
            set->menu_style = XSET_MENU_STRING;
            xset_set_set( set, "title", _("Set New Tab Command") );
            xset_set_set( set, "desc", _("Enter program or bash command line to be run automatically whenever a new tab is opened:\n\nUse:\n\t%%e\tevent type  (evt_tab_new)\n\t%%w\twindow id  (see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg $fm_pwd, etc) can be used in this command.") );
            set->line = g_strdup( "#sockets-events-tabnew" );

            set = xset_set( "evt_tab_focus", "lbl", _("_Focus") );
            set->menu_style = XSET_MENU_STRING;
            xset_set_set( set, "title", _("Set Tab Focus Command") );
            xset_set_set( set, "desc", _("Enter program or bash command line to be run automatically whenever a tab gets focus:\n\nUse:\n\t%%e\tevent type  (evt_tab_focus)\n\t%%w\twindow id  (see spacefm -s help)\n\t%%p\tpanel\n\t%%t\ttab\n\nExported bash variables (eg $fm_pwd, etc) can be used in this command.") );
            set->line = g_strdup( "#sockets-events-tabfoc" );

            set = xset_set( "evt_tab_close", "lbl", C_("View|Events|Tab|", "_Close") );
            set->menu_style = XSET_MENU_STRING;
            xset_set_set( set, "title", _("Set Tab Close Command") );
            xset_set_set( set, "desc", _("Enter program or bash command line to be run automatically whenever a tab is closed:\n\nUse:\n\t%%e\tevent type  (evt_tab_close)\n\t%%w\twindow id  (see spacefm -s help)\n\t%%p\tpanel\n\t%%t\tclosed tab") );
            set->line = g_strdup( "#sockets-events-tabcls" );

        set = xset_set( "evt_device", "lbl", _("_Device") );
        set->menu_style = XSET_MENU_STRING;
        xset_set_set( set, "title", _("Set Device Command") );
        xset_set_set( set, "desc", _("Enter program or bash command line to be run automatically whenever a device state changes:\n\nUse:\n\t%%e\tevent type  (evt_device)\n\t%%f\tdevice file\n\t%%v\tchange  (added|removed|changed)\n") );
        set->line = g_strdup( "#sockets-events-device" );

    set = xset_set( "main_title", "lbl", _("Wi_ndow Title") );
    set->menu_style = XSET_MENU_STRING;
    xset_set_set( set, "title", _("Set Window Title Format") );
    xset_set_set( set, "desc", _("Set window title format:\n\nUse:\n\t%%n\tcurrent folder name (eg bin)\n\t%%d\tcurrent folder path (eg /usr/bin)\n\t%%p\tcurrent panel number (1-4)\n\t%%t\tcurrent tab number\n\t%%P\ttotal number of panels visible\n\t%%T\ttotal number of tabs in current panel\n\t*\tasterisk shown if tasks running in window") );
    xset_set_set( set, "s", "%d" );
    xset_set_set( set, "z", "%d" );

    set = xset_set( "main_icon", "lbl", _("_Window Icon") );
    set->menu_style = XSET_MENU_ICON;
    set->title = g_strdup( _("Set Window Icon") );
    set->desc = g_strdup( _("Enter an icon name, icon file path, or stock item name:\n\nNot all icons may work due to various issues.\n\nProvided alternate SpaceFM icons:\n\tspacefm-[48|128]-[cube|pyramid]-[blue|green|red]\n\tspacefm-48-folder-[blue|red]\n\nFor example: spacefm-48-pyramid-green") );

    set = xset_set( "main_full", "lbl", _("_Fullscreen") );
    set->menu_style = XSET_MENU_CHECK;

    set = xset_set( "main_design_mode", "lbl", _("_Design Mode") );
    xset_set_set( set, "icn", "gtk-help" );

    set = xset_set( "main_prefs", "lbl", _("_Preferences") );
    xset_set_set( set, "icn", "gtk-preferences" );

    set = xset_set( "main_tool", "lbl", _("_Tool") );
    set->menu_style = XSET_MENU_SUBMENU;

    set = xset_get( "root_bar" );  // in Preferences
    set->b = XSET_B_TRUE;

    // Plugins
    set = xset_set( "plug_install", "lbl", _("_Install") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "plug_ifile plug_iurl" );
    xset_set_set( set, "icn", "gtk-add" );
    set->line = g_strdup( "#plugins-install" );

        set = xset_set( "plug_ifile", "lbl", _("_File") );
        xset_set_set( set, "icn", "gtk-file" );
        set->line = g_strdup( "#plugins-install" );
        set = xset_set( "plug_iurl", "lbl", _("_URL") );
        xset_set_set( set, "icn", "gtk-network" );
        set->line = g_strdup( "#plugins-install" );

    set = xset_get( "sep_p1" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_set( "plug_copy", "lbl", _("_Import") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "plug_cfile plug_curl sep_p1 plug_cverb" );
    xset_set_set( set, "icn", "gtk-copy" );
    set->line = g_strdup( "#plugins-import" );

        set = xset_set( "plug_cfile", "lbl", _("_File") );
        xset_set_set( set, "icn", "gtk-file" );
        set->line = g_strdup( "#plugins-import" );
        set = xset_set( "plug_curl", "lbl", _("_URL") );
        xset_set_set( set, "icn", "gtk-network" );
        set->line = g_strdup( "#plugins-import" );
        set = xset_set( "plug_cverb", "lbl", _("_Verbose") );
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_TRUE;
        set->line = g_strdup( "#plugins-import" );
        
    set = xset_set( "plug_browse", "lbl", _("_Browse") );

    set = xset_set( "plug_inc", "lbl", _("In_cluded") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "icn", "gtk-media-play" );

    // Help
    set = xset_get( "sep_h1" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_h2" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_h3" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_set( "main_help", "lbl", _("_User's Manual") );
    xset_set_set( set, "icn", "gtk-help" );

    set = xset_set( "main_faq", "lbl", _("_FAQ") );
    xset_set_set( set, "icn", "gtk-help" );

    set = xset_set( "main_homepage", "lbl", _("_Homepage") );
    xset_set_set( set, "icn", "spacefm" );

    set = xset_set( "main_news", "lbl", _("SpaceFM _News") );
    xset_set_set( set, "icn", "spacefm" );

    set = xset_set( "main_getplug", "lbl", _("_Get Plugins") );
    xset_set_set( set, "icn", "spacefm" );

    set = xset_set( "main_help_opt", "lbl", _("_Options") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "main_help_browser main_help_url" );
    xset_set_set( set, "icn", "gtk-properties" );

        set = xset_set( "main_help_browser", "lbl", _("_Browser") );
        set->menu_style = XSET_MENU_STRING;
        xset_set_set( set, "title", _("Choose HTML Browser") );
        xset_set_set( set, "desc", _("Enter browser name or bash command line to be used to display HTML files and URLs:\n\nUse:\n\t%%u\turl\n\n(Leave blank for automatic browser detection)") );
        xset_set_set( set, "icn", "gtk-edit" );

        set = xset_set( "main_help_url", "lbl", _("_Manual Location") );
        set->menu_style = XSET_MENU_STRING;
        xset_set_set( set, "title", _("Choose User's Manual Location") );
        xset_set_set( set, "desc", _("Enter local file path or remote URL for the SpaceFM User's Manual:\n\n(Leave blank for default)\n") );
        xset_set_set( set, "icn", "gtk-edit" );

    set = xset_set( "main_about", "lbl", _("_About") );
    xset_set_set( set, "icn", "gtk-about" );

    set = xset_set( "main_dev", "lbl", _("_Show") );
    xset_set_set( set, "icn", "gtk-harddisk" );
    set = xset_get( "main_dev_sep" );
    set->menu_style = XSET_MENU_SEP;

    // Tasks
    set = xset_get( "sep_t1" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_t2" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_t3" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_t4" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_t5" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_t6" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_set( "main_tasks", "lbl", _("_Task Manager") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "task_show_manager task_hide_manager sep_t1 task_columns task_popups task_errors task_queue" );
    set->line = g_strdup( "#tasks" );
    
    set = xset_set( "task_col_status", "lbl", _("_Status") );
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;
    set->x = g_strdup( "0" );   // column position
    set->y = g_strdup( "130" ); // column width
    
    set = xset_set( "task_col_count", "lbl", _("_Count") );
    set->menu_style = XSET_MENU_CHECK;
    set->x = g_strdup_printf( "%d", 1 );
    set->line = g_strdup( "#tasks-menu-col" );
    
    set = xset_set( "task_col_path", "lbl", _("_Folder") );
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;
    set->x = g_strdup_printf( "%d", 2 );
    set->line = g_strdup( "#tasks-menu-col" );
    
    set = xset_set( "task_col_file", "lbl", _("_Item") );
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;
    set->x = g_strdup_printf( "%d", 3 );
    set->line = g_strdup( "#tasks-menu-col" );
    
    set = xset_set( "task_col_to", "lbl", _("_To") );
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;
    set->x = g_strdup_printf( "%d", 4 );
    set->line = g_strdup( "#tasks-menu-col" );
    
    set = xset_set( "task_col_progress", "lbl", _("_Progress") );
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;
    set->x = g_strdup_printf( "%d", 5 );
    set->y = g_strdup( "100" );
    set->line = g_strdup( "#tasks-menu-col" );
    
    set = xset_set( "task_col_total", "lbl", _("T_otal") );
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;
    set->x = g_strdup_printf( "%d", 6 );
    set->y = g_strdup( "120" );
    set->line = g_strdup( "#tasks-menu-col" );
    
    set = xset_set( "task_col_started", "lbl", _("Sta_rted") );
    set->menu_style = XSET_MENU_CHECK;
    set->x = g_strdup_printf( "%d", 7 );
    set->line = g_strdup( "#tasks-menu-col" );
    
    set = xset_set( "task_col_elapsed", "lbl", _("_Elapsed") );
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;
    set->x = g_strdup_printf( "%d", 8 );
    set->y = g_strdup( "70" );
    set->line = g_strdup( "#tasks-menu-col" );
    
    set = xset_set( "task_col_curspeed", "lbl", _("C_urrent Speed") );
    set->menu_style = XSET_MENU_CHECK;
    set->x = g_strdup_printf( "%d", 9 );
    set->line = g_strdup( "#tasks-menu-col" );
    
    set = xset_set( "task_col_curest", "lbl", _("Current Re_main") );
    set->menu_style = XSET_MENU_CHECK;
    set->x = g_strdup_printf( "%d", 10 );
    set->line = g_strdup( "#tasks-menu-col" );

    set = xset_set( "task_col_avgspeed", "lbl", _("_Average Speed") );
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;
    set->x = g_strdup_printf( "%d", 11 );
    set->y = g_strdup( "60" );
    set->line = g_strdup( "#tasks-menu-col" );
    
    set = xset_set( "task_col_avgest", "lbl", _("A_verage Remain") );
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;
    set->x = g_strdup_printf( "%d", 12 );
    set->y = g_strdup( "65" );
    set->line = g_strdup( "#tasks-menu-col" );

    set = xset_set( "task_col_reorder", "lbl", _("Reor_der") );
    set->line = g_strdup( "#tasks-menu-col" );

    set = xset_set( "task_stop", "lbl", _("_Stop") );
    xset_set_set( set, "icn", "gtk-stop" );
    set->line = g_strdup( "#tasks-menu-stop" );
    set = xset_set( "task_pause", "lbl", _("Pa_use") );
    xset_set_set( set, "icn", "gtk-media-pause" );
    set->line = g_strdup( "#tasks-menu-pause" );
    set = xset_set( "task_que", "lbl", _("_Queue") );
    xset_set_set( set, "icn", "gtk-add" );
    set->line = g_strdup( "#tasks-menu-queue" );
    set = xset_set( "task_resume", "lbl", _("_Resume") );
    xset_set_set( set, "icn", "gtk-media-play" );
    set->line = g_strdup( "#tasks-menu-resume" );
    set = xset_set( "task_showout", "lbl", _("Sho_w Output") );
    set->line = g_strdup( "#tasks-menu-showout" );

    set = xset_set( "task_all", "lbl", _("_All Tasks") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "task_stop_all task_pause_all task_que_all task_resume_all" );
    set->line = g_strdup( "#tasks-menu-all" );

        set = xset_set( "task_stop_all", "lbl", _("_Stop") );
        xset_set_set( set, "icn", "gtk-stop" );
        set->line = g_strdup( "#tasks-menu-all" );
        set = xset_set( "task_pause_all", "lbl", _("Pa_use") );
        xset_set_set( set, "icn", "gtk-media-pause" );
        set->line = g_strdup( "#tasks-menu-all" );
        set = xset_set( "task_que_all", "lbl", _("_Queue") );
        xset_set_set( set, "icn", "gtk-add" );
        set->line = g_strdup( "#tasks-menu-all" );
        set = xset_set( "task_resume_all", "lbl", _("_Resume") );
        xset_set_set( set, "icn", "gtk-media-play" );
        set->line = g_strdup( "#tasks-menu-all" );

    set = xset_set( "task_show_manager", "lbl", _("Show _Manager") );
    set->menu_style = XSET_MENU_RADIO;
    set->b = XSET_B_FALSE;
    set->line = g_strdup( "#tasks-menu-show" );

    set = xset_set( "task_hide_manager", "lbl", _("Auto-_Hide Manager") );
    set->menu_style = XSET_MENU_RADIO;
    set->b = XSET_B_TRUE;
    set->line = g_strdup( "#tasks-menu-auto" );

    set = xset_set( "font_task", "lbl", _("_Font") );
    set->menu_style = XSET_MENU_FONTDLG;
    xset_set_set( set, "icn", "gtk-select-font" );
    xset_set_set( set, "title", _("Task Manager Font") );
    xset_set_set( set, "desc", _("copying  File  1:15  65.2 M  30.2 M/s") );
    set->line = g_strdup( "#tasks-menu-col" );

    set = xset_set( "task_columns", "lbl", _("_Columns") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "task_col_count task_col_path task_col_file task_col_to task_col_progress task_col_total task_col_started task_col_elapsed task_col_curspeed task_col_curest task_col_avgspeed task_col_avgest sep_t2 task_col_reorder font_task" );
    set->line = g_strdup( "#tasks-menu-col" );

    set = xset_set( "task_popups", "lbl", _("_Popups") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "task_pop_all task_pop_top task_pop_above task_pop_stick sep_t6 task_pop_detail task_pop_over task_pop_err task_pop_font" );
    set->line = g_strdup( "#tasks-menu-popall" );

        set = xset_set( "task_pop_all", "lbl", _("Popup _All Tasks") );
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_FALSE;
        set->line = g_strdup( "#tasks-menu-popall" );

        set = xset_set( "task_pop_top", "lbl", _("Stay On _Top") );
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_FALSE;
        set->line = g_strdup( "#tasks-menu-poptop" );

        set = xset_set( "task_pop_above", "lbl", _("A_bove Others") );
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_FALSE;
        set->line = g_strdup( "#tasks-menu-popabove" );

        set = xset_set( "task_pop_stick", "lbl", _("All _Workspaces") );
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_FALSE;
        set->line = g_strdup( "#tasks-menu-popstick" );

        set = xset_set( "task_pop_detail", "lbl", _("_Detailed Stats") );
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_FALSE;
        set->line = g_strdup( "#tasks-menu-popdet" );

        set = xset_set( "task_pop_over", "lbl", _("_Overwrite Option") );
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_TRUE;
        set->line = g_strdup( "#tasks-menu-popover" );

        set = xset_set( "task_pop_err", "lbl", _("_Error Option") );
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_TRUE;
        set->line = g_strdup( "#tasks-menu-poperropt" );

        set = xset_set( "task_pop_font", "lbl", _("_Font") );
        set->menu_style = XSET_MENU_FONTDLG;
        xset_set_set( set, "icn", "gtk-select-font" );
        xset_set_set( set, "title", _("Task Popup Font (affects new tasks)") );
        xset_set_set( set, "desc", _("Example Output 0123456789") );
        set->s = g_strdup( "Monospace 11" );
        set->line = g_strdup( "#tasks-menu-popfont" );
    
    set = xset_set( "task_errors", "lbl", _("Err_ors") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "task_err_first task_err_any task_err_cont" );
    set->line = g_strdup( "#tasks-menu-poperr" );

        set = xset_set( "task_err_first", "lbl", _("Stop If _First") );
        set->menu_style = XSET_MENU_RADIO;
        set->b = XSET_B_TRUE;
        set->line = g_strdup( "#tasks-menu-poperr" );

        set = xset_set( "task_err_any", "lbl", _("Stop On _Any") );
        set->menu_style = XSET_MENU_RADIO;
        set->b = XSET_B_FALSE;
        set->line = g_strdup( "#tasks-menu-poperr" );

        set = xset_set( "task_err_cont", "lbl", _("_Continue") );
        set->menu_style = XSET_MENU_RADIO;
        set->b = XSET_B_FALSE;
        set->line = g_strdup( "#tasks-menu-poperr" );

    set = xset_set( "task_queue", "lbl", _("Qu_eue") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "task_q_new task_q_smart task_q_pause" );
    set->line = g_strdup( "#tasks-menu-new" );

        set = xset_set( "task_q_new", "lbl", _("_Queue New Tasks") );
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_TRUE;
        set->line = g_strdup( "#tasks-menu-new" );

        set = xset_set( "task_q_smart", "lbl", _("_Smart Queue") );
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_TRUE;
        set->line = g_strdup( "#tasks-menu-smart" );

        set = xset_set( "task_q_pause", "lbl", _("_Pause On Error") );
        set->menu_style = XSET_MENU_CHECK;
        set->line = g_strdup( "#tasks-menu-qpause" );

    // Desktop
    set = xset_get( "sep_desk1" );
    set->menu_style = XSET_MENU_SEP;
    set = xset_get( "sep_desk2" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_set( "desk_icons", "lbl", _("Arrange _Icons") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "icn", "gtk-sort-ascending" );
    xset_set_set( set, "desc", "desk_sort_name desk_sort_type desk_sort_date desk_sort_size desk_sort_cust sep_desk1 desk_sort_ascend desk_sort_descend" );

        set = xset_set( "desk_sort_name", "lbl", _("By _Name") );
        set->menu_style = XSET_MENU_RADIO;

        set = xset_set( "desk_sort_type", "lbl", _("By _Type") );
        set->menu_style = XSET_MENU_RADIO;

        set = xset_set( "desk_sort_date", "lbl", _("By _Date") );
        set->menu_style = XSET_MENU_RADIO;

        set = xset_set( "desk_sort_size", "lbl", _("By _Size") );
        set->menu_style = XSET_MENU_RADIO;

        set = xset_set( "desk_sort_cust", "lbl", _("_Custom") );
        set->menu_style = XSET_MENU_RADIO;

        set = xset_set( "desk_sort_ascend", "lbl", _("_Ascending") );
        set->menu_style = XSET_MENU_RADIO;

        set = xset_set( "desk_sort_descend", "lbl", _("D_escending") );
        set->menu_style = XSET_MENU_RADIO;

    set = xset_set( "desk_pref", "lbl", _("Desktop _Settings") );
    xset_set_set( set, "icn", "gtk-preferences" );

    set = xset_set( "desk_dev", "lbl", _("De_vices") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "icn", "gtk-harddisk" );

    set = xset_set( "desk_book", "lbl", _("_Bookmarks") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "icn", "gtk-jump-to" );

    set = xset_set( "desk_open", "lbl", _("_Desktop Folder") );
    xset_set_set( set, "icn", "gtk-open" );

    // Menu Item Properties
    set = xset_get( "sep_ctxt" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_set( "context_dlg", "lbl", _("_Font") );
    set->menu_style = XSET_MENU_FONTDLG;
    xset_set_set( set, "icn", "gtk-select-font" );
    xset_set_set( set, "title", _("Editor Font") );
    xset_set_set( set, "desc", _("Example Input 0123456789") );
    set->s = g_strdup( "Monospace 11" );
    // set->b reserved for Ignore Context

    // PANELS COMMON
    set = xset_get( "sep_new" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_edit" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_tab" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_entry" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_mopt1" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_mopt2" );
    set->menu_style = XSET_MENU_SEP;

    xset_set( "date_format", "s", "%Y-%m-%d %H:%M" );

    set = xset_set( "input_font", "lbl", _("_Font") );
    set->menu_style = XSET_MENU_FONTDLG;
    xset_set_set( set, "icn", "gtk-select-font" );
    xset_set_set( set, "title", _("Input Font") );
    xset_set_set( set, "desc", _("Example Input 0123456789") );

    set = xset_set( "con_open", "lbl", _("_Open") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "icn", "gtk-open" );

    set = xset_set( "open_execute", "lbl", _("E_xecute") );
    xset_set_set( set, "icn", "gtk-execute" );

    set = xset_set( "open_edit", "lbl", _("Edi_t") );
    xset_set_set( set, "icn", "gtk-edit" );

    set = xset_set( "open_edit_root", "lbl", _("Edit As _Root") );
    xset_set_set( set, "icn", "gtk-dialog-warning" );

    set = xset_set( "open_other", "lbl", _("_Choose...") );
    xset_set_set( set, "icn", "gtk-open" );

    set = xset_set( "open_all", "lbl", _("_Default") );//virtual

    set = xset_set( "open_in_tab", "lbl", _("In _Tab") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "opentab_new opentab_prev opentab_next opentab_1 opentab_2 opentab_3 opentab_4 opentab_5 opentab_6 opentab_7 opentab_8 opentab_9 opentab_10" );

        xset_set( "opentab_new", "lbl", _("N_ew") );
        xset_set( "opentab_prev", "lbl", _("_Prev") );
        xset_set( "opentab_next", "lbl", _("_Next") );
        xset_set( "opentab_1", "lbl", _("Tab _1") );
        xset_set( "opentab_2", "lbl", _("Tab _2") );
        xset_set( "opentab_3", "lbl", _("Tab _3") );
        xset_set( "opentab_4", "lbl", _("Tab _4") );
        xset_set( "opentab_5", "lbl", _("Tab _5") );
        xset_set( "opentab_6", "lbl", _("Tab _6") );
        xset_set( "opentab_7", "lbl", _("Tab _7") );
        xset_set( "opentab_8", "lbl", _("Tab _8") );
        xset_set( "opentab_9", "lbl", _("Tab _9") );
        xset_set( "opentab_10", "lbl", _("Tab 1_0") );

    set = xset_set( "open_in_panel", "lbl", _("In _Panel") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "open_in_panelprev open_in_panelnext open_in_panel1 open_in_panel2 open_in_panel3 open_in_panel4" );
    
    xset_set( "open_in_panelprev", "lbl", _("_Prev") );
    xset_set( "open_in_panelnext", "lbl", _("_Next") );
    xset_set( "open_in_panel1", "lbl", _("Panel _1") );
    xset_set( "open_in_panel2", "lbl", _("Panel _2") );
    xset_set( "open_in_panel3", "lbl", _("Panel _3") );
    xset_set( "open_in_panel4", "lbl", _("Panel _4") );

    set = xset_set( "arc_extract", "lbl", _("_Extract") );
    xset_set_set( set, "icn", "gtk-convert" );

    set = xset_set( "arc_extractto", "lbl", _("Extract _To") );
    xset_set_set( set, "icn", "gtk-convert" );

    set = xset_set( "arc_list", "lbl", _("_List Contents") );
    xset_set_set( set, "icn", "gtk-file" );

    set = xset_get( "sep_arc1" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_set( "arc_default", "lbl", _("_Archive Default") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "arc_def_open arc_def_ex arc_def_exto arc_def_list sep_arc1 arc_def_parent arc_def_write" );

        set = xset_set( "arc_def_open", "lbl", _("_Open With App") );
        set->menu_style = XSET_MENU_RADIO;

        set = xset_set( "arc_def_ex", "lbl", _("_Extract") );
        set->menu_style = XSET_MENU_RADIO;
        set->b = XSET_B_TRUE;
        
        set = xset_set( "arc_def_exto", "lbl", _("Extract _To") );
        set->menu_style = XSET_MENU_RADIO;

        set = xset_set( "arc_def_list", "lbl", _("_List Contents") );
        set->menu_style = XSET_MENU_RADIO;

        set = xset_set( "arc_def_parent", "lbl", _("_Create Subfolder") );
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_TRUE;

        set = xset_set( "arc_def_write", "lbl", _("_Write Access") );
        set->menu_style = XSET_MENU_CHECK;

    set = xset_set( "iso_mount", "lbl", _("_Mount ISO") );
    xset_set_set( set, "icn", "gtk-cdrom" );

    set = xset_set( "iso_auto", "lbl", _("_Auto-Mount ISO") );
    set->menu_style = XSET_MENU_CHECK;

    set = xset_get( "sep_o1" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_set( "open_new", "lbl", _("_New") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "new_file new_folder new_link new_archive sep_o1 tab_new tab_new_here new_bookmark" );
    xset_set_set( set, "icn", "gtk-new" );

        set = xset_set( "new_file", "lbl", _("_File") );
        xset_set_set( set, "icn", "gtk-file" );

        set = xset_set( "new_folder", "lbl", _("Fol_der") );
        xset_set_set( set, "icn", "gtk-directory" );

        set = xset_set( "new_link", "lbl", _("_Link") );
        xset_set_set( set, "icn", "gtk-file" );

        set = xset_set( "new_bookmark", "lbl", C_("New|", "_Bookmark") );
        xset_set_set( set, "shared_key", "book_new" );
        xset_set_set( set, "icn", "gtk-jump-to" );
        
        set = xset_set( "new_archive", "lbl", _("_Archive") );
        xset_set_set( set, "icn", "gtk-save-as" );

        set = xset_get( "arc_dlg" );
        set->b = XSET_B_TRUE;
        xset_set_set( set, "s", "1" );
        
        set = xset_set( "tab_new", "lbl", C_("New|", "_Tab") );
        xset_set_set( set, "icn", "gtk-add" );
        set = xset_set( "tab_new_here", "lbl", _("Tab _Here") );
        xset_set_set( set, "icn", "gtk-add" );

        set = xset_set( "new_app", "lbl", _("_Desktop Application") );
        xset_set_set( set, "icn", "gtk-add" );

    set = xset_get( "sep_g1" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_set( "con_go", "lbl", _("_Go") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "go_back go_forward go_up go_home go_default go_set_default edit_canon sep_g1 go_tab go_focus" );
    xset_set_set( set, "icn", "gtk-go-forward" );

    set = xset_set( "go_back", "lbl", _("_Back") );
        xset_set_set( set, "icn", "gtk-go-back" );
    set = xset_set( "go_forward", "lbl", _("_Forward") );
        xset_set_set( set, "icn", "gtk-go-forward" );
    set = xset_set( "go_up", "lbl", _("_Up") );
        xset_set_set( set, "icn", "gtk-go-up" );
    set = xset_set( "go_home", "lbl", _("_Home") );
        xset_set_set( set, "icn", "gtk-home" );
    set = xset_set( "go_default", "lbl", _("_Default") );
        xset_set_set( set, "icn", "gtk-home" );

    set = xset_set( "go_set_default", "lbl", _("_Set Default") );
        xset_set_set( set, "icn", "gtk-save" );

    set = xset_set( "edit_canon", "lbl", _("Re_al Path") );

    set = xset_set( "go_focus", "lbl", _("Fo_cus") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "focus_path_bar focus_filelist focus_dirtree focus_book focus_device" );    

        set = xset_set( "focus_path_bar", "lbl", _("_Path Bar") );
            xset_set_set( set, "icn", "gtk-dialog-question" );
        set = xset_set( "focus_filelist", "lbl", _("_File List") );
            xset_set_set( set, "icn", "gtk-file" );
        set = xset_set( "focus_dirtree", "lbl", _("_Tree") );
            xset_set_set( set, "icn", "gtk-directory" );
        set = xset_set( "focus_book", "lbl", _("_Bookmarks") );
            xset_set_set( set, "icn", "gtk-jump-to" );
        set = xset_set( "focus_device", "lbl", _("De_vices") );
            xset_set_set( set, "icn", "gtk-harddisk" );

    set = xset_set( "go_tab", "lbl", C_("Go|", "_Tab") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "tab_prev tab_next tab_close tab_1 tab_2 tab_3 tab_4 tab_5 tab_6 tab_7 tab_8 tab_9 tab_10" );

        xset_set( "tab_prev", "lbl", _("_Prev") );
        xset_set( "tab_next", "lbl", _("_Next") );
        set = xset_set( "tab_close", "lbl", _("_Close") );
            xset_set_set( set, "icn", "gtk-close" );        
        xset_set( "tab_1", "lbl", _("Tab _1") );
        xset_set( "tab_2", "lbl", _("Tab _2") );
        xset_set( "tab_3", "lbl", _("Tab _3") );
        xset_set( "tab_4", "lbl", _("Tab _4") );
        xset_set( "tab_5", "lbl", _("Tab _5") );
        xset_set( "tab_6", "lbl", _("Tab _6") );
        xset_set( "tab_7", "lbl", _("Tab _7") );
        xset_set( "tab_8", "lbl", _("Tab _8") );
        xset_set( "tab_9", "lbl", _("Tab _9") );
        xset_set( "tab_10", "lbl", _("Tab 1_0") );

    set = xset_set( "con_view", "lbl", _("_View") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "icn", "gtk-preferences" );

    set = xset_set( "view_list_style", "lbl", _("Styl_e") );
    set->menu_style = XSET_MENU_SUBMENU;

    set = xset_set( "view_columns", "lbl", _("C_olumns") );
    set->menu_style = XSET_MENU_SUBMENU;

    set = xset_set( "view_reorder_col", "lbl", _("_Reorder") );

    set = xset_set( "rubberband", "lbl", _("_Rubberband Select") );
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_get( "sep_s1" );
    set->menu_style = XSET_MENU_SEP;
    set = xset_get( "sep_s2" );
    set->menu_style = XSET_MENU_SEP;
    set = xset_get( "sep_s3" );
    set->menu_style = XSET_MENU_SEP;
    set = xset_get( "sep_s4" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_set( "view_sortby", "lbl", _("_Sort") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "sortby_name sortby_size sortby_type sortby_perm sortby_owner sortby_date sep_s1 sortby_ascend sortby_descend sep_s2 sortx_natural sortx_case sep_s3 sortx_folders sortx_files sortx_mix sep_s4 sortx_hidfirst sortx_hidlast" );

        set = xset_set( "sortby_name", "lbl", _("_Name") );
        set->menu_style = XSET_MENU_RADIO;
        set = xset_set( "sortby_size", "lbl", _("_Size") );
        set->menu_style = XSET_MENU_RADIO;
        set = xset_set( "sortby_type", "lbl", _("_Type") );
        set->menu_style = XSET_MENU_RADIO;
        set = xset_set( "sortby_perm", "lbl", _("_Permission") );
        set->menu_style = XSET_MENU_RADIO;
        set = xset_set( "sortby_owner", "lbl", _("_Owner") );
        set->menu_style = XSET_MENU_RADIO;
        set = xset_set( "sortby_date", "lbl", _("_Modified") );
        set->menu_style = XSET_MENU_RADIO;
        set = xset_set( "sortby_ascend", "lbl", _("_Ascending") );
        set->menu_style = XSET_MENU_RADIO;
        set = xset_set( "sortby_descend", "lbl", _("_Descending") );
        set->menu_style = XSET_MENU_RADIO;

        set = xset_set( "sortx_natural", "lbl", _("Nat_ural") );
        set->menu_style = XSET_MENU_CHECK;
        set = xset_set( "sortx_case", "lbl", _("_Case Sensitive") );
        set->menu_style = XSET_MENU_CHECK;
        set = xset_set( "sortx_folders", "lbl", _("Folders Fi_rst") );
        set->menu_style = XSET_MENU_RADIO;
        set = xset_set( "sortx_files", "lbl", _("F_iles First") );
        set->menu_style = XSET_MENU_RADIO;
        set = xset_set( "sortx_mix", "lbl", _("Mi_xed") );
        set->menu_style = XSET_MENU_RADIO;
        set = xset_set( "sortx_hidfirst", "lbl", _("_Hidden First") );
        set->menu_style = XSET_MENU_RADIO;
        set = xset_set( "sortx_hidlast", "lbl", _("Hidden _Last") );
        set->menu_style = XSET_MENU_RADIO;

    set = xset_set( "view_refresh", "lbl", _("Re_fresh") );
    xset_set_set( set, "icn", "gtk-refresh" );

    set = xset_set( "path_seek", "lbl", _("Auto See_k") );
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;
    set->line = g_strdup( "#gui-pathbar-seek" );

    set = xset_set( "path_hand", "lbl", _("P_rotocol Handler...") );
    set->menu_style = XSET_MENU_STRING;
    xset_set_set( set, "title", _("Set Protocol Handler") );
    xset_set_set( set, "desc", _("Enter command to be used to mount or open protocols (such as nfs://, smb://, etc):\n\nIf this setting is empty, SpaceFM will open protocols using 'udevil mount'.\n\nTIP:  To unmount networks, install udevil or set Unmount Command to a command which handles network protocols.\n") );
    xset_set_set( set, "icn", "gtk-execute" );
    set->line = g_strdup( "#gui-pathbar-protohand" );

    set = xset_set( "path_help", "lbl", _("Path Bar _Help") );
    xset_set_set( set, "icn", "gtk-help" );

    // EDIT
    set = xset_set( "edit_cut", "lbl", _("Cu_t") );
    xset_set_set( set, "icn", "gtk-cut" );

    set = xset_set( "edit_copy", "lbl", _("_Copy") );
    xset_set_set( set, "icn", "gtk-copy" );

    set = xset_set( "edit_paste", "lbl", _("_Paste") );
    xset_set_set( set, "icn", "gtk-paste" );
    
    set = xset_set( "edit_rename", "lbl", _("_Rename") );
    xset_set_set( set, "icn", "gtk-edit" );
    
    set = xset_set( "edit_delete", "lbl", _("_Delete") );
    xset_set_set( set, "icn", "gtk-delete" );

    set = xset_get( "sep_e1" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_e2" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_get( "sep_e3" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_set( "edit_submenu", "lbl", _("_Edit") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "copy_name copy_parent copy_path sep_e1 paste_link paste_target paste_as sep_e2 copy_to move_to edit_root edit_hide sep_e3 select_all select_patt select_invert select_un" );
    xset_set_set( set, "icn", "gtk-edit" );

        set = xset_set( "copy_name", "lbl", _("Copy _Name") );
        xset_set_set( set, "icn", "gtk-copy" );

        set = xset_set( "copy_path", "lbl", _("Copy _Path") );
        xset_set_set( set, "icn", "gtk-copy" );

        set = xset_set( "copy_parent", "lbl", _("Copy Pa_rent") );
        xset_set_set( set, "icn", "gtk-copy" );

        set = xset_set( "paste_link", "lbl", _("Paste _Link") );
        xset_set_set( set, "icn", "gtk-paste" );

        set = xset_set( "paste_target", "lbl", _("Paste _Target") );
        xset_set_set( set, "icn", "gtk-paste" );

        set = xset_set( "paste_as", "lbl", _("Paste _As") );
        xset_set_set( set, "icn", "gtk-paste" );

    set = xset_get( "sep_c1" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_set( "copy_to", "lbl", _("_Copy To") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "copy_loc copy_loc_last sep_c1 copy_tab copy_panel" );

        set = xset_set( "copy_loc", "lbl", _("L_ocation") );
        set = xset_set( "copy_loc_last", "lbl", _("L_ast Location") );
        xset_set_set( set, "icn", "gtk-redo" );

        set = xset_set( "copy_tab", "lbl", C_("Edit|CopyTo|", "_Tab") );
        set->menu_style = XSET_MENU_SUBMENU;
        xset_set_set( set, "desc", "copy_tab_prev copy_tab_next copy_tab_1 copy_tab_2 copy_tab_3 copy_tab_4 copy_tab_5 copy_tab_6 copy_tab_7 copy_tab_8 copy_tab_9 copy_tab_10" );

            xset_set( "copy_tab_prev", "lbl", _("_Prev") );
            xset_set( "copy_tab_next", "lbl", _("_Next") );
            xset_set( "copy_tab_1", "lbl", _("Tab _1") );
            xset_set( "copy_tab_2", "lbl", _("Tab _2") );
            xset_set( "copy_tab_3", "lbl", _("Tab _3") );
            xset_set( "copy_tab_4", "lbl", _("Tab _4") );
            xset_set( "copy_tab_5", "lbl", _("Tab _5") );
            xset_set( "copy_tab_6", "lbl", _("Tab _6") );
            xset_set( "copy_tab_7", "lbl", _("Tab _7") );
            xset_set( "copy_tab_8", "lbl", _("Tab _8") );
            xset_set( "copy_tab_9", "lbl", _("Tab _9") );
            xset_set( "copy_tab_10", "lbl", _("Tab 1_0") );

        set = xset_set( "copy_panel", "lbl", C_("Edit|CopyTo|", "_Panel") );
        set->menu_style = XSET_MENU_SUBMENU;
        xset_set_set( set, "desc", "copy_panel_prev copy_panel_next copy_panel_1 copy_panel_2 copy_panel_3 copy_panel_4" );

            xset_set( "copy_panel_prev", "lbl", _("_Prev") );
            xset_set( "copy_panel_next", "lbl", _("_Next") );
            xset_set( "copy_panel_1", "lbl", _("Panel _1") );
            xset_set( "copy_panel_2", "lbl", _("Panel _2") );
            xset_set( "copy_panel_3", "lbl", _("Panel _3") );
            xset_set( "copy_panel_4", "lbl", _("Panel _4") );

    set = xset_get( "sep_c2" );
    set->menu_style = XSET_MENU_SEP;

    set = xset_set( "move_to", "lbl", _("_Move To") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "move_loc move_loc_last sep_c2 move_tab move_panel" );

        set = xset_set( "move_loc", "lbl", _("_Location") );
        set = xset_set( "move_loc_last", "lbl", _("L_ast Location") );
        xset_set_set( set, "icn", "gtk-redo" );
        set = xset_set( "move_tab", "lbl", C_("Edit|MoveTo|", "_Tab") );
        set->menu_style = XSET_MENU_SUBMENU;
        xset_set_set( set, "desc", "move_tab_prev move_tab_next move_tab_1 move_tab_2 move_tab_3 move_tab_4 move_tab_5 move_tab_6 move_tab_7 move_tab_8 move_tab_9 move_tab_10" );

            xset_set( "move_tab_prev", "lbl", _("_Prev") );
            xset_set( "move_tab_next", "lbl", _("_Next") );
            xset_set( "move_tab_1", "lbl", _("Tab _1") );
            xset_set( "move_tab_2", "lbl", _("Tab _2") );
            xset_set( "move_tab_3", "lbl", _("Tab _3") );
            xset_set( "move_tab_4", "lbl", _("Tab _4") );
            xset_set( "move_tab_5", "lbl", _("Tab _5") );
            xset_set( "move_tab_6", "lbl", _("Tab _6") );
            xset_set( "move_tab_7", "lbl", _("Tab _7") );
            xset_set( "move_tab_8", "lbl", _("Tab _8") );
            xset_set( "move_tab_9", "lbl", _("Tab _9") );
            xset_set( "move_tab_10", "lbl", _("Tab 1_0") );

        set = xset_set( "move_panel", "lbl", C_("Edit|MoveTo|", "_Panel") );
        set->menu_style = XSET_MENU_SUBMENU;
        xset_set_set( set, "desc", "move_panel_prev move_panel_next move_panel_1 move_panel_2 move_panel_3 move_panel_4" );

            xset_set( "move_panel_prev", "lbl", _("_Prev") );
            xset_set( "move_panel_next", "lbl", _("_Next") );
            xset_set( "move_panel_1", "lbl", _("Panel _1") );
            xset_set( "move_panel_2", "lbl", _("Panel _2") );
            xset_set( "move_panel_3", "lbl", _("Panel _3") );
            xset_set( "move_panel_4", "lbl", _("Panel _4") );

    set = xset_set( "edit_hide", "lbl", _("_Hide") );

    set = xset_set( "select_all", "lbl", _("_Select All") );
    xset_set_set( set, "icn", "gtk-select-all" );

    set = xset_set( "select_un", "lbl", _("_Unselect All") );

    set = xset_set( "select_invert", "lbl", _("_Invert Selection") );

    set = xset_set( "select_patt", "lbl", _("S_elect By Pattern") );

    set = xset_set( "edit_root", "lbl", _("R_oot") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "root_copy_loc root_move2 root_delete" );
    xset_set_set( set, "icn", "gtk-dialog-warning" );

        set = xset_set( "root_copy_loc", "lbl", _("_Copy To") );
        set = xset_set( "root_move2", "lbl", _("Move _To") );
        set = xset_set( "root_delete", "lbl", _("_Delete") );
        xset_set_set( set, "icn", "gtk-delete" );

    // Properties
    set = xset_set( "con_prop", "lbl", _("Propert_ies") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "" );
    xset_set_set( set, "icn", "gtk-properties" );

    set = xset_set( "prop_info", "lbl", _("_Info") );
    xset_set_set( set, "icn", "gtk-dialog-info" );

    set = xset_set( "prop_perm", "lbl", _("_Permissions") );
    xset_set_set( set, "icn", "GTK_STOCK_DIALOG_AUTHENTICATION" );

    set = xset_set( "prop_quick", "lbl", _("_Quick") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "perm_r perm_rw perm_rwx perm_r_r perm_rw_r perm_rw_rw perm_rwxr_x perm_rwxrwx perm_r_r_r perm_rw_r_r perm_rw_rw_rw perm_rwxr_r perm_rwxr_xr_x perm_rwxrwxrwx perm_rwxrwxrwt perm_unstick perm_stick perm_recurs" );

        xset_set( "perm_r", "lbl", "r--------" );
        xset_set( "perm_rw", "lbl", "rw-------" );
        xset_set( "perm_rwx", "lbl", "rwx------" );
        xset_set( "perm_r_r", "lbl", "r--r-----" );
        xset_set( "perm_rw_r", "lbl", "rw-r-----" );
        xset_set( "perm_rw_rw", "lbl", "rw-rw----" );
        xset_set( "perm_rwxr_x", "lbl", "rwxr-x---" );
        xset_set( "perm_rwxrwx", "lbl", "rwxrwx---" );
        xset_set( "perm_r_r_r", "lbl", "r--r--r--" );
        xset_set( "perm_rw_r_r", "lbl", "rw-r--r--" );
        xset_set( "perm_rw_rw_rw", "lbl", "rw-rw-rw-" );
        xset_set( "perm_rwxr_r", "lbl", "rwxr--r--" );
        xset_set( "perm_rwxr_xr_x", "lbl", "rwxr-xr-x" );
        xset_set( "perm_rwxrwxrwx", "lbl", "rwxrwxrwx" );
        xset_set( "perm_rwxrwxrwt", "lbl", "rwxrwxrwt" );
        xset_set( "perm_unstick", "lbl", "-t" );
        xset_set( "perm_stick", "lbl", "+t" );

        set = xset_set( "perm_recurs", "lbl", _("_Recursive") );
        set->menu_style = XSET_MENU_SUBMENU;
        xset_set_set( set, "desc", "perm_go_w perm_go_rwx perm_ugo_w perm_ugo_rx perm_ugo_rwx" );

        xset_set( "perm_go_w", "lbl", "go-w" );
        xset_set( "perm_go_rwx", "lbl", "go-rwx" );
        xset_set( "perm_ugo_w", "lbl", "ugo+w" );
        xset_set( "perm_ugo_rx", "lbl", "ugo+rX" );
        xset_set( "perm_ugo_rwx", "lbl", "ugo+rwX" );

    set = xset_set( "prop_root", "lbl", _("_Root") );
    set->menu_style = XSET_MENU_SUBMENU;
    xset_set_set( set, "desc", "rperm_rw rperm_rwx rperm_rw_r rperm_rw_rw rperm_rwxr_x rperm_rwxrwx rperm_rw_r_r rperm_rw_rw_rw rperm_rwxr_r rperm_rwxr_xr_x rperm_rwxrwxrwx rperm_rwxrwxrwt rperm_unstick rperm_stick rperm_recurs rperm_own" );
    xset_set_set( set, "icn", "gtk-dialog-warning" );

        xset_set( "rperm_rw", "lbl", "rw-------" );
        xset_set( "rperm_rwx", "lbl", "rwx------" );
        xset_set( "rperm_rw_r", "lbl", "rw-r-----" );
        xset_set( "rperm_rw_rw", "lbl", "rw-rw----" );
        xset_set( "rperm_rwxr_x", "lbl", "rwxr-x---" );
        xset_set( "rperm_rwxrwx", "lbl", "rwxrwx---" );
        xset_set( "rperm_rw_r_r", "lbl", "rw-r--r--" );
        xset_set( "rperm_rw_rw_rw", "lbl", "rw-rw-rw-" );
        xset_set( "rperm_rwxr_r", "lbl", "rwxr--r--" );
        xset_set( "rperm_rwxr_xr_x", "lbl", "rwxr-xr-x" );
        xset_set( "rperm_rwxrwxrwx", "lbl", "rwxrwxrwx" );
        xset_set( "rperm_rwxrwxrwt", "lbl", "rwxrwxrwt" );
        xset_set( "rperm_unstick", "lbl", "-t" );
        xset_set( "rperm_stick", "lbl", "+t" );

        set = xset_set( "rperm_recurs", "lbl", _("_Recursive") );
        set->menu_style = XSET_MENU_SUBMENU;
        xset_set_set( set, "desc", "rperm_go_w rperm_go_rwx rperm_ugo_w rperm_ugo_rx rperm_ugo_rwx" );

        xset_set( "rperm_go_w", "lbl", "go-w" );
        xset_set( "rperm_go_rwx", "lbl", "go-rwx" );
        xset_set( "rperm_ugo_w", "lbl", "ugo+w" );
        xset_set( "rperm_ugo_rx", "lbl", "ugo+rX" );
        xset_set( "rperm_ugo_rwx", "lbl", "ugo+rwX" );

        set = xset_set( "rperm_own", "lbl", _("_Owner") );
        set->menu_style = XSET_MENU_SUBMENU;
        xset_set_set( set, "desc", "own_myuser own_myuser_users own_user1 own_user1_users own_user2 own_user2_users own_root own_root_users own_root_myuser own_root_user1 own_root_user2 own_recurs" );

        xset_set( "own_myuser", "lbl", "myuser" );
        xset_set( "own_myuser_users", "lbl", "myuser:users" );
        xset_set( "own_user1", "lbl", "user1" );
        xset_set( "own_user1_users", "lbl", "user1:users" );
        xset_set( "own_user2", "lbl", "user2" );
        xset_set( "own_user2_users", "lbl", "user2:users" );
        xset_set( "own_root", "lbl", "root" );
        xset_set( "own_root_users", "lbl", "root:users" );
        xset_set( "own_root_myuser", "lbl", "root:myuser" );
        xset_set( "own_root_user1", "lbl", "root:user1" );
        xset_set( "own_root_user2", "lbl", "root:user2" );

        set = xset_set( "own_recurs", "lbl", _("_Recursive") );
        set->menu_style = XSET_MENU_SUBMENU;
        xset_set_set( set, "desc", "rown_myuser rown_myuser_users rown_user1 rown_user1_users rown_user2 rown_user2_users rown_root rown_root_users rown_root_myuser rown_root_user1 rown_root_user2" );

        xset_set( "rown_myuser", "lbl", "myuser" );
        xset_set( "rown_myuser_users", "lbl", "myuser:users" );
        xset_set( "rown_user1", "lbl", "user1" );
        xset_set( "rown_user1_users", "lbl", "user1:users" );
        xset_set( "rown_user2", "lbl", "user2" );
        xset_set( "rown_user2_users", "lbl", "user2:users" );
        xset_set( "rown_root", "lbl", "root" );
        xset_set( "rown_root_users", "lbl", "root:users" );
        xset_set( "rown_root_myuser", "lbl", "root:myuser" );
        xset_set( "rown_root_user1", "lbl", "root:user1" );
        xset_set( "rown_root_user2", "lbl", "root:user2" );



    // PANELS
    int p;
    for ( p = 1; p < 5; p++ )
    {
        set = xset_set_panel( p, "show_toolbox", "lbl", _("_Toolbar") );
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_TRUE;

        set = xset_set_panel( p, "show_devmon", "lbl", _("_Devices") );
        set->menu_style = XSET_MENU_CHECK;

        set = xset_set_panel( p, "show_dirtree", "lbl", _("T_ree") );
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_TRUE;

        set = xset_set_panel( p, "show_book", "lbl", _("_Bookmarks") );
        set->menu_style = XSET_MENU_CHECK;

        set = xset_set_panel( p, "show_sidebar", "lbl", _("_Side Toolbar") );
        set->menu_style = XSET_MENU_CHECK;

        set = xset_set_panel( p, "list_detailed", "lbl", _("_Detailed") );
        set->menu_style = XSET_MENU_RADIO;

        set = xset_set_panel( p, "list_icons", "lbl", _("_Icons") );
        set->menu_style = XSET_MENU_RADIO;

        set = xset_set_panel( p, "list_compact", "lbl", _("_Compact") );
        set->menu_style = XSET_MENU_RADIO;
        set->b = XSET_B_TRUE;

        set = xset_set_panel( p, "show_hidden", "lbl", _("_Hidden Files") );
        set->menu_style = XSET_MENU_CHECK;

        set = xset_set_panel( p, "font_file", "lbl", _("_Font") );
        set->menu_style = XSET_MENU_FONTDLG;
        xset_set_set( set, "icn", "gtk-select-font" );
        set->title = g_strdup_printf( _("File List Font (Panel %d)"), p );
        xset_set_set( set, "desc", _("Example  1.1 M  file  -rwxr--r--  user:group  2011-01-01 01:11") );

        set = xset_set_panel( p, "font_dev", "lbl", _("_Font") );
        set->menu_style = XSET_MENU_FONTDLG;
        xset_set_set( set, "icn", "gtk-select-font" );
        set->title = g_strdup_printf( _("Devices Font (Panel %d)"), p );
        xset_set_set( set, "desc", _("sr0 [no media] :EXAMPLE") );
        set->line = g_strdup( "#devices-settings-font" );

        set = xset_set_panel( p, "font_book", "lbl", _("_Font") );
        set->menu_style = XSET_MENU_FONTDLG;
        xset_set_set( set, "icn", "gtk-select-font" );
        set->title = g_strdup_printf( _("Bookmarks Font (Panel %d)"), p );
        xset_set_set( set, "desc", _("Example Bookmark Name") );

        set = xset_set_panel( p, "font_path", "lbl", _("_Font") );
        set->menu_style = XSET_MENU_FONTDLG;
        xset_set_set( set, "icn", "gtk-select-font" );
        set->title = g_strdup_printf( _("Path Bar Font (Panel %d)"), p );
        xset_set_set( set, "desc", _("$ cat /home/user/example") );
        set->line = g_strdup( "#gui-pathbar-font" );

        set = xset_set_panel( p, "font_tab", "lbl", _("_Font") );
        set->menu_style = XSET_MENU_FONTDLG;
        xset_set_set( set, "icn", "gtk-select-font" );
        set->title = g_strdup_printf( _("Tab Font (Panel %d)"), p );
        xset_set_set( set, "desc", "/usr/bin" );

        set = xset_set_panel( p, "icon_tab", "lbl", _("_Icon") );
        set->menu_style = XSET_MENU_ICON;
        xset_set_set( set, "icn", "gtk-directory" );

        set = xset_set_panel( p, "font_status", "lbl", _("_Font") );
        set->menu_style = XSET_MENU_FONTDLG;
        xset_set_set( set, "icn", "gtk-select-font" );
        set->title = g_strdup_printf( _("Status Bar Font (Panel %d)"), p );
        xset_set_set( set, "desc", _("12 G free / 200 G   52 items") );

        set = xset_set_panel( p, "icon_status", "lbl", _("_Icon") );
        set->menu_style = XSET_MENU_ICON;
        xset_set_set( set, "icn", "gtk-yes" );
        
        set = xset_set_panel( p, "detcol_name", "lbl", _("_Name") );
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_TRUE;                   // visible
        set->x = g_strdup_printf( "%d", 0 );    // position
        xset_set_b_panel_mode( p, "detcol_name", 1, TRUE );  // horiz context
        
        set = xset_set_panel( p, "detcol_size", "lbl", _("_Size") );
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_TRUE;
        set->x = g_strdup_printf( "%d", 1 );
        xset_set_b_panel_mode( p, "detcol_size", 1, TRUE );  // horiz context

        set = xset_set_panel( p, "detcol_type", "lbl", _("_Type") );
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_TRUE;
        set->x = g_strdup_printf( "%d", 2 );

        set = xset_set_panel( p, "detcol_perm", "lbl", _("_Permission") );
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_TRUE;
        set->x = g_strdup_printf( "%d", 3 );

        set = xset_set_panel( p, "detcol_owner", "lbl", _("_Owner") );
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_TRUE;
        set->x = g_strdup_printf( "%d", 4 );

        set = xset_set_panel( p, "detcol_date", "lbl", _("_Modified") );
        set->menu_style = XSET_MENU_CHECK;
        set->b = XSET_B_TRUE;
        set->x = g_strdup_printf( "%d", 5 );

        set = xset_get_panel( p, "sort_extra" );
        set->b = XSET_B_TRUE;  //sort_natural
        set->x = g_strdup_printf( "%d", XSET_B_FALSE );  // sort_case
        set->y = g_strdup( "1" ); //PTK_LIST_SORT_DIR_FIRST from ptk-file-list.h
        set->z = g_strdup_printf( "%d", XSET_B_TRUE );  // sort_hidden_first
    }
    
    //speed
    set = xset_set( "book_newtab", "lbl", _("_New Tab") );
    set->menu_style = XSET_MENU_CHECK;

    set = xset_set( "book_single", "lbl", _("_Single Click") );
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;

    set = xset_set( "dev_newtab", "lbl", _("_New Tab") );
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;
    set->line = g_strdup( "#devices-settings-newtab" );

    set = xset_set( "dev_single", "lbl", _("_Single Click") );
    set->menu_style = XSET_MENU_CHECK;
    set->b = XSET_B_TRUE;
    set->line = g_strdup( "#devices-settings-single" );
    
    // mark all labels and icons as default
    GList* l;
    for ( l = xsets; l; l = l->next )
    {
        if ( ((XSet*)l->data)->lock )
        {
            if ( ((XSet*)l->data)->in_terminal == XSET_B_TRUE )
                ((XSet*)l->data)->in_terminal = XSET_B_UNSET;
            if ( ((XSet*)l->data)->keep_terminal == XSET_B_TRUE )
                ((XSet*)l->data)->keep_terminal = XSET_B_UNSET;

        }
    }
}

void def_key( char* name, int key, int keymod )
{
    XSet* set = xset_get( name );
    
    // key already set or unset?
    if ( set->key != 0 || key == 0 )
        return;
        
    // key combo already in use?
    GList* l;
    for ( l = keysets; l; l = l->next )
    {
        if ( ((XSet*)l->data)->key == key && ((XSet*)l->data)->keymod == keymod )
            return;
    }
    set->key = key;
    set->keymod = keymod;
}

void xset_default_keys()
{
    XSet* set;
    GList* l;

    // read all currently set or unset keys
    keysets = NULL;
    for ( l = xsets; l; l = l->next )
    {
        if ( ((XSet*)l->data)->key )
            keysets = g_list_prepend( keysets, (XSet*)l->data );
    }

    def_key( "tab_prev", 65056, 5 );        // ctrl-tab  or use ctrl-pgdn??
    def_key( "tab_next", 65289, 4 );
    def_key( "tab_close", 119, 4 );
    def_key( "tab_new", 116, 4 );
    def_key( "tab_1", 0x31, 8 );            // Alt-1
    def_key( "tab_2", 0x32, 8 );
    def_key( "tab_3", 0x33, 8 );
    def_key( "tab_4", 0x34, 8 );
    def_key( "tab_5", 0x35, 8 );
    def_key( "tab_6", 0x36, 8 );
    def_key( "tab_7", 0x37, 8 );
    def_key( "tab_8", 0x38, 8 );
    def_key( "tab_9", 0x39, 8 );            // Alt-9
    def_key( "tab_10", 0x30, 8 );           // Alt-0
    def_key( "edit_cut", 120, 4 );
    def_key( "edit_copy", 99, 4 );
    def_key( "edit_paste", 118, 4 );
    def_key( "edit_rename", 65471, 0 );
    def_key( "edit_delete", 65535, 0 );
    def_key( "copy_name", 67, 9 );
    def_key( "copy_path", 67, 5 );
    def_key( "paste_link", 86, 5 );
    def_key( "paste_as", 65, 5 );
    def_key( "select_all", 97, 4 );
    def_key( "main_terminal", 65473, 0 );   //F4
    def_key( "go_default", 65307, 0 );
    def_key( "go_back", 65361, 8 );
    def_key( "go_forward", 65363, 8 );
    def_key( "go_up", 65362, 8 );
    def_key( "focus_path_bar", 0x6c, 4 );   // Ctrl-L
    def_key( "view_refresh", 65474, 0 );
    def_key( "prop_info", 0xff0d, 8 );
    def_key( "prop_perm", 112, 4 );
    def_key( "panel1_show_hidden", 104, 4 );
    def_key( "book_new", 100, 4 );
    def_key( "new_folder", 102, 4 );
    def_key( "new_file", 70, 5 );
    def_key( "main_new_window", 110, 4 );
    def_key( "open_all", 65475, 0 );        //F6
    def_key( "main_full", 0xffc8, 0 );      //F11
    def_key( "panel1_show", 0x31, 4 );
    def_key( "panel2_show", 0x32, 4 );
    def_key( "panel3_show", 0x33, 4 );
    def_key( "panel4_show", 0x34, 4 );
    def_key( "main_help", 0xffbe, 0 );      //F1
    def_key( "main_exit", 0x71, 4 );        // Ctrl-Q

    if ( keysets )
        g_list_free( keysets );
}

