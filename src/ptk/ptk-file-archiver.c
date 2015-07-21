/*
 * SpaceFM ptk-file-archiver.c
 * 
 * Copyright (C) 2015 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2013-2014 OmegaPhil <OmegaPhil@startmail.com>
 * Copyright (C) 2014 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>
 * 
 * License: See COPYING file
 * 
*/


#include <glib/gi18n.h>
#include <string.h>

#include "desktop-window.h"
#include "ptk-file-archiver.h"
#include "ptk-file-task.h"
#include "ptk-handler.h"
#include "vfs-file-info.h"
#include "vfs-mime-type.h"
#include "settings.h"

//#include "gtk2-compat.h"

// Archive handlers treeview model enum
enum {
    COL_XSET_NAME,
    COL_HANDLER_NAME
};

// Archive creation handlers combobox model enum
enum {
    // COL_XSET_NAME
    COL_HANDLER_EXTENSIONS = 1
};

static gchar* archive_handler_get_first_extension( XSet* handler_xset )
{
    // Function deals with the possibility that a handler is responsible
    // for multiple MIME types and therefore file extensions. Functions
    // like archive creation need only one extension
    gchar* first_ext = NULL;
    gchar* name;
    int i;
    if ( handler_xset && handler_xset->x )
    {
        // find first extension
        gchar** pathnames = g_strsplit( handler_xset->x, " ", -1 );
        if ( pathnames )
        {
            for ( i = 0; pathnames[i]; ++i )
            {
                // getting just the extension of the pathname list element
                name = get_name_extension( pathnames[i], FALSE, &first_ext );
                g_free( name );
                if ( first_ext )
                {
                    // add a dot to extension
                    char* str = first_ext;
                    first_ext = g_strconcat( ".", first_ext, NULL );
                    g_free( str );
                    break;
                }
            }
            g_strfreev( pathnames );
        }
    }
    if ( first_ext )
        return first_ext;
    else
        return g_strdup( "" );
}

static gboolean archive_handler_run_in_term( XSet* handler_xset,
                                             int operation )
{
    // Making sure a valid handler_xset has been passed
    if (!handler_xset)
    {
        g_warning("archive_handler_run_in_term has been called with an "
                  "invalid handler_xset!");
        return FALSE;
    }

    int ret;
    if ( operation == ARC_COMPRESS )
        ret = handler_xset->in_terminal;
    else if ( operation == ARC_EXTRACT )
        ret = handler_xset->keep_terminal;
    else if ( operation == ARC_LIST )
        ret = handler_xset->scroll_lock;
    else
    {
        g_warning("archive_handler_run_in_term was passed an invalid"
                    " archive operation ('%d')!", operation);
        return FALSE;
    }
    return ret == XSET_B_TRUE;
}

static void on_format_changed( GtkComboBox* combo, gpointer user_data )
{
    int len = 0;
    char *path, *name, *new_name;

    // Obtaining reference to dialog
    GtkFileChooser* dlg = GTK_FILE_CHOOSER( user_data );

    // Obtaining new archive filename
    path = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER( dlg ) );
    if( !path )
        return;
    name = g_path_get_basename( path );
    g_free( path );

    // Fetching the combo model
    GtkListStore* list = g_object_get_data( G_OBJECT( dlg ),
                                            "combo-model" );

    // Attempting to detect and remove extension from any current archive
    // handler - otherwise cycling through the handlers just appends
    // extensions to the filename
    // Obtaining iterator pointing at first handler
    GtkTreeIter iter;
    if (!gtk_tree_model_get_iter_first( GTK_TREE_MODEL( list ), &iter ))
    {
        // Failed to get iterator - warning user and exiting
        g_warning("Unable to get an iterator to the start of the model "
                  "associated with combobox!");
        return;
    }

    // Loop through available handlers
    XSet* handler_xset;
    gchar* xset_name = NULL, *extension;
    do
    {
        gtk_tree_model_get( GTK_TREE_MODEL( list ), &iter,
                            COL_XSET_NAME, &xset_name,
                            //COL_HANDLER_EXTENSIONS, &extensions,
                            -1 );
        if ( handler_xset = xset_is( xset_name ) )
        {
            // Obtaining archive extension
            extension = archive_handler_get_first_extension(handler_xset);

            // Checking to see if the current archive filename has this
            if (g_str_has_suffix( name, extension ))
            {
                /* It does - recording its length if its the longest match
                 * yet, and continuing */
                if (strlen( extension ) > len)
                    len = strlen( extension );
            }
            g_free(extension);
        }
        g_free( xset_name );
    }
    while(gtk_tree_model_iter_next( GTK_TREE_MODEL( list ), &iter ));

    // Cropping current extension if found
    if (len)
    {
        len = strlen( name ) - len;
        name[len] = '\0';
    }

    // Getting at currently selected archive handler
    if ( gtk_combo_box_get_active_iter( GTK_COMBO_BOX( combo ), &iter ) )
    {
        // You have to fetch both items here
        gtk_tree_model_get( GTK_TREE_MODEL( list ), &iter,
                            COL_XSET_NAME, &xset_name,
                            //COL_HANDLER_EXTENSIONS, &extensions,
                            -1 );
        if ( handler_xset = xset_is( xset_name ) )
        {
            // Obtaining archive extension
            extension = archive_handler_get_first_extension(handler_xset);

            // Appending extension to original filename
            new_name = g_strconcat( name, extension, NULL );

            // Updating new archive filename
            gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER( dlg ), new_name );
            
            g_free( new_name );
            g_free( extension );
        }
        g_free( xset_name );
    }

    g_free( name );

    // Loading command
    if ( handler_xset )
    {
        GtkTextView* view = (GtkTextView*)g_object_get_data( G_OBJECT( dlg ),
                                                                    "view" );
        char* err_msg = ptk_handler_load_script( HANDLER_MODE_ARC,
                        HANDLER_COMPRESS, handler_xset,
                        GTK_TEXT_VIEW( view ), NULL );
        if ( err_msg )
        {
            xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_ERROR,
                                _("Error Loading Handler"), NULL, 0, 
                                err_msg, NULL, NULL );
            g_free( err_msg );
        }
    }
}


