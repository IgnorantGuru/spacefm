/*
*  C Implementation: pref_dialog
*
* Description:
*
*
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "pcmanfm.h"

#include <string.h>
#include <gtk/gtk.h>

#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "glib-utils.h"
#include <glib/gi18n.h>

#include "pref-dialog.h"
#include "settings.h"
#include "ptk-utils.h"
#include "main-window.h"
#include "ptk-file-browser.h"
#include "desktop.h"
#include "ptk-location-view.h"

#include "gtk2-compat.h"


typedef struct _FMPrefDlg FMPrefDlg;
struct _FMPrefDlg
{
    GtkWidget* dlg;
    GtkWidget* notebook;
    GtkWidget* encoding;
    GtkWidget* bm_open_method;
    GtkWidget* max_thumb_size;
    GtkWidget* show_thumbnail;
    GtkWidget* thumb_label1;
    GtkWidget* thumb_label2;
    GtkWidget* terminal;
    GtkWidget* big_icon_size;
    GtkWidget* small_icon_size;
    GtkWidget* tool_icon_size;
    GtkWidget* single_click;
    GtkWidget* single_hover;
    GtkWidget* use_si_prefix;
    //GtkWidget* rubberband;
    GtkWidget* root_bar;
    GtkWidget* drag_action;

    /* Interface tab */
    GtkWidget* always_show_tabs;
    GtkWidget* hide_close_tab_buttons;
    //GtkWidget* hide_folder_content_border;

    //GtkWidget* show_desktop;
    GtkWidget* show_wallpaper;
    GtkWidget* wallpaper;
    GtkWidget* wallpaper_mode;
    GtkWidget* img_preview;
    GtkWidget* show_wm_menu;
    GtkWidget* desk_single_click;
    GtkWidget* desk_single_hover;
    GtkWidget* desk_open_mime;
    GtkWidget* desk_font;
    GtkWidget* margin_top;
    GtkWidget* margin_left;
    GtkWidget* margin_right;
    GtkWidget* margin_bottom;
    GtkWidget* margin_pad;

    GtkWidget* bg_color1;
    GtkWidget* text_color;
    GtkWidget* shadow_color;
    
    //MOD
    GtkWidget* confirm_delete;
    GtkWidget* click_exec;
    GtkWidget* su_command;
    GtkWidget* gsu_command;
    GtkWidget* date_format;
    GtkWidget* date_display;
    GtkWidget* editor;
    GtkWidget* editor_terminal;
    GtkWidget* root_editor;
    GtkWidget* root_editor_terminal;
};

extern gboolean daemon_mode;    /* defined in main.c */

static FMPrefDlg* data = NULL;
static const int tool_icon_sizes[] = { 
    0,
    GTK_ICON_SIZE_MENU,
    GTK_ICON_SIZE_SMALL_TOOLBAR,
    GTK_ICON_SIZE_LARGE_TOOLBAR,
    GTK_ICON_SIZE_BUTTON,
    GTK_ICON_SIZE_DND,
    GTK_ICON_SIZE_DIALOG };
// also change max_icon_size in settings.c & lists in prefdlg.ui prefdlg2.ui
// see create_size in vfs-thumbnail-loader.c:_vfs_thumbnail_load()
static const int big_icon_sizes[] = { 512, 384, 256, 192, 128, 96, 72, 64, 48, 36, 32, 24, 22 };
static const int small_icon_sizes[] = { 512, 384, 256, 192, 128, 96, 72, 64, 48, 36, 32, 24, 22, 16, 12 };
static const char* date_formats[] =
{
    "%Y-%m-%d %H:%M",
    "%Y-%m-%d",
    "%Y-%m-%d %H:%M:%S"
};
static const int drag_actions[] = { 0, 1, 2, 3 };
/*
static void
on_show_desktop_toggled( GtkToggleButton* show_desktop, GtkWidget* desktop_page )
{
    gtk_container_foreach( GTK_CONTAINER(desktop_page),
                           (GtkCallback) gtk_widget_set_sensitive,
                           (gpointer) gtk_toggle_button_get_active( show_desktop ) );
    gtk_widget_set_sensitive( GTK_WIDGET(show_desktop), TRUE );
}
*/
static void set_preview_image( GtkImage* img, const char* file )
{
    GdkPixbuf* pix = NULL;
    pix = gdk_pixbuf_new_from_file_at_scale( file, 128, 128, TRUE, NULL );
    if( pix )
    {
        gtk_image_set_from_pixbuf( img, pix );
        g_object_unref( pix );
    }
}

static void on_update_img_preview( GtkFileChooser *chooser, GtkImage* img )
{
    char* file = gtk_file_chooser_get_preview_filename( chooser );
    if( file )
    {
        set_preview_image( img, file );
        g_free( file );
    }
    else
    {
        gtk_image_clear( img );
    }
}

static void
dir_unload_thumbnails( const char* path, VFSDir* dir, gpointer user_data )
{
    vfs_dir_unload_thumbnails( dir, GPOINTER_TO_INT( user_data ) );
}

static const char* font_button_get_font( GtkWidget* button )
{
    if ( !g_strcmp0( gtk_button_get_label( GTK_BUTTON( button ) ),
                                                    _("Default") ) )
        return NULL;
    else
        return gtk_button_get_label( GTK_BUTTON( button ) );
}

static void font_button_set_font( GtkWidget* button,
                                            PangoFontDescription* font_desc,
                                            const char* font_name )
{
    if ( !font_desc && !font_name )
    {
        gtk_button_set_label( GTK_BUTTON( button ), _("Default") );
        gtk_widget_modify_font( GTK_WIDGET( button ), NULL );
    }
    else
    {
        char* font_name_str = NULL;
        char* fontname;
        if ( font_name )
            fontname = (char*)font_name;
        else
            fontname = font_name_str =
                                pango_font_description_to_string( font_desc );
        gtk_button_set_label( GTK_BUTTON( button ), fontname );        
        g_free( font_name_str );
    }
}

void on_font_button_clicked( GtkButton* button, FMPrefDlg* data )
{
    char* fontname = xset_font_dialog( GTK_WIDGET( data->dlg ),
                                    _("Choose Desktop Font"),
                                   _("Example Item 0123456789"),
                                   font_button_get_font( data->desk_font ) );
    if ( fontname )
    {
        font_button_set_font( data->desk_font, NULL, fontname[0] ?
                                                 fontname : NULL );
        g_free( fontname );
    }
}

