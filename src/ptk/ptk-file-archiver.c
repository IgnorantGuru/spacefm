/*
*  C Implementation: ptk-file-archiver
*
* Description:
*
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include <glib/gi18n.h>
#include <string.h>

#include "ptk-file-archiver.h"
#include "ptk-console-output.h"
#include "ptk-file-task.h"

#include "vfs-file-info.h"
#include "vfs-mime-type.h"
#include "settings.h"
#include "exo-tree-view.h"

#include "gtk2-compat.h"

typedef struct _ArchiveHandler
{
    const char* mime_type;
    const char* compress_cmd;
    const char* extract_cmd;
    const char* list_cmd;
    const char* file_ext;
    const char* name;
    gboolean multiple_files;
}
ArchiveHandler;

const ArchiveHandler handlers[]=
    {
        {
            "application/x-bzip-compressed-tar",
            "tar %o -cvjf",
            "tar -xvjf",
            "tar -tvf",
            ".tar.bz2", "arc_tar_bz2", TRUE
        },
        {
            "application/x-compressed-tar",
            "tar %o -cvzf",
            "tar -xvzf",
            "tar -tvf",
            ".tar.gz", "arc_tar_gz", TRUE
        },
        {
            "application/x-xz-compressed-tar",  //MOD added
            "tar %o -cvJf",
            "tar -xvJf",
            "tar -tvf",
            ".tar.xz", "arc_tar_xz", TRUE
        },
        {
            "application/zip",
            "zip %o -r",
            "unzip",
            "unzip -l",
            ".zip", "arc_zip", TRUE
        },
        {
            "application/x-7z-compressed",
            "7za %o a", // hack - also uses 7zr if available
            "7za x",
            "7za l",
            ".7z", "arc_7z", TRUE
        },
        {
            "application/x-tar",
            "tar %o -cvf",
            "tar -xvf",
            "tar -tvf",
            ".tar", "arc_tar", TRUE
        },
        {
            "application/x-rar",
            "rar a -r %o",
            "unrar -o- x",
            "unrar lt",
            ".rar", "arc_rar", TRUE
        },
        {
            "application/x-gzip",
            NULL,
            "gunzip",
            NULL,
            ".gz", NULL, TRUE
        }
    };


gboolean on_configure_selection_change( GtkTreeSelection* tree_sel,
                                      GtkWidget* dlg )
{
    /* TODO
    enable_context( ctxt );
    return FALSE;*/
}

void on_configure_button_press( GtkWidget* widget, GtkWidget* dlg )
{
    /* TODO
    GtkTreeIter it;
    GtkTreeSelection* tree_sel;
    GtkTreeModel* model;

    if ( widget == GTK_WIDGET( ctxt->btn_add ) || 
                                        widget == GTK_WIDGET( ctxt->btn_apply ) )
    {
        int sub = gtk_combo_box_get_active( GTK_COMBO_BOX( ctxt->box_sub ) );
        int comp = gtk_combo_box_get_active( GTK_COMBO_BOX( ctxt->box_comp ) );
        if ( sub < 0 || comp < 0 )
            return;
        model = gtk_tree_view_get_model( GTK_TREE_VIEW( ctxt->view ) );
        if ( widget == GTK_WIDGET( ctxt->btn_add ) )
            gtk_list_store_append( GTK_LIST_STORE( model ), &it );
        else
        {
            tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( ctxt->view ) );
            if ( !gtk_tree_selection_get_selected( tree_sel, NULL, &it ) )
                return;
        }
        char* value = gtk_combo_box_text_get_active_text( 
                                        GTK_COMBO_BOX_TEXT( ctxt->box_value ) );
        char* disp = context_display( sub, comp, value );
        gtk_list_store_set( GTK_LIST_STORE( model ), &it,
                                    CONTEXT_COL_DISP, disp,
                                    CONTEXT_COL_SUB, sub,
                                    CONTEXT_COL_COMP, comp,
                                    CONTEXT_COL_VALUE, value,
                                    -1 );
        g_free( disp );
        g_free( value );
        gtk_widget_set_sensitive( GTK_WIDGET( ctxt->btn_ok ), TRUE );
        if ( widget == GTK_WIDGET( ctxt->btn_add ) )
            gtk_tree_selection_select_iter( gtk_tree_view_get_selection(
                                        GTK_TREE_VIEW( ctxt->view ) ), &it );
        enable_context( ctxt );
        return;
    }
    
    //remove
    model = gtk_tree_view_get_model( GTK_TREE_VIEW( ctxt->view ) );
    tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( ctxt->view ) );
    if ( gtk_tree_selection_get_selected( tree_sel, NULL, &it ) )
        gtk_list_store_remove( GTK_LIST_STORE( model ), &it );
        
    enable_context( ctxt );*/
}

void on_configure_row_activated( GtkTreeView* view, GtkTreePath* tree_path,
                                        GtkTreeViewColumn* col, GtkWidget* dlg )
{
    /* TODO
    GtkTreeIter it;
    char* value;
    int sub, comp;

    GtkTreeModel* model = gtk_tree_view_get_model( GTK_TREE_VIEW( ctxt->view ) );
    if ( !gtk_tree_model_get_iter( model, &it, tree_path ) )
        return;
    gtk_tree_model_get( model, &it, 
                                    CONTEXT_COL_VALUE, &value,
                                    CONTEXT_COL_SUB, &sub,
                                    CONTEXT_COL_COMP, &comp,
                                    -1 );
    gtk_combo_box_set_active( GTK_COMBO_BOX( ctxt->box_sub ), sub );
    gtk_combo_box_set_active( GTK_COMBO_BOX( ctxt->box_comp ), comp );
    gtk_entry_set_text( GTK_ENTRY( gtk_bin_get_child( GTK_BIN( ctxt->box_value ) ) ), value );
    gtk_widget_grab_focus( ctxt->box_value );
    //enable_context( ctxt );
    */
}

