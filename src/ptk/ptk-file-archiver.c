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

// Archive creation handlers combobox model enum
enum {
    // COL_XSET_NAME
    COL_HANDLER_EXTENSIONS = 1
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
static void on_configure_handler_enabled_check(
    GtkToggleButton *togglebutton,
    gpointer user_data );
static void restore_defaults( GtkWidget* dlg );
static gboolean validate_handler( GtkWidget* dlg );


static XSet* add_new_arctype()
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

static gchar* archive_handler_get_first_extension( XSet* handler_xset )
{
    // Function deals with the possibility that a handler is responsible
    // for multiple MIME types and therefore file extensions. Functions
    // like archive creation need only one extension
    if (handler_xset)
    {
        // Obtaining first handled extension
        gchar** extensions = g_strsplit( handler_xset->x, ":", -1 );
        gchar* first_extension = g_strdup( extensions[0] );

        // Clearing up
        g_strfreev( extensions );

        // Returning first extension
        return first_extension;
    }
    else return g_strdup( "" );
}

static gboolean archive_handler_is_format_supported( XSet* handler_xset,
                                                     const char* type,
                                                     const char* extension,
                                                     int operation )
{
    gboolean format_supported = FALSE, mime_or_extension_support = FALSE;
    gchar *ext = NULL;

    /* If one was passed, ensuring an extension starts with '.' (
     * get_name_extension does not provide this) */
    if (extension && extension[0] != '.')
    {
        ext = g_strconcat( ".", extension, NULL );
    }
    else ext = g_strdup( "" );

    // Checking it if its enabled
    if (handler_xset && handler_xset->b == XSET_B_TRUE)
    {
        /* Obtaining handled MIME types and file extensions
         * (colon-delimited) */
        gchar** mime_types = g_strsplit( handler_xset->s, ":", -1 );
        gchar** extensions = g_strsplit( handler_xset->x, ":", -1 );

        // Looping for handled MIME types (NULL-terminated list)
        int i;
        for (i = 0; mime_types[i] != NULL; ++i)
        {
            // Checking to see if the handler can deal with the
            // current MIME type
            if (g_strcmp0( mime_types[i], type ) == 0)
            {
                // It can - flagging and breaking
                mime_or_extension_support = TRUE;
                break;
            }
        }

        // Looping for handled extensions if mime type wasn't supported
        if (!mime_or_extension_support)
        {
            for (i = 0; extensions[i] != NULL; ++i)
            {
                // Checking to see if the handler can deal with the
                // current extension
                if (g_strcmp0( extensions[i], ext ) == 0)
                {
                    // It can - flagging and breaking
                    mime_or_extension_support = TRUE;
                    break;
                }
            }
        }

        /* Checking if a found handler can cope with the requested
         * operation - deal with possibility of empty command set to run
         * in terminal, therefore '+' stored */
        if (mime_or_extension_support)
        {
            switch (operation)
            {
                case ARC_COMPRESS:

                    if (handler_xset->y
                        && g_strcmp0( handler_xset->y, "" ) != 0
                        && g_strcmp0( handler_xset->y, "+" ) != 0)
                    {
                        /* Compression possible - setting flag and
                         * breaking */
                        format_supported = TRUE;
                    }
                    break;

                case ARC_EXTRACT:

                    if (handler_xset->z
                        && g_strcmp0( handler_xset->z, "" ) != 0
                        && g_strcmp0( handler_xset->z, "+" ) != 0)
                    {
                        /* Extraction possible - setting flag and
                         * breaking */
                        format_supported = TRUE;
                    }
                    break;

                case ARC_LIST:

                    if (handler_xset->context
                    && g_strcmp0( handler_xset->context, "" ) != 0
                    && g_strcmp0( handler_xset->context, "+" ) != 0)
                    {
                        /* Listing possible - setting flag and
                         * breaking */
                        format_supported = TRUE;
                    }
                    break;

                default:

                    /* Invalid archive operation passed - warning
                     * user and exiting */
                    g_warning("archive_handler_is_format_supported "
                    "was passed an invalid archive operation ('%d') "
                    "on type '%s'!", operation, type);
                    format_supported = FALSE;
                    break;
            }
        }

        // Clearing up
        g_strfreev( mime_types );
        g_strfreev( extensions );
    }

    // Clearing up
    g_free( ext );

    // Returning result
    return format_supported;
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

    // Dealing with different operations
    switch (operation)
    {
        case ARC_COMPRESS:

            return handler_xset->y && *handler_xset->y == '+';

        case ARC_EXTRACT:

            return handler_xset->z && *handler_xset->z == '+';

        case ARC_LIST:

            return handler_xset->context && *handler_xset->context == '+';

        default:

            // Invalid archive operation passed - warning user and
            // exiting
            g_warning("archive_handler_run_in_term was passed an invalid"
            " archive operation ('%d')!", operation);
            return FALSE;
    }
}

// handler_xset_name optional if handler_xset passed
static void config_load_handler_settings( XSet* handler_xset,
                                    gchar* handler_xset_name,
                                    GtkWidget* dlg )
{
    // Fetching actual xset if only the name has been passed
    if ( !handler_xset )
        handler_xset = xset_get( handler_xset_name );

    // Fetching widget references
    GtkWidget* chkbtn_handler_enabled = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
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
    GtkWidget* btn_remove = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                        "btn_remove" );
    GtkWidget* btn_apply = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "btn_apply" );

    /* At this point a handler exists, so making remove and apply buttons
     * sensitive as well as the enabled checkbutton */
    gtk_widget_set_sensitive( GTK_WIDGET( btn_remove ), TRUE );
    gtk_widget_set_sensitive( GTK_WIDGET( btn_apply ), TRUE );
    gtk_widget_set_sensitive( GTK_WIDGET( chkbtn_handler_enabled ), TRUE );

    // Configuring widgets with handler settings. Only name, MIME and
    // extension warrant a warning
    // Commands are prefixed with '+' when they are ran in a terminal
    gboolean check_value = handler_xset->b != XSET_B_TRUE ? FALSE : TRUE;
    int start;
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_enabled ),
                                    check_value );
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

static void config_unload_handler_settings( GtkWidget* dlg )
{
    // Fetching widget references
    GtkWidget* chkbtn_handler_enabled = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
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
    GtkWidget* btn_remove = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                        "btn_remove" );
    GtkWidget* btn_apply = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "btn_apply" );

    // Disabling main change buttons
    gtk_widget_set_sensitive( GTK_WIDGET( btn_remove ), FALSE );
    gtk_widget_set_sensitive( GTK_WIDGET( btn_apply ), FALSE );

    // Unchecking handler if enabled (this disables all handler widgets)
    if (gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON ( chkbtn_handler_enabled ) ))
    {
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_enabled ),
                                      FALSE);
        on_configure_handler_enabled_check( GTK_TOGGLE_BUTTON ( chkbtn_handler_enabled ),
                                            dlg );
    }
    gtk_widget_set_sensitive( GTK_WIDGET( chkbtn_handler_enabled ), FALSE );

    // Resetting all widgets
    gtk_entry_set_text( GTK_ENTRY( entry_handler_name ), g_strdup( "" ) );
    gtk_entry_set_text( GTK_ENTRY( entry_handler_mime ), g_strdup( "" ) );
    gtk_entry_set_text( GTK_ENTRY( entry_handler_extension ), g_strdup( "" ) );
    gtk_entry_set_text( GTK_ENTRY( entry_handler_compress ), g_strdup( "" ) );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_compress_term ),
                                  FALSE);
    gtk_entry_set_text( GTK_ENTRY( entry_handler_extract ), g_strdup( "" ) );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_extract_term ),
                                  FALSE);
    gtk_entry_set_text( GTK_ENTRY( entry_handler_list ), g_strdup( "" ) );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_list_term ),
                                  FALSE);
}

