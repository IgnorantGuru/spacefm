/*
*  C Implementation: ptk-file-menu
*
* Description:
*
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <unistd.h> /* for access */

#include "ptk-file-menu.h"
#include <glib.h>
#include "glib-mem.h"
#include <string.h>
#include <stdlib.h>

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "vfs-app-desktop.h"
#include "ptk-utils.h"
#include "ptk-file-misc.h"
#include "ptk-file-archiver.h"
#include "ptk-clipboard.h"
#include "ptk-app-chooser.h"
#include "settings.h"  //MOD
#include "item-prop.h"  //MOD
#include "main-window.h"
#include "ptk-location-view.h"
#include "ptk-file-list.h"  //sfm for sort extra
#include "pref-dialog.h"
//#include "ptk-bookmarks.h"

#include "gtk2-compat.h"

//#define get_toplevel_win(data)  ( (GtkWindow*) (data->browser ? ( gtk_widget_get_toplevel((GtkWidget*) data->browser) ) : NULL) )

gboolean on_app_button_press( GtkWidget* item, GdkEventButton* event,
                                                            PtkFileMenu* data );
gboolean app_menu_keypress( GtkWidget* widget, GdkEventKey* event,
                                                            PtkFileMenu* data );
static void show_app_menu( GtkWidget* menu, GtkWidget* app_item, PtkFileMenu* data, 
                                                    guint button, guint32 time );

/* Signal handlers for popup menu */
static void
on_popup_open_activate ( GtkMenuItem *menuitem,
                         PtkFileMenu* data );
static void
on_popup_open_with_another_activate ( GtkMenuItem *menuitem,
                                      PtkFileMenu* data );
#if 0
static void
on_file_properties_activate ( GtkMenuItem *menuitem,
                              PtkFileMenu* data );
#endif
static void
on_popup_run_app ( GtkMenuItem *menuitem,
                   PtkFileMenu* data );
static void
on_popup_open_in_new_tab_activate ( GtkMenuItem *menuitem,
                                    PtkFileMenu* data );
static void
on_popup_open_in_new_win_activate ( GtkMenuItem *menuitem,
                                    PtkFileMenu* data );
static void on_popup_open_in_terminal_activate( GtkMenuItem *menuitem,
                                                PtkFileMenu* data );
static void
on_popup_cut_activate ( GtkMenuItem *menuitem,
                        PtkFileMenu* data );
static void
on_popup_copy_activate ( GtkMenuItem *menuitem,
                         PtkFileMenu* data );
static void
on_popup_paste_activate ( GtkMenuItem *menuitem,
                          PtkFileMenu* data );
static void
on_popup_paste_link_activate ( GtkMenuItem *menuitem,
                          PtkFileMenu* data );   //MOD added
static void
on_popup_paste_target_activate ( GtkMenuItem *menuitem,
                          PtkFileMenu* data );   //MOD added
static void
on_popup_copy_text_activate ( GtkMenuItem *menuitem,
                          PtkFileMenu* data );   //MOD added
static void
on_popup_copy_name_activate ( GtkMenuItem *menuitem,
                          PtkFileMenu* data );   //MOD added
void
on_popup_copy_parent_activate ( GtkMenuItem *menuitem,
                          PtkFileMenu* data );   //MOD added

void
on_popup_paste_as_activate ( GtkMenuItem *menuitem,
                          PtkFileMenu* data );   //sfm added

static void
on_popup_delete_activate ( GtkMenuItem *menuitem,
                           PtkFileMenu* data );
static void
on_popup_rename_activate ( GtkMenuItem *menuitem,
                           PtkFileMenu* data );
static void
on_popup_compress_activate ( GtkMenuItem *menuitem,
                             PtkFileMenu* data );
static void
on_popup_extract_here_activate ( GtkMenuItem *menuitem,
                                 PtkFileMenu* data );
static void
on_popup_extract_to_activate ( GtkMenuItem *menuitem,
                               PtkFileMenu* data );
void on_popup_extract_list_activate ( GtkMenuItem *menuitem,
                                      PtkFileMenu* data );
static void
on_popup_new_folder_activate ( GtkMenuItem *menuitem,
                               PtkFileMenu* data );
static void
on_popup_new_text_file_activate ( GtkMenuItem *menuitem,
                                  PtkFileMenu* data );
static void
on_popup_new_link_activate ( GtkMenuItem *menuitem,
                                  PtkFileMenu* data );

static void
on_popup_file_properties_activate ( GtkMenuItem *menuitem,
                                    PtkFileMenu* data );

void on_popup_file_permissions_activate ( GtkMenuItem *menuitem,
                                    PtkFileMenu* data );

static void on_popup_open_files_activate( GtkMenuItem *menuitem,
                               PtkFileMenu* data );  //MOD
static void on_popup_open_all( GtkMenuItem *menuitem, PtkFileMenu* data );

void
on_popup_canon ( GtkMenuItem *menuitem, PtkFileMenu* data );

/*
static void on_popup_run_command( GtkMenuItem *menuitem,
                               PtkFileMenu* data );  //MOD
static void on_popup_user_6 ( GtkMenuItem *menuitem,
                                        PtkFileMenu* data );  //MOD
static void on_popup_user_7 ( GtkMenuItem *menuitem,
                                        PtkFileMenu* data );  //MOD
static void on_popup_user_8 ( GtkMenuItem *menuitem,
                                        PtkFileMenu* data );  //MOD
static void on_popup_user_9 ( GtkMenuItem *menuitem,
                                        PtkFileMenu* data );  //MOD
*/
/*
static PtkMenuItemEntry create_new_menu[] =
    {
        PTK_IMG_MENU_ITEM( N_( "_Folder" ), "gtk-directory", on_popup_new_folder_activate, GDK_f, GDK_CONTROL_MASK ),  //MOD stole ctrl-f
        PTK_IMG_MENU_ITEM( N_( "_Text File" ), "gtk-edit", on_popup_new_text_file_activate, GDK_f, GDK_CONTROL_MASK | GDK_SHIFT_MASK ),  //MOD added ctrl-shift-f
        PTK_MENU_END
    };

static PtkMenuItemEntry extract_menu[] =
    {
        PTK_MENU_ITEM( N_( "E_xtract Here" ), on_popup_extract_here_activate, 0, 0 ),
        PTK_IMG_MENU_ITEM( N_( "Extract _To" ), "gtk-directory", on_popup_extract_to_activate, 0, 0 ),
        PTK_MENU_END
    };

static PtkMenuItemEntry basic_popup_menu[] =
    {
        PTK_MENU_ITEM( N_( "Open _with..." ), NULL, 0, 0 ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_STOCK_MENU_ITEM( "gtk-cut", on_popup_cut_activate ),
        PTK_STOCK_MENU_ITEM( "gtk-copy", on_popup_copy_activate ),
        PTK_IMG_MENU_ITEM( N_( "Copy as Te_xt" ), GTK_STOCK_COPY, on_popup_copy_text_activate, GDK_C, GDK_CONTROL_MASK | GDK_SHIFT_MASK ),   //MOD added
        PTK_IMG_MENU_ITEM( N_( "Copy _Name" ), GTK_STOCK_COPY, on_popup_copy_name_activate, GDK_C, GDK_MOD1_MASK | GDK_SHIFT_MASK ),   //MOD added
        PTK_STOCK_MENU_ITEM( "gtk-paste", on_popup_paste_activate ),
        PTK_IMG_MENU_ITEM( N_( "Paste as _Link" ), GTK_STOCK_PASTE, on_popup_paste_link_activate, GDK_V, GDK_CONTROL_MASK | GDK_SHIFT_MASK ),   //MOD added
        PTK_IMG_MENU_ITEM( N_( "Paste as Tar_get" ), GTK_STOCK_PASTE, on_popup_paste_target_activate, GDK_V, GDK_MOD1_MASK | GDK_SHIFT_MASK ),   //MOD added
        PTK_IMG_MENU_ITEM( N_( "_Delete" ), "gtk-delete", on_popup_delete_activate, GDK_Delete, 0 ),
        PTK_IMG_MENU_ITEM( N_( "_Rename" ), "gtk-edit", on_popup_rename_activate, GDK_F2, 0 ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_MENU_ITEM( N_( "Compress" ), on_popup_compress_activate, 0, 0 ),
        PTK_POPUP_MENU( N_( "E_xtract" ), extract_menu ),
        PTK_POPUP_IMG_MENU( N_( "_Create New" ), "gtk-new", create_new_menu ),
        PTK_IMG_MENU_ITEM( N_( "R_un Command..." ), GTK_STOCK_EXECUTE, on_popup_run_command, GDK_r, GDK_CONTROL_MASK ),  //MOD
        PTK_SEPARATOR_MENU_ITEM,
        PTK_IMG_MENU_ITEM( N_( "_Properties" ), "gtk-info", on_popup_file_properties_activate, GDK_Return, GDK_MOD1_MASK ),
        PTK_MENU_END
    };

static PtkMenuItemEntry dir_popup_menu_items[] =
    {
        PTK_SEPARATOR_MENU_ITEM,
        PTK_MENU_ITEM( N_( "Open in New _Tab" ), on_popup_open_in_new_tab_activate, 0, 0 ),
        PTK_MENU_ITEM( N_( "Open in New _Window" ), on_popup_open_in_new_win_activate, 0, 0 ),
        PTK_IMG_MENU_ITEM( N_( "Open in Terminal" ), GTK_STOCK_EXECUTE, on_popup_open_in_terminal_activate, 0, 0 ),
        PTK_MENU_END
    };

#if 0
static gboolean same_file_type( GList* files )
{
    GList * l;
    VFSMimeType* mime_type;
    if ( ! files || ! files->next )
        return TRUE;
    mime_type = vfs_file_info_get_mime_type( ( VFSFileInfo* ) l->data );
    for ( l = files->next; l ; l = l->next )
    {
        VFSMimeType * mime_type2;
        mime_type2 = vfs_file_info_get_mime_type( ( VFSFileInfo* ) l->data );
        vfs_mime_type_unref( mime_type2 );
        if ( mime_type != mime_type2 )
        {
            vfs_mime_type_unref( mime_type );
            return FALSE;
        }
    }
    vfs_mime_type_unref( mime_type );
    return TRUE;
}
#endif
*/

void on_popup_list_detailed( GtkMenuItem *menuitem, PtkFileBrowser* browser )
{
    int p = browser->mypanel;

    if ( xset_get_b_panel( p, "list_detailed" ) )
    {
        xset_set_b_panel( p, "list_icons", FALSE );
        xset_set_b_panel( p, "list_compact", FALSE );
    }
    else
    {
        if ( !xset_get_b_panel( p, "list_icons" )
                        && !xset_get_b_panel( p, "list_compact" ) )
            xset_set_b_panel( p, "list_icons", TRUE );
    }
    update_views_all_windows( NULL, browser );
}

void on_popup_list_icons( GtkMenuItem *menuitem, PtkFileBrowser* browser )
{
    int p = browser->mypanel;

    if ( xset_get_b_panel( p, "list_icons" ) )
    {
        xset_set_b_panel( p, "list_detailed", FALSE );
        xset_set_b_panel( p, "list_compact", FALSE );
    }
    else
    {
        if ( !xset_get_b_panel( p, "list_detailed" )
                        && !xset_get_b_panel( p, "list_compact" ) )
            xset_set_b_panel( p, "list_detailed", TRUE );
    }
    update_views_all_windows( NULL, browser );
}

void on_popup_list_compact( GtkMenuItem *menuitem, PtkFileBrowser* browser )
{
    int p = browser->mypanel;

    if ( xset_get_b_panel( p, "list_compact" ) )
    {
        xset_set_b_panel( p, "list_detailed", FALSE );
        xset_set_b_panel( p, "list_icons", FALSE );
    }
    else
    {
        if ( !xset_get_b_panel( p, "list_icons" )
                        && !xset_get_b_panel( p, "list_detailed" ) )
            xset_set_b_panel( p, "list_detailed", TRUE );
    }
    update_views_all_windows( NULL, browser );
}

void on_popup_show_hidden( GtkMenuItem *menuitem, PtkFileMenu* data )
{
    if ( data->browser )
        ptk_file_browser_show_hidden_files( data->browser,
                    xset_get_b_panel( data->browser->mypanel, "show_hidden" ) );
}

void on_copycmd( GtkMenuItem *menuitem, PtkFileMenu* data, XSet* set2 )
{
    XSet* set;
    if ( menuitem )
        set = (XSet*)g_object_get_data( G_OBJECT( menuitem ), "set" );
    else
        set = set2;
    if ( !set )
        return;
    if ( data->browser )
        ptk_file_browser_copycmd( data->browser, data->sel_files, data->cwd,
                                                                    set->name );
#ifdef DESKTOP_INTEGRATION
    else if ( data->desktop )
        desktop_window_copycmd( data->desktop, data->sel_files, data->cwd,
                                                                    set->name );
#endif
}

void on_popup_rootcmd_activate( GtkMenuItem *menuitem, PtkFileMenu* data, XSet* set2 )
{
    XSet* set;
    if ( menuitem )
        set = (XSet*)g_object_get_data( G_OBJECT( menuitem ), "set" );
    else
        set = set2;
    if ( set )
        ptk_file_misc_rootcmd( data->desktop, data->browser, data->sel_files,
                                                        data->cwd, set->name );
}

void on_popup_select_pattern( GtkMenuItem *menuitem, PtkFileMenu* data )
{
    if ( data->browser )
        ptk_file_browser_select_pattern( NULL, data->browser, NULL );
}

void on_open_in_tab( GtkMenuItem *menuitem, PtkFileMenu* data )
{
    int tab_num = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( menuitem ),
                                                                "tab_num" ) );
    if ( data->browser )
        ptk_file_browser_open_in_tab( data->browser, tab_num, data->file_path );
}

void on_open_in_panel( GtkMenuItem *menuitem, PtkFileMenu* data )
{
    int panel_num = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( menuitem ),
                                                                "panel_num" ) );
    if ( data->browser )
        main_window_open_in_panel( data->browser, panel_num, data->file_path );
}

void on_file_edit( GtkMenuItem *menuitem, PtkFileMenu* data )
{
    xset_edit( data->browser ? GTK_WIDGET( data->browser ) : 
                               GTK_WIDGET( data->desktop ),
               data->file_path, FALSE, TRUE );
}

void on_file_root_edit( GtkMenuItem *menuitem, PtkFileMenu* data )
{
    xset_edit( data->browser ? GTK_WIDGET( data->browser ) : 
                               GTK_WIDGET( data->desktop ),
               data->file_path, TRUE, FALSE );
}

void on_popup_sort_extra( GtkMenuItem *menuitem, PtkFileBrowser* file_browser,
                                                                    XSet* set2 )
{
    XSet* set;
    if ( menuitem )
        set = (XSet*)g_object_get_data( G_OBJECT( menuitem ), "set" );
    else
        set = set2;
    ptk_file_browser_set_sort_extra( file_browser, set->name );
}