static void on_format_changed( GtkComboBox* combo, gpointer user_data )
{
    GtkFileChooser* dlg = GTK_FILE_CHOOSER(user_data);

    int i, n, len;
    char* ext = NULL;
    char *path, *name, *new_name;

    path = gtk_file_chooser_get_filename( dlg );
    if( !path )
        return;
    ext = gtk_combo_box_text_get_active_text( GTK_COMBO_BOX_TEXT( combo ) );
    name = g_path_get_basename( path );
    g_free( path );
    n = gtk_tree_model_iter_n_children( gtk_combo_box_get_model(combo),
                                        NULL );
    for( i = 0; i < n; ++i )
    {
        if( g_str_has_suffix( name, handlers[i].file_ext ) )
            break;
    }
    if( i < n )
    {
        len = strlen( name ) - strlen( handlers[i].file_ext );
        name[len] = '\0';
    }
    new_name = g_strjoin( NULL, name, ext, NULL );
    g_free( name );
    g_free( ext );
    gtk_file_chooser_set_current_name( dlg, new_name );
    g_free( new_name );

    // set options
    i = gtk_combo_box_get_active(combo);
    GtkEntry* entry = (GtkEntry*)g_object_get_data( G_OBJECT(dlg), "entry" );
    if ( xset_get_s( handlers[i].name ) )
        gtk_entry_set_text( entry, xset_get_s( handlers[i].name ) );
    else
        gtk_entry_buffer_delete_text( gtk_entry_get_buffer( entry ), 0, -1 );
}
                                                        
void ptk_file_archiver_create( PtkFileBrowser* file_browser, GList* files,
                                                                const char* cwd )
{
    GList* l;
    GtkWidget* dlg;
    GtkFileFilter* filter;
    char* dest_file;
    char* ext;
    char* str;
    char* cmd;
    int res;
    int argc, cmdc, i, n;
    int format;
    GtkWidget* combo;
    GtkWidget* hbox;
    char* udest_file;
    char* desc;

    dlg = gtk_file_chooser_dialog_new( _("Save Archive"),
                                       GTK_WINDOW( gtk_widget_get_toplevel(
                                             GTK_WIDGET( file_browser ) ) ),
                                       GTK_FILE_CHOOSER_ACTION_SAVE,
                                       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                       GTK_STOCK_SAVE, GTK_RESPONSE_OK, NULL );
    filter = gtk_file_filter_new();
    hbox = gtk_hbox_new( FALSE, 4 );
    gtk_box_pack_start( GTK_BOX(hbox),
                        gtk_label_new( _("Archive Format:") ),
                        FALSE, TRUE, 2 );

    combo = gtk_combo_box_text_new();

    for( i = 0; i < G_N_ELEMENTS(handlers); ++i )
    {
        if( handlers[i].compress_cmd )
        {
            gtk_file_filter_add_mime_type( filter, handlers[i].mime_type );
            gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(combo), handlers[i].file_ext );
        }
    }
    gtk_file_chooser_set_filter( GTK_FILE_CHOOSER(dlg), filter );
    n = gtk_tree_model_iter_n_children( gtk_combo_box_get_model( 
                                                    GTK_COMBO_BOX( combo ) ), NULL );
    i = xset_get_int( "arc_dlg", "z" );
    if ( i < 0 || i > n - 1 )
        i = 0;
    gtk_combo_box_set_active( GTK_COMBO_BOX(combo), i );
    g_signal_connect( combo, "changed", G_CALLBACK(on_format_changed), dlg );
    gtk_box_pack_start( GTK_BOX(hbox),
                        combo,
                        FALSE, FALSE, 2 );
    gtk_box_pack_start( GTK_BOX(hbox),
                        gtk_label_new( _("Options:") ),
                        FALSE, FALSE, 2 );

    GtkEntry* entry = ( GtkEntry* ) gtk_entry_new();
    if ( xset_get_s( handlers[i].name ) )
        gtk_entry_set_text( entry, xset_get_s( handlers[i].name ) );
    gtk_box_pack_start( GTK_BOX(hbox), GTK_WIDGET( entry ), TRUE, TRUE, 4 );
    g_object_set_data( G_OBJECT( dlg ), "entry", entry );

    gtk_widget_show_all( hbox );
    gtk_box_pack_start( GTK_BOX( gtk_dialog_get_content_area ( 
                                        GTK_DIALOG( dlg ) ) ),
                                        hbox, FALSE, TRUE, 0 );

    gtk_file_chooser_set_action( GTK_FILE_CHOOSER(dlg), GTK_FILE_CHOOSER_ACTION_SAVE );
    gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER(dlg), TRUE );

    if( files )
    {
        ext = gtk_combo_box_text_get_active_text( GTK_COMBO_BOX_TEXT(combo) );
        dest_file = g_strjoin( NULL, 
                        vfs_file_info_get_disp_name( (VFSFileInfo*)files->data ),
                        ext, NULL );
        gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER(dlg), dest_file );
        g_free( dest_file );
        g_free( ext );

    }
    gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER (dlg), cwd );
    
    int width = xset_get_int( "arc_dlg", "x" );
    int height = xset_get_int( "arc_dlg", "y" );
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
    
    res = gtk_dialog_run(GTK_DIALOG(dlg));

    dest_file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dlg));
    format = gtk_combo_box_get_active( GTK_COMBO_BOX(combo) );
    char* options = g_strdup( gtk_entry_get_text( entry ) );
    
    if ( res == GTK_RESPONSE_OK )
    {
        char* str;
        GtkAllocation allocation;
        gtk_widget_get_allocation ( GTK_WIDGET ( dlg ), &allocation );
        width = allocation.width;
        height = allocation.height;
        if ( width && height )
        {
            str = g_strdup_printf( "%d", width );
            xset_set( "arc_dlg", "x", str );
            g_free( str );
            str = g_strdup_printf( "%d", height );
            xset_set( "arc_dlg", "y", str );
            g_free( str );
        }
        str = g_strdup_printf( "%d", format );
        xset_set( "arc_dlg", "z", str );
        g_free( str );
        xset_set( handlers[format].name, "s", gtk_entry_get_text( entry ) );
    }

    gtk_widget_destroy( dlg );

    if ( res != GTK_RESPONSE_OK )
    {
        g_free( dest_file );
        return;
    }

    char* s1;
    if ( options )
    {
        s1 = replace_string( handlers[format].compress_cmd, "%o", options, FALSE );
        g_free( options );
    }
    else
        s1 = g_strdup( handlers[format].compress_cmd );
        
    if ( format == 4 )
    {
        // for 7z use 7za OR 7zr
        str = g_find_program_in_path( "7za" );
        if ( !str )
            str = g_find_program_in_path( "7zr" );
        if ( str )
        {
            cmd = s1;
            s1 = replace_string( cmd, "7za", str, FALSE );
            g_free( cmd );
            g_free( str );
        }
    }

    udest_file = g_filename_display_name( dest_file );
    g_free( dest_file );
    
    char* udest_quote = bash_quote( udest_file );
    cmd = g_strdup_printf( "%s %s", s1, udest_quote );
    g_free( udest_file );
    g_free( udest_quote );
    g_free( s1 );

    // add selected files
    for( l = files; l; l = l->next )
    {
        // FIXME: Maybe we should consider filename encoding here.
        s1 = cmd;
        desc = bash_quote( (char *) vfs_file_info_get_name( (VFSFileInfo*) l->data ) );
        cmd = g_strdup_printf( "%s %s", s1, desc );
        g_free( desc );
        g_free( s1 );
    }
    
    // task
    char* task_name = g_strdup_printf( _("Archive") );
    PtkFileTask* task = ptk_file_exec_new( task_name, cwd, GTK_WIDGET( file_browser ),
                                                        file_browser->task_view );
    g_free( task_name );
    task->task->exec_browser = file_browser;
    if ( format == 3 || format == 4 || format == 6 )
    {
        // use terminal for noisy rar, 7z, zip creation
        task->task->exec_terminal = TRUE;
        task->task->exec_sync = FALSE;
        s1 = cmd;
        cmd = g_strdup_printf( "%s ; fm_err=$?; if [ $fm_err -ne 0 ]; then echo; echo -n '%s: '; read s; exit $fm_err; fi", s1, "[ Finished With Errors ]  Press Enter to close" );
        g_free( s1 );
    }
    else
    {
        task->task->exec_sync = TRUE;
    }
    task->task->exec_command = cmd;
    task->task->exec_show_error = TRUE;
    task->task->exec_export = TRUE;
    XSet* set = xset_get( "new_archive" );
    if ( set->icon )
        task->task->exec_icon = g_strdup( set->icon );
    ptk_file_task_run( task );
}

