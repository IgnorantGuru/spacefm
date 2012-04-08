/*
*  C Implementation: ptk-path-entry
*
* Description: A custom entry widget with auto-completion
*
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "ptk-path-entry.h"
#include <gdk/gdkkeysyms.h>
#include "vfs-file-info.h"  /* for vfs_file_resolve_path */
#include <string.h>
#include "settings.h"
#include "main-window.h"

enum
{
    COL_NAME,
    COL_PATH,
    N_COLS
};

/*
static GQuark use_hand_cursor = (GQuark)"hand_cursor";
#define is_hand_cursor_used( entry )    (g_object_get_qdata(entry, use_hand_cursor))
*/

static char*
get_cwd( GtkEntry* entry )
{
    const char* path = gtk_entry_get_text( entry );    
    if ( path[0] == '/' )
        return g_path_get_dirname( path );
    else if ( path[0] != '$' && path[0] != '+' && path[0] != '&'
                        && path[0] != '!' && path[0] != '\0' && path[0] != ' ' )
    {
        EntryData* edata = (EntryData*)g_object_get_data(
                                                    G_OBJECT( entry ), "edata" );
        if ( edata && edata->browser )
        {
            char* real_path = vfs_file_resolve_path( ptk_file_browser_get_cwd(
                                    edata->browser ), path );
            char* ret = g_path_get_dirname( real_path );
            g_free( real_path );
            return ret;            
        }
    }
    return NULL;
}

static gboolean match_func( GtkEntryCompletion *completion,
                                                     const gchar *key,
                                                     GtkTreeIter *it,
                                                     gpointer user_data)
{
    char* name = NULL;
    GtkTreeModel* model = gtk_entry_completion_get_model(completion);

    key = (const char*)g_object_get_data( G_OBJECT(completion), "fn" );
    gtk_tree_model_get( model, it, COL_NAME, &name, -1 );

    if( G_LIKELY(name) )
    {
        if( *key == 0 || 0 == g_ascii_strncasecmp( name, key, strlen(key) ) )
        {
            g_free( name );
            return TRUE;
        }
        g_free( name );
    }
    return FALSE;
}

static void update_completion( GtkEntry* entry,
                               GtkEntryCompletion* completion )
{
    char* new_dir, *fn;
    const char* old_dir;
    GtkListStore* list;
    const char *sep;

    sep = strrchr( gtk_entry_get_text(entry), '/' );
    if( sep )
        fn = (char*)sep + 1;
    else
        fn = (char*)gtk_entry_get_text(entry);
    g_object_set_data_full( G_OBJECT(completion), "fn", g_strdup(fn), (GDestroyNotify)g_free );

    new_dir = get_cwd( entry );
    old_dir = (const char*)g_object_get_data( (GObject*)completion, "cwd" );
    if( old_dir && new_dir && 0 == g_ascii_strcasecmp( old_dir, new_dir ) )
    {
        g_free( new_dir );
        return;
    }
    g_object_set_data_full( (GObject*)completion, "cwd",
                             new_dir, g_free );
    list = (GtkListStore*)gtk_entry_completion_get_model( completion );
    gtk_list_store_clear( list );
    if( new_dir )
    {
        GDir* dir;
        if( (dir = g_dir_open( new_dir, 0, NULL )) )
        {
            const char* name;
            while( (name = g_dir_read_name( dir )) )
            {
                char* full_path = g_build_filename( new_dir, name, NULL );
                if( g_file_test( full_path, G_FILE_TEST_IS_DIR ) )
                {
                    GtkTreeIter it;
                    char* disp_name = g_filename_display_basename( full_path );
                    gtk_list_store_append( list, &it );
                    gtk_list_store_set( list, &it, COL_NAME, disp_name, COL_PATH, full_path, -1 );
                    g_free( disp_name );
                }
                g_free( full_path );
            }
            g_dir_close( dir );

            gtk_entry_completion_set_match_func( completion, match_func, new_dir, NULL );
        }
        else
            gtk_entry_completion_set_match_func( completion, NULL, NULL, NULL );
    }
}

static void
on_changed( GtkEntry* entry, gpointer user_data )
{
    GtkEntryCompletion* completion;
    completion = gtk_entry_get_completion( entry );
    update_completion( entry, completion );
}