void on_popup_sortby( GtkMenuItem *menuitem, PtkFileBrowser* file_browser, int order )
{
    char* val;
    int v;

    int sort_order;
    if ( menuitem )
        sort_order = GPOINTER_TO_INT( g_object_get_data( G_OBJECT(menuitem),
                                                                "sortorder" ) );    
    else
        sort_order = order;
    
    if ( sort_order < 0 )
    {
        if ( sort_order == -1 )
            v = GTK_SORT_ASCENDING;
        else
            v = GTK_SORT_DESCENDING;
        val = g_strdup_printf( "%d", v );
        xset_set_panel( file_browser->mypanel, "list_detailed", "y", val );
        g_free( val );
        ptk_file_browser_set_sort_type( file_browser, v );
    }
    else
    {
        val = g_strdup_printf( "%d", sort_order );
        xset_set_panel( file_browser->mypanel, "list_detailed", "x", val );
        g_free( val );    
        ptk_file_browser_set_sort_order( file_browser, sort_order );
    }
}

void on_popup_detailed_column( GtkMenuItem *menuitem, PtkFileBrowser* file_browser )
{
    if ( file_browser->view_mode == PTK_FB_LIST_VIEW )
    {
        // get visiblity for correct mode
        FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;
        int p = file_browser->mypanel;
        char mode = main_window->panel_context[p-1];
        
        XSet* set = xset_get_panel_mode( p, "detcol_size", mode );
        set->b = xset_get_panel( p, "detcol_size" )->b;
        set = xset_get_panel_mode( p, "detcol_type", mode );
        set->b = xset_get_panel( p, "detcol_type" )->b;
        set = xset_get_panel_mode( p, "detcol_perm", mode );
        set->b = xset_get_panel( p, "detcol_perm" )->b;
        set = xset_get_panel_mode( p, "detcol_owner", mode );
        set->b = xset_get_panel( p, "detcol_owner" )->b;
        set = xset_get_panel_mode( p, "detcol_date", mode );
        set->b = xset_get_panel( p, "detcol_date" )->b;
        
        update_views_all_windows( NULL, file_browser );
    }
}

void on_popup_toggle_view( GtkMenuItem *menuitem, PtkFileBrowser* file_browser )
{
    // get visiblity for correct mode
    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;
    int p = file_browser->mypanel;
    char mode = main_window->panel_context[p-1];
    
    XSet* set = xset_get_panel_mode( p, "show_toolbox", mode );
    set->b = xset_get_panel( p, "show_toolbox" )->b;
    set = xset_get_panel_mode( p, "show_devmon", mode );
    set->b = xset_get_panel( p, "show_devmon" )->b;
    set = xset_get_panel_mode( p, "show_dirtree", mode );
    set->b = xset_get_panel( p, "show_dirtree" )->b;
    set = xset_get_panel_mode( p, "show_book", mode );
    set->b = xset_get_panel( p, "show_book" )->b;
    set = xset_get_panel_mode( p, "show_sidebar", mode );
    set->b = xset_get_panel( p, "show_sidebar" )->b;
    
    update_views_all_windows( NULL, file_browser );
}

void on_archive_default( GtkMenuItem *menuitem, XSet* set )
{
    const char* arcname[] =
    {
        "arc_def_open",
        "arc_def_ex",
        "arc_def_exto",
        "arc_def_list"
    };
    int i;
    for ( i = 0; i < G_N_ELEMENTS( arcname ); i++ )
    {
        if ( !strcmp( set->name, arcname[i] ) )
            set->b = XSET_B_TRUE;
        else
            xset_set_b( arcname[i], FALSE );
    }
}

void on_hide_file( GtkMenuItem *menuitem, PtkFileMenu* data )
{
    if ( data->browser )
        ptk_file_browser_hide_selected( data->browser, data->sel_files, data->cwd );
}

void on_permission( GtkMenuItem *menuitem, PtkFileMenu* data )
{
    if ( data->browser )
        ptk_file_browser_on_permission( menuitem, data->browser, data->sel_files,
                                                                    data->cwd );
}

int bookmark_item_comp2( const char* item, const char* path )
{
    return strcmp( ptk_bookmarks_item_get_path( item ), path );
}

void on_add_bookmark( GtkMenuItem *menuitem, PtkFileMenu* data )
{
    char* name;
    char* path;
    
    if ( !data->cwd )
        return;
    
    if ( data->file_path && g_file_test( data->file_path, G_FILE_TEST_IS_DIR ) )
        path = data->file_path;
    else
        path = data->cwd;
        
    if ( ! g_list_find_custom( app_settings.bookmarks->list,
                               path,
                               ( GCompareFunc ) bookmark_item_comp2 ) )
    {
        name = g_path_get_basename( path );
        ptk_bookmarks_append( name, path );
        g_free( name );
    }
    else
        xset_msg_dialog( data->browser ? GTK_WIDGET( data->browser ) : 
                                         GTK_WIDGET( data->desktop ),
                         GTK_MESSAGE_INFO,
                         _("Bookmark Exists"), NULL, 0,
                         _("Bookmark already exists"), NULL, NULL );
}

void on_popup_mount_iso( GtkMenuItem *menuitem, PtkFileMenu* data )
{
    mount_iso( data->browser, data->file_path );
}

void on_popup_desktop_sort_activate( GtkMenuItem *menuitem,
                                            DesktopWindow* desktop, XSet* set2 )
{
    XSet* set;
    if ( menuitem )
        set = (XSet*)g_object_get_data( G_OBJECT( menuitem ), "set" );
    else
        set = set2;
    if ( !( set && desktop && g_str_has_prefix( set->name, "desk_sort_" ) ) )
        return;

#ifdef DESKTOP_INTEGRATION
    int by;
    char* xname = set->name + 10;
    if ( !strcmp( xname, "name" ) )
        by = DW_SORT_BY_NAME;
    else if ( !strcmp( xname, "size" ) )
        by = DW_SORT_BY_SIZE;
    else if ( !strcmp( xname, "type" ) )
        by = DW_SORT_BY_TYPE;
    else if ( !strcmp( xname, "date" ) )
        by = DW_SORT_BY_MTIME;
    else if ( !strcmp( xname, "cust" ) )
        by = DW_SORT_CUSTOM;
    else
    {
        if ( !strcmp( xname, "ascend" ) )
            by = GTK_SORT_ASCENDING;
        else if ( !strcmp( xname, "descend" ) )
            by = GTK_SORT_DESCENDING;
        else
            return;
        desktop_window_sort_items( desktop, desktop->sort_by, by );
        return;
    }
    desktop_window_sort_items( desktop, by, desktop->sort_type );
#endif
}

void on_popup_desktop_pref_activate( GtkMenuItem *menuitem, DesktopWindow* desktop )
{
    if ( desktop )
        fm_edit_preference( GTK_WINDOW( desktop ), PREF_DESKTOP );
}

void on_popup_desktop_new_app_activate( GtkMenuItem *menuitem, DesktopWindow* desktop )
{
#ifdef DESKTOP_INTEGRATION
    if ( desktop )
        desktop_window_add_application( desktop );
#endif
}

void on_popup_desktop_select( GtkMenuItem *menuitem,
                                            DesktopWindow* desktop, XSet* set2 )
{
    XSet* set;
    if ( menuitem )
        set = (XSet*)g_object_get_data( G_OBJECT( menuitem ), "set" );
    else
        set = set2;
    if ( !( set && set->name && desktop ) )
        return;
        
#ifdef DESKTOP_INTEGRATION
    DWSelectMode mode;
    if ( !strcmp( set->name, "select_all" ) )
        mode = DW_SELECT_ALL;
    else if ( !strcmp( set->name, "select_un" ) )
        mode = DW_SELECT_NONE;
    else if ( !strcmp( set->name, "select_invert" ) )
        mode = DW_SELECT_INVERSE;
    else if ( !strcmp( set->name, "select_patt" ) )
        mode = DW_SELECT_PATTERN;
    else
        return;
    desktop_window_select( desktop, mode );
#endif
}

void on_bookmark_activate( GtkWidget* item, const char* name )
{
    if ( name )
        open_in_prog( ptk_bookmarks_item_get_path( name ) );
}

static void ptk_file_menu_free( PtkFileMenu *data )
{
    if ( data->file_path )
        g_free( data->file_path );
    if ( data->info )
        vfs_file_info_unref( data->info );
    g_free( data->cwd );
    if ( data->sel_files )
        vfs_file_info_list_free( data->sel_files );
    if ( data->accel_group )
        g_object_unref( data->accel_group );
    g_slice_free( PtkFileMenu, data );
}

