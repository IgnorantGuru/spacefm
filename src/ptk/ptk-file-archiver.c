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

// Archive handlers treeview model enum
enum {
    COL_XSET_NAME,
    COL_HANDLER_NAME
};

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


// Function prototypes
void restore_defaults( GtkWidget* dlg );


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

// handler_xset_name optional if handler_xset passed
void config_load_handler_settings( XSet* handler_xset,
                                    gchar* handler_xset_name,
                                    GtkWidget* dlg )
{
    // Fetching actual xset if only the name has been passed
    if ( !handler_xset )
        handler_xset = xset_get( handler_xset_name );

    // Fetching widget references
    GtkWidget* chkbtn_handler_enabled = g_object_get_data( G_OBJECT( dlg ),
                                            "chkbtn_handler_enabled" );
    GtkWidget* entry_handler_name = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                                "entry_handler_name" );
    GtkWidget* entry_handler_mime = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                                "entry_handler_mime" );
    GtkWidget* entry_handler_extension = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "entry_handler_extension" );
    GtkWidget* entry_handler_compress = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "entry_handler_compress" );
    GtkWidget* entry_handler_extract = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "entry_handler_extract" );
    GtkWidget* entry_handler_list = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                                "entry_handler_list" );
    GtkWidget* chkbtn_handler_compress_term = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                        "chkbtn_handler_compress_term" );
    GtkWidget* chkbtn_handler_extract_term = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                        "chkbtn_handler_extract_term" );
    GtkWidget* chkbtn_handler_list_term = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "chkbtn_handler_list_term" );

    // Configuring widgets with handler settings. Only name, MIME and
    // extension warrant a warning
    // Commands are prefixed with '+' when they are ran in a terminal
    gboolean check_value = handler_xset->b != XSET_B_TRUE ? FALSE : TRUE;
    int start;
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_enabled ),
                                    check_value);
    if (!handler_xset->menu_label)
    {
        // Handler name is NULL - fall back to null-length string and
        // warn user
        gtk_entry_set_text( GTK_ENTRY( entry_handler_name ),
                            g_strdup( "" ) );
        g_warning("Archive handler associated with xset '%s' has no name",
                    handler_xset->name);
    }
    else
    {
        gtk_entry_set_text( GTK_ENTRY( entry_handler_name ),
                            g_strdup( handler_xset->menu_label ) );
    }
    if (!handler_xset->s)
    {
        gtk_entry_set_text( GTK_ENTRY( entry_handler_mime ),
                            g_strdup( "" ) );
        g_warning("Archive handler '%s' has no configured MIME type",
                    handler_xset->menu_label);
    }
    else
    {
        gtk_entry_set_text( GTK_ENTRY( entry_handler_mime ),
                            g_strdup( handler_xset->s ) );
    }
    if (!handler_xset->x)
    {
        gtk_entry_set_text( GTK_ENTRY( entry_handler_extension ),
                            g_strdup( "" ) );
        g_warning("Archive handler '%s' has no configured extension",
                    handler_xset->menu_label);
    }
    else
    {
        gtk_entry_set_text( GTK_ENTRY( entry_handler_extension ),
                            g_strdup( handler_xset->x ) );
    }
    if (!handler_xset->y)
    {
        gtk_entry_set_text( GTK_ENTRY( entry_handler_compress ),
                            g_strdup( "" ) );
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_compress_term ),
                                        FALSE);
    }
    else
    {
        if ( handler_xset->y[0] == '+' )
        {
            gtk_entry_set_text( GTK_ENTRY( entry_handler_compress ),
                                g_strdup( (handler_xset->y) + 1 ) );
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_compress_term ),
                                            TRUE);
        }
        else
        {
            gtk_entry_set_text( GTK_ENTRY( entry_handler_compress ),
                                g_strdup( handler_xset->y ) );
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_compress_term ),
                                            FALSE);
        }
    }
    if (!handler_xset->z)
    {
        gtk_entry_set_text( GTK_ENTRY( entry_handler_extract ),
                            g_strdup( "" ) );
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_extract_term ),
                                        FALSE);
    }
    else
    {
        if ( handler_xset->z[0] == '+' )
        {
            gtk_entry_set_text( GTK_ENTRY( entry_handler_extract ),
                                g_strdup( (handler_xset->z) + 1 ) );
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_extract_term ),
                                            TRUE);
        }
        else
        {
            gtk_entry_set_text( GTK_ENTRY( entry_handler_extract ),
                                g_strdup( handler_xset->z ) );
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_extract_term ),
                                            FALSE);
        }
    }
    if (!handler_xset->context)
    {
        gtk_entry_set_text( GTK_ENTRY( entry_handler_list ),
                            g_strdup( "" ) );
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_list_term ),
                                        FALSE);
    }
    else
    {
        if ( handler_xset->context[0] == '+' )
        {
            gtk_entry_set_text( GTK_ENTRY( entry_handler_list ),
                                g_strdup( (handler_xset->context) + 1 ) );
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_list_term ),
                                            TRUE);
        }
        else
        {
            gtk_entry_set_text( GTK_ENTRY( entry_handler_list ),
                                g_strdup( handler_xset->context ) );
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_list_term ),
                                            FALSE);
        }
    }
}