static void on_response( GtkDialog* dlg, int response, FMPrefDlg* user_data )
{
    int i, n;
    gboolean b;
    int ibig_icon = -1, ismall_icon = -1, itool_icon = -1;
    const char* filename_encoding;

    int max_thumb;
    gboolean show_thumbnail;
    int big_icon;
    int small_icon;
    int tool_icon;
    //gboolean show_desktop;
    gboolean show_wallpaper;
    gboolean single_click;
    gboolean single_hover;
    //gboolean rubberband;
    gboolean root_bar;
    gboolean root_set_change = FALSE;
    WallpaperMode wallpaper_mode;
    GdkColor bg1;
    GdkColor bg2;
    GdkColor text;
    GdkColor shadow;
    char* wallpaper;
    const GList* l;
    PtkFileBrowser* file_browser;
    gboolean use_si_prefix;
    GtkNotebook* notebook;
    int cur_tabx, p;
    FMMainWindow* a_window;
    char* str;

    GtkWidget * tab_label;
    /* interface settings */
    gboolean always_show_tabs;
    gboolean hide_close_tab_buttons;
    //gboolean hide_folder_content_border;

    /* built-in response codes of GTK+ are all negative */
    if( response >= 0 )
        return;

    if ( response == GTK_RESPONSE_OK )
    {
        /* file name encoding */
        //filename_encoding = gtk_entry_get_text( GTK_ENTRY( data->encoding ) );
        //if ( filename_encoding
        //    && g_ascii_strcasecmp ( filename_encoding, "UTF-8" ) )
        //{
        //    strcpy( app_settings.encoding, filename_encoding );
        //    setenv( "G_FILENAME_ENCODING", app_settings.encoding, 1 );
        //}
        //else
        //{
        //    app_settings.encoding[ 0 ] = '\0';
        //    unsetenv( "G_FILENAME_ENCODING" );
        //}

        //app_settings.open_bookmark_method = gtk_combo_box_get_active( GTK_COMBO_BOX( data->bm_open_method ) ) + 1;

        show_thumbnail = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( data->show_thumbnail ) );
        max_thumb = ( ( int ) gtk_spin_button_get_value( GTK_SPIN_BUTTON( data->max_thumb_size ) ) ) << 10;

        /* interface settings */

        always_show_tabs = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( data->always_show_tabs ) );
        if ( always_show_tabs != app_settings.always_show_tabs )
        {
            app_settings.always_show_tabs = always_show_tabs;
            // update all windows/all panels
            for ( l = fm_main_window_get_all(); l; l = l->next )
            {
                a_window = FM_MAIN_WINDOW( l->data );
                for ( p = 1; p < 5; p++ )
                {
                    notebook = GTK_NOTEBOOK( a_window->panel[p-1] );
                    n = gtk_notebook_get_n_pages( notebook );
                    if ( always_show_tabs )
                        gtk_notebook_set_show_tabs( notebook, TRUE );
                    else if ( n == 1 )
                        gtk_notebook_set_show_tabs( notebook, FALSE );
                }
            }
        }
        hide_close_tab_buttons = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( data->hide_close_tab_buttons ) );
        if ( hide_close_tab_buttons != app_settings.hide_close_tab_buttons )
        {
            app_settings.hide_close_tab_buttons = hide_close_tab_buttons;
            // update all windows/all panels/all browsers
            for ( l = fm_main_window_get_all(); l; l = l->next )
            {
                a_window = FM_MAIN_WINDOW( l->data );
                for ( p = 1; p < 5; p++ )
                {
                    notebook = GTK_NOTEBOOK( a_window->panel[p-1] );
                    n = gtk_notebook_get_n_pages( notebook );
                    for ( i = 0; i < n; ++i )
                    {
                        file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                                         notebook, i ) );
                        tab_label = fm_main_window_create_tab_label( a_window,
                                                                    file_browser );
                        gtk_notebook_set_tab_label( notebook, GTK_WIDGET(file_browser),
                                                                        tab_label );
                        fm_main_window_update_tab_label( a_window, file_browser,
                                                    file_browser->dir->disp_path );
                    }
                }
            }
        }