static char* generate_bash_error_function( gboolean run_in_terminal,
                                           char* parent_quote )
{
    /* When ran in a terminal, errors need to result in a pause so that
     * the user can review the situation. Even outside a terminal, IG
     * has requested text is output
     * No translation for security purposes */
    char *error_pause = NULL, *finished_with_errors = NULL;
    if (run_in_terminal)
    {
        error_pause = "       read\n";
        finished_with_errors = "[ Finished With Errors ]  Press Enter "
                               "to close: ";
    }
    else
    {
        error_pause = "";
        finished_with_errors = "[ Finished With Errors ]";
    }

    return g_strdup_printf( ""
        "fm_handle_err() {\n"
        "    fm_err=$?\n"
        "%s%s%s"
        "    if [ $fm_err -ne 0 ]; then\n"
        "       echo; echo -n '%s'\n"
        "%s"
        "       exit $fm_err\n"
        "    fi\n"
        "}",
        parent_quote ? "    rmdir --ignore-fail-on-non-empty " : "",
        parent_quote ? parent_quote : "",
        parent_quote ? "\n" : "",
        finished_with_errors, error_pause );
}

char* replace_archive_subs( const char* line, const char* n, const char* N,
                            const char* o, const char* x, const char* g )
{
    char* s;
    char* old_s;
    char* sub;
    char* percent;
    char ch;

    if ( !line )
        return g_strdup( "" );
    
    s = g_strdup( "" );
    char* ptr = (char*) line;
    while ( ptr[0] )
    {
        percent = strchr( ptr, '%' );
        if ( !percent )
        {
            // no more percents - copy end of string
            old_s = s;
            s = g_strdup_printf( "%s%s", s, ptr );
            g_free( old_s );
            break;
        }
        if ( percent[1] == 'n' && n )
            sub = (char*) n;
        else if ( percent[1] == 'N' && N )
            sub = (char*) N;
        else if ( ( percent[1] == 'o' || percent[1] == 'O' ) && o )
            sub = (char*) o;
        else if ( percent[1] == 'x' && x )
            sub = (char*) x;
        else if ( ( percent[1] == 'g' || percent[1] == 'G' ) && g )
            sub = (char*) g;
        else if ( percent[1] == '%' )
        {
            // double percent %% - reduce to single and skip
            percent[1] = '\0';
            old_s = s;
            s = g_strdup_printf( "%s%s", s, ptr );
            g_free( old_s );
            percent[1] = '%';
            ptr = percent + 2;
            continue;
        }
        else
        {
            // not recognized % - copy ptr to percent literally
            ch = percent[1];  // save the character after percent, change to null
            percent[1] = '\0';
            old_s = s;
            s = g_strdup_printf( "%s%s", s, ptr );
            g_free( old_s );
            percent[1] = ch;  // restore character after percent
            ptr = percent + 1;
            continue;
        }
        // copy ptr to percent - 1 and sub
        percent[0] = '\0';   // change % to end of string
        old_s = s;
        s = g_strdup_printf( "%s%s%s", s, ptr, sub );
        g_free( old_s );
        percent[0] = '%';    // restore %
        ptr = percent + 2;
    }
    return s;
}

