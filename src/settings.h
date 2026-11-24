/*
 * SpaceFM settings.h
 * 
 * Copyright (C) 2015 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>
 * 
 * License: See COPYING file
 * 
*/

#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include "ptk-file-browser.h"
#include "desktop-window.h"

// this determines time before item is selected by hover in single-click mode
#define SINGLE_CLICK_TIMEOUT 150

// This limits the small icon size for side panes and task list
#define PANE_MAX_ICON_SIZE 48

typedef enum {
    WPM_STRETCH,
    WPM_FULL,
    WPM_CENTER,
    WPM_TILE,
    WPM_ZOOM,
    WPM_TRANSPARENT
}WallpaperMode;

typedef struct
{
    /* General Settings */
    char encoding[ 32 ];
    //gboolean show_hidden_files;
    //gboolean show_side_pane;
    //int side_pane_mode;
    gboolean show_thumbnail;
    int max_thumb_size;

    int big_icon_size;
    int small_icon_size;
    int tool_icon_size;

    gboolean use_trash_can;
    gboolean single_click;
    gboolean no_single_hover;

    /* char* iconTheme; */
    //char* terminal;

    //gboolean show_location_bar;

    gboolean no_execute;    //MOD
    gboolean no_confirm;    //MOD
    gboolean sdebug;            //sfm
    gboolean load_saved_tabs;   //sfm
    char* date_format;  //MOD for speed dupe of xset
    
    //int open_bookmark_method; /* 1: current tab, 2: new tab, 3: new window */
    //int view_mode; /* icon view or detailed list view */
    int sort_order; /* Sort by name, size, time */
    int sort_type; /* ascending, descending */

    /* Window State */
    //int splitter_pos;
    int width;
    int height;
    gboolean maximized;

    /* Desktop */
    //gboolean show_desktop;
    gboolean show_wallpaper;
    char* wallpaper;
    WallpaperMode wallpaper_mode;
    int desktop_sort_by;
    int desktop_sort_type;
    gboolean show_wm_menu;
    gboolean desk_single_click;
    gboolean desk_no_single_hover;
    gboolean desk_open_mime;
    PangoFontDescription* desk_font;
    GdkColor desktop_bg1;
    GdkColor desktop_bg2;
    GdkColor desktop_text;
    GdkColor desktop_shadow;
    int margin_top;
    int margin_left;
    int margin_right;
    int margin_bottom;
    int margin_pad;
    
    /* Interface */
    gboolean always_show_tabs;
    gboolean hide_close_tab_buttons;
    //gboolean hide_side_pane_buttons;
    //gboolean hide_folder_content_border;

    /* Units */
    gboolean use_si_prefix;
}
AppSettings;

extern AppSettings app_settings;

void load_conf();
void load_settings( char* config_dir );
char* save_settings( gpointer main_window_ptr );
void free_settings();
const char* xset_get_config_dir();
const char* xset_get_tmp_dir();
const char* xset_get_shared_tmp_dir();
const char* xset_get_user_tmp_dir();


///////////////////////////////////////////////////////////////////////////////
//MOD extra settings below

extern GList* xsets;

enum {
    XSET_B_UNSET,
    XSET_B_TRUE,
    XSET_B_FALSE
};

enum {
    XSET_CMD_LINE,
    XSET_CMD_SCRIPT,
    XSET_CMD_APP,
    XSET_CMD_BOOKMARK
};

enum {   // do not renumber - these values are saved in session files
    XSET_MENU_NORMAL,
    XSET_MENU_CHECK,
    XSET_MENU_STRING,
    XSET_MENU_RADIO,
    XSET_MENU_FILEDLG,
    XSET_MENU_FONTDLG,
    XSET_MENU_ICON,
    XSET_MENU_COLORDLG,
    XSET_MENU_CONFIRM,
    XSET_MENU_DUMMY3,
    XSET_MENU_DUMMY4,
    XSET_MENU_DUMMY5,
    XSET_MENU_DUMMY6,
    XSET_MENU_DUMMY7,
    XSET_MENU_DUMMY8,
    XSET_MENU_DUMMY9,
    XSET_MENU_DUMMY10,
    XSET_MENU_SUBMENU,  // add new before submenu
    XSET_MENU_SEP
};