void on_configure_button_press( GtkButton* widget, GtkWidget* dlg )
{
    const char* dialog_title = _("Archive Handlers");
    gchar* help_string;

    // Fetching widgets and basic handler details
    GtkButton* btn_add = g_object_get_data( G_OBJECT( dlg ),
                                            "btn_add" );
    GtkButton* btn_apply = g_object_get_data( G_OBJECT( dlg ),
                                            "btn_apply" );
    GtkTreeView* view_handlers = g_object_get_data( G_OBJECT( dlg ),
                                            "view_handlers" );
    GtkWidget* chkbtn_handler_enabled = g_object_get_data(
                                            G_OBJECT( dlg ),
                                            "chkbtn_handler_enabled" );
    GtkWidget* entry_handler_name = (GtkWidget*)g_object_get_data(
                                            G_OBJECT( dlg ),
                                                "entry_handler_name" );
    GtkWidget* entry_handler_mime = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                                "entry_handler_mime" );
    GtkWidget* entry_handler_extension = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "entry_handler_extension" );
    GtkWidget* entry_handler_compress = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "entry_handler_compress" );
    GtkWidget* entry_handler_extract = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "entry_handler_extract" );
    GtkWidget* entry_handler_list = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                                "entry_handler_list" );
    GtkWidget* chkbtn_handler_compress_term = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                        "chkbtn_handler_compress_term" );
    GtkWidget* chkbtn_handler_extract_term = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                        "chkbtn_handler_extract_term" );
    GtkWidget* chkbtn_handler_list_term = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "chkbtn_handler_list_term" );
    const gchar* handler_name = gtk_entry_get_text( GTK_ENTRY ( entry_handler_name ) );
    const gchar* handler_mime = gtk_entry_get_text( GTK_ENTRY ( entry_handler_mime ) );
    const gchar* handler_extension = gtk_entry_get_text( GTK_ENTRY ( entry_handler_extension ) );

    // Fetching selection from treeview
    GtkTreeSelection* selection;
    selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( view_handlers ) );

    // Fetching the model and iter from the selection
    GtkTreeIter it;
    GtkTreeModel* model;

    // Checking if no selection is present (happens when the dialog first
    // loads, and when a handler is deleted)
    if ( !gtk_tree_selection_get_selected( GTK_TREE_SELECTION( selection ),
         NULL, NULL ) )
    {
        // There isnt selection - selecting the first item in the list
        GtkTreePath* new_path = gtk_tree_path_new_first();
        gtk_tree_selection_select_path( GTK_TREE_SELECTION( selection ),
                                new_path );
        gtk_tree_path_free( new_path );
        new_path = NULL;
    }

    // Obtaining iter for the current selection
    gchar* handler_name_from_model = NULL;  // Used to detect renames
    gchar* xset_name = NULL;
    XSet* handler_xset = NULL;

    // Getting selection fails if there are no handlers
    if ( gtk_tree_selection_get_selected( GTK_TREE_SELECTION( selection ),
         &model, &it ) )
    {
        // Succeeded - handlers present
        // Fetching data from the model based on the iterator. Note that
        // this variable used for the G_STRING is defined on the stack,
        // so should be freed for me
        gtk_tree_model_get( model, &it,
                            COL_XSET_NAME, &xset_name,
                            COL_HANDLER_NAME, &handler_name_from_model,
                            -1 );

        // Fetching the xset now I have the xset name
        handler_xset = xset_get(xset_name);

        // Making sure it has been fetched
        if (!handler_xset)
        {
            g_warning("Unable to fetch the xset for the archive handler"
            " '%s' - does it exist?", handler_name);
            return;
        }
    }

    if ( widget == btn_add )
    {
        // Adding new placeholder handler, disabled
        XSet* new_handler_xset = add_new_arctype();
        new_handler_xset->b = XSET_B_FALSE;

        // Generating placeholder handler label (this is my 'name', but
        // is separate from an actual internal xset name which has
        // already been decided by add_new_arctype)
        // Problem: Since the actual name isn't used, how do I determine
        // which label is free?
        // This isn't needed - name uniqueness would only be relevant if
        // the label was the actual handler's name
        /*gchar* new_handler_label;
        for ( i = 0; i < 100 && new_handler_label = g_strdup_printf( "arctype_rar", xset_is( new_handler_name ); ++i; )
        {

        }
        * */
        xset_set_set( new_handler_xset, "label", "New Handler" );
        xset_set_set( new_handler_xset, "s", "" );  // Mime Type(s)
        xset_set_set( new_handler_xset, "x", "" );  // Extension(s)
        xset_set_set( new_handler_xset, "y", "" );  // Compress command
        xset_set_set( new_handler_xset, "z", "" );  // Extract command
        xset_set_set( new_handler_xset, "cxt", "" );  // List command

        // Fetching list store
        GtkListStore* list = (GtkListStore*)g_object_get_data( G_OBJECT( dlg ), "list" );

        // Obtaining appending iterator for treeview model
        GtkTreeIter iter;
        gtk_list_store_append( GTK_LIST_STORE( list ), &iter );

        // Adding handler to model
        gchar* new_handler_name = g_strdup( "New Handler" );
        gchar* new_xset_name = g_strdup( new_handler_xset->name );
        gtk_list_store_set( GTK_LIST_STORE( list ), &iter,
                            COL_XSET_NAME, new_xset_name,
                            COL_HANDLER_NAME, new_handler_name,
                            -1 );

        // Updating available archive handlers list
        gchar* new_handlers_list = g_strdup_printf ( "%s %s",
                                                xset_get_s( "arc_conf" ),
                                                new_xset_name );
        xset_set( "arc_conf", "s", new_handlers_list );

        // Freeing strings
        g_free(new_handler_name);
        g_free(new_xset_name);
        g_free(new_handlers_list);

        // Activating the new handler - the normal loading code
        // automatically kicks in
        GtkTreePath* new_handler_path = gtk_tree_model_get_path( GTK_TREE_MODEL( model ),
                                                                &iter );
        gtk_tree_view_set_cursor( GTK_TREE_VIEW( view_handlers ),
                                        new_handler_path, NULL, FALSE );
        gtk_tree_path_free( new_handler_path );
    }
    else if ( widget == btn_apply )
    {
        // Exiting if apply has been pressed when no handlers are present
        if (xset_name == NULL) return;

        // Validating data. Note that data straight from widgets shouldnt
        // be modified or stored
        if (g_strcmp0(handler_name, "") <= 0)
        {
            // Handler name not set - warning user and exiting. Note
            // that the created dialog does not have an icon set
            xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_WARNING,
                                dialog_title, NULL, FALSE,
                                _("Please enter a valid handler name "
                                "before saving."), NULL, NULL );
            gtk_widget_grab_focus( entry_handler_name );
            return;
        }
        if (g_strcmp0(handler_mime, "") <= 0)
        {
            // Handler MIME not set - warning user and exiting. Note
            // that the created dialog does not have an icon set
            xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_WARNING,
                                dialog_title, NULL, FALSE,
                                g_strdup_printf(_("Please enter a valid "
                                "MIME content type for the '%s' handler "
                                "before saving."),
                                handler_name), NULL, NULL );
            gtk_widget_grab_focus( entry_handler_mime );
            return;
        }
        if (g_strcmp0(handler_extension, "") <= 0 || *handler_extension != '.')
        {
            // Handler extension is either not set or does not start with
            // a full stop - warning user and exiting. Note
            // that the created dialog does not have an icon set
            xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_WARNING,
                                dialog_title, NULL, FALSE,
                                g_strdup_printf(_("Please enter a valid "
                                "file extension for the '%s' handler "
                                "before saving."),
                                handler_name), NULL, NULL );
            gtk_widget_grab_focus( entry_handler_extension );
            return;
        }

        // Other settings are commands to run in different situations -
        // since different handlers may or may not need different
        // commands, nothing further is mandated
        // Prepending commands with '+' if they are to be ran in a
        // terminal. Declared outside the ifs to avoid the block scope...
        // g_strdup'd to avoid anal const compiler warning...
        const gboolean handler_compress_term = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON ( chkbtn_handler_compress_term ) );
        const gboolean handler_extract_term = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON ( chkbtn_handler_extract_term ) );
        const gboolean handler_list_term = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON ( chkbtn_handler_list_term ) );
        gchar* handler_compress, *handler_extract, *handler_list;

        if (handler_compress_term)
        {
            handler_compress = g_strconcat( "+",
                gtk_entry_get_text( GTK_ENTRY ( entry_handler_compress ) ),
                NULL );
        }
        else
        {
            handler_compress = g_strdup( gtk_entry_get_text(
                GTK_ENTRY ( entry_handler_compress ) ) );
        }
        if (handler_extract_term)
        {
            handler_extract = g_strconcat( "+",
                gtk_entry_get_text( GTK_ENTRY ( entry_handler_extract ) ),
                NULL );
        }
        else
        {
            handler_extract = g_strdup( gtk_entry_get_text(
                GTK_ENTRY ( entry_handler_extract ) ) );
        }
        if (handler_list_term)
        {
            handler_list = g_strconcat( "+",
                gtk_entry_get_text( GTK_ENTRY ( entry_handler_list ) ),
                NULL );
        }
        else
        {
            handler_list = g_strdup( gtk_entry_get_text(
                GTK_ENTRY ( entry_handler_list ) ) );
        }

        // Determining current handler enabled state
        gboolean handler_enabled = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON( chkbtn_handler_enabled ) ) ?
            XSET_B_TRUE : XSET_B_FALSE;

        // Checking if the handler has been renamed
        if (g_strcmp0( handler_name_from_model, handler_name ) != 0)
        {
            // It has - updating model
            gtk_list_store_set( GTK_LIST_STORE( model ), &it,
                        COL_XSET_NAME, xset_name,
                        COL_HANDLER_NAME, handler_name,
                        -1 );
        }

        // Saving archive handler
        // Finally saving the archive handler parameters
        handler_xset->b = handler_enabled;
        xset_set_set( handler_xset, "label", handler_name );
        xset_set_set( handler_xset, "s", handler_mime );
        xset_set_set( handler_xset, "x", handler_extension );
        xset_set_set( handler_xset, "y", handler_compress );
        xset_set_set( handler_xset, "z", handler_extract );
        xset_set_set( handler_xset, "cxt", handler_list );

        // Freeing strings
        g_free(handler_compress);
        g_free(handler_extract);
        g_free(handler_list);
        return;
    }
    else
    {
        // Exiting if remove has been pressed when no handlers are present
        if (xset_name == NULL) return;

        // Updating available archive handlers list - fetching current
        // handlers
        char* archive_handlers_s = xset_get_s( "arc_conf" );
        gchar** archive_handlers = g_strsplit( archive_handlers_s, " ", -1 );
        gchar* new_archive_handlers_s = g_strdup( "" );
        gchar* new_archive_handlers_s_temp;

        // Looping for handlers (NULL-terminated list)
        int i;
        for (i = 0; archive_handlers[i] != NULL; ++i)
        {
            // Appending to new archive handlers list when it isnt the
            // deleted handler - remember that archive handlers are
            // referred to by their xset names, not handler names!!
            if (g_strcmp0( archive_handlers[i], xset_name ) != 0)
            {
                // Debug code
                //g_message("archive_handlers[i]: %s\nxset_name: %s",
                //                        archive_handlers[i], xset_name);

                new_archive_handlers_s_temp = new_archive_handlers_s;
                if (g_strcmp0( new_archive_handlers_s, "" ) == 0)
                {
                    new_archive_handlers_s = g_strdup( archive_handlers[i] );
                }
                else
                {
                    new_archive_handlers_s = g_strdup_printf( "%s %s",
                        new_archive_handlers_s, archive_handlers[i] );
                }
                g_free(new_archive_handlers_s_temp);
            }
        }

        // Finally updating handlers
        xset_set( "arc_conf", "s", new_archive_handlers_s );

        // Clearing up
        g_strfreev( archive_handlers );
        g_free(new_archive_handlers_s);

        // Deleting xset
        xset_custom_delete( handler_xset, FALSE );
        handler_xset = NULL;

        // Removing handler from the list
        gtk_list_store_remove( GTK_LIST_STORE( model ), &it );
        return;
    }
}