void ptk_file_archiver_create( DesktopWindow *desktop,
                               PtkFileBrowser *file_browser, GList *files,
                               const char *cwd )
{
    GList *l;
    GtkWidget* combo, *dlg, *hbox;
    GtkFileFilter* filter;

    char *cmd_to_run = NULL, *desc = NULL, *dest_file = NULL,
        *ext = NULL, *s1 = NULL, *str = NULL, *udest_file = NULL,
        *archive_name = NULL, *final_command = NULL;
    int i, n, format, res;

    /* Generating dialog - extra NULL on the NULL-terminated list to
     * placate an irrelevant compilation warning. See notes in
     * ptk-handler.c:ptk_handler_show_config about GTK failure to
     * identify top-level widget */
    GtkWidget *top_level = file_browser ? gtk_widget_get_toplevel(
                                GTK_WIDGET( file_browser->main_window ) ) :
                                NULL;
    dlg = gtk_file_chooser_dialog_new( _("Create Archive"),
                            top_level ? GTK_WINDOW( top_level ) : NULL,
                                       GTK_FILE_CHOOSER_ACTION_SAVE, NULL,
                                       NULL );

    /* Adding standard buttons and saving references in the dialog
     * 'Configure' button has custom text but a stock image */
    GtkButton* btn_configure = GTK_BUTTON( gtk_dialog_add_button(
                                                GTK_DIALOG( dlg ),
                                                _("Conf_igure"),
                                                GTK_RESPONSE_NONE ) );
    GtkWidget* btn_configure_image = xset_get_image( "GTK_STOCK_PREFERENCES",
                                                GTK_ICON_SIZE_BUTTON );
    gtk_button_set_image( GTK_BUTTON( btn_configure ),
                          GTK_WIDGET ( btn_configure_image ) );
    g_object_set_data( G_OBJECT( dlg ), "btn_configure",
                       GTK_BUTTON( btn_configure ) );
    g_object_set_data( G_OBJECT( dlg ), "btn_cancel",
                        gtk_dialog_add_button( GTK_DIALOG( dlg ),
                                                GTK_STOCK_CANCEL,
                                                GTK_RESPONSE_CANCEL ) );
    g_object_set_data( G_OBJECT( dlg ), "btn_ok",
                        gtk_dialog_add_button( GTK_DIALOG( dlg ),
                                                GTK_STOCK_OK,
                                                GTK_RESPONSE_OK ) );

    /* Adding the help button but preventing it from taking the focus on
     * click */
    gtk_button_set_focus_on_click(
                                    GTK_BUTTON(
                                        gtk_dialog_add_button(
                                            GTK_DIALOG( dlg ),
                                            GTK_STOCK_HELP,
                                            GTK_RESPONSE_HELP
                                        )
                                    ),
                                    FALSE );

    filter = gtk_file_filter_new();

    /* Top hbox has 'Command:' label, 'Archive Format:' label then format
     * combobox */
    GtkWidget *hbox_top = gtk_hbox_new( FALSE, 4 );
    GtkWidget *lbl_command = gtk_label_new( NULL );
    gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_command ),
                                        _("Co_mpress Commands:") );
    gtk_box_pack_start( GTK_BOX( hbox_top ), lbl_command, FALSE, TRUE, 2 );

    // Generating a ComboBox with model behind, and saving model for use
    // in callback - now that archive handlers are custom, can't rely on
    // presence or a particular order
    // Model is xset name then extensions the handler deals with
    GtkListStore* list = gtk_list_store_new( 2, G_TYPE_STRING, G_TYPE_STRING );
    combo = gtk_combo_box_new_with_model( GTK_TREE_MODEL( list ) );
    g_object_set_data( G_OBJECT( dlg ), "combo-model", (gpointer)list );

    // Need to manually create the combobox dropdown cells!! Mapping the
    // extensions column from the model to the displayed cell
    GtkCellRenderer* renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( combo ), renderer,
                                TRUE );
    gtk_cell_layout_set_attributes( GTK_CELL_LAYOUT( combo ), renderer,
                                    "text", COL_HANDLER_EXTENSIONS,
                                    NULL );

    // Fetching available archive handlers
    char* archive_handlers_s = xset_get_s( "arc_conf2" );

    // Dealing with possibility of no handlers
    if (g_strcmp0( archive_handlers_s, "" ) <= 0)
    {
        /* Telling user to ensure handlers are available and bringing
         * up configuration */
        xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_ERROR,
                        _("Archive Handlers - Create Archive"), NULL,
                        GTK_BUTTONS_OK, _("No archive handlers "
                        "configured. You must add a handler before "
                        "creating an archive."),
                        NULL, NULL);
        ptk_handler_show_config( HANDLER_MODE_ARC, desktop, file_browser, NULL );
        return;
    }

    // Splitting archive handlers
    gchar** archive_handlers = g_strsplit( archive_handlers_s, " ", -1 );

    // Debug code
    //g_message("archive_handlers_s: %s", archive_handlers_s);

    // Looping for handlers (NULL-terminated list)
    GtkTreeIter iter;
    gchar* xset_name, *extensions;
    XSet* handler_xset;
    // Get xset name of last used handler
    xset_name = xset_get_s( "arc_dlg" );  // do not free
    format = 4;  // default tar.gz
    n = 0;
    for (i = 0; archive_handlers[i] != NULL; ++i)
    {
        // Fetching handler
        handler_xset = xset_is( archive_handlers[i] );

        if ( handler_xset && handler_xset->b == XSET_B_TRUE )
            /* Checking to see if handler is enabled, can cope with
             * compression and the extension is set - dealing with empty
             * command yet 'run in terminal' still ticked
                                   && handler_xset->y
                                   && g_strcmp0( handler_xset->y, "" ) != 0
                                   && g_strcmp0( handler_xset->y, "+" ) != 0
                                   && g_strcmp0( handler_xset->x, "" ) != 0) */
        {
            /* Adding to filter so that only relevant archives
             * are displayed when the user chooses an archive name to
             * create. Note that the handler may be responsible for
             * multiple MIME types and extensions */
            gtk_file_filter_add_mime_type( filter, handler_xset->s );

            // Appending to combobox
            // Obtaining appending iterator for model
            gtk_list_store_append( GTK_LIST_STORE( list ), &iter );

            // Adding to model
            extensions = g_strconcat( handler_xset->menu_label, " (",
                                      handler_xset->x, ")", NULL );
            gtk_list_store_set( GTK_LIST_STORE( list ), &iter,
                                COL_XSET_NAME, archive_handlers[i],
                                COL_HANDLER_EXTENSIONS, extensions,
                                -1 );
            g_free( extensions );
            
            // Is last used handler?
            if ( !g_strcmp0( xset_name, handler_xset->name ) )
                format = n;
            n++;            
        }
    }

    // Clearing up archive_handlers
    g_strfreev( archive_handlers );

    // Applying filter
    gtk_file_chooser_set_filter( GTK_FILE_CHOOSER( dlg ), filter );

    // Restoring previous selected handler
    xset_name = NULL;
    n = gtk_tree_model_iter_n_children( gtk_combo_box_get_model(
                                            GTK_COMBO_BOX( combo )
                                        ), NULL );
    if ( format < 0 || format > n - 1 )
        format = 0;
    gtk_combo_box_set_active( GTK_COMBO_BOX( combo ), format );

    // Adding filter box to hbox and connecting callback
    g_signal_connect( combo, "changed", G_CALLBACK( on_format_changed ),
                      dlg );
    gtk_box_pack_end( GTK_BOX( hbox_top ), combo, FALSE, FALSE, 2 );

    GtkWidget *lbl_archive_format = gtk_label_new( NULL );
    gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_archive_format ),
                                         _("_Archive Format:") );
    gtk_box_pack_end( GTK_BOX( hbox_top ), lbl_archive_format, FALSE, FALSE,
                      2 );
    gtk_widget_show_all( hbox_top );

    GtkTextView* view = (GtkTextView*)gtk_text_view_new();
    gtk_text_view_set_wrap_mode( GTK_TEXT_VIEW( view ), GTK_WRAP_WORD_CHAR );
    GtkWidget* view_scroll = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( view_scroll ),
                                     GTK_POLICY_AUTOMATIC,
                                     GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER( view_scroll ),
                                      GTK_WIDGET ( view ) );
    g_object_set_data( G_OBJECT( dlg ), "view", view );

    /* Loading command for handler, based off the format handler */
    // Obtaining iterator from string turned into a path into the model
    str = g_strdup_printf( "%d", format );
    if ( gtk_tree_model_get_iter_from_string( GTK_TREE_MODEL( list ),
                                            &iter, str ) )
    {
        gtk_tree_model_get( GTK_TREE_MODEL( list ), &iter,
                            COL_XSET_NAME, &xset_name,
                            //COL_HANDLER_EXTENSIONS, &extensions,
                            -1 );
        if ( handler_xset = xset_is( xset_name ) )
        {
            char* err_msg = ptk_handler_load_script( HANDLER_MODE_ARC,
                            HANDLER_COMPRESS, handler_xset,
                            GTK_TEXT_VIEW( view ), NULL );
            if ( err_msg )
            {
                xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_ERROR,
                                    _("Error Loading Handler"), NULL, 0, 
                                    err_msg, NULL, NULL );
                g_free( err_msg );
            }

        }
        g_free( xset_name );
    }
    else
    {
        // Recording the fact getting the iter failed
        g_warning( "Unable to fetch the iter from handler ordinal %d!", format );
    };
    g_free( str );
    
    // Mnemonically attaching widgets to labels
    gtk_label_set_mnemonic_widget( GTK_LABEL( lbl_archive_format ),
                                   combo );
    gtk_label_set_mnemonic_widget( GTK_LABEL( lbl_command ),
                                   GTK_WIDGET( view ) );

    /* Creating hbox for the command textview, on a line under the top
     * hbox */
    GtkWidget *hbox_bottom = gtk_hbox_new( FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox_bottom ), GTK_WIDGET( view_scroll ),
                        TRUE, TRUE, 4 );
    gtk_widget_show_all( hbox_bottom );

    // Packing the two hboxes into a vbox, then adding to dialog at bottom
    GtkWidget *vbox = gtk_vbox_new( FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( hbox_top ), TRUE,
                        TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( hbox_bottom ),
                        TRUE, TRUE, 1 );
    gtk_box_pack_start( GTK_BOX( gtk_dialog_get_content_area (
                                    GTK_DIALOG( dlg )
                                ) ),
                                vbox, FALSE, TRUE, 0 );

    // Configuring dialog
    gtk_file_chooser_set_action( GTK_FILE_CHOOSER( dlg ),
                                 GTK_FILE_CHOOSER_ACTION_SAVE );
    gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER( dlg ),
                                                    TRUE );

    // Populating name of archive and setting the correct directory
    if( files )
    {
        // Fetching first extension handler deals with
        ext = archive_handler_get_first_extension( handler_xset );
        dest_file = g_strjoin( NULL,
                        vfs_file_info_get_disp_name( (VFSFileInfo*)files->data ),
                        ext, NULL );
        gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER( dlg ),
                                           dest_file );
        g_free( dest_file );
        dest_file = NULL;
        g_free( ext );
        ext = NULL;

    }
    gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER( dlg ), cwd );

    // Setting dimension and position
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

    // Displaying dialog
    gchar* command = NULL;
    gboolean run_in_terminal;
    gtk_widget_show_all( dlg );
    
    while( res = gtk_dialog_run( GTK_DIALOG( dlg ) ) )
    {
        if ( res == GTK_RESPONSE_OK )
        {
            // Dialog OK'd - fetching archive filename
            dest_file = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER( dlg ) );

            // Fetching archive handler selected
            if(!gtk_combo_box_get_active_iter( GTK_COMBO_BOX( combo ),
                &iter ))
            {
                // Unable to fetch iter from combo box - warning user and
                // exiting
                g_warning( "Unable to fetch iter from combobox!" );
                g_free( dest_file );
                gtk_widget_destroy( dlg );
                return;
            }

            // Fetching model data
            gtk_tree_model_get( GTK_TREE_MODEL( list ), &iter,
                                COL_XSET_NAME, &xset_name,
                                //COL_HANDLER_EXTENSIONS, &extensions,
                                -1 );

            handler_xset = xset_get( xset_name );
            // Saving selected archive handler name as default
            xset_set( "arc_dlg", "s", xset_name );
            g_free( xset_name );
            
            // run in the terminal or not
            run_in_terminal = handler_xset->in_terminal == XSET_B_TRUE;

            // Get command from text view
            GtkTextBuffer* buf = gtk_text_view_get_buffer( view );
            GtkTextIter iter, siter;
            gtk_text_buffer_get_start_iter( buf, &siter );
            gtk_text_buffer_get_end_iter( buf, &iter );
            command = gtk_text_buffer_get_text( buf, &siter, &iter, FALSE );

            // reject command that contains only whitespace and comments
            if ( ptk_handler_command_is_empty( command ) )
            {
                xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_ERROR,
                                    _("Create Archive"), NULL, 0, 
                                    _("The archive creation command is empty.  Please enter a command."),
                                    NULL, NULL );
                g_free( command );
                continue;
            }