/* Retrieve popup menu for selected file(s) */
GtkWidget* ptk_file_menu_new( DesktopWindow* desktop, PtkFileBrowser* browser,
                                const char* file_path, VFSFileInfo* info,
                                const char* cwd, GList* sel_files )
{   // either desktop or browser must be non-NULL
    GtkWidget * popup = NULL;
    VFSMimeType* mime_type;
    GtkWidget *app_menu_item;
    GtkWidget* submenu;
    gboolean is_dir;
    gboolean is_text;
    gboolean is_clip;
    char **apps, **app;
    const char* app_name = NULL;
    VFSAppDesktop* desktop_file;
    char* name;
    char* desc;
    char* str;
    XSet* set_radio;
    GdkPixbuf* app_icon;
    int icon_w, icon_h;
    GtkWidget* app_img;
    int i;
    PtkFileMenu* data;
    int no_write_access = 0, no_read_access = 0;
    XSet* set, *set2;
    GtkMenuItem* item;

    if ( !desktop && !browser )
        return NULL;
        
    data = g_slice_new0( PtkFileMenu );

    data->cwd = g_strdup( cwd );
    data->browser = browser;
    data->desktop = desktop;

    data->file_path = g_strdup( file_path );
    if ( info )
        data->info = vfs_file_info_ref( info );
    else
        data->info = NULL;
    data->sel_files = sel_files;

    data->accel_group = gtk_accel_group_new ();

    popup = gtk_menu_new();
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    g_object_weak_ref( G_OBJECT( popup ), (GWeakNotify) ptk_file_menu_free, data );
    g_signal_connect_after( ( gpointer ) popup, "selection-done",
                            G_CALLBACK ( gtk_widget_destroy ), NULL );

    //is_dir = file_path && g_file_test( file_path, G_FILE_TEST_IS_DIR );
    is_dir = ( info && vfs_file_info_is_dir( info ) );
    // Note: network filesystems may become unresponsive here
    is_text = info && file_path && vfs_file_info_is_text( info, file_path );

    // test R/W access to cwd instead of selected file
    // Note: network filesystems may become unresponsive here
#if defined(HAVE_EUIDACCESS)
    no_read_access = euidaccess( cwd, R_OK );
    no_write_access = euidaccess( cwd, W_OK );
#elif defined(HAVE_EACCESS)
    no_read_access = eaccess( cwd, R_OK );
    no_write_access = eaccess( cwd, W_OK );
#endif

    GtkClipboard* clip = gtk_clipboard_get( GDK_SELECTION_CLIPBOARD );
    if ( ! gtk_clipboard_wait_is_target_available ( clip,
                gdk_atom_intern( "x-special/gnome-copied-files", FALSE ) ) &&
         ! gtk_clipboard_wait_is_target_available ( clip,
                    gdk_atom_intern( "text/uri-list", FALSE ) ) )
        is_clip = FALSE;
    else
        is_clip = TRUE;

    int p = 0;
    int tab_count = 0;
    int tab_num = 0;
    int panel_count = 0;
    if ( browser )
    {
        p = browser->mypanel;
        main_window_get_counts( browser, &panel_count, &tab_count, &tab_num );
    }

    XSetContext* context = xset_context_new();

    // Get mime type and apps
    if ( info )
    {
        mime_type = vfs_file_info_get_mime_type( info );
        apps = vfs_mime_type_get_actions( mime_type );
        context->var[CONTEXT_MIME] = g_strdup( vfs_mime_type_get_type( mime_type ) );
    }
    else
    {
        mime_type = NULL;
        apps = NULL;
        context->var[CONTEXT_MIME] = g_strdup( "" );
    }

    // context
    if ( file_path )
        context->var[CONTEXT_NAME] = g_path_get_basename( file_path );
    else
        context->var[CONTEXT_NAME] = g_strdup( "" );
    context->var[CONTEXT_DIR] = g_strdup( cwd );
    context->var[CONTEXT_WRITE_ACCESS] = no_write_access ? g_strdup( "false" ) :
                                                           g_strdup( "true" );
    context->var[CONTEXT_IS_TEXT] = is_text ? g_strdup( "true" ) :
                                              g_strdup( "false" );
    context->var[CONTEXT_IS_DIR] = is_dir ? g_strdup( "true" ) :
                                            g_strdup( "false" );
    context->var[CONTEXT_MUL_SEL] = sel_files && sel_files->next ?
                                        g_strdup( "true" ) : g_strdup( "false" );
    context->var[CONTEXT_CLIP_FILES] = is_clip ? g_strdup( "true" ) :
                                                            g_strdup( "false" );
    if ( info )
        context->var[CONTEXT_IS_LINK] = vfs_file_info_is_symlink( info ) ?
                                    g_strdup( "true" ) : g_strdup( "false" );
    else
        context->var[CONTEXT_IS_LINK] = g_strdup( "false" );

    if ( browser )
        main_context_fill( browser, context );
#ifdef DESKTOP_INTEGRATION
    else
        desktop_context_fill( desktop, context );
#endif

    if ( !context->valid )
    {
        // rare exception due to context_fill hacks - fb was probably destroyed
        g_warning( "context_fill rare exception" );
        context = xset_context_new();
        g_slice_free( XSetContext, context );
        context = NULL;
        return NULL;
    }

    // Open >
    set = xset_get( "con_open" );
    set->disable = !( sel_files || desktop );
    item = GTK_MENU_ITEM( xset_add_menuitem( desktop, browser, popup,
                                                    accel_group, set ) );
    if ( sel_files )
    {
        submenu = gtk_menu_item_get_submenu( item );

        // Execute
        if ( !is_dir && info && file_path && 
                    ( info->flags & VFS_FILE_INFO_DESKTOP_ENTRY || 
                        // Note: network filesystems may become unresponsive here
                        vfs_file_info_is_executable( info, file_path ) ) )
        {
            set = xset_set_cb( "open_execute", on_popup_open_activate, data );
            xset_add_menuitem( desktop, browser, submenu, accel_group, set );
        }

        // add apps
        if ( is_text )
        {
            char **tmp, **txt_apps;
            VFSMimeType* txt_type;
            int len1, len2;
            txt_type = vfs_mime_type_get_from_type( XDG_MIME_TYPE_PLAIN_TEXT );
            txt_apps = vfs_mime_type_get_actions( txt_type );
            if ( txt_apps )
            {
                len1 = apps ? g_strv_length( apps ) : 0;
                len2 = g_strv_length( txt_apps );
                tmp = apps;
                apps = vfs_mime_type_join_actions( apps, len1, txt_apps, len2 );
                g_strfreev( txt_apps );
                g_strfreev( tmp );
            }
            vfs_mime_type_unref( txt_type );
        }
        if ( apps )
        {
            for ( app = apps; *app; ++app )
            {
                if ( ( app - apps ) == 1 )  // Add a separator after default app
                {
                    item = GTK_MENU_ITEM( gtk_separator_menu_item_new() );
                    gtk_widget_show ( GTK_WIDGET( item ) );
                    gtk_container_add ( GTK_CONTAINER ( submenu ), 
                                                        GTK_WIDGET( item ) );
                }
                desktop_file = vfs_app_desktop_new( *app );
                app_name = vfs_app_desktop_get_disp_name( desktop_file );
                if ( app_name )
                    app_menu_item = gtk_image_menu_item_new_with_label ( app_name );
                else
                    app_menu_item = gtk_image_menu_item_new_with_label ( *app );
                gtk_container_add ( GTK_CONTAINER ( submenu ), app_menu_item );
                g_signal_connect( G_OBJECT( app_menu_item ), "activate",
                                  G_CALLBACK( on_popup_run_app ), ( gpointer ) data );
                g_object_set_data( G_OBJECT( app_menu_item ), "menu", submenu );
                g_signal_connect( G_OBJECT( app_menu_item ), "button-press-event",
                                            G_CALLBACK( on_app_button_press ),
                                            ( gpointer ) data );
                g_signal_connect( G_OBJECT( app_menu_item ), "button-release-event",
                                            G_CALLBACK( on_app_button_press ),
                                            ( gpointer ) data );
                g_object_set_data_full( G_OBJECT( app_menu_item ), "desktop_file",
                                        desktop_file, vfs_app_desktop_unref );
                gtk_icon_size_lookup_for_settings( gtk_settings_get_default(),
                                                   GTK_ICON_SIZE_MENU,
                                                   &icon_w, &icon_h );
                app_icon = vfs_app_desktop_get_icon( desktop_file,
                                    icon_w > icon_h ? icon_w : icon_h, TRUE );
                if ( app_icon )
                {
                    app_img = gtk_image_new_from_pixbuf( app_icon );
                    if ( app_img )
                        gtk_image_menu_item_set_image ( GTK_IMAGE_MENU_ITEM( 
                                                app_menu_item ), app_img );
                    g_object_unref( app_icon );
                }
            }
            g_strfreev( apps );
        }

        // open with other
        item = GTK_MENU_ITEM( gtk_separator_menu_item_new() );
        gtk_menu_shell_append( GTK_MENU_SHELL( submenu ), GTK_WIDGET( item ) );

        set = xset_set_cb( "open_other", on_popup_open_with_another_activate, data );
        xset_add_menuitem( desktop, browser, submenu, accel_group, set );
                        
        // Default
        char* plain_type = NULL;
        if ( mime_type )
            plain_type = g_strdup( vfs_mime_type_get_type( mime_type ) );
        if ( !plain_type )
            plain_type = g_strdup( "" );
        str = plain_type;
        plain_type = replace_string( str, "-", "_", FALSE );
        g_free( str );
        str = replace_string( plain_type, " ", "", FALSE );
        g_free( plain_type );
        plain_type = g_strdup_printf( "open_all_type_%s", str );
        g_free( str );
        set = xset_set_cb( plain_type, on_popup_open_all, data );
        g_free( plain_type );
        set->lock = TRUE;
        set->menu_style = XSET_MENU_NORMAL;
        if ( set->shared_key )
            g_free( set->shared_key );
        set->shared_key = g_strdup( "open_all" );
        set2 = xset_get( "open_all" );
        if ( set->menu_label )
            g_free( set->menu_label );
        set->menu_label = g_strdup( set2->menu_label );
        if ( set->context )
        {
            g_free( set->context );
            set->context = NULL;
        }
        item = GTK_MENU_ITEM( xset_add_menuitem( desktop, browser, submenu,
                                                        accel_group, set ) );
        app_icon = mime_type ? vfs_mime_type_get_icon( mime_type, FALSE ) : NULL;
        if ( app_icon )
        {
            app_img = gtk_image_new_from_pixbuf( app_icon );
            if ( app_img )
                gtk_image_menu_item_set_image ( GTK_IMAGE_MENU_ITEM( item ),
                                                                    app_img );
            g_object_unref( app_icon );
        } 
        if ( set->menu_label )
            g_free( set->menu_label );
        set->menu_label = NULL;  // don't bother to save this
                                   
        // Edit / Dir
        if ( ( is_dir && browser ) || ( is_text && sel_files && !sel_files->next ) )
        {
            item = GTK_MENU_ITEM( gtk_separator_menu_item_new() );
            gtk_menu_shell_append( GTK_MENU_SHELL( submenu ), GTK_WIDGET( item ) );
            
            if ( is_text )
            {
                // Edit
                set = xset_set_cb( "open_edit", on_file_edit, data );
                set->disable = ( geteuid() == 0 );
                xset_add_menuitem( desktop, browser, submenu, accel_group, set );
                set = xset_set_cb( "open_edit_root", on_file_root_edit, data );
                xset_add_menuitem( desktop, browser, submenu, accel_group, set );
            }
            else if ( browser && is_dir )
            {
                // Open Dir
                set = xset_set_cb( "opentab_prev", on_open_in_tab, data );
                    xset_set_ob1_int( set, "tab_num", -1 );
                    set->disable = ( tab_num == 1 );
                set = xset_set_cb( "opentab_next", on_open_in_tab, data );
                    xset_set_ob1_int( set, "tab_num", -2 );
                    set->disable = ( tab_num == tab_count );
                set = xset_set_cb( "opentab_new", on_popup_open_in_new_tab_activate,
                                                                        data );
                for ( i = 1; i < 11; i++ )
                {
                    name = g_strdup_printf( "opentab_%d", i );
                    set = xset_set_cb( name, on_open_in_tab, data );
                        xset_set_ob1_int( set, "tab_num", i );
                        set->disable = ( i > tab_count ) || ( i == tab_num );
                    g_free( name );
                }

                set = xset_set_cb( "open_in_panelprev", on_open_in_panel, data );
                    xset_set_ob1_int( set, "panel_num", -1 );
                    set->disable = ( panel_count == 1 );
                set = xset_set_cb( "open_in_panelnext", on_open_in_panel, data );
                    xset_set_ob1_int( set, "panel_num", -2 );
                    set->disable = ( panel_count == 1 );

                for ( i = 1; i < 5; i++ )
                {
                    name = g_strdup_printf( "open_in_panel%d", i );
                    set = xset_set_cb( name, on_open_in_panel, data );
                        xset_set_ob1_int( set, "panel_num", i );
                        //set->disable = ( p == i );
                    g_free( name );
                }

                set = xset_get( "open_in_tab" );
                xset_add_menuitem( desktop, browser, submenu, accel_group, set );    
                set = xset_get( "open_in_panel" );
                xset_add_menuitem( desktop, browser, submenu, accel_group, set );
            }
        }

        // Obtaining file extension
        char *not_used = NULL, *extension = NULL;
        if (info)
            not_used = get_name_extension(
                (char*)vfs_file_info_get_name( info ), FALSE,
                &extension );

        // Archive commands
        gboolean can_extract = ptk_file_archiver_is_format_supported(
                                    mime_type, extension, ARC_EXTRACT );
        gboolean can_list = ptk_file_archiver_is_format_supported( mime_type,
                                                  extension, ARC_LIST );
        if ( can_extract || can_list )
        {
            item = GTK_MENU_ITEM( gtk_separator_menu_item_new() );
            gtk_menu_shell_append( GTK_MENU_SHELL( submenu ), GTK_WIDGET( item ) );

            set = xset_set_cb( "arc_extract", on_popup_extract_here_activate, data );
            xset_set_ob1( set, "set", set );

            /* Disabling extraction if archive handler can't cope or
             * there is no write access to the current directory - this
             * is a semi-permanent setting so need to explicitly flip
             * back to false otherwise */
            if (can_extract)
                set->disable = no_write_access;
            else
                set->disable = TRUE;
            xset_add_menuitem( desktop, browser, submenu, accel_group, set );    

            set = xset_set_cb( "arc_extractto", on_popup_extract_to_activate, data );
            xset_set_ob1( set, "set", set );
            if (can_extract)
                set->disable = FALSE;
            else
                set->disable = TRUE;
            xset_add_menuitem( desktop, browser, submenu, accel_group, set );    

            set = xset_set_cb( "arc_list", on_popup_extract_list_activate, data );
            xset_set_ob1( set, "set", set );
            if (can_list)
                set->disable = FALSE;
            else
                set->disable = TRUE;
            xset_add_menuitem( desktop, browser, submenu, accel_group, set );    

            set = xset_set_cb( "arc_def_open", on_archive_default, set );
            xset_set_ob2( set, NULL, NULL );
            set_radio = set;

            set = xset_set_cb( "arc_def_ex", on_archive_default, set );
            xset_set_ob2( set, NULL, set_radio );
            
            set = xset_set_cb( "arc_def_exto", on_archive_default, set );
            xset_set_ob2( set, NULL, set_radio );

            set = xset_set_cb( "arc_def_list", on_archive_default, set );
            xset_set_ob2( set, NULL, set_radio );

            if ( geteuid() == 0 )
            {
                set = xset_get( "arc_def_write" );
                set->b = XSET_B_FALSE;
                set->disable = TRUE;
            }
            
            xset_set_cb( "arc_conf2", ptk_file_archiver_config, browser );
            
            xset_add_menuitem( desktop, browser, submenu, accel_group,
                                                        xset_get( "arc_default" ) );    
        }
        else if ( file_path && mime_type && !is_dir && ( 
                  !strcmp( vfs_mime_type_get_type( mime_type ),
                                            "application/x-cd-image" ) ||
                  !strcmp( vfs_mime_type_get_type( mime_type ),
                                            "application/x-iso9660-image" ) ||
                  g_str_has_suffix( file_path, ".iso" ) ||
                  g_str_has_suffix( file_path, ".img" ) ) )
        {
            item = GTK_MENU_ITEM( gtk_separator_menu_item_new() );
            gtk_menu_shell_append( GTK_MENU_SHELL( submenu ), GTK_WIDGET( item ) );

            set = xset_set_cb( "iso_mount", on_popup_mount_iso, data );
            xset_add_menuitem( desktop, browser, submenu, accel_group, set );    

            set = xset_get( "iso_auto" );
            xset_add_menuitem( desktop, browser, submenu, accel_group, set );    
            str = g_find_program_in_path( "udevil" );
            set->disable = !str;
            g_free( str );
        }
        g_signal_connect (submenu, "key-press-event",
                                    G_CALLBACK (app_menu_keypress), data );

        // Cleaning up
        g_free( not_used );
        g_free( extension );
    }
#ifdef DESKTOP_INTEGRATION
    else if ( desktop )
    {
        // desktop, no selected files
        submenu = gtk_menu_item_get_submenu( item );
        set = xset_set_cb( "desk_open", desktop_window_open_desktop_dir,
                                                                desktop );
        xset_add_menuitem( desktop, browser, submenu, accel_group, set );
    }
#endif
    
    if ( mime_type )
        vfs_mime_type_unref( mime_type );

    // Go >
    if ( browser )
    {
        set = xset_set_cb( "go_back", ptk_file_browser_go_back, browser );
            set->disable = !( browser->curHistory && browser->curHistory->prev );
        set = xset_set_cb( "go_forward", ptk_file_browser_go_forward, browser );
            set->disable = !( browser->curHistory && browser->curHistory->next );
        set = xset_set_cb( "go_up", ptk_file_browser_go_up, browser );
            set->disable = !strcmp( cwd, "/" );
        xset_set_cb( "go_home", ptk_file_browser_go_home, browser );
        xset_set_cb( "go_default", ptk_file_browser_go_default, browser );
        xset_set_cb( "go_set_default", ptk_file_browser_set_default_folder, browser );
        xset_set_cb( "edit_canon", on_popup_canon, data );
        xset_set_cb( "go_refresh", ptk_file_browser_refresh, browser );
        set = xset_set_cb( "focus_path_bar", ptk_file_browser_focus, browser );
            xset_set_ob1_int( set, "job", 0 );
        set = xset_set_cb( "focus_filelist", ptk_file_browser_focus, browser );
            xset_set_ob1_int( set, "job", 4 );
        set = xset_set_cb( "focus_dirtree", ptk_file_browser_focus, browser );
            xset_set_ob1_int( set, "job", 1 );
        set = xset_set_cb( "focus_book", ptk_file_browser_focus, browser );
            xset_set_ob1_int( set, "job", 2 );
        set = xset_set_cb( "focus_device", ptk_file_browser_focus, browser );
            xset_set_ob1_int( set, "job", 3 );
        
        // Go > Tab >
        set = xset_set_cb( "tab_prev", ptk_file_browser_go_tab, browser );
            xset_set_ob1_int( set, "tab_num", -1 );
            set->disable = ( tab_count < 2 );
        set = xset_set_cb( "tab_next", ptk_file_browser_go_tab, browser );
            xset_set_ob1_int( set, "tab_num", -2 );
            set->disable = ( tab_count < 2 );
        set = xset_set_cb( "tab_close", ptk_file_browser_go_tab, browser );
            xset_set_ob1_int( set, "tab_num", -3 );

        for ( i = 1; i < 11; i++ )
        {
            name = g_strdup_printf( "tab_%d", i );
            set = xset_set_cb( name, ptk_file_browser_go_tab, browser );
                xset_set_ob1_int( set, "tab_num", i );
                set->disable = ( i > tab_count ) || ( i == tab_num );
            g_free( name );
        }
        set = xset_get( "con_go" );
        xset_add_menuitem( desktop, browser, popup, accel_group, set );
    }
    
    // New >
    if ( desktop || browser )
    {
        set = xset_set_cb( "new_file", on_popup_new_text_file_activate, data );
        set = xset_set_cb( "new_folder", on_popup_new_folder_activate, data );
        set = xset_set_cb( "new_link", on_popup_new_link_activate, data );

        if ( desktop )
        {
            xset_set_cb( "new_app", on_popup_desktop_new_app_activate, desktop );

            set = xset_get( "open_new" );
            xset_set_set( set, "desc", "new_file new_folder new_link sep_o1 new_app" );
        }
        else
        {
            set = xset_set_cb( "new_archive", on_popup_compress_activate, data );
            set->disable = ( !sel_files || !browser );
            set = xset_set_cb( "tab_new", on_shortcut_new_tab_activate, browser );
            set->disable = !browser;
            set = xset_set_cb( "tab_new_here", on_popup_open_in_new_tab_here, data );
            set->disable = !browser;
            set = xset_set_cb( "new_bookmark", on_add_bookmark, data );
            set->disable = !browser;

            set = xset_get( "open_new" );
            xset_set_set( set, "desc", "new_file new_folder new_link new_archive sep_o1 tab_new tab_new_here new_bookmark" );
        }
        xset_add_menuitem( desktop, browser, popup, accel_group, set );

        set = xset_get( "sep_new" );
        xset_add_menuitem( desktop, browser, popup, accel_group, set );
    }
    
    // Edit  
    if ( browser || desktop )
    {  
        set = xset_set_cb( "copy_name", on_popup_copy_name_activate, data );
        set->disable = !sel_files;
        set = xset_set_cb( "copy_path", on_popup_copy_text_activate, data );
        set->disable = !sel_files;
        set = xset_set_cb( "copy_parent", on_popup_copy_parent_activate, data );
        set->disable = !sel_files;
        set = xset_set_cb( "paste_link", on_popup_paste_link_activate, data );
        set->disable = !is_clip || no_write_access;
        set = xset_set_cb( "paste_target", on_popup_paste_target_activate, data );
        set->disable = !is_clip || no_write_access;

        set = xset_set_cb( "paste_as", on_popup_paste_as_activate, data );
            set->disable = !is_clip;

        set = xset_set_cb( "root_copy_loc", on_popup_rootcmd_activate, data );
            xset_set_ob1( set, "set", set );
            set->disable = !sel_files;
        set = xset_set_cb( "root_move2", on_popup_rootcmd_activate, data );
            xset_set_ob1( set, "set", set );
            set->disable = !sel_files;
        set = xset_set_cb( "root_delete", on_popup_rootcmd_activate, data );
            xset_set_ob1( set, "set", set );
            set->disable = !sel_files;

        set = xset_set_cb( "edit_hide", on_hide_file, data );
        set->disable = !sel_files || no_write_access || desktop || !browser;

        if ( browser )
        {
            xset_set_cb( "select_all", ptk_file_browser_select_all, data->browser );
            set = xset_set_cb( "select_un", ptk_file_browser_unselect_all, browser );
            set->disable = !sel_files;
            xset_set_cb( "select_invert", ptk_file_browser_invert_selection, browser );
            xset_set_cb( "select_patt", on_popup_select_pattern, data );
        }
        else
        {
            xset_set_cb( "select_all", on_popup_desktop_select, desktop );
            set = xset_set_cb( "select_un", on_popup_desktop_select, desktop );
            set->disable = !sel_files;
            xset_set_cb( "select_invert", on_popup_desktop_select, desktop );
            xset_set_cb( "select_patt", on_popup_desktop_select, desktop );
        }

        static const char* copycmd[] =
        {
            "copy_loc",
            "copy_loc_last",
            "copy_tab_prev",
            "copy_tab_next",
            "copy_tab_1",
            "copy_tab_2",
            "copy_tab_3",
            "copy_tab_4",
            "copy_tab_5",
            "copy_tab_6",
            "copy_tab_7",
            "copy_tab_8",
            "copy_tab_9",
            "copy_tab_10",
            "copy_panel_prev",
            "copy_panel_next",
            "copy_panel_1",
            "copy_panel_2",
            "copy_panel_3",
            "copy_panel_4",
            "move_loc",
            "move_loc_last",
            "move_tab_prev",
            "move_tab_next",
            "move_tab_1",
            "move_tab_2",
            "move_tab_3",
            "move_tab_4",
            "move_tab_5",
            "move_tab_6",
            "move_tab_7",
            "move_tab_8",
            "move_tab_9",
            "move_tab_10",
            "move_panel_prev",
            "move_panel_next",
            "move_panel_1",
            "move_panel_2",
            "move_panel_3",
            "move_panel_4"
        };
        for ( i = 0; i < G_N_ELEMENTS( copycmd ); i++ )
        {
            set = xset_set_cb( copycmd[i], on_copycmd, data );
            xset_set_ob1( set, "set", set );
        }

        // enables
        set = xset_get( "copy_loc_last" );
        set2 = xset_get( "move_loc_last" );
        
        if ( desktop )
        {
            set = xset_get( "copy_tab" );
            set->disable = TRUE;
            set = xset_get( "copy_panel" );
            set->disable = TRUE;
            set = xset_get( "move_tab" );
            set->disable = TRUE;
            set = xset_get( "move_panel" );
            set->disable = TRUE;
        }
        else
        {
            set = xset_get( "copy_tab_prev" );
            set->disable = ( tab_num == 1 );
            set = xset_get( "copy_tab_next" );
            set->disable = ( tab_num == tab_count );
            set = xset_get( "move_tab_prev" );
            set->disable = ( tab_num == 1 );
            set = xset_get( "move_tab_next" );
            set->disable = ( tab_num == tab_count );
            
            set = xset_get( "copy_panel_prev" );
            set->disable = ( panel_count < 2 );
            set = xset_get( "copy_panel_next" );
            set->disable = ( panel_count < 2 );
            set = xset_get( "move_panel_prev" );
            set->disable = ( panel_count < 2 );
            set = xset_get( "move_panel_next" );
            set->disable = ( panel_count < 2 );
        
            gboolean b;
            for ( i = 1; i < 11; i++ )
            {
                str = g_strdup_printf( "copy_tab_%d", i );
                set = xset_get( str );
                g_free( str );
                set->disable = ( i > tab_count ) || ( i == tab_num );

                str = g_strdup_printf( "move_tab_%d", i );
                set = xset_get( str );
                g_free( str );
                set->disable = ( i > tab_count ) || ( i == tab_num );

                if ( i > 4 )
                    continue;
                
                b = main_window_panel_is_visible( browser, i );

                str = g_strdup_printf( "copy_panel_%d", i );
                set = xset_get( str );
                g_free( str );
                set->disable = ( i == p ) || !b;

                str = g_strdup_printf( "move_panel_%d", i );
                set = xset_get( str );
                g_free( str );
                set->disable = ( i == p ) || !b;
            }
        }
        
        set = xset_get( "copy_to" );
        set->disable = !sel_files;
        
        set = xset_get( "move_to" );
        set->disable = !sel_files;

        set = xset_get( "edit_root" );
        set->disable = ( geteuid() == 0 ) || !sel_files;

        set = xset_get( "edit_submenu" );
        xset_add_menuitem( desktop, browser, popup, accel_group, set );
    }
    
    set = xset_set_cb( "edit_cut", on_popup_cut_activate, data );
    set->disable = !sel_files;
    xset_add_menuitem( desktop, browser, popup, accel_group, set );
    set = xset_set_cb( "edit_copy", on_popup_copy_activate, data );
    set->disable = !sel_files;
    xset_add_menuitem( desktop, browser, popup, accel_group, set );
    set = xset_set_cb( "edit_paste", on_popup_paste_activate, data );
    set->disable = !is_clip || no_write_access;
    xset_add_menuitem( desktop, browser, popup, accel_group, set );
    set = xset_set_cb( "edit_rename", on_popup_rename_activate, data );
    set->disable = !sel_files;
    xset_add_menuitem( desktop, browser, popup, accel_group, set );
    set = xset_set_cb( "edit_delete", on_popup_delete_activate, data );
    set->disable = !sel_files || no_write_access;
    xset_add_menuitem( desktop, browser, popup, accel_group, set );

    set = xset_get( "sep_edit" );
    xset_add_menuitem( desktop, browser, popup, accel_group, set );

    // View >
    if ( browser )
    {
        FMMainWindow* main_window = (FMMainWindow*)browser->main_window;
        char mode = main_window->panel_context[p-1];

        gboolean show_side = FALSE;
        xset_set_cb( "view_refresh", ptk_file_browser_refresh, browser );
        set = xset_set_cb_panel( p, "show_toolbox", on_popup_toggle_view,
                                                                browser );
            set->b = xset_get_panel_mode( p, "show_toolbox", mode )->b;
        set = xset_set_cb_panel( p, "show_devmon", on_popup_toggle_view,
                                                                browser );
            set->b = xset_get_panel_mode( p, "show_devmon", mode )->b;
            if ( set->b == XSET_B_TRUE ) show_side = TRUE;
        set = xset_set_cb_panel( p, "show_dirtree", on_popup_toggle_view,
                                                                browser );
            set->b = xset_get_panel_mode( p, "show_dirtree", mode )->b;
            if ( set->b == XSET_B_TRUE ) show_side = TRUE;
        set = xset_set_cb_panel( p, "show_book", on_popup_toggle_view,
                                                                browser );
            set->b = xset_get_panel_mode( p, "show_book", mode )->b;
            if ( set->b == XSET_B_TRUE ) show_side = TRUE;
        set = xset_set_cb_panel( p, "show_sidebar", on_popup_toggle_view,
                                                                browser );
            set->b = xset_get_panel_mode( p, "show_sidebar", mode )->b;
            set->disable = !show_side;
        xset_set_cb_panel( p, "show_hidden", on_popup_show_hidden, data );
        
        if ( browser->view_mode == PTK_FB_LIST_VIEW )
        {
            set = xset_set_cb_panel( p, "detcol_size", on_popup_detailed_column,
                                                                browser );
            set->b = xset_get_panel_mode( p, "detcol_size", mode )->b;
            set = xset_set_cb_panel( p, "detcol_type", on_popup_detailed_column,
                                                                browser );
            set->b = xset_get_panel_mode( p, "detcol_type", mode )->b;
            set = xset_set_cb_panel( p, "detcol_perm", on_popup_detailed_column,
                                                                browser );
            set->b = xset_get_panel_mode( p, "detcol_perm", mode )->b;
            set = xset_set_cb_panel( p, "detcol_owner", on_popup_detailed_column,
                                                                browser );
            set->b = xset_get_panel_mode( p, "detcol_owner", mode )->b;
            set = xset_set_cb_panel( p, "detcol_date", on_popup_detailed_column,
                                                                browser );
            set->b = xset_get_panel_mode( p, "detcol_date", mode )->b;
 
            xset_set_cb( "view_reorder_col", on_reorder, browser );
            set = xset_set( "view_columns", "disable", "0" );
            desc = g_strdup_printf( "panel%d_detcol_size panel%d_detcol_type panel%d_detcol_perm panel%d_detcol_owner panel%d_detcol_date sep_v4 view_reorder_col",
                                                            p, p, p, p, p );
            xset_set_set( set, "desc", desc );
            g_free( desc );
            set = xset_set_cb( "rubberband", main_window_rubberband_all, NULL );
            set->disable = FALSE;
        }
        else
        {
            xset_set( "view_columns", "disable", "1" );
            xset_set( "rubberband", "disable", "1" );
        }
        
        set = xset_set_cb_panel( p, "list_detailed", on_popup_list_detailed,
                                                                browser );
            xset_set_ob2( set, NULL, NULL );
            set_radio = set;
        set = xset_set_cb_panel( p, "list_icons", on_popup_list_icons, browser );
            xset_set_ob2( set, NULL, set_radio );
        set = xset_set_cb_panel( p, "list_compact", on_popup_list_compact,
                                                                browser );
            xset_set_ob2( set, NULL, set_radio );

        set = xset_set_cb( "sortby_name", on_popup_sortby, browser );
            xset_set_ob1_int( set, "sortorder", PTK_FB_SORT_BY_NAME );
            xset_set_ob2( set, NULL, NULL );
            set->b = browser->sort_order == PTK_FB_SORT_BY_NAME ?
                                                    XSET_B_TRUE : XSET_B_FALSE;
            set_radio = set;
        set = xset_set_cb( "sortby_size", on_popup_sortby, browser );
            xset_set_ob1_int( set, "sortorder", PTK_FB_SORT_BY_SIZE );
            xset_set_ob2( set, NULL, set_radio );
            set->b = browser->sort_order == PTK_FB_SORT_BY_SIZE ?
                                                    XSET_B_TRUE : XSET_B_FALSE;
        set = xset_set_cb( "sortby_type", on_popup_sortby, browser );
            xset_set_ob1_int( set, "sortorder", PTK_FB_SORT_BY_TYPE );
            xset_set_ob2( set, NULL, set_radio );
            set->b = browser->sort_order == PTK_FB_SORT_BY_TYPE ?
                                                    XSET_B_TRUE : XSET_B_FALSE;
        set = xset_set_cb( "sortby_perm", on_popup_sortby, browser );
            xset_set_ob1_int( set, "sortorder", PTK_FB_SORT_BY_PERM );
            xset_set_ob2( set, NULL, set_radio );
            set->b = browser->sort_order == PTK_FB_SORT_BY_PERM ?
                                                    XSET_B_TRUE : XSET_B_FALSE;
        set = xset_set_cb( "sortby_owner", on_popup_sortby, browser );
            xset_set_ob1_int( set, "sortorder", PTK_FB_SORT_BY_OWNER );
            xset_set_ob2( set, NULL, set_radio );
            set->b = browser->sort_order == PTK_FB_SORT_BY_OWNER ?
                                                    XSET_B_TRUE : XSET_B_FALSE;
        set = xset_set_cb( "sortby_date", on_popup_sortby, browser );
            xset_set_ob1_int( set, "sortorder", PTK_FB_SORT_BY_MTIME );
            xset_set_ob2( set, NULL, set_radio );
            set->b = browser->sort_order == PTK_FB_SORT_BY_MTIME ?
                                                    XSET_B_TRUE : XSET_B_FALSE;

        set = xset_set_cb( "sortby_ascend", on_popup_sortby, browser );
            xset_set_ob1_int( set, "sortorder", -1 );
            xset_set_ob2( set, NULL, NULL );
            set->b = browser->sort_type == GTK_SORT_ASCENDING ?
                                                    XSET_B_TRUE : XSET_B_FALSE;
            set_radio = set;
        set = xset_set_cb( "sortby_descend", on_popup_sortby, browser );
            xset_set_ob1_int( set, "sortorder", -2 );
            xset_set_ob2( set, NULL, set_radio );
            set->b = browser->sort_type == GTK_SORT_DESCENDING ?
                                                    XSET_B_TRUE : XSET_B_FALSE;

        set = xset_set_cb( "sortx_natural", on_popup_sort_extra, browser );
            set->b = PTK_FILE_LIST( browser->file_list )->sort_natural ? 
                                                    XSET_B_TRUE : XSET_B_FALSE;
        set = xset_set_cb( "sortx_case", on_popup_sort_extra, browser );
            set->b = PTK_FILE_LIST( browser->file_list )->sort_case ?
                                                    XSET_B_TRUE : XSET_B_FALSE;
            set->disable = !PTK_FILE_LIST( browser->file_list )->sort_natural;

        set = xset_set_cb( "sortx_folders", on_popup_sort_extra, browser );
            xset_set_ob2( set, NULL, NULL );
            set->b = PTK_FILE_LIST( browser->file_list )->sort_dir == 
                            PTK_LIST_SORT_DIR_FIRST ? XSET_B_TRUE : XSET_B_FALSE;
            set_radio = set;
        set = xset_set_cb( "sortx_files", on_popup_sort_extra, browser );
            xset_set_ob2( set, NULL, set_radio );
            set->b = PTK_FILE_LIST( browser->file_list )->sort_dir == 
                            PTK_LIST_SORT_DIR_LAST ? XSET_B_TRUE : XSET_B_FALSE;
        set = xset_set_cb( "sortx_mix", on_popup_sort_extra, browser );
            xset_set_ob2( set, NULL, set_radio );
            set->b = PTK_FILE_LIST( browser->file_list )->sort_dir == 
                            PTK_LIST_SORT_DIR_MIXED ? XSET_B_TRUE : XSET_B_FALSE;

        set = xset_set_cb( "sortx_hidfirst", on_popup_sort_extra, browser );
            xset_set_ob2( set, NULL, NULL );
            set->b = PTK_FILE_LIST( browser->file_list )->sort_hidden_first ?
                                                    XSET_B_TRUE : XSET_B_FALSE;
            set_radio = set;
        set = xset_set_cb( "sortx_hidlast", on_popup_sort_extra, browser );
            xset_set_ob2( set, NULL, set_radio );
            set->b = PTK_FILE_LIST( browser->file_list )->sort_hidden_first ? 
                                                    XSET_B_FALSE : XSET_B_TRUE;

        xset_set_cb_panel( p, "font_file", main_update_fonts, browser );
        set = xset_get( "view_list_style" );
        desc = g_strdup_printf( "panel%d_list_detailed panel%d_list_compact panel%d_list_icons sep_v5 rubberband sep_v6 panel%d_font_file",
                                        p, p, p, p );
        xset_set_set( set, "desc", desc );
        g_free( desc );
        set = xset_get( "view_fonts" );
        desc = g_strdup_printf( "panel%d_font_device panel%d_font_dir panel%d_font_book panel%d_font_files panel%d_font_tabs panel%d_font_status panel%d_font_pathbar",
                                        p, p, p, p, p, p ,p );
        xset_set_set( set, "desc", desc );
        g_free( desc );
        set = xset_get( "con_view" );
        desc = g_strdup_printf( "panel%d_show_toolbox panel%d_show_sidebar panel%d_show_devmon panel%d_show_book panel%d_show_dirtree sep_v7 panel%d_show_hidden view_list_style view_sortby view_columns sep_v8 view_refresh",
                                        p, p, p, p, p, p );
        xset_set_set( set, "desc", desc );
        g_free( desc );
        xset_add_menuitem( desktop, browser, popup, accel_group, set );
    }

    // Properties
    if ( browser )
    {
        set = xset_set_cb( "prop_info", on_popup_file_properties_activate, data );
        set = xset_set_cb( "prop_perm", on_popup_file_permissions_activate, data );
        
        static const char* permcmd[] =
        {
            "perm_r",
            "perm_rw",
            "perm_rwx",
            "perm_r_r",
            "perm_rw_r",
            "perm_rw_rw",
            "perm_rwxr_x",
            "perm_rwxrwx",
            "perm_r_r_r",
            "perm_rw_r_r",
            "perm_rw_rw_rw",
            "perm_rwxr_r",
            "perm_rwxr_xr_x",
            "perm_rwxrwxrwx",
            "perm_rwxrwxrwt",
            "perm_unstick",
            "perm_stick",
            "perm_go_w",
            "perm_go_rwx",
            "perm_ugo_w",
            "perm_ugo_rx",
            "perm_ugo_rwx",
            "rperm_rw",
            "rperm_rwx",
            "rperm_rw_r",
            "rperm_rw_rw",
            "rperm_rwxr_x",
            "rperm_rwxrwx",
            "rperm_rw_r_r",
            "rperm_rw_rw_rw",
            "rperm_rwxr_r",
            "rperm_rwxr_xr_x",
            "rperm_rwxrwxrwx",
            "rperm_rwxrwxrwt",
            "rperm_unstick",
            "rperm_stick",
            "rperm_go_w",
            "rperm_go_rwx",
            "rperm_ugo_w",
            "rperm_ugo_rx",
            "rperm_ugo_rwx",
            "own_myuser",
            "own_myuser_users",
            "own_user1",
            "own_user1_users",
            "own_user2",
            "own_user2_users",
            "own_root",
            "own_root_users",
            "own_root_myuser",
            "own_root_user1",
            "own_root_user2",
            "rown_myuser",
            "rown_myuser_users",
            "rown_user1",
            "rown_user1_users",
            "rown_user2",
            "rown_user2_users",
            "rown_root",
            "rown_root_users",
            "rown_root_myuser",
            "rown_root_user1",
            "rown_root_user2"
        };
        for ( i = 0; i < G_N_ELEMENTS( permcmd ); i++ )
        {
            set = xset_set_cb( permcmd[i], on_permission, data );
            xset_set_ob1( set, "set", set );
        }

        set = xset_get( "prop_quick" );
        set->disable = no_write_access || !sel_files;
        
        set = xset_get( "prop_root" );
        set->disable = !sel_files;
        
        set = xset_get( "con_prop" );
        if ( geteuid() == 0 )
            desc = g_strdup_printf( "prop_info prop_perm prop_root" );
        else
            desc = g_strdup_printf( "prop_info prop_perm prop_quick prop_root" );
        xset_set_set( set, "desc", desc );
        g_free( desc );
        xset_add_menuitem( desktop, browser, popup, accel_group, set );
    }
    else if ( desktop )
    {
        // Desktop|Devices
        set = xset_get( "desk_dev" );
        item = GTK_MENU_ITEM( xset_add_menuitem( desktop, NULL, popup,
                                                        accel_group, set ) );
        submenu = gtk_menu_item_get_submenu( item );
        
        ptk_location_view_dev_menu( GTK_WIDGET( desktop ), NULL, submenu );
#ifndef HAVE_HAL
        set = xset_get( "sep_dm3" );
        xset_add_menuitem( desktop, NULL, submenu, accel_group, set );

        set = xset_get( "dev_menu_settings" );
        xset_add_menuitem( desktop, NULL, submenu, accel_group, set );
#endif

        // Desktop|Bookmarks
        set = xset_get( "desk_book" );
        GtkMenuItem* book_item = GTK_MENU_ITEM( xset_add_menuitem( desktop,
                                                        NULL, popup,
                                                        accel_group, set ) );
        submenu = gtk_menu_item_get_submenu( book_item );

        GtkWidget* folder_image;
        XSet* set = xset_get( "book_icon" );
        const char* book_icon = set->icon;
        int count = 0;
        GList* l;
        for ( l = app_settings.bookmarks->list; l; l = l->next )
        {
            item = GTK_MENU_ITEM( gtk_image_menu_item_new_with_label(
                                                            (char*)l->data ) );
            if ( book_icon )
                folder_image = xset_get_image( book_icon, GTK_ICON_SIZE_MENU );
            else
                folder_image = NULL;
            if ( !folder_image )
                folder_image = xset_get_image( "gtk-directory", GTK_ICON_SIZE_MENU );
            if ( folder_image )
                gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( item ),
                                                            folder_image );
            g_signal_connect( item, "activate", G_CALLBACK( on_bookmark_activate ),
                                                        (char*)l->data );
            gtk_menu_shell_append( GTK_MENU_SHELL( submenu ), GTK_WIDGET( item ) );
            if ( ++count > 200 )
                break;
        }
        if ( count == 0 )
            gtk_widget_set_sensitive( GTK_WIDGET( book_item ), FALSE );
            
        // Desktop|Icons >
        set = xset_set_cb( "desk_sort_name", on_popup_desktop_sort_activate,
                                                                    desktop );
            set->b = app_settings.desktop_sort_by == DW_SORT_BY_NAME ? 
                                                XSET_B_TRUE : XSET_B_FALSE;
            xset_set_ob2( set, NULL, NULL );
            set_radio = set;
        set = xset_set_cb( "desk_sort_type", on_popup_desktop_sort_activate,
                                                                    desktop );
            set->b = app_settings.desktop_sort_by == DW_SORT_BY_TYPE ? 
                                                XSET_B_TRUE : XSET_B_FALSE;
            xset_set_ob2( set, NULL, set_radio );
        set = xset_set_cb( "desk_sort_date", on_popup_desktop_sort_activate,
                                                                    desktop );
            set->b = app_settings.desktop_sort_by == DW_SORT_BY_MTIME ? 
                                                XSET_B_TRUE : XSET_B_FALSE;
            xset_set_ob2( set, NULL, set_radio );
        set = xset_set_cb( "desk_sort_size", on_popup_desktop_sort_activate,
                                                                    desktop );
            set->b = app_settings.desktop_sort_by == DW_SORT_BY_SIZE ? 
                                                XSET_B_TRUE : XSET_B_FALSE;
            xset_set_ob2( set, NULL, set_radio );

        set = xset_set_cb( "desk_sort_cust", on_popup_desktop_sort_activate,
                                                                    desktop );
            set->b = app_settings.desktop_sort_by == DW_SORT_CUSTOM ? 
                                                XSET_B_TRUE : XSET_B_FALSE;
            xset_set_ob2( set, NULL, set_radio );

        set = xset_set_cb( "desk_sort_ascend", on_popup_desktop_sort_activate,
                                                                    desktop );
            set->b = app_settings.desktop_sort_type == GTK_SORT_ASCENDING ? 
                                                XSET_B_TRUE : XSET_B_FALSE;
            xset_set_ob2( set, NULL, NULL );
            set_radio = set;
            set->disable = app_settings.desktop_sort_by == DW_SORT_CUSTOM;
        set = xset_set_cb( "desk_sort_descend", on_popup_desktop_sort_activate,
                                                                    desktop );
            set->b = app_settings.desktop_sort_type == GTK_SORT_DESCENDING ? 
                                                XSET_B_TRUE : XSET_B_FALSE;
            xset_set_ob2( set, NULL, set_radio );
            set->disable = app_settings.desktop_sort_by == DW_SORT_CUSTOM;

        set = xset_get( "desk_icons" );
        item = GTK_MENU_ITEM( xset_add_menuitem( desktop, NULL, popup,
                                                        accel_group, set ) );

        set = xset_set_cb( "desk_pref", on_popup_desktop_pref_activate, desktop );
        xset_add_menuitem( desktop, NULL, popup, accel_group, set );

        // Desktop|Info
        set = xset_get( "sep_desk2" );
        xset_add_menuitem( desktop, NULL, popup, accel_group, set );

        set = xset_set_cb( "prop_info", on_popup_file_properties_activate, data );
        xset_add_menuitem( desktop, NULL, popup, accel_group, set );
    }
    
    gtk_widget_show_all( GTK_WIDGET( popup ) );

    g_signal_connect( popup, "selection-done",
                      G_CALLBACK( gtk_widget_destroy ), NULL );
    g_signal_connect (popup, "key-press-event",
                    G_CALLBACK (xset_menu_keypress), NULL );
    return popup;
}

