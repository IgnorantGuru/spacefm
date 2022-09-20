/*
*  C Implementation: ptk-file-misc
*
* Description: Miscellaneous GUI-realated functions for files
*
*
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "ptk-file-misc.h"

#include <glib.h>
#include "glib-mem.h"

#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h> /* for realpath */

#include <glib/gi18n.h>
#include "ptk-utils.h"
#include "ptk-file-task.h"
#include "vfs-file-task.h"
#include "ptk-file-properties.h"
#include "ptk-input-dialog.h"
#include "ptk-file-browser.h"
#include "vfs-app-desktop.h"
#include "vfs-execute.h"
#include "ptk-app-chooser.h"
#include "ptk-clipboard.h"
#include "ptk-file-archiver.h"
#include "ptk-location-view.h"
#include <gdk/gdkkeysyms.h>

#include "settings.h"
#include "ptk-handler.h"
#include "gtk2-compat.h"

typedef struct
{
    PtkFileBrowser* file_browser;
    DesktopWindow* desktop;
    const char* cwd;
} ParentInfo;

typedef struct
{
    char* full_path;
    const char* old_path;
    char* new_path;
    char* desc;
    gboolean is_dir;
    gboolean is_link;
    gboolean clip_copy;
    PtkRenameMode create_new;
    
    GtkWidget* dlg;
    GtkWidget* parent;
    PtkFileBrowser* browser;
    DesktopWindow* desktop;
    
    GtkLabel* label_type;
    GtkLabel* label_mime;
    GtkWidget* hbox_type;
    char* mime_type;

    GtkLabel* label_target;
    GtkEntry* entry_target;
    GtkWidget* hbox_target;
    GtkWidget* browse_target;

    GtkLabel* label_template;
    GtkComboBox* combo_template;
    GtkComboBox* combo_template_dir;
    GtkWidget* hbox_template;
    GtkWidget* browse_template;

    GtkLabel* label_name;
    GtkWidget* scroll_name;
    GtkWidget* input_name;
    GtkTextBuffer* buf_name;
    GtkLabel* blank_name;

    GtkWidget* hbox_ext;
    GtkLabel* label_ext;
    GtkEntry* entry_ext;

    GtkLabel* label_full_name;
    GtkWidget* scroll_full_name;
    GtkWidget* input_full_name;
    GtkTextBuffer* buf_full_name;
    GtkLabel* blank_full_name;
    
    GtkLabel* label_path;
    GtkWidget* scroll_path;
    GtkWidget* input_path;
    GtkTextBuffer* buf_path;
    GtkLabel* blank_path;

    GtkLabel* label_full_path;
    GtkWidget* scroll_full_path;
    GtkWidget* input_full_path;
    GtkTextBuffer* buf_full_path;

    GtkWidget* opt_move;
    GtkWidget* opt_copy;
    GtkWidget* opt_link;
    GtkWidget* opt_copy_target;
    GtkWidget* opt_link_target;
    GtkWidget* opt_as_root;

    GtkWidget* opt_new_file;
    GtkWidget* opt_new_folder;
    GtkWidget* opt_new_link;
    
    GtkWidget* options;
    GtkWidget* browse;
    GtkWidget* revert;
    GtkWidget* cancel;
    GtkWidget* next;
    GtkWidget* open;
    
    GtkWidget* last_widget;
    
    gboolean full_path_exists;
    gboolean full_path_exists_dir;
    gboolean full_path_same;
    gboolean path_missing;
    gboolean path_exists_file;
    gboolean mode_change;
    gboolean is_move;
} MoveSet;

void on_toggled( GtkMenuItem* item, MoveSet* mset );
char* get_template_dir();

void ptk_delete_files( GtkWindow* parent_win,
                       const char* cwd,
                       GList* sel_files, GtkTreeView* task_view )
{
    GtkWidget * dlg;
    gchar* file_path;
    gint ret;
    GList* sel;
    VFSFileInfo* file;
    PtkFileTask* task = NULL;
    GList* file_list;

    if ( ! sel_files )
        return ;

    if( ! app_settings.use_trash_can && ! app_settings.no_confirm ) //MOD
    {
        // count
        int count = g_list_length( sel_files );
        char* msg = g_strdup_printf( ngettext( "Delete %d selected item ?",
                                               "Delete %d selected items ?",
                                               count ), count );
        dlg = gtk_message_dialog_new( parent_win,
                                      GTK_DIALOG_MODAL,
                                      GTK_MESSAGE_WARNING,
                                      GTK_BUTTONS_YES_NO,
                                      msg, NULL );
        gtk_dialog_set_default_response (GTK_DIALOG (dlg), GTK_RESPONSE_YES); //MOD
        gtk_window_set_title( GTK_WINDOW( dlg ), _("Confirm Delete") );
        xset_set_window_icon( GTK_WINDOW( dlg ) );

        ret = gtk_dialog_run( GTK_DIALOG( dlg ) );
        gtk_widget_destroy( dlg );
        g_free( msg );
        if ( ret != GTK_RESPONSE_YES )
            return ;
    }

    file_list = NULL;
    for ( sel = sel_files; sel; sel = g_list_next( sel ) )
    {
        file = ( VFSFileInfo* ) sel->data;
        file_path = g_build_filename( cwd,
                                      vfs_file_info_get_name( file ), NULL );
        file_list = g_list_prepend( file_list, file_path );
    }
    /* file_list = g_list_reverse( file_list ); */
    task = ptk_file_task_new( app_settings.use_trash_can ? VFS_FILE_TASK_TRASH : VFS_FILE_TASK_DELETE,
                              file_list,
                              NULL,
                              parent_win ? GTK_WINDOW( parent_win ) : NULL,
                              GTK_WIDGET( task_view ) );
    ptk_file_task_run( task );
}

char* get_real_link_target( const char* link_path )
{
    char buf[ PATH_MAX + 1 ];
    char* canon;
    ssize_t len;
    struct stat64 stat;
    char* target_path;

    if ( !link_path )
        return NULL;
    
    // canonicalize target
    if ( !( target_path = g_strdup( realpath( link_path, buf ) ) ) )
    {
        /* fall back to immediate target if canonical target
         * missing.
         * g_file_read_link() doesn't behave like readlink,
         * gives nothing if final target missing */
        len = readlink( link_path, buf, PATH_MAX );
        if ( len > 0 )
            target_path = g_strndup( buf, len );
    }
    return target_path;
}

static void select_file_name_part( GtkEntry* entry )
{
    GtkEditable * editable = GTK_EDITABLE( entry );
    const char* file_name = gtk_entry_get_text( entry );
    const char* dot;
    int pos;

    if ( !file_name[ 0 ] || file_name[ 0 ] == '.' )
        return ;
    /* FIXME: Simply finding '.' usually gets wrong filename suffix. */
    dot = g_utf8_strrchr( file_name, -1, '.' );
    if ( dot )
    {
        pos = g_utf8_pointer_to_offset( file_name, dot );
        gtk_editable_select_region( editable, 0, pos );
    }
}

void on_help_activate( GtkMenuItem* item, MoveSet* mset )
{
    xset_show_help( GTK_WIDGET( mset->dlg ), NULL,
                        mset->create_new ? "#gui-newf" : "#gui-rename" );
}

static gboolean on_move_keypress ( GtkWidget *widget, GdkEventKey *event,
                                                                MoveSet* mset )
{
    int keymod = ( event->state & ( GDK_SHIFT_MASK | GDK_CONTROL_MASK |
             GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK ) );

    if ( keymod == 0 )
    {
        if ( event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter )
        {
            if ( gtk_widget_get_sensitive( GTK_WIDGET( mset->next ) ) )
                gtk_dialog_response( GTK_DIALOG( mset->dlg ), GTK_RESPONSE_OK );
            return TRUE;
        }
        else if ( event->keyval == GDK_KEY_F1 )
        {
            on_help_activate( NULL, mset );
            return TRUE;
        }
    }
    return FALSE;
}

static gboolean on_move_entry_keypress ( GtkWidget *widget, GdkEventKey *event,
                                                                MoveSet* mset )
{
    int keymod = ( event->state & ( GDK_SHIFT_MASK | GDK_CONTROL_MASK |
             GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK ) );

    if ( keymod == 0 )
    {
        if ( event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter )
        {
            if ( gtk_widget_get_sensitive( GTK_WIDGET( mset->next ) ) )
                gtk_dialog_response( GTK_DIALOG( mset->dlg ), GTK_RESPONSE_OK );
            return TRUE;
        }
        else if ( event->keyval == GDK_KEY_F1 )
        {
            on_help_activate( NULL, mset );
            return TRUE;
        }
    }
    return FALSE;
}

void on_move_change( GtkWidget* widget, MoveSet* mset )
{
    char* name;
    char* full_name;
    char* ext;
    char* full_path;
    char* path;
    char* cwd;
    char* str;
    GtkTextIter iter, siter;

    g_signal_handlers_block_matched( mset->entry_ext, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                     on_move_change, NULL );
    g_signal_handlers_block_matched( mset->buf_name, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                     on_move_change, NULL );
    g_signal_handlers_block_matched( mset->buf_full_name, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                     on_move_change, NULL );
    g_signal_handlers_block_matched( mset->buf_path, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                     on_move_change, NULL );
    g_signal_handlers_block_matched( mset->buf_full_path, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                     on_move_change, NULL );

    // change is_dir to reflect state of new folder or link option
    if ( mset->create_new )
    {
        gboolean new_folder = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
                                                    mset->opt_new_folder ) );
        gboolean new_link = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
                                                    mset->opt_new_link ) );
        if ( new_folder || ( new_link && 
                             g_file_test( gtk_entry_get_text( 
                                             GTK_ENTRY( mset->entry_target ) ),
                                          G_FILE_TEST_IS_DIR ) &&
                             gtk_entry_get_text( GTK_ENTRY( 
                                          mset->entry_target ) )[0] == '/' ) )
        {
             if ( !mset->is_dir )
                mset->is_dir = TRUE;
        }
        else if ( mset->is_dir )
            mset->is_dir = FALSE;
        if ( mset->is_dir && 
                        gtk_widget_is_focus( GTK_WIDGET( mset->entry_ext ) ) )
            gtk_widget_grab_focus( GTK_WIDGET( mset->input_name ) );
        gtk_widget_set_sensitive( GTK_WIDGET( mset->entry_ext ), !mset->is_dir );
        gtk_widget_set_sensitive( GTK_WIDGET( mset->label_ext ), !mset->is_dir );
    }
    
    if ( widget == GTK_WIDGET( mset->buf_name ) || widget == GTK_WIDGET( mset->entry_ext ) )
    {
        if ( widget == GTK_WIDGET( mset->buf_name ) )
            mset->last_widget = GTK_WIDGET( mset->input_name );
        else
            mset->last_widget = GTK_WIDGET( mset->entry_ext );
            
        gtk_text_buffer_get_start_iter( mset->buf_name, &siter );
        gtk_text_buffer_get_end_iter( mset->buf_name, &iter );
        name = gtk_text_buffer_get_text( mset->buf_name, &siter, &iter, FALSE );
        if ( name[0] == '\0' )
        {
            g_free( name );
            name = NULL;
        }
        ext = (char*)gtk_entry_get_text( mset->entry_ext );
        if ( ext[0] == '\0' )
        {
            ext = NULL;
        }
        else if ( ext[0] == '.' )
            ext++;  // ignore leading dot in extension field
            
        // update full_name
        if ( name && ext )
            full_name = g_strdup_printf( "%s.%s", name, ext );
        else if ( name && !ext )
            full_name = g_strdup( name );
        else if ( !name && ext )
            full_name = g_strdup( ext );
        else
            full_name = g_strdup( "" );
        if ( name )
            g_free( name );
        gtk_text_buffer_set_text( mset->buf_full_name, full_name, -1 );

        // update full_path
        gtk_text_buffer_get_start_iter( mset->buf_path, &siter );
        gtk_text_buffer_get_end_iter( mset->buf_path, &iter );
        path = gtk_text_buffer_get_text( mset->buf_path, &siter, &iter, FALSE );
        if ( !strcmp( path, "." ) )
        {
            g_free( path );
            path = g_path_get_dirname( mset->full_path );
        }
        else if ( !strcmp( path, ".." ) )
        {
            g_free( path );
            cwd = g_path_get_dirname( mset->full_path );
            path = g_path_get_dirname( cwd );
            g_free( cwd );
        }

        if ( path[0] == '/' )
            full_path = g_build_filename( path, full_name, NULL );
        else
        {
            cwd = g_path_get_dirname( mset->full_path );
            full_path = g_build_filename( cwd, path, full_name, NULL );
            g_free( cwd );
        }
        gtk_text_buffer_set_text( mset->buf_full_path, full_path, -1 );
        
        g_free( full_name );
    }   
    else if ( widget == GTK_WIDGET( mset->buf_full_name ) )
    {
        mset->last_widget = GTK_WIDGET( mset->input_full_name );
        // update name & ext
        gtk_text_buffer_get_start_iter( mset->buf_full_name, &siter );
        gtk_text_buffer_get_end_iter( mset->buf_full_name, &iter );
        full_name = gtk_text_buffer_get_text( mset->buf_full_name, &siter, &iter, FALSE );
        name = get_name_extension( full_name, mset->is_dir, &ext );
        gtk_text_buffer_set_text( mset->buf_name, name, -1 );
        if ( ext )
            gtk_entry_set_text( mset->entry_ext, ext );
        else
            gtk_entry_set_text( mset->entry_ext, "" );
        g_free( name );
        g_free( ext );

        // update full_path
        gtk_text_buffer_get_start_iter( mset->buf_path, &siter );
        gtk_text_buffer_get_end_iter( mset->buf_path, &iter );
        path = gtk_text_buffer_get_text( mset->buf_path, &siter, &iter, FALSE );
        if ( !strcmp( path, "." ) )
        {
            g_free( path );
            path = g_path_get_dirname( mset->full_path );
        }
        else if ( !strcmp( path, ".." ) )
        {
            g_free( path );
            cwd = g_path_get_dirname( mset->full_path );
            path = g_path_get_dirname( cwd );
            g_free( cwd );
        }
        
        if ( path[0] == '/' )
            full_path = g_build_filename( path, full_name, NULL );
        else
        {
            cwd = g_path_get_dirname( mset->full_path );
            full_path = g_build_filename( cwd, path, full_name, NULL );
            g_free( cwd );
        }
        gtk_text_buffer_set_text( mset->buf_full_path, full_path, -1 );
        
        g_free( full_name );
    }
    else if ( widget == GTK_WIDGET( mset->buf_path ) )
    {
        mset->last_widget = GTK_WIDGET( mset->input_path );
        // update full_path
        gtk_text_buffer_get_start_iter( mset->buf_full_name, &siter );
        gtk_text_buffer_get_end_iter( mset->buf_full_name, &iter );
        full_name = gtk_text_buffer_get_text( mset->buf_full_name, &siter, &iter, FALSE );

        gtk_text_buffer_get_start_iter( mset->buf_path, &siter );
        gtk_text_buffer_get_end_iter( mset->buf_path, &iter );
        path = gtk_text_buffer_get_text( mset->buf_path, &siter, &iter, FALSE );
        if ( !strcmp( path, "." ) )
        {
            g_free( path );
            path = g_path_get_dirname( mset->full_path );
        }
        else if ( !strcmp( path, ".." ) )
        {
            g_free( path );
            cwd = g_path_get_dirname( mset->full_path );
            path = g_path_get_dirname( cwd );
            g_free( cwd );
        }
        
        if ( path[0] == '/' )
            full_path = g_build_filename( path, full_name, NULL );
        else
        {
            cwd = g_path_get_dirname( mset->full_path );
            full_path = g_build_filename( cwd, path, full_name, NULL );
            g_free( cwd );
        }
        gtk_text_buffer_set_text( mset->buf_full_path, full_path, -1 );
 
        g_free( full_name );
    }
    else //if ( widget == GTK_WIDGET( mset->buf_full_path ) )
    {
        mset->last_widget = GTK_WIDGET( mset->input_full_path );
        gtk_text_buffer_get_start_iter( mset->buf_full_path, &siter );
        gtk_text_buffer_get_end_iter( mset->buf_full_path, &iter );
        full_path = gtk_text_buffer_get_text( mset->buf_full_path, &siter, &iter, FALSE );

        // update name & ext
        if ( full_path[0] == '\0' )
            full_name = g_strdup( "" );
        else
            full_name = g_path_get_basename( full_path );
        
        path = g_path_get_dirname( full_path );
        if ( !strcmp( path, "." ) )
        {
            g_free( path );
            path = g_path_get_dirname( mset->full_path );
        }
        else if ( !strcmp( path, ".." ) )
        {
            g_free( path );
            cwd = g_path_get_dirname( mset->full_path );
            path = g_path_get_dirname( cwd );
            g_free( cwd );
        }
        else if ( path[0] != '/' )
        {
            cwd = g_path_get_dirname( mset->full_path );
            str = path;
            path = g_build_filename( cwd, str, NULL );
            g_free( str );
            g_free( cwd );
        }
        name = get_name_extension( full_name, mset->is_dir, &ext );
        gtk_text_buffer_set_text( mset->buf_name, name, -1 );
        if ( ext )
            gtk_entry_set_text( mset->entry_ext, ext );
        else
            gtk_entry_set_text( mset->entry_ext, "" );
        
        // update full_name
        if ( name && ext )
            full_name = g_strdup_printf( "%s.%s", name, ext );
        else if ( name && !ext )
            full_name = g_strdup( name );
        else if ( !name && ext )
            full_name = g_strdup( ext );
        else
            full_name = g_strdup( "" );
        g_free( name );
        g_free( ext );
        gtk_text_buffer_set_text( mset->buf_full_name, full_name, -1 );
        
        // update path
        gtk_text_buffer_set_text( mset->buf_path, path, -1 );
        
        if ( full_path[0] != '/' )
        {
            // update full_path for tests below
            cwd = g_path_get_dirname( mset->full_path );
            str = full_path;
            full_path = g_build_filename( cwd, str, NULL );
            g_free( str );
            g_free( cwd );
        }
        
        g_free( full_name );
    }

    // change relative path to absolute
    if ( path[0] != '/' )
    {
        g_free( path );
        path = g_path_get_dirname( full_path );
    }

//printf("path=%s   full=%s\n", path, full_path );

    // tests
    struct stat64 statbuf;
    gboolean full_path_exists = FALSE;
    gboolean full_path_exists_dir = FALSE;
    gboolean full_path_same = FALSE;
    gboolean path_missing = FALSE;
    gboolean path_exists_file = FALSE;
    gboolean is_move = FALSE;
    
    if ( !strcmp( full_path, mset->full_path ) )
    {
        full_path_same = TRUE;
        if ( mset->create_new && 
                    gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
                                                        mset->opt_new_link ) ) )
        {
            if ( lstat64( full_path, &statbuf ) == 0 )
            {
                full_path_exists = TRUE;
                if ( g_file_test( full_path, G_FILE_TEST_IS_DIR ) )
                    full_path_exists_dir = TRUE;
            }
        }
    }
    else
    {
        if ( lstat64( full_path, &statbuf ) == 0 )
        {
            full_path_exists = TRUE;
            if ( g_file_test( full_path, G_FILE_TEST_IS_DIR ) )
                full_path_exists_dir = TRUE;
        }
        else if ( lstat64( path, &statbuf ) == 0 )
        {
            if ( !g_file_test( path, G_FILE_TEST_IS_DIR ) )
                path_exists_file = TRUE;
        }
        else
            path_missing = TRUE;
            
        if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( mset->opt_move ) ) )
        {
            is_move = strcmp( path, mset->old_path );
        }
    }
    g_free( path );
    g_free( full_path );