static void on_configure_button_press( GtkButton* widget, GtkWidget* dlg )
{
    const char* dialog_title = _("Archive Handlers");

    // Fetching widgets and handler details
    GtkButton* btn_add = (GtkButton*)g_object_get_data( G_OBJECT( dlg ),
                                            "btn_add" );
    GtkButton* btn_apply = (GtkButton*)g_object_get_data( G_OBJECT( dlg ),
                                            "btn_apply" );
    GtkButton* btn_remove = (GtkButton*)g_object_get_data( G_OBJECT( dlg ),
                                            "btn_remove" );
    GtkTreeView* view_handlers = (GtkTreeView*)g_object_get_data( G_OBJECT( dlg ),
                                            "view_handlers" );
    GtkWidget* chkbtn_handler_enabled = (GtkWidget*)g_object_get_data(
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
    const gboolean handler_compress_term = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON ( chkbtn_handler_compress_term ) );
    const gboolean handler_extract_term = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON ( chkbtn_handler_extract_term ) );
    const gboolean handler_list_term = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON ( chkbtn_handler_list_term ) );
    gchar* handler_compress, *handler_extract, *handler_list;

    // Commands are prefixed with '+' when they are to be ran in a
    // terminal
    // g_strdup'd to avoid anal const compiler warning...
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
            goto cleanexit;
        }
    }

    if ( widget == btn_add )
    {
        // Exiting if there is no handler to add
        if (g_strcmp0( handler_name, "" ) <= 0)
            goto cleanexit;

        // Adding new handler as a copy of the current active handler
        XSet* new_handler_xset = add_new_arctype();
        new_handler_xset->b = gtk_toggle_button_get_active(
                                GTK_TOGGLE_BUTTON( chkbtn_handler_enabled )
                              ) ? XSET_B_TRUE : XSET_B_FALSE;
        xset_set_set( new_handler_xset, "label", g_strdup( handler_name ) );
        xset_set_set( new_handler_xset, "s", g_strdup( handler_mime ) );  // Mime Type(s)
        xset_set_set( new_handler_xset, "x", g_strdup( handler_extension ) );  // Extension(s)
        xset_set_set( new_handler_xset, "y", g_strdup( handler_compress ) );  // Compress command
        xset_set_set( new_handler_xset, "z", g_strdup( handler_extract ) );  // Extract command
        xset_set_set( new_handler_xset, "cxt", g_strdup( handler_list ) );  // List command

        // Fetching list store
        GtkListStore* list = (GtkListStore*)g_object_get_data( G_OBJECT( dlg ), "list" );

        // Obtaining appending iterator for treeview model
        GtkTreeIter iter;
        gtk_list_store_append( GTK_LIST_STORE( list ), &iter );

        // Adding handler to model
        gchar* new_handler_name = g_strdup( handler_name );
        gchar* new_xset_name = g_strdup( new_handler_xset->name );
        gtk_list_store_set( GTK_LIST_STORE( list ), &iter,
                            COL_XSET_NAME, new_xset_name,
                            COL_HANDLER_NAME, new_handler_name,
                            -1 );

        // Updating available archive handlers list
        gchar* new_handlers_list = g_strdup_printf( "%s %s",
                                                xset_get_s( "arc_conf2" ),
                                                new_xset_name );
        xset_set( "arc_conf2", "s", new_handlers_list );

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

        // Making sure the remove and apply buttons are sensitive
        gtk_widget_set_sensitive( GTK_WIDGET( btn_remove ), TRUE );
        gtk_widget_set_sensitive( GTK_WIDGET( btn_apply ), TRUE );

        /* Validating - remember that IG wants the handler to be saved
         * even with invalid commands */
        validate_handler( dlg );
    }
    else if ( widget == btn_apply )
    {
        // Exiting if apply has been pressed when no handlers are present
        if (xset_name == NULL) goto cleanexit;

        /* Validating - remember that IG wants the handler to be saved
         * even with invalid commands */
        validate_handler( dlg );

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
        handler_xset->b = handler_enabled;
        xset_set_set( handler_xset, "label", handler_name );
        xset_set_set( handler_xset, "s", handler_mime );
        xset_set_set( handler_xset, "x", handler_extension );
        xset_set_set( handler_xset, "y", handler_compress );
        xset_set_set( handler_xset, "z", handler_extract );
        xset_set_set( handler_xset, "cxt", handler_list );
    }
    else
    {
        // Exiting if remove has been pressed when no handlers are present
        if (xset_name == NULL) goto cleanexit;

        // Updating available archive handlers list - fetching current
        // handlers
        char* archive_handlers_s = xset_get_s( "arc_conf2" );
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
        xset_set( "arc_conf2", "s", new_archive_handlers_s );

        // Deleting xset
        xset_custom_delete( handler_xset, FALSE );
        handler_xset = NULL;

        // Removing handler from the list
        gtk_list_store_remove( GTK_LIST_STORE( model ), &it );

        if (g_strcmp0( new_archive_handlers_s, "" ) <= 0)
        {
            /* Making remove and apply buttons insensitive if the last
             * handler has been removed */
            gtk_widget_set_sensitive( GTK_WIDGET( btn_remove ), FALSE );
            gtk_widget_set_sensitive( GTK_WIDGET( btn_apply ), FALSE );
        }
        else
        {
            /* Still remaining handlers - selecting the first one,
             * otherwise nothing is now selected */
            GtkTreePath *new_path = gtk_tree_path_new_first();
            gtk_tree_selection_select_path( GTK_TREE_SELECTION( selection ),
                                    new_path );
            gtk_tree_path_free( new_path );
        }

        // Clearing up
        g_strfreev( archive_handlers );
        g_free( new_archive_handlers_s );
    }

cleanexit:

    // Freeing strings
    g_free( handler_compress );
    g_free( handler_extract );
    g_free( handler_list );
}

static void on_configure_changed( GtkTreeSelection* selection,
                                  GtkWidget* dlg )
{
    /* This event is triggered when the selected row is changed through
     * the keyboard, or no row is selected at all */

    // Fetching the model and iter from the selection
    GtkTreeIter it;
    GtkTreeModel* model;
    if ( !gtk_tree_selection_get_selected( selection, &model, &it ) )
    {
        // User has unselected all rows - removing loaded handler
        config_unload_handler_settings( dlg );
        return;
    }

    /* Fetching data from the model based on the iterator. Note that this
     * variable used for the G_STRING is defined on the stack, so should
     * be freed for me */
    gchar* handler_name;  // Not actually used...
    gchar* xset_name;
    gtk_tree_model_get( model, &it,
                        COL_XSET_NAME, &xset_name,
                        COL_HANDLER_NAME, &handler_name,
                        -1 );

    // Loading new archive handler values
    config_load_handler_settings( NULL, xset_name, dlg );

    /* Focussing archive handler name
     * Selects the text rather than just placing the cursor at the start
     * of the text... */
    /*GtkWidget* entry_handler_name = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                                "entry_handler_name" );
    gtk_widget_grab_focus( entry_handler_name );*/
}

static void on_configure_drag_end( GtkWidget* widget,
                                   GdkDragContext* drag_context,
                                   GtkListStore* list )
{
    // Regenerating archive handlers list xset
    // Obtaining iterator pointing at first handler
    GtkTreeIter iter;
    if (!gtk_tree_model_get_iter_first( GTK_TREE_MODEL( list ), &iter ))
    {
        // Failed to get iterator - warning user and exiting
        g_warning("Drag'n'drop end event detected, but unable to get an"
        " iterator to the start of the model!");
        return;
    }

    // Looping for all handlers
    gchar* handler_name_unused;  // Not actually used...
    gchar* xset_name;
    gchar* archive_handlers = g_strdup( "" );
    gchar* archive_handlers_temp;
    do
    {
        // Fetching data from the model based on the iterator. Note that
        // this variable used for the G_STRING is defined on the stack,
        // so should be freed for me
        gtk_tree_model_get( GTK_TREE_MODEL( list ), &iter,
                            COL_XSET_NAME, &xset_name,
                            COL_HANDLER_NAME, &handler_name_unused,
                            -1 );

        archive_handlers_temp = archive_handlers;
        if (g_strcmp0( archive_handlers, "" ) == 0)
        {
            archive_handlers = g_strdup( xset_name );
        }
        else
        {
            archive_handlers = g_strdup_printf( "%s %s",
                archive_handlers, xset_name );
        }
        g_free(archive_handlers_temp);
    }
    while(gtk_tree_model_iter_next( GTK_TREE_MODEL( list ), &iter ));

    // Saving the new archive handlers list
    xset_set( "arc_conf2", "s", archive_handlers );
    g_free(archive_handlers);

    // Ensuring first handler is selected (otherwise none are)
    GtkTreeSelection *selection = gtk_tree_view_get_selection(
                                        GTK_TREE_VIEW( widget ) );
    GtkTreePath *new_path = gtk_tree_path_new_first();
    gtk_tree_selection_select_path( GTK_TREE_SELECTION( selection ),
                            new_path );
    gtk_tree_path_free( new_path );
}

static void on_configure_handler_enabled_check( GtkToggleButton *togglebutton,
    gpointer user_data )
{
    // Fetching current status
    gboolean enabled = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON ( togglebutton ) );

    // Getting at widgets
    GtkWidget* entry_handler_name = (GtkWidget*)g_object_get_data(
                                            G_OBJECT( user_data ),
                                                "entry_handler_name" );
    GtkWidget* entry_handler_mime = (GtkWidget*)g_object_get_data( G_OBJECT( user_data ),
                                                "entry_handler_mime" );
    GtkWidget* entry_handler_extension = (GtkWidget*)g_object_get_data( G_OBJECT( user_data ),
                                            "entry_handler_extension" );
    GtkWidget* entry_handler_compress = (GtkWidget*)g_object_get_data( G_OBJECT( user_data ),
                                            "entry_handler_compress" );
    GtkWidget* entry_handler_extract = (GtkWidget*)g_object_get_data( G_OBJECT( user_data ),
                                            "entry_handler_extract" );
    GtkWidget* entry_handler_list = (GtkWidget*)g_object_get_data( G_OBJECT( user_data ),
                                                "entry_handler_list" );
    GtkWidget* chkbtn_handler_compress_term = (GtkWidget*)g_object_get_data( G_OBJECT( user_data ),
                                        "chkbtn_handler_compress_term" );
    GtkWidget* chkbtn_handler_extract_term = (GtkWidget*)g_object_get_data( G_OBJECT( user_data ),
                                        "chkbtn_handler_extract_term" );
    GtkWidget* chkbtn_handler_list_term = (GtkWidget*)g_object_get_data( G_OBJECT( user_data ),
                                            "chkbtn_handler_list_term" );

    // Setting sensitive/insensitive various widgets as appropriate
    gtk_widget_set_sensitive( GTK_WIDGET( entry_handler_name ), enabled );
    gtk_widget_set_sensitive( GTK_WIDGET( entry_handler_mime ), enabled );
    gtk_widget_set_sensitive( GTK_WIDGET( entry_handler_extension ), enabled );
    gtk_widget_set_sensitive( GTK_WIDGET( entry_handler_compress ), enabled );
    gtk_widget_set_sensitive( GTK_WIDGET( entry_handler_extract ), enabled );
    gtk_widget_set_sensitive( GTK_WIDGET( entry_handler_list ), enabled );
    gtk_widget_set_sensitive( GTK_WIDGET( chkbtn_handler_compress_term ), enabled );
    gtk_widget_set_sensitive( GTK_WIDGET( chkbtn_handler_extract_term ), enabled );
    gtk_widget_set_sensitive( GTK_WIDGET( chkbtn_handler_list_term ), enabled );
}