void on_configure_changed( GtkTreeSelection* selection, GtkWidget* dlg )
{
    // This event is triggered when the selected row is changed through
    // the keyboard

    // Fetching the model and iter from the selection
    GtkTreeIter it;
    GtkTreeModel* model;
    if ( !gtk_tree_selection_get_selected( selection, &model, &it ) )
        return;

    // Fetching data from the model based on the iterator. Note that this
    // variable used for the G_STRING is defined on the stack, so should
    // be freed for me
    gchar* handler_name;  // Not actually used...
    gchar* xset_name;
    gtk_tree_model_get( model, &it,
                        COL_XSET_NAME, &xset_name,
                        COL_HANDLER_NAME, &handler_name,
                        -1 );

    // Loading new archive handler values
    config_load_handler_settings( NULL, xset_name, dlg );

    // Focussing archive handler name
    // Selects the text rather than just placing the cursor at the start
    // of the text...
    /*GtkWidget* entry_handler_name = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                                "entry_handler_name" );
    gtk_widget_grab_focus( entry_handler_name );*/
}

void on_configure_row_activated( GtkTreeView* view, GtkTreePath* tree_path,
                                        GtkTreeViewColumn* col, GtkWidget* dlg )
{
    // This event is triggered when the selected row is changed by the
    // mouse

    // Fetching the model from the view
    GtkTreeModel* model = gtk_tree_view_get_model( GTK_TREE_VIEW( view ) );

    // Obtaining an iterator based on the view position
    GtkTreeIter it;
    if ( !gtk_tree_model_get_iter( model, &it, tree_path ) )
        return;

    // Fetching data from the model based on the iterator. Note that this
    // variable used for the G_STRING is defined on the stack, so should
    // be freed for me
    gchar* handler_name;  // Not actually used...
    gchar* xset_name;
    gtk_tree_model_get( model, &it,
                        COL_XSET_NAME, &xset_name,
                        COL_HANDLER_NAME, &handler_name,
                        -1 );

    // Loading new archive handler values
    config_load_handler_settings( NULL, xset_name, dlg );

    // Focussing archive handler name
    // Selects the text rather than just placing the cursor at the start
    // of the text...
    /*GtkWidget* entry_handler_name = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                                "entry_handler_name" );
    gtk_widget_grab_focus( entry_handler_name );*/
}