#if 0   // I don't like this warning here - users may have custom ways of doing
            /* This is duplicating GUI validation code but it is just
             * not worth spinning out a series of validation functions
             * for this
             * Checking to see if the archive handler compression command
             * has been deleted or has invalid placeholders - not
             * required to only have one of the particular type */
            if (g_strcmp0( command, "" ) <= 0 ||
                (
                    !g_strstr_len( command, -1, "%o" ) &&
                    !g_strstr_len( command, -1, "%O" )
                )
                ||
                (
                    !g_strstr_len( command, -1, "%n" ) &&
                    !g_strstr_len( command, -1, "%N" )
                )
            )
            {
                // It has/is - warning user
/* this looks like a very tall dialog - will fit on smaller (600) screens? */
                xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_WARNING,
                                _("Create Archive"), NULL, FALSE,
                                _("The following variables "
                                "should be in the archive creation"
                                " command:\n\n"
                                "One of the following:\n\n"
                                "%%%%n: First selected file/directory to"
                                " archive\n"
                                "%%%%N: All selected files/directories to"
                                " archive\n\n"
                                "and one of the following:\n\n"
                                "%%%%o: Resulting single archive\n"
                                "%%%%O: Resulting archive per source "
                                "file/directory (see %%%%n/%%%%N)\n"),
                                NULL, NULL );
                gtk_widget_grab_focus( GTK_WIDGET( view ) );
            }