/*
        hide_folder_content_border = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( data->hide_folder_content_border ) );
        if ( hide_folder_content_border != app_settings.hide_folder_content_border )
        {
            app_settings.hide_folder_content_border = hide_folder_content_border;
            // update all windows/all panels/all browsers
            for ( l = fm_main_window_get_all(); l; l = l->next )
            {
                a_window = FM_MAIN_WINDOW( l->data );
                for ( p = 1; p < 5; p++ )
                {
                    notebook = a_window->panel[p-1];
                    n = gtk_notebook_get_n_pages( notebook );
                    for ( i = 0; i < n; ++i )
                    {
                        file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                                         notebook, i ) );
                        if ( hide_folder_content_border )
                            ptk_file_browser_hide_shadow( file_browser );
                        else
                            ptk_file_browser_show_shadow( file_browser );
                    }
                }
            }
        }
*/
/*        hide_side_pane_buttons = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( data->hide_side_pane_buttons ) );
        if ( hide_side_pane_buttons != app_settings.hide_side_pane_buttons )
        {
            app_settings.hide_side_pane_buttons = hide_side_pane_buttons;
            for ( l = fm_main_window_get_all(); l; l = l->next )
            {
                FMMainWindow* main_window = FM_MAIN_WINDOW( l->data );
                GtkNotebook* notebook = main_window->notebook;
                n = gtk_notebook_get_n_pages( notebook );

                for ( i = 0; i < n; ++i )
                {
                  file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page( notebook, i ) );

                  if ( hide_side_pane_buttons)
                  {
                    ptk_file_browser_hide_side_pane_buttons( file_browser );
                  }
                  else
                  {
                    ptk_file_browser_show_side_pane_buttons( file_browser );
                  }

                }
            }
        }
*/

        // Desktop settings =================================================

        // checkboxes
        int desk_single_click = !!gtk_toggle_button_get_active(
                                GTK_TOGGLE_BUTTON( data->desk_single_click ) );
        if ( app_settings.desk_single_click != desk_single_click )
        {
            app_settings.desk_single_click = desk_single_click;
            fm_desktop_set_single_click( app_settings.desk_single_click );
        }
        int desk_no_single_hover = !gtk_toggle_button_get_active(
                                GTK_TOGGLE_BUTTON( data->desk_single_hover ) );
        if ( app_settings.desk_no_single_hover != desk_no_single_hover )
            app_settings.desk_no_single_hover = desk_no_single_hover;
        app_settings.show_wm_menu = gtk_toggle_button_get_active(
                                GTK_TOGGLE_BUTTON( data->show_wm_menu ) );
        app_settings.desk_open_mime = gtk_toggle_button_get_active(
                                GTK_TOGGLE_BUTTON( data->desk_open_mime ) );

        // wallpaper
        show_wallpaper = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( 
                                                    data->show_wallpaper ) );
        wallpaper = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(
                                                    data->wallpaper ) );
        wallpaper_mode = gtk_combo_box_get_active(
                                        (GtkComboBox*)data->wallpaper_mode );

        // colors
        gtk_color_button_get_color(GTK_COLOR_BUTTON(data->bg_color1), &bg1);
        gtk_color_button_get_color(GTK_COLOR_BUTTON(data->text_color), &text);
        gtk_color_button_get_color(GTK_COLOR_BUTTON(data->shadow_color), &shadow);

        // need update ?
        gboolean need_update_bg = FALSE;
        // are desktop colors are changed ?
        if ( !gdk_color_equal( &bg1, &app_settings.desktop_bg1 ) ||
                !gdk_color_equal( &bg2, &app_settings.desktop_bg2 ) ||
                !gdk_color_equal( &text, &app_settings.desktop_text ) ||
                !gdk_color_equal( &shadow, &app_settings.desktop_shadow ) )
        {
            app_settings.desktop_bg1 = bg1;
            app_settings.desktop_bg2 = bg2;
            app_settings.desktop_text = text;
            app_settings.desktop_shadow = shadow;

            fm_desktop_update_colors();

            if( wallpaper_mode == WPM_CENTER || !show_wallpaper )
                need_update_bg = TRUE;
        }

        // are wallpaper settings are changed ?
        if ( need_update_bg ||
             wallpaper_mode != app_settings.wallpaper_mode ||
             show_wallpaper != app_settings.show_wallpaper ||
             ( g_strcmp0( wallpaper, app_settings.wallpaper ) ) )
        {
            gboolean was_transparent = app_settings.show_wallpaper == 1 &&
                            app_settings.wallpaper_mode == WPM_TRANSPARENT;
            gboolean is_transparent = show_wallpaper == 1 &&
                            wallpaper_mode == WPM_TRANSPARENT;
            app_settings.wallpaper_mode = wallpaper_mode;
            app_settings.show_wallpaper = show_wallpaper;
            g_free( app_settings.wallpaper );
            app_settings.wallpaper = wallpaper;
            fm_desktop_update_wallpaper( !was_transparent != !is_transparent );
            if ( is_transparent && !was_transparent &&
                                                !xset_get_b( "desk_pref" ) )
            {
                xset_msg_dialog( GTK_WIDGET( dlg ), 0, _("Transparency Requirements"),
                        NULL, 0, _("General Note: For desktop transparency to "
                        "work, you need to be running a compositing window "
                        "manager or separate compositor like compton or xcompmgr. "
                        "You can then use use nitrogen or xwinwrap to set wallpaper "
                        "on the background.\n\n"
                        "This message will not repeat."), NULL, NULL );
                xset_set_b( "desk_pref", TRUE );
            }
        }

        //font
        gboolean update_icons = FALSE;
        char* old_fontname = app_settings.desk_font ?
                                    pango_font_description_to_string(
                                                    app_settings.desk_font ) :
                                    NULL;
        const char* new_fontname = font_button_get_font( data->desk_font );
        if ( g_strcmp0( new_fontname, old_fontname ) )
        {
            app_settings.desk_font = new_fontname ?
                            pango_font_description_from_string( new_fontname ) :
                                                                        NULL;
            update_icons = TRUE;
        }
        g_free( old_fontname );

        // margins
        int margin_top = atoi( gtk_entry_get_text( 
                                        GTK_ENTRY( data->margin_top ) ) );
        int margin_left = atoi( gtk_entry_get_text( 
                                        GTK_ENTRY( data->margin_left ) ) );
        int margin_right = atoi( gtk_entry_get_text( 
                                        GTK_ENTRY( data->margin_right ) ) );
        int margin_bottom = atoi( gtk_entry_get_text( 
                                        GTK_ENTRY( data->margin_bottom ) ) );
        int margin_pad = atoi( gtk_entry_get_text( 
                                        GTK_ENTRY( data->margin_pad ) ) );
        if ( margin_top != app_settings.margin_top ||
             margin_left != app_settings.margin_left ||
             margin_right != app_settings.margin_right ||
             margin_bottom != app_settings.margin_bottom ||
             margin_pad != app_settings.margin_pad )
        {
            // margins changed
            app_settings.margin_top = margin_top;
            app_settings.margin_left = margin_left;
            app_settings.margin_right = margin_right;
            app_settings.margin_bottom = margin_bottom;
            app_settings.margin_pad = margin_pad;
            if ( app_settings.margin_top < 0 || app_settings.margin_top > 999 )
                app_settings.margin_top = 12;
            if ( app_settings.margin_left < 0 || app_settings.margin_left > 999 )
                app_settings.margin_left = 6;
            if ( app_settings.margin_right < 0 || app_settings.margin_right > 999 )
                app_settings.margin_right = 6;
            if ( app_settings.margin_bottom < 0 || app_settings.margin_bottom > 999 )
                app_settings.margin_bottom = 12;
            if ( app_settings.margin_pad < 0 || app_settings.margin_pad > 999 )
                app_settings.margin_pad = 6;
            update_icons = TRUE;
        }

        if ( update_icons )
            fm_desktop_update_icons();
        

        // ===============================================================

        /* thumbnail settings are changed */
        if( app_settings.show_thumbnail != show_thumbnail || app_settings.max_thumb_size != max_thumb )
        {
            app_settings.show_thumbnail = !show_thumbnail;  // toggle reverses this
            app_settings.max_thumb_size = max_thumb;
            // update all windows/all panels/all browsers + desktop
            main_window_toggle_thumbnails_all_windows();
        }

        /* icon sizes are changed? */
        ibig_icon = gtk_combo_box_get_active( GTK_COMBO_BOX( data->big_icon_size ) );
        big_icon = ibig_icon >= 0 ? big_icon_sizes[ ibig_icon ] : app_settings.big_icon_size;
        ismall_icon = gtk_combo_box_get_active( GTK_COMBO_BOX( data->small_icon_size ) );
        small_icon = ismall_icon >= 0 ? small_icon_sizes[ ismall_icon ] : app_settings.small_icon_size;
        itool_icon = gtk_combo_box_get_active( GTK_COMBO_BOX( data->tool_icon_size ) );
        if ( itool_icon >= 0 && itool_icon <= GTK_ICON_SIZE_DIALOG )
            tool_icon = tool_icon_sizes[ itool_icon ];

        if ( big_icon != app_settings.big_icon_size
            || small_icon != app_settings.small_icon_size )
        {
            vfs_mime_type_set_icon_size( big_icon, small_icon );
            vfs_file_info_set_thumbnail_size( big_icon, small_icon );

            /* unload old thumbnails (icons of *.desktop files will be unloaded here, too)  */
            if( big_icon != app_settings.big_icon_size )
                vfs_dir_foreach( (GHFunc)dir_unload_thumbnails, GINT_TO_POINTER( 1 ) );
            if( small_icon != app_settings.small_icon_size )
                vfs_dir_foreach( (GHFunc)dir_unload_thumbnails, GINT_TO_POINTER( 0 ) );

            // update desktop icons
            if ( big_icon != app_settings.big_icon_size )
            {
                app_settings.big_icon_size = big_icon;
                fm_desktop_update_icons();
            }
            app_settings.big_icon_size = big_icon;
            app_settings.small_icon_size = small_icon;

            // update all windows/all panels/all browsers
            for ( l = fm_main_window_get_all(); l; l = l->next )
            {
                a_window = FM_MAIN_WINDOW( l->data );
                for ( p = 1; p < 5; p++ )
                {
                    notebook = GTK_NOTEBOOK( a_window->panel[p-1] );
                    n = gtk_notebook_get_n_pages( notebook );
                    for ( i = 0; i < n; ++i )
                    {
                        file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                                         notebook, i ) );
                        // update views
                        gtk_widget_destroy( file_browser->folder_view );
                        file_browser->folder_view = NULL;
                        if ( file_browser->side_dir )
                        {
                            gtk_widget_destroy( file_browser->side_dir );
                            file_browser->side_dir = NULL;
                        }
                        ptk_file_browser_update_views( NULL, file_browser );
                        if ( file_browser->side_book )
                            ptk_bookmark_view_update_icons( NULL, file_browser );
                    }
                }
            }
            update_volume_icons();            
        }

        if ( tool_icon != app_settings.tool_icon_size )
        {
            app_settings.tool_icon_size = tool_icon;
            main_window_rebuild_all_toolbars( NULL );
        }

        /* unit settings changed? */
        gboolean need_refresh = FALSE;
        use_si_prefix = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( data->use_si_prefix ) );
        if( use_si_prefix != app_settings.use_si_prefix )
        {
            app_settings.use_si_prefix = use_si_prefix;
            need_refresh = TRUE;
        }

        // date format
        char* etext = g_strdup( gtk_entry_get_text( GTK_ENTRY( gtk_bin_get_child(
                                            GTK_BIN( data->date_format ) ) ) ) );
        if ( g_strcmp0( etext, xset_get_s( "date_format" ) ) )
        {
            if ( etext[0] == '\0' )
                xset_set( "date_format", "s", "%Y-%m-%d %H:%M" );
            else
                xset_set( "date_format", "s", etext );
            g_free( etext );
            if ( app_settings.date_format )
                g_free( app_settings.date_format );
            app_settings.date_format = g_strdup( xset_get_s( "date_format" ) );
            need_refresh = TRUE;
        }
        if ( need_refresh )
            main_window_refresh_all();
        
        /* single click changed? */
        single_click = gtk_toggle_button_get_active( (GtkToggleButton*)data->single_click );
        if( single_click != app_settings.single_click )
        {
            app_settings.single_click = single_click;
            // update all windows/all panels/all browsers
            for ( l = fm_main_window_get_all(); l; l = l->next )
            {
                a_window = FM_MAIN_WINDOW( l->data );
                for ( p = 1; p < 5; p++ )
                {
                    notebook = GTK_NOTEBOOK( a_window->panel[p-1] );
                    n = gtk_notebook_get_n_pages( notebook );
                    for ( i = 0; i < n; ++i )
                    {
                        file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                                         notebook, i ) );
                        ptk_file_browser_set_single_click( file_browser, app_settings.single_click );
                    }
                }
            }
        }

        /* single click - hover selects changed? */
        gboolean no_single_hover = !gtk_toggle_button_get_active(
                                        (GtkToggleButton*)data->single_hover );
        if( no_single_hover != app_settings.no_single_hover )
        {
            app_settings.no_single_hover = no_single_hover;
            // update all windows/all panels/all browsers
            for ( l = fm_main_window_get_all(); l; l = l->next )
            {
                a_window = FM_MAIN_WINDOW( l->data );
                for ( p = 1; p < 5; p++ )
                {
                    notebook = GTK_NOTEBOOK( a_window->panel[p-1] );
                    n = gtk_notebook_get_n_pages( notebook );
                    for ( i = 0; i < n; ++i )
                    {
                        file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                                         notebook, i ) );
                        ptk_file_browser_set_single_click_timeout( file_browser,
                                        app_settings.no_single_hover ? 0 :
                                                        SINGLE_CLICK_TIMEOUT );
                    }
                }
            }
        }
        
        //MOD
        app_settings.no_execute = !gtk_toggle_button_get_active(
                                            (GtkToggleButton*)data->click_exec );
        app_settings.no_confirm = !gtk_toggle_button_get_active(
                                            (GtkToggleButton*)data->confirm_delete );

        /*
        rubberband = gtk_toggle_button_get_active(
                                            (GtkToggleButton*)data->rubberband );
        if ( !!rubberband != !!xset_get_b( "rubberband" ) )
        {
            xset_set_b( "rubberband", rubberband );
            main_window_rubberband_all();
        }
        */
        
        root_bar = gtk_toggle_button_get_active(
                                            (GtkToggleButton*)data->root_bar );
        if ( !!root_bar != !!xset_get_b( "root_bar" ) )
        {
            xset_set_b( "root_bar", root_bar );
            main_window_root_bar_all();
        }
        
        char* s = g_strdup_printf( "%d",
                    gtk_combo_box_get_active( GTK_COMBO_BOX( data->drag_action ) ) );
        xset_set( "drag_action", "x", s );
        g_free( s );

        // terminal su command
        char* custom_su = NULL;
        if ( settings_terminal_su )
            // get su from /etc/spacefm/spacefm.conf
            custom_su = g_find_program_in_path( settings_terminal_su );
        int idx = gtk_combo_box_get_active( GTK_COMBO_BOX( data->su_command ) );
        if ( idx > -1 )
        {
            if ( custom_su )
            {
                if ( idx == 0 )
                    xset_set( "su_command", "s", custom_su );
                else
                    xset_set( "su_command", "s", su_commands[idx - 1] );
                g_free( custom_su );
            }
            else
                xset_set( "su_command", "s", su_commands[idx] );
        }
        
        // graphical su command
        char* custom_gsu = NULL;
        if ( settings_graphical_su )
            // get gsu from /etc/spacefm/spacefm.conf
            custom_gsu = g_find_program_in_path( settings_graphical_su );