/*
    printf( "\nTEST\n    full_path_same %d %d\n", full_path_same, mset->full_path_same );
    printf( "    full_path_exists %d %d\n", full_path_exists, mset->full_path_exists );
    printf( "    full_path_exists_dir %d %d\n", full_path_exists_dir, mset->full_path_exists_dir );
    printf( "    path_missing %d %d\n", path_missing, mset->path_missing );
    printf( "    path_exists_file %d %d\n", path_exists_file, mset->path_exists_file );
*/
    // update display
    if ( mset->full_path_same != full_path_same
                || mset->full_path_exists != full_path_exists
                || mset->full_path_exists_dir != full_path_exists_dir
                || mset->path_missing != path_missing
                || mset->path_exists_file != path_exists_file
                || mset->mode_change )
    {
        // state change
        mset->full_path_exists = full_path_exists;
        mset->full_path_exists_dir = full_path_exists_dir;
        mset->path_missing = path_missing;
        mset->path_exists_file = path_exists_file;
        mset->full_path_same = full_path_same;
        mset->mode_change = FALSE;

        //gboolean opt_move = gtk_toggle_button_get_active( mset->opt_move );
        //gboolean opt_copy = gtk_toggle_button_get_active( mset->opt_copy );
        //gboolean opt_link = gtk_toggle_button_get_active( mset->opt_link );
        //gboolean opt_copy_target = gtk_toggle_button_get_active( mset->opt_copy_target );
        //gboolean opt_link_target = gtk_toggle_button_get_active( mset->opt_link_target );

        if ( full_path_same && ( mset->create_new == PTK_RENAME ||
                                 mset->create_new == PTK_RENAME_NEW_LINK ) )
        {
            gtk_widget_set_sensitive( mset->next, gtk_toggle_button_get_active(
                                            GTK_TOGGLE_BUTTON( mset->opt_move ) ) );
            gtk_label_set_markup_with_mnemonic( mset->label_full_path,
                                    _("<b>P_ath:</b>   <i>original</i>") );
            gtk_label_set_markup_with_mnemonic( mset->label_name,
                                    _("<b>_Name:</b>   <i>original</i>") );
            gtk_label_set_markup_with_mnemonic( mset->label_full_name,
                                    _("<b>_Filename:</b>   <i>original</i>") );
            gtk_label_set_markup_with_mnemonic( mset->label_path,
                                            _("<b>_Parent:</b>   <i>original</i>") );
        }
        else if ( full_path_exists_dir )
        {
            gtk_widget_set_sensitive( mset->next, FALSE );
            gtk_label_set_markup_with_mnemonic( mset->label_full_path,
                                    _("<b>P_ath:</b>   <i>exists as folder</i>") );
            gtk_label_set_markup_with_mnemonic( mset->label_name,
                                    _("<b>_Name:</b>   <i>exists as folder</i>") );
            gtk_label_set_markup_with_mnemonic( mset->label_full_name,
                                    _("<b>_Filename:</b>   <i>exists as folder</i>") );
            gtk_label_set_markup_with_mnemonic( mset->label_path, _("<b>_Parent:</b>") );
        }
        else if ( full_path_exists )
        {
            if ( mset->is_dir )
            {
                gtk_widget_set_sensitive( mset->next, FALSE );
                gtk_label_set_markup_with_mnemonic( mset->label_full_path,
                                        _("<b>P_ath:</b>   <i>exists as file</i>") );
                gtk_label_set_markup_with_mnemonic( mset->label_name,
                                        _("<b>_Name:</b>   <i>exists as file</i>") );
                gtk_label_set_markup_with_mnemonic( mset->label_full_name,
                                        _("<b>_Filename:</b>   <i>exists as file</i>") );
                gtk_label_set_markup_with_mnemonic( mset->label_path, _("<b>_Parent:</b>") );
            }
            else
            {
                gtk_widget_set_sensitive( mset->next, TRUE );
                gtk_label_set_markup_with_mnemonic( mset->label_full_path,
                                _("<b>P_ath:</b>   <i>* overwrite existing file</i>") );
                gtk_label_set_markup_with_mnemonic( mset->label_name,
                                _("<b>_Name:</b>   <i>* overwrite existing file</i>") );
                gtk_label_set_markup_with_mnemonic( mset->label_full_name,
                                _("<b>_Filename:</b>   <i>* overwrite existing file</i>") );
                gtk_label_set_markup_with_mnemonic( mset->label_path, _("<b>_Parent:</b>") );
            }
        }
        else if ( path_exists_file )
        {
            gtk_widget_set_sensitive( mset->next, FALSE );
            gtk_label_set_markup_with_mnemonic( mset->label_full_path,
                                _("<b>P_ath:</b>   <i>parent exists as file</i>") );
            gtk_label_set_markup_with_mnemonic( mset->label_name,
                                _("<b>_Name:</b>") );
            gtk_label_set_markup_with_mnemonic( mset->label_full_name,
                                _("<b>_Filename:</b>") );
            gtk_label_set_markup_with_mnemonic( mset->label_path,
                                _("<b>_Parent:</b>   <i>parent exists as file</i>") );
        }
        else if ( path_missing )
        {
            gtk_widget_set_sensitive( mset->next, TRUE );
            gtk_label_set_markup_with_mnemonic( mset->label_full_path,
                                _("<b>P_ath:</b>   <i>* create parent</i>") );
            gtk_label_set_markup_with_mnemonic( mset->label_name,
                                _("<b>_Name:</b>") );
            gtk_label_set_markup_with_mnemonic( mset->label_full_name,
                                _("<b>_Filename:</b>") );
            gtk_label_set_markup_with_mnemonic( mset->label_path,
                                _("<b>_Parent:</b>   <i>* create parent</i>") );
        }
        else
        {
            gtk_widget_set_sensitive( mset->next, TRUE );
            gtk_label_set_markup_with_mnemonic( mset->label_full_path,
                                _("<b>P_ath:</b>") );
            gtk_label_set_markup_with_mnemonic( mset->label_name,
                                _("<b>_Name:</b>") );
            gtk_label_set_markup_with_mnemonic( mset->label_full_name,
                                _("<b>_Filename:</b>") );
            gtk_label_set_markup_with_mnemonic( mset->label_path,
                                _("<b>_Parent:</b>") );
        }        
    }

    if ( is_move != mset->is_move && !mset->create_new )
    {
        mset->is_move = is_move;
        if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( mset->opt_move ) ) )
            gtk_button_set_label( GTK_BUTTON( mset->next ),
                                        is_move != 0 ? _("_Move") : _("_Rename") );
    }

    if ( mset->create_new && 
        gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( mset->opt_new_link ) ) )
    {
        path = g_strdup( gtk_entry_get_text( GTK_ENTRY( mset->entry_target ) ) );
        g_strstrip( path );
        gtk_widget_set_sensitive( mset->next, ( path && path[0] != '\0' && 
                                !( full_path_same && full_path_exists ) &&
                                !full_path_exists_dir ) );
        g_free( path );
    }
    
    if ( mset->open )
        gtk_widget_set_sensitive( mset->open, gtk_widget_get_sensitive( mset->next ) );
    
    g_signal_handlers_unblock_matched( mset->entry_ext, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                     on_move_change, NULL );
    g_signal_handlers_unblock_matched( mset->buf_name, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                     on_move_change, NULL );
    g_signal_handlers_unblock_matched( mset->buf_full_name, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                     on_move_change, NULL );
    g_signal_handlers_unblock_matched( mset->buf_path, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                     on_move_change, NULL );
    g_signal_handlers_unblock_matched( mset->buf_full_path, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                     on_move_change, NULL );
}

void select_input( GtkWidget* widget, MoveSet* mset )
{
    if ( GTK_IS_EDITABLE( widget ) )
        gtk_editable_select_region( GTK_EDITABLE( widget ), 0, -1 );
    else if ( GTK_IS_COMBO_BOX( widget ) )
        gtk_editable_select_region( GTK_EDITABLE( GTK_ENTRY( gtk_bin_get_child(
                                                GTK_BIN( widget ) ) ) ), 0, -1 );
    else
    {
        GtkTextIter iter, siter;
        GtkTextBuffer* buf = gtk_text_view_get_buffer( GTK_TEXT_VIEW( widget ) );
        if ( widget == GTK_WIDGET( mset->input_full_name )
                && !gtk_widget_get_visible( gtk_widget_get_parent( mset->input_name ) ) )
        {
            // name is not visible so select name in filename
            gtk_text_buffer_get_start_iter( mset->buf_full_name, &siter );
            gtk_text_buffer_get_end_iter( mset->buf_full_name, &iter );
            char* full_name = gtk_text_buffer_get_text( mset->buf_full_name,
                                                        &siter, &iter, FALSE );
            char* ext;
            char* name = get_name_extension( full_name, mset->is_dir, &ext );
            g_free( ext );
            g_free( full_name );
            gtk_text_buffer_get_iter_at_offset( buf, &iter,
                                                    g_utf8_strlen( name, -1 ) );
            g_free( name );
        }
        else
            gtk_text_buffer_get_end_iter( buf, &iter );

        gtk_text_buffer_get_start_iter( buf, &siter );
        gtk_text_buffer_select_range( buf, &iter, &siter );
    }
}

static gboolean on_focus( GtkWidget* widget, GtkDirectionType direction, MoveSet* mset )
{
/*
    if ( direction == GTK_DIR_TAB_FORWARD || direction == GTK_DIR_TAB_BACKWARD )
    {
        if ( widget == mset->label_mime )
            return TRUE;
    }
*/
    select_input( widget, mset );
    return FALSE;
}

static gboolean on_button_focus( GtkWidget* widget, GtkDirectionType direction,
                                                                    MoveSet* mset )
{
    if ( direction == GTK_DIR_TAB_FORWARD || direction == GTK_DIR_TAB_BACKWARD )
    {
        if ( widget == mset->options || widget == mset->opt_move ||
                                                    widget == mset->opt_new_file )
        {
            GtkWidget* input = NULL;
            if ( gtk_widget_get_visible( gtk_widget_get_parent( mset->input_name ) ) )
                input = mset->input_name;
            else if ( gtk_widget_get_visible( gtk_widget_get_parent( mset->input_full_name ) ) )
                input = mset->input_full_name;
            else if ( gtk_widget_get_visible( gtk_widget_get_parent( mset->input_path ) ) )
                input = mset->input_path;
            else if ( gtk_widget_get_visible( gtk_widget_get_parent( mset->input_full_path ) ) )
                input = mset->input_full_path;
            else if ( gtk_widget_get_visible( gtk_widget_get_parent( GTK_WIDGET(
                                                        mset->entry_target ) ) ) )
                input = GTK_WIDGET( mset->entry_target );
            else if ( gtk_widget_get_visible( gtk_widget_get_parent( GTK_WIDGET(
                                                        mset->combo_template ) ) ) )
                input = GTK_WIDGET( mset->combo_template );
            else if ( gtk_widget_get_visible( gtk_widget_get_parent( GTK_WIDGET(
                                                        mset->combo_template_dir ) ) ) )
                input = GTK_WIDGET( mset->combo_template_dir );
            if ( input )
            {
                select_input( input, mset );
                gtk_widget_grab_focus( input );
            }
        }
        else
        {
            GtkWidget* input = NULL;
            if ( gtk_widget_get_visible( gtk_widget_get_parent( mset->input_full_path ) ) )
                input = mset->input_full_path;
            else if ( gtk_widget_get_visible( gtk_widget_get_parent( mset->input_path ) ) )
                input = mset->input_path;
            else if ( gtk_widget_get_visible( gtk_widget_get_parent( mset->input_full_name ) ) )
                input = mset->input_full_name;
            else if ( gtk_widget_get_visible( gtk_widget_get_parent( mset->input_name ) ) )
                input = mset->input_name;
            if ( input )
            {
                select_input( input, mset );
                gtk_widget_grab_focus( input );
            }
        }
        return TRUE;
    }
    return FALSE;
}

void on_revert_button_press( GtkWidget* widget, MoveSet* mset )
{
    GtkWidget* temp = mset->last_widget;
    gtk_text_buffer_set_text( mset->buf_full_path, mset->new_path, -1 );
    mset->last_widget = temp;
    select_input( mset->last_widget, mset );
    gtk_widget_grab_focus( mset->last_widget );
}

void on_create_browse_button_press( GtkWidget* widget, MoveSet* mset )
{
    int action;
    const char* title;
    const char* text;
    char* dir;
    char* name;
    char* new_path;
    
    if ( widget == GTK_WIDGET( mset->browse_target ) )
    {
        title = _("Select Link Target");
        action = GTK_FILE_CHOOSER_ACTION_OPEN;
        text = gtk_entry_get_text( mset->entry_target );
        if ( text[0] == '/' )
        {
            dir = g_path_get_dirname( text );
            name = g_path_get_basename( text );
        }
        else
        {
            dir = g_path_get_dirname( mset->full_path );
            name = text[0] != '\0' ? g_strdup( text ) : NULL;
        }
    }
    else if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( mset->opt_new_file ) ) )
    {
        title = _("Select Template File");
        action = GTK_FILE_CHOOSER_ACTION_OPEN;
        text = gtk_entry_get_text( GTK_ENTRY( gtk_bin_get_child( GTK_BIN(
                                                    mset->combo_template ) ) ) );
        if ( text && text[0] == '/' )
        {
            dir = g_path_get_dirname( text );
            name = g_path_get_basename( text );
        }
        else
        {
            dir = get_template_dir();
            if ( !dir )
                dir = g_path_get_dirname( mset->full_path );
            name = g_strdup( text );
        }
     }
    else
    {
        title = _("Select Template Folder");
        action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
        text = gtk_entry_get_text( GTK_ENTRY( gtk_bin_get_child( GTK_BIN(
                                                    mset->combo_template ) ) ) );
        if ( text && text[0] == '/' )
        {
            dir = g_path_get_dirname( text );
            name = g_path_get_basename( text );
        }
        else
        {
            dir = get_template_dir();
            if ( !dir )
                dir = g_path_get_dirname( mset->full_path );
            name = g_strdup( text );
        }
    }
        
    GtkWidget* dlg = gtk_file_chooser_dialog_new( title,
                           mset->parent ? GTK_WINDOW( mset->parent ) : NULL, 
                           action,
                           GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                           GTK_STOCK_OK, GTK_RESPONSE_OK, NULL );
                    
    xset_set_window_icon( GTK_WINDOW( dlg ) );

    if ( !name )
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dlg), dir );
    else
    {
        char* path = g_build_filename( dir, name, NULL );
        gtk_file_chooser_set_filename( GTK_FILE_CHOOSER( dlg ), path );
        g_free( path );
    }
    g_free( dir );
    g_free( name );

    int width = xset_get_int( "move_dlg_help", "x" );
    int height = xset_get_int( "move_dlg_help", "y" );
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
    if( response == GTK_RESPONSE_OK )
    {
        new_path = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER ( dlg ) );
        char* path = new_path;
        GtkWidget* w;
        if ( widget == GTK_WIDGET( mset->browse_target ) )
            w = GTK_WIDGET( mset->entry_target );
        else
        {
            if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( mset->opt_new_file ) ) )
                w = gtk_bin_get_child( GTK_BIN( mset->combo_template ) );
            else
                w = gtk_bin_get_child( GTK_BIN( mset->combo_template_dir ) );
            dir = get_template_dir();
            if ( dir )
            {
                if ( g_str_has_prefix( new_path, dir ) && new_path[strlen( dir )] == '/' )
                    path = new_path + strlen( dir ) + 1;
                g_free( dir );
            }
        }
        gtk_entry_set_text( GTK_ENTRY( w ), path );
        g_free( new_path );
    }
    
    GtkAllocation allocation;
    gtk_widget_get_allocation ( GTK_WIDGET ( dlg ), &allocation );
    width = allocation.width;
    height = allocation.height;
    if ( width && height )
    {
        char* str = g_strdup_printf( "%d", width );
        xset_set( "move_dlg_help", "x", str );
        g_free( str );
        str = g_strdup_printf( "%d", height );
        xset_set( "move_dlg_help", "y", str );
        g_free( str );
    }

    gtk_widget_destroy( dlg );
}

enum {
    MODE_FILENAME,
    MODE_PARENT,
    MODE_PATH
};

void on_browse_mode_toggled( GtkMenuItem* item, GtkWidget* dlg )
{
    int i;
    GtkWidget** mode = (GtkWidget**)g_object_get_data( G_OBJECT( dlg ), "mode" );

    for ( i = MODE_FILENAME; i <= MODE_PATH; i++ )
    {
        if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( mode[i] ) ) )
        {
            int action = i == MODE_PARENT ? GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER :
                                        GTK_FILE_CHOOSER_ACTION_SAVE;
            GtkAllocation allocation;
            gtk_widget_get_allocation( GTK_WIDGET(dlg), &allocation );
            int width = allocation.width;
            int height = allocation.height;
            gtk_file_chooser_set_action( GTK_FILE_CHOOSER( dlg ), action );
            if ( width && height )
            {
                // under some circumstances, changing the action changes the size
                gtk_window_set_position( GTK_WINDOW( dlg ), GTK_WIN_POS_CENTER_ALWAYS );
                gtk_window_resize( GTK_WINDOW( dlg ), width, height );
                while( gtk_events_pending() )
                    gtk_main_iteration();
                gtk_window_set_position( GTK_WINDOW( dlg ), GTK_WIN_POS_CENTER );
            }
            return;
        }
    }
}