void populate_archive_handlers( GtkListStore* list, GtkWidget* dlg )
{
    // Fetching available archive handlers (literally gets member s from
    // the xset) - user-defined order has already been set
    char* archive_handlers_s = xset_get_s( "arc_conf" );
    gchar** archive_handlers = g_strsplit( archive_handlers_s, " ", -1 );

    // Debug code
    //g_message("archive_handlers_s: %s", archive_handlers_s);

    // Looping for handlers (NULL-terminated list)
    GtkTreeIter iter;
    int i;
    for (i = 0; archive_handlers[i] != NULL; ++i)
    {
        // Obtaining appending iterator for treeview model
        gtk_list_store_append( GTK_LIST_STORE( list ), &iter );

        // Fetching handler
        XSet* handler_xset = xset_get( archive_handlers[i] );

        // Adding handler to model
        gchar* handler_name = g_strdup( handler_xset->menu_label );
        gchar* xset_name = g_strdup( archive_handlers[i] );
        gtk_list_store_set( GTK_LIST_STORE( list ), &iter,
                            COL_XSET_NAME, xset_name,
                            COL_HANDLER_NAME, handler_name,
                            -1 );

        // Populating widgets if this is the first handler
        if ( i == 0 )
            config_load_handler_settings( handler_xset, NULL,
                                          GTK_WIDGET( dlg ) );
    }

    // Clearing up archive_handlers
    g_strfreev( archive_handlers );
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

void ptk_file_archiver_config( PtkFileBrowser* file_browser )
{
    /*
    Archives Types - 1 per xset as:
        set->name       xset name
        set->b          enabled  (XSET_UNSET|XSET_B_FALSE|XSET_B_TRUE)
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
        newset->b = XSET_B_TRUE;                              // enable
        xset_set_set( newset, "label", "Windows CAB" );        // set archive Name - this internally sets menu_label
        xset_set_set( newset, "s", "application/winjunk" );    // set Mime Type(s)
        xset_set_set( newset, "x", ".cab" );                   // set Extension(s)
        xset_set_set( newset, "y", "createcab" );              // set Compress cmd
        xset_set_set( newset, "z", "excab" );                  // set Extract cmd
        xset_set_set( newset, "cxt", "listcab" );              // set List cmd - This really is cxt and not ctxt - xset_set_set bug thats already worked around

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

    // TODO: <IgnorantGuru> Also, you might have a look at how your config dialog behaves from the desktop menu.  Specifically, you may want to pass your function (DesktopWindow* desktop) in lieu of file_browser in that case.  So the prototype will be:
    // nm don't have that branch handy.  But you function can accept both file_browser and desktop and use whichever is non-NULL for the parent
    // If that doesn't make sense now, ask me later or I can hack it in.  That archive menu appears when right-clicking a desktop item

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
    // 'Restore defaults' button has custom text but a stock image
    GtkButton* btn_defaults = GTK_BUTTON( gtk_dialog_add_button( GTK_DIALOG( dlg ),
                                                "Restore Defaults",
                                                GTK_RESPONSE_NONE ) );
    GtkWidget* btn_defaults_image = xset_get_image( "GTK_STOCK_CLEAR",
                                                GTK_ICON_SIZE_BUTTON );
    gtk_button_set_image( GTK_BUTTON( btn_defaults ), GTK_WIDGET ( btn_defaults_image ) );
    g_object_set_data( G_OBJECT( dlg ), "btn_defaults", GTK_BUTTON( btn_defaults ) );
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
                        G_CALLBACK( on_configure_changed ), dlg );

    // Adding column to the treeview
    GtkTreeViewColumn* col = gtk_tree_view_column_new();

    // Change columns to optimal size whenever the model changes
    gtk_tree_view_column_set_sizing( col, GTK_TREE_VIEW_COLUMN_AUTOSIZE );
    
    GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start( col, renderer, TRUE );

    // Tie model data to the column
    gtk_tree_view_column_add_attribute( col, renderer,
                                         "text", COL_HANDLER_NAME);

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
                        GTK_ENTRY( entry_handler_extract ) );
    GtkWidget* entry_handler_list = gtk_entry_new();
    g_object_set_data( G_OBJECT( dlg ), "entry_handler_list",
                        GTK_ENTRY( entry_handler_list ) );
    GtkWidget* chkbtn_handler_compress_term = gtk_check_button_new();
    g_object_set_data( G_OBJECT( dlg ), "chkbtn_handler_compress_term",
                        GTK_CHECK_BUTTON( chkbtn_handler_compress_term ) );
    gtk_widget_set_tooltip_text( GTK_WIDGET( chkbtn_handler_compress_term ),
                                    "Run in terminal" );
    GtkWidget* chkbtn_handler_extract_term = gtk_check_button_new();
    g_object_set_data( G_OBJECT( dlg ), "chkbtn_handler_extract_term",
                        GTK_CHECK_BUTTON( chkbtn_handler_extract_term ) );
    gtk_widget_set_tooltip_text( GTK_WIDGET( chkbtn_handler_extract_term ),
                                    "Run in terminal" );
    GtkWidget* chkbtn_handler_list_term = gtk_check_button_new();
    g_object_set_data( G_OBJECT( dlg ), "chkbtn_handler_list_term",
                        GTK_CHECK_BUTTON( chkbtn_handler_list_term ) );
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

    // Adding archive handlers to list
    populate_archive_handlers( GTK_LIST_STORE( list ), GTK_WIDGET( dlg ) );

    // TODO: set_reorderable TRUE?
    // TODO: Help text in label on right? below stuff of substitutions?

    // Rendering dialog - while loop is used to deal with standard
    // buttons that should not cause the dialog to exit
    gtk_widget_show_all( GTK_WIDGET( dlg ) );
    int response;
    while ( response = gtk_dialog_run( GTK_DIALOG( dlg ) ) )
    {
        if ( response == GTK_RESPONSE_OK )
        {
            break;
        }
        else if ( response == GTK_RESPONSE_HELP )
        {
            // TODO: Sort out proper help
            xset_show_help( dlg, NULL, "#designmode-style-context" );
        }
        else if ( response == GTK_RESPONSE_NONE )
        {
            // Restore defaults requested
            restore_defaults( GTK_WIDGET( dlg ) );
        }
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

void restore_defaults( GtkWidget* dlg )
{
    // Note that defaults are also maintained in settings.c:xset_defaults

    // Exiting if the user doesn't really want to restore defaults
    if (xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_WARNING,
                        _("Archive Handlers - Restore Defaults"), NULL,
                        GTK_BUTTONS_OK_CANCEL, _("Are you sure you want "
                        "to reset archive handlers to their default "
                        "settings?"),
                        NULL, NULL) != GTK_RESPONSE_OK) return;

    // Removing current archive handlers
    char* archive_handlers_s = xset_get_s( "arc_conf" );
    gchar** archive_handlers = g_strsplit( archive_handlers_s, " ", -1 );

    // Looping for handlers (NULL-terminated list)
    int i;
    XSet* handler_xset;
    for (i = 0; archive_handlers[i] != NULL; ++i)
    {
        // Deleting handler
        handler_xset = xset_get( archive_handlers[i] );
        xset_custom_delete( handler_xset, FALSE );
    }

    // Clearing up
    g_strfreev( archive_handlers );

    // Restoring xsets back to their default state
    // Main list of archive handlers
    xset_set( "arc_conf", "s", "arctype_rar arctype_zip" );

    // Individual archive handlers
    XSet* set = xset_set( "arctype_rar", "label", _("RAR") );  // Name as it appears in list - translatable?
    set->b = XSET_B_TRUE;
    set->s = g_strdup( "application/x-rar" );
    set->x = g_strdup( ".rar" );
    //set->y = NULL;                   // compress command
    set->z = g_strdup( "unrar" );    // extract command
    set->context = g_strdup( "" );   // list command

    set = xset_set( "arctype_zip", "label", ("Zip") );  // Name as it appears in list - translatable?
    set->b = XSET_B_TRUE;
    set->s = g_strdup( "application/x-zip" );
    set->x = g_strdup( ".zip" );
    set->y = g_strdup( "zip" );      // compress command
    set->z = g_strdup( "unzip" );    // extract command
    set->context = g_strdup( "" );   // list command

    // Clearing and adding archive handlers to list (this also selects
    // the first handler and therefore populates the handler widgets)
    GtkListStore* list = g_object_get_data( G_OBJECT( dlg ), "list" );
    gtk_list_store_clear( GTK_LIST_STORE( list ) );
    populate_archive_handlers( GTK_LIST_STORE( list ), GTK_WIDGET( dlg ) );
}