#ifdef PREFERABLE_SUDO_PROG
        if ( !custom_gsu )
            // get build-time gsu
            custom_gsu = g_find_program_in_path( PREFERABLE_SUDO_PROG );
#endif
        idx = gtk_combo_box_get_active( GTK_COMBO_BOX( data->gsu_command ) );
        if ( idx > -1 )
        {
            if ( custom_gsu )
            {
                if ( idx == 0 )
                    xset_set( "gsu_command", "s", custom_gsu );
                else
                    xset_set( "gsu_command", "s", gsu_commands[idx - 1] );
                g_free( custom_gsu );
            }
            else
                xset_set( "gsu_command", "s", gsu_commands[idx] );
        }
        
        //MOD editors
        xset_set( "editor", "s", gtk_entry_get_text( GTK_ENTRY( data->editor ) ) );
        xset_set_b( "editor", gtk_toggle_button_get_active(
                                            (GtkToggleButton*)data->editor_terminal ) );
        const char* root_editor = gtk_entry_get_text( GTK_ENTRY( data->root_editor ) );
        const char* old_root_editor = xset_get_s( "root_editor" );
        if ( !old_root_editor )
        {
            if ( root_editor[0] != '\0' )
            {
                xset_set( "root_editor", "s", root_editor );
                root_set_change = TRUE;
            }
        }
        else if ( strcmp( root_editor, old_root_editor ) )
        {
            xset_set( "root_editor", "s", root_editor );
            root_set_change = TRUE;
        }
        if ( !!gtk_toggle_button_get_active(
                                    (GtkToggleButton*)data->root_editor_terminal )
                                    != !!xset_get_b( "root_editor" ) )
        {
            xset_set_b( "root_editor", gtk_toggle_button_get_active(
                                    (GtkToggleButton*)data->root_editor_terminal ) );
            root_set_change = TRUE;
        }

        //MOD terminal
        char* old_terminal = xset_get_s( "main_terminal" );
        char* terminal = gtk_combo_box_text_get_active_text( GTK_COMBO_BOX_TEXT( data->terminal ) );
        g_strstrip( terminal );
        if ( g_strcmp0( terminal, old_terminal ) )
        {
            xset_set( "main_terminal", "s", terminal[0] == '\0' ? NULL : terminal );
            root_set_change = TRUE;
            if ( terminal && ( strstr( terminal, "lxterminal" ) ||
                               strstr( terminal, "urxvtc" ) ||
                               strstr( terminal, "konsole" ) ||
                               strstr( terminal, "gnome-terminal" ) ) )
                        /* when changing this list adjust also
                         * vfs-file-task.c Line ~1655
                         * ptk-location-view.c:ptk_location_view_mount_network()
                         * and string below */
            {
                str = g_strdup_printf( _("Use of some terminals (%s) with SpaceFM may prevent the Run As Task and Run In Terminal options being used together to run commands, due to limitations in these terminals.  For example, this may cause protocols mounted in a terminal to fail to automatically open after being mounted.  For full use of SpaceFM's features, selecting another terminal is recommended."),
                    "gnome-terminal, konsole, lxterminal, urxvtc" );
                xset_msg_dialog( GTK_WIDGET( dlg ), 0,
                                    _("Limited Terminal Emulator Selected"),
                                    NULL, 0, str, NULL,
                                    "#designmode-command-terminal" );
                g_free( str );
            }
        }
        // report missing terminal
        if ( str = strchr( terminal, ' ' ) )
            str[0] = '\0';
        str = g_find_program_in_path( terminal );
        if ( !str )
        {
            str = g_strdup_printf( "Unable to find terminal program '%s'", terminal );
            ptk_show_error( GTK_WINDOW( dlg ), "Error", str );
        }
        g_free( str );
        g_free( terminal );
        
        /* save to config file */

        char* err_msg = save_settings( NULL );
        if ( err_msg )
        {
            char* msg = g_strdup_printf( "Error: Unable to save session file.\n\n%s",
                                                                            err_msg );
            ptk_show_error( GTK_WINDOW( dlg ), "Error", msg );
            g_free( msg );
            g_free( err_msg );
        }
        
        if ( xset_get_b( "main_terminal" ) )
        {
            root_set_change = TRUE;
            xset_set_b( "main_terminal", FALSE );
        }
        
        // root settings saved?
        if ( geteuid() != 0 )
        {
            /*
            char* root_set_path= g_strdup_printf(
                                    "/etc/spacefm/%s-as-root", g_get_user_name() );
            if ( !g_file_test( root_set_path, G_FILE_TEST_EXISTS ) )
            {
                g_free( root_set_path );
                root_set_path= g_strdup_printf(
                                            "/etc/spacefm/%d-as-root", geteuid() );
                if ( !g_file_test( root_set_path, G_FILE_TEST_EXISTS ) )
                    root_set_change = TRUE;
            }
            */
            if ( root_set_change )
            {
                // task
                char* msg = g_strdup_printf( _("You will now be asked for your root password to save the root settings for this user to a file in %s/spacefm/  Supplying the password in the next window is recommended.  Because SpaceFM runs some commands as root via su, these settings are best protected by root."), SYSCONFDIR );
                xset_msg_dialog( GTK_WIDGET( dlg ), 0, _("Save Root Settings"), NULL, 0, msg, NULL, NULL );
                g_free( msg );
                PtkFileTask* task = ptk_file_exec_new( _("Save Root Settings"), NULL, NULL,
                                                                    NULL );
                task->task->exec_command = g_strdup_printf( "echo" );
                task->task->exec_as_user = g_strdup_printf( "root" );
                task->task->exec_sync = FALSE;
                task->task->exec_export = FALSE;
                task->task->exec_write_root = TRUE;
                ptk_file_task_run( task );            
            }
        }
    }
    gtk_widget_destroy( GTK_WIDGET( dlg ) );
    g_free( data );
    data = NULL;
    pcmanfm_unref();
}