void on_browse_button_press( GtkWidget* widget, MoveSet* mset )
{
    char* str;
    GtkTextIter iter, siter;
    int mode_default = MODE_PARENT;
    
    XSet* set = xset_get( "move_dlg_help" );
    if ( set->z )
        mode_default = xset_get_int( "move_dlg_help", "z" );

    // action create folder does not work properly so not used:
    //  it creates a folder by default with no way to stop it
    //  it gives 'folder already exists' error popup
    GtkWidget* dlg = gtk_file_chooser_dialog_new( _("Browse"),
                                   mset->parent ? GTK_WINDOW( mset->parent ) : NULL, 
                                   mode_default == MODE_PARENT ?
                                        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER :
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                   GTK_STOCK_OK, GTK_RESPONSE_OK, NULL );
    gtk_window_set_role( GTK_WINDOW( dlg ), "file_dialog" );

    gtk_text_buffer_get_start_iter( mset->buf_path, &siter );
    gtk_text_buffer_get_end_iter( mset->buf_path, &iter );
    char* path = gtk_text_buffer_get_text( mset->buf_path, &siter,
                                                            &iter, FALSE );
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dlg), path );
    g_free( path );
    
    if ( mode_default != MODE_PARENT )
    {
        gtk_text_buffer_get_start_iter( mset->buf_full_name, &siter );
        gtk_text_buffer_get_end_iter( mset->buf_full_name, &iter );
        path = gtk_text_buffer_get_text( mset->buf_full_name, &siter,
                                                                &iter, FALSE );
        gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER(dlg), path );
        g_free( path );
    }
    
    gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER(dlg), FALSE );

    // Mode
    int i;
    GtkWidget* mode[3];
    GtkWidget* hbox = gtk_hbox_new( FALSE, 4 );
    mode[MODE_FILENAME] = gtk_radio_button_new_with_mnemonic( NULL,
                                                                _("Fil_ename") );
    mode[MODE_PARENT] = gtk_radio_button_new_with_mnemonic_from_widget(
                            GTK_RADIO_BUTTON(mode[MODE_FILENAME]), _("Pa_rent") );
    mode[MODE_PATH] =  gtk_radio_button_new_with_mnemonic_from_widget( 
                            GTK_RADIO_BUTTON(mode[MODE_FILENAME]), _("P_ath") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( mode[mode_default] ), TRUE );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        gtk_label_new( _("Insert as") ), FALSE, TRUE, 2 );
    for ( i = MODE_FILENAME; i <= MODE_PATH; i++ )
    {
        gtk_button_set_focus_on_click( GTK_BUTTON( mode[i] ), FALSE );
        g_signal_connect( G_OBJECT( mode[i] ), "toggled",
                                        G_CALLBACK( on_browse_mode_toggled ), dlg );
        gtk_box_pack_start( GTK_BOX( hbox ), mode[i], FALSE, TRUE, 2 );
    }
    gtk_box_pack_start( GTK_BOX( gtk_dialog_get_content_area ( 
                                        GTK_DIALOG( dlg ) ) ),
                                        hbox, FALSE, TRUE, 6 );
    g_object_set_data( G_OBJECT( dlg ), "mode", mode );
    gtk_widget_show_all( hbox );

    int width = xset_get_int( "move_dlg_help", "x" );
    int height = xset_get_int( "move_dlg_help", "y" );
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
    // bogus GTK warning here: Unable to retrieve the file info for...
    if( response == GTK_RESPONSE_OK )
    {
        for ( i = MODE_FILENAME; i <= MODE_PATH; i++ )
        {
            if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( mode[i] ) ) )
            {
                if ( i == MODE_FILENAME )
                {
                    path = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER ( dlg ) );
                    str = g_path_get_basename( path );
                    gtk_text_buffer_set_text( mset->buf_full_name, str, -1 );
                    g_free( str );
                }
                else if ( i == MODE_PARENT )
                {
                    path = gtk_file_chooser_get_current_folder( 
                                                    GTK_FILE_CHOOSER ( dlg ) );
                    gtk_text_buffer_set_text( mset->buf_path, path, -1 );
                }
                else
                {
                    path = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER ( dlg ) );
                    gtk_text_buffer_set_text( mset->buf_full_path, path, -1 );
                }
                g_free( path );
                break;
            }
        }
    }
    
    // save size
    GtkAllocation allocation;
    gtk_widget_get_allocation( GTK_WIDGET(dlg), &allocation );
    width = allocation.width;
    height = allocation.height;
    if ( width && height )
    {
        str = g_strdup_printf( "%d", width );
        xset_set( "move_dlg_help", "x", str );
        g_free( str );
        str = g_strdup_printf( "%d", height );
        xset_set( "move_dlg_help", "y", str );
        g_free( str );
    }

    // save mode
    for ( i = MODE_FILENAME; i <= MODE_PATH; i++ )
    {
        if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( mode[i] ) ) )
        {
            str = g_strdup_printf( "%d", i );
            xset_set( "move_dlg_help", "z", str );
            g_free( str );
            break;
        }
    }
    
    gtk_widget_destroy( dlg );    
}

void on_font_change( GtkMenuItem* item, MoveSet* mset )
{
    PangoFontDescription* font_desc = NULL;
    
    if ( xset_get_s( "move_dlg_font" ) )
        font_desc = pango_font_description_from_string(
                                                xset_get_s( "move_dlg_font" ) );

    gtk_widget_modify_font( GTK_WIDGET( mset->input_name ), font_desc );
    gtk_widget_modify_font( GTK_WIDGET( mset->entry_ext ), font_desc );
    gtk_widget_modify_font( GTK_WIDGET( mset->input_full_name ), font_desc );
    gtk_widget_modify_font( GTK_WIDGET( mset->input_path ), font_desc );
    gtk_widget_modify_font( GTK_WIDGET( mset->input_full_path ), font_desc );
    gtk_widget_modify_font( GTK_WIDGET( mset->label_mime ), font_desc );
    if ( mset->entry_target )
        gtk_widget_modify_font( GTK_WIDGET( mset->entry_target ), font_desc );
    if ( mset->create_new )
    {
        // doesn't change drop-down font
        //gtk_widget_modify_font( GTK_WIDGET( mset->combo_template ), font_desc );
        gtk_widget_modify_font( GTK_WIDGET( gtk_bin_get_child(
                                GTK_BIN( mset->combo_template ) ) ), font_desc );
        //gtk_widget_modify_font( GTK_WIDGET( mset->combo_template_dir ), font_desc );
        gtk_widget_modify_font( GTK_WIDGET( gtk_bin_get_child(
                                GTK_BIN( mset->combo_template_dir ) ) ), font_desc );
    }

    if ( font_desc )
        pango_font_description_free( font_desc );
}

void on_opt_toggled( GtkMenuItem* item, MoveSet* mset )
{
    const char* action;
    char* btn_label = NULL;
    char* root_msg;
    char* title;
    GtkTextIter iter, siter;

    gboolean move = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( mset->opt_move ) );
    gboolean copy = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( mset->opt_copy ) );
    gboolean link = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( mset->opt_link ) );
    gboolean copy_target = gtk_toggle_button_get_active( 
                                        GTK_TOGGLE_BUTTON( mset->opt_copy_target ) );
    gboolean link_target = gtk_toggle_button_get_active( 
                                        GTK_TOGGLE_BUTTON( mset->opt_link_target ) );
    gboolean as_root = gtk_toggle_button_get_active( 
                                        GTK_TOGGLE_BUTTON( mset->opt_as_root ) );

    gboolean new_file = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( mset->opt_new_file ) );
    gboolean new_folder = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( mset->opt_new_folder ) );
    gboolean new_link = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( mset->opt_new_link ) );

    const char* desc = NULL;
    if ( mset->create_new )
    {
        btn_label = _("C_reate");
        action = C_("Title|", "Create New");
        if ( new_file )
            desc = C_("Title|CreateNew|", "File");
        else if ( new_folder )
            desc = C_("Title|CreateNew|", "Folder");
        else
            desc = C_("Title|CreateNew|", "Link");
    }
    else
    {
        gtk_text_buffer_get_start_iter( mset->buf_full_path, &siter );
        gtk_text_buffer_get_end_iter( mset->buf_full_path, &iter );
        char* full_path = gtk_text_buffer_get_text( mset->buf_full_path, &siter, &iter,
                                                                            FALSE );
        char* new_path = g_path_get_dirname( full_path );

        gboolean rename = ( !strcmp( mset->old_path, new_path ) || !strcmp( new_path, "." ) );
        g_free( new_path );
        g_free( full_path );

        if ( move )
        {
            btn_label = rename ? _("_Rename") : _("_Move");
            action = _("Move");
        }
        else if ( copy )
        {
            btn_label = _("C_opy");
            action = _("Copy");
        }
        else if ( link )
        {
            btn_label = _("_Link");
            action = _("Create Link To");
        }
        else if ( copy_target )
        {
            btn_label = _("C_opy");
            action = _("Copy");
            desc = _("Link Target");
        }
        else if ( link_target )
        {
            btn_label = _("_Link");
            action = _("Create Link To");
            desc = _("Target");
        }
    }
    
    if ( as_root )
        root_msg = _(" As Root");
    else
        root_msg = "";

    // Window Icon
    const char* win_icon;
    if ( as_root )
        win_icon = "gtk-dialog-warning";
    else if ( mset->create_new )
        win_icon = "gtk-new";
    else
        win_icon = "gtk-edit";
    GdkPixbuf* pixbuf = gtk_icon_theme_load_icon( gtk_icon_theme_get_default(),
                        win_icon, 16,
                        GTK_ICON_LOOKUP_USE_BUILTIN, NULL );
    if ( pixbuf )
        gtk_window_set_icon( GTK_WINDOW( mset->dlg ), pixbuf );    

    // title
    if ( !desc )
        desc = mset->desc;
    title = g_strdup_printf( "%s %s%s", action, desc, root_msg );
    gtk_window_set_title( GTK_WINDOW( mset->dlg ), title );
    g_free( title );

    if ( btn_label )
        gtk_button_set_label( GTK_BUTTON( mset->next ), btn_label );
    gtk_button_set_image( GTK_BUTTON( mset->next ), xset_get_image( as_root ?
                                    "GTK_STOCK_DIALOG_WARNING" : "GTK_STOCK_YES",
                                    GTK_ICON_SIZE_BUTTON ) );

    mset->full_path_same = FALSE;
    mset->mode_change = TRUE;
    on_move_change( GTK_WIDGET( mset->buf_full_path ), mset );
    if ( mset->create_new )
        on_toggled( NULL, mset );
}

void on_toggled( GtkMenuItem* item, MoveSet* mset )
{
    //int (*show) () = NULL;
    void (*show) () = NULL;
    gboolean someone_is_visible = FALSE;
    gboolean opts_visible = FALSE;

    // opts
    if ( xset_get_b( "move_copy" ) || mset->clip_copy )
        gtk_widget_show( mset->opt_copy );
    else
    {
        if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( mset->opt_copy ) ) )
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( mset->opt_move ), TRUE );
        gtk_widget_hide( mset->opt_copy );
    }

    if ( xset_get_b( "move_link" ) )
    {
        gtk_widget_show( mset->opt_link );
    }
    else
    {
        if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( mset->opt_link ) ) )
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( mset->opt_move ), TRUE );
        gtk_widget_hide( mset->opt_link );
    }

    if ( xset_get_b( "move_copyt" ) && mset->is_link )
        gtk_widget_show( mset->opt_copy_target );
    else
    {
        if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( mset->opt_copy_target ) ) )
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( mset->opt_move ), TRUE );
        gtk_widget_hide( mset->opt_copy_target );
    }

    if ( xset_get_b( "move_linkt" ) && mset->is_link )
        gtk_widget_show( mset->opt_link_target );
    else
    {
        if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( mset->opt_link_target ) ) )
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( mset->opt_move ), TRUE );
        gtk_widget_hide( mset->opt_link_target );
    }

    if ( xset_get_b( "move_as_root" ) )
        gtk_widget_show( mset->opt_as_root );
    else
    {
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( mset->opt_as_root ), FALSE );
        gtk_widget_hide( mset->opt_as_root );
    }
    
    if ( !gtk_widget_get_visible( mset->opt_copy )
            && !gtk_widget_get_visible( mset->opt_link )
            && !gtk_widget_get_visible( mset->opt_copy_target )
            && !gtk_widget_get_visible( mset->opt_link_target ) )
    {
        gtk_widget_hide( mset->opt_move );
        opts_visible = gtk_widget_get_visible( mset->opt_as_root );
    }
    else
    {
        gtk_widget_show( mset->opt_move );
        opts_visible = TRUE;
    }
    
    // entries
    if ( xset_get_b( "move_name" ) )
    {
        show = (GFunc)gtk_widget_show;
        someone_is_visible = TRUE;
    }
    else
        show = (GFunc)gtk_widget_hide;
    show( mset->label_name );
    show( mset->scroll_name );
    show( mset->hbox_ext );
    show( mset->blank_name );

    if ( xset_get_b( "move_filename" ) )
    {
        show = (GFunc)gtk_widget_show;
        someone_is_visible = TRUE;
    }
    else
        show = (GFunc)gtk_widget_hide;
    show( mset->label_full_name );
    show( mset->scroll_full_name );
    show( mset->blank_full_name );

    if ( xset_get_b( "move_parent" ) )
    {
        show = (GFunc)gtk_widget_show;
        someone_is_visible = TRUE;
    }
    else
        show = (GFunc)gtk_widget_hide;
    show( mset->label_path );
    show( mset->scroll_path );
    show( mset->blank_path );

    if ( xset_get_b( "move_path" ) )
    {
        show = (GFunc)gtk_widget_show;
        someone_is_visible = TRUE;
    }
    else
        show = (GFunc)gtk_widget_hide;
    show( mset->label_full_path );
    show( mset->scroll_full_path );

    if ( !mset->is_link && !mset->create_new && xset_get_b( "move_type" ) )
    {
        show = (GFunc)gtk_widget_show;
    }
    else
        show = (GFunc)gtk_widget_hide;
    show( mset->hbox_type );

    gboolean new_file = FALSE;
    gboolean new_folder = FALSE;
    gboolean new_link = FALSE;
    if ( mset->create_new )
    {
        new_file = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( mset->opt_new_file ) );
        new_folder = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( mset->opt_new_folder ) );
        new_link = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( mset->opt_new_link ) );
    }
    
    if ( new_link || ( mset->is_link && xset_get_b( "move_target" ) ) )
    {
        show = (GFunc)gtk_widget_show;
    }
    else
        show = (GFunc)gtk_widget_hide;
    show( mset->hbox_target );

    if ( ( new_file || new_folder ) && xset_get_b( "move_template" ) )
    {
        show = (GFunc)gtk_widget_show;
        if ( new_file )
        {
            gtk_widget_show( GTK_WIDGET( mset->combo_template ) );
            gtk_label_set_mnemonic_widget( mset->label_template,
                                        GTK_WIDGET( mset->combo_template ) );
            gtk_widget_hide( GTK_WIDGET( mset->combo_template_dir ) );
        }
        else
        {
            gtk_widget_show( GTK_WIDGET( mset->combo_template_dir ) );
            gtk_label_set_mnemonic_widget( mset->label_template,
                                        GTK_WIDGET( mset->combo_template_dir ) );
            gtk_widget_hide( GTK_WIDGET( mset->combo_template ) );
        }
    }
    else
        show = (GFunc)gtk_widget_hide;
    show( mset->hbox_template );

    if ( !someone_is_visible )
    {
        xset_set_b( "move_filename", TRUE );
        on_toggled( NULL, mset );
    }
    
    if ( opts_visible )
    {
        if ( gtk_widget_get_visible( mset->hbox_type ) )
        {}
        else if ( gtk_widget_get_visible( GTK_WIDGET( mset->label_full_path ) ) )
        {}
        else if ( gtk_widget_get_visible( GTK_WIDGET( mset->blank_path ) ) )
            gtk_widget_hide( GTK_WIDGET( mset->blank_path ) );
        else if ( gtk_widget_get_visible( GTK_WIDGET( mset->blank_full_name ) ) )
            gtk_widget_hide( GTK_WIDGET( mset->blank_full_name ) );
        else if ( gtk_widget_get_visible( GTK_WIDGET( mset->blank_name ) ) )
            gtk_widget_hide( GTK_WIDGET( mset->blank_name ) );
    }
}

gboolean on_mnemonic_activate(GtkWidget *widget, gboolean arg1, MoveSet* mset )
{
    select_input( widget, mset );
    return FALSE;
}
                                          
void on_options_button_press( GtkWidget* btn, MoveSet* mset )
{
    GtkWidget* popup = gtk_menu_new();
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    xset_context_new();

    XSet* set = xset_set_cb( "move_name", on_toggled, mset );
    xset_add_menuitem( mset->desktop, mset->browser, popup, accel_group, set );
    set = xset_set_cb( "move_filename", on_toggled, mset );
    xset_add_menuitem( mset->desktop, mset->browser, popup, accel_group, set );
    set = xset_set_cb( "move_parent", on_toggled, mset );
    xset_add_menuitem( mset->desktop, mset->browser, popup, accel_group, set );
    set = xset_set_cb( "move_path", on_toggled, mset );
    xset_add_menuitem( mset->desktop, mset->browser, popup, accel_group, set );
    set = xset_set_cb( "move_type", on_toggled, mset );
    set->disable = ( mset->create_new || mset->is_link );
    xset_add_menuitem( mset->desktop, mset->browser, popup, accel_group, set );
    set = xset_set_cb( "move_target", on_toggled, mset );
    set->disable = mset->create_new || !mset->is_link;
    xset_add_menuitem( mset->desktop, mset->browser, popup, accel_group, set );
    set = xset_set_cb( "move_template", on_toggled, mset );
    set->disable = !mset->create_new;
    xset_add_menuitem( mset->desktop, mset->browser, popup, accel_group, set );

    set = xset_set_cb( "move_copy", on_toggled, mset );
        set->disable = mset->clip_copy || mset->create_new;
    set = xset_set_cb( "move_link", on_toggled, mset );
        set->disable = mset->create_new;
    set = xset_set_cb( "move_copyt", on_toggled, mset );
        set->disable = !mset->is_link;
    set = xset_set_cb( "move_linkt", on_toggled, mset );
        set->disable = !mset->is_link;
    xset_set_cb( "move_as_root", on_toggled, mset );
    set = xset_get( "move_option" );
    xset_add_menuitem( mset->desktop, mset->browser, popup, accel_group, set );

    set = xset_get( "sep_mopt1" );
    xset_add_menuitem( mset->desktop, mset->browser, popup, accel_group, set );
    set = xset_get( "move_dlg_confirm_create" );
    xset_add_menuitem( mset->desktop, mset->browser, popup, accel_group, set );
    set = xset_set_cb( "move_dlg_font", on_font_change, mset );
    xset_add_menuitem( mset->desktop, mset->browser, popup, accel_group, set );
    set = xset_get( "sep_mopt2" );
    xset_add_menuitem( mset->desktop, mset->browser, popup, accel_group, set );
    set = xset_set_cb( "move_dlg_help", on_help_activate, mset );
    xset_add_menuitem( mset->desktop, mset->browser, popup, accel_group, set );

    gtk_widget_show_all( GTK_WIDGET( popup ) );
    g_signal_connect( popup, "selection-done",
          G_CALLBACK( gtk_widget_destroy ), NULL );
    gtk_menu_popup( GTK_MENU( popup ), NULL, NULL,
                        NULL, NULL, 0, gtk_get_current_event_time() );
}

static gboolean on_label_focus( GtkWidget* widget, GtkDirectionType direction, MoveSet* mset )
{
    GtkWidget* input = NULL;

    if ( direction == GTK_DIR_TAB_FORWARD )
    {
        if ( widget == GTK_WIDGET( mset->label_name ) )
            input = mset->input_name;
        else if ( widget == GTK_WIDGET( mset->label_ext ) )
            input = GTK_WIDGET( mset->entry_ext );
        else if ( widget == GTK_WIDGET( mset->label_full_name ) )
            input = mset->input_full_name;
        else if ( widget == GTK_WIDGET( mset->label_path ) )
            input = mset->input_path;
        else if ( widget == GTK_WIDGET( mset->label_full_path ) )
            input = mset->input_full_path;
        else if ( widget == GTK_WIDGET( mset->label_type ) )
        {
            on_button_focus( mset->options, GTK_DIR_TAB_FORWARD, mset );
            return TRUE;
        }
        else if ( widget == GTK_WIDGET( mset->label_target ) )
            input = GTK_WIDGET( mset->entry_target );
        else if ( widget == GTK_WIDGET( mset->label_template ) )
        {
            if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( mset->opt_new_file ) ) )
                input = GTK_WIDGET( mset->combo_template );
            else
                input = GTK_WIDGET( mset->combo_template_dir );
        }
    }
    else if ( direction == GTK_DIR_TAB_BACKWARD )
    {
        GtkWidget* input2;
        GtkWidget* first_input;

        if ( widget == GTK_WIDGET( mset->label_name ) )
        {
            if ( mset->combo_template_dir )
                input = GTK_WIDGET( mset->combo_template_dir );
            else if ( mset->combo_template )
                input = GTK_WIDGET( mset->combo_template );
            else if ( mset->entry_target )
                input = GTK_WIDGET( mset->entry_target );
            else
                input = mset->input_full_path;
        }
        else if ( widget == GTK_WIDGET( mset->label_ext ) )
            input = mset->input_name;
        else if ( widget == GTK_WIDGET( mset->label_full_name ) )
        {
            if ( gtk_widget_get_visible( gtk_widget_get_parent( 
                                GTK_WIDGET( mset->entry_ext ) ) ) 
                                && gtk_widget_get_sensitive(
                                GTK_WIDGET( mset->entry_ext ) ) )
                input = GTK_WIDGET( mset->entry_ext );
            else
                input = mset->input_name;
        }
        else if ( widget == GTK_WIDGET( mset->label_path ) )
            input = mset->input_full_name;
        else if ( widget == GTK_WIDGET( mset->label_full_path ) )
            input = mset->input_path;
        else
            input = mset->input_full_path;
        
        first_input = input;
        while ( input && !gtk_widget_get_visible( gtk_widget_get_parent( input ) ) )
        {
            input2 = NULL;
            if ( input == GTK_WIDGET( mset->combo_template_dir ) )
            {
                if ( mset->combo_template )
                    input2 = GTK_WIDGET( mset->combo_template );
                else if ( mset->entry_target )
                    input2 = GTK_WIDGET( mset->entry_target );
                else
                    input2 = mset->input_full_path;
            }
            else if ( input == GTK_WIDGET( mset->combo_template ) )
            {
                if ( mset->entry_target )
                    input2 = GTK_WIDGET( mset->entry_target );
                else
                    input2 = mset->input_full_path;
            }
            else if ( input == GTK_WIDGET( mset->entry_target ) )
                input2 = mset->input_full_path;
            else if ( input == mset->input_full_path )
                input2 = mset->input_path;
            else if ( input == mset->input_path )
                input2 = mset->input_full_name;
            else if ( input == mset->input_full_name )
            {
                if ( gtk_widget_get_visible( gtk_widget_get_parent( 
                            GTK_WIDGET( mset->entry_ext ) ) ) 
                            && gtk_widget_get_sensitive( 
                            GTK_WIDGET( mset->entry_ext ) ) )
                    input2 = GTK_WIDGET( mset->entry_ext );
                else
                    input2 = mset->input_name;
            }
            else if ( input == GTK_WIDGET( mset->entry_ext ) )
                input2 = mset->input_name;
            else if ( input == mset->input_name )
            {
                if ( mset->combo_template_dir )
                    input2 = GTK_WIDGET( mset->combo_template_dir );
                else if ( mset->combo_template )
                    input2 = GTK_WIDGET( mset->combo_template );
                else if ( mset->entry_target )
                    input2 = GTK_WIDGET( mset->entry_target );
                else
                    input2 = mset->input_full_path;
            }
            
            if ( input2 == first_input )
                input = NULL;
            else
                input = input2;
        }
    }

    if ( input == GTK_WIDGET( mset->label_mime ) )
    {
        gtk_label_select_region( mset->label_mime, 0, -1 );
        gtk_widget_grab_focus( GTK_WIDGET( mset->label_mime ) );
    }
    else if ( input )
    {
        select_input( input, mset );
        gtk_widget_grab_focus( input );
    }
    return TRUE;
}