static gboolean
on_key_press( GtkWidget *entry, GdkEventKey* evt, EntryData* edata )
{    
    int keymod = ( evt->state & ( GDK_SHIFT_MASK | GDK_CONTROL_MASK |
                 GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK ) );
                 
    if( evt->keyval == GDK_Tab && !keymod )
    {
#if GTK_CHECK_VERSION(2, 8, 0)
        /* This API exists since gtk+ 2.6, but gtk+ 2.6.x seems to have bugs
           related to this API which cause crash.
           Reported in bug #1570063 by Orlando Fiol <fiolorlando@gmail.com>
        */
        gtk_entry_completion_insert_prefix( gtk_entry_get_completion(GTK_ENTRY(entry)) );
#endif
        gtk_editable_set_position( (GtkEditable*)entry, -1 );
        return TRUE;
    }
    else if ( ( evt->keyval == GDK_Up || evt->keyval == GDK_Down ) && !keymod )
    {
        const char* text = gtk_entry_get_text( GTK_ENTRY( entry ) );
        if ( text[0] != '$' && text[0] != '+' && text[0] != '&' && text[0] != '!' 
                                                            && text[0] != '\0' )
            return FALSE;  // pass non-command arrows to completion
        
        char* line = NULL;
        GList* l;
        if ( evt->keyval == GDK_Up )
        {
            if ( edata->current )
            {
                if ( text[0] != '\0' && strcmp( edata->current->data, text ) )
                {
                    if ( edata->editing )
                        g_free( edata->editing );
                    edata->editing = g_strdup( text );
                    l = g_list_last( edata->history );
                    line = (char*)l->data;
                    edata->current = l;
                }
                else if ( edata->current->prev )
                {
                    line = (char*)edata->current->prev->data;
                    edata->current = edata->current->prev;
                }
            }
            else if ( edata->history )
            {
                if ( edata->editing && ( text[0] == '\0' || !strcmp( text, "$ " ) ) )
                    line = edata->editing;
                else
                {
                    l = g_list_last( edata->history );
                    line = (char*)l->data;
                    edata->current = l;
                    
                    if ( text[0] != '\0' && strcmp( text, "$ " ) )
                    {
                        if ( edata->editing )
                            g_free( edata->editing );
                        edata->editing = g_strdup( text );
                    }
                }
            }
        }
        else  // GDK_Down
        {
            if ( edata->current && edata->current->next )
            {
                if ( strcmp( edata->current->data, text ) )
                {
                    if ( text[0] != '\0' )
                    {
                        if ( edata->editing )
                            g_free( edata->editing );
                        edata->editing = strdup( text );
                    }
                    line = "$ ";
                    edata->current = NULL;
                }
                else
                {
                    line = (char*)edata->current->next->data;
                    edata->current = edata->current->next;
                }
            }
            else if ( !strcmp( text, "$ " ) || text[0] == '\0' )
            {
                if ( edata->editing && strcmp( text, edata->editing ) )
                    line = edata->editing;
                else
                    line = "$ ";
                edata->current = NULL;
            }
            else
            {
                if ( edata->current && !strcmp( text, edata->current->data ) )
                    line = edata->editing ? edata->editing : "$ ";
                else
                {
                    if ( edata->editing )
                        g_free( edata->editing );
                    edata->editing = strdup( text );
                    line = "$ ";
                }
                edata->current = NULL;
            }
        }
        if ( line )
        {
            gtk_entry_set_text( GTK_ENTRY( entry ), line );
            gtk_editable_set_position( (GtkEditable*)entry, -1 );
        }
        return TRUE;
    }
    else if ( evt->keyval == GDK_Escape && !keymod )
    {
        const char* text = gtk_entry_get_text( GTK_ENTRY( entry ) );
        if ( text[0] == '$' || text[0] == '+' || text[0] == '&'
                    || text[0] == '!' || text[0] == '\0' || text[0] == ' ' )
        {
            const char* line;
            const char* text = gtk_entry_get_text( GTK_ENTRY( entry ) );
            const char* cwd = ptk_file_browser_get_cwd( edata->browser );
            if ( !strcmp( text, "$ " ) || text[0] == '\0' )
                line = cwd;
            /*
            else if ( !strcmp( text, cwd ) )
            {
                if ( edata->editing && strcmp( text, edata->editing ) )
                    line = edata->editing;
                else
                    line = "$ ";
            }
            */
            else
            {
                if ( edata->editing )
                    g_free( edata->editing );
                edata->editing = strdup( text );
                line = "$ ";
            }
            gtk_entry_set_text( GTK_ENTRY( entry ), line );
            gtk_editable_set_position( (GtkEditable*)entry, -1 );
            edata->current = NULL;
            return TRUE;   
        }
    }
    else if ( evt->keyval == GDK_BackSpace && keymod == 1 ) // shift
    {
        gtk_entry_set_text( GTK_ENTRY( entry ), "" );
        return TRUE;
    }
    return FALSE;
}