enum {   // do not reorder - these values are saved in session files
    // also update builtin_tool_name builtin_tool_icon in settings.c
    XSET_TOOL_NOT,
    XSET_TOOL_CUSTOM,
    XSET_TOOL_DEVICES,
    XSET_TOOL_BOOKMARKS,
    XSET_TOOL_TREE,
    XSET_TOOL_HOME,
    XSET_TOOL_DEFAULT,
    XSET_TOOL_UP,
    XSET_TOOL_BACK,
    XSET_TOOL_BACK_MENU,
    XSET_TOOL_FWD,
    XSET_TOOL_FWD_MENU,
    XSET_TOOL_REFRESH,
    XSET_TOOL_NEW_TAB,
    XSET_TOOL_NEW_TAB_HERE,
    XSET_TOOL_SHOW_HIDDEN,
    XSET_TOOL_SHOW_THUMB,
    XSET_TOOL_LARGE_ICONS,
    XSET_TOOL_INVALID      // keep this always last
};

enum {
    XSET_JOB_KEY,
    XSET_JOB_ICON,
    XSET_JOB_LABEL,
    XSET_JOB_EDIT,
    XSET_JOB_EDIT_ROOT,
    XSET_JOB_LINE,
    XSET_JOB_SCRIPT,
    XSET_JOB_CUSTOM,
    XSET_JOB_TERM,
    XSET_JOB_KEEP,
    XSET_JOB_USER,
    XSET_JOB_TASK,
    XSET_JOB_POP,
    XSET_JOB_ERR,
    XSET_JOB_OUT,
    XSET_JOB_BOOKMARK,
    XSET_JOB_APP,
    XSET_JOB_COMMAND,
    XSET_JOB_SUBMENU,
    XSET_JOB_SUBMENU_BOOK,
    XSET_JOB_SEP,
    XSET_JOB_ADD_TOOL,
    XSET_JOB_IMPORT_FILE,
    XSET_JOB_IMPORT_URL,
    XSET_JOB_IMPORT_GTK,
    XSET_JOB_CUT,
    XSET_JOB_COPY,
    XSET_JOB_PASTE,
    XSET_JOB_REMOVE,
    XSET_JOB_REMOVE_BOOK,
    XSET_JOB_NORMAL,
    XSET_JOB_CHECK,
    XSET_JOB_CONFIRM,
    XSET_JOB_DIALOG,
    XSET_JOB_MESSAGE,
    XSET_JOB_COPYNAME,
    XSET_JOB_PROP,
    XSET_JOB_PROP_CMD,
    XSET_JOB_IGNORE_CONTEXT,
    XSET_JOB_SCROLL,
    XSET_JOB_EXPORT,
    XSET_JOB_BROWSE_FILES,
    XSET_JOB_BROWSE_DATA,
    XSET_JOB_BROWSE_PLUGIN,
    XSET_JOB_HELP,
    XSET_JOB_HELP_NEW,
    XSET_JOB_HELP_ADD,
    XSET_JOB_HELP_BROWSE,
    XSET_JOB_HELP_STYLE,
    XSET_JOB_HELP_BOOK,
    XSET_JOB_TOOLTIPS
};

enum {
    PLUGIN_JOB_INSTALL,
    PLUGIN_JOB_COPY,
    PLUGIN_JOB_REMOVE
};

enum {
    PLUGIN_USE_HAND_ARC,
    PLUGIN_USE_HAND_FS,
    PLUGIN_USE_HAND_NET,
    PLUGIN_USE_HAND_FILE,
    PLUGIN_USE_BOOKMARKS,
    PLUGIN_USE_NORMAL
};

typedef struct
{
    char* name;
    char b;                 // tri-state 0=unset(false) 1=true 2=false
    char* s;
    char* x;
    char* y;
    char* z;                // for menu_string locked, stores default
    gboolean disable;       // not saved, default false
    char* menu_label;
    int menu_style;         // not saved or read if locked
    char* icon;
    void (*cb_func) ();     // not saved
    gpointer cb_data;       // not saved
    char* ob1;              // not saved
    gpointer ob1_data;      // not saved
    char* ob2;              // not saved
    gpointer ob2_data;      // not saved
    PtkFileBrowser* browser;// not saved - set automatically
    DesktopWindow* desktop; // not saved - set automatically
    int key;
    int keymod;
    char* shared_key;       // not saved
    char* desc;             // not saved or read if locked
    char* title;            // not saved or read if locked
    char* next;
    char* context;
    char tool;              // 0=not 1=true 2=false
    gboolean lock;          // not saved, default true
    
    // Custom Command ( !lock )
    char* prev;
    char* parent;
    char* child;
    char* line;             // or help if lock
    // x = XSET_CMD_LINE..XSET_CMD_BOOKMARK
    // y = user
    // z = custom executable
    char task;
    char task_pop;
    char task_err;
    char task_out;
    char in_terminal;       // or save menu_label if lock
    char keep_terminal;     // or save icon if lock
    char scroll_lock;
    char opener;

    // Plugin (not saved at all)
    gboolean plugin;
    gboolean plugin_top;
    char* plug_name;
    char* plug_dir;
    
} XSet;

