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
    //char **argv, **cmdv;
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
                        FALSE, FALSE, 2 );

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
                        gtk_vseparator_new(),
                        FALSE, FALSE, 2 );
    gtk_box_pack_start( GTK_BOX(hbox),
                        gtk_label_new( _("Options:") ),
                        FALSE, FALSE, 2 );
    GtkEntry* entry = ( GtkEntry* ) gtk_entry_new();
    if ( xset_get_s( handlers[i].name ) )
        gtk_entry_set_text( entry, xset_get_s( handlers[i].name ) );
    //gtk_entry_set_width_chars( entry, 30 );
    GtkWidget* align = gtk_alignment_new( 0, 0, .5, 1 );
    gtk_alignment_set_padding( GTK_ALIGNMENT( align ), 0, 0, 0, 0 );
    gtk_container_add ( GTK_CONTAINER ( align ), GTK_WIDGET( entry ) );
    gtk_box_pack_start( GTK_BOX(hbox), align, TRUE, TRUE, 4 );
    g_object_set_data( G_OBJECT( dlg ), "entry", entry );

    gtk_widget_show_all( hbox );
    gtk_file_chooser_set_extra_widget( GTK_FILE_CHOOSER(dlg), hbox );

    gtk_file_chooser_set_action( GTK_FILE_CHOOSER(dlg), GTK_FILE_CHOOSER_ACTION_SAVE );
    gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER(dlg), TRUE );

    if( files )
    {
//        gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER(dlg),
//                    vfs_file_info_get_disp_name( (VFSFileInfo*)files->data ) );


        ext = gtk_combo_box_text_get_active_text( GTK_COMBO_BOX_TEXT(combo) );
        dest_file = g_strjoin( NULL, 
                        vfs_file_info_get_disp_name( (VFSFileInfo*)files->data ),
                        ext, NULL );
        gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER(dlg), dest_file );
        g_free( dest_file );

/*
        if ( !files->next )
        {
            dest_file = g_build_filename( cwd,
                    vfs_file_info_get_disp_name( (VFSFileInfo*)files->data ), NULL );
            if ( g_file_test( dest_file, G_FILE_TEST_IS_DIR ) )
            {
                g_free( dest_file );
                dest_file = g_strjoin( NULL,
                                vfs_file_info_get_disp_name( (VFSFileInfo*)files->data ),
                                ext, NULL );
            }
            else
            {
                g_free( dest_file );
                dest_file = g_strdup( vfs_file_info_get_disp_name(
                                                    (VFSFileInfo*)files->data ) );
            }
            gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER(dlg), dest_file );
            g_free( dest_file );
        }
        else
            gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER(dlg), "new archive" );
*/
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
/*
    if ( !g_str_has_suffix( udest_file, handlers[format].file_ext ) )
    {
        g_free( dest_file );
        dest_file = udest_file;
        udest_file = g_strdup_printf( "%s%s", dest_file, handlers[format].file_ext );
    }
*/
    g_free( dest_file );
    
    // overwrite?
/*
    if ( g_file_test( udest_file, G_FILE_TEST_EXISTS ) )
    {
        char* afile = g_path_get_basename( udest_file );
        char* msg = g_strdup_printf( _("Archive '%s' exists.\n\nOverwrite?"), afile );
        g_free( afile );
        if ( xset_msg_dialog( GTK_WIDGET( file_browser ), GTK_MESSAGE_QUESTION,
                                        _("Overwrite?"), NULL, GTK_BUTTONS_OK_CANCEL,
                                        msg, NULL, NULL ) != GTK_RESPONSE_OK )
        {
            g_free( udest_file );
            g_free( s1 );
            g_free( msg );
            return;
        }
        g_free( msg );
    }
*/    
    char* udest_quote = bash_quote( udest_file );
    //char* cmd = g_strdup_printf( "%s %s \"${fm_filenames[@]}\"", s1, udest_quote );
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
        cmd = g_strdup_printf( "%s ; fm_err=$?; if [ $fm_err -ne 0 ]; then echo; echo -n '%s: '; read s; exit $fm_err; fi", s1, _("[ Finished With Errors ]  Press Enter to close") );
        g_free( s1 );
    }
    else
    {
        task->task->exec_sync = TRUE;
    }
    task->task->exec_command = cmd;
    task->task->exec_show_error = TRUE;
    task->task->exec_export = TRUE;
    //task->task->exec_keep_tmp = TRUE;
    XSet* set = xset_get( "new_archive" );
    if ( set->icon )
        task->task->exec_icon = g_strdup( set->icon );
    ptk_file_task_run( task );