static void on_configure_row_activated( GtkTreeView* view,
                                        GtkTreePath* tree_path,
                                        GtkTreeViewColumn* col,
                                        GtkWidget* dlg )
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

static void populate_archive_handlers( GtkListStore* list, GtkWidget* dlg )
{
    // Fetching available archive handlers (literally gets member s from
    // the xset) - user-defined order has already been set
    char* archive_handlers_s = xset_get_s( "arc_conf2" );
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
    gchar* xset_name, *extensions, *extension;
    do
    {
        gtk_tree_model_get( GTK_TREE_MODEL( list ), &iter,
                            COL_XSET_NAME, &xset_name,
                            COL_HANDLER_EXTENSIONS, &extensions,
                            -1 );
        handler_xset = xset_get( xset_name );

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
    }
    while(gtk_tree_model_iter_next( GTK_TREE_MODEL( list ), &iter ));

    // Cropping current extension if found
    if (len)
    {
        len = strlen( name ) - len;
        name[len] = '\0';
    }

    // Clearing up
    g_free(extension);

    // Getting at currently selected archive handler
    gtk_combo_box_get_active_iter( GTK_COMBO_BOX( combo ), &iter );

    // You have to fetch both items here
    gtk_tree_model_get( GTK_TREE_MODEL( list ), &iter,
                        COL_XSET_NAME, &xset_name,
                        COL_HANDLER_EXTENSIONS, &extensions,
                        -1 );
    handler_xset = xset_get( xset_name );

    // Obtaining archive extension
    extension = archive_handler_get_first_extension(handler_xset);

    // Appending extension to original filename
    new_name = g_strconcat( name, extension, NULL );

    // Updating new archive filename
    gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER( dlg ), new_name );

    // Cleaning up
    g_free( name );
    g_free( extension );
    g_free( new_name );

    // Fetching reference to entry in dialog
    GtkEntry* entry = (GtkEntry*)g_object_get_data( G_OBJECT( dlg ), "entry" );

    // Updating command
    // Dealing with '+' representing running in terminal
    gchar* compress_cmd;
    if ( handler_xset->y && handler_xset->y[0] == '+' )
    {
        compress_cmd = g_strdup( handler_xset->y + 1 );
    }
    else
    {
        compress_cmd = g_strdup( handler_xset->y );
    }
    gtk_entry_set_text( GTK_ENTRY( entry ), compress_cmd );
    g_free(compress_cmd);
}

/*
static void on_format_changed( GtkComboBox* combo,
                                      gpointer user_data )
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
*/

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
            set = xset_get( "arc_conf2" );
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

    // Forcing dialog icon - WM breaks without this for IG, not needed
    // for me
    xset_set_window_icon( GTK_WINDOW( dlg ) );

    // Setting saved dialog size
    int width = xset_get_int( "arc_conf2", "x" );
    int height = xset_get_int( "arc_conf2", "y" );
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
                                                _("Re_store Defaults"),
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

    // Enabling item reordering (GTK-handled drag'n'drop)
    gtk_tree_view_set_reorderable( GTK_TREE_VIEW( view_handlers ), TRUE );

    // Connecting treeview callbacks
    g_signal_connect( G_OBJECT( view_handlers ), "drag-end",
                        G_CALLBACK( on_configure_drag_end ),
                        GTK_LIST_STORE( list ) );
    g_signal_connect( G_OBJECT( view_handlers ), "row-activated",
                        G_CALLBACK( on_configure_row_activated ),
                        GTK_WIDGET( dlg ) );
    g_signal_connect( G_OBJECT( gtk_tree_view_get_selection(
                                    GTK_TREE_VIEW( view_handlers ) ) ),
                        "changed",
                        G_CALLBACK( on_configure_changed ),
                        GTK_WIDGET( dlg ) );

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
    gtk_widget_set_sensitive( GTK_WIDGET( btn_remove ), FALSE );
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
    gtk_widget_set_sensitive( GTK_WIDGET( btn_apply ), FALSE );
    g_signal_connect( G_OBJECT( btn_apply ), "clicked",
                        G_CALLBACK( on_configure_button_press ), dlg );
    g_object_set_data( G_OBJECT( dlg ), "btn_apply", GTK_BUTTON( btn_apply ) );

    // Generating right-hand side of dialog
    GtkWidget* chkbtn_handler_enabled = gtk_check_button_new_with_label( _("Handler Enabled") );
    g_signal_connect( G_OBJECT( chkbtn_handler_enabled ), "toggled",
                G_CALLBACK ( on_configure_handler_enabled_check ), dlg );
    g_object_set_data( G_OBJECT( dlg ), "chkbtn_handler_enabled",
                        GTK_CHECK_BUTTON( chkbtn_handler_enabled ) );
    GtkWidget* lbl_handler_name = gtk_label_new( NULL );
    gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_handler_name ),
                                        _("_Name:") ),
    gtk_misc_set_alignment( GTK_MISC( lbl_handler_name ), 0, 0.5 );
    GtkWidget* lbl_handler_mime = gtk_label_new( NULL );
    gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_handler_mime ),
                                        _("MIME _Type:") );
    gtk_misc_set_alignment( GTK_MISC( lbl_handler_mime ), 0, 0.5 );
    GtkWidget* lbl_handler_extension = gtk_label_new( NULL );
    gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_handler_extension ),
                                        _("E_xtension:") );
    gtk_misc_set_alignment( GTK_MISC( lbl_handler_extension ), 0, 0.5 );
    GtkWidget* lbl_handler_compress = gtk_label_new( NULL );
    gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_handler_compress ),
                                        _("<b>Co_mpress:</b>") );
    gtk_misc_set_alignment( GTK_MISC( lbl_handler_compress ), 0, 0.5 );
    GtkWidget* lbl_handler_extract = gtk_label_new( NULL );
    gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_handler_extract ),
                                        _("<b>_Extract:</b>") );
    gtk_misc_set_alignment( GTK_MISC( lbl_handler_extract ), 0, 0.5 );
    GtkWidget* lbl_handler_list = gtk_label_new( NULL );
    gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_handler_list ),
                                        _("<b>_List:</b>") );
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

    // Setting widgets to be activated associated with label mnemonics
    gtk_label_set_mnemonic_widget( GTK_LABEL( lbl_handler_name ),
                                   entry_handler_name );
    gtk_label_set_mnemonic_widget( GTK_LABEL( lbl_handler_mime ),
                                   entry_handler_mime );
    gtk_label_set_mnemonic_widget( GTK_LABEL( lbl_handler_extension ),
                                   entry_handler_extension );
    gtk_label_set_mnemonic_widget( GTK_LABEL( lbl_handler_compress ),
                                   entry_handler_compress );
    gtk_label_set_mnemonic_widget( GTK_LABEL( lbl_handler_extract ),
                                   entry_handler_extract );
    gtk_label_set_mnemonic_widget( GTK_LABEL( lbl_handler_list ),
                                   entry_handler_list );

    GtkWidget* chkbtn_handler_compress_term =
                gtk_check_button_new_with_label( _("Run In Terminal") );
    g_object_set_data( G_OBJECT( dlg ), "chkbtn_handler_compress_term",
                        GTK_CHECK_BUTTON( chkbtn_handler_compress_term ) );
    GtkWidget* chkbtn_handler_extract_term =
                gtk_check_button_new_with_label( _("Run In Terminal") );
    g_object_set_data( G_OBJECT( dlg ), "chkbtn_handler_extract_term",
                        GTK_CHECK_BUTTON( chkbtn_handler_extract_term ) );
    GtkWidget* chkbtn_handler_list_term =
                gtk_check_button_new_with_label( _("Run In Terminal") );
    g_object_set_data( G_OBJECT( dlg ), "chkbtn_handler_list_term",
                        GTK_CHECK_BUTTON( chkbtn_handler_list_term ) );

    // Creating container boxes - at this point the dialog already comes
    // with one GtkVBox then inside that a GtkHButtonBox
    // For the right side of the dialog, standard GtkBox approach fails
    // to allow precise padding of labels to allow all entries to line up
    // - so reimplementing with GtkTable. Would many GtkAlignments have
    // worked?
    GtkWidget* hbox_main = gtk_hbox_new( FALSE, 4 );
    GtkWidget* vbox_handlers = gtk_vbox_new( FALSE, 4 );
    GtkWidget* hbox_view_buttons = gtk_hbox_new( FALSE, 4 );
    GtkWidget* tbl_settings = gtk_table_new( 11, 3 , FALSE );

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
                        GTK_WIDGET( chkbtn_handler_enabled ), 0, 1, 1, 2,
                        GTK_FILL, GTK_FILL, 0, 0 );

    gtk_table_set_row_spacing( GTK_TABLE( tbl_settings ), 1, 5 );

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

    gtk_table_set_row_spacing( GTK_TABLE( tbl_settings ), 5, 10 );

    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( lbl_handler_compress ), 0, 1, 6, 7,
                        GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( chkbtn_handler_compress_term ), 1, 4, 6, 7,
                        GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( entry_handler_compress ), 0, 4, 7, 8,
                        GTK_FILL, GTK_FILL, 0, 0 );

    gtk_table_set_row_spacing( GTK_TABLE( tbl_settings ), 7, 5 );

    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( lbl_handler_extract ), 0, 1, 8, 9,
                        GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( chkbtn_handler_extract_term ), 1, 4, 8, 9,
                        GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( entry_handler_extract ), 0, 4, 9, 10,
                        GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0 );

    gtk_table_set_row_spacing( GTK_TABLE( tbl_settings ), 9, 5 );

    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( lbl_handler_list ), 0, 1, 10, 11,
                        GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( chkbtn_handler_list_term ), 1, 4, 10, 11,
                        GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( entry_handler_list ), 0, 4, 11, 12,
                        GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0 );

    // Packing boxes into dialog with padding to separate from dialog's
    // standard buttons at the bottom
    gtk_box_pack_start(
                GTK_BOX(
                    gtk_dialog_get_content_area( GTK_DIALOG( dlg ) )
                ),
                GTK_WIDGET( hbox_main ), TRUE, TRUE, 4 );

    // Adding archive handlers to list
    populate_archive_handlers( GTK_LIST_STORE( list ), GTK_WIDGET( dlg ) );

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
        xset_set( "arc_conf2", "x", str );
        g_free( str );
        str = g_strdup_printf( "%d", height );
        xset_set( "arc_conf2", "y", str );
        g_free( str );
    }

    // Clearing up dialog
    gtk_widget_destroy( dlg );
}