void
on_popup_open_activate ( GtkMenuItem *menuitem,
                         PtkFileMenu* data )
{
    GList* sel_files = data->sel_files;
    if( ! sel_files )
        sel_files = g_list_prepend( sel_files, data->info );
    ptk_open_files_with_app( data->cwd, sel_files,
                             NULL, data->browser, TRUE, FALSE );  //MOD
    if( sel_files != data->sel_files )
        g_list_free( sel_files );
}

void
on_popup_open_with_another_activate ( GtkMenuItem *menuitem,
                                      PtkFileMenu* data )
{
    char * app = NULL;
    VFSMimeType* mime_type;

    if ( data->info )
    {
        mime_type = vfs_file_info_get_mime_type( data->info );
        if ( G_LIKELY( ! mime_type ) )
        {
            mime_type = vfs_mime_type_get_from_type( XDG_MIME_TYPE_UNKNOWN );
        }
    }
    else
    {
        mime_type = vfs_mime_type_get_from_type( XDG_MIME_TYPE_DIRECTORY );
    }

    GtkWidget* parent_win;
    if ( data->browser )
        parent_win = gtk_widget_get_toplevel( GTK_WIDGET( data->browser ) );
    else
        parent_win = GTK_WIDGET( data->desktop );
    app = (char *) ptk_choose_app_for_mime_type( GTK_WINDOW( parent_win ),
                                        mime_type, FALSE, TRUE, TRUE, FALSE );
    if ( app )
    {
        GList* sel_files = data->sel_files;
        if( ! sel_files )
            sel_files = g_list_prepend( sel_files, data->info );
        ptk_open_files_with_app( data->cwd, sel_files,
                                app, data->browser, FALSE, FALSE );
        if( sel_files != data->sel_files )
            g_list_free( sel_files );
        g_free( app );
    }
    vfs_mime_type_unref( mime_type );
}