void ptk_file_archiver_extract( PtkFileBrowser* file_browser, GList* files,
                                            const char* cwd, const char* dest_dir )
{
    GtkWidget* dlg;
    GtkWidget* dlgparent = NULL;
    char* choose_dir = NULL;
    gboolean create_parent;
    gboolean write_access;
    gboolean list_contents = FALSE;
    VFSFileInfo* file;
    VFSMimeType* mime;
    const char* type;
    GList* l;
    char* mkparent;
    char* perm;
    char* prompt;
    char* full_path;
    char* full_quote;
    char* dest_quote;
    const char* dest;
    char* cmd;
    char* str;
    int i, n, j;
    struct stat64 statbuf;
    gboolean keep_term;
    gboolean in_term;    
    const char* suffix[] = { ".tar", ".tar.gz", ".tgz", ".tar.bz2", ".tar.xz",
                                            ".txz", ".zip", ".rar", ".7z" };

    if( !files )
        return;
        
    if ( file_browser )
        dlgparent = gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) );
    //else if ( desktop )
    //    dlgparent = gtk_widget_get_toplevel( desktop );  // causes drag action???
        
    if( !dest_dir )
    {
        dlg = gtk_file_chooser_dialog_new( _("Extract To"),
                                           GTK_WINDOW( dlgparent ),
                                           GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                           GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                           GTK_STOCK_OK, GTK_RESPONSE_OK, NULL );

        GtkWidget* hbox = gtk_hbox_new( FALSE, 10 );
        GtkWidget* chk_parent = gtk_check_button_new_with_mnemonic(
                                                    _("Cre_ate subfolder(s)") );
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chk_parent ),
                                                    xset_get_b( "arc_dlg" ) );
        GtkWidget* chk_write = gtk_check_button_new_with_mnemonic(
                                                    _("Make contents user-_writable") );
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chk_write ),
                                xset_get_int( "arc_dlg", "s" ) == 1 && geteuid() != 0 );
        gtk_widget_set_sensitive( chk_write, geteuid() != 0 );
        gtk_box_pack_start( GTK_BOX(hbox), chk_parent, FALSE, FALSE, 6 );
        gtk_box_pack_start( GTK_BOX(hbox), chk_write, FALSE, FALSE, 6 );
        gtk_widget_show_all( hbox );
        gtk_file_chooser_set_extra_widget( GTK_FILE_CHOOSER(dlg), hbox );

        gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER (dlg), cwd );

        int width = xset_get_int( "arc_dlg", "x" );
        int height = xset_get_int( "arc_dlg", "y" );
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

        if( gtk_dialog_run( GTK_DIALOG(dlg) ) == GTK_RESPONSE_OK )
        {
            GtkAllocation allocation;
            gtk_widget_get_allocation ( GTK_WIDGET ( dlg ), &allocation );
            width = allocation.width;
            height = allocation.height;
            if ( width && height )
            {
                str = g_strdup_printf( "%d", width );
                xset_set( "arc_dlg", "x", str );
                g_free( str );
                str = g_strdup_printf( "%d", height );
                xset_set( "arc_dlg", "y", str );
                g_free( str );
            }
            choose_dir = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(dlg) );
            create_parent = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( chk_parent ) );
            write_access = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( chk_write ) );
            xset_set_b( "arc_dlg", create_parent );
            str = g_strdup_printf( "%d", write_access ? 1 : 0 );
            xset_set( "arc_dlg", "s", str );
            g_free( str );
        }
        gtk_widget_destroy( dlg );
        if( !choose_dir )
            return;
        dest = choose_dir;
    }
    else
    {
        create_parent = xset_get_b( "arc_def_parent" );
        write_access = xset_get_b( "arc_def_write" );
        list_contents = !strcmp( dest_dir, "////LIST" );
        dest = dest_dir;
    }

    for ( l = files; l; l = l->next )
    {
        file = (VFSFileInfo*)l->data;
        mime = vfs_file_info_get_mime_type( file );
        type = vfs_mime_type_get_type( mime );
        for ( i = 0; i < G_N_ELEMENTS(handlers); ++i )
        {
            if( 0 == strcmp( type, handlers[i].mime_type ) )
                break;
        }
        if ( i == G_N_ELEMENTS(handlers) )
            continue;
        if ( ( list_contents && !handlers[i].list_cmd )
                || ( !list_contents && !handlers[i].extract_cmd ) )
            continue;
    
        // handler found
        keep_term = TRUE;
        in_term = FALSE;
        full_path = g_build_filename( cwd, vfs_file_info_get_name( file ), NULL );
        full_quote = bash_quote( full_path );
        dest_quote = bash_quote( dest );
        if ( list_contents )
        {
            // list contents
            cmd = g_strdup_printf( "%s %s | more -d", handlers[i].list_cmd, full_quote );
            in_term = TRUE;
        }
        else if ( !strcmp( type, "application/x-gzip" ) )
        {
            // extract .gz
            char* base = g_build_filename( dest, vfs_file_info_get_name( file ),
                                                                        NULL );
            if ( g_str_has_suffix( base, ".gz" ) )
                base[strlen(base)-3] = '\0';
            char* test_path = g_strdup( base );
            n = 1;
            while ( lstat64( test_path, &statbuf ) == 0 )
            {
                g_free( test_path );
                test_path = g_strdup_printf( "%s-%s%d", base, _("copy"), ++n );
            }
            g_free( dest_quote );
            dest_quote = bash_quote( test_path );
            g_free( test_path );
            g_free( base );
            cmd = g_strdup_printf( "%s -c %s > %s", handlers[i].extract_cmd,
                                                    full_quote, dest_quote );
        }
        else
        {
            // extract
            if ( create_parent && strcmp( type, "application/x-gzip" ) )
            {
                // create parent
                char* full_name = g_path_get_basename( full_path );
                char* parent_name = NULL;
                for ( j = 0; j < G_N_ELEMENTS(suffix); ++j )
                {
                    if ( g_str_has_suffix( full_name, suffix[j] ) )
                    {
                        n = strlen( full_name ) - strlen( suffix[j] );
                        full_name[n] = '\0';
                        parent_name = g_strdup( full_name );
                        full_name[n] = '.';
                        break;
                    }
                }
                if ( !parent_name )
                    parent_name = g_strdup( full_name );
                g_free( full_name );
                
                char* parent_path = g_build_filename( dest, parent_name, NULL );
                char* parent_orig = g_strdup( parent_path );
                n = 1;
                while ( lstat64( parent_path, &statbuf ) == 0 )
                {
                    g_free( parent_path );
                    parent_path = g_strdup_printf( "%s-%s%d", parent_orig,
                                                                _("copy"), ++n );
                }
                g_free( parent_orig );

                char* parent_quote = bash_quote( parent_path );
                mkparent = g_strdup_printf( "mkdir -p %s && cd %s && ", parent_quote,
                                                                    parent_quote );
                if ( write_access && geteuid() != 0 )
                    perm = g_strdup_printf( " && chmod -R u+rwX %s", parent_quote );
                else
                    perm = g_strdup( "" );
                g_free( parent_path );
                g_free( parent_quote );
            }
            else
            {
                // no create parent
                mkparent = g_strdup( "" );
                if ( write_access && geteuid() != 0 && strcmp( type, "application/x-gzip" ) )
                    perm = g_strdup_printf( " && chmod -R u+rwX %s/*", dest_quote );
                else
                    perm = g_strdup( "" );
            }
            
            if ( i == 3 || i == 4 || i == 6 )
            {
                // zip 7z rar in terminal for password & output
                in_term = TRUE;  // run in terminal
                keep_term = FALSE;
                prompt = g_strdup_printf( " ; fm_err=$?; if [ $fm_err -ne 0 ]; then echo; echo -n '%s: '; read s; exit $fm_err; fi", /* no translate for security*/
                            "[ Finished With Errors ]  Press Enter to close" );
            }
            else
                prompt = g_strdup( "" );
            
            char* handler = NULL;
            if ( i == 4 )
            {
                // for 7z use 7za OR 7zr
                str = g_find_program_in_path( "7za" );
                if ( !str )
                    str = g_find_program_in_path( "7zr" );
                if ( str )
                {
                    handler = replace_string( handlers[i].extract_cmd, "7za", str, FALSE );
                    g_free( str );
                }
            }
            cmd = g_strdup_printf( "cd %s && %s%s %s%s%s", dest_quote, mkparent,
                                handler ? handler : handlers[i].extract_cmd,
                                full_quote, prompt, perm );
            g_free( mkparent );
            g_free( perm );
            g_free( prompt );
            g_free( handler );
        }
        g_free( dest_quote );
        g_free( full_quote );
        g_free( full_path );

        // task
        char* task_name = g_strdup_printf( _("Extract %s"),
                                                vfs_file_info_get_name( file ) );
        PtkFileTask* task = ptk_file_exec_new( task_name, cwd, dlgparent,
                                file_browser ? file_browser->task_view : NULL );
        g_free( task_name );
        task->task->exec_command = cmd;
        task->task->exec_browser = file_browser;
        task->task->exec_sync = !in_term;
        task->task->exec_show_error = TRUE;
        task->task->exec_show_output = in_term;
        task->task->exec_terminal = in_term;
        task->task->exec_keep_terminal = keep_term;
        task->task->exec_export = FALSE;
        XSet* set = xset_get( "arc_extract" );
        if ( set->icon )
            task->task->exec_icon = g_strdup( set->icon );
        ptk_file_task_run( task );
    }
    if ( choose_dir )
        g_free( choose_dir );
}