/*
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
*/

void ptk_file_archiver_create( PtkFileBrowser* file_browser, GList* files,
                               const char* cwd )
{
    GList *l;
    GtkWidget* combo, *dlg, *hbox;
    GtkFileFilter* filter;
    char* cmd = NULL, *cmd_to_run = NULL, *desc = NULL, *dest_file = NULL,
        *ext = NULL, *s1 = NULL, *str = NULL, *udest_file = NULL,
        *archive_name = NULL, *final_command = NULL;
    int i, n, format, res;

    // Generating dialog
    dlg = gtk_file_chooser_dialog_new( _("Create Archive"),
                                       GTK_WINDOW( gtk_widget_get_toplevel(
                                             GTK_WIDGET( file_browser ) ) ),
                                       GTK_FILE_CHOOSER_ACTION_SAVE, NULL );

    /* Adding standard buttons and saving references in the dialog
     * (GTK doesnt provide a trivial way to reference child widgets from
     * the window!!)
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
    hbox = gtk_hbox_new( FALSE, 4 );
    GtkWidget* lbl_archive_format = gtk_label_new( NULL );
    gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_archive_format ),
                                         _("_Archive Format:") );
    gtk_box_pack_start( GTK_BOX(hbox), lbl_archive_format,
                        FALSE, TRUE, 2 );

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
        ptk_file_archiver_config( file_browser );
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
    for (i = 0; archive_handlers[i] != NULL; ++i)
    {
        // Fetching handler
        handler_xset = xset_get( archive_handlers[i] );

        /* Checking to see if handler is enabled, can cope with
         * compression and the extension is set - dealing with empty
         * command yet 'run in terminal' still ticked */
        if(handler_xset->b == XSET_B_TRUE && handler_xset->y
           && g_strcmp0( handler_xset->y, "" ) != 0
           && g_strcmp0( handler_xset->y, "+" ) != 0
           && g_strcmp0( handler_xset->x, "" ) != 0)
        {
            /* It can - adding to filter so that only relevant archives
             * are displayed when the user chooses an archive name to
             * create. Note that the handler may be responsible for
             * multiple MIME types and extensions */
            gtk_file_filter_add_mime_type( filter, handler_xset->s );

            // Appending to combobox
            // Obtaining appending iterator for model
            gtk_list_store_append( GTK_LIST_STORE( list ), &iter );

            // Adding to model
            xset_name = g_strdup( archive_handlers[i] );
            extensions = g_strconcat( handler_xset->menu_label, " (",
                                      handler_xset->x, ")", NULL );
            gtk_list_store_set( GTK_LIST_STORE( list ), &iter,
                                COL_XSET_NAME, xset_name,
                                COL_HANDLER_EXTENSIONS, extensions,
                                -1 );
        }
    }

    // Clearing up archive_handlers
    g_strfreev( archive_handlers );

    // Applying filter
    gtk_file_chooser_set_filter( GTK_FILE_CHOOSER( dlg ), filter );

    // Restoring previous selected handler
    n = gtk_tree_model_iter_n_children( gtk_combo_box_get_model(
                                            GTK_COMBO_BOX( combo )
                                        ), NULL );
    i = xset_get_int( "arc_dlg", "z" );
    if ( i < 0 || i > n - 1 )
        i = 0;
    gtk_combo_box_set_active( GTK_COMBO_BOX( combo ), i );

    // Adding filter box to hbox and connecting callback
    g_signal_connect( combo, "changed", G_CALLBACK( on_format_changed ),
                      dlg );
    GtkWidget *lbl_command = gtk_label_new( NULL );
    gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_command ),
                                        _("Co_mmand:") );
    gtk_box_pack_start( GTK_BOX( hbox ), combo, FALSE, FALSE, 2 );
    gtk_box_pack_start( GTK_BOX( hbox ), lbl_command, FALSE, FALSE, 2 );

    // Loading command for handler, based off the i'th handler
    GtkEntry* entry = (GtkEntry*)gtk_entry_new();

    // Obtaining iterator from string turned into a path into the model
    gchar* compress_cmd;
    if(gtk_tree_model_get_iter_from_string( GTK_TREE_MODEL( list ),
                                    &iter, g_strdup_printf( "%d", i ) ))
    {
        // You have to fetch both items here
        gtk_tree_model_get( GTK_TREE_MODEL( list ), &iter,
                            COL_XSET_NAME, &xset_name,
                            COL_HANDLER_EXTENSIONS, &extensions,
                            -1 );
        handler_xset = xset_get( xset_name );

        // Dealing with '+' representing running in terminal
        if ( handler_xset->y && handler_xset->y[0] == '+' )
        {
            compress_cmd = g_strdup( handler_xset->y + 1 );
        }
        else
        {
            compress_cmd = g_strdup( handler_xset->y );
        }
        gtk_entry_set_text( GTK_ENTRY( entry ), compress_cmd );
        g_free(compress_cmd);
        compress_cmd = NULL;
    }
    else
    {
        // Recording the fact getting the iter failed
        g_warning( "Unable to fetch the iter from handler ordinal %d!", i );
    };

    // Mnemonically attaching widgets to labels
    gtk_label_set_mnemonic_widget( GTK_LABEL( lbl_archive_format ),
                                   combo );
    gtk_label_set_mnemonic_widget( GTK_LABEL( lbl_command ),
                                   GTK_WIDGET( entry ) );

    // Adding options to hbox
    gtk_box_pack_start( GTK_BOX( hbox ), GTK_WIDGET( entry ), TRUE,
                        TRUE, 4 );
    g_object_set_data( G_OBJECT( dlg ), "entry", entry );

    // Adding hbox to dialog at bottom
    gtk_widget_show_all( hbox );
    gtk_box_pack_start( GTK_BOX( gtk_dialog_get_content_area (
                                    GTK_DIALOG( dlg )
                                ) ),
                                hbox, FALSE, TRUE, 0 );

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
        g_free( ext );

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
    gchar* command;
    gboolean run_in_terminal;

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
                return;
            }

            // Fetching model data
            gtk_tree_model_get( GTK_TREE_MODEL( list ), &iter,
                                COL_XSET_NAME, &xset_name,
                                COL_HANDLER_EXTENSIONS, &extensions,
                                -1 );
            handler_xset = xset_get( xset_name );

            // Fetching normal compression command and whether it should
            // run in the terminal or not
            if ( handler_xset->y && handler_xset->y[0] == '+' )
            {
                compress_cmd = g_strdup( handler_xset->y + 1 );
                run_in_terminal = TRUE;
            }
            else
            {
                compress_cmd = g_strdup( handler_xset->y );
                run_in_terminal = FALSE;
            }

            // Fetching user-selected handler data
            format = gtk_combo_box_get_active( GTK_COMBO_BOX( combo ) );
            command = g_strdup( gtk_entry_get_text( entry ) );

            // Saving selected archive handler ordinal
            str = g_strdup_printf( "%d", format );
            xset_set( "arc_dlg", "z", str );
            g_free( str );

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
                    !g_strstr_len( command, -1, "%f" ) &&
                    !g_strstr_len( command, -1, "%F" )
                )
            )
            {
                // It has/is - warning user
                xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_WARNING,
                                _("Create Archive"), NULL, FALSE,
                                _("The following substitution variables "
                                "should be in the archive creation"
                                " command:\n\n"
                                "One of the following:\n\n"
                                "%%%%f: First selected file/directory to"
                                " archive\n"
                                "%%%%F: All selected files/directories to"
                                " archive\n\n"
                                "and one of the following:\n\n"
                                "%%%%o: Resulting single archive\n"
                                "%%%%O: Resulting archive per source "
                                "file/directory (see %%%%f/%%%%F)\n\n"
                                "Continuing anyway"),
                                NULL, NULL );
                gtk_widget_grab_focus( GTK_WIDGET( entry ) );
            }

            // Checking to see if the archive handler compression command
            // has changed
            if (g_strcmp0( command, compress_cmd ) != 0)
            {
                // It has - saving, taking into account running in
                // terminal
                xset_set_set( handler_xset, "y",
    (run_in_terminal) ? g_strconcat( "+", command, NULL ) : command );
            }

            // Cleaning up
            g_free( compress_cmd );

            // Exiting loop
            break;
        }
        else if ( res == GTK_RESPONSE_NONE )
        {
            /* User wants to configure archive handlers - call up the
             * config dialog then exit, as this dialog would need to be
             * reconstructed if changes occur */
            gtk_widget_destroy( dlg );
            ptk_file_archiver_config( file_browser );
            return;
        }
        else if ( res == GTK_RESPONSE_HELP )
        {
            // TODO: Sort out proper help
            xset_show_help( dlg, NULL, "#designmode-style-context" );
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

    // Dealing with separate archives for each source file/directory ('%O')
    if (g_strstr_len( command, -1, "%O" ))
    {
        /* '%O' is present - the archiving command should be generated
         * and ran for each individual file */

        // Fetching extension
        ext = archive_handler_get_first_extension( handler_xset );

        /* Looping for all selected files/directories - all are used
         * when '%F' is present, only the first otherwise */
        for( i = 0, l = files;
             l && ( i == 0 || g_strstr_len( command, -1, "%F" ) );
             l = l->next, ++i)
        {
            // FIXME: Maybe we should consider filename encoding here.
            desc = (char *) vfs_file_info_get_name(
                                            (VFSFileInfo*) l->data );

            /* In %O mode, every source file is output to its own archive,
             * so the resulting archive name is based on the filename and
             * substituted every time */

            // Obtaining archive name, quoting and substituting
            archive_name = g_strconcat( desc, ext, NULL );
            s1 = archive_name;
            archive_name = bash_quote( archive_name );
            g_free(s1);
            cmd = replace_string( command, "%O", archive_name, FALSE );
            g_free(archive_name);

            /* Bash quoting desc - desc original value comes from the
             * VFSFileInfo struct and therefore should not be freed */
            desc = bash_quote( desc );

            // Detecting first run
            if (i == 0)
            {
                // Replacing out %f with 1st file
                s1 = cmd;
                cmd = replace_string( cmd, "%f", desc, FALSE );
                g_free(s1);

                // Removing %f from command - its used up
                s1 = command;
                command = replace_string( command, "%f", "", FALSE );
                g_free(s1);
            }

            // Replacing out %F with nth file (NOT ALL FILES)
            cmd_to_run = replace_string( cmd, "%F", desc, FALSE );

            // Dealing with remaining standard SpaceFM substitutions
            s1 = cmd_to_run;
            cmd_to_run = replace_line_subs( cmd_to_run );
            g_free(s1);

            // Appending to final command as appropriate
            if (i == 0)
                final_command = g_strdup( cmd_to_run );
            else
            {
                s1 = final_command;
                final_command = g_strconcat( final_command, " && echo && ",
                                             cmd_to_run, NULL );
                g_free(s1);
            }

            // Cleaning up
            g_free( desc );
            g_free( cmd );
            g_free( cmd_to_run );
        }
    }
    else
    {
        /* '%O' isn't present - the normal single command is needed
         * Obtaining valid quoted UTF8 file name for archive to create */
        udest_file = g_filename_display_name( dest_file );
        g_free( dest_file );
        char* udest_quote = bash_quote( udest_file );
        g_free( udest_file );

        // Inserting archive name into appropriate place in command
        final_command = replace_string( command, "%o", udest_quote,
                                        FALSE );
        g_free( udest_quote );

        if (files)
        {
            desc = bash_quote( (char *) vfs_file_info_get_name(
                                            (VFSFileInfo*) files->data ) );

            // Dealing with first selected file substitution
            s1 = final_command;
            final_command = replace_string( final_command, "%f", desc,
                                            FALSE );
            g_free(s1);

            /* Generating string of selected files/directories to archive if
             * %F is present */
            if (g_strstr_len( final_command, -1, "%F" ))
            {
                s1 = g_strdup( "" );
                for( l = files; l; l = l->next)
                {
                    // FIXME: Maybe we should consider filename encoding here.
                    str = s1;
                    desc = bash_quote( (char *) vfs_file_info_get_name(
                                                    (VFSFileInfo*) l->data ) );
                    if (g_strcmp0( s1, "" ) <= 0)
                    {
                        s1 = g_strdup( desc );
                    }
                    else
                    {
                        s1 = g_strdup_printf( "%s %s", s1, desc );
                    }
                    g_free( desc );
                    g_free( str );
                }

                str = final_command;
                final_command = replace_string( final_command, "%F", s1,
                                                FALSE );

                // Cleaning up
                g_free( s1 );
                g_free( str );
            }
        }

        // Dealing with remaining standard SpaceFM substitutions
        s1 = final_command;
        final_command = replace_line_subs( final_command );
        g_free(s1);
    }

    /* Cleaning up - final_command does not need freeing, as this
     * remains pointing to data in the task */
    g_free( command );

    /* When ran in a terminal, adding code to warn user on failure and
     * to keep the terminal open */
    if (run_in_terminal)
    {
        s1 = final_command;
        final_command = g_strdup_printf( "%s ; fm_err=$?; if [ $fm_err -ne 0 ]; then echo; echo -n '%s: '; read s; exit $fm_err; fi",
                                         s1,
                                         "[ Finished With Errors ]  Press Enter to close" );
        g_free( s1 );
    }

    // Creating task
    char* task_name = g_strdup_printf( _("Archive") );
    PtkFileTask* task = ptk_file_exec_new( task_name, cwd,
                                           GTK_WIDGET( file_browser ),
                                           file_browser->task_view );
    g_free( task_name );
    task->task->exec_browser = file_browser;

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

void ptk_file_archiver_extract( PtkFileBrowser* file_browser, GList* files,
                                            const char* cwd, const char* dest_dir )
{
    /* Note that it seems this function is also used to list the contents
     * of archives! */

    GtkWidget* dlg;
    GtkWidget* dlgparent = NULL;
    char* choose_dir = NULL;
    gboolean create_parent, in_term, keep_term, write_access;
    gboolean list_contents = FALSE;
    VFSFileInfo* file;
    VFSMimeType* mime;
    const char *dest, *type;
    GList* l;
    char *dest_quote = NULL, *full_path = NULL, *full_quote = NULL,
        *mkparent = NULL, *perm = NULL, *prompt = NULL, *name = NULL,
        *extension = NULL, *cmd = NULL, *str = NULL, *final_command = NULL;
    int i, n, j;
    struct stat64 statbuf;

    // Making sure files to act on have been passed
    if( !files )
        return;

    // Determining parent of dialog
    if ( file_browser )
        dlgparent = gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) );
    //else if ( desktop )
    //    dlgparent = gtk_widget_get_toplevel( desktop );  // causes drag action???

    // Checking if destination directory hasn't been specified
    if( !dest_dir )
    {
        // It hasn't - generating dialog to ask user. Only dealing with
        // user-writable contents if the user isn't root
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
                                                    _("Make contents "
                                                    "user-_writable") );
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chk_write ),
                                xset_get_int( "arc_dlg", "s" ) == 1 &&
                                geteuid() != 0 );
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
        if( gtk_dialog_run( GTK_DIALOG(dlg) ) == GTK_RESPONSE_OK )
        {
            // User OK'd - saving dialog dimensions
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

            // Fetching user-specified settings and saving
            choose_dir = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER( dlg ) );
            create_parent = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( chk_parent ) );
            write_access = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( chk_write ) );
            xset_set_b( "arc_dlg", create_parent );
            str = g_strdup_printf( "%d", write_access ? 1 : 0 );
            xset_set( "arc_dlg", "s", str );
            g_free( str );
          }

        // Destroying dialog
        gtk_widget_destroy( dlg );

        // Exiting if user didnt choose an extraction directory
        if( !choose_dir )
            return;

        // This DOES NOT need to be freed by me despite the documentation
        // saying so! Otherwise enjoy ur double free
        dest = choose_dir;
    }
    else
    {
        // Extraction directory specified - loading defaults
        create_parent = xset_get_b( "arc_def_parent" );
        write_access = xset_get_b( "arc_def_write" );

        // Detecting whether this function call is actually to list the
        // contents of the archive or not...
        list_contents = !strcmp( dest_dir, "////LIST" );

        dest = dest_dir;
    }

    /* Quoting destination directory (doing this outside of the later
     * loop as its needed after the selected files loop completes) */
    dest_quote = bash_quote( dest );

    // Fetching available archive handlers and splitting
    char* archive_handlers_s = xset_get_s( "arc_conf2" );
    gchar** archive_handlers = g_strsplit( archive_handlers_s, " ", -1 );
    XSet* handler_xset;

    /* Setting desired archive operation and keeping in terminal while
     * listing */
    int archive_operation = (list_contents) ? ARC_LIST : ARC_EXTRACT;
    keep_term = list_contents;

    // Looping for all files to attempt to list/extract
    for ( l = files; l; l = l->next )
    {
        gboolean format_supported = FALSE;

        // Fetching file details
        file = (VFSFileInfo*)l->data;
        mime = vfs_file_info_get_mime_type( file );
        type = vfs_mime_type_get_type( mime );
        name = get_name_extension( (char*)vfs_file_info_get_name( file ),
                                   FALSE, &extension );

        // Looping for handlers (NULL-terminated list)
        for (i = 0; archive_handlers[i] != NULL; ++i)
        {
            // Fetching handler
            handler_xset = xset_get( archive_handlers[i] );

            // Checking to see if handler is enabled and can cope with
            // extraction/listing
            if(archive_handler_is_format_supported( handler_xset, type,
                                                    extension,
                                                    archive_operation ))
            {
                // It can - setting flag and leaving loop
                format_supported = TRUE;
                break;
            }
        }

        // Cleaning up
        g_free( name );
        g_free( extension );

        // Continuing to next file if a handler hasnt been found
        if (!format_supported)
            continue;

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

        // Determining file paths
        full_path = g_build_filename( cwd, vfs_file_info_get_name( file ),
                                      NULL );
        full_quote = bash_quote( full_path );

        // Checking if the operation is to list an archive(s)
        if ( list_contents )
        {
            // It is - generating appropriate command, dealing with 'run
            // in terminal'
            cmd = replace_string( (*handler_xset->context == '+') ?
                    handler_xset->context + 1 : handler_xset->context,
                    "%o", full_quote, FALSE );
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
            gchar** extensions = g_strsplit( handler_xset->x, ":", -1 );
            for (i = 0; extensions[i] != NULL; ++i)
            {
                // Debug code
                //g_message( "extensions[i]: %s", extensions[i]);

                // Checking if the current extension is being used
                if (g_str_has_suffix( filename, extensions[i] ))
                {
                    // It is - determining filename without extension
                    // and breaking
                    n = strlen( filename ) - strlen( extensions[i] );
                    filename[n] = '\0';
                    filename_no_archive_ext = g_strdup( filename );
                    filename[n] = '.';
                    break;
                }
            }

            // Clearing up
            g_strfreev( extensions );

            // Making sure extension has been found, moving to next file
            // otherwise
            if (filename_no_archive_ext == NULL)
            {
                g_warning( "Unable to process '%s' - does not use an "
                           "extension registered with the '%s' archive "
                           "handler!", filename, handler_xset->menu_label);

                // Cleaning up
                g_free( filename );
                g_free( full_quote );
                g_free( full_path );
                continue;
            }

            /* Now the extraction filename is obtained, determine the
             * normal filename without the extension */
            gchar *filename_no_ext = get_name_extension( filename_no_archive_ext,
                                                         FALSE,
                                                         &extension );

            /* 'Completing' the extension and dealing with files with
             * no extension */
            if (!extension)
                extension = g_strdup( "" );
            else
            {
                str = extension;
                extension = g_strconcat( ".", extension, NULL );
                g_free( str );
            }

            // Cleaning up
            g_free( filename );

            /* Determining extraction command - dealing with 'run in
             * terminal' and placeholders. Doing this here as parent
             * directory creation needs access to the command */
            gchar* extract_cmd = replace_string(
                (*handler_xset->z == '+') ? handler_xset->z + 1 : handler_xset->z,
                "%o", full_quote, FALSE );

            /* Dealing with creation of parent directory if needed -
             * never create a parent directory if %F is used - this is
             * an override substitution for the sake of gzip */
            gchar* parent_path = NULL;
            if (create_parent && !g_strstr_len( extract_cmd, -1, "%F" ))
            {
                /* Determining full path of parent directory to make
                 * (also used later in %f substitution) */
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
                char* parent_quote = bash_quote( parent_path );
                mkparent = g_strdup_printf( "mkdir -p %s && cd %s && ",
                                            parent_quote, parent_quote );

                // Cleaning up (parent_path is used later)
                g_free( parent_quote );
            }
            else
            {
                // Parent directory doesn't need to be created
                mkparent = g_strdup( "" );
                parent_path = g_strdup( "" );
                create_parent = FALSE;

                /* Making sure any '%F's turn into normal '%f's now
                 * they've played their role */
                gchar* old_extract_cmd = extract_cmd;
                extract_cmd = replace_string( extract_cmd, "%F", "%f",
                                               FALSE );
                g_free( old_extract_cmd );
            }

            // Debug code
            //g_message( "full_quote: %s\ndest: %s", full_quote, dest );

            /* Singular file extraction target (e.g. stdout-redirected
             * gzip) */
            if (g_strstr_len( extract_cmd, -1, "%f" ))
            {
                /* Creating extraction target, taking into account whether
                 * a parent directory has been created or not - target is
                 * guaranteed not to exist so as to avoid overwriting */
                gchar* extract_target = g_build_filename(
                                    (create_parent) ? parent_path : dest,
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
                                    (create_parent) ? parent_path : dest,
                                    str, NULL );
                    g_free( str );
                }

                // Quoting and substituting command
                gchar* extract_target_quote = bash_quote( extract_target );
                gchar* old_extract_cmd = extract_cmd;
                extract_cmd = replace_string( extract_cmd, "%f",
                                              extract_target_quote, FALSE );
                g_free( extract_target_quote );
                g_free( old_extract_cmd );
                g_free( extract_target );
            }

            // Finally constructing command to run
            cmd = g_strdup_printf( "cd %s && %s%s", dest_quote,
                                   mkparent, extract_cmd );

            // Cleaning up
            g_free( extract_cmd );
            g_free( filename_no_archive_ext );
            g_free( filename_no_ext );
            g_free( extension );
            g_free( mkparent );
            g_free( parent_path );
        }

        // Building up final_command
        if (!final_command)
            final_command = g_strdup( cmd );
        else
        {
            str = final_command;
            final_command = g_strconcat( final_command, " && echo && ",
                                         cmd, NULL );
            g_free( str );
        }

        // Cleaning up
        g_free( full_quote );
        g_free( full_path );
        g_free( cmd );
    }

    // Dealing with standard SpaceFM substitutions
    str = final_command;
    final_command = replace_line_subs( final_command );
    g_free(str);

    /* Generating prompt to keep terminal open after error if
     * appropriate */
    if (in_term)
        prompt = g_strdup_printf( "; fm_err=$?; if [ $fm_err -ne 0 ]; then echo; echo -n '%s: '; read s; exit $fm_err; fi", /* no translate for security */
                "[ Finished With Errors ]  Press Enter to close" );
    else
        prompt = g_strdup_printf( "" );

    /* Dealing with the need to make extracted files writable if
     * desired (e.g. a tar of files originally archived from a CD
     * will be readonly). Root users don't obey such access
     * permissions and making such owned files writeable may be a
     * security issue */
    if (!list_contents && write_access && geteuid() != 0)
        perm = g_strdup_printf( " && chmod -R u+rwX %s/*",
                                dest_quote );
    else perm = g_strdup( "" );

    // Finally constructing command to run
    str = final_command;
    final_command = g_strconcat( final_command, perm, prompt, NULL );
    g_free( str );
    g_free( perm );
    g_free( prompt );
    g_free( dest_quote );

    // Creating task
    char* task_name = g_strdup_printf( _("Extract %s"),
                                vfs_file_info_get_name( file ) );
    PtkFileTask* task = ptk_file_exec_new( task_name, cwd, dlgparent,
                    file_browser ? file_browser->task_view : NULL );
    g_free( task_name );

    // Configuring task
    task->task->exec_command = final_command;
    task->task->exec_browser = file_browser;
    task->task->exec_sync = !in_term;
    task->task->exec_show_error = TRUE;
    task->task->exec_show_output = in_term;
    task->task->exec_terminal = in_term;
    task->task->exec_keep_terminal = keep_term;
    task->task->exec_export = TRUE;  // Setup SpaceFM bash variables

    // Setting custom icon
    XSet* set = xset_get( "arc_extract" );
    if ( set->icon )
        task->task->exec_icon = g_strdup( set->icon );

    // Running task
    ptk_file_task_run( task );

    /* Clearing up - final_command does not need freeing, as this
     * remains pointing to data in the task */
    g_strfreev( archive_handlers );
    if ( choose_dir )
        g_free( choose_dir );
}