void on_popup_open_all( GtkMenuItem *menuitem, PtkFileMenu* data )
{
    if ( xset_opener( NULL, data->browser, 1 ) )
        return;

    GList* sel_files;

    sel_files = data->sel_files;
    if( ! sel_files )
        sel_files = g_list_prepend( sel_files, data->info );
    ptk_open_files_with_app( data->cwd, sel_files,
                             NULL, FALSE /*allow MIME app - was data->browser*/,
                             FALSE, TRUE );
    if( sel_files != data->sel_files )
        g_list_free( sel_files );
}

void on_popup_run_app( GtkMenuItem *menuitem, PtkFileMenu* data )
{
    VFSAppDesktop * desktop_file;
    const char* app = NULL;
    GList* sel_files;

    desktop_file = ( VFSAppDesktop* ) g_object_get_data( G_OBJECT( menuitem ),
                                                         "desktop_file" );
    if ( !desktop_file )
        return ;

    app = vfs_app_desktop_get_name( desktop_file );
    sel_files = data->sel_files;
    if( ! sel_files )
        sel_files = g_list_prepend( sel_files, data->info );
    ptk_open_files_with_app( data->cwd, sel_files,
                             (char *) app, data->browser, FALSE, FALSE ); //MOD
    if( sel_files != data->sel_files )
        g_list_free( sel_files );
}

enum {
    APP_JOB_NONE,
    APP_JOB_DEFAULT,
    APP_JOB_REMOVE,
    APP_JOB_EDIT,
    APP_JOB_EDIT_LIST,
    APP_JOB_ADD,
    APP_JOB_BROWSE,
    APP_JOB_BROWSE_SHARED,
    APP_JOB_EDIT_TYPE,
    APP_JOB_VIEW,
    APP_JOB_VIEW_TYPE,
    APP_JOB_VIEW_OVER,
    APP_JOB_UPDATE,
    APP_JOB_BROWSE_MIME,
    APP_JOB_BROWSE_MIME_USR,
    APP_JOB_HELP,
    APP_JOB_USR
};

char* get_shared_desktop_file_location( const char* name )
{
    const gchar* const * dirs;
    char* ret;

    dirs = g_get_system_data_dirs();
    for ( ; *dirs; ++dirs )
    {
        if ( ret = vfs_mime_type_locate_desktop_file( *dirs, name ) )
            return ret;
    }
    return NULL;
}