void on_date_format_changed( GtkComboBox *widget, FMPrefDlg* data )
{
    char buf[ 128 ];
    const char* etext;

    time_t now = time( NULL );
    etext = gtk_entry_get_text( GTK_ENTRY( gtk_bin_get_child( GTK_BIN( data->date_format ) ) ) );
    strftime( buf, sizeof( buf ), etext, localtime( &now ) );
    gtk_label_set_text( GTK_LABEL( data->date_display ), buf );
}

void on_single_click_toggled( GtkWidget* widget, FMPrefDlg* data )
{
    gtk_widget_set_sensitive( data->single_hover,
                    gtk_toggle_button_get_active( 
                    GTK_TOGGLE_BUTTON( data->single_click ) ) );
}

void on_desk_single_click_toggled( GtkWidget* widget, FMPrefDlg* data )
{
    gtk_widget_set_sensitive( data->desk_single_hover,
                    gtk_toggle_button_get_active( 
                    GTK_TOGGLE_BUTTON( data->desk_single_click ) ) );
}

void on_show_thumbnail_toggled( GtkWidget* widget, FMPrefDlg* data )
{
    gtk_widget_set_sensitive( data->max_thumb_size,
                    gtk_toggle_button_get_active( 
                    GTK_TOGGLE_BUTTON( data->show_thumbnail ) ) );
    gtk_widget_set_sensitive( data->thumb_label1, 
                    gtk_toggle_button_get_active( 
                    GTK_TOGGLE_BUTTON( data->show_thumbnail ) ) );
    gtk_widget_set_sensitive( data->thumb_label2, 
                    gtk_toggle_button_get_active( 
                    GTK_TOGGLE_BUTTON( data->show_thumbnail ) ) );
}

void on_wallpaper_mode_changed( GtkComboBox *combobox, FMPrefDlg* data )
{
    gint active = gtk_combo_box_get_active(
                                        (GtkComboBox*)data->wallpaper_mode );
    gtk_widget_set_sensitive( data->wallpaper, active != WPM_TRANSPARENT );
}

void on_wallpaper_toggled( GtkToggleButton* show_wallpaper, FMPrefDlg* data )
{
    gboolean enabled = gtk_toggle_button_get_active( show_wallpaper );
    gint active = gtk_combo_box_get_active(
                                        (GtkComboBox*)data->wallpaper_mode );
    gtk_widget_set_sensitive( GTK_WIDGET( data->wallpaper ), enabled &&
                                          active != WPM_TRANSPARENT );
    gtk_widget_set_sensitive( GTK_WIDGET( data->wallpaper_mode ), enabled );
}