typedef struct
{
    GtkMenuItem* item;
    char* name;
} XMenuItem;

// cache these for speed in event handlers
extern XSet* evt_win_focus;
extern XSet* evt_win_move;
extern XSet* evt_win_click;
extern XSet* evt_win_key;
extern XSet* evt_win_close;
extern XSet* evt_pnl_show;
extern XSet* evt_pnl_focus;
extern XSet* evt_pnl_sel;
extern XSet* evt_tab_new;
extern XSet* evt_tab_chdir;
extern XSet* evt_tab_focus;
extern XSet* evt_tab_close;
extern XSet* evt_device;

// instance-wide command history
extern GList* xset_cmd_history;

static const char* terminal_programs[] =  //for pref-dialog.c
{
    "roxterm",
    "terminal",
    "xfce4-terminal",
    "gnome-terminal",
    "aterm",
    "Eterm",
    "konsole",
    "lxterminal",
    "mlterm",
    "mrxvt",
    "rxvt",
    "sakura",
    "terminator",
    "urxvt",
    "xterm",
    "x-terminal-emulator",
    "lilyterm",
    "qterminal"
};

static const char* su_commands[] = // order and contents must match prefdlg.ui
{
    "/bin/su",
    "/usr/bin/sudo",
    "/usr/bin/su-to-root"
};

static const char* gsu_commands[] = // order and contents must match prefdlg.ui
{
    "/usr/bin/gksu",
    "/usr/bin/gksudo",
    "/usr/bin/gnomesu",
    "/usr/bin/xdg-su",
    "/usr/bin/kdesu",   // may be translated to "$(kde4-config --path libexec)/kdesu"
    "/usr/bin/kdesudo",
    "/usr/bin/ktsuss",
    "/usr/bin/lxqt-sudo",
    "/usr/bin/lxsu",
    "/usr/bin/su-to-root",
    "/bin/su",
    "/usr/bin/sudo"
};

// These will contain the su and gsu settings from /etc/spacefm/spacefm.conf
extern char* settings_terminal_su;
extern char* settings_graphical_su;

typedef struct
{
    gboolean valid;
    char* var[40];
} XSetContext;


char* randhex8();
char* replace_string( const char* orig, const char* str, const char* replace,
                                                            gboolean quote );
char* replace_line_subs( const char* line );
char* bash_quote( const char* str );
void string_copy_free( char** s, const char* src );
gboolean is_alphanum( char* str );
char* get_name_extension( char* full_name, gboolean is_dir, char** ext );
char* unescape( const char* t );
void xset_autosave( gboolean force, gboolean delay );
void xset_autosave_cancel();

void open_in_prog( const char* path );
void xset_set_window_icon( GtkWindow* win );
char* get_valid_su();
char* get_valid_gsu();
gboolean xset_copy_file( char* src, char* dest );
gboolean dir_has_files( const char* path );
XSet* xset_get( const char* name );
char* xset_get_s( const char* name );
gboolean xset_get_bool( const char* name, const char* var );
gboolean xset_get_b( const char* name );
XSet* xset_get_panel( int panel, const char* name );
char* xset_get_s_panel( int panel, const char* name );
gboolean xset_get_b_panel( int panel, const char* name );
gboolean xset_get_bool_panel( int panel, const char* name, const char* var );
XSet* xset_set_b( const char* name, gboolean bval );
XSet* xset_set_b_panel( int panel, const char* name, gboolean bval );
int xset_get_int( const char* name, const char* var );
int xset_get_int_panel( int panel, const char* name, const char* var );
XSet* xset_set_panel( int panel, const char* name, const char* var, const char* value );
XSet* xset_set_cb_panel( int panel, const char* name, void (*cb_func) (), gpointer cb_data );
gboolean xset_get_b_set( XSet* set );
XSet* xset_get_panel_mode( int panel, const char* name, char mode );
gboolean xset_get_b_panel_mode( int panel, const char* name, char mode );
XSet* xset_set_panel_mode( int panel, const char* name, char mode,
                                      const char* var, const char* value );
XSet* xset_set_b_panel_mode( int panel, const char* name, char mode,
                                                            gboolean bval );