void app_job( GtkWidget* item, GtkWidget* app_item )
{
    char* path;
    char* str;
    
    VFSAppDesktop* desktop_file = ( VFSAppDesktop* ) g_object_get_data( 
                                        G_OBJECT( app_item ), "desktop_file" );
    if ( !( desktop_file && desktop_file->file_name ) )
        return;
    
    int job = GPOINTER_TO_INT( g_object_get_data( G_OBJECT(item), "job" ) );
    PtkFileMenu* data = (PtkFileMenu*)g_object_get_data( G_OBJECT(item), "data" );
    if ( !( data && data->info ) )
        return;

    VFSMimeType* mime_type = vfs_file_info_get_mime_type( data->info );
    if ( !mime_type )
        mime_type = vfs_mime_type_get_from_type( XDG_MIME_TYPE_UNKNOWN );

    switch ( job ) {
    case APP_JOB_DEFAULT:
        vfs_mime_type_set_default_action( mime_type, desktop_file->file_name );
        break;
    case APP_JOB_REMOVE:
        // for text files, spacefm displays both the actions for the type
        // and the actions for text/plain, so removing an app may appear to not
        // work if that app is still associated with text/plain
        vfs_mime_type_remove_action( mime_type, desktop_file->file_name );
        if ( strcmp( mime_type->type, "text/plain" ) && 
                                g_str_has_prefix( mime_type->type, "text/" ) )
            xset_msg_dialog( data->browser ? GTK_WIDGET( data->browser ) : GTK_WIDGET( data->desktop ), 0, _("Remove Text Type Association"), NULL, 0, _("NOTE:  When compiling the list of applications to appear in the Open submenu for a text file, SpaceFM will include applications associated with the MIME type (eg text/html) AND applications associated with text/plain.  If you select Remove on an application, it will be removed as an associated application for the MIME type (eg text/html), but will NOT be removed as an associated application for text/plain (unless the MIME type is text/plain).  Thus using Remove may not remove the application from the Open submenu for this type, unless you also remove it from text/plain."), NULL, "#designmode-mime-remove" );
        break;
    case APP_JOB_EDIT:
        path = g_build_filename( g_get_user_data_dir(), "applications",
                                                    desktop_file->file_name, NULL );
        if ( !g_file_test( path, G_FILE_TEST_EXISTS ) )
        {
            // need to copy
            char* share_desktop = vfs_mime_type_locate_desktop_file( NULL,
                                                        desktop_file->file_name );
            if ( !( share_desktop && strcmp( share_desktop, path ) ) )
            {
                g_free( share_desktop );
                g_free( path );
                return;
            }
            xset_copy_file( share_desktop, path );
            g_free( share_desktop );
            if ( !g_file_test( path, G_FILE_TEST_EXISTS ) )
            {
                g_free( path );
                return;
            }
        }
        xset_edit( data->browser ? GTK_WIDGET( data->browser )
                            : GTK_WIDGET( data->desktop ), path, FALSE, FALSE );        
        g_free( path );
        break;
    case APP_JOB_VIEW:
        path = get_shared_desktop_file_location( desktop_file->file_name );
        if ( path )
            xset_edit( data->browser ? GTK_WIDGET( data->browser )
                            : GTK_WIDGET( data->desktop ), path, FALSE, TRUE );        
        break;
    case APP_JOB_EDIT_LIST:
        path = g_build_filename( g_get_user_data_dir(), "applications",
                                                    "mimeapps.list", NULL );
        xset_edit( data->browser ? GTK_WIDGET( data->browser )
                            : GTK_WIDGET( data->desktop ), path, FALSE, TRUE );        
        g_free( path );
        break;
    case APP_JOB_ADD:
        path = ptk_choose_app_for_mime_type( 
                                GTK_WINDOW( gtk_widget_get_toplevel( data->browser ?
                                                GTK_WIDGET( data->browser ) :
                                                GTK_WIDGET( data->desktop ) ) ),
                                mime_type, FALSE, TRUE, TRUE, TRUE );
        // ptk_choose_app_for_mime_type returns either a bare command that 
        // was already set as default, or a (custom or shared) desktop file
        if ( path && g_str_has_suffix( path, ".desktop" ) && !strchr( path, '/' )
                                                                    && mime_type )
            vfs_mime_type_append_action( mime_type->type, path );
        g_free( path );
        break;
    case APP_JOB_BROWSE:
        path = g_build_filename( g_get_user_data_dir(), "applications", NULL );
        g_mkdir_with_parents( path, 0700 );

        if ( data->browser )
            ptk_file_browser_emit_open( data->browser, path, PTK_OPEN_NEW_TAB );
        break;
    case APP_JOB_BROWSE_SHARED:
        str = get_shared_desktop_file_location( desktop_file->file_name );
        if ( str )
            path = g_path_get_dirname( str );
        else
            path = g_strdup( "/usr/share/applications" );
        g_free( str );
        if ( data->browser )
            ptk_file_browser_emit_open( data->browser, path, PTK_OPEN_NEW_TAB );
        break;
    case APP_JOB_EDIT_TYPE:
        path = g_build_filename( g_get_user_data_dir(), "mime/packages", NULL );
        g_mkdir_with_parents( path, 0700 );
        g_free( path );
        str = replace_string( mime_type->type, "/", "-", FALSE );
        path = str;
        str = g_strdup_printf( "%s.xml", path );
        g_free( path );
        path = g_build_filename( g_get_user_data_dir(), "mime/packages", str, NULL );
        g_free( str );
        if ( !g_file_test( path, G_FILE_TEST_EXISTS ) )
        {
            // need to create
            char* msg = g_strdup_printf( "<?xml version='1.0' encoding='utf-8'?>\n<mime-info xmlns='http://www.freedesktop.org/standards/shared-mime-info'>\n<mime-type type='%s'>\n\n<!-- This file was generated by SpaceFM to allow you to change the name of the\n     above mime type and to change the filename or magic patterns that define\n     this type.\n     \n     IMPORTANT:  After saving this file, restart SpaceFM.  You may need to run:\n         update-mime-database ~/.local/share/mime\n\n     Delete this file from ~/.local/share/mime/packages/ to revert to default.\n     \n     To make this definition file apply to all users, copy this file to \n     /usr/share/mime/packages/ and:  sudo update-mime-database /usr/share/mime\n\n     For help editing this file:\n     http://library.gnome.org/admin/system-admin-guide/stable/mimetypes-source-xml.html.en\n     http://www.freedesktop.org/wiki/Specifications/AddingMIMETutor\n\n     Example to define the name of a PNG file (with optional translations):\n\n        <comment>Portable Network Graphics file</comment>\n        <comment xml:lang=\"en\">Portable Network Graphics file</comment>\n     \n     Example to detect PNG files by glob pattern:\n\n        <glob pattern=\"*.png\"/>\n\n     Example to detect PNG files by file contents:\n\n        <magic priority=\"50\">\n            <match type=\"string\" value=\"\\x89PNG\" offset=\"0\"/>  \n        </magic>\n-->", mime_type->type );
            
            // build from /usr/share/mime type ?
            str = g_strdup_printf( "%s.xml", mime_type->type );
            char* usr_path = g_build_filename( "/usr/share/mime", str, NULL );
            g_free( str );
            char* contents = NULL;
            if ( g_file_get_contents ( usr_path, &contents, NULL, NULL ) )
            {
                char* start = NULL;
                if ( str = strstr( contents, "\n<mime-type " ) )
                {
                    if ( str = strstr( str, ">\n" ) )
                    {
                        str[1] = '\0';
                        start = contents;
                        if ( str = strstr( str + 2, "<!--Created automatically" ) )
                        {
                            if ( str = strstr( str, "-->" ) )
                                start = str + 4;
                        }
                    }
                }
                if ( start )
                    str = g_strdup_printf( "%s\n\n%s</mime-info>\n", msg, start );
                else
                    str = NULL;
                g_free( contents );
                contents = str;
            }
            g_free( usr_path );
            
            if ( !contents )
                contents = g_strdup_printf( "%s\n\n<!-- insert your patterns below -->\n\n\n</mime-type>\n</mime-info>\n\n", msg );
            g_free( msg );
            
            // write file
            FILE* file = fopen( path, "w" );
            if ( file )
            {
                fputs( contents, file );
                fclose( file );
            }
            g_free( contents );
        }
        if ( g_file_test( path, G_FILE_TEST_EXISTS ) )
        {
            xset_edit( data->browser ? GTK_WIDGET( data->browser )
                            : GTK_WIDGET( data->desktop ), path, FALSE, FALSE );        
        }
        g_free( path );
        vfs_dir_monitor_mime();
        break;
    case APP_JOB_VIEW_TYPE:
        str = g_strdup_printf( "%s.xml", mime_type->type );
        path = g_build_filename( "/usr/share/mime", str, NULL );
        g_free( str );
        if ( g_file_test( path, G_FILE_TEST_EXISTS ) )
        {
            xset_edit( data->browser ? GTK_WIDGET( data->browser )
                            : GTK_WIDGET( data->desktop ), path, FALSE, TRUE );        
        }
        g_free( path );
        break;
    case APP_JOB_VIEW_OVER:
        path = "/usr/share/mime/packages/Overrides.xml";
        xset_edit( data->browser ? GTK_WIDGET( data->browser )
                        : GTK_WIDGET( data->desktop ), path, TRUE, FALSE );
        break;
    case APP_JOB_BROWSE_MIME_USR:
        if ( data->browser )
            ptk_file_browser_emit_open( data->browser, "/usr/share/mime/packages",
                                                            PTK_OPEN_NEW_TAB );
        break;
    case APP_JOB_BROWSE_MIME:
        path = g_build_filename( g_get_user_data_dir(), "mime/packages", NULL );
        g_mkdir_with_parents( path, 0700 );
        if ( data->browser )
            ptk_file_browser_emit_open( data->browser, path, PTK_OPEN_NEW_TAB );
        vfs_dir_monitor_mime();
        break;
    case APP_JOB_UPDATE:
        path = g_strdup_printf( "update-mime-database %s/mime",
                                                        g_get_user_data_dir() );
        g_spawn_command_line_async( path, NULL );
        g_free( path );
        path = g_strdup_printf( "update-desktop-database %s/applications",
                                                        g_get_user_data_dir() );
        g_spawn_command_line_async( path, NULL );
        g_free( path );
        break;
    case APP_JOB_HELP:
            xset_show_help( data->browser ? GTK_WIDGET( data->browser ) :
                                        GTK_WIDGET( data->desktop ),
                                        NULL, "#designmode-mime" );
        break;
    }
    if ( mime_type )
        vfs_mime_type_unref( mime_type );
}

gboolean app_menu_keypress( GtkWidget* menu, GdkEventKey* event,
                                                            PtkFileMenu* data )
{
    int job = -1;
    XSet* set;
    PtkFileMenu* app_data = NULL;
    VFSAppDesktop* desktop_file = NULL;
    
    GtkWidget* item = gtk_menu_shell_get_selected_item( GTK_MENU_SHELL( menu ) );
    if ( item )
    {
        // if original menu, desktop_file will be set
        desktop_file = ( VFSAppDesktop* ) g_object_get_data( 
                                    G_OBJECT( item ), "desktop_file" );
        // else if app menu, data will be set
        app_data = (PtkFileMenu*)g_object_get_data( G_OBJECT(item), "data" );

        if ( !desktop_file && !app_data )
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
            char* help = NULL;
            if ( app_data )
            {
                job = GPOINTER_TO_INT( g_object_get_data( G_OBJECT(item), "job" ) );
                switch ( job ) {
                case APP_JOB_DEFAULT:
                    help = "#designmode-mime-set";
                    break;
                case APP_JOB_REMOVE:
                    help = "#designmode-mime-remove";
                    break;
                case APP_JOB_ADD:
                    help = "#designmode-mime-add";
                    break;
                case APP_JOB_EDIT:
                    help = "#designmode-mime-appdesktop";
                    break;
                case APP_JOB_EDIT_LIST:
                    help = "#designmode-mime-mimeappslist";
                    break;
                case APP_JOB_BROWSE:
                    help = "#designmode-mime-appdir";
                    break;
                case APP_JOB_EDIT_TYPE:
                    help = "#designmode-mime-xml";
                    break;
                case APP_JOB_BROWSE_MIME:
                    help = "#designmode-mime-mimedir";
                    break;
                case APP_JOB_USR:
                    help = "#designmode-mime-usr";
                    break;
                case APP_JOB_BROWSE_SHARED:
                case APP_JOB_VIEW:
                case APP_JOB_VIEW_TYPE:
                case APP_JOB_VIEW_OVER:
                case APP_JOB_BROWSE_MIME_USR:
                    help = "#designmode-mime-usr";
                    break;
                }
            }
            if ( !help )
                help = "#designmode-mime";
            gtk_menu_shell_deactivate( GTK_MENU_SHELL( menu ) );
            xset_show_help( NULL, NULL, help );
            return TRUE;
        }
        else if ( desktop_file && event->keyval == GDK_KEY_F2 )
        {
            show_app_menu( menu, item, data, 0, event->time );
            return TRUE;
        }
        else if ( event->keyval == GDK_KEY_F4 )
            job = APP_JOB_EDIT;
        else if ( event->keyval == GDK_KEY_Delete )
            job = APP_JOB_REMOVE;
        else if ( event->keyval == GDK_KEY_Insert )
            job = APP_JOB_ADD;
    }
    if ( desktop_file && job != -1 )
    {
        gtk_menu_shell_deactivate( GTK_MENU_SHELL( menu ) );
        g_object_set_data( G_OBJECT( item ), "job", GINT_TO_POINTER( job ) );
        g_object_set_data( G_OBJECT( item ), "data", data );
        app_job( item, item );
        return TRUE;
    }
    return FALSE;
}

void on_app_menu_hide(GtkWidget *widget, GtkWidget* app_menu )
{
    gtk_widget_set_sensitive( widget, TRUE );
    gtk_menu_shell_deactivate( GTK_MENU_SHELL( app_menu ) );
}

GtkWidget* app_menu_additem( GtkWidget* menu, char* label, gchar* stock_icon,
                                int job, GtkWidget* app_item, PtkFileMenu* data )
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
    g_object_set_data( G_OBJECT(item), "data", data );
    gtk_container_add ( GTK_CONTAINER ( menu ), item );
    g_signal_connect( item, "activate", G_CALLBACK( app_job ), app_item );
    return item;
}