/*
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
                prompt = g_strdup_printf( " ; fm_err=$?; if [ $fm_err -ne 0 ]; then echo; echo -n '%s: '; read s; exit $fm_err; fi", /* no translate for security*//*
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
*/

/*
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
*/

gboolean ptk_file_archiver_is_format_supported( VFSMimeType* mime,
                                                char* extension,
                                                int operation )
{
    // Exiting if nothing was passed
    if (!mime && !extension)
        return FALSE;

    /* Operation doesnt need validation here - archive_handler_is_format_supported
     * takes care of this */
    
    // Fetching and validating MIME type if provided
    char *type = NULL;
    if (mime)
        type = (char*)vfs_mime_type_get_type( mime );

    // Fetching available archive handlers and splitting
    char* archive_handlers_s = xset_get_s( "arc_conf2" );
    gchar** archive_handlers = g_strsplit( archive_handlers_s, " ", -1 );

    // Debug code
    /*g_message("archive_handlers_s: %s\nextension: %s", archive_handlers_s,
              extension);*/

    // Looping for handlers (NULL-terminated list)
    int i;
    gboolean handler_found = FALSE;  // Flag variable used to ensure cleanup
    for (i = 0; archive_handlers[i] != NULL; ++i)
    {
        // Fetching handler
        XSet* handler_xset = xset_get( archive_handlers[i] );

        // Checking to see if handler can cope with format and operation
        if(archive_handler_is_format_supported( handler_xset, type,
                                                extension,
                                                operation ))
        {
            // It can - flagging and breaking
            handler_found = TRUE;
            break;
        }
    }

    // Clearing up archive_handlers
    g_strfreev( archive_handlers );

    // Returning result
    return handler_found;
}