#endif

            // Getting prior command for comparison
            char* compress_cmd = NULL;
            char* err_msg = ptk_handler_load_script( HANDLER_MODE_ARC,
                            HANDLER_COMPRESS, handler_xset,
                            NULL, &compress_cmd );
            if ( err_msg )
            {
                g_warning( "%s", err_msg );
                g_free( err_msg );
                compress_cmd = g_strdup( "" );
            }
            
            // Checking to see if the compression command has changed
            if ( g_strcmp0( compress_cmd, command ) )
            {
                // command has changed - saving command
                g_free( compress_cmd );
                if ( handler_xset->disable )
                {
                    // commmand was default - need to save all commands
                    // get default extract command from const
                    compress_cmd = ptk_handler_get_command( HANDLER_MODE_ARC,
                                        HANDLER_EXTRACT, handler_xset );
                    // write extract command script
                    err_msg = ptk_handler_save_script( HANDLER_MODE_ARC,
                                                    HANDLER_EXTRACT,
                                                    handler_xset,
                                                    NULL, compress_cmd );
                    if ( err_msg )
                    {
                        g_warning( "%s", err_msg );
                        g_free( err_msg );
                    }
                    g_free( compress_cmd );
                    // get default list command from const
                    compress_cmd = ptk_handler_get_command( HANDLER_MODE_ARC,
                                        HANDLER_LIST, handler_xset );
                    // write list command script
                    err_msg = ptk_handler_save_script( HANDLER_MODE_ARC,
                                                    HANDLER_LIST,
                                                    handler_xset,
                                                    NULL, compress_cmd );
                    if ( err_msg )
                    {
                        g_warning( "%s", err_msg );
                        g_free( err_msg );
                    }
                    g_free( compress_cmd );
                    handler_xset->disable = FALSE;  // not default handler now
                }
                // save updated compress command
                err_msg = ptk_handler_save_script( HANDLER_MODE_ARC,
                                                    HANDLER_COMPRESS,
                                                    handler_xset,
                                                    NULL, command );
                if ( err_msg )
                {
                    xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_ERROR,
                                        _("Error Saving Handler"), NULL, 0, 
                                        err_msg, NULL, NULL );
                    g_free( err_msg );
                }                
            }
            else
                g_free( compress_cmd );
            
            // Saving settings
            xset_autosave( FALSE, FALSE );
            break;
        }
        else if ( res == GTK_RESPONSE_NONE )
        {
            /* User wants to configure archive handlers - call up the
             * config dialog then exit, as this dialog would need to be
             * reconstructed if changes occur */
            gtk_widget_destroy( dlg );
            ptk_handler_show_config( HANDLER_MODE_ARC, desktop, file_browser,
                                                                    NULL );
            return;
        }
        else if ( res == GTK_RESPONSE_HELP )
        {
            xset_show_help( dlg, NULL, "#handlers-arc" );
        }
        else
        {
            // Destroying dialog
            gtk_widget_destroy( dlg );
            return;
        }
    }

    // Saving dialog dimensions
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

    // Destroying dialog
    gtk_widget_destroy( dlg );

    // Make Archive Creation Command

    // Dealing with separate archives for each source file/directory ('%O')
    if (g_strstr_len( command, -1, "%O" ))
    {
        /* '%O' is present - the archiving command should be generated
         * and ran for each individual file */

        // Fetching extension
        ext = archive_handler_get_first_extension( handler_xset );

        /* Looping for all selected files/directories - all are used
         * when '%N' is present, only the first otherwise */
        for( i = 0, l = files;
             l && ( i == 0 || g_strstr_len( command, -1, "%N" ) );
             l = l->next, ++i)
        {
            desc = (char *) vfs_file_info_get_name(
                                            (VFSFileInfo*) l->data );

            /* In %O mode, every source file is output to its own archive,
             * so the resulting archive name is based on the filename and
             * substituted every time */

            // Obtaining archive name, quoting and substituting for %O
            archive_name = g_strconcat( desc, ext, NULL );
            s1 = archive_name;
            archive_name = bash_quote( archive_name );
            g_free(s1);

            /* Bash quoting desc - desc original value comes from the
             * VFSFileInfo struct and therefore should not be freed */
            if ( desc[0] == '-' )
            {
                // special handling for filename starting with a dash
                // due to tar interpreting it as option
                s1 = g_strdup_printf( "./%s", desc );
                desc = bash_quote( s1 );
                g_free( s1 );
            }
            else
                desc = bash_quote( desc );
            
            // Replace sub vars  %n %N %O (and erroneous %o treat as %O)
            cmd_to_run = replace_archive_subs( command,
                            i == 0 ? desc : "",  // first run only %n = desc
                            desc,  // Replace %N with nth file (NOT ALL FILES)
                            archive_name, NULL, NULL );
            g_free( archive_name );
            g_free( desc );
            
            // Appending to final command as appropriate
            if (i == 0)
                final_command = g_strconcat( cmd_to_run,
                                        "\n[[ $? -eq 0 ]] || fm_handle_err\n",
                                        NULL );
            else
            {
                s1 = final_command;
                final_command = g_strconcat( final_command, "echo\n",
                                        cmd_to_run,
                                        "\n[[ $? -eq 0 ]] || fm_handle_err\n",
                                        NULL );
                g_free(s1);
            }
            g_free( cmd_to_run );
        }
    }
    else
    {
        /* '%O' isn't present - the normal single command is needed
         * Obtaining valid quoted UTF8 file name %o for archive to create */
        udest_file = g_filename_display_name( dest_file );
        char* udest_quote = bash_quote( udest_file );
        g_free( udest_file );
        char* all = g_strdup ( "" );
        char* first;
        if ( files )
        {
            desc = (char*) vfs_file_info_get_name( (VFSFileInfo*) files->data );
            if ( desc[0] == '-' )
            {
                // special handling for filename starting with a dash
                // due to tar interpreting it as option
                s1 = g_strdup_printf( "./%s", desc );
                first = bash_quote( s1 );
                g_free( s1 );
            }
            else
                first = bash_quote( desc );
            
            /* Generating string of selected files/directories to archive if
             * '%N' is present */
            if ( g_strstr_len( command, -1, "%N" ) )
            {
                for( l = files; l; l = l->next)
                {
                    desc = (char*) vfs_file_info_get_name( (VFSFileInfo*) l->data );
                    if ( desc[0] == '-' )
                    {
                        // special handling for filename starting with a dash
                        // due to tar interpreting it as option
                        s1 = g_strdup_printf( "./%s", desc );
                        desc = bash_quote( s1 );
                        g_free( s1 );
                    }
                    else
                        desc = bash_quote( desc );

                    str = all;
                    all = g_strdup_printf( "%s%s%s", all,
                                           all[0] ? " " : "", desc );
                    g_free( str );
                    g_free( desc );
                }
            }
        }
        else
        {
            // no files selected!
            first = g_strdup( "" );
        }
        
        // Replace sub vars  %n %N %o
        cmd_to_run = replace_archive_subs( command, first, all, udest_quote,
                                           NULL, NULL );
        
        // Enforce error check
        final_command = g_strconcat( cmd_to_run,
                                     "\n[[ $? -eq 0 ]] || fm_handle_err\n",
                                     NULL );
        g_free( cmd_to_run );
        g_free( udest_quote );
        g_free( first );
        g_free( all );
    }
    g_free( dest_file );

    /* When ran in a terminal, errors need to result in a pause so that
     * the user can review the situation - in any case an error check
     * needs to be made */
    str = generate_bash_error_function( run_in_terminal, NULL );
    s1 = final_command;
    final_command = g_strconcat( str, "\n\n", final_command, NULL );
    g_free( str );
    g_free( s1 );

    /* Cleaning up - final_command does not need freeing, as this
     * is freed by the task */
    g_free( command );

    // Creating task
    char* task_name = g_strdup_printf( _("Archive") );
    PtkFileTask* task = ptk_file_exec_new( task_name, cwd,
                        file_browser ? GTK_WIDGET( file_browser ) : NULL,
                        file_browser ? file_browser->task_view : NULL );
    g_free( task_name );

    /* Setting correct exec reference - probably causes different bash
     * to be output */
    if (file_browser)
        task->task->exec_browser = file_browser;
    else
        task->task->exec_desktop = desktop;

    // Using terminals for certain handlers
    if (run_in_terminal)
    {
        task->task->exec_terminal = TRUE;
        task->task->exec_sync = FALSE;
    }
    else task->task->exec_sync = TRUE;

    // Final configuration, setting custom icon
    task->task->exec_command = final_command;
    task->task->exec_show_error = TRUE;
    task->task->exec_export = TRUE;  // Setup SpaceFM bash variables
    XSet* set = xset_get( "new_archive" );
    if ( set->icon )
        task->task->exec_icon = g_strdup( set->icon );

    // Running task
    ptk_file_task_run( task );
}