gboolean fm_edit_preference( GtkWindow* parent, int page )
{
    const char* filename_encoding;
    int i;
    int ibig_icon = -1, ismall_icon = -1, itool_icon = -1;
    GtkWidget* img_preview;
    GtkWidget* dlg;
    char* str;
    
    if( ! data )
    {
        GtkTreeModel* model;
        // this invokes GVFS-RemoteVolumeMonitor via IsSupported
#if GTK_CHECK_VERSION(2, 24, 0)
        GtkBuilder* builder = _gtk_builder_new_from_file( PACKAGE_UI_DIR "/prefdlg2.ui", NULL );
#else
        GtkBuilder* builder = _gtk_builder_new_from_file( PACKAGE_UI_DIR "/prefdlg.ui", NULL );
#endif
        if ( !builder )
            return FALSE;
        pcmanfm_ref();

        data = g_new0( FMPrefDlg, 1 );
        dlg = (GtkWidget*)gtk_builder_get_object( builder, "dlg" );
        if ( parent )
            gtk_window_set_transient_for( GTK_WINDOW( dlg ), parent );
        xset_set_window_icon( GTK_WINDOW( dlg ) );

        ptk_dialog_fit_small_screen( GTK_DIALOG( dlg ) );
        data->dlg = dlg;
        data->notebook = (GtkWidget*)gtk_builder_get_object( builder, "notebook" );
        gtk_dialog_set_alternative_button_order( GTK_DIALOG( dlg ), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1 );

        /* Setup 'General' tab */

        data->encoding = (GtkWidget*)gtk_builder_get_object( builder, "filename_encoding" );
        data->bm_open_method = (GtkWidget*)gtk_builder_get_object( builder, "bm_open_method" );
        data->show_thumbnail = (GtkWidget*)gtk_builder_get_object( builder, "show_thumbnail" );
        data->thumb_label1 = (GtkWidget*)gtk_builder_get_object( builder, "thumb_label1" );
        data->thumb_label2 = (GtkWidget*)gtk_builder_get_object( builder, "thumb_label2" );
        data->max_thumb_size = (GtkWidget*)gtk_builder_get_object( builder, "max_thumb_size" );
        data->terminal = (GtkWidget*)gtk_builder_get_object( builder, "terminal" );
        data->big_icon_size = (GtkWidget*)gtk_builder_get_object( builder, "big_icon_size" );
        data->small_icon_size = (GtkWidget*)gtk_builder_get_object( builder, "small_icon_size" );
        data->tool_icon_size = (GtkWidget*)gtk_builder_get_object( builder, "tool_icon_size" );
        data->single_click = (GtkWidget*)gtk_builder_get_object( builder, "single_click" );
        data->single_hover = (GtkWidget*)gtk_builder_get_object( builder, "single_hover" );
        data->use_si_prefix = (GtkWidget*)gtk_builder_get_object( builder, "use_si_prefix" );
        //data->rubberband = (GtkWidget*)gtk_builder_get_object( builder, "rubberband" );
        data->root_bar = (GtkWidget*)gtk_builder_get_object( builder, "root_bar" );
        data->drag_action = (GtkWidget*)gtk_builder_get_object( builder, "drag_action" );

        model = GTK_TREE_MODEL( gtk_list_store_new( 1, G_TYPE_STRING ) );
        gtk_combo_box_set_model( GTK_COMBO_BOX( data->terminal ), model );
        gtk_combo_box_set_entry_text_column( GTK_COMBO_BOX( data->terminal ), 0 );
        g_object_unref( model );

        //if ( '\0' == ( char ) app_settings.encoding[ 0 ] )
        //    gtk_entry_set_text( GTK_ENTRY( data->encoding ), "UTF-8" );
        //else
        //    gtk_entry_set_text( GTK_ENTRY( data->encoding ), app_settings.encoding );
        
        /*
        if ( app_settings.open_bookmark_method >= 1 &&
                app_settings.open_bookmark_method <= 2 )
        {
            gtk_combo_box_set_active( GTK_COMBO_BOX( data->bm_open_method ),
                                      app_settings.open_bookmark_method - 1 );
        }
        else
        {
            gtk_combo_box_set_active( GTK_COMBO_BOX( data->bm_open_method ), 0 );
        }
        */
        
        gtk_spin_button_set_value ( GTK_SPIN_BUTTON( data->max_thumb_size ),
                                    app_settings.max_thumb_size >> 10 );

        gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON( data->show_thumbnail ),
                                       app_settings.show_thumbnail );
        g_signal_connect( data->show_thumbnail, "toggled",
                                G_CALLBACK( on_show_thumbnail_toggled ), data );
        gtk_widget_set_sensitive( data->max_thumb_size, app_settings.show_thumbnail );
        gtk_widget_set_sensitive( data->thumb_label1, app_settings.show_thumbnail );
        gtk_widget_set_sensitive( data->thumb_label2, app_settings.show_thumbnail );


        for ( i = 0; i < G_N_ELEMENTS( terminal_programs ); ++i )
        {
            gtk_combo_box_text_append_text ( GTK_COMBO_BOX_TEXT( data->terminal ), terminal_programs[ i ] );
        }

        char* terminal = xset_get_s( "main_terminal" );
        if ( terminal )
        {
            for ( i = 0; i < G_N_ELEMENTS( terminal_programs ); ++i )
            {
                if ( 0 == strcmp( terminal_programs[ i ], terminal ) )
                    break;
            }
            if ( i >= G_N_ELEMENTS( terminal_programs ) )
            { /* Found */
                gtk_combo_box_text_prepend_text ( GTK_COMBO_BOX_TEXT( data->terminal ), terminal );
                i = 0;
            }
            gtk_combo_box_set_active( GTK_COMBO_BOX( data->terminal ), i );
        }

        for ( i = 0; i < G_N_ELEMENTS( big_icon_sizes ); ++i )
        {
            if ( big_icon_sizes[ i ] == app_settings.big_icon_size )
            {
                ibig_icon = i;
                break;
            }
        }
        gtk_combo_box_set_active( GTK_COMBO_BOX( data->big_icon_size ), ibig_icon );

        for ( i = 0; i < G_N_ELEMENTS( small_icon_sizes ); ++i )
        {
            if ( small_icon_sizes[ i ] == app_settings.small_icon_size )
            {
                ismall_icon = i;
                break;
            }
        }
        gtk_combo_box_set_active( GTK_COMBO_BOX( data->small_icon_size ), ismall_icon );

        //sfm
        itool_icon = 0;
        for ( i = 0; i < G_N_ELEMENTS( tool_icon_sizes ); ++i )
        {
            if ( tool_icon_sizes[ i ] == app_settings.tool_icon_size )
            {
                itool_icon = i;
                break;
            }
        }
        gtk_combo_box_set_active( GTK_COMBO_BOX( data->tool_icon_size ), itool_icon );

        gtk_toggle_button_set_active( (GtkToggleButton*)data->single_click,
                                                app_settings.single_click );
        gtk_toggle_button_set_active( (GtkToggleButton*)data->single_hover,
                                                !app_settings.no_single_hover );
        gtk_widget_set_sensitive( data->single_hover, app_settings.single_click );
        g_signal_connect( data->single_click, "toggled",
                                G_CALLBACK( on_single_click_toggled ), data );
        
        /* Setup 'Interface' tab */

        data->always_show_tabs = (GtkWidget*)gtk_builder_get_object( builder, "always_show_tabs" );
        gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON( data->always_show_tabs ),
                                       app_settings.always_show_tabs );

        data->hide_close_tab_buttons = (GtkWidget*)gtk_builder_get_object( builder,
                                                            "hide_close_tab_buttons" );
        gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON( data->hide_close_tab_buttons ),
                                       app_settings.hide_close_tab_buttons );