static void restore_defaults( GtkWidget* dlg )
{
    // Note that defaults are also maintained in settings.c:xset_defaults

    // Exiting if the user doesn't really want to restore defaults
    gboolean overwrite_handlers;
    if (xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_WARNING,
                        _("Archive Handlers - Restore Defaults"), NULL,
                        GTK_BUTTONS_YES_NO, _("Do you want to overwrite"
                        " any existing default handlers? Missing handlers"
                        " will be added regardless."),
                        NULL, NULL) == GTK_RESPONSE_YES)
        overwrite_handlers = TRUE;
    else overwrite_handlers = FALSE;

    char *handlers_to_add = NULL, *str = NULL;

    /* Fetching current list of archive handlers, and constructing a
     * searchable version ensuring that a unique handler can be specified */
    char *handlers = xset_get_s( "arc_conf2" );
    char *handlers_search = NULL;
    if (g_strcmp0( handlers, "" ) <= 0)
        handlers_search = g_strdup( "" );
    else
        handlers_search = g_strconcat( " ", handlers, " ", NULL );

    gboolean handler_present = TRUE;
    XSet* set = xset_is( "arctype_7z" );
    if (!set)
    {
        set = xset_set( "arctype_7z", "label", "7-Zip" );
        handler_present = FALSE;
    }
    if (overwrite_handlers || !handler_present)
    {
        set->menu_label = g_strdup( "7-Zip" );
        set->b = XSET_B_TRUE;
        set->s = g_strdup( "application/x-7z-compressed" );
        set->x = g_strdup( ".7z" );
        set->y = g_strdup( "+\"$(which 7za || echo 7zr)\" a %o %F" );  // compress command
        set->z = g_strdup( "+\"$(which 7za || echo 7zr)\" x %o" );     // extract command
        set->context = g_strdup( "+\"$(which 7za || echo 7zr)\" l %o" );  // list command
    }

    /* Archive handler list check is separate as the XSet may exist but
     * the handler may not be in the list */
    if (!g_strstr_len( handlers_search, -1, " arctype_7z " ))
        handlers_to_add = g_strdup( "arctype_7z" );

    handler_present = TRUE;
    set = xset_is( "arctype_gz" );
    if (!set)
    {
        set = xset_set( "arctype_gz", "label", "Gzip" );
        handler_present = FALSE;
    }
    if (overwrite_handlers || !handler_present)
    {
        set->menu_label = g_strdup( "Gzip" );
        set->b = XSET_B_TRUE;
        set->s = g_strdup( "application/x-gzip:application/x-gzpdf" );
        set->x = g_strdup( ".gz" );
        set->y = g_strdup( "gzip -c %F > %O" );     // compress command
        set->z = g_strdup( "gzip -cd %o > %F" );    // extract command
        set->context = g_strdup( "+gunzip -l %o" );  // list command
    }

    /* Archive handler list check is separate as the XSet may exist but
     * the handler may not be in the list */
    if (!g_strstr_len( handlers_search, -1, " arctype_gz " ))
    {
        if (handlers_to_add)
        {
            str = handlers_to_add;
            handlers_to_add = g_strconcat( handlers_to_add,
                                           " arctype_gz", NULL );
            g_free( str );
        }
        else handlers_to_add = g_strdup( "arctype_gz" );
    }

    handler_present = TRUE;
    set = xset_is( "arctype_rar" );
    if (!set)
    {
        set = xset_set( "arctype_rar", "label", "RAR" );
        handler_present = FALSE;
    }
    if (overwrite_handlers || !handler_present)
    {
        set->menu_label = g_strdup( "RAR" );
        set->b = XSET_B_TRUE;
        set->s = g_strdup( "application/x-rar" );
        set->x = g_strdup( ".rar" );
        set->y = g_strdup( "+rar a -r %o %F" );     // compress command
        set->z = g_strdup( "+unrar -o- x %o" );     // extract command
        set->context = g_strdup( "+unrar lt %o" );  // list command
    }

    /* Archive handler list check is separate as the XSet may exist but
     * the handler may not be in the list */
    if (!g_strstr_len( handlers_search, -1, " arctype_rar " ))
    {
        if (handlers_to_add)
        {
            str = handlers_to_add;
            handlers_to_add = g_strconcat( handlers_to_add,
                                           " arctype_rar", NULL );
            g_free( str );
        }
        else handlers_to_add = g_strdup( "arctype_rar" );
    }

    handler_present = TRUE;
    set = xset_is( "arctype_tar" );
    if (!set)
    {
        set = xset_set( "arctype_tar", "label", "Tar" );
        handler_present = FALSE;
    }
    if (overwrite_handlers || !handler_present)
    {
        set->menu_label = g_strdup( "Tar" );
        set->b = XSET_B_TRUE;
        set->s = g_strdup( "application/x-tar" );
        set->x = g_strdup( ".tar" );
        set->y = g_strdup( "tar -cvf %o %F" );       // compress command
        set->z = g_strdup( "tar -xvf %o" );          // extract command
        set->context = g_strdup( "+tar -tvf %o" );   // list command
    }

    /* Archive handler list check is separate as the XSet may exist but
     * the handler may not be in the list */
    if (!g_strstr_len( handlers_search, -1, " arctype_tar " ))
    {
        if (handlers_to_add)
        {
            str = handlers_to_add;
            handlers_to_add = g_strconcat( handlers_to_add,
                                           " arctype_tar", NULL );
            g_free( str );
        }
        else handlers_to_add = g_strdup( "arctype_tar" );
    }

    handler_present = TRUE;
    set = xset_is( "arctype_tar_bz2" );
    if (!set)
    {
        set = xset_set( "arctype_tar_bz2", "label", "Tar (bzip2)" );
        handler_present = FALSE;
    }
    if (overwrite_handlers || !handler_present)
    {
        set->menu_label = g_strdup( "Tar bzip2" );
        set->b = XSET_B_TRUE;
        set->s = g_strdup( "application/x-bzip-compressed-tar" );
        set->x = g_strdup( ".tar.bz2" );
        set->y = g_strdup( "tar -cvjf %o %F" );       // compress command
        set->z = g_strdup( "tar -xvjf %o" );          // extract command
        set->context = g_strdup( "+tar -tvf %o" );    // list command
    }

    /* Archive handler list check is separate as the XSet may exist but
     * the handler may not be in the list */
    if (!g_strstr_len( handlers_search, -1, " arctype_tar_bz2 " ))
    {
        if (handlers_to_add)
        {
            str = handlers_to_add;
            handlers_to_add = g_strconcat( handlers_to_add,
                                           " arctype_tar_bz2", NULL );
            g_free( str );
        }
        else handlers_to_add = g_strdup( "arctype_tar_bz2" );
    }

    handler_present = TRUE;
    set = xset_is( "arctype_tar_gz" );
    if (!set)
    {
        set = xset_set( "arctype_tar_gz", "label", "Tar (gzip)" );
        handler_present = FALSE;
    }
    if (overwrite_handlers || !handler_present)
    {
        set->menu_label = g_strdup( "Tar Gzip" );
        set->b = XSET_B_TRUE;
        set->s = g_strdup( "application/x-compressed-tar" );
        set->x = g_strdup( ".tar.gz" );
        set->y = g_strdup( "tar -cvzf %o %F" );       // compress command
        set->z = g_strdup( "tar -xvzf %o" );          // extract command
        set->context = g_strdup( "+tar -tvf %o" );    // list command
    }

    /* Archive handler list check is separate as the XSet may exist but
     * the handler may not be in the list */
    if (!g_strstr_len( handlers_search, -1, " arctype_tar_gz " ))
    {
        if (handlers_to_add)
        {
            str = handlers_to_add;
            handlers_to_add = g_strconcat( handlers_to_add,
                                           " arctype_tar_gz", NULL );
            g_free( str );
        }
        else handlers_to_add = g_strdup( "arctype_tar_gz" );
    }

    handler_present = TRUE;
    set = xset_is( "arctype_tar_xz" );
    if (!set)
    {
        set = xset_set( "arctype_tar_xz", "label", "Tar (xz)" );
        handler_present = FALSE;
    }
    if (overwrite_handlers || !handler_present)
    {
        set->menu_label = g_strdup( "Tar xz" );
        set->b = XSET_B_TRUE;
        set->s = g_strdup( "application/x-xz-compressed-tar" );
        set->x = g_strdup( ".tar.xz" );
        set->y = g_strdup( "tar -cvJf %o %F" );       // compress command
        set->z = g_strdup( "tar -xvJf %o" );          // extract command
        set->context = g_strdup( "+tar -tvf %o" );    // list command
    }

    /* Archive handler list check is separate as the XSet may exist but
     * the handler may not be in the list */
    if (!g_strstr_len( handlers_search, -1, " arctype_tar_xz " ))
    {
        if (handlers_to_add)
        {
            str = handlers_to_add;
            handlers_to_add = g_strconcat( handlers_to_add,
                                           " arctype_tar_xz", NULL );
            g_free( str );
        }
        else handlers_to_add = g_strdup( "arctype_tar_xz" );
    }

    handler_present = TRUE;
    set = xset_is( "arctype_zip" );
    if (!set)
    {
        set = xset_set( "arctype_zip", "label", "Zip" );
        handler_present = FALSE;
    }
    if (overwrite_handlers || !handler_present)
    {
        set->menu_label = g_strdup( "Zip" );
        set->b = XSET_B_TRUE;
        set->s = g_strdup( "application/x-zip:application/zip" );
        set->x = g_strdup( ".zip" );
        set->y = g_strdup( "+zip -r %o %F" );       // compress command
        set->z = g_strdup( "+unzip %o" );           // extract command
        set->context = g_strdup( "+unzip -l %o" );  // list command
    }

    /* Archive handler list check is separate as the XSet may exist but
     * the handler may not be in the list */
    if (!g_strstr_len( handlers_search, -1, " arctype_zip " ))
    {
        if (handlers_to_add)
        {
            str = handlers_to_add;
            handlers_to_add = g_strconcat( handlers_to_add,
                                           " arctype_zip", NULL );
            g_free( str );
        }
        else handlers_to_add = g_strdup( "arctype_zip" );
    }

    // Clearing up
    g_free( handlers_search );

    // Updating list of archive handlers
    if (handlers_to_add)
    {
        if (g_strcmp0( handlers, "" ) <= 0)
            handlers = handlers_to_add;
        else
        {
            handlers = g_strconcat( handlers, " ", handlers_to_add, NULL );
            g_free( handlers_to_add );
        }
        xset_set( "arc_conf2", "s", handlers );
        g_free( handlers );
    }

    /* Clearing and adding archive handlers to list (this also selects
     * the first handler and therefore populates the handler widgets) */
    GtkListStore* list = (GtkListStore*)g_object_get_data( G_OBJECT( dlg ), "list" );
    gtk_list_store_clear( GTK_LIST_STORE( list ) );
    populate_archive_handlers( GTK_LIST_STORE( list ), GTK_WIDGET( dlg ) );
}