void copy_entry_to_clipboard( GtkWidget* widget, MoveSet* mset )
{
    GtkClipboard* clip = gtk_clipboard_get( GDK_SELECTION_CLIPBOARD );
    GtkTextBuffer* buf = NULL;
    GtkTextIter iter, siter;

    if ( widget == GTK_WIDGET( mset->label_name ) )
        buf = mset->buf_name;
    else if ( widget == GTK_WIDGET( mset->label_ext ) )
    {
        gtk_clipboard_set_text( clip, gtk_entry_get_text( mset->entry_ext ), -1 );
        return;
    }
    else if ( widget == GTK_WIDGET( mset->label_full_name ) )
        buf = mset->buf_full_name;
    else if ( widget == GTK_WIDGET( mset->label_path ) )
        buf = mset->buf_path;
    else if ( widget == GTK_WIDGET( mset->label_full_path ) )
        buf = mset->buf_full_path;
    else if ( widget == GTK_WIDGET( mset->label_type ) )
    {
        gtk_clipboard_set_text( clip, mset->mime_type, -1 );
        return;
    }
    else if ( widget == GTK_WIDGET( mset->label_target ) )
    {
        gtk_clipboard_set_text( clip, gtk_entry_get_text( mset->entry_target ), -1 );
        return;
    }
    else if ( widget == GTK_WIDGET( mset->label_template ) )
    {
        GtkWidget* w;
        if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( mset->opt_new_file ) ) )
            w = gtk_bin_get_child( GTK_BIN( mset->combo_template ) );
        else
            w = gtk_bin_get_child( GTK_BIN( mset->combo_template_dir ) );
        gtk_clipboard_set_text( clip, gtk_entry_get_text( GTK_ENTRY( w ) ), -1 );
    }
    
    if ( !buf )
        return;
    gtk_text_buffer_get_start_iter( buf, &siter );
    gtk_text_buffer_get_end_iter( buf, &iter );
    char* text = gtk_text_buffer_get_text( buf, &siter, &iter, FALSE );
    gtk_clipboard_set_text( clip, text, -1 );
    g_free( text );
}

gboolean on_label_button_press( GtkWidget *widget,
                                    GdkEventButton *event,
                                    MoveSet* mset )
{
    if ( event->type == GDK_BUTTON_PRESS )
    {
        if ( event->button == 1 || event->button == 2 )
        {
            GtkWidget* input = NULL;
            if ( widget == GTK_WIDGET( mset->label_name ) )
                input = mset->input_name;
            else if ( widget == GTK_WIDGET( mset->label_ext ) )
                input = GTK_WIDGET( mset->entry_ext );
            else if ( widget == GTK_WIDGET( mset->label_full_name ) )
                input = mset->input_full_name;
            else if ( widget == GTK_WIDGET( mset->label_path ) )
                input = mset->input_path;
            else if ( widget == GTK_WIDGET( mset->label_full_path ) )
                input = mset->input_full_path;
            else if ( widget == GTK_WIDGET( mset->label_type ) )
            {
                gtk_label_select_region( mset->label_mime, 0, -1 );
                gtk_widget_grab_focus( GTK_WIDGET( mset->label_mime ) );
                if ( event->button == 2 )
                    copy_entry_to_clipboard( widget, mset );
                return TRUE;
            }
            else if ( widget == GTK_WIDGET( mset->label_target ) )
                input = GTK_WIDGET( mset->entry_target );
            else if ( widget == GTK_WIDGET( mset->label_template ) )
            {
                if ( gtk_toggle_button_get_active( 
                                        GTK_TOGGLE_BUTTON( mset->opt_new_file ) ) )
                    input = GTK_WIDGET( mset->combo_template );
                else
                    input = GTK_WIDGET( mset->combo_template_dir );
            }
            
            if ( input )
            {
                select_input( input, mset );
                gtk_widget_grab_focus( input );
                if ( event->button == 2 )
                    copy_entry_to_clipboard( widget, mset );
            }
        }
    }
    else if ( event->type == GDK_2BUTTON_PRESS )
        copy_entry_to_clipboard( widget, mset );
    return TRUE;
}

char* get_unique_name( const char* dir, const char* ext )
{
    char* name, *path;
    
    char* base = _("new");
    if ( ext && ext[0] != '\0' )
    {
        name = g_strdup_printf( "%s.%s", base, ext );
        path = g_build_filename( dir, name, NULL );
        g_free( name );
    }
    else
        path = g_build_filename( dir, base, NULL );
        
    int n = 2;
    struct stat64 statbuf;
    while ( lstat64( path, &statbuf ) == 0 ) // g_file_test doesn't see broken links
    {
        g_free( path );
        if ( n == 1000 )
            return g_strdup( base );
        if ( ext && ext[0] != '\0' )
            name = g_strdup_printf( "%s%d.%s", base, n++, ext );
        else
            name = g_strdup_printf( "%s%d", base, n++ );
        path = g_build_filename( dir, name, NULL );
        g_free( name );
    }
    return path;
}

char* get_template_dir()
{
    char* templates_path = NULL;

#if GLIB_CHECK_VERSION(2, 14, 0)
    templates_path = g_strdup( g_get_user_special_dir(
                                            G_USER_DIRECTORY_TEMPLATES ) );
#endif
    if ( !templates_path )
    {
        templates_path = g_strdup( g_getenv( "XDG_TEMPLATES_DIR" ) );
    }
    if ( !g_strcmp0( templates_path, g_get_home_dir() ) )
    {
        /* If $XDG_TEMPLATES_DIR == $HOME this means it is disabled. Don't
         * recurse it as this is too many files/folders and may slow
         * dialog open and cause filesystem find loops.
         * https://wiki.freedesktop.org/www/Software/xdg-user-dirs/ */
        g_free( templates_path );
        templates_path = NULL;
    }
    if ( !dir_has_files( templates_path ) )
    {
        g_free( templates_path );
        templates_path = g_build_filename( g_get_home_dir(), "Templates",
                                                                NULL );
        if ( !dir_has_files( templates_path ) )
        {
            g_free( templates_path );
            templates_path = g_build_filename( g_get_home_dir(),
                                                    ".templates", NULL );
            if ( !dir_has_files( templates_path ) )
            {
                g_free( templates_path );
                templates_path = NULL;
            }
        }
    }
    return templates_path;
}

GList* get_templates( const char* templates_dir, const char* subdir,
                                            GList* templates, gboolean getdir )
{
    const char* name;
    char* path;
    char* subsubdir;
    char* templates_path;
    
    if ( !templates_dir )
    {
        templates_path = get_template_dir();
        if ( templates_path )
            templates = get_templates( templates_path, NULL, templates, getdir );
        g_free( templates_path );
        return templates;
    }
    
    templates_path = g_build_filename( templates_dir, subdir, NULL );
    GDir* dir = g_dir_open( templates_path, 0, NULL );
    if ( dir )
    {
        while ( name = g_dir_read_name( dir ) )
        {
            path = g_build_filename( templates_path, name, NULL );
            if ( getdir )
            {
                if ( g_file_test( path, G_FILE_TEST_IS_DIR ) )
                {
                    if ( subdir )
                        subsubdir = g_build_filename( subdir, name, NULL );
                    else
                        subsubdir = g_strdup( name );
                    templates = g_list_prepend( templates,
                                            g_strdup_printf( "%s/", subsubdir ) );
                    // prevent filesystem loops during recursive find
                    if ( !g_file_test( path, G_FILE_TEST_IS_SYMLINK ) )
                        templates = get_templates( templates_dir, subsubdir,
                                                   templates, getdir );
                    g_free( subsubdir );
                }
            }
            else
            {
                if ( g_file_test( path, G_FILE_TEST_IS_REGULAR ) )
                {
                    if ( subdir )
                        templates = g_list_prepend( templates, g_build_filename(
                                                            subdir, name, NULL ) );
                    else
                        templates = g_list_prepend( templates, g_strdup( name ) );
                }
                else if ( g_file_test( path, G_FILE_TEST_IS_DIR ) &&
                          // prevent filesystem loops during recursive find
                          !g_file_test( path, G_FILE_TEST_IS_SYMLINK ) )
                {
                    if ( subdir )
                    {    
                        subsubdir = g_build_filename( subdir, name, NULL );
                        templates = get_templates( templates_dir, subsubdir,
                                                            templates, getdir );
                        g_free( subsubdir );
                    }
                    else
                        templates = get_templates( templates_dir, name, templates,
                                                                        getdir );
                }
            }
            g_free( path );
        }
        g_dir_close( dir );
    }
    g_free( templates_path );
    return templates;
}

void on_template_changed( GtkWidget* widget, MoveSet* mset )
{
    char* str = NULL;
    char* str2;
    if ( !gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( mset->opt_new_file ) ) )
        return;
    char* text = g_strdup( gtk_entry_get_text( GTK_ENTRY( gtk_bin_get_child(
                                        GTK_BIN( mset->combo_template ) ) ) ) );
    if ( text )
    {
        g_strstrip( text );
        str = text;
        while ( str2 = strchr( str, '/' ) )
            str = str2 + 1;
        if ( str[0] == '.' )
            str++;
        if ( str2 = strchr( str, '.' ) )
            str = str2 + 1;
        else
            str = NULL;
    }
    gtk_entry_set_text( mset->entry_ext, str ? str : "" );

    // need new name due to extension added?
    GtkTextIter iter, siter;
    gtk_text_buffer_get_start_iter( mset->buf_full_path, &siter );
    gtk_text_buffer_get_end_iter( mset->buf_full_path, &iter );
    char* full_path = gtk_text_buffer_get_text( mset->buf_full_path, &siter, &iter, FALSE );
    struct stat64 statbuf;
    if ( lstat64( full_path, &statbuf ) == 0 ) // g_file_test doesn't see broken links
    {
        char* dir = g_path_get_dirname( full_path );
        g_free( full_path );
        full_path = get_unique_name( dir, str );
        if ( full_path )
        {
            gtk_text_buffer_set_text( mset->buf_full_path, full_path, -1 );
        }
        g_free( dir );
    }
    g_free( full_path );

    g_free( text );
}

gboolean update_new_display_delayed( char* path )
{
    char* dir_path = g_path_get_dirname( path );
    VFSDir* vdir = vfs_dir_get_by_path_soft( dir_path );
    g_free( dir_path );
    if ( vdir && vdir->avoid_changes )
    {
        VFSFileInfo* file = vfs_file_info_new();
        vfs_file_info_get( file, path, NULL );
        vfs_dir_emit_file_created( vdir, vfs_file_info_get_name( file ), TRUE );
        vfs_file_info_unref( file );
        vfs_dir_flush_notify_cache();
    }
    if ( vdir )
        g_object_unref( vdir );
    g_free( path );
    return FALSE;
}

void update_new_display( const char* path )
{
    // for devices like nfs, emit created so the new file is shown
    // update now
    update_new_display_delayed( g_strdup( path ) );
    // update a little later for exec tasks
    g_timeout_add( 1500, (GSourceFunc)update_new_display_delayed, g_strdup( path ) );
}

/*
static gboolean on_dlg_keypress( GtkWidget *widget, GdkEventKey *event,
                                                            MoveSet* mset )
{
    if ( event->keyval == GDK_KEY_F1 && event->state == 0 )
    {
        on_help_activate( NULL, mset );
        return TRUE;
    }
    return FALSE;
}
*/