/*        data->hide_side_pane_buttons = (GtkWidget*)gtk_builder_get_object( builder, "hide_side_pane_buttons" );
        gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON( data->hide_side_pane_buttons ),
                                       app_settings.hide_side_pane_buttons );
*/
        //data->hide_folder_content_border = (GtkWidget*)gtk_builder_get_object( builder, "hide_folder_content_border" );
        //gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON( data->hide_folder_content_border ),
        //                               app_settings.hide_folder_content_border );

        //MOD Interface
        data->confirm_delete = (GtkWidget*)gtk_builder_get_object( builder,
                                                                "confirm_delete" );
        gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON( data->confirm_delete ),
                                                        !app_settings.no_confirm );                                
        data->click_exec = (GtkWidget*)gtk_builder_get_object( builder,
                                                                "click_exec" );
        gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON( data->click_exec ),
                                                        !app_settings.no_execute );

        //gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON( data->rubberband ),
        //                                                xset_get_b( "rubberband" ) );

        gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON( data->root_bar ),
                                                        xset_get_b( "root_bar" ) );
        gtk_widget_set_sensitive( data->root_bar, geteuid() == 0 );

        int drag_action = xset_get_int( "drag_action", "x" );
        int drag_action_set = 0;
        for ( i = 0; i < G_N_ELEMENTS( drag_actions ); ++i )
        {
            if ( drag_actions[ i ] == drag_action )
            {
                drag_action_set = i;
                break;
            }
        }
        gtk_combo_box_set_active( GTK_COMBO_BOX( data->drag_action ), drag_action_set );

        gtk_toggle_button_set_active( (GtkToggleButton*)data->use_si_prefix, app_settings.use_si_prefix );


        // Desktop Tab ====================================================
        data->show_wallpaper = (GtkWidget*)gtk_builder_get_object( builder,
                                                        "show_wallpaper" );
        data->wallpaper = (GtkWidget*)gtk_builder_get_object( builder,
                                                        "wallpaper" );
        img_preview = gtk_image_new();
        gtk_widget_set_size_request( img_preview, 128, 128 );
        gtk_file_chooser_set_preview_widget( (GtkFileChooser*)data->wallpaper,
                                                        img_preview );
        g_signal_connect( data->wallpaper, "update-preview",
                                G_CALLBACK(on_update_img_preview), img_preview );

        data->wallpaper_mode = (GtkWidget*)gtk_builder_get_object( builder,
                                                        "wallpaper_mode" );
        data->show_wm_menu = (GtkWidget*)gtk_builder_get_object( builder,
                                                        "show_wm_menu" );
        data->desk_single_click = (GtkWidget*)gtk_builder_get_object( builder,
                                                        "desk_single_click" );
        data->desk_single_hover = (GtkWidget*)gtk_builder_get_object( builder,
                                                        "desk_single_hover" );
        data->desk_open_mime = (GtkWidget*)gtk_builder_get_object( builder,
                                                        "desk_open_mime" );
        data->bg_color1 = (GtkWidget*)gtk_builder_get_object( builder,
                                                        "bg_color1" );
        data->text_color = (GtkWidget*)gtk_builder_get_object( builder,
                                                        "text_color" );
        data->shadow_color = (GtkWidget*)gtk_builder_get_object( builder,
                                                        "shadow_color" );
        data->desk_font = (GtkWidget*)gtk_builder_get_object( builder,
                                                        "desk_font" );
        data->margin_top = (GtkWidget*)gtk_builder_get_object( builder,
                                                        "margin_top" );
        data->margin_left = (GtkWidget*)gtk_builder_get_object( builder,
                                                        "margin_left" );
        data->margin_right = (GtkWidget*)gtk_builder_get_object( builder,
                                                        "margin_right" );
        data->margin_bottom = (GtkWidget*)gtk_builder_get_object( builder,
                                                        "margin_bottom" );
        data->margin_pad = (GtkWidget*)gtk_builder_get_object( builder,
                                                        "margin_pad" );

        // wallpaper
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( data->show_wallpaper ),
                                      app_settings.show_wallpaper );
        if ( app_settings.wallpaper )
        {
            /* FIXME: GTK+ has a known bug here. Sometimes it doesn't update the preview...
             * so, we do it manually */
            set_preview_image( GTK_IMAGE( img_preview ), app_settings.wallpaper );
            gtk_file_chooser_set_filename( GTK_FILE_CHOOSER( data->wallpaper ),
                                           app_settings.wallpaper );
        }
        g_signal_connect( data->wallpaper_mode, "changed",
                                G_CALLBACK( on_wallpaper_mode_changed ), data );
        gtk_combo_box_set_active( (GtkComboBox*)data->wallpaper_mode,
                                                app_settings.wallpaper_mode );
        
        // checkboxes
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( data->show_wm_menu ),
                                      app_settings.show_wm_menu );
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( data->desk_single_click ),
                                      app_settings.desk_single_click );
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( data->desk_single_hover ),
                                      !app_settings.desk_no_single_hover );
        gtk_widget_set_sensitive( data->desk_single_hover,
                                        app_settings.desk_single_click );
        g_signal_connect( data->desk_single_click, "toggled",
                                G_CALLBACK( on_desk_single_click_toggled ), data );
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( data->desk_open_mime ),
                                      app_settings.desk_open_mime );
        
        //font
        font_button_set_font( data->desk_font, app_settings.desk_font, NULL );
        g_signal_connect( data->desk_font, "clicked",
                                G_CALLBACK(on_font_button_clicked), data );
        
        // colors
        data->bg_color1 = (GtkWidget*)gtk_builder_get_object( builder, "bg_color1" );
        data->text_color = (GtkWidget*)gtk_builder_get_object( builder, "text_color" );
        data->shadow_color = (GtkWidget*)gtk_builder_get_object( builder, "shadow_color" );
        gtk_color_button_set_color(GTK_COLOR_BUTTON(data->bg_color1),
                                   &app_settings.desktop_bg1);
        gtk_color_button_set_color(GTK_COLOR_BUTTON(data->text_color),
                                   &app_settings.desktop_text);
        gtk_color_button_set_color(GTK_COLOR_BUTTON(data->shadow_color),
                                   &app_settings.desktop_shadow);

        // wallpaper signals
        on_wallpaper_toggled( GTK_TOGGLE_BUTTON( data->show_wallpaper ), data );
        g_signal_connect( data->show_wallpaper, "toggled",
                                G_CALLBACK( on_wallpaper_toggled ), data );
        
        // margins
        str = g_strdup_printf( "%d", app_settings.margin_top );
        gtk_entry_set_text( GTK_ENTRY( data->margin_top ), str );
        g_free( str );
        str = g_strdup_printf( "%d", app_settings.margin_right );
        gtk_entry_set_text( GTK_ENTRY( data->margin_right ), str );
        g_free( str );
        str = g_strdup_printf( "%d", app_settings.margin_left );
        gtk_entry_set_text( GTK_ENTRY( data->margin_left ), str );
        g_free( str );
        str = g_strdup_printf( "%d", app_settings.margin_bottom );
        gtk_entry_set_text( GTK_ENTRY( data->margin_bottom ), str );
        g_free( str );
        str = g_strdup_printf( "%d", app_settings.margin_pad );
        gtk_entry_set_text( GTK_ENTRY( data->margin_pad ), str );
        g_free( str );


        // Advanced Tab ==================================================
     
        // terminal su
        int idx;
        GtkTreeIter it;
        char* custom_su = NULL;
        char* use_su;
        data->su_command = (GtkWidget*)gtk_builder_get_object( builder,
                                                                "su_command" );
        use_su = xset_get_s( "su_command" );
        if ( settings_terminal_su )
            // get su from /etc/spacefm/spacefm.conf
            custom_su = g_find_program_in_path( settings_terminal_su );
        if ( custom_su )
        {
            GtkListStore* su_list = GTK_LIST_STORE( gtk_combo_box_get_model( 
                                        GTK_COMBO_BOX( data->su_command ) ) );
            gtk_list_store_prepend( su_list, &it );
            gtk_list_store_set( GTK_LIST_STORE( su_list ), &it, 0, custom_su,
                                                                        -1 );
        }
        if ( !use_su )
            idx = 0;
        else if ( custom_su && !g_strcmp0( custom_su, use_su ) )
            idx = 0;
        else
        {
            for ( i = 0; i < G_N_ELEMENTS( su_commands ); i++ )
            {
                if ( !strcmp( su_commands[i], use_su ) )
                    break;
            }
            if ( i == G_N_ELEMENTS( su_commands ) )
                idx = 0;
            else if ( custom_su )
                idx = i + 1;
            else
                idx = i;
        }
        gtk_combo_box_set_active( GTK_COMBO_BOX( data->su_command ), idx );
        g_free( custom_su );
        
        // graphical su
        char* custom_gsu = NULL;
        char* use_gsu;
        data->gsu_command = (GtkWidget*)gtk_builder_get_object( builder,
                                                                "gsu_command" );
        use_gsu = xset_get_s( "gsu_command" );
        if ( settings_graphical_su )
            // get gsu from /etc/spacefm/spacefm.conf
            custom_gsu = g_find_program_in_path( settings_graphical_su );