static void show_app_menu( GtkWidget* menu, GtkWidget* app_item, PtkFileMenu* data, 
                                                    guint button, guint32 time )
{
    GtkWidget* newitem;
    GtkWidget* submenu;
    char* str;
    char* str2;
    char* path;
    char* icon;
    const char* type;

    if ( !( data && data->info ) )
        return;
    
    VFSMimeType* mime_type = vfs_file_info_get_mime_type( data->info );
    if ( mime_type )
    {
        type = vfs_mime_type_get_type( mime_type );
        vfs_mime_type_unref( mime_type );
    }
    else
        type = "unknown";

    VFSAppDesktop* desktop_file = ( VFSAppDesktop* ) g_object_get_data( 
                                    G_OBJECT( app_item ), "desktop_file" );
    if ( !desktop_file )
        return ;

    GtkWidget* app_menu = gtk_menu_new();
    GtkAccelGroup* accel_group = gtk_accel_group_new();

    // Set Default
    newitem = app_menu_additem( app_menu, _("_Set As Default"),
                                GTK_STOCK_SAVE, APP_JOB_DEFAULT, app_item, data );
    //gtk_widget_add_accelerator( newitem, "activate", accel_group,
    //                        GDK_k, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

    // Remove
    newitem = app_menu_additem( app_menu, _("_Remove"),
                                GTK_STOCK_DELETE, APP_JOB_REMOVE, app_item, data );

    // Add
    newitem = app_menu_additem( app_menu, _("_Add..."),
                                GTK_STOCK_ADD, APP_JOB_ADD, app_item, data );
                                
    // Separator
    gtk_container_add ( GTK_CONTAINER ( app_menu ), gtk_separator_menu_item_new() );

    // *.desktop (missing)
    if ( desktop_file->file_name )
    {
        path = g_build_filename( g_get_user_data_dir(), "applications",
                                                    desktop_file->file_name, NULL );
        if ( g_file_test( path, G_FILE_TEST_EXISTS ) )
        {
            str = replace_string( desktop_file->file_name, ".desktop", "._desktop",
                                                                        FALSE );
            icon = GTK_STOCK_EDIT;
        }
        else
        {
            str2 = replace_string( desktop_file->file_name, ".desktop", "._desktop",
                                                                        FALSE );
            str = g_strdup_printf( "%s (*%s)", str2, _("copy") );
            g_free( str2 );
            icon = GTK_STOCK_NEW;
        }
        newitem = app_menu_additem( app_menu, str,
                                    icon, APP_JOB_EDIT, app_item, data );
        g_free( str );
        g_free( path );
    }

    // mimeapps.list
    newitem = app_menu_additem( app_menu, "_mimeapps.list",
                                GTK_STOCK_EDIT, APP_JOB_EDIT_LIST, app_item, data );

    // applications/
    newitem = app_menu_additem( app_menu, "appli_cations/",
                                GTK_STOCK_DIRECTORY, APP_JOB_BROWSE, app_item, data );
    gtk_widget_set_sensitive( GTK_WIDGET( newitem ), !!data->browser );    

    // Separator
    gtk_container_add ( GTK_CONTAINER ( app_menu ), gtk_separator_menu_item_new() );

    // *.xml (missing)
    str = replace_string( type, "/", "-", FALSE );
    path = str;
    str = g_strdup_printf( "%s.xml", path );
    g_free( path );
    path = g_build_filename( g_get_user_data_dir(), "mime/packages", str, NULL );
    if ( path && g_file_test( path, G_FILE_TEST_EXISTS ) )
    {
        g_free( path );
        str = replace_string( type, "/", "-", FALSE );
        path = str;
        str = g_strdup_printf( "%s._xml", path );
        g_free( path );
        icon = GTK_STOCK_EDIT;
    }
    else
    {
        g_free( path );
        str = replace_string( type, "/", "-", FALSE );
        path = str;
        str = g_strdup_printf( "%s._xml (*%s)", path, _("new") );
        g_free( path );
        icon = GTK_STOCK_NEW;
    }
    newitem = app_menu_additem( app_menu, str,
                                icon, APP_JOB_EDIT_TYPE, app_item, data );
    
    // mime/packages/
    newitem = app_menu_additem( app_menu, "mime/pac_kages/",
                                GTK_STOCK_DIRECTORY, APP_JOB_BROWSE_MIME, app_item, data );
    gtk_widget_set_sensitive( GTK_WIDGET( newitem ), !!data->browser );    

    // Run update-mime-database   (now done automatically)
    //str = g_strdup_printf( "%s update-mime-database", _("Ru_n") );
    //newitem = app_menu_additem( app_menu, str,
    //                            GTK_STOCK_EXECUTE, APP_JOB_UPDATE, app_item, data );
    //g_free( str );

    // Separator
    gtk_container_add ( GTK_CONTAINER ( app_menu ), gtk_separator_menu_item_new() );

    // /usr submenu
    newitem = gtk_image_menu_item_new_with_mnemonic( "/_usr" );
    submenu = gtk_menu_new();
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( newitem ), submenu );
    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM( newitem ), 
          gtk_image_new_from_stock( GTK_STOCK_DIRECTORY, GTK_ICON_SIZE_MENU ) );
    gtk_container_add ( GTK_CONTAINER ( app_menu ), newitem );
    g_object_set_data( G_OBJECT( newitem ), "job", GINT_TO_POINTER( APP_JOB_USR ) );
    g_object_set_data( G_OBJECT( newitem ), "data", data );
    g_signal_connect( submenu, "key_press_event",
                      G_CALLBACK( app_menu_keypress ), data );

        // View /usr .desktop
        if ( desktop_file->file_name )
        {
            newitem = app_menu_additem( submenu, desktop_file->file_name,
                                        GTK_STOCK_FILE, APP_JOB_VIEW, app_item, data );
            path = get_shared_desktop_file_location( desktop_file->file_name );

            gtk_widget_set_sensitive( GTK_WIDGET( newitem ), !!path );
            g_free( path );
        }

        // /usr applications/
        newitem = app_menu_additem( submenu, "appli_cations/",
                                    GTK_STOCK_DIRECTORY, APP_JOB_BROWSE_SHARED,
                                    app_item, data );
        gtk_widget_set_sensitive( GTK_WIDGET( newitem ), !!data->browser );    

        // Separator
        gtk_container_add ( GTK_CONTAINER ( submenu ), gtk_separator_menu_item_new() );

        // /usr *.xml
        str = g_strdup_printf( "%s.xml", type );
        path = g_build_filename( "/usr/share/mime", str, NULL );
        g_free( str );
        str = g_strdup_printf( "%s._xml", type );
        newitem = app_menu_additem( submenu, str,
                                    GTK_STOCK_FILE, APP_JOB_VIEW_TYPE, app_item,
                                                                        data );
        g_free( str );
        gtk_widget_set_sensitive( GTK_WIDGET( newitem ),
                                        g_file_test( path, G_FILE_TEST_EXISTS ) );    
        g_free( path );

        // /usr *Overrides.xml
        newitem = app_menu_additem( submenu, "_Overrides.xml",
                                    GTK_STOCK_EDIT, APP_JOB_VIEW_OVER, app_item,
                                                                        data );
        gtk_widget_set_sensitive( GTK_WIDGET( newitem ),
                            g_file_test( "/usr/share/mime/packages/Overrides.xml",
                            G_FILE_TEST_EXISTS ) );    

        // mime/packages/
        newitem = app_menu_additem( submenu, "mime/pac_kages/",
                    GTK_STOCK_DIRECTORY, APP_JOB_BROWSE_MIME_USR, app_item, data );
        gtk_widget_set_sensitive( GTK_WIDGET( newitem ), !!data->browser &&
                    g_file_test( "/usr/share/mime/packages", G_FILE_TEST_IS_DIR ) );    

    // Separator
    gtk_container_add ( GTK_CONTAINER ( app_menu ), gtk_separator_menu_item_new() );

    // Help
    newitem = app_menu_additem( app_menu, "_Help",
                                GTK_STOCK_HELP, APP_JOB_HELP,
                                app_item, data );

    // show menu
    gtk_widget_show_all( GTK_WIDGET( app_menu ) );
    gtk_menu_popup( GTK_MENU( app_menu ), GTK_WIDGET( menu ), app_item, NULL, NULL,
                                                                button, time );
    gtk_widget_set_sensitive( GTK_WIDGET( menu ), FALSE );
    
    g_signal_connect( menu, "hide", G_CALLBACK( on_app_menu_hide ), app_menu );
    g_signal_connect( app_menu, "selection-done",
                      G_CALLBACK( gtk_widget_destroy ), NULL );
    g_signal_connect( app_menu, "key_press_event",
                      G_CALLBACK( app_menu_keypress ), data );
}

gboolean on_app_button_press( GtkWidget* item, GdkEventButton* event,
                                                            PtkFileMenu* data )
{
    int job = -1;
        
    GtkWidget* menu = (GtkWidget*)g_object_get_data( G_OBJECT(item), "menu" );
    int keymod = ( event->state & ( GDK_SHIFT_MASK | GDK_CONTROL_MASK |
                 GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK ) );

    if ( event->type == GDK_BUTTON_RELEASE )
    {
        if ( event->button == 1 && keymod == 0 )
        {
            // user released left button - due to an apparent gtk bug, activate
            // doesn't always fire on this event so handle it ourselves
            // see also settings.c xset_design_cb()
            // test: gtk2 Crux theme with touchpad on Edit|Copy To|Location
            // https://github.com/IgnorantGuru/spacefm/issues/31
            // https://github.com/IgnorantGuru/spacefm/issues/228
            if ( menu )
                gtk_menu_shell_deactivate( GTK_MENU_SHELL( menu ) );
            gtk_menu_item_activate( GTK_MENU_ITEM( item ) );
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
                show_app_menu( menu, item, data, event->button, event->time );
                return TRUE;
            }
        }
/*
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
*/
    }
    else if ( event->button == 2 )
    {
        // middle click
        if ( keymod == 0 )
        {
            // no modifier
            show_app_menu( menu, item, data, event->button, event->time );
            return TRUE;
        }
/*
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
            job = XSET_JOB_CONTEXT;
        }        
*/
    }
/*
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
*/
    return FALSE;  // TRUE won't stop activate on button-press (will on release)
}

void on_popup_open_in_new_tab_activate( GtkMenuItem *menuitem,
                                        PtkFileMenu* data )
{
    GList * sel;
    VFSFileInfo* file;
    char* full_path;

    if ( data->sel_files )
    {
        for ( sel = data->sel_files; sel; sel = sel->next )
        {
            file = ( VFSFileInfo* ) sel->data;
            full_path = g_build_filename( data->cwd,
                                          vfs_file_info_get_name( file ), NULL );
            if ( data->browser && g_file_test( full_path, G_FILE_TEST_IS_DIR ) )
            {
                ptk_file_browser_emit_open( data->browser, full_path, PTK_OPEN_NEW_TAB );
            }
            g_free( full_path );
        }
    }
    else if ( data->browser )
    {
        ptk_file_browser_emit_open( data->browser, data->file_path, PTK_OPEN_NEW_TAB );
    }
}

void on_popup_open_in_new_tab_here( GtkMenuItem *menuitem,
                                        PtkFileMenu* data )
{
    if ( data->browser && data->cwd && g_file_test( data->cwd, G_FILE_TEST_IS_DIR ) )
        ptk_file_browser_emit_open( data->browser, data->cwd, PTK_OPEN_NEW_TAB );
}

/*
void on_popup_open_in_terminal_activate( GtkMenuItem *menuitem,
                                         PtkFileMenu* data )
{
    ptk_file_browser_open_terminal( menuitem, data->browser );
}

void on_popup_run_command( GtkMenuItem *menuitem,
                                         PtkFileMenu* data )
{
    ptk_file_browser_run_command( data->browser );  //MOD Ctrl-r
}

void on_popup_open_files_activate( GtkMenuItem *menuitem,
                                         PtkFileMenu* data )
{
    ptk_file_browser_open_files( data->browser, NULL );  //MOD F4
}

void on_popup_user_6( GtkMenuItem *menuitem,
                                         PtkFileMenu* data )
{
    ptk_file_browser_open_files( data->browser, "/F6" );  //MOD
}

void on_popup_user_7( GtkMenuItem *menuitem,
                                         PtkFileMenu* data )
{
    ptk_file_browser_open_files( data->browser, "/F7" );  //MOD
}

void on_popup_user_8( GtkMenuItem *menuitem,
                                         PtkFileMenu* data )
{
    ptk_file_browser_open_files( data->browser, "/F8" );  //MOD
}

void on_popup_user_9( GtkMenuItem *menuitem,
                                         PtkFileMenu* data )
{
    ptk_file_browser_open_files( data->browser, "/F9" );  //MOD
}

void on_popup_open_in_new_win_activate( GtkMenuItem *menuitem,
                                        PtkFileMenu* data )
{
    GList * sel;
    GList* sel_files = data->sel_files;
    VFSFileInfo* file;
    char* full_path;

    if ( sel_files )
    {
        for ( sel = sel_files; sel; sel = sel->next )
        {
            file = ( VFSFileInfo* ) sel->data;
            full_path = g_build_filename( data->cwd,
                                          vfs_file_info_get_name( file ), NULL );
            if ( g_file_test( full_path, G_FILE_TEST_IS_DIR ) )
            {
                ptk_file_browser_emit_open( data->browser, full_path, PTK_OPEN_NEW_WINDOW );
            }
            g_free( full_path );
        }
    }
    else
    {
        ptk_file_browser_emit_open( data->browser, data->file_path, PTK_OPEN_NEW_WINDOW );
    }
}
*/
void
on_popup_cut_activate ( GtkMenuItem *menuitem,
                        PtkFileMenu* data )
{
    if ( data->sel_files )
        ptk_clipboard_cut_or_copy_files( data->cwd,
                                         data->sel_files, FALSE );
}

void
on_popup_copy_activate ( GtkMenuItem *menuitem,
                         PtkFileMenu* data )
{
    if ( data->sel_files )
        ptk_clipboard_cut_or_copy_files( data->cwd,
                                         data->sel_files, TRUE );
}

void
on_popup_paste_activate ( GtkMenuItem *menuitem,
                          PtkFileMenu* data )
{
/*
    if ( data->sel_files )
    {
        char* dest_dir;
        GtkWidget* parent;
        parent = (GtkWidget*)get_toplevel_win( data );
        dest_dir = g_build_filename( data->cwd,
                                     vfs_file_info_get_name( data->info ), NULL );
        if( ! g_file_test( dest_dir, G_FILE_TEST_IS_DIR ) )
        {
            g_free( dest_dir );
            dest_dir = NULL;
        }
        ptk_clipboard_paste_files( GTK_WINDOW( parent ), dest_dir ? dest_dir : data->cwd,
        data->browser->task_view );
    }
*/
    if ( data->browser )
    {
        GtkWidget* parent_win = gtk_widget_get_toplevel( 
                                            GTK_WIDGET( data->browser ) );
        ptk_clipboard_paste_files( GTK_WINDOW( parent_win ), data->cwd,
                            GTK_TREE_VIEW( data->browser->task_view ), NULL );
    }
#ifdef DESKTOP_INTEGRATION
    else if ( data->desktop )
    {
        desktop_window_set_insert_item( data->desktop );
        ptk_clipboard_paste_files( GTK_WINDOW( data->desktop ), data->cwd, NULL,
                            (GFunc)desktop_window_insert_task_complete );        
    }
#endif
}

void
on_popup_paste_link_activate ( GtkMenuItem *menuitem,
                          PtkFileMenu* data )   //MOD added
{
    if ( data->browser )
        ptk_file_browser_paste_link( data->browser );
#ifdef DESKTOP_INTEGRATION
    else if ( data->desktop )
    {
        desktop_window_set_insert_item( data->desktop );
        ptk_clipboard_paste_links(
            GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( data->desktop ) ) ),
            data->cwd, NULL, (GFunc)desktop_window_insert_task_complete );
    }
#endif
}