XSetContext* xset_context_new();
XSet* xset_get_plugin_mirror( XSet* set );
void write_src_functions( FILE* file );
char* xset_custom_get_script( XSet* set, gboolean create );
gboolean have_rw_access( const char* path );
gboolean have_x_access( const char* path );
char* xset_get_keyname( XSet* set, int key_val, int key_mod );
void xset_set_key( GtkWidget* parent, XSet* set );

XSet* xset_set( const char* name, const char* var, const char* value );
XSet* xset_set_set( XSet* set, const char* var, const char* value );
void xset_custom_delete( XSet* set, gboolean delete_next );
void xset_custom_activate( GtkWidget* item, XSet* set );
XSet* xset_custom_remove( XSet* set );
char* xset_custom_get_app_name_icon( XSet* set, GdkPixbuf** icon, int icon_size );
GdkPixbuf* xset_custom_get_bookmark_icon( XSet* set, int icon_size );
void xset_custom_export( GtkWidget* parent, PtkFileBrowser* file_browser,
                                                                    XSet* set );
GtkWidget* xset_design_show_menu( GtkWidget* menu, XSet* set, XSet* book_insert,
                                  guint button, guint32 time );
void xset_add_menu( DesktopWindow* desktop, PtkFileBrowser* file_browser,
                    GtkWidget* menu, GtkAccelGroup *accel_group, char* elements );
GtkWidget* xset_add_menuitem( DesktopWindow* desktop, PtkFileBrowser* file_browser,
                                    GtkWidget* menu, GtkAccelGroup *accel_group,
                                    XSet* set );
GtkWidget* xset_get_image( const char* icon, int icon_size );
XSet* xset_set_cb( const char* name, void (*cb_func) (), gpointer cb_data );
XSet* xset_set_ob1_int( XSet* set, const char* ob1, int ob1_int );
XSet* xset_set_ob1( XSet* set, const char* ob1, gpointer ob1_data );
XSet* xset_set_ob2( XSet* set, const char* ob2, gpointer ob2_data );
XSet* xset_is( const char* name );
XSet* xset_find_custom( const char* search );

void xset_menu_cb( GtkWidget* item, XSet* set );
gboolean xset_menu_keypress( GtkWidget* widget, GdkEventKey* event,
                                                            gpointer user_data );
gboolean xset_text_dialog( GtkWidget* parent, const char* title, GtkWidget* image,
                            gboolean large, const char* msg1, const char* msg2,
                            const char* defstring, char** answer, const char* defreset,
                            gboolean edit_care, const char* help );
char* xset_file_dialog( GtkWidget* parent, GtkFileChooserAction action,
                        const char* title, const char* deffolder, const char* deffile );
char* xset_font_dialog( GtkWidget* parent, const char* title,
                                    const char* preview, const char* deffont );
void xset_edit( GtkWidget* parent, const char* path, gboolean force_root, gboolean no_root );
void xset_open_url( GtkWidget* parent, const char* url );
void xset_fill_toolbar( GtkWidget* parent, PtkFileBrowser* file_browser,
                        GtkWidget* toolbar, XSet* set_parent,
                        gboolean show_tooltips );
int xset_msg_dialog( GtkWidget* parent, int action, const char* title, GtkWidget* image,
                    int buttons, const char* msg1, const char* msg2, const char* help );
GtkTextView* multi_input_new( GtkScrolledWindow* scrolled, const char* text,
                                                            gboolean def_font );
void multi_input_select_region( GtkWidget* input, int start, int end );
char* multi_input_get_text( GtkWidget* input );
XSet* xset_custom_new();
gboolean write_root_settings( FILE* file, const char* path );
GList* xset_get_plugins( gboolean included );
void install_plugin_file( gpointer main_win, GtkWidget* handler_dlg,
                          const char* path, const char* plug_dir, int type,
                          int job, XSet* insert_set );
XSet* xset_import_plugin( const char* plug_dir, gboolean* is_bookmarks );
void clean_plugin_mirrors();
char* plain_ascii_name( const char* orig_name );
char* clean_label( const char* menu_label, gboolean kill_special, gboolean convert_amp );
void xset_show_help( GtkWidget* parent, XSet* set, const char* anchor );
gboolean xset_opener( DesktopWindow* desktop, PtkFileBrowser* file_browser,
                                                            char job );
const char* xset_get_builtin_toolitem_label( char tool_type );
char* xset_icon_chooser_dialog( GtkWindow* parent, const char* def_icon );


#endif