#ifdef PREFERABLE_SUDO_PROG
        if ( !custom_gsu )
            // get build-time gsu
            custom_gsu = g_find_program_in_path( PREFERABLE_SUDO_PROG );
#endif
        if ( custom_gsu )
        {
            GtkListStore* gsu_list = GTK_LIST_STORE( gtk_combo_box_get_model( 
                                        GTK_COMBO_BOX( data->gsu_command ) ) );
            gtk_list_store_prepend( gsu_list, &it );
            gtk_list_store_set( GTK_LIST_STORE( gsu_list ), &it, 0, custom_gsu,
                                                                        -1 );
        }
        
        if ( !use_gsu )
            idx = 0;
        else if ( custom_gsu && !g_strcmp0( custom_gsu, use_gsu ) )
            idx = 0;
        else
        {
            for ( i = 0; i < G_N_ELEMENTS( gsu_commands ); i++ )
            {
                if ( !strcmp( gsu_commands[i], use_gsu ) )
                    break;
            }
            if ( i == G_N_ELEMENTS( gsu_commands ) )
                idx = 0;
            else if ( custom_gsu )
                idx = i + 1;
            else
                idx = i;
        }
        gtk_combo_box_set_active( GTK_COMBO_BOX( data->gsu_command ), idx );
        g_free( custom_gsu );
    
        // date format
        data->date_format = (GtkWidget*)gtk_builder_get_object( builder,
                                                                "date_format" );
        data->date_display = (GtkWidget*)gtk_builder_get_object( builder,
                                                                "label_date_disp" );
        model = GTK_TREE_MODEL( gtk_list_store_new( 1, G_TYPE_STRING ) );
        gtk_combo_box_set_model( GTK_COMBO_BOX( data->date_format ), model );
        gtk_combo_box_set_entry_text_column( GTK_COMBO_BOX( data->date_format ), 0 );
        g_object_unref( model );
        for ( i = 0; i < G_N_ELEMENTS( date_formats ); ++i )
        {
            gtk_combo_box_text_append_text ( GTK_COMBO_BOX_TEXT( data->date_format ), date_formats[ i ] );
        }
        char* date_s = xset_get_s( "date_format" );
        if ( date_s )
        {
            for ( i = 0; i < G_N_ELEMENTS( date_formats ); ++i )
            {
                if ( 0 == strcmp( date_formats[ i ], date_s ) )
                    break;
            }
            if ( i >= G_N_ELEMENTS( date_formats ) )
            {
                gtk_combo_box_text_prepend_text ( GTK_COMBO_BOX_TEXT( data->date_format ), date_s );
                i = 0;
            }
            gtk_combo_box_set_active( GTK_COMBO_BOX( data->date_format ), i );
        }
        on_date_format_changed( NULL, data );
        g_signal_connect( data->date_format, "changed", G_CALLBACK( on_date_format_changed),
                                                                            data );

        // editors
        data->editor = (GtkWidget*)gtk_builder_get_object( builder,
                                                                "editor" );
        if ( xset_get_s( "editor" ) )
            gtk_entry_set_text( GTK_ENTRY( data->editor ), xset_get_s( "editor" ) );
        data->editor_terminal = (GtkWidget*)gtk_builder_get_object( builder,
                                                                "editor_terminal" );
        gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON( data->editor_terminal ),
                                                    xset_get_b( "editor" ) );
        data->root_editor = (GtkWidget*)gtk_builder_get_object( builder,
                                                                "root_editor" );
        if ( xset_get_s( "root_editor" ) )
            gtk_entry_set_text( GTK_ENTRY( data->root_editor ), xset_get_s( "root_editor" ) );
        data->root_editor_terminal = (GtkWidget*)gtk_builder_get_object( builder,
                                                            "root_editor_terminal" );
        gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON( data->root_editor_terminal ),
                                                    xset_get_b( "root_editor" ) );


        g_signal_connect( dlg, "response", G_CALLBACK(on_response), data );
        g_object_unref( builder );
    }

    // Set current Preferences page
    const int desktop_page_num = 2;
#ifndef DESKTOP_INTEGRATION
    // hide the Desktop tab if no desktop integration at build time
    gtk_widget_hide( gtk_notebook_get_nth_page(
                                           (GtkNotebook*)data->notebook,
                                            desktop_page_num ) );
    if ( page >= desktop_page_num )
        page++;
#endif
    // notebook page number 3 is permanently hidden Volume Management
    if ( page > desktop_page_num )
        page++;
    gtk_notebook_set_current_page( (GtkNotebook*)data->notebook, page );

    gtk_window_present( (GtkWindow*)data->dlg );
    return TRUE;
}