static gboolean
on_focus_in( GtkWidget *entry, GdkEventFocus* evt, gpointer user_data )
{
    GtkEntryCompletion* completion = gtk_entry_completion_new();
    GtkListStore* list = gtk_list_store_new( N_COLS, G_TYPE_STRING, G_TYPE_STRING );
    GtkCellRenderer* render;

    gtk_entry_completion_set_minimum_key_length( completion, 1 );
    gtk_entry_completion_set_model( completion, GTK_TREE_MODEL(list) );
    g_object_unref( list );

    /* gtk_entry_completion_set_text_column( completion, COL_PATH ); */
    g_object_set( completion, "text-column", COL_PATH, NULL );
    render = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start( (GtkCellLayout*)completion, render, TRUE );
    gtk_cell_layout_add_attribute( (GtkCellLayout*)completion, render, "text", COL_NAME );

    gtk_entry_completion_set_inline_completion( completion, TRUE );
#if GTK_CHECK_VERSION( 2, 8, 0)
    /* gtk+ prior to 2.8.0 doesn't have this API */
    gtk_entry_completion_set_popup_set_width( completion, TRUE );
#endif
    gtk_entry_set_completion( GTK_ENTRY(entry), completion );
    g_signal_connect( G_OBJECT(entry), "changed", G_CALLBACK(on_changed), NULL );
    g_object_unref( completion );

    return FALSE;
}

static gboolean
on_focus_out( GtkWidget *entry, GdkEventFocus* evt, gpointer user_data )
{
    g_signal_handlers_disconnect_by_func( entry, on_changed, NULL );
    gtk_entry_set_completion( GTK_ENTRY(entry), NULL );
    return FALSE;
}

#if 0
/* Weird!  We cannot change the cursor of GtkEntry... */

static gboolean on_mouse_move(GtkWidget      *entry,
                                                             GdkEventMotion *evt,
                                                             gpointer        user_data)
{
    if( evt->state == GDK_CONTROL_MASK )
    {
        if( ! is_hand_cursor_used( entry ) )
        {
            GdkCursor* hand = gdk_cursor_new_for_display( gtk_widget_get_display(entry), GDK_HAND2 );
            gdk_window_set_cursor( entry->window, hand );
            gdk_cursor_unref( hand );
            g_object_set_qdata( entry, use_hand_cursor, (gpointer)TRUE );
            g_debug( "SET" );
        }
        return TRUE;
    }
    else
    {
        if( is_hand_cursor_used( entry ) )
        {
            gdk_window_set_cursor( entry->window, NULL );
            g_object_set_qdata( entry, use_hand_cursor, (gpointer)FALSE );
            g_debug( "UNSET" );
        }
    }
    return FALSE;
}

static gboolean on_button_press(GtkWidget      *entry,
                                                                 GdkEventButton *evt,
                                                                 gpointer        user_data)
{
    return FALSE;
}
#endif

void ptk_path_entry_help( GtkWidget* widget, GtkWidget* parent )
{
    GtkWidget* parent_win = gtk_widget_get_toplevel( GTK_WIDGET( parent ) );
    GtkWidget* dlg = gtk_message_dialog_new( GTK_WINDOW( parent_win ),
                                  GTK_DIALOG_MODAL,
                                  GTK_MESSAGE_INFO,
                                  GTK_BUTTONS_OK,
                                  "In addition to a folder or file path, commands can be entered in the Path Bar.  Prefixes:\n\t$\trun as task\n\t&\trun and forget\n\t+\trun in terminal\n\t!\trun as root\nUse:\n\t%%F\tselected files  or  %%f first selected file\n\t%%N\tselected filenames  or  %%n first selected filename\n\t%%d\tcurrent directory\n\t%%v\tselected device (eg /dev/sda1)\n\t%%m\tdevice mount point (eg /media/dvd);  %%l device label\n\t%%b\tselected bookmark\n\t%%t\tselected task directory;  %%p task pid\n\t%%a\tmenu item value\n\t$fm_panel, $fm_tab, $fm_command, etc\n\nExample:  $ echo \"Current Directory: %%d\"\nExample:  +! umount %v" );
    gtk_window_set_title( GTK_WINDOW( dlg ), "Path Bar Help" );
    gtk_dialog_run( GTK_DIALOG( dlg ) );
    gtk_widget_destroy( dlg );
}