gboolean ptk_file_archiver_is_format_supported( VFSMimeType* mime,
                                                gboolean extract )
{
    int i;
    const char* type;

    if( !mime ) return FALSE;
    type = vfs_mime_type_get_type( mime );
    if(! type ) return FALSE;

    for( i = 0; i < G_N_ELEMENTS(handlers); ++i )
    {
        if( 0 == strcmp( type, handlers[i].mime_type ) )
        {
            if( extract )
            {
                if( handlers[i].extract_cmd )
                    return TRUE;
            }
            else if( handlers[i].compress_cmd )
            {
                return TRUE;
            }
            break;
        }
    }
    return FALSE;
}

XSet* add_new_arctype()
{   
    // creates a new xset for a custom archive type
    XSet* set;
    char* rand;
    char* name = NULL;
    
    // get a unique new xset name
    do
    {
        g_free( name );
        rand = randhex8();
        name = g_strdup_printf( "arccust_%s", rand );
        g_free( rand );
    }
    while ( xset_is( name ) );

    // create and return the xset
    set = xset_get( name );
    g_free( name );
    return set;
}

void ptk_file_archiver_config( PtkFileBrowser* file_browser )
{
    /*
    Archives Types - 1 per xset as:
        set->name       xset name
        set->b          enabled  (XSET_UNSET|XSET_FALSE|XSET_TRUE)
        set->menu_label Display Name
        set->s          Mime Type(s)
        set->x          Extension(s)
        set->y          Compress Command
        set->z          Extract Command
        set->context    List Command

    Configure menu item is used to store some dialog data:
        get this set with:
            set = xset_get( "arc_conf" );
        set->x          dialog width  (string)
        set->y          dialog height (string)
        set->s          space separated list of xset names (archive types)

    Example to add a new custom archive type:
        XSet* newset = add_new_arctype();
        newset->b = XSET_TRUE;                              // enable
        xset_set_set( newset, "label", "Windows CAB" );        // set archive Name
        xset_set_set( newset, "s", "application/winjunk" );    // set Mime Type(s)
        xset_set_set( newset, "x", ".cab" );                   // set Extension(s)
        xset_set_set( newset, "y", "createcab" );              // set Compress cmd
        xset_set_set( newset, "z", "excab" );                  // set Extract cmd
        xset_set_set( newset, "cxt", "listcab" );              // set List cmd
    
    Example to retrieve an xset for an archive type:
        XSet* set = xset_is( "arctype_rar" );
        if ( !set )
            // there is no set named "arctype_rar" (remove it from the list)
        else
        {
            const char* display_name = set->menu_label;
            const char* compress_cmd = set->y;
            gboolean enabled = xset_get_b_set( set );
            // etc
        }
    */

    // TODO: Spaces or tabs?

    // Archive handlers dialog, attaching to top-level window (in GTK,
    // everything is a 'widget') - no buttons etc added as everything is
    // custom...
    GtkWidget* dlg = gtk_dialog_new_with_buttons( _("Archive Handlers"),
                    GTK_WINDOW(
                        gtk_widget_get_toplevel(
                            GTK_WIDGET( file_browser )
                        )
                    ),
                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                    NULL );
    gtk_container_set_border_width( GTK_CONTAINER ( dlg ), 5 );

    // Setting saved dialog size
    int width = xset_get_int( "arc_conf", "x" );
    int height = xset_get_int( "arc_conf", "y" );
    if ( width && height )
        gtk_window_set_default_size( GTK_WINDOW( dlg ), width, height );

    // Adding the help button but preventing it from taking the focus on click
    gtk_button_set_focus_on_click(
                                    GTK_BUTTON(
                                        gtk_dialog_add_button(
                                            GTK_DIALOG( dlg ),
                                            GTK_STOCK_HELP,
                                            GTK_RESPONSE_HELP
                                        )
                                    ),
                                    FALSE );
        
    // Adding standard buttons and saving references in the dialog
    // (GTK doesnt provide a trivial way to reference child widgets from
    // the window!!)
    g_object_set_data( G_OBJECT( dlg ), "btn_cancel",
                        gtk_dialog_add_button( GTK_DIALOG( dlg ),
                                                GTK_STOCK_CANCEL,
                                                GTK_RESPONSE_CANCEL ) );
    g_object_set_data( G_OBJECT( dlg ), "btn_ok",
                        gtk_dialog_add_button( GTK_DIALOG( dlg ),
                                                GTK_STOCK_OK,
                                                GTK_RESPONSE_OK ) );

    // Generating left-hand side of dialog
    GtkWidget* lbl_handlers = gtk_label_new( NULL );
    gtk_label_set_markup( GTK_LABEL( lbl_handlers ), _("<b>Handlers</b>") );
    gtk_misc_set_alignment( GTK_MISC( lbl_handlers ), 0, 0 );

    // Generating the main manager list
    // Creating model - xset name then handler name
    GtkListStore* list = gtk_list_store_new( 2, G_TYPE_STRING, G_TYPE_STRING );
    g_object_set_data( G_OBJECT( dlg ), "list", list );

    // Creating treeview - setting single-click mode (normally this 
    // widget is used for file lists, where double-clicking is the norm
    // for doing an action)
    GtkWidget* view_handlers = exo_tree_view_new();
    gtk_tree_view_set_model( GTK_TREE_VIEW( view_handlers ), GTK_TREE_MODEL( list ) );
    exo_tree_view_set_single_click( (ExoTreeView*)view_handlers, TRUE );
    gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( view_handlers ), FALSE );
    g_object_set_data( G_OBJECT( dlg ), "view_handlers", view_handlers );

    // Turning the treeview into a scrollable widget
    GtkWidget* view_scroll = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( view_scroll ),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER( view_scroll ), view_handlers );
    
    // Connecting treeview callbacks
    g_signal_connect( G_OBJECT( view_handlers ), "row-activated",
                        G_CALLBACK( on_configure_row_activated ), dlg );
    g_signal_connect( G_OBJECT( gtk_tree_view_get_selection( 
                                    GTK_TREE_VIEW( view_handlers ) ) ),
                        "changed",
                        G_CALLBACK( on_configure_selection_change ),
                        dlg );

    // Adding column to the treeview
    GtkTreeViewColumn* col = gtk_tree_view_column_new();
    
    // Change columns to optimal size whenever the model changes
    gtk_tree_view_column_set_sizing( col, GTK_TREE_VIEW_COLUMN_AUTOSIZE );
    
    GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start( col, renderer, TRUE );
    
    // Tie model data to the column
    // TODO: Hook up proper column with the relevant enum
    gtk_tree_view_column_set_attributes( col, renderer,
                                         "text", 0, NULL );
                                         
    gtk_tree_view_append_column ( GTK_TREE_VIEW( view_handlers ), col );
    
    // Set column to take all available space - false by default
    gtk_tree_view_column_set_expand ( col, TRUE );

    // Treeview widgets
    GtkButton* btn_remove = GTK_BUTTON( gtk_button_new_with_mnemonic( _("_Remove") ) );
    gtk_button_set_image( btn_remove, xset_get_image( "GTK_STOCK_REMOVE",
                                                    GTK_ICON_SIZE_BUTTON ) );
    gtk_button_set_focus_on_click( btn_remove, FALSE );
    g_signal_connect( G_OBJECT( btn_remove ), "clicked",
                        G_CALLBACK( on_configure_button_press ), dlg );
    g_object_set_data( G_OBJECT( dlg ), "btn_remove", GTK_BUTTON( btn_remove ) );

    GtkButton* btn_add = GTK_BUTTON( gtk_button_new_with_mnemonic( _("_Add") ) );
    gtk_button_set_image( btn_add, xset_get_image( "GTK_STOCK_ADD",
                                                GTK_ICON_SIZE_BUTTON ) );
    gtk_button_set_focus_on_click( btn_add, FALSE );
    g_signal_connect( G_OBJECT( btn_add ), "clicked",
                        G_CALLBACK( on_configure_button_press ), dlg );
    g_object_set_data( G_OBJECT( dlg ), "btn_add", GTK_BUTTON( btn_add ) );

    GtkButton* btn_apply = GTK_BUTTON( gtk_button_new_with_mnemonic( _("A_pply") ) );
    gtk_button_set_image( btn_apply, xset_get_image( "GTK_STOCK_APPLY",
                                                GTK_ICON_SIZE_BUTTON ) );
    gtk_button_set_focus_on_click( btn_apply, FALSE );
    g_signal_connect( G_OBJECT( btn_apply ), "clicked",
                        G_CALLBACK( on_configure_button_press ), dlg );
    g_object_set_data( G_OBJECT( dlg ), "btn_apply", GTK_BUTTON( btn_apply ) );

    // Generating right-hand side of dialog
    GtkWidget* lbl_settings = gtk_label_new( NULL );
    gtk_label_set_markup( GTK_LABEL( lbl_settings ), _("<b>Handler Settings</b>") );
    gtk_misc_set_alignment( GTK_MISC( lbl_settings ), 0, 0 );
    GtkWidget* chkbtn_handler_enabled = gtk_check_button_new_with_label( _("Enabled") );
    g_object_set_data( G_OBJECT( dlg ), "chkbtn_handler_enabled",
                        GTK_CHECK_BUTTON( chkbtn_handler_enabled ) );
    GtkWidget* lbl_handler_name = gtk_label_new( _("Name:") );
    gtk_misc_set_alignment( GTK_MISC( lbl_handler_name ), 0, 0.5 );
    GtkWidget* lbl_handler_mime = gtk_label_new( _("MIME Type:") );
    gtk_misc_set_alignment( GTK_MISC( lbl_handler_mime ), 0, 0.5 );
    GtkWidget* lbl_handler_extension = gtk_label_new( _("Extension:") );
    gtk_misc_set_alignment( GTK_MISC( lbl_handler_extension ), 0, 0.5 );
    GtkWidget* lbl_commands = gtk_label_new( NULL );
    gtk_label_set_markup( GTK_LABEL( lbl_commands ), _("<b>Commands</b>") );
    gtk_misc_set_alignment( GTK_MISC( lbl_commands ), 0, 0 );
    GtkWidget* lbl_handler_compress = gtk_label_new( _("Compress:") );
    gtk_misc_set_alignment( GTK_MISC( lbl_handler_compress ), 0, 0.5 );
    GtkWidget* lbl_handler_extract = gtk_label_new( _("Extract:") );
    gtk_misc_set_alignment( GTK_MISC( lbl_handler_extract ), 0, 0.5 );
    GtkWidget* lbl_handler_list = gtk_label_new( _("List:") );
    gtk_misc_set_alignment( GTK_MISC( lbl_handler_list ), 0, 0.5 );
    GtkWidget* entry_handler_name = gtk_entry_new();
    g_object_set_data( G_OBJECT( dlg ), "entry_handler_name",
                        GTK_ENTRY( entry_handler_name ) );
    GtkWidget* entry_handler_mime = gtk_entry_new();
    g_object_set_data( G_OBJECT( dlg ), "entry_handler_mime",
                        GTK_ENTRY( entry_handler_mime ) );
    GtkWidget* entry_handler_extension = gtk_entry_new();
    g_object_set_data( G_OBJECT( dlg ), "entry_handler_extension",
                        GTK_ENTRY( entry_handler_extension ) );
    GtkWidget* entry_handler_compress = gtk_entry_new();
    g_object_set_data( G_OBJECT( dlg ), "entry_handler_compress",
                        GTK_ENTRY( entry_handler_compress ) );
    GtkWidget* entry_handler_extract = gtk_entry_new();
    g_object_set_data( G_OBJECT( dlg ), "entry_handler_extract",
                        GTK_ENTRY( entry_handler_extension ) );
    GtkWidget* entry_handler_list = gtk_entry_new();
    g_object_set_data( G_OBJECT( dlg ), "entry_handler_list",
                        GTK_ENTRY( entry_handler_list ) );
    GtkWidget* chkbtn_handler_compress_term = gtk_check_button_new();
    gtk_widget_set_tooltip_text( GTK_WIDGET( chkbtn_handler_compress_term ),
                                    "Run in terminal" );
    GtkWidget* chkbtn_handler_extract_term = gtk_check_button_new();
    gtk_widget_set_tooltip_text( GTK_WIDGET( chkbtn_handler_extract_term ),
                                    "Run in terminal" );
    GtkWidget* chkbtn_handler_list_term = gtk_check_button_new();
    gtk_widget_set_tooltip_text( GTK_WIDGET( chkbtn_handler_list_term ),
                                    "Run in terminal" );

    // Creating container boxes - at this point the dialog already comes
    // with one GtkVBox then inside that a GtkHButtonBox
    // For the right side of the dialog, standard GtkBox approach fails
    // to allow precise padding of labels to allow all entries to line up
    // - so reimplementing with GtkTable. Would many GtkAlignments have
    // worked?
    GtkWidget* hbox_main = gtk_hbox_new( FALSE, 4 );
    GtkWidget* vbox_handlers = gtk_vbox_new( FALSE, 4 );
    GtkWidget* hbox_view_buttons = gtk_hbox_new( FALSE, 4 );
    GtkWidget* tbl_settings = gtk_table_new( 9, 3 , FALSE );
    
    // Packing widgets into boxes
    // Remember, start and end-ness is broken
    // vbox_handlers packing must not expand so that the right side can
    // take the space
    gtk_box_pack_start( GTK_BOX( hbox_main ),
                        GTK_WIDGET( vbox_handlers ), FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox_main ),
                       GTK_WIDGET( tbl_settings ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( vbox_handlers ),
                        GTK_WIDGET( lbl_handlers ), FALSE, FALSE, 4 );

    // view_handlers isn't added but view_scroll is - view_handlers is
    // inside view_scroll. No padding added to get it to align with the
    // enabled widget on the right side
    gtk_box_pack_start( GTK_BOX( vbox_handlers ),
                        GTK_WIDGET( view_scroll ), TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( vbox_handlers ),
                        GTK_WIDGET( hbox_view_buttons ), FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox_view_buttons ),
                        GTK_WIDGET( btn_remove ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox_view_buttons ),
                        GTK_WIDGET( gtk_vseparator_new() ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox_view_buttons ),
                        GTK_WIDGET( btn_add ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox_view_buttons ),
                        GTK_WIDGET( btn_apply ), TRUE, TRUE, 4 );

    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( lbl_settings ), 0, 1, 0, 1,
                        GTK_FILL, GTK_FILL, 0, 4 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( chkbtn_handler_enabled ), 0, 1, 1, 2,
                        GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( lbl_handler_name ), 0, 1, 2, 3,
                        GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( entry_handler_name ), 1, 4, 2, 3,
                        GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( lbl_handler_mime ), 0, 1, 3, 4,
                        GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( entry_handler_mime ), 1, 4, 3, 4,
                        GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( lbl_handler_extension ), 0, 1, 4, 5,
                        GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( entry_handler_extension ), 1, 4, 4, 5,
                        GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0 );

    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( lbl_commands ), 0, 1, 5, 6, GTK_FILL,
                        GTK_FILL, 0, 10 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( lbl_handler_compress ), 0, 1, 6, 7,
                        GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( entry_handler_compress ), 1, 2, 6, 7,
                        GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( chkbtn_handler_compress_term ), 3, 4, 6, 7,
                        GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( lbl_handler_extract ), 0, 1, 7, 8,
                        GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( entry_handler_extract ), 1, 2, 7, 8,
                        GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( chkbtn_handler_extract_term ), 3, 4, 7, 8,
                        GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( lbl_handler_list ), 0, 1, 8, 9,
                        GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( entry_handler_list ), 1, 2, 8, 9,
                        GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( chkbtn_handler_list_term ), 3, 4, 8, 9,
                        GTK_FILL, GTK_FILL, 0, 0 );

    // Packing boxes into dialog with padding to separate from dialog's
    // standard buttons at the bottom
    gtk_box_pack_start( 
				GTK_BOX(
					gtk_dialog_get_content_area( GTK_DIALOG( dlg ) )
				),
				GTK_WIDGET( hbox_main ), TRUE, TRUE, 4 );
	
	// Fetching available archive handlers (literally gets member s from
	// the xset)
	char* archiveHandlers = xset_get_s( "arc_conf" );

	// TODO: Custom handlers are to be added to this - arctype rar and zip already exist??
	// TODO: Commands prepended with + indicate terminal running

	/*
    // plugin?
    XSet* mset = xset_get_plugin_mirror( set );

    // set match / action
    char* elements = mset->context;
    char* action = get_element_next( &elements );
    char* match = get_element_next( &elements );
    if ( match && action )
    {
        i = atoi( match );
        if ( i < 0 || i > 3 )
            i = 0;
        gtk_combo_box_set_active( GTK_COMBO_BOX( ctxt->box_match ), i );
        i = atoi( action );
        if ( i < 0 || i > 3 )
            i = 0;
        gtk_combo_box_set_active( GTK_COMBO_BOX( ctxt->box_action ), i );
        g_free( match );
        g_free( action );
    }
    else
    {
        gtk_combo_box_set_active( GTK_COMBO_BOX( ctxt->box_match ), 0 );
        gtk_combo_box_set_active( GTK_COMBO_BOX( ctxt->box_action ), 0 );        
        if ( match )
            g_free( match );
        if ( action )
            g_free( action );
    }
    // set rules
    int sub, comp;
    char* value;
    char* disp;
    GtkTreeIter it;
    gboolean is_rules = FALSE;
    while ( get_rule_next( &elements, &sub, &comp, &value ) )
    {
        disp = context_display( sub, comp, value );
        gtk_list_store_append( GTK_LIST_STORE( list ), &it );
        gtk_list_store_set( GTK_LIST_STORE( list ), &it,
                                            CONTEXT_COL_DISP, disp,
                                            CONTEXT_COL_SUB, sub,
                                            CONTEXT_COL_COMP, comp,
                                            CONTEXT_COL_VALUE, value,
                                            -1 );
        g_free( disp );
        if ( value )
            g_free( value );
        is_rules = TRUE;
    }
    gtk_combo_box_set_active( GTK_COMBO_BOX( ctxt->box_sub ), 0 );
    gtk_widget_set_sensitive( GTK_WIDGET( ctxt->btn_ok ), is_rules );
    */

    // Rendering dialog - while loop is done to allow user to launch
    // help I think
    gtk_widget_show_all( GTK_WIDGET( dlg ) );
    int response;
    while ( response = gtk_dialog_run( GTK_DIALOG( dlg ) ) )
    {
        if ( response == GTK_RESPONSE_OK )
        {
            break;
        }
        else if ( response == GTK_RESPONSE_HELP )
            // TODO: Sort out proper help
            xset_show_help( dlg, NULL, "#designmode-style-context" );
        else
            break;
    }

    // Fetching dialog dimensions
    GtkAllocation allocation;
    gtk_widget_get_allocation ( GTK_WIDGET( dlg ), &allocation );
    width = allocation.width;
    height = allocation.height;
    
    // Checking if they are valid
    if ( width && height )
    {
        // They are - saving
        char* str = g_strdup_printf( "%d", width );
        xset_set( "arc_conf", "x", str );
        g_free( str );
        str = g_strdup_printf( "%d", height );
        xset_set( "arc_conf", "y", str );
        g_free( str );
    }

    // Clearing up dialog
    // TODO: Does this handle strdup'd strings saved as g_object data?
    gtk_widget_destroy( dlg );

    // Placeholder
    /*xset_msg_dialog( NULL, GTK_MESSAGE_ERROR, _("Configure Unavailable"), NULL,
                        0, "This feature is not yet available", NULL, NULL );
    */
}