int ptk_rename_file( DesktopWindow* desktop, PtkFileBrowser* file_browser,
                                        const char* file_dir, VFSFileInfo* file,
                                        const char* dest_dir, gboolean clip_copy,
                                        PtkRenameMode create_new,
                                        AutoOpenCreate* auto_open )
{   
    char* full_name;
    char* full_path;
    char* path;
    char* old_path;
    char* root_mkdir = g_strdup( "" );
    char* to_path;
    char* from_path;
    char* task_name;
    char* str;
    GtkTextIter iter, siter;
    GtkWidget* task_view = NULL;
    int ret = 1;
    gboolean target_missing = FALSE;
    GList* templates;
    FILE* touchfile;
    struct stat64 statbuf;

    if ( !file_dir )
        return 0;
    MoveSet* mset = g_slice_new0( MoveSet );
    full_name = NULL;

    if ( !create_new )
    {
        if ( !file )
            return 0;
        // special processing for files with inconsistent real name and display name
        if( G_UNLIKELY( vfs_file_info_is_desktop_entry(file) ) )
            full_name = g_filename_display_name( file->name );
        if ( !full_name )
            full_name = g_strdup( vfs_file_info_get_disp_name( file ) );
        if ( !full_name )
            full_name = g_strdup( vfs_file_info_get_name( file ) );
        
        mset->is_dir = vfs_file_info_is_dir( file );
        mset->is_link = vfs_file_info_is_symlink( file );
        mset->clip_copy = clip_copy;
        mset->full_path = g_build_filename( file_dir, full_name, NULL );
        if ( dest_dir )
            mset->new_path = g_build_filename( dest_dir, full_name, NULL );
        else
            mset->new_path = g_strdup( mset->full_path );
        g_free( full_name );
        full_name = NULL;
    }
    else if ( create_new == PTK_RENAME_NEW_LINK && file )
    {
        full_name = g_strdup( vfs_file_info_get_disp_name( file ) );
        if ( !full_name )
            full_name = g_strdup( vfs_file_info_get_name( file ) );
        mset->full_path = g_build_filename( file_dir, full_name, NULL );
        mset->new_path = g_strdup( mset->full_path );
        mset->is_dir = vfs_file_info_is_dir( file );  // is_dir is dynamic for create
        mset->is_link = vfs_file_info_is_symlink( file );
        mset->clip_copy = FALSE;
    }
    else
    {
        mset->full_path = get_unique_name( file_dir, NULL );
        mset->new_path = g_strdup( mset->full_path );
        mset->is_dir = FALSE;  // is_dir is dynamic for create
        mset->is_link = FALSE;
        mset->clip_copy = FALSE;
    }
    
    mset->create_new = create_new;
    mset->old_path = file_dir;

    mset->full_path_exists = FALSE;
    mset->full_path_exists_dir = FALSE;
    mset->full_path_same = FALSE;
    mset->path_missing = FALSE;
    mset->path_exists_file = FALSE;
    mset->is_move = FALSE;
    

    // Dialog
    char* root_msg;
    if ( mset->is_link )
        mset->desc = _("Link");
    else if ( mset->is_dir )
        mset->desc = _("Folder");
    else
        mset->desc = _("File");
/*
    char* title;
    char* action;

        
    if ( job == JOB_COPY )
        action = "Copy";
    else if ( job == JOB_MOVE )
        action = "Rename / Move";
    else
        action = "Create Link To";
        
        
    title = g_strdup_printf( "%s %s%s", action, mset->desc, root_msg );
*/
    mset->browser = file_browser;
    mset->desktop = desktop;
    
    if ( file_browser )
    {
        mset->parent = gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) );
        task_view = file_browser->task_view;
    }
    else
        mset->parent = GTK_WIDGET( desktop );

    mset->dlg = gtk_dialog_new_with_buttons( _("Move"),
                                mset->parent ? GTK_WINDOW( mset->parent ) : NULL,
                                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                NULL, NULL );
    //g_free( title );
    gtk_window_set_role( GTK_WINDOW( mset->dlg ), "rename_dialog" );

    // Buttons
    mset->options = gtk_button_new_with_mnemonic( _("Opt_ions") );
    gtk_dialog_add_action_widget( GTK_DIALOG( mset->dlg ), mset->options, GTK_RESPONSE_YES);
    gtk_button_set_image( GTK_BUTTON( mset->options ), xset_get_image( "GTK_STOCK_PROPERTIES",
                                                        GTK_ICON_SIZE_BUTTON ) );
    gtk_button_set_focus_on_click( GTK_BUTTON( mset->options ), FALSE );
    g_signal_connect( G_OBJECT( mset->options ), "clicked",
                          G_CALLBACK( on_options_button_press ), mset );

    mset->browse = gtk_button_new_with_mnemonic( _("_Browse") );
    gtk_dialog_add_action_widget( GTK_DIALOG( mset->dlg ), mset->browse, GTK_RESPONSE_YES);
    gtk_button_set_image( GTK_BUTTON( mset->browse ), xset_get_image( "GTK_STOCK_OPEN",
                                                        GTK_ICON_SIZE_BUTTON ) );
    gtk_button_set_focus_on_click( GTK_BUTTON( mset->browse ), FALSE );
    g_signal_connect( G_OBJECT( mset->browse ), "clicked",
                          G_CALLBACK( on_browse_button_press ), mset );

    mset->revert = gtk_button_new_with_mnemonic( _("Re_vert") );
    gtk_dialog_add_action_widget( GTK_DIALOG( mset->dlg ), mset->revert, GTK_RESPONSE_NO);
    gtk_button_set_image( GTK_BUTTON( mset->revert ), xset_get_image( "GTK_STOCK_REVERT_TO_SAVED",
                                                        GTK_ICON_SIZE_BUTTON ) );
    gtk_button_set_focus_on_click( GTK_BUTTON( mset->revert ), FALSE );
    g_signal_connect( G_OBJECT( mset->revert ), "clicked",
                          G_CALLBACK( on_revert_button_press ), mset );

    mset->cancel = gtk_button_new_from_stock( GTK_STOCK_CANCEL );
    gtk_dialog_add_action_widget( GTK_DIALOG( mset->dlg ), mset->cancel, GTK_RESPONSE_CANCEL);

    mset->next = gtk_button_new_from_stock( GTK_STOCK_OK );
    gtk_dialog_add_action_widget( GTK_DIALOG( mset->dlg ), mset->next, GTK_RESPONSE_OK);
    gtk_button_set_focus_on_click( GTK_BUTTON( mset->next ), FALSE );
    gtk_button_set_image( GTK_BUTTON( mset->next ), xset_get_image( "GTK_STOCK_YES",
                                                        GTK_ICON_SIZE_BUTTON ) );
    gtk_button_set_label( GTK_BUTTON( mset->next ), _("_Rename") );

    if ( create_new && auto_open )
    {
        mset->open = gtk_button_new_with_mnemonic( _("& _Open") );
        gtk_dialog_add_action_widget( GTK_DIALOG( mset->dlg ), mset->open, GTK_RESPONSE_APPLY);
        gtk_button_set_focus_on_click( GTK_BUTTON( mset->open ), FALSE );
    }
    else
        mset->open = NULL;

    // Window
    int width = xset_get_int( "move_dlg_font", "x" );
    int height = xset_get_int( "move_dlg_font", "y" );
    if ( width && height )
        gtk_window_set_default_size( GTK_WINDOW( mset->dlg ), width, height );
    else
        gtk_widget_set_size_request( GTK_WIDGET( mset->dlg ), 600, -1 );
    gtk_window_set_resizable( GTK_WINDOW( mset->dlg ), TRUE );
    gtk_window_set_type_hint( GTK_WINDOW ( mset->dlg ),
                               GDK_WINDOW_TYPE_HINT_DIALOG );
    gtk_widget_show_all( mset->dlg );
    
    // Entries
    
    // Type
    char* type;
    mset->label_type = GTK_LABEL( gtk_label_new( NULL ) );
    gtk_label_set_markup_with_mnemonic( mset->label_type, _("<b>Type:</b>") );
    if ( mset->is_link )
    {
        //mset->mime_type = g_strdup_printf( "inode/symlink" );
        //type = g_strdup_printf( "symbolic link ( inode/symlink )" );
        path = g_file_read_link( mset->full_path, NULL );
        if ( path )
        {
            mset->mime_type = path;
            if ( g_file_test( path, G_FILE_TEST_EXISTS ) )
                type = g_strdup_printf( _("Link-> %s"), path );
            else
            {
                type = g_strdup_printf( _("!Link-> %s (missing)"), path );
                target_missing = TRUE;
            }
        }
        else
        {
            mset->mime_type = g_strdup_printf( "inode/symlink" );
            type = g_strdup_printf( "symbolic link ( inode/symlink )" );
        }
    }
    else if ( file )
    {
        VFSMimeType* mime_type = vfs_file_info_get_mime_type( file );
        if ( mime_type )
        {
            mset->mime_type = g_strdup( vfs_mime_type_get_type( mime_type ) );
            type = g_strdup_printf( " %s ( %s )",
                                    vfs_mime_type_get_description( mime_type ),
                                    mset->mime_type );
            vfs_mime_type_unref( mime_type );
        }
        else
        {
            mset->mime_type = g_strdup_printf( "?" );
            type = g_strdup( mset->mime_type );
        }
    }
    else //create
    {
        mset->mime_type = g_strdup_printf( "?" );
        type = g_strdup( mset->mime_type );
    }
    mset->label_mime = GTK_LABEL( gtk_label_new( type ) );
    gtk_label_set_ellipsize( mset->label_mime, PANGO_ELLIPSIZE_MIDDLE );
    g_free( type );

    gtk_label_set_selectable( mset->label_mime, TRUE );
    gtk_misc_set_alignment( GTK_MISC( mset->label_mime ), 0, 0 );
    gtk_label_set_selectable(  mset->label_type, TRUE );
    g_signal_connect( G_OBJECT( mset->label_type ), "button-press-event",
                          G_CALLBACK( on_label_button_press ), mset );
    g_signal_connect( G_OBJECT( mset->label_type ), "focus",
                          G_CALLBACK( on_label_focus ), mset );


    // Target
    if ( mset->is_link || create_new )
    {
        mset->label_target = GTK_LABEL( gtk_label_new( NULL ) );
        gtk_label_set_markup_with_mnemonic( mset->label_target, _("<b>_Target:</b>") );
        gtk_misc_set_alignment( GTK_MISC( mset->label_target ), 0, 1 );
        mset->entry_target = GTK_ENTRY( gtk_entry_new() );
        gtk_label_set_mnemonic_widget( mset->label_target, GTK_WIDGET( mset->entry_target ) );
        g_signal_connect( G_OBJECT( mset->entry_target ), "mnemonic-activate",
                              G_CALLBACK( on_mnemonic_activate ), mset );
        gtk_label_set_selectable(  mset->label_target, TRUE );
        g_signal_connect( G_OBJECT( mset->label_target ), "button-press-event",
                              G_CALLBACK( on_label_button_press ), mset );
        g_signal_connect( G_OBJECT( mset->label_target ), "focus",
                              G_CALLBACK( on_label_focus ), mset );
        g_signal_connect( G_OBJECT( mset->entry_target ), "key-press-event",
                              G_CALLBACK( on_move_entry_keypress ), mset );
        //g_signal_connect_after( G_OBJECT( mset->entry_target ), "focus",
        //                      G_CALLBACK( on_focus ), mset );
        //gtk_widget_set_sensitive( GTK_WIDGET( mset->entry_target ), !mset->is_dir );
        //gtk_widget_set_sensitive( GTK_WIDGET( mset->label_target ), !mset->is_dir );

        if ( create_new )
        {
            // Target Browse button
            mset->browse_target = gtk_button_new();
            gtk_button_set_image( GTK_BUTTON( mset->browse_target ),
                            xset_get_image( "GTK_STOCK_OPEN", GTK_ICON_SIZE_BUTTON ) );
            gtk_button_set_focus_on_click( GTK_BUTTON( mset->browse_target ), FALSE );
            if ( mset->new_path && file )
                gtk_entry_set_text( GTK_ENTRY( mset->entry_target ), mset->new_path );
            g_signal_connect( G_OBJECT( mset->browse_target ), "clicked",
                            G_CALLBACK( on_create_browse_button_press ), mset );
        }
        else
        {
            gtk_entry_set_text( GTK_ENTRY( mset->entry_target ), mset->mime_type );
            gtk_editable_set_editable( GTK_EDITABLE( mset->entry_target ), FALSE );
            mset->browse_target = NULL;
        }
        g_signal_connect( G_OBJECT( mset->entry_target ), "changed",
                              G_CALLBACK( on_move_change ), mset );
    }
    else
        mset->label_target = NULL;
        
    // Template
    if ( create_new )
    {
        mset->label_template = GTK_LABEL( gtk_label_new( NULL ) );
        gtk_label_set_markup_with_mnemonic( mset->label_template, _("<b>_Template:</b>") );
        gtk_misc_set_alignment( GTK_MISC( mset->label_template ), 0, 1 );
        g_signal_connect( G_OBJECT( mset->entry_target ), "mnemonic-activate",
                              G_CALLBACK( on_mnemonic_activate ), mset );
        gtk_label_set_selectable(  mset->label_template, TRUE );
        g_signal_connect( G_OBJECT( mset->label_template ), "button-press-event",
                              G_CALLBACK( on_label_button_press ), mset );
        g_signal_connect( G_OBJECT( mset->label_template ), "focus",
                              G_CALLBACK( on_label_focus ), mset );

        // template combo
        mset->combo_template = GTK_COMBO_BOX( gtk_combo_box_text_new_with_entry() );
        gtk_combo_box_set_focus_on_click( mset->combo_template, FALSE );

        // add entries
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( mset->combo_template ),
                                                                    _("Empty File") );
        templates = NULL;
        templates = get_templates( NULL, NULL, templates, FALSE );
        if ( templates )
        {
            templates = g_list_sort( templates, (GCompareFunc) g_strcmp0 );
            GList* l;
            int x = 0;
            for ( l = templates; l && x++ < 500; l = l->next )
            {
                gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( mset->combo_template ),
                                                                    (char*)l->data );
                g_free( l->data );
            }
            g_list_free( templates );
        }
        //gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( mset->combo_template ),
        //                                                            _("Add...") );
        gtk_combo_box_set_active( GTK_COMBO_BOX( mset->combo_template ), 0 );
        g_signal_connect( G_OBJECT( mset->combo_template ), "changed",
                                    G_CALLBACK( on_template_changed ), mset );
        g_signal_connect( G_OBJECT( gtk_bin_get_child( GTK_BIN( mset->combo_template ) ) ),
                        "key-press-event", G_CALLBACK( on_move_entry_keypress ), mset );
        //gtk_label_set_mnemonic_widget( mset->label_template, GTK_WIDGET( mset->combo_template ) );

        // template_dir combo
        mset->combo_template_dir = GTK_COMBO_BOX( gtk_combo_box_text_new_with_entry() );
        gtk_combo_box_set_focus_on_click( mset->combo_template_dir, FALSE );

        // add entries
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( mset->combo_template_dir ),
                                                                    _("Empty Folder") );
        templates = NULL;
        templates = get_templates( NULL, NULL, templates, TRUE );
        if ( templates )
        {
            templates = g_list_sort( templates, (GCompareFunc) g_strcmp0 );
            GList* l;
            for ( l = templates; l; l = l->next )
            {
                gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( 
                                                mset->combo_template_dir ),
                                                (char*)l->data );
                g_free( l->data );
            }
            g_list_free( templates );
        }
        //gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( mset->combo_template_dir ),
        //                                                        _("Add...") );
        gtk_combo_box_set_active( GTK_COMBO_BOX( mset->combo_template_dir ), 0 );
        g_signal_connect( G_OBJECT( gtk_bin_get_child( GTK_BIN( mset->combo_template_dir ) ) ),
                        "key-press-event", G_CALLBACK( on_move_entry_keypress ), mset );
        //g_signal_connect( G_OBJECT( mset->combo_template_dir ), "changed",
        //                            G_CALLBACK( on_template_changed ), mset );
        //gtk_label_set_mnemonic_widget( mset->label_template, GTK_WIDGET( 
        //                                            mset->combo_template_dir ) );

        // Template Browse button
        mset->browse_template = gtk_button_new();
        gtk_button_set_image( GTK_BUTTON( mset->browse_template ),
                        xset_get_image( "GTK_STOCK_OPEN", GTK_ICON_SIZE_BUTTON ) );
        gtk_button_set_focus_on_click( GTK_BUTTON( mset->browse_template ), FALSE );
        g_signal_connect( G_OBJECT( mset->browse_template ), "clicked",
                            G_CALLBACK( on_create_browse_button_press ), mset );
    }
    else
    {
        mset->label_template = NULL;
        mset->combo_template = NULL;
        mset->combo_template_dir = NULL;
    }
    
    // Name
    mset->label_name = GTK_LABEL( gtk_label_new( NULL ) );
    gtk_label_set_markup_with_mnemonic( mset->label_name, _("<b>_Name:</b>") );
    gtk_misc_set_alignment( GTK_MISC( mset->label_name ), 0, 0 );
    mset->scroll_name = gtk_scrolled_window_new( NULL, NULL );
    mset->input_name = GTK_WIDGET( multi_input_new( 
                    GTK_SCROLLED_WINDOW( mset->scroll_name ), NULL, FALSE ) );
    gtk_label_set_mnemonic_widget( mset->label_name, mset->input_name );
    g_signal_connect( G_OBJECT( mset->input_name ), "key-press-event",
                          G_CALLBACK( on_move_keypress ), mset );
    g_signal_connect( G_OBJECT( mset->input_name ), "mnemonic-activate",
                          G_CALLBACK( on_mnemonic_activate ), mset );
    gtk_label_set_selectable(  mset->label_name, TRUE );
    g_signal_connect( G_OBJECT( mset->label_name ), "button-press-event",
                          G_CALLBACK( on_label_button_press ), mset );
    g_signal_connect( G_OBJECT( mset->label_name ), "focus",
                          G_CALLBACK( on_label_focus ), mset );
    mset->buf_name =  GTK_TEXT_BUFFER( gtk_text_view_get_buffer( 
                                        GTK_TEXT_VIEW( mset->input_name ) ) );
    g_signal_connect( G_OBJECT( mset->buf_name ), "changed",
                          G_CALLBACK( on_move_change ), mset );
    g_signal_connect( G_OBJECT( mset->input_name ), "focus",
                          G_CALLBACK( on_focus ), mset );
    mset->blank_name = GTK_LABEL( gtk_label_new( NULL ) );
    
    // Ext
    mset->label_ext = GTK_LABEL( gtk_label_new( NULL ) );
    gtk_label_set_markup_with_mnemonic( mset->label_ext, _("<b>E_xtension:</b>") );
    gtk_misc_set_alignment( GTK_MISC( mset->label_ext ), 0, 1 );
    mset->entry_ext = GTK_ENTRY( gtk_entry_new() );
    gtk_label_set_mnemonic_widget( mset->label_ext, GTK_WIDGET( mset->entry_ext ) );
    g_signal_connect( G_OBJECT( mset->entry_ext ), "mnemonic-activate",
                          G_CALLBACK( on_mnemonic_activate ), mset );
    gtk_label_set_selectable(  mset->label_ext, TRUE );
    g_signal_connect( G_OBJECT( mset->label_ext ), "button-press-event",
                          G_CALLBACK( on_label_button_press ), mset );
    g_signal_connect( G_OBJECT( mset->label_ext ), "focus",
                          G_CALLBACK( on_label_focus ), mset );
    g_signal_connect( G_OBJECT( mset->entry_ext ), "key-press-event",
                          G_CALLBACK( on_move_entry_keypress ), mset );
    g_signal_connect( G_OBJECT( mset->entry_ext ), "changed",
                          G_CALLBACK( on_move_change ), mset );
    g_signal_connect_after( G_OBJECT( mset->entry_ext ), "focus",
                          G_CALLBACK( on_focus ), mset );
    gtk_widget_set_sensitive( GTK_WIDGET( mset->entry_ext ), !mset->is_dir );
    gtk_widget_set_sensitive( GTK_WIDGET( mset->label_ext ), !mset->is_dir );

    // Filename
    mset->label_full_name = GTK_LABEL( gtk_label_new( NULL ) );
    gtk_label_set_markup_with_mnemonic( mset->label_full_name, _("<b>_Filename:</b>") );    gtk_misc_set_alignment( GTK_MISC( mset->label_full_name ), 0, 0 );
    mset->scroll_full_name = gtk_scrolled_window_new( NULL, NULL );
    mset->input_full_name = GTK_WIDGET( multi_input_new( 
                    GTK_SCROLLED_WINDOW( mset->scroll_full_name ), NULL, FALSE ) );
    gtk_label_set_mnemonic_widget( mset->label_full_name, mset->input_full_name );
    g_signal_connect( G_OBJECT( mset->input_full_name ), "mnemonic-activate",
                          G_CALLBACK( on_mnemonic_activate ), mset );
    gtk_label_set_selectable(  mset->label_full_name, TRUE );
    g_signal_connect( G_OBJECT( mset->label_full_name ), "button-press-event",
                          G_CALLBACK( on_label_button_press ), mset );
    g_signal_connect( G_OBJECT( mset->label_full_name ), "focus",
                          G_CALLBACK( on_label_focus ), mset );
    g_signal_connect( G_OBJECT( mset->input_full_name ), "key-press-event",
                          G_CALLBACK( on_move_keypress ), mset );
    mset->buf_full_name =  gtk_text_view_get_buffer( 
                                        GTK_TEXT_VIEW( mset->input_full_name ) );
    g_signal_connect( G_OBJECT( mset->buf_full_name ), "changed",
                          G_CALLBACK( on_move_change ), mset );
    g_signal_connect( G_OBJECT( mset->input_full_name ), "focus",
                          G_CALLBACK( on_focus ), mset );
    mset->blank_full_name = GTK_LABEL( gtk_label_new( NULL ) );

    // Parent
    mset->label_path = GTK_LABEL( gtk_label_new( NULL ) );
    gtk_label_set_markup_with_mnemonic( mset->label_path, _("<b>_Parent:</b>") );
    gtk_misc_set_alignment( GTK_MISC( mset->label_path ), 0, 0 );
    mset->scroll_path = gtk_scrolled_window_new( NULL, NULL );
    mset->input_path = GTK_WIDGET( multi_input_new( GTK_SCROLLED_WINDOW( mset->scroll_path ),
                                                                NULL, FALSE ) );
    gtk_label_set_mnemonic_widget( mset->label_path, mset->input_path );
    g_signal_connect( G_OBJECT( mset->input_path ), "mnemonic-activate",
                          G_CALLBACK( on_mnemonic_activate ), mset );
    gtk_label_set_selectable(  mset->label_path, TRUE );
    g_signal_connect( G_OBJECT( mset->label_path ), "button-press-event",
                          G_CALLBACK( on_label_button_press ), mset );
    g_signal_connect( G_OBJECT( mset->label_path ), "focus",
                          G_CALLBACK( on_label_focus ), mset );
    g_signal_connect( G_OBJECT( mset->input_path ), "key-press-event",
                          G_CALLBACK( on_move_keypress ), mset );
    mset->buf_path =  gtk_text_view_get_buffer( GTK_TEXT_VIEW( mset->input_path ) );
    g_signal_connect( G_OBJECT( mset->buf_path ), "changed",
                          G_CALLBACK( on_move_change ), mset );
    g_signal_connect( G_OBJECT( mset->input_path ), "focus",
                          G_CALLBACK( on_focus ), mset );
    mset->blank_path = GTK_LABEL( gtk_label_new( NULL ) );

    // Path
    mset->label_full_path = GTK_LABEL( gtk_label_new( NULL ) );
    gtk_label_set_markup_with_mnemonic( mset->label_full_path, _("<b>P_ath:</b>") );
    gtk_misc_set_alignment( GTK_MISC( mset->label_full_path ), 0, 0 );
    mset->scroll_full_path = gtk_scrolled_window_new( NULL, NULL );
    // set initial path
    mset->input_full_path = GTK_WIDGET( multi_input_new( 
                                GTK_SCROLLED_WINDOW( mset->scroll_full_path ),
                                mset->new_path, FALSE ) );
    gtk_label_set_mnemonic_widget( mset->label_full_path, mset->input_full_path );
    g_signal_connect( G_OBJECT( mset->input_full_path ), "mnemonic-activate",
                          G_CALLBACK( on_mnemonic_activate ), mset );
    gtk_label_set_selectable(  mset->label_full_path, TRUE );
    g_signal_connect( G_OBJECT( mset->label_full_path ), "button-press-event",
                          G_CALLBACK( on_label_button_press ), mset );
    g_signal_connect( G_OBJECT( mset->label_full_path ), "focus",
                          G_CALLBACK( on_label_focus ), mset );
    g_signal_connect( G_OBJECT( mset->input_full_path ), "key-press-event",
                          G_CALLBACK( on_move_keypress ), mset );
    mset->buf_full_path =  gtk_text_view_get_buffer( 
                                    GTK_TEXT_VIEW( mset->input_full_path ) );
    g_signal_connect( G_OBJECT( mset->buf_full_path ), "changed",
                          G_CALLBACK( on_move_change ), mset );
    g_signal_connect( G_OBJECT( mset->input_full_path ), "focus",
                          G_CALLBACK( on_focus ), mset );

    // Options
    mset->opt_move =  gtk_radio_button_new_with_mnemonic( NULL, _("Mov_e") );
    mset->opt_copy =  gtk_radio_button_new_with_mnemonic_from_widget(
                                    GTK_RADIO_BUTTON(mset->opt_move), _("Cop_y") );
    mset->opt_link =  gtk_radio_button_new_with_mnemonic_from_widget( 
                                    GTK_RADIO_BUTTON(mset->opt_move), _("Lin_k") );
    mset->opt_copy_target =  gtk_radio_button_new_with_mnemonic_from_widget( 
                                    GTK_RADIO_BUTTON(mset->opt_move), _("Copy _Target") );
    mset->opt_link_target =  gtk_radio_button_new_with_mnemonic_from_widget( 
                                    GTK_RADIO_BUTTON(mset->opt_move), _("Link Tar_get") );
    mset->opt_as_root = gtk_check_button_new_with_mnemonic( _("A_s Root") );

    mset->opt_new_file = gtk_radio_button_new_with_mnemonic( NULL, C_("New|Radio", "Fil_e") );
    mset->opt_new_folder = gtk_radio_button_new_with_mnemonic_from_widget( 
                        GTK_RADIO_BUTTON( mset->opt_new_file ), C_("New|Radio", "Fol_der") );
    mset->opt_new_link = gtk_radio_button_new_with_mnemonic_from_widget( 
                        GTK_RADIO_BUTTON( mset->opt_new_file ), C_("New|Radio", "_Link") );

    gtk_button_set_focus_on_click( GTK_BUTTON( mset->opt_move ), FALSE );
    g_signal_connect( G_OBJECT( mset->opt_move ), "focus",
                                            G_CALLBACK( on_button_focus ), mset );
    gtk_button_set_focus_on_click( GTK_BUTTON( mset->opt_copy ), FALSE );
    gtk_button_set_focus_on_click( GTK_BUTTON( mset->opt_link ), FALSE );
    gtk_button_set_focus_on_click( GTK_BUTTON( mset->opt_copy_target ), FALSE );
    gtk_button_set_focus_on_click( GTK_BUTTON( mset->opt_link_target ), FALSE );
    gtk_button_set_focus_on_click( GTK_BUTTON( mset->opt_as_root ), FALSE );
    gtk_button_set_focus_on_click( GTK_BUTTON( mset->opt_new_file ), FALSE );
    g_signal_connect( G_OBJECT( mset->opt_new_file ), "focus",
                                            G_CALLBACK( on_button_focus ), mset );
    gtk_button_set_focus_on_click( GTK_BUTTON( mset->opt_new_folder ), FALSE );
    gtk_button_set_focus_on_click( GTK_BUTTON( mset->opt_new_link ), FALSE );
    gtk_widget_set_sensitive( mset->opt_copy_target, mset->is_link && !target_missing );
    gtk_widget_set_sensitive( mset->opt_link_target, mset->is_link );

    // Pack
    GtkWidget* dlg_vbox = gtk_dialog_get_content_area( GTK_DIALOG( mset->dlg ) );
    gtk_container_set_border_width( GTK_CONTAINER ( mset->dlg ), 10 );

    gtk_box_pack_start( GTK_BOX( dlg_vbox ),
                        GTK_WIDGET( mset->label_name ), FALSE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( dlg_vbox ),
                        GTK_WIDGET( mset->scroll_name ), TRUE, TRUE, 0 );

    mset->hbox_ext = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( mset->hbox_ext ),
                        GTK_WIDGET( mset->label_ext ), FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( mset->hbox_ext ),
                            GTK_WIDGET( gtk_label_new( " " ) ), FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( mset->hbox_ext ),
                        GTK_WIDGET( mset->entry_ext ), TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( dlg_vbox ),
                        GTK_WIDGET( mset->hbox_ext ), FALSE, TRUE, 5 );
    gtk_box_pack_start( GTK_BOX( dlg_vbox ),
                        GTK_WIDGET( mset->blank_name ), FALSE, TRUE, 0 );

    gtk_box_pack_start( GTK_BOX( dlg_vbox ),
                        GTK_WIDGET( mset->label_full_name ), FALSE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( dlg_vbox ),
                        GTK_WIDGET( mset->scroll_full_name ), TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( dlg_vbox ),
                        GTK_WIDGET( mset->blank_full_name ), FALSE, TRUE, 0 );

    gtk_box_pack_start( GTK_BOX( dlg_vbox ),
                        GTK_WIDGET( mset->label_path ), FALSE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( dlg_vbox ),
                        GTK_WIDGET( mset->scroll_path ), TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( dlg_vbox ),
                        GTK_WIDGET( mset->blank_path ), FALSE, TRUE, 0 );

    gtk_box_pack_start( GTK_BOX( dlg_vbox ),
                        GTK_WIDGET( mset->label_full_path ), FALSE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( dlg_vbox ),
                        GTK_WIDGET( mset->scroll_full_path ), TRUE, TRUE, 0 );

    mset->hbox_type = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( mset->hbox_type ),
                        GTK_WIDGET( mset->label_type ), FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( mset->hbox_type ),
                        GTK_WIDGET( mset->label_mime ), TRUE, TRUE, 5 );
    gtk_box_pack_start( GTK_BOX( dlg_vbox ),
                        GTK_WIDGET( mset->hbox_type ), FALSE, TRUE, 5 );

    mset->hbox_target = gtk_hbox_new( FALSE, 0 );
    if ( mset->label_target )
    {
        gtk_box_pack_start( GTK_BOX( mset->hbox_target ),
                            GTK_WIDGET( mset->label_target ), FALSE, TRUE, 0 );
        if ( !create_new )
            gtk_box_pack_start( GTK_BOX( mset->hbox_target ),
                            GTK_WIDGET( gtk_label_new( " " ) ), FALSE, TRUE, 0 );
        gtk_box_pack_start( GTK_BOX( mset->hbox_target ),
                            GTK_WIDGET( mset->entry_target ), TRUE, TRUE,
                            create_new ? 3 : 0 );
        if ( mset->browse_target )
            gtk_box_pack_start( GTK_BOX( mset->hbox_target ),
                            GTK_WIDGET( mset->browse_target ), FALSE, TRUE, 0 );
            gtk_box_pack_start( GTK_BOX( dlg_vbox ),
                            GTK_WIDGET( mset->hbox_target ), FALSE, TRUE, 5 );
    }

    mset->hbox_template = gtk_hbox_new( FALSE, 0 );
    if ( mset->label_template )
    {
        gtk_box_pack_start( GTK_BOX( mset->hbox_template ),
                            GTK_WIDGET( mset->label_template ), FALSE, TRUE, 0 );
        gtk_box_pack_start( GTK_BOX( mset->hbox_template ),
                            GTK_WIDGET( mset->combo_template ), TRUE, TRUE, 3 );
        gtk_box_pack_start( GTK_BOX( mset->hbox_template ),
                            GTK_WIDGET( mset->combo_template_dir ), TRUE, TRUE, 3 );
        gtk_box_pack_start( GTK_BOX( mset->hbox_template ),
                            GTK_WIDGET( mset->browse_template ), FALSE, TRUE, 0 );
        gtk_box_pack_start( GTK_BOX( dlg_vbox ),
                            GTK_WIDGET( mset->hbox_template ), FALSE, TRUE, 5 );
    }

    GtkWidget* hbox = gtk_hbox_new( FALSE, 4 );
    if ( create_new )
    {
        gtk_box_pack_start( GTK_BOX( hbox ),
                            GTK_WIDGET( gtk_label_new( _("New") ) ), FALSE, TRUE, 3 );
        gtk_box_pack_start( GTK_BOX( hbox ),
                            GTK_WIDGET( mset->opt_new_file ), FALSE, TRUE, 3 );
        gtk_box_pack_start( GTK_BOX( hbox ),
                            GTK_WIDGET( mset->opt_new_folder ), FALSE, TRUE, 3 );
        gtk_box_pack_start( GTK_BOX( hbox ),
                            GTK_WIDGET( mset->opt_new_link ), FALSE, TRUE, 3 );
    }
    else
    {
        gtk_box_pack_start( GTK_BOX( hbox ),
                            GTK_WIDGET( mset->opt_move ), FALSE, TRUE, 3 );
        gtk_box_pack_start( GTK_BOX( hbox ),
                            GTK_WIDGET( mset->opt_copy ), FALSE, TRUE, 3 );
        gtk_box_pack_start( GTK_BOX( hbox ),
                            GTK_WIDGET( mset->opt_link ), FALSE, TRUE, 3 );
        gtk_box_pack_start( GTK_BOX( hbox ),
                            GTK_WIDGET( mset->opt_copy_target ), FALSE, TRUE, 3 );
        gtk_box_pack_start( GTK_BOX( hbox ),
                            GTK_WIDGET( mset->opt_link_target ), FALSE, TRUE, 3 );
    }
    gtk_box_pack_start( GTK_BOX( hbox ),
                            GTK_WIDGET( gtk_label_new( "  " ) ), FALSE, TRUE, 3 );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( mset->opt_as_root ), FALSE, TRUE, 6 );
    gtk_box_pack_start( GTK_BOX( dlg_vbox ),
                        GTK_WIDGET( hbox ), FALSE, TRUE, 10 );
    
    // show
    on_font_change( NULL, mset );
    gtk_widget_show_all( mset->dlg );
    on_toggled( NULL, mset );
    if ( mset->clip_copy )
    {
        gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON( mset->opt_copy ), TRUE );
        gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON( mset->opt_move ), FALSE );
    }
    else if ( create_new == PTK_RENAME_NEW_DIR )
    {
        gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON( mset->opt_new_folder ), TRUE );
        gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON( mset->opt_new_file ), FALSE );
    }
    else if ( create_new == PTK_RENAME_NEW_LINK )
    {
        gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON( mset->opt_new_link ), TRUE );
        gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON( mset->opt_new_file ), FALSE );
    }
    
    // signals
    g_signal_connect( G_OBJECT( mset->opt_move ), "toggled",
                                        G_CALLBACK( on_opt_toggled ), mset );
    g_signal_connect( G_OBJECT( mset->opt_copy ), "toggled",
                                        G_CALLBACK( on_opt_toggled ), mset );
    g_signal_connect( G_OBJECT( mset->opt_link ), "toggled",
                                        G_CALLBACK( on_opt_toggled ), mset );
    g_signal_connect( G_OBJECT( mset->opt_copy_target ), "toggled",
                                        G_CALLBACK( on_opt_toggled ), mset );
    g_signal_connect( G_OBJECT( mset->opt_link_target ), "toggled",
                                        G_CALLBACK( on_opt_toggled ), mset );
    g_signal_connect( G_OBJECT( mset->opt_as_root ), "toggled",
                                        G_CALLBACK( on_opt_toggled ), mset );
    g_signal_connect( G_OBJECT( mset->opt_new_file ), "toggled",
                                        G_CALLBACK( on_opt_toggled ), mset );
    g_signal_connect( G_OBJECT( mset->opt_new_folder ), "toggled",
                                        G_CALLBACK( on_opt_toggled ), mset );
    g_signal_connect( G_OBJECT( mset->opt_new_link ), "toggled",
                                        G_CALLBACK( on_opt_toggled ), mset );

    // init
    on_move_change( GTK_WIDGET( mset->buf_full_path ), mset );
    on_opt_toggled( NULL, mset );

    /* instead of using last used widget, just use first visible
    // last widget
    int last = xset_get_int( "move_dlg_font", "z" );
    if ( last == 1 )
        mset->last_widget = mset->input_name;
    else if ( last == 2 )
        mset->last_widget = GTK_WIDGET( mset->entry_ext );
    else if ( last == 3 )
        mset->last_widget = mset->input_path;
    else if ( last == 4 )
        mset->last_widget = mset->input_full_path;
    else
        mset->last_widget = mset->input_full_name;

    if ( !gtk_widget_get_visible( gtk_widget_get_parent( mset->last_widget ) ) )
    {
        if ( gtk_widget_get_visible( gtk_widget_get_parent( mset->input_name ) ) )
            mset->last_widget = mset->input_name;
        else if ( gtk_widget_get_visible( gtk_widget_get_parent( mset->input_full_name ) ) )
            mset->last_widget = mset->input_full_name;
        else if ( gtk_widget_get_visible( gtk_widget_get_parent( mset->input_path ) ) )
            mset->last_widget = mset->input_path;
        else if ( gtk_widget_get_visible( gtk_widget_get_parent( mset->input_full_path ) ) )
            mset->last_widget = mset->input_full_path;
    }
    */
    if ( gtk_widget_get_visible( gtk_widget_get_parent( mset->input_name ) ) )
        mset->last_widget = mset->input_name;
    else if ( gtk_widget_get_visible( gtk_widget_get_parent( mset->input_full_name ) ) )
        mset->last_widget = mset->input_full_name;
    else if ( gtk_widget_get_visible( gtk_widget_get_parent( mset->input_path ) ) )
        mset->last_widget = mset->input_path;
    else if ( gtk_widget_get_visible( gtk_widget_get_parent( mset->input_full_path ) ) )
        mset->last_widget = mset->input_full_path;
    
    // select last widget
    select_input( mset->last_widget, mset );
    gtk_widget_grab_focus( mset->last_widget );

    g_signal_connect( G_OBJECT( mset->options ), "focus",
                            G_CALLBACK( on_button_focus ), mset );
    g_signal_connect( G_OBJECT( mset->next ), "focus",
                            G_CALLBACK( on_button_focus ), mset );
    g_signal_connect( G_OBJECT( mset->cancel ), "focus",
                            G_CALLBACK( on_button_focus ), mset );

    // not used to speed keypress processing 
    //g_signal_connect( G_OBJECT( mset->dlg ), "key-press-event",
    //                        G_CALLBACK( on_dlg_keypress ), mset );

    // run
    int response;
    while ( response = gtk_dialog_run( GTK_DIALOG( mset->dlg ) ) )
    {
        if ( response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY )
        {
            gtk_text_buffer_get_start_iter( mset->buf_full_path, &siter );
            gtk_text_buffer_get_end_iter( mset->buf_full_path, &iter );
            full_path = gtk_text_buffer_get_text( mset->buf_full_path, &siter, &iter, FALSE );
            if ( full_path[0] != '/' )
            {
                // update full_path to absolute
                char* cwd = g_path_get_dirname( mset->full_path );
                char* old_path = full_path;
                full_path = g_build_filename( cwd, old_path, NULL );
                g_free( cwd );
                g_free( old_path );
            }
            if ( strchr( full_path, '\n' ) )
            {
                ptk_show_error( GTK_WINDOW( mset->dlg ),
                                _("Error"), _("Path contains linefeeds") );   
                g_free( full_path );
                continue;
            }
            full_name = g_path_get_basename( full_path );
            path = g_path_get_dirname( full_path );
            old_path = g_path_get_dirname( mset->full_path );
            gboolean overwrite = FALSE;
            char* msg;
            
            if ( response == GTK_RESPONSE_APPLY )
                ret = 2;
            
            if ( !create_new && ( mset->full_path_same ||
                                    !strcmp( full_path, mset->full_path ) ) )
            {
                // not changed, proceed to next file
                g_free( full_path );
                g_free( full_name );
                g_free( path );
                g_free( old_path );
                break;
            }
            
            // determine job
            gboolean move = gtk_toggle_button_get_active( 
                                            GTK_TOGGLE_BUTTON( mset->opt_move ) );
            gboolean copy = gtk_toggle_button_get_active( 
                                            GTK_TOGGLE_BUTTON( mset->opt_copy ) );
            gboolean link = gtk_toggle_button_get_active( 
                                            GTK_TOGGLE_BUTTON( mset->opt_link ) );
            gboolean copy_target = gtk_toggle_button_get_active( 
                                            GTK_TOGGLE_BUTTON( mset->opt_copy_target ) );
            gboolean link_target = gtk_toggle_button_get_active( 
                                            GTK_TOGGLE_BUTTON( mset->opt_link_target ) );
            gboolean as_root = gtk_toggle_button_get_active( 
                                            GTK_TOGGLE_BUTTON( mset->opt_as_root ) );
            gboolean new_file = gtk_toggle_button_get_active( 
                                            GTK_TOGGLE_BUTTON( mset->opt_new_file ) );
            gboolean new_folder = gtk_toggle_button_get_active( 
                                            GTK_TOGGLE_BUTTON( mset->opt_new_folder ) );
            gboolean new_link = gtk_toggle_button_get_active( 
                                            GTK_TOGGLE_BUTTON( mset->opt_new_link ) );

            if ( as_root )
                root_msg = _(" As Root");
            else
                root_msg = "";

            if ( !g_file_test( path, G_FILE_TEST_EXISTS ) )
            {
                // create parent folder
                if ( xset_get_b( "move_dlg_confirm_create" ) )
                {
                    if ( xset_msg_dialog( mset->parent, GTK_MESSAGE_QUESTION,
                                            _("Create Parent Folder"), NULL,
                                             GTK_BUTTONS_YES_NO,
                                             _("The parent folder does not exist.  Create it?"), NULL, NULL )
                                                            != GTK_RESPONSE_YES )
                        goto _continue_free;
                }
                if ( as_root )
                {
                    g_free( root_mkdir );
                    to_path = bash_quote( path );
                    root_mkdir = g_strdup_printf( "mkdir -p %s && ", to_path );
                    g_free( to_path );
                }
                else if ( g_mkdir_with_parents( path, 0755 ) != 0 )
                {
                    msg = g_strdup_printf( _("Error creating parent folder\n\n%s"),
                                                            strerror( errno ) );
                    ptk_show_error( GTK_WINDOW( mset->dlg ),
                                    _("Mkdir Error"), msg );
                    g_free( msg );
                    goto _continue_free;
                }
                else
                    update_new_display( path );
            }
            else if ( lstat64( full_path, &statbuf ) == 0 )
            {
                // overwrite
                if ( g_file_test( full_path, G_FILE_TEST_IS_DIR ) )
                    goto _continue_free;  // just in case
                if ( xset_msg_dialog( mset->parent, GTK_MESSAGE_WARNING,
                                        _("Overwrite Existing File"), NULL,
                                         GTK_BUTTONS_YES_NO,
                                         _("OVERWRITE WARNING"), _("The file path exists.  Overwrite existing file?"), NULL )
                                                        != GTK_RESPONSE_YES )
                    goto _continue_free;
                overwrite = TRUE;
            }

            if ( create_new && new_link )
            {
                // new link task
                task_name = g_strdup_printf( _("Create Link%s"), root_msg );
                PtkFileTask* task = ptk_file_exec_new( task_name, NULL, mset->parent,
                                                                    task_view );
                g_free( task_name );

                str = g_strdup( gtk_entry_get_text( mset->entry_target ) );
                g_strstrip( str );
                while ( g_str_has_suffix( str, "/" ) && str[1] != '\0' )
                    str[g_utf8_strlen( str, -1 ) - 1] = '\0';
                from_path = bash_quote( str );
                g_free( str );
                to_path = bash_quote( full_path );

                if ( overwrite )
                {
                    task->task->exec_command = g_strdup_printf( "%sln -sf %s %s",
                                                root_mkdir, from_path, to_path );
                }
                else
                {
                    task->task->exec_command = g_strdup_printf( "%sln -s %s %s",
                                                root_mkdir, from_path, to_path );
                }
                g_free( from_path );
                g_free( to_path );
                task->task->exec_sync = TRUE;
                task->task->exec_popup = FALSE;
                task->task->exec_show_output = FALSE;
                task->task->exec_show_error = TRUE;
                task->task->exec_export = FALSE;
                task->task->exec_as_user = as_root ? g_strdup( "root" ) : NULL;
                if ( auto_open )
                {   
                    auto_open->path = g_strdup( full_path );
                    auto_open->open_file = ( response == GTK_RESPONSE_APPLY );
                    task->complete_notify = auto_open->callback;
                    task->user_data = auto_open;
                }
                ptk_file_task_run( task );
                update_new_display( full_path );
            }
            else if ( create_new && new_file )
            {
                // new file task
                if ( gtk_widget_get_visible( gtk_widget_get_parent(
                                        GTK_WIDGET( mset->combo_template ) ) ) &&
                        ( str = gtk_combo_box_text_get_active_text( 
                                GTK_COMBO_BOX_TEXT( mset->combo_template ) ) ) )
                {
                    g_strstrip( str );
                    if ( str[0] == '/' )
                        from_path = bash_quote( str );
                    else if ( !g_strcmp0( _("Empty File"), str ) || str[0] == '\0' )
                        from_path = NULL;
                    else
                    {
                        char* tdir = get_template_dir();
                        if ( tdir )
                        {
                            from_path = g_build_filename( tdir, str, NULL );
                            if ( !g_file_test( from_path, G_FILE_TEST_IS_REGULAR ) )
                            {
                                ptk_show_error( GTK_WINDOW( mset->dlg ), _("Template Missing"),
                                                    _("The specified template does not exist") );
                                g_free( from_path );
                                g_free( str );
                                g_free( tdir );
                                goto _continue_free;
                            }
                            g_free( str );
                            g_free( tdir );
                            str = from_path;
                            from_path = bash_quote( str );
                        }
                        else
                            from_path = NULL;
                    }
                    g_free( str );
                }
                else
                    from_path = NULL;
                to_path = bash_quote( full_path );
                char* over_cmd;
                if ( overwrite )
                    over_cmd = g_strdup_printf( "rm -f %s && ", to_path );
                else
                    over_cmd = g_strdup( "" );

                task_name = g_strdup_printf( _("Create New File%s"), root_msg );
                PtkFileTask* task = ptk_file_exec_new( task_name, NULL, mset->parent,
                                                                    task_view );
                g_free( task_name );
                if ( !from_path )
                    task->task->exec_command = g_strdup_printf( "%s%stouch %s",
                                            root_mkdir, over_cmd, to_path );
                else
                    task->task->exec_command = g_strdup_printf( "%s%scp -f %s %s",
                                            root_mkdir, over_cmd, from_path, to_path );
                g_free( from_path );
                g_free( to_path );
                g_free( over_cmd );
                task->task->exec_sync = TRUE;
                task->task->exec_popup = FALSE;
                task->task->exec_show_output = FALSE;
                task->task->exec_show_error = TRUE;
                task->task->exec_export = FALSE;
                task->task->exec_as_user = as_root ? g_strdup( "root" ) : NULL;
                if ( auto_open )
                {   
                    auto_open->path = g_strdup( full_path );
                    auto_open->open_file = ( response == GTK_RESPONSE_APPLY );
                    task->complete_notify = auto_open->callback;
                    task->user_data = auto_open;
                }
                ptk_file_task_run( task );
                update_new_display( full_path );
            }
            else if ( create_new )
            {
                // new folder task
                if ( !new_folder )
                    goto _continue_free;  // failsafe
                if ( gtk_widget_get_visible( gtk_widget_get_parent(
                                        GTK_WIDGET( mset->combo_template_dir ) ) ) && 
                        ( str = gtk_combo_box_text_get_active_text( 
                                GTK_COMBO_BOX_TEXT( mset->combo_template_dir ) ) ) )
                {
                    g_strstrip( str );
                    if ( str[0] == '/' )
                        from_path = bash_quote( str );
                    else if ( !g_strcmp0( _("Empty Folder"), str ) || str[0] == '\0' )
                        from_path = NULL;
                    else
                    {
                        char* tdir = get_template_dir();
                        if ( tdir )
                        {
                            from_path = g_build_filename( tdir, str, NULL );
                            if ( !g_file_test( from_path, G_FILE_TEST_IS_DIR ) )
                            {
                                ptk_show_error( GTK_WINDOW( mset->dlg ), _("Template Missing"),
                                                    _("The specified template does not exist") );
                                g_free( from_path );
                                g_free( str );
                                g_free( tdir );
                                goto _continue_free;
                            }
                            g_free( str );
                            g_free( tdir );
                            str = from_path;
                            from_path = bash_quote( str );
                        }
                        else
                            from_path = NULL;
                    }
                    g_free( str );
                }
                else
                    from_path = NULL;
                to_path = bash_quote( full_path );

                task_name = g_strdup_printf( _("Create New Folder%s"), root_msg );
                PtkFileTask* task = ptk_file_exec_new( task_name, NULL, mset->parent,
                                                                    task_view );
                g_free( task_name );
                if ( !from_path )
                    task->task->exec_command = g_strdup_printf( "%smkdir %s",
                                            root_mkdir, to_path );
                else
                    task->task->exec_command = g_strdup_printf( "%scp -rL %s %s",
                                            root_mkdir, from_path, to_path );
                g_free( from_path );
                g_free( to_path );
                task->task->exec_sync = TRUE;
                task->task->exec_popup = FALSE;
                task->task->exec_show_output = FALSE;
                task->task->exec_show_error = TRUE;
                task->task->exec_export = FALSE;
                task->task->exec_as_user = as_root ? g_strdup( "root" ) : NULL;
                if ( auto_open )
                {   
                    auto_open->path = g_strdup( full_path );
                    auto_open->open_file = ( response == GTK_RESPONSE_APPLY );
                    task->complete_notify = auto_open->callback;
                    task->user_data = auto_open;
                }
                ptk_file_task_run( task );
                update_new_display( full_path );
            }
            else if ( copy || copy_target )
            {
                // copy task
                task_name = g_strdup_printf( _("Copy%s"), root_msg );
                PtkFileTask* task = ptk_file_exec_new( task_name, NULL, mset->parent,
                                                                    task_view );
                g_free( task_name );
                char* over_opt = NULL;
                to_path = bash_quote( full_path );
                if ( copy || !mset->is_link )
                    from_path = bash_quote( mset->full_path );
                else
                {
                    str = get_real_link_target( mset->full_path );
                    if ( !str )
                    {
                        ptk_show_error( GTK_WINDOW( mset->dlg ), _("Copy Target Error"),
                                                _("Error determining link's target") );
                        goto _continue_free;
                    }
                    from_path = bash_quote( str );
                    g_free( str );
                }
                if ( overwrite )
                    over_opt = g_strdup( " --remove-destination" );
                if ( !over_opt )
                    over_opt = g_strdup( "" );
                
                if ( mset->is_dir )
                {
                    task->task->exec_command = g_strdup_printf( "%scp -Pfr %s %s",
                                            root_mkdir, from_path, to_path );
                }
                else
                {
                    task->task->exec_command = g_strdup_printf( "%scp -Pf%s %s %s",
                                            root_mkdir, over_opt, from_path, to_path );
                }
                g_free( from_path );
                g_free( to_path );
                g_free( over_opt );
                task->task->exec_sync = TRUE;
                task->task->exec_popup = FALSE;
                task->task->exec_show_output = FALSE;
                task->task->exec_show_error = TRUE;
                task->task->exec_export = FALSE;
                task->task->exec_as_user = as_root ? g_strdup( "root" ) : NULL;
                ptk_file_task_run( task );
                update_new_display( full_path );
            }
            else if ( link || link_target )
            {
                // link task
                task_name = g_strdup_printf( _("Create Link%s"), root_msg );
                PtkFileTask* task = ptk_file_exec_new( task_name, NULL, mset->parent,
                                                                    task_view );
                g_free( task_name );
                if ( link || !mset->is_link )
                    from_path = bash_quote( mset->full_path );
                else
                {
                    str = get_real_link_target( mset->full_path );
                    if ( !str )
                    {
                        ptk_show_error( GTK_WINDOW( mset->dlg ), _("Link Target Error"),
                                                _("Error determining link's target") );
                        goto _continue_free;
                    }
                    from_path = bash_quote( str );
                    g_free( str );
                }
                to_path = bash_quote( full_path );
                if ( overwrite )
                {
                    task->task->exec_command = g_strdup_printf( "%sln -sf %s %s",
                                                root_mkdir, from_path, to_path );
                }
                else
                {
                    task->task->exec_command = g_strdup_printf( "%sln -s %s %s",
                                                root_mkdir, from_path, to_path );
                }
                g_free( from_path );
                g_free( to_path );
                task->task->exec_sync = TRUE;
                task->task->exec_popup = FALSE;
                task->task->exec_show_output = FALSE;
                task->task->exec_show_error = TRUE;
                task->task->exec_export = FALSE;
                task->task->exec_as_user = as_root ? g_strdup( "root" ) : NULL;
                ptk_file_task_run( task );
                update_new_display( full_path );
            }
            // need move?  (do move as task in case it takes a long time)
            else if ( as_root || strcmp( old_path, path ) )
            {
_move_task:
                // move task - this is jumped to from the below rename block on
                // EXDEV error
                task_name = g_strdup_printf( _("Move%s"), root_msg );
                PtkFileTask* task = ptk_file_exec_new( task_name, NULL, mset->parent,
                                                                    task_view );
                g_free( task_name );
                from_path = bash_quote( mset->full_path );
                to_path = bash_quote( full_path );
                if ( overwrite )
                {
                    task->task->exec_command = g_strdup_printf( "%smv -f %s %s",
                                                root_mkdir, from_path, to_path );
                }
                else
                {
                    task->task->exec_command = g_strdup_printf( "%smv %s %s",
                                                root_mkdir, from_path, to_path );
                }
                g_free( from_path );
                g_free( to_path );
                task->task->exec_sync = TRUE;
                task->task->exec_popup = FALSE;
                task->task->exec_show_output = FALSE;
                task->task->exec_show_error = TRUE;
                task->task->exec_export = FALSE;
                task->task->exec_as_user = as_root ? g_strdup( "root" ) : NULL;
                ptk_file_task_run( task );
                update_new_display( full_path );
            }
            else
            {
                // rename (does overwrite)
                if ( rename( mset->full_path, full_path ) != 0 )
                {
                    // Respond to an EXDEV error by switching to a move (e.g. aufs
                    // directory rename fails due to the directory existing in
                    // multiple underlying branches)
                    if ( errno == EXDEV )
                        goto _move_task;

                    // Unknown error has occurred - alert user as usual
                    msg = g_strdup_printf( _("Error renaming file\n\n%s"),
                                                            strerror( errno ) );
                    ptk_show_error( GTK_WINDOW( mset->dlg ),
                                    _("Rename Error"), msg );
                    g_free( msg );
                    goto _continue_free;
                }
                else
                    update_new_display( full_path );
            }
            g_free( full_path );
            g_free( full_name );
            g_free( path );
            g_free( old_path );
            break;
_continue_free:
            g_free( full_path );
            g_free( full_name );
            g_free( path );
            g_free( old_path );
        }
        else if ( response == GTK_RESPONSE_CANCEL
                                    || response == GTK_RESPONSE_DELETE_EVENT )
        {
            ret = 0;
            break;
        }
    }
    if ( response == 0 )
        ret = 0;
        
    // save size
    GtkAllocation allocation;
    gtk_widget_get_allocation ( GTK_WIDGET( mset->dlg ), &allocation );
    width = allocation.width;
    height = allocation.height;
    if ( width && height )
    {
        str = g_strdup_printf( "%d", width );
        xset_set( "move_dlg_font", "x", str );
        g_free( str );
        str = g_strdup_printf( "%d", height );
        xset_set( "move_dlg_font", "y", str );
        g_free( str );
    }

    // save last_widget - this is no longer used but save anyway
    int last;
    if ( mset->last_widget == mset->input_name )
        last = 1;
    else if ( mset->last_widget == GTK_WIDGET( mset->entry_ext ) )
        last = 2;
    else if ( mset->last_widget == mset->input_path )
        last = 3;
    else if ( mset->last_widget == mset->input_full_path )
        last = 4;
    else
        last = 0;
    str = g_strdup_printf( "%d", last );
    xset_set( "move_dlg_font", "z", str );
    g_free( str );
    
    // destroy
    gtk_widget_destroy( mset->dlg );

    g_free( root_mkdir );

    g_free( mset->full_path );
    if ( mset->new_path )
        g_free( mset->new_path );
    if ( mset->mime_type )
        g_free( mset->mime_type );
    g_slice_free( MoveSet, mset );
    
    return ret;
}