void
on_popup_paste_target_activate ( GtkMenuItem *menuitem,
                          PtkFileMenu* data )   //MOD added
{
    if ( data->browser )
        ptk_file_browser_paste_target( data->browser );
#ifdef DESKTOP_INTEGRATION
    else if ( data->desktop )
    {
        desktop_window_set_insert_item( data->desktop );
        ptk_clipboard_paste_targets(
            GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( data->desktop ) ) ),
            data->cwd,
            NULL, (GFunc)desktop_window_insert_task_complete );
    }
#endif
}

void
on_popup_copy_text_activate ( GtkMenuItem *menuitem,
                          PtkFileMenu* data )   //MOD added
{
    ptk_clipboard_copy_as_text( data->cwd, data->sel_files );
}

void
on_popup_copy_name_activate ( GtkMenuItem *menuitem,
                          PtkFileMenu* data )   //MOD added
{
    ptk_clipboard_copy_name( data->cwd, data->sel_files );
}

void
on_popup_copy_parent_activate ( GtkMenuItem *menuitem,
                          PtkFileMenu* data )   //MOD added
{
    if ( data->cwd )
        ptk_clipboard_copy_text( data->cwd );
}

void
on_popup_paste_as_activate ( GtkMenuItem *menuitem,
                          PtkFileMenu* data )   //sfm added
{
#ifdef DESKTOP_INTEGRATION
    if ( data->desktop )
        desktop_window_set_insert_item( data->desktop );
    ptk_file_misc_paste_as( data->desktop, data->browser, data->cwd,
            data->desktop ? (GFunc)desktop_window_insert_task_complete : NULL );
#else
    ptk_file_misc_paste_as( data->desktop, data->browser, data->cwd, NULL );
#endif
}

void
on_popup_delete_activate ( GtkMenuItem *menuitem,
                           PtkFileMenu* data )
{
    if ( data->sel_files )
    {
        if ( data->browser )
        {
            GtkWidget* parent_win = gtk_widget_get_toplevel( 
                                                GTK_WIDGET( data->browser ) );
            ptk_delete_files( GTK_WINDOW( parent_win ),
                              data->cwd,
                              data->sel_files, 
                              GTK_TREE_VIEW( data->browser->task_view ) );
        }
        else if ( data->desktop )
        {
            ptk_delete_files( GTK_WINDOW( data->desktop ),
                              data->cwd,
                              data->sel_files, NULL );            
        }
    }
}

void
on_popup_rename_activate ( GtkMenuItem *menuitem,
                           PtkFileMenu* data )
{
    if ( data->browser )
        ptk_file_browser_rename_selected_files( data->browser, data->sel_files,
                                                                    data->cwd );
#ifdef DESKTOP_INTEGRATION
    else if ( data->desktop && data->sel_files )
    {
        desktop_window_rename_selected_files( data->desktop, data->sel_files,
                                                                    data->cwd );
    }
#endif
}

void on_popup_compress_activate ( GtkMenuItem *menuitem,
                                  PtkFileMenu* data )
{
    if ( data->browser )
        ptk_file_archiver_create( data->browser, data->sel_files, data->cwd );
}

void on_popup_extract_to_activate ( GtkMenuItem *menuitem,
                                    PtkFileMenu* data )
{
    ptk_file_archiver_extract( data->browser, data->sel_files, data->cwd, NULL );
}

void on_popup_extract_here_activate ( GtkMenuItem *menuitem,
                                      PtkFileMenu* data )
{
    ptk_file_archiver_extract( data->browser, data->sel_files,
                                                        data->cwd, data->cwd );
}

void on_popup_extract_list_activate ( GtkMenuItem *menuitem,
                                      PtkFileMenu* data )
{
    ptk_file_archiver_extract( data->browser, data->sel_files,
                                                    data->cwd, "////LIST" );
}

void on_autoopen_create_cb( gpointer task, AutoOpenCreate* ao )
{
    VFSFileInfo* file;
    if ( !ao )
        return;

    if ( ao->path && GTK_IS_WIDGET( ao->file_browser ) 
                                && g_file_test( ao->path, G_FILE_TEST_EXISTS ) )
    {
        char* cwd = g_path_get_dirname( ao->path );

        // select file
        if ( !g_strcmp0( cwd, ptk_file_browser_get_cwd( ao->file_browser ) ) )
        {
            file = vfs_file_info_new();
            vfs_file_info_get( file, ao->path, NULL );
            vfs_dir_emit_file_created( ao->file_browser->dir,
                                    vfs_file_info_get_name( file ), TRUE );
            vfs_file_info_unref( file );
            vfs_dir_flush_notify_cache();
            ptk_file_browser_select_file( ao->file_browser, ao->path );
        }

        // open file
        if ( ao->open_file )
        {
            if ( g_file_test( ao->path, G_FILE_TEST_IS_DIR ) )
            {
                ptk_file_browser_chdir( ao->file_browser, ao->path,
                                                        PTK_FB_CHDIR_ADD_HISTORY );
                ao->path = NULL;
            }
            else
            {
                file = vfs_file_info_new();
                vfs_file_info_get( file, ao->path, NULL );
                GList* sel_files = NULL;
                sel_files = g_list_prepend( sel_files, file );
                ptk_open_files_with_app( cwd, sel_files,
                                         NULL, ao->file_browser, FALSE, TRUE );
                vfs_file_info_unref( file );
                g_list_free( sel_files );
            }
        }
        
        g_free( cwd );
    }
    g_free( ao->path );
    g_slice_free( AutoOpenCreate, ao );
}

static void
create_new_file( PtkFileMenu* data, int create_new )
{
    char* cwd;
    AutoOpenCreate* ao = NULL;
    
    if ( data->cwd )
    {
        if ( data->browser )
        {
            ao = g_slice_new0( AutoOpenCreate );
            ao->path = NULL;
            ao->file_browser = data->browser;
            ao->callback = (GFunc)on_autoopen_create_cb;
            ao->open_file = FALSE;
        }
        else if ( data->desktop )
        {
            ao = g_slice_new0( AutoOpenCreate );
            ao->path = NULL;
            ao->file_browser = (PtkFileBrowser*)data->desktop;  // hack
#ifdef DESKTOP_INTEGRATION
            ao->callback = (GFunc)desktop_window_on_autoopen_cb;
#endif
            ao->open_file = FALSE;            
        }
        int result = ptk_rename_file( data->desktop, data->browser, data->cwd,
                        data->sel_files ? (VFSFileInfo*)data->sel_files->data : NULL,
                        NULL, FALSE, create_new, ao );
        if ( result == 0 )
        {
            ao->file_browser = NULL;
            g_free( ao->path );
            ao->path = NULL;
            g_slice_free( AutoOpenCreate, ao );
            ao = NULL;
        }
    }
}

void
on_popup_new_folder_activate ( GtkMenuItem *menuitem,
                               PtkFileMenu* data )
{
    create_new_file( data, PTK_RENAME_NEW_DIR );
}

void
on_popup_new_text_file_activate ( GtkMenuItem *menuitem,
                                  PtkFileMenu* data )
{
    create_new_file( data, PTK_RENAME_NEW_FILE );
}

void
on_popup_new_link_activate ( GtkMenuItem *menuitem,
                                  PtkFileMenu* data )
{
    create_new_file( data, PTK_RENAME_NEW_LINK );
}

void
on_popup_file_properties_activate ( GtkMenuItem *menuitem,
                                    PtkFileMenu* data )
{
    GtkWidget* parent_win;
    if ( data->browser )
        parent_win = gtk_widget_get_toplevel( GTK_WIDGET( data->browser ) );
    else
        parent_win = GTK_WIDGET( data->desktop );
    ptk_show_file_properties( GTK_WINDOW( parent_win ),
                              data->cwd,
                              data->sel_files, 0 );
}

void
on_popup_file_permissions_activate ( GtkMenuItem *menuitem,
                                    PtkFileMenu* data )
{
    GtkWidget* parent_win;
    if ( data->browser )
        parent_win = gtk_widget_get_toplevel( GTK_WIDGET( data->browser ) );
    else
        parent_win = GTK_WIDGET( data->desktop );
    ptk_show_file_properties( GTK_WINDOW( parent_win ),
                              data->cwd,
                              data->sel_files, 1 );
}

void
on_popup_canon ( GtkMenuItem *menuitem, PtkFileMenu* data )
{
    if ( !data->browser )
        return;

    ptk_file_browser_canon( data->browser, data->file_path ? data->file_path : data->cwd );
}

void ptk_file_menu_action( DesktopWindow* desktop, PtkFileBrowser* browser,
                                                            char* setname )
{
    const char * cwd;
    char* file_path = NULL;
    VFSFileInfo* info;
    GList* sel_files = NULL;
    int i;
    char* xname;
    
    if ( ( !browser && !desktop ) || !setname )
        return;
    XSet* set = xset_get( setname );
    
    // setup data
    if ( browser )
    {
        cwd = ptk_file_browser_get_cwd( browser );
        sel_files = ptk_file_browser_get_selected_files( browser );
    }
    else
    {
        cwd = vfs_get_desktop_dir();
#ifdef DESKTOP_INTEGRATION
        sel_files = desktop_window_get_selected_files( desktop );
#endif
    }
    if( !sel_files )
        info = NULL;
    else
    {
        info = vfs_file_info_ref( (VFSFileInfo*)sel_files->data );
        file_path = g_build_filename( cwd, vfs_file_info_get_name( info ), NULL );
    }
    
    PtkFileMenu* data = g_slice_new0( PtkFileMenu );
    data->cwd = g_strdup( cwd );
    data->browser = browser;
    data->desktop = desktop;

    data->file_path = file_path;
    if ( info )
        data->info = vfs_file_info_ref( info );
    else
        data->info = NULL;
    
    data->sel_files = sel_files;
    data->accel_group = NULL;

    // action
    if ( g_str_has_prefix( set->name, "open_" ) )
    {
        xname = set->name + 5;
        if ( !strcmp( xname, "edit" ) )
            xset_edit( GTK_WIDGET( data->browser ), data->file_path, FALSE, TRUE );
        else if ( !strcmp( xname, "edit_root" ) )
            xset_edit( GTK_WIDGET( data->browser ), data->file_path, TRUE, FALSE );
        else if ( !strcmp( xname, "other" ) )
            on_popup_open_with_another_activate( NULL, data );
        else if ( !strcmp( xname, "execute" ) )
            on_popup_open_activate( NULL, data );
        else if ( !strcmp( xname, "all" ) )
            on_popup_open_all( NULL, data );
    }
    else if ( g_str_has_prefix( set->name, "arc_" ) )
    {
        xname = set->name + 4;
        if ( !strcmp( xname, "extract" ) )
            on_popup_extract_here_activate( NULL, data );
        else if ( !strcmp( xname, "extractto" ) )
            on_popup_extract_to_activate( NULL, data );
        else if ( !strcmp( xname, "extract" ) )
            on_popup_extract_list_activate( NULL, data );
        else if ( !strcmp( xname, "conf" ) )
            ptk_file_archiver_config( browser );
    }
    else if ( g_str_has_prefix( set->name, "iso_" ) )
    {
        xname = set->name + 4;
        if ( !strcmp( xname, "mount" ) )
            on_popup_mount_iso( NULL, data );
    }
    else if ( g_str_has_prefix( set->name, "new_" ) )
    {
        xname = set->name + 4;
        if ( !strcmp( xname, "file" ) )
            on_popup_new_text_file_activate( NULL, data );
        else if ( !strcmp( xname, "folder" ) )
            on_popup_new_folder_activate( NULL, data );
        else if ( !strcmp( xname, "link" ) )
            on_popup_new_link_activate( NULL, data );
        else if ( !strcmp( xname, "bookmark" ) )
            on_add_bookmark( NULL, data );
        else if ( !strcmp( xname, "archive" ) )
        {
            if ( browser )
                on_popup_compress_activate( NULL, data );
        }
    }
    else if ( !strcmp( set->name, "prop_info" ) )
        on_popup_file_properties_activate( NULL, data );
    else if ( !strcmp( set->name, "prop_perm" ) )
        on_popup_file_permissions_activate( NULL, data );
    else if ( g_str_has_prefix( set->name, "edit_" ) )
    {
        xname = set->name + 5;
        if ( !strcmp( xname, "cut" ) )
            on_popup_cut_activate( NULL, data );
        else if ( !strcmp( xname, "copy" ) )
            on_popup_copy_activate( NULL, data );
        else if ( !strcmp( xname, "paste" ) )
            on_popup_paste_activate( NULL, data );
        else if ( !strcmp( xname, "rename" ) )
            on_popup_rename_activate( NULL, data );
        else if ( !strcmp( xname, "delete" ) )
            on_popup_delete_activate( NULL, data );
        else if ( !strcmp( xname, "hide" ) )
            on_hide_file( NULL, data );
        else if ( !strcmp( xname, "canon" ) )
        {
            if ( browser )
                on_popup_canon( NULL, data );
        }
    }
    else if ( !strcmp( set->name, "copy_name" ) )
        on_popup_copy_name_activate( NULL, data );
    else if ( !strcmp( set->name, "copy_path" ) )
        on_popup_copy_text_activate( NULL, data );
    else if ( !strcmp( set->name, "copy_parent" ) )
        on_popup_copy_parent_activate( NULL, data );
    else if ( g_str_has_prefix( set->name, "copy_loc" )
                || g_str_has_prefix( set->name, "copy_tab_" )
                || g_str_has_prefix( set->name, "copy_panel_" )
                || g_str_has_prefix( set->name, "move_loc" )
                || g_str_has_prefix( set->name, "move_tab_" )
                || g_str_has_prefix( set->name, "move_panel_" ) )
        on_copycmd( NULL, data, set );
    else if ( g_str_has_prefix( set->name, "root_" ) )
    {
        xname = set->name + 5;
        if ( !strcmp( xname, "copy_loc" )
                            || !strcmp( xname, "move2" ) 
                            || !strcmp( xname, "delete" ) )
            on_popup_rootcmd_activate( NULL, data, set );
    }
    else if ( browser )
    {
        // browser only
        if ( g_str_has_prefix( set->name, "open_in_panel" ) )
        {
            xname = set->name + 13;
            if ( !strcmp( xname, "prev" ) )
                i = -1;
            else if ( !strcmp( xname, "next" ) )
                i = -2;
            else
                i = atoi( xname );
            main_window_open_in_panel( data->browser, i, data->file_path );
        }
        else if ( g_str_has_prefix( set->name, "opentab_" ) )
        {
            xname = set->name + 8;
            if ( !strcmp( xname, "new" ) )
                on_popup_open_in_new_tab_activate( NULL, data );
            else
            {
                if ( !strcmp( xname, "prev" ) )
                    i = -1;
                else if ( !strcmp( xname, "next" ) )
                    i = -2;
                else
                    i = atoi( xname );
                ptk_file_browser_open_in_tab( data->browser, i, data->file_path );
            }
        }
        else if ( !strcmp( set->name, "tab_new" ) )
            on_shortcut_new_tab_activate( NULL, browser );
        else if ( !strcmp( set->name, "tab_new_here" ) )
            on_popup_open_in_new_tab_here( NULL, data );
    }
    else
    {
        // desktop only
        
    }
    ptk_file_menu_free( data );
}