static gboolean on_button_release(GtkEntry      *entry,
                                                                    GdkEventButton *evt,
                                                                    gpointer        user_data)
{
    if ( GDK_BUTTON_RELEASE != evt->type )
        return FALSE;
    if ( ( ( evt->state & GDK_CONTROL_MASK ) && 1 == evt->button ) )
    {
        int pos;
        const char *text, *sep;
        char *path;

        text = gtk_entry_get_text( entry );
        if ( !( text[0] == '$' || text[0] == '+' || text[0] == '&'
                  || text[0] == '!' || text[0] == '\0' ) )
        {
            pos = gtk_editable_get_position( GTK_EDITABLE(entry) );
            if( G_LIKELY( text && *text ) )
            {
                sep = g_utf8_offset_to_pointer( text, pos );
                if( G_LIKELY( sep ) )
                {
                    while( *sep && *sep != '/' )
                        sep = g_utf8_next_char(sep);
                    if( G_UNLIKELY( sep == text ) )
                    {
                        if( '/' == *sep )
                            ++sep;
                        else
                            return FALSE;
                    }
                    path = g_strndup( text, (sep - text) );
                    gtk_entry_set_text( entry, path );
                    g_free( path );

                    gtk_widget_activate( (GtkWidget*)entry );
                }
            }
        }
    }
    return FALSE;
}


void on_populate_popup( GtkEntry *entry, GtkMenu *menu, PtkFileBrowser* file_browser )
{
    if ( !file_browser )
        return;
    XSetContext* context = xset_context_new();
    main_context_fill( file_browser, context );

    GtkAccelGroup* accel_group = gtk_accel_group_new();
    XSet* set = xset_get( "sep_entry" );
    xset_add_menuitem( NULL, file_browser, GTK_WIDGET( menu ), accel_group, set );
    set = xset_set_cb_panel( file_browser->mypanel, "font_path", main_update_fonts, file_browser );
    xset_add_menuitem( NULL, file_browser, GTK_WIDGET( menu ), accel_group, set );
    set = xset_set_cb( "path_help", ptk_path_entry_help, file_browser );
    xset_add_menuitem( NULL, file_browser, GTK_WIDGET( menu ), accel_group, set );
    gtk_widget_show_all( GTK_WIDGET( menu ) );
    g_signal_connect( menu, "key-press-event",
                      G_CALLBACK( xset_menu_keypress ), NULL );
}

void on_entry_insert( GtkEntryBuffer *buf, guint position, gchar *chars,
                                            guint n_chars, gpointer user_data )
{   // remove linefeeds from pasted text
    if ( !strchr( gtk_entry_buffer_get_text( buf ), '\n' ) )
        return;

    char* new_text = replace_string( gtk_entry_buffer_get_text( buf ), "\n", "", FALSE );
    gtk_entry_buffer_set_text( buf, new_text, -1 );
    g_free( new_text );
}

void entry_data_free( EntryData* edata )
{
    if ( edata->history != NULL )
    {
        g_list_foreach( edata->history, ( GFunc ) g_free, NULL );
        g_list_free( edata->history );
    }
    if ( edata->editing )
        g_free( edata->editing );
    g_slice_free( EntryData, edata );
}

GtkWidget* ptk_path_entry_new( PtkFileBrowser* file_browser )
{
    GtkWidget* entry = gtk_entry_new();
    gtk_entry_set_has_frame( GTK_ENTRY( entry ), TRUE );
    
    // set font
    if ( xset_get_s_panel( file_browser->mypanel, "font_path" ) )
    {
        PangoFontDescription* font_desc = pango_font_description_from_string(
                        xset_get_s_panel( file_browser->mypanel, "font_path" ) );
        gtk_widget_modify_font( entry, font_desc );
        pango_font_description_free( font_desc );
    }

    EntryData* edata = g_slice_new0( EntryData );
    edata->history = NULL;
    edata->current = NULL;
    edata->editing = NULL;
    edata->browser = file_browser;
    
    g_signal_connect( entry, "focus-in-event", G_CALLBACK(on_focus_in), NULL );
    g_signal_connect( entry, "focus-out-event", G_CALLBACK(on_focus_out), NULL );

    /* used to eat the tab key */
    g_signal_connect( entry, "key-press-event", G_CALLBACK(on_key_press), edata );

/*
    g_signal_connect( entry, "motion-notify-event", G_CALLBACK(on_mouse_move), NULL );
    g_signal_connect( entry, "button-press-event", G_CALLBACK(on_button_press), NULL );
*/
    g_signal_connect( entry, "button-release-event", G_CALLBACK(on_button_release), NULL );
    g_signal_connect( entry, "populate-popup", G_CALLBACK(on_populate_popup), file_browser );

    g_signal_connect_after( G_OBJECT( gtk_entry_get_buffer( GTK_ENTRY( entry ) ) ),
                                        "inserted-text",
                                        G_CALLBACK( on_entry_insert ), NULL );

    g_object_weak_ref( G_OBJECT( entry ), (GWeakNotify) entry_data_free, edata );
    g_object_set_data( G_OBJECT( entry ), "edata", edata );
    return entry;
}