static void on_create_subfolder_toggled( GtkToggleButton *togglebutton,
                                         GtkWidget* chk_write )
{
    gboolean enabled = gtk_toggle_button_get_active(
                                        GTK_TOGGLE_BUTTON ( togglebutton ) );
    gtk_widget_set_sensitive( chk_write, enabled && geteuid() != 0 );
}

void ptk_file_archiver_extract( DesktopWindow *desktop,
                                PtkFileBrowser *file_browser,
                                GList *files, const char *cwd,
                                const char *dest_dir, int job )
{   /* This function is also used to list the contents of archives */
    GtkWidget* dlg;
    GtkWidget* dlgparent = NULL;
    char* choose_dir = NULL;
    gboolean create_parent = FALSE, in_term = FALSE, keep_term = FALSE;
    gboolean  write_access = FALSE, list_contents = FALSE;
    char* parent_quote = NULL;
    VFSFileInfo* file;
    VFSMimeType* mime_type;
    const char *dest, *type;
    GList* l;
    char *dest_quote = NULL, *full_path = NULL, *full_quote = NULL,
        *mkparent = NULL, *perm = NULL,
        *cmd = NULL, *str = NULL, *final_command = NULL, *s1 = NULL, *extension;
    int i, n, j, res;
    struct stat64 statbuf;

    // Making sure files to act on have been passed
    if( !files || job == HANDLER_COMPRESS )
        return;

    // Detecting whether this function call is actually to list the
    // contents of the archive or not...
    list_contents = job == HANDLER_LIST;

    // Determining parent of dialog
    if ( file_browser )
        dlgparent = gtk_widget_get_toplevel(
                                    GTK_WIDGET( file_browser->main_window ) );
    //else if ( desktop )
    //    dlgparent = gtk_widget_get_toplevel( desktop );  // causes drag action???

    // Checking if extract to directory hasn't been specified
    if ( !dest_dir && !list_contents )
    {
        /* It hasn't - generating dialog to ask user. Only dealing with
         * user-writable contents if the user isn't root */
        dlg = gtk_file_chooser_dialog_new( _("Extract To"),
                                dlgparent ? GTK_WINDOW( dlgparent ) : NULL,
                                           GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, NULL,
                                           NULL );

        /* Adding standard buttons and saving references in the dialog
         * 'Configure' button has custom text but a stock image */
        GtkButton* btn_configure = GTK_BUTTON( gtk_dialog_add_button(
                                                    GTK_DIALOG( dlg ),
                                                    _("Conf_igure"),
                                                    GTK_RESPONSE_NONE ) );
        GtkWidget* btn_configure_image = xset_get_image( "GTK_STOCK_PREFERENCES",
                                                    GTK_ICON_SIZE_BUTTON );
        gtk_button_set_image( GTK_BUTTON( btn_configure ),
                              GTK_WIDGET ( btn_configure_image ) );
        g_object_set_data( G_OBJECT( dlg ), "btn_configure",
                           GTK_BUTTON( btn_configure ) );
        g_object_set_data( G_OBJECT( dlg ), "btn_cancel",
                            gtk_dialog_add_button( GTK_DIALOG( dlg ),
                                                    GTK_STOCK_CANCEL,
                                                    GTK_RESPONSE_CANCEL ) );
        g_object_set_data( G_OBJECT( dlg ), "btn_ok",
                            gtk_dialog_add_button( GTK_DIALOG( dlg ),
                                                    GTK_STOCK_OK,
                                                    GTK_RESPONSE_OK ) );

        /* Adding the help button but preventing it from taking the focus on
         * click */
        gtk_button_set_focus_on_click(
                                        GTK_BUTTON(
                                            gtk_dialog_add_button(
                                                GTK_DIALOG( dlg ),
                                                GTK_STOCK_HELP,
                                                GTK_RESPONSE_HELP
                                            )
                                        ),
                                        FALSE );

        GtkWidget* hbox = gtk_hbox_new( FALSE, 10 );
        GtkWidget* chk_parent = gtk_check_button_new_with_mnemonic(
                                            _("Cre_ate subfolder(s)") );
        GtkWidget* chk_write = gtk_check_button_new_with_mnemonic(
                                                    _("Make contents "
                                                    "user-_writable") );
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chk_parent ),
                                              xset_get_b( "arc_dlg" ) );
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chk_write ),
                                xset_get_int( "arc_dlg", "z" ) == 1 &&
                                geteuid() != 0 );
        gtk_widget_set_sensitive( chk_write, xset_get_b( "arc_dlg" ) &&
                                             geteuid() != 0 );
        g_signal_connect( G_OBJECT( chk_parent ), "toggled",
                    G_CALLBACK ( on_create_subfolder_toggled ), chk_write );
        gtk_box_pack_start( GTK_BOX(hbox), chk_parent, FALSE, FALSE, 6 );
        gtk_box_pack_start( GTK_BOX(hbox), chk_write, FALSE, FALSE, 6 );
        gtk_widget_show_all( hbox );
        gtk_file_chooser_set_extra_widget( GTK_FILE_CHOOSER(dlg), hbox );

        // Setting dialog to current working directory
        gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER (dlg), cwd );

        // Fetching saved dialog dimensions and applying
        int width = xset_get_int( "arc_dlg", "x" );
        int height = xset_get_int( "arc_dlg", "y" );
        if ( width && height )
        {
            // filechooser won't honor default size or size request ?
            gtk_widget_show_all( dlg );
            gtk_window_set_position( GTK_WINDOW( dlg ),
                                     GTK_WIN_POS_CENTER_ALWAYS );
            gtk_window_resize( GTK_WINDOW( dlg ), width, height );
            while( gtk_events_pending() )
                gtk_main_iteration();
            gtk_window_set_position( GTK_WINDOW( dlg ),
                                     GTK_WIN_POS_CENTER );
        }

        // Displaying dialog
        while( res = gtk_dialog_run( GTK_DIALOG( dlg ) ) )
        {
            if ( res == GTK_RESPONSE_OK )
            {
                // Fetching user-specified settings and saving
                choose_dir = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER( dlg ) );
                create_parent = gtk_toggle_button_get_active(
                                            GTK_TOGGLE_BUTTON( chk_parent ) );
                write_access = create_parent && 
                    gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( chk_write ) );
                xset_set_b( "arc_dlg", create_parent );
                xset_set( "arc_dlg", "z", write_access ? "1" : "0" );
                break;
            }
            else if ( res == GTK_RESPONSE_NONE )
            {
                /* User wants to configure archive handlers - call up the
                 * config dialog then exit, as this dialog would need to be
                 * reconstructed if changes occur */
                gtk_widget_destroy( dlg );
                ptk_handler_show_config( HANDLER_MODE_ARC, desktop,
                                                        file_browser, NULL );
                return;
            }
            else if ( res == GTK_RESPONSE_HELP )
            {
                xset_show_help( dlg, NULL, "#handlers-arc" );
            }
            else
            {
                // Destroying dialog
                gtk_widget_destroy( dlg );
                return;
            }
        }

        // Saving dialog dimensions
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

        // Destroying dialog
        gtk_widget_destroy( dlg );

        // Exiting if user didnt choose an extraction directory
        if( !choose_dir )
            return;
        dest = choose_dir;
    }
    else
    {
        // Extraction directory specified - loading defaults
        create_parent = xset_get_b( "arc_def_parent" );
        write_access = create_parent && xset_get_b( "arc_def_write" );

        dest = dest_dir;
    }

    /* Quoting destination directory (doing this outside of the later
     * loop as its needed after the selected files loop completes) */
    dest_quote = bash_quote( dest ? dest : cwd );

    // Fetching available archive handlers and splitting
    char* archive_handlers_s = xset_get_s( "arc_conf2" );
    gchar** archive_handlers = archive_handlers_s ?
                               g_strsplit( archive_handlers_s, " ", -1 ) :
                               NULL;
    XSet* handler_xset = NULL;

    /* Setting desired archive operation and keeping in terminal while
     * listing */
    int archive_operation = list_contents ? ARC_LIST : ARC_EXTRACT;
    keep_term = list_contents;

    // Looping for all files to attempt to list/extract
    for ( l = files; l; l = l->next )
    {

        // Fetching file details
        file = (VFSFileInfo*)l->data;
        mime_type = vfs_file_info_get_mime_type( file );
        // Determining file paths
        full_path = g_build_filename( cwd, vfs_file_info_get_name( file ),
                                      NULL );

        // Get handler with non-empty command
        GSList* handlers_slist = ptk_handler_file_has_handlers(
                                        HANDLER_MODE_ARC, archive_operation,
                                        full_path, mime_type, TRUE, FALSE, TRUE );
        if ( handlers_slist )
        {
            handler_xset = (XSet*)handlers_slist->data;
            g_slist_free( handlers_slist );
        }
        else
            handler_xset = NULL;
        vfs_mime_type_unref( mime_type );

        // Continuing to next file if a handler hasnt been found
        if ( !handler_xset )
        {
            g_warning( "%s %s", _("No archive handler/command found for file:"),
                                                            full_path );
            g_free( full_path );
            continue;
        }
        printf( "Archive Handler Selected: %s\n", handler_xset->menu_label );
        
        /* Handler found - fetching the 'run in terminal' preference, if
         * the operation is listing then the terminal should be kept
         * open, otherwise the user should explicitly keep the terminal
         * running via the handler's command
         * Since multiple commands are now batched together, only one
         * of the handlers needing to run in a terminal will cause all of
         * them to */
        if (!in_term)
            in_term = archive_handler_run_in_term( handler_xset,
                                                   archive_operation );

        // Archive to list or extract:
        full_quote = bash_quote( full_path );   // %x
        gchar* extract_target = NULL;           // %g or %G
        gchar* mkparent = g_strdup( "" );
        perm = g_strdup( "" );

        if ( list_contents )
        {
            // List archive contents only
            char* err_msg = ptk_handler_load_script( HANDLER_MODE_ARC,
                                            HANDLER_LIST, handler_xset,
                                            NULL, &cmd );
            if ( err_msg )
            {
                g_warning( err_msg, NULL );
                g_free( err_msg );
                cmd = g_strdup( "" );
            }
        }
        else
        {
            /* An archive is to be extracted
             * Obtaining filename minus the archive extension - this is
             * needed if a parent directory must be created, and if the
             * extraction target is a file without the handler extension
             * filename is g_strdup'd to get rid of the const */
            gchar* filename = g_strdup( vfs_file_info_get_name( file ) );
            gchar* filename_no_archive_ext = NULL;

            /* Looping for all extensions registered with the current
             * archive handler (NULL-terminated list) */
            gchar** pathnames = handler_xset->x ?
                           g_strsplit( handler_xset->x, " ", -1 ) :
                           NULL;
            gchar *filename_no_ext;
            if ( pathnames )
            {
                for (i = 0; pathnames[i]; ++i)
                {
                    // getting just the extension of the pathname list element
                    filename_no_ext = get_name_extension( pathnames[i],
                                                         FALSE,
                                                         &extension );
                    if ( extension )
                    {
                        // add a dot to extension
                        str = extension;
                        extension = g_strconcat( ".", extension, NULL );
                        g_free( str );
                        // Checking if the current extension is being used
                        if ( g_str_has_suffix( filename, extension ) )
                        {
                            // It is - determining filename without extension
                            n = strlen( filename ) - strlen( extension );
                            char ch = filename[n];
                            filename[n] = '\0';
                            filename_no_archive_ext = g_strdup( filename );
                            filename[n] = ch;
                            break;
                        }
                    }
                    g_free( filename_no_ext );
                    g_free( extension );
                }
            }
            g_strfreev( pathnames );

            /* An archive may not have an extension, or there may be no
             * extensions specified for the handler (they are optional)
             * - making sure filename_no_archive_ext is set in this case */
            if ( !filename_no_archive_ext )
                filename_no_archive_ext = g_strdup( filename );

            /* Now the extraction filename is obtained, determine the
             * normal filename without the extension */
            filename_no_ext = get_name_extension( filename_no_archive_ext,
                                                         FALSE,
                                                         &extension );

            /* 'Completing' the extension and dealing with files with
             * no extension */
            if ( !extension )
                extension = g_strdup( "" );
            else
            {
                str = extension;
                extension = g_strconcat( ".", extension, NULL );
                g_free( str );
            }

            /* Get extraction command - Doing this here as parent
             * directory creation needs access to the command. */
            char* err_msg = ptk_handler_load_script( HANDLER_MODE_ARC,
                                                    HANDLER_EXTRACT,
                                                    handler_xset,
                                                    NULL, &cmd );
            if ( err_msg )
            {
                g_warning( err_msg, NULL );
                g_free( err_msg );
                cmd = g_strdup( "" );
            }

            /* Dealing with creation of parent directory if needed -
             * never create a parent directory if '%G' is used - this is
             * an override substitution for the sake of gzip */
            gchar* parent_path = NULL;
            if ( create_parent && !g_strstr_len( cmd, -1, "%G" ) )
            {
                /* Determining full path of parent directory to make
                 * (also used later in '%g' substitution) */
                parent_path = g_build_filename( dest, filename_no_archive_ext,
                                                NULL );
                gchar* parent_orig = g_strdup( parent_path );
                n = 1;

                // Looping to find a path that doesnt exist
                while ( lstat64( parent_path, &statbuf ) == 0 )
                {
                    g_free( parent_path );
                    parent_path = g_strdup_printf( "%s-%s%d",
                                                   parent_orig,
                                                   _("copy"), ++n );
                }
                g_free( parent_orig );

                // Generating shell command to make directory
                parent_quote = bash_quote( parent_path );
                g_free( mkparent );
                mkparent = g_strdup_printf( ""
                    "mkdir -p %s || fm_handle_err\n"
                    "cd %s || fm_handle_err\n",
                    parent_quote, parent_quote );

                /* Dealing with the need to make extracted files writable if
                 * desired (e.g. a tar of files originally archived from a CD
                 * will be readonly). Root users don't obey such access
                 * permissions and making such owned files writeable may be a
                 * security issue */
                if ( write_access && geteuid() != 0 )
                {
                    /* deliberately omitting fm_handle_error - only a
                     * convenience function */
                    g_free( perm );
                    perm = g_strdup_printf( "chmod -R u+rwX %s\n",
                                            parent_quote );
                }
                g_free( parent_quote );
                parent_quote = NULL;
            }
            else
            {
                // Parent directory doesn't need to be created
                create_parent = FALSE;
            }

            // Debug code
            //g_message( "full_quote: %s\ndest: %s", full_quote, dest );

            /* Singular file extraction target (e.g. stdout-redirected
             * gzip) */
            if ( g_strstr_len( cmd, -1, "%g" ) || g_strstr_len( cmd, -1, "%G" ) )
            {
                /* Creating extraction target, taking into account whether
                 * a parent directory has been created or not - target is
                 * guaranteed not to exist so as to avoid overwriting */
                extract_target = g_build_filename(
                                    create_parent ? parent_path : dest,
                                                filename_no_archive_ext,
                                                NULL );
                n = 1;

                // Looping to find a path that doesnt exist
                while ( lstat64( extract_target, &statbuf ) == 0 )
                {
                    g_free( extract_target );
                    str = g_strdup_printf( "%s-%s%d%s", filename_no_ext,
                                           _("copy"), ++n, extension );
                    extract_target = g_build_filename(
                                    create_parent ? parent_path : dest,
                                    str, NULL );
                    g_free( str );
                }

                // Quoting target
                str = extract_target;
                extract_target = bash_quote( extract_target );
                g_free( str );
            }

            // Cleaning up
            g_free( filename );
            g_free( filename_no_archive_ext );
            g_free( filename_no_ext );
            g_free( extension );
            g_free( parent_path );
        }

        // Substituting %x %g %G
        str = cmd;
        cmd = replace_archive_subs( cmd, NULL, NULL, NULL, full_quote,
                                    extract_target );
        g_free( str );
        
        /* Finally constructing command to run, taking into account more than
         * one archive to list/extract. The mkparent command itself has error
         * checking - final error check not here as I want the code shared with
         * the list code flow */
        str = final_command;
        final_command = g_strdup_printf( "%s\ncd %s || fm_handle_err\n%s%s"
                                 "\n[[ $? -eq 0 ]] || fm_handle_err\n%s\n",
                  (g_strcmp0( final_command, "" ) < 0) ? "" : final_command,
                                         dest_quote, mkparent, cmd, perm );
        g_free( str );
        
        // Cleaning up
        g_free( full_quote );
        g_free( full_path );
        g_free( cmd );
        g_free( mkparent );
        g_free( perm );
    }

    /* When ran in a terminal, errors need to result in a pause so that
     * the user can review the situation - in any case an error check
     * needs to be made */
    str = generate_bash_error_function( in_term,
                                        create_parent ? parent_quote : NULL );
    s1 = final_command;
    final_command = g_strconcat( str, "\n", final_command, NULL );
    g_free( str );
    g_free( s1 );
    g_free( dest_quote );
    g_free( parent_quote );
    g_free( choose_dir );
    g_strfreev( archive_handlers );

    // Creating task
    char* task_name = g_strdup_printf( _("Extract %s"),
                                vfs_file_info_get_name( file ) );
    PtkFileTask* task = ptk_file_exec_new( task_name, cwd, dlgparent,
                    file_browser ? file_browser->task_view : NULL );
    g_free( task_name );

    /* Setting correct exec reference - probably causes different bash
     * to be output */
    if (file_browser)
        task->task->exec_browser = file_browser;
    else
        task->task->exec_desktop = desktop;

    // Configuring task
    task->task->exec_command = final_command;
    task->task->exec_browser = file_browser;
    task->task->exec_sync = !in_term;
    task->task->exec_show_error = TRUE;
    task->task->exec_scroll_lock = FALSE;
    task->task->exec_show_output = list_contents && !in_term;
    task->task->exec_terminal = in_term;
    task->task->exec_keep_terminal = keep_term;
    task->task->exec_export = TRUE;  // Setup SpaceFM bash variables

    // Setting custom icon
    XSet* set = xset_get( "arc_extract" );
    if ( set->icon )
        task->task->exec_icon = g_strdup( set->icon );

    // Running task
    ptk_file_task_run( task );
}