/*    
    g_shell_parse_argv( handlers[format].compress_cmd,
                        &cmdc, &cmdv, NULL );

    n = g_list_length( files );
    argc = cmdc + n + 1;
    argv = g_new0( char*, argc + 1 );

    for( i = 0; i < cmdc; ++i )
        argv[i] = cmdv[i];

    argv[i] = dest_file;
    ++i;

    for( l = files; l; l = l->next )
    {
        // FIXME: Maybe we should consider filename encoding here.
        argv[i] = (char *) vfs_file_info_get_name( (VFSFileInfo*) l->data );
        ++i;
    }
    argv[i] = NULL;

    udest_file = g_filename_display_name( dest_file );
    desc = g_strdup_printf( _("Creating Compressed File: %s"), udest_file );
    g_free( udest_file );

    ptk_console_output_run( parent_win, _("Compress Files"),
                            desc,
                            working_dir,
                            argc, argv );
    g_free( dest_file );
    g_strfreev( cmdv );
    g_free( argv );
*/
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
                    perm = g_strdup_printf( "" );
                g_free( parent_path );
                g_free( parent_quote );
            }
            else
            {
                // no create parent
                mkparent = g_strdup_printf( "" );
                if ( write_access && geteuid() != 0 && strcmp( type, "application/x-gzip" ) )
                    perm = g_strdup_printf( " && chmod -R u+rwX %s/*", dest_quote );
                else
                    perm = g_strdup_printf( "" );
            }
            
            if ( i == 3 || i == 4 || i == 6 )
            {
                // zip 7z rar in terminal for password & output
                in_term = TRUE;  // run in terminal
                keep_term = FALSE;
                prompt = g_strdup_printf( " ; fm_err=$?; if [ $fm_err -ne 0 ]; then echo; echo -n '%s: '; read s; exit $fm_err; fi", _("[ Finished With Errors ]  Press Enter to close") );
            }
            else
                prompt = g_strdup_printf( "" );
            
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
    //task->task->exec_keep_tmp = TRUE;
        XSet* set = xset_get( "arc_extract" );
        if ( set->icon )
            task->task->exec_icon = g_strdup( set->icon );
        ptk_file_task_run( task );
    }
    if ( choose_dir )
        g_free( choose_dir );
    

/*
    file = (VFSFileInfo*)files->data;
    mime = vfs_file_info_get_mime_type( file );
    type = vfs_mime_type_get_type( mime );

    for( i = 0; i < G_N_ELEMENTS(handlers); ++i )
    {
        if( 0 == strcmp( type, handlers[i].mime_type ) )
            break;
    }

    if( i < G_N_ELEMENTS(handlers) )    // handler found
    {
        g_shell_parse_argv( handlers[i].extract_cmd,
                            &cmdc, &cmdv, NULL );

        n = g_list_length( files );
        argc = cmdc + n;
        argv = g_new0( char*, argc + 1 );

        for( i = 0; i < cmdc; ++i )
            argv[i] = cmdv[i];

        for( l = files; l; l = l->next )
        {
            file = (VFSFileInfo*)l->data;
            full_path = g_build_filename( working_dir,
                                          vfs_file_info_get_name( file ),
                                          NULL );
            // FIXME: Maybe we should consider filename encoding here.
            argv[i] = full_path;
            ++i;
        }
        argv[i] = NULL;
        argc = i;

        udest_dir = g_filename_display_name( dest_dir );
        desc = g_strdup_printf( _("Extracting Files to: %s"), udest_dir );
        g_free( udest_dir );
        ptk_console_output_run( parent_win, _("Extract Files"),
                                desc,
                                dest_dir,
                                argc, argv );
        g_strfreev( cmdv );
        for( i = cmdc; i < argc; ++i )
            g_free( argv[i] );
        g_free( argv );
    }

    g_free( _dest_dir );
*/
}

gboolean ptk_file_archiver_is_format_supported( VFSMimeType* mime,
                                                gboolean extract )
{
    int i;
    const char* type;

    if( !mime ) return FALSE;
    type = vfs_mime_type_get_type( mime );
    if(! type ) return FALSE;

    /* alias = mime_type_get_alias( type ); */

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
{   // creates a new xset for a custom archive type
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

    xset_msg_dialog( NULL, GTK_MESSAGE_ERROR, _("Configure Unavailable"), NULL,
                        0, "This feature is not yet available", NULL, NULL );
}