static gboolean validate_handler( GtkWidget* dlg )
{
    const char* dialog_title = _("Archive Handlers");

    // Fetching widgets and handler details
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
    const gboolean handler_compress_term = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON ( chkbtn_handler_compress_term ) );
    const gboolean handler_extract_term = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON ( chkbtn_handler_extract_term ) );
    const gboolean handler_list_term = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON ( chkbtn_handler_list_term ) );
    gchar* handler_compress, *handler_extract, *handler_list;

    /* Commands are prefixed with '+' when they are to be ran in a
     * terminal
     * g_strdup'd to avoid anal const compiler warning... */
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

    /* Validating data. Note that data straight from widgets shouldnt
     * be modified or stored
     * Note that archive creation also allows for a command to be
     * saved */
    if (g_strcmp0( handler_name, "" ) <= 0)
    {
        /* Handler name not set - warning user and exiting. Note
         * that the created dialog does not have an icon set */
        xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_WARNING,
                            dialog_title, NULL, FALSE,
                            _("Please enter a valid handler name "
                            "before saving."), NULL, NULL );
        gtk_widget_grab_focus( entry_handler_name );
        return FALSE;
    }

    // Empty MIME is allowed if extension is filled
    if (g_strcmp0( handler_mime, "" ) <= 0 &&
        g_strcmp0( handler_extension, "" ) <= 0)
    {
        /* Handler MIME not set - warning user and exiting. Note
         * that the created dialog does not have an icon set */
        xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_WARNING,
                            dialog_title, NULL, FALSE,
                            g_strdup_printf(_("Please enter a valid "
                            "MIME content type OR extension for the "
                            "'%s' handler before saving."),
                            handler_name), NULL, NULL );
        gtk_widget_grab_focus( entry_handler_mime );
        return FALSE;
    }
    if (g_strstr_len( handler_mime, -1, " " ) &&
        g_strcmp0( handler_extension, "" ) <= 0)
    {
        /* Handler MIME contains a space - warning user and exiting.
         * Note that the created dialog does not have an icon set */
        xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_WARNING,
                            dialog_title, NULL, FALSE,
                            g_strdup_printf(_("Please ensure the MIME"
                            " content type for the '%s' handler "
                            "contains no spaces before saving."),
                            handler_name), NULL, NULL );
        gtk_widget_grab_focus( entry_handler_mime );
        return FALSE;
    }

    /* Empty extension is allowed if MIME type has been given, but if
     * anything has been entered it must be valid */
    if (
        (
            g_strcmp0( handler_extension, "" ) <= 0 &&
            g_strcmp0( handler_mime, "" ) <= 0
        )
        ||
        (
            g_strcmp0( handler_extension, "" ) > 0 &&
            *handler_extension != '.'
        )
    )
    {
        /* Handler extension is either not set or does not start with
         * a full stop - warning user and exiting. Note that the created
         * dialog does not have an icon set */
        xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_WARNING,
                            dialog_title, NULL, FALSE,
                            g_strdup_printf(_("Please enter a valid "
                            "file extension OR MIME content type for"
                            " the '%s' handler before saving."),
                            handler_name), NULL, NULL );
        gtk_widget_grab_focus( entry_handler_extension );
        return FALSE;
    }

    /* Other settings are commands to run in different situations -
     * since different handlers may or may not need different
     * commands, empty commands are allowed but if something is given,
     * relevant substitution characters should be in place */

    /* Compression handler validation - remember to maintain this code
     * in ptk_file_archiver_create too
     * Checking if a compression command has been entered */
    if (g_strcmp0( handler_compress, "" ) != 0 &&
        g_strcmp0( handler_compress, "+" ) != 0)
    {
        /* It has - making sure all substitution characters are in
         * place - not mandatory to only have one of the particular
         * type */
        if (
            (
                !g_strstr_len( handler_compress, -1, "%o" ) &&
                !g_strstr_len( handler_compress, -1, "%O" )
            )
            ||
            (
                !g_strstr_len( handler_compress, -1, "%f" ) &&
                !g_strstr_len( handler_compress, -1, "%F" )
            )
        )
        {
            xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_WARNING,
                            dialog_title, NULL, FALSE,
                            g_strdup_printf(_("The following "
                            "substitution variables should be in the"
                            " '%s' compression command:\n\n"
                            "One of the following:\n\n"
                            "%%%%f: First selected file/directory to"
                            " archive\n"
                            "%%%%F: All selected files/directories to"
                            " archive\n\n"
                            "and one of the following:\n\n"
                            "%%%%o: Resulting single archive\n"
                            "%%%%O: Resulting archive per source "
                            "file/directory (see %%%%f/%%%%F)"),
                            handler_name), NULL, NULL );
            gtk_widget_grab_focus( entry_handler_compress );
            return FALSE;
        }
    }

    if (g_strcmp0( handler_extract, "" ) != 0 &&
        g_strcmp0( handler_extract, "+" ) != 0 &&
        (
            !g_strstr_len( handler_extract, -1, "%o" )
        ))
    {
        /* Not all substitution characters are in place - warning
         * user and exiting. Note that the created dialog does not
         * have an icon set
         * TODO: IG problem */
        xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_WARNING,
                            dialog_title, NULL, FALSE,
                            g_strdup_printf(_("The following "
                            "placeholders should be in the '%s' extraction"
                            " command:\n\n%%%%o: "
                            "Archive to extract"),
                            handler_name), NULL, NULL );
        gtk_widget_grab_focus( entry_handler_extract );
        return FALSE;
    }

    if (g_strcmp0( handler_list, "" ) != 0 &&
        g_strcmp0( handler_list, "+" ) != 0 &&
        (
            !g_strstr_len( handler_list, -1, "%o" )
        ))
    {
        /* Not all substitution characters are in place  - warning
         * user and exiting. Note that the created dialog does not
         * have an icon set
         * TODO: Confirm if IG still has this problem */
        xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_WARNING,
                            dialog_title, NULL, FALSE,
                            g_strdup_printf(_("The following "
                            "placeholders should be in the '%s' list"
                            " command:\n\n%%%%o: "
                            "Archive to list"),
                            handler_name), NULL, NULL );
        gtk_widget_grab_focus( entry_handler_list );
        return FALSE;
    }

    // Validation passed
    return TRUE;
}