/*
gboolean  ptk_rename_file( GtkWindow* parent_win,
                      const char* cwd,
                      VFSFileInfo* file )
{
    GtkWidget * input_dlg;
    GtkLabel* prompt;
    char* ufile_name;
    char* file_name;
    char* from_path;
    char* to_path;
    gboolean ret = FALSE;
    char* disp_name = NULL;

    if ( !cwd || !file )
        return FALSE;

    // special processing for files with incosistent real name and display name
    if( G_UNLIKELY( vfs_file_info_is_desktop_entry(file) ) )
        disp_name = g_filename_display_name( file->name );

    input_dlg = ptk_input_dialog_new( _( "Rename File" ),
                                      _( "Please input new file name:" ),
                                      disp_name ? disp_name : vfs_file_info_get_disp_name( file ), parent_win );
    g_free( disp_name );
    gtk_window_set_default_size( GTK_WINDOW( input_dlg ),
                                 360, -1 );

    // Without this line, selected region in entry cannot be set
    gtk_widget_show( input_dlg );
    if ( ! vfs_file_info_is_dir( file ) )
        select_file_name_part( GTK_ENTRY( ptk_input_dialog_get_entry( GTK_WIDGET( input_dlg ) ) ) );

    while ( gtk_dialog_run( GTK_DIALOG( input_dlg ) ) == GTK_RESPONSE_OK )
    {
        prompt = GTK_LABEL( ptk_input_dialog_get_label( input_dlg ) );

        ufile_name = ptk_input_dialog_get_text( input_dlg );
        file_name = g_filename_from_utf8( ufile_name, -1, NULL, NULL, NULL );
        g_free( ufile_name );

        if ( file_name )
        {
            from_path = g_build_filename( cwd,
                                          vfs_file_info_get_name( file ), NULL );
            to_path = g_build_filename( cwd,
                                        file_name, NULL );

            if ( g_file_test( to_path, G_FILE_TEST_EXISTS ) )
            {
                gtk_label_set_text( prompt,
                                    _( "The file name you specified already exists.\n"
                                       "Please input a new one:" ) );
            }
            else
            {
                if ( 0 == rename( from_path, to_path ) )
                {
                    ret = TRUE;
                    break;
                }
                else
                {
                    gtk_label_set_text( prompt, strerror( errno ) );
                }
            }
            g_free( from_path );
            g_free( to_path );
        }
        else
        {
            gtk_label_set_text( prompt,
                                _( "The file name you specified already exists.\n"
                                   "Please input a new one:" ) );
        }
    }
    gtk_widget_destroy( input_dlg );
    return ret;
}
*/

