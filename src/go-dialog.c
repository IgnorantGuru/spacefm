#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>

#include <stdlib.h>
#include <string.h>

#include "go-dialog.h"
#include "main-window.h"
#include "ptk-file-browser.h"
#include "ptk-path-entry.h"
#include "ptk-utils.h"

static const char * chosen_path;

static gboolean show_go_dialog( GtkWindow* parent, char * initial_path )
{
    GtkBuilder* builder = _gtk_builder_new_from_file( PACKAGE_UI_DIR "/godlg.ui", NULL );
    GtkDialog* dlg = GTK_DIALOG(gtk_builder_get_object( builder, "godlg" ));

    GtkEntry* path_entry = ( GtkEntry* ) ptk_path_entry_new ( NULL );
    gtk_entry_set_activates_default( path_entry, TRUE );
    gtk_entry_set_text( path_entry, initial_path );

    gtk_container_add( GTK_CONTAINER( gtk_dialog_get_content_area( dlg ) ), GTK_WIDGET( path_entry ) );
    gtk_widget_show_all( gtk_dialog_get_content_area( dlg ) );
    gtk_widget_grab_focus( GTK_WIDGET( path_entry ) );

    gboolean ret = ( gtk_dialog_run( GTK_DIALOG( dlg ) ) == GTK_RESPONSE_OK );
    if ( ret )
    {
        chosen_path = strdup( gtk_entry_get_text( path_entry ) );
    }
    gtk_widget_destroy( GTK_WIDGET( dlg ) );
    return ret;
}

gboolean fm_go( FMMainWindow* main_window )
{
    int i = gtk_notebook_get_current_page( GTK_NOTEBOOK( main_window->notebook ) );
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                    GTK_NOTEBOOK( main_window->notebook ), i ) );

    if( file_browser->dir )
    {
        char* disp_path = file_browser->dir->disp_path;
        if( disp_path )
        {

            if( show_go_dialog( GTK_WINDOW( main_window ), disp_path ) )
            {
                char* dir_path = g_filename_from_utf8( chosen_path , -1, NULL, NULL, NULL );
                free( (char*)chosen_path );
                char* final_path = vfs_file_resolve_path( ptk_file_browser_get_cwd(file_browser), dir_path );
                g_free( dir_path );
                ptk_file_browser_chdir( file_browser, final_path, PTK_FB_CHDIR_ADD_HISTORY );
                g_free( final_path );
                gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );
            }
        }
    }
    return TRUE;
}