gboolean  ptk_create_new_file( GtkWindow* parent_win,
                          const char* cwd,
                          gboolean create_folder,
                          VFSFileInfo** file )
{
    gchar * full_path;
    gchar* ufile_name;
    gchar* file_name;
    GtkLabel* prompt;
    int result;
    GtkWidget* dlg;
    gboolean ret = FALSE;
	gboolean looponce = FALSE;  //MOD

    if ( create_folder )
    {
        dlg = ptk_input_dialog_new( _( "New Folder" ),
                                    _( "New folder name:" ),
                                    _( "New Folder" ),
                                    parent_win );
    }
    else
    {
        dlg = ptk_input_dialog_new( _( "New File" ),
                                    _( "New filename:" ),
                                    _( "New File" ),
                                    parent_win );
    }
	
    while ( gtk_dialog_run( GTK_DIALOG( dlg ) ) == GTK_RESPONSE_OK )
    {
		looponce = TRUE;  //MOD 
        ufile_name = ptk_input_dialog_get_text( dlg );
        if ( g_get_filename_charsets( NULL ) )
            file_name = ufile_name;
        else
        {
            file_name = g_filename_from_utf8( ufile_name, -1,
                                              NULL, NULL, NULL );
            g_free( ufile_name );
        }
        full_path = g_build_filename( cwd, file_name, NULL );
        g_free( file_name );
        if ( g_file_test( full_path, G_FILE_TEST_EXISTS ) )
        {
            prompt = GTK_LABEL( ptk_input_dialog_get_label( dlg ) );
            gtk_label_set_text( prompt,
                                _( "Name already exists.\n\nPlease input a new one:" ) );
            g_free( full_path );
            continue;
        }
        if ( create_folder )
        {
            result = mkdir( full_path, 0755 );
            ret = (result==0);
        }
        else
        {
            result = open( full_path, O_CREAT, 0644 );
            if ( result != -1 )
            {
                close( result );
                ret = TRUE;
            }
        }

        if( ret && file )
        {
            *file = vfs_file_info_new();
            vfs_file_info_get( *file, full_path, NULL );
        }

        g_free( full_path );
        break;
    }
    gtk_widget_destroy( dlg );

    if( ! ret && looponce )  //MOD
        ptk_show_error( parent_win,
                        _("Error"),
                        _( "The new file cannot be created" ) );

    return ret;
}

void ptk_show_file_properties( GtkWindow* parent_win,
                               const char* cwd,
                               GList* sel_files, int page )
{
    GtkWidget * dlg;

    if ( sel_files )
    {
        /* Make a copy of the list */
        sel_files = g_list_copy( sel_files );
        g_list_foreach( sel_files, (GFunc) vfs_file_info_ref, NULL );

        dlg = file_properties_dlg_new( parent_win, cwd, sel_files, page );
    }
    else
    {
        // no files selected, use cwd as file
        VFSFileInfo* file = vfs_file_info_new();
        vfs_file_info_get( file, cwd, NULL );
        sel_files = g_list_prepend( NULL, vfs_file_info_ref( file ) );
        char* parent_dir = g_path_get_dirname( cwd );
        dlg = file_properties_dlg_new( parent_win, parent_dir, sel_files, page );
    }
    g_signal_connect_swapped( dlg, "destroy",
                              G_CALLBACK( vfs_file_info_list_free ), sel_files );
    gtk_widget_show( dlg );
}

static gboolean open_archives_with_handler( ParentInfo* parent,
                                            GList* sel_files,
                                            char* full_path,
                                            VFSMimeType* mime_type )
{
    gboolean extract_here = xset_get_b( "arc_def_ex" );
    const char* dest_dir = NULL;
    int cmd;
    
    if ( xset_get_b( "arc_def_open" ) )
        // user has open archives with app option enabled
        return FALSE;  // don't handle these files
    
    // determine default archive action in this dir
    if ( extract_here && have_rw_access( parent->cwd ) )
    {
        // Extract Here
        cmd = HANDLER_EXTRACT;
        dest_dir = parent->cwd;
    }
    else if ( extract_here || xset_get_b( "arc_def_exto" ) )
    {
        // Extract Here but no write access or Extract To option
        cmd = HANDLER_EXTRACT;
    }
    else if ( xset_get_b( "arc_def_list" ) )
    {
        // List contents
        cmd = HANDLER_LIST;
    }
    else
        return FALSE;  // don't handle these files
    
    // type or pathname has archive handler? - don't test command non-empty
    // here because only applies to first file
    GSList* handlers_slist = ptk_handler_file_has_handlers(
                                    HANDLER_MODE_ARC, cmd,
                                    full_path, mime_type, FALSE, FALSE, TRUE );
    if ( handlers_slist )
    {
        g_slist_free( handlers_slist );
        ptk_file_archiver_extract( parent->desktop, parent->file_browser,
                                   sel_files, parent->cwd, dest_dir, cmd, TRUE );
        return TRUE;  // all files handled
    }
    return FALSE;  // don't handle these files
}

static void open_files_with_handler( ParentInfo* parent,
                                     GList* files,
                                     XSet* handler_set )
{
    GList* l;
    char* str;
    char* quoted;
    char* command_final;
    char* command;
    char* name;

    printf( "\nSelected File Handler '%s'\n", handler_set->menu_label );

    // get command - was already checked as non-empty
    char* err_msg = ptk_handler_load_script( HANDLER_MODE_FILE,
                    HANDLER_MOUNT, handler_set,
                    NULL, &command );
    if ( err_msg )
    {
        xset_msg_dialog( parent->file_browser ?
                            GTK_WIDGET( parent->file_browser ) : NULL,
                            GTK_MESSAGE_ERROR,
                            _("Error Loading Handler"), NULL, 0, 
                            err_msg, NULL, NULL );
        g_free( err_msg );
        return;
    }
    // auto mount point
    if ( strstr( command, "%a" ) )
    {
        name = ptk_location_view_create_mount_point(
                                HANDLER_MODE_FILE, NULL, NULL,
                                files && files->data ?
                                    (char*)files->data : NULL );
        str = command;
        command = replace_string( command, "%a", name, FALSE );
        g_free( str );
        g_free( name );        
    }
    
    /* prepare bash vars for just the files being opened by this handler,
     * not necessarily all selected */
    GString* fm_filenames = g_string_new( "fm_filenames=(\n" );
    GString* fm_files = g_string_new( "fm_files=(\n" );
    // command looks like it handles multiple files ?
    gboolean multiple = ( strstr( command, "%N" ) || strstr( command, "%F" )
                                || strstr( command, "fm_files[" )
                                || strstr( command, "fm_filenames[" ) );
    if ( multiple )
    {
        for ( l = files; l; l = l->next )
        {
            // filename
            name = g_path_get_basename( (char*)l->data );
            quoted = bash_quote( name );
            g_free( name );
            g_string_append_printf( fm_filenames, "%s\n", quoted );
            g_free( quoted );
            // file path
            quoted = bash_quote( (char*)l->data );
            g_string_append_printf( fm_files, "%s\n", quoted );
            g_free( quoted );
        }
    }
    g_string_append( fm_filenames, ")\nfm_filename=\"$fm_filenames[0]\"\n" );
    g_string_append( fm_files, ")\nfm_file=\"$fm_files[0]\"\n" );
    // replace standard sub vars
    str = command;
    command = replace_line_subs( command );
    g_free( str );

    // start task(s)
    for ( l = files; l; l = l->next )
    {
        if ( multiple )
        {
            command_final = g_strdup_printf( "%s%s%s",
                                                    fm_filenames->str,
                                                    fm_files->str,
                                                    command );
        }
        else
        {
            // add sub vars for single file
            // filename
            name = g_path_get_basename( (char*)l->data );
            quoted = bash_quote( name );
            g_free( name );
            str = g_strdup_printf( "fm_filename=%s\n", quoted );
            g_free( quoted );
            // file path
            quoted = bash_quote( (char*)l->data );
            command_final = g_strdup_printf( "%s%s%sfm_file=%s\n%s",
                                                    fm_filenames->str,
                                                    fm_files->str,
                                                    str,
                                                    quoted,
                                                    command );
            g_free( str );
            g_free( quoted );
        }

        // Run task
        PtkFileTask* task = ptk_file_exec_new( handler_set->menu_label,
                                parent->cwd,
                                parent->file_browser ?
                                    GTK_WIDGET( parent->file_browser ) : NULL,
                                parent->file_browser ?
                                    parent->file_browser->task_view : NULL );
        // don't free cwd!
        task->task->exec_browser = parent->file_browser;
        task->task->exec_desktop = parent->desktop;
        task->task->exec_command = command_final;
        if ( handler_set->icon )
            task->task->exec_icon = g_strdup( handler_set->icon );
        task->task->exec_terminal = ( handler_set->in_terminal == XSET_B_TRUE );
        task->task->exec_keep_terminal = FALSE;
        // file handlers store Run As Task in keep_terminal
        task->task->exec_sync = handler_set->keep_terminal == XSET_B_TRUE;
        task->task->exec_show_error = task->task->exec_sync;
        task->task->exec_export = TRUE;
        ptk_file_task_run( task );
        
        if ( multiple )
            break;
    }
    g_string_free( fm_filenames, TRUE );
    g_string_free( fm_files, TRUE );
    g_free( command );
}

static gboolean open_files_with_app( ParentInfo* parent,
                                     GList* files,
                                     const char* app_desktop )
{
    gchar * name;
    GError* err = NULL;
    VFSAppDesktop* app;
    GdkScreen* screen;
    XSet* handler_set;
    
    if ( app_desktop && g_str_has_prefix( app_desktop, "###" ) &&
                            ( handler_set = xset_is( app_desktop + 3 ) )
                            && files )
    {
        // is a handler
        open_files_with_handler( parent, files, handler_set );
        return TRUE;
    }
    else if ( app_desktop )
    {
        /* Check whether this is an app desktop file or just a command line */
        if ( g_str_has_suffix ( app_desktop, ".desktop" ) )
            app = vfs_app_desktop_new( app_desktop );
        else
        {
            /* Not a desktop entry name */
            /*
            * If we are lucky enough, there might be a desktop entry
            * for this program
            */
            name = g_strconcat ( app_desktop, ".desktop", NULL );
            if ( g_file_test( name, G_FILE_TEST_EXISTS ) )
            {
                app = vfs_app_desktop_new( name );
            }
            else
            {
                /* dirty hack! */
                app = vfs_app_desktop_new( NULL );
                app->exec = g_strdup( app_desktop ); // freed by vfs_app_desktop_unref
            }
            g_free( name );
        }

        if ( parent->file_browser )
            screen = gtk_widget_get_screen( GTK_WIDGET( parent->file_browser ) );
        else if ( parent->desktop )
            screen = gtk_widget_get_screen( GTK_WIDGET( parent->desktop ) );
        else
            screen = gdk_screen_get_default();

        printf("EXEC(%s)=%s\n", app->full_path ? app->full_path : app_desktop,
                                                                    app->exec );
        if ( ! vfs_app_desktop_open_files( screen, parent->cwd, app, files, &err ) )
        {
            GtkWidget * toplevel = parent->file_browser ? gtk_widget_get_toplevel(
                                    GTK_WIDGET( parent->file_browser ) ) : NULL;
            ptk_show_error( GTK_WINDOW( toplevel ), _("Error"), err->message );
            g_error_free( err );
        }
        vfs_app_desktop_unref( app );
    }
    return TRUE;
}

static void open_files_with_each_app( gpointer key, gpointer value,
                                                    gpointer user_data )
{
    char* app_desktop = (char*)key;  // is const unless handler
    const char* cwd;
    GList* files = (GList*)value;
    ParentInfo* parent = (ParentInfo*)user_data;
    open_files_with_app( parent, files, app_desktop );
}

static void free_file_list_hash( gpointer key, gpointer value, gpointer user_data )
{
    const char * app_desktop;
    GList* files;

    app_desktop = ( const char* ) key;
    files = ( GList* ) value;
    g_list_foreach( files, ( GFunc ) g_free, NULL );
    g_list_free( files );
}

void ptk_open_files_with_app( const char* cwd,
                              GList* sel_files,
                              const char* app_desktop,
                              DesktopWindow* desktop,
                              PtkFileBrowser* file_browser,
                              gboolean xforce, gboolean xnever )
{
    // if xnever, never execute an executable
    // if xforce, force execute of executable ignoring app_settings.no_execute
    GList * l;
    gchar* full_path = NULL;
    VFSFileInfo* file;
    VFSMimeType* mime_type;
    GList* files_to_open = NULL;
    GHashTable* file_list_hash = NULL;
    GError* err;
    char* new_dir = NULL;
    char* alloc_desktop;
    GtkWidget* toplevel;
    PtkFileBrowser* fb;

    ParentInfo* parent = g_slice_new0( ParentInfo );
    parent->desktop = desktop;
    parent->file_browser = file_browser;
    parent->cwd = cwd;
    
    for ( l = sel_files; l; l = l->next )
    {
        file = ( VFSFileInfo* ) l->data;
        if ( G_UNLIKELY( ! file ) )
            continue;

        full_path = g_build_filename( cwd,
                                      vfs_file_info_get_name( file ),
                                      NULL );
        if ( G_LIKELY( full_path ) )
        {
            if ( app_desktop )
                // specified app to open all files
                files_to_open = g_list_append( files_to_open, full_path );
            else
            {
                // No app specified - Use default app for each file
                
                // Is a dir?  Open in browser
                if ( G_LIKELY( file_browser ) && 
                                g_file_test( full_path, G_FILE_TEST_IS_DIR ) )
                {
                    if ( ! new_dir )
                        new_dir = full_path;
                    else
                    {
                        if ( G_LIKELY( file_browser ) )
                        {
                            ptk_file_browser_emit_open( file_browser,
                                                        full_path, PTK_OPEN_NEW_TAB );
                        }
                    }
                    continue;
                }
                
                /* If this file is an executable file, run it. */
                if ( !xnever && vfs_file_info_is_executable( file, full_path ) &&
                                    ( ! app_settings.no_execute || xforce ) )
                {
                    char * argv[ 2 ] = { full_path, NULL };
                    GdkScreen* screen = file_browser ? gtk_widget_get_screen(
                                                GTK_WIDGET(file_browser) ) :
                                                 gdk_screen_get_default();
                    err = NULL;
                    if ( ! vfs_exec_on_screen ( screen, cwd, argv, NULL,
                                                vfs_file_info_get_disp_name( file ),
                                                VFS_EXEC_DEFAULT_FLAGS, 
                                                TRUE, &err ) )
                    {
                        toplevel = file_browser ? gtk_widget_get_toplevel(
                                                GTK_WIDGET( file_browser ) ) :
                                                NULL;
                        ptk_show_error( ( GtkWindow* ) toplevel,
                                        _("Error"),
                                        err->message );
                        g_error_free( err );
                    }
                    else
                    {
                        if ( G_LIKELY( file_browser ) )
                        {
                            ptk_file_browser_emit_open( file_browser,
                                                        full_path,
                                                        PTK_OPEN_FILE );
                        }
                    }
                    g_free( full_path );
                    continue;
                }

                /* Find app to open this file and place copy in alloc_desktop.
                 * This string is freed when hash table is destroyed. */
                alloc_desktop = NULL;

                mime_type = vfs_file_info_get_mime_type( file );
                
                // has archive handler?
                if ( l == sel_files /* test first file only */ &&
                            open_archives_with_handler( parent, sel_files,
                                                        full_path, mime_type ) )
                {
                    // all files were handled by open_archives_with_handler
                    vfs_mime_type_unref( mime_type );
                    break;
                }
                
                // if has file handler, set alloc_desktop = ###XSETNAME
                GSList* handlers_slist = ptk_handler_file_has_handlers(
                                    HANDLER_MODE_FILE, HANDLER_MOUNT,
                                    full_path, mime_type, TRUE, FALSE, TRUE );
                if ( handlers_slist )
                {
                    XSet* handler_set = (XSet*)handlers_slist->data;
                    g_slist_free( handlers_slist );
                    alloc_desktop = g_strconcat( "###", handler_set->name, NULL );
                }
                
                /* The file itself is a desktop entry file. */
                /* was: if( g_str_has_suffix( vfs_file_info_get_name( file ), ".desktop" ) ) */
                if ( !alloc_desktop )
                {
                    if ( file->flags & VFS_FILE_INFO_DESKTOP_ENTRY &&
                                    ( ! app_settings.no_execute || xforce ) )
                        alloc_desktop = full_path;
                    else
                        alloc_desktop = vfs_mime_type_get_default_action(
                                                            mime_type );
                }

                if ( !alloc_desktop && mime_type_is_text_file( full_path,
                                                            mime_type->type ) )
                {
                    /* FIXME: special handling for plain text file */
                    vfs_mime_type_unref( mime_type );
                    mime_type = vfs_mime_type_get_from_type( XDG_MIME_TYPE_PLAIN_TEXT );
                    alloc_desktop = vfs_mime_type_get_default_action( mime_type );
                }
                
                vfs_mime_type_unref( mime_type );

                if ( !alloc_desktop && vfs_file_info_is_symlink( file ) )
                {
                    // broken link?
                    char* target_path = g_file_read_link( full_path, NULL );
                    if ( target_path )
                    {
                        if ( !g_file_test( target_path, G_FILE_TEST_EXISTS ) )
                        {
                            char* msg = g_strdup_printf( _("This symlink's target is missing or you do not have permission to access it:\n%s\n\nTarget: %s"),
                                                    full_path, target_path );
                            toplevel = file_browser ? gtk_widget_get_toplevel(
                                                 GTK_WIDGET( file_browser ) ) :
                                                 NULL;
                            ptk_show_error( ( GtkWindow* ) toplevel,
                                            _("Broken Link"), msg );
                            g_free(msg);
                            g_free( full_path );
                            g_free( target_path );
                            continue;
                        }
                        g_free( target_path );
                    }
                }
                if ( !alloc_desktop )
                {
                    /* Let the user choose an application */
                    toplevel = file_browser ? gtk_widget_get_toplevel( 
                                        GTK_WIDGET( file_browser ) ) : NULL;
                    alloc_desktop = ptk_choose_app_for_mime_type(
                                      ( GtkWindow* ) toplevel,
                                      mime_type, TRUE, TRUE, TRUE, !file_browser );
                }
                if ( !alloc_desktop )
                {
                    g_free( full_path );
                    continue;
                }

                // add full_path to list, update hash table
                files_to_open = NULL;
                if ( !file_list_hash )
                    /* this will free the keys (alloc_desktop) when hash table
                     * destroyed or new key inserted/replaced */
                    file_list_hash = g_hash_table_new_full( g_str_hash,
                                                g_str_equal, g_free, NULL );
                else
                    // get existing file list for this app
                    files_to_open = g_hash_table_lookup( file_list_hash,
                                                        alloc_desktop );

                if ( alloc_desktop != full_path )
                    /* it's not a desktop file itself - add file to list.
                     * Otherwise use full_path as hash table key, which will
                     * be freed when hash table is destroyed. */
                    files_to_open = g_list_append( files_to_open, full_path );
                // update file list in hash table
                g_hash_table_replace( file_list_hash, alloc_desktop,
                                                            files_to_open );
            }
        }
    }

    if ( app_desktop && files_to_open )
    {
        // specified app to open all files
        open_files_with_app( parent, files_to_open, app_desktop );
        g_list_foreach( files_to_open, ( GFunc ) g_free, NULL );
        g_list_free( files_to_open );
    }
    else if ( file_list_hash )
    {
        // No app specified - Use default app to open each associated list of files
        // free_file_list_hash frees each file list and its strings
        g_hash_table_foreach( file_list_hash,
                              open_files_with_each_app, parent );
        g_hash_table_foreach( file_list_hash,
                              free_file_list_hash, NULL );
        g_hash_table_destroy( file_list_hash );
    }

    if ( new_dir )
    {
        if ( G_LIKELY( file_browser ) )
            ptk_file_browser_emit_open( file_browser,
                                        full_path, PTK_OPEN_DIR );
        g_free( new_dir );
    }
    g_slice_free( ParentInfo, parent );
}

void ptk_file_misc_paste_as( DesktopWindow* desktop, PtkFileBrowser* file_browser,
                                            const char* cwd, GFunc callback )
{
    gchar* file_path;
    char* str;
    gboolean is_cut = FALSE;
    gint missing_targets;
    VFSFileInfo* file;
    char* file_dir;
        
    GList* files = ptk_clipboard_get_file_paths( cwd, &is_cut, &missing_targets );
    
    GList* l;
    
    for ( l = files; l; l = l->next )
    {
        file_path = (char*)l->data;
        file = vfs_file_info_new();
        vfs_file_info_get( file, file_path, NULL );
        file_dir = g_path_get_dirname( file_path );
        if ( !ptk_rename_file( desktop, file_browser, file_dir, file, cwd, !is_cut,
                                                        PTK_RENAME, NULL ) )
        {
            vfs_file_info_unref( file );
            g_free( file_dir );
            missing_targets = 0;
            break;
        }
        vfs_file_info_unref( file );
        g_free( file_dir );
    }
    g_list_foreach( files, ( GFunc ) g_free, NULL );
    g_list_free( files );

    if ( callback && desktop )
        callback( NULL, desktop );

    if ( missing_targets > 0 )
    {
        GtkWindow* parent;
        if ( file_browser )
            parent = GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET(
                                                        file_browser ) ) );
        else if ( desktop )
            parent = GTK_WINDOW( desktop );
        ptk_show_error( parent,
                        g_strdup_printf ( _("Error") ),
                        g_strdup_printf ( "%i target%s missing",
                        missing_targets, 
                        missing_targets > 1 ? g_strdup_printf ( "s are" ) : 
                        g_strdup_printf ( " is" ) ) );
    }
}

void ptk_file_misc_rootcmd( DesktopWindow* desktop, PtkFileBrowser* file_browser,
                                                GList* sel_files,
                                                char* cwd, char* setname )
{
    /*
     * root_copy_loc    copy to location
     * root_move2       move to
     * root_delete      delete
     */
    if ( !setname || ( !file_browser && !desktop ) || !sel_files )
        return;
    XSet* set;
    char* path;
    char* cmd;
    char* task_name;
    
    GtkWidget* parent = file_browser ? GTK_WIDGET( file_browser ) :
                                       GTK_WIDGET( desktop );
    char* file_paths = g_strdup( "" );
    GList* sel;
    char* file_path;
    char* file_path_q;
    char* str;
    int item_count = 0;
    for ( sel = sel_files; sel; sel = sel->next )
    {
        file_path = g_build_filename( cwd,
                    vfs_file_info_get_name( ( VFSFileInfo* ) sel->data ), NULL );
        file_path_q = bash_quote( file_path );
        str = file_paths;
        file_paths = g_strdup_printf( "%s %s", file_paths, file_path_q );
        g_free( str );
        g_free( file_path );
        g_free( file_path_q );
        item_count++;
    }
    
    if ( !strcmp( setname, "root_delete" ) )
    {
        if ( !app_settings.no_confirm )
        {
            str = g_strdup_printf( ngettext( "Delete %d selected item as root ?",
                                             "Delete %d selected items as root ?",
                                             item_count ), item_count );
            if ( xset_msg_dialog( parent, GTK_MESSAGE_WARNING,
                                _("Confirm Delete As Root"), NULL, GTK_BUTTONS_YES_NO,
                                _("DELETE AS ROOT"), str, NULL ) != GTK_RESPONSE_YES )
            {
                g_free( str );
                return;
            }
            g_free( str );
        }
        cmd = g_strdup_printf( "rm -r %s", file_paths );
        task_name = g_strdup( _("Delete As Root") );
    }
    else
    {
        char* folder;
        set = xset_get( setname );
        if ( set->desc )
            folder = set->desc;
        else
            folder = cwd;
        path = xset_file_dialog( parent, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                                _("Choose Location"), folder, NULL );
        if ( path && g_file_test( path, G_FILE_TEST_IS_DIR ) )
        {
            xset_set_set( set, "desc", path );
            char* quote_path = bash_quote( path );
            
            if ( !strcmp( setname, "root_move2" ) )
            {
                task_name = g_strdup( _("Move As Root") );
                // problem: no warning if already exists
                cmd = g_strdup_printf( "mv -f %s %s", file_paths, quote_path );
            }
            else
            {
                task_name = g_strdup( _("Copy As Root") );
                // problem: no warning if already exists
                cmd = g_strdup_printf( "cp -r %s %s", file_paths, quote_path );
            }
            
            g_free( quote_path );
            g_free( path );
        }
        else
            return;
    }
    g_free( file_paths );

    // root task
    PtkFileTask* task = ptk_file_exec_new( task_name, cwd, parent,
                                file_browser ? file_browser->task_view : NULL );
    g_free( task_name );
    task->task->exec_command = cmd;
    task->task->exec_sync = TRUE;
    task->task->exec_popup = FALSE;
    task->task->exec_show_output = FALSE;
    task->task->exec_show_error = TRUE;
    task->task->exec_export = FALSE;
    task->task->exec_as_user = g_strdup( "root" );
    ptk_file_task_run( task );          
}
