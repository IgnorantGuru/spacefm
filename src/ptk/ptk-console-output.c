#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "glib-mem.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h> 	/* for kill(2) */

#include "ptk-utils.h"

typedef struct _PtkConsoleOutputData
{
    GtkWidget* main_dlg;
    GtkWidget* scroll;
    GtkTextView* view;
    GtkTextBuffer* buf;

    GPid pid;
    int stdo, stdi, stde;
    GIOChannel *chstdo, *chstdi, *chstde;
}PtkConsoleOutputData;

static gboolean on_complete( gpointer user_data )
{
    GtkAdjustment* adj = GTK_ADJUSTMENT( user_data );
    gdk_threads_enter();
    gtk_adjustment_set_value( adj, adj->upper-adj->page_size );
    gdk_threads_leave();
    return FALSE;
}

static gboolean delayed_destroy( gpointer dlg )
{
    gdk_threads_enter();
    gtk_widget_destroy( GTK_WIDGET( dlg ) );
    gdk_threads_leave();
    return FALSE;
}

static gboolean on_output( GIOChannel* ch, GIOCondition cond, gpointer user_data )
{
    GtkTextIter it;
    char buffer[4096];
    gsize rlen = 0;
    GtkAdjustment* adj;
    int status;
    PtkConsoleOutputData *data = (PtkConsoleOutputData*)user_data;

    if( cond & G_IO_IN )
    {
        g_io_channel_read_chars( ch, buffer, sizeof(buffer), &rlen, NULL );
        GDK_THREADS_ENTER();
        /*gdk_window_freeze_updates( GTK_WIDGET(data->view)->window );*/
        gtk_text_buffer_get_end_iter( data->buf, &it );
        gtk_text_buffer_insert( data->buf, &it, buffer, rlen );
        adj = gtk_scrolled_window_get_vadjustment( GTK_SCROLLED_WINDOW(data->scroll) );
        gtk_adjustment_set_value( adj, adj->upper-adj->page_size );
        /*gdk_window_thaw_updates( GTK_WIDGET(data->view)->window );*/
        GDK_THREADS_LEAVE();
    }
    if( cond == G_IO_HUP )
    {
        if( data->chstdo == ch )
        {
            strcpy(buffer, _("\nComplete!"));
            rlen = strlen(buffer);

            GDK_THREADS_ENTER();

            gtk_text_buffer_get_end_iter( data->buf, &it );
            gtk_text_buffer_insert( data->buf, &it, buffer, rlen );
            gtk_dialog_set_response_sensitive( GTK_DIALOG(data->main_dlg), GTK_RESPONSE_CANCEL, FALSE );
            gtk_dialog_set_response_sensitive( GTK_DIALOG(data->main_dlg), GTK_RESPONSE_CLOSE, TRUE );

            if( data->pid )
            {
                status = 0;
                waitpid( data->pid, &status, 0 );
                data->pid = 0;

                if( WIFEXITED(status) && 0 == WEXITSTATUS(status) )
                {
                    g_idle_add( delayed_destroy, data->main_dlg );
                    GDK_THREADS_LEAVE();
                    return FALSE;
                }
                adj = gtk_scrolled_window_get_vadjustment( GTK_SCROLLED_WINDOW(data->scroll) );
                gtk_adjustment_set_value( adj, adj->upper-adj->page_size );
                g_idle_add( on_complete, adj );
            }
            GDK_THREADS_LEAVE();
        }

        return FALSE;
    }
    return TRUE;
}

static void on_destroy( gpointer data, GObject* obj )
{
    int status = 0;
    PtkConsoleOutputData* ptk_data = (PtkConsoleOutputData *)data;

    do{}
    while( g_source_remove_by_user_data( ptk_data ) );

    if( ptk_data->chstdo )
    {
        g_io_channel_unref(ptk_data->chstdo);
        close( ptk_data->stdo );
    }
    if( ptk_data->chstde )
    {
        g_io_channel_unref(ptk_data->chstde);
        close( ptk_data->stde );
    }

    if( ptk_data->pid )
    {
        kill( ptk_data->pid, 15 );
        waitpid( ptk_data->pid, &status, 0 );
    }
    g_slice_free( PtkConsoleOutputData, ptk_data );
}

static void on_response( GtkDialog* dlg, int response,
                         PtkConsoleOutputData* data )
{
    gtk_widget_destroy( data->main_dlg );
}

int ptk_console_output_run( GtkWindow* parent_win,
                            const char* title,
                            const char* desc,
                            const char* working_dir,
                            int argc, char* argv[] )
{
    GtkWidget* main_dlg;
    GtkWidget* desc_label;
    PtkConsoleOutputData* data;
    GtkWidget* hbox;
    GtkWidget* entry;
    GdkColor black={0}, white={0, 65535, 65535, 65535};
    char* cmd;
    GError* err;

    data = g_slice_new0( PtkConsoleOutputData );
    cmd = g_strjoinv( " ", &argv[0] );

    main_dlg = gtk_dialog_new_with_buttons( title, NULL, 0,
                                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                            GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL );
    gtk_dialog_set_alternative_button_order( GTK_DIALOG(main_dlg), GTK_RESPONSE_CLOSE, GTK_RESPONSE_CANCEL, -1 );

    g_object_weak_ref( G_OBJECT(main_dlg), on_destroy, data );

    data->main_dlg = main_dlg;
    gtk_dialog_set_response_sensitive( GTK_DIALOG(main_dlg), GTK_RESPONSE_CLOSE, FALSE );
    gtk_window_set_type_hint( GTK_WINDOW(main_dlg),
                              GDK_WINDOW_TYPE_HINT_NORMAL );

    desc_label = gtk_label_new( desc );
    gtk_label_set_line_wrap( GTK_LABEL(desc_label), TRUE );
    gtk_box_pack_start( GTK_BOX(GTK_DIALOG(main_dlg)->vbox),
                        desc_label, FALSE, TRUE, 2 );

    hbox = gtk_hbox_new( FALSE, 2 );

    gtk_box_pack_start( GTK_BOX(hbox), gtk_label_new( _("Command:") ),
                        FALSE, FALSE, 2 );

    entry = gtk_entry_new();
    gtk_entry_set_text( GTK_ENTRY(entry), cmd );
    gtk_editable_set_position( GTK_EDITABLE(entry), 0 );
    gtk_editable_set_editable( GTK_EDITABLE(entry), FALSE );
    gtk_editable_select_region( GTK_EDITABLE(entry), 0, 0 );
    g_free( cmd );
    gtk_box_pack_start( GTK_BOX(hbox), entry, TRUE, TRUE, 2 );

    gtk_box_pack_start( GTK_BOX(GTK_DIALOG(main_dlg)->vbox),
                        hbox, FALSE, TRUE, 2 );

    data->buf = GTK_TEXT_BUFFER(gtk_text_buffer_new(NULL));
    data->view = GTK_TEXT_VIEW(gtk_text_view_new_with_buffer( data->buf ));
    gtk_widget_modify_base( GTK_WIDGET(data->view), GTK_STATE_NORMAL, &black );
    gtk_widget_modify_text( GTK_WIDGET(data->view), GTK_STATE_NORMAL, &white );
    data->scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(data->scroll),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_ALWAYS );
    gtk_container_add( GTK_CONTAINER(data->scroll), GTK_WIDGET(data->view) );
    gtk_box_pack_start( GTK_BOX(GTK_DIALOG(main_dlg)->vbox), data->scroll, TRUE, TRUE, 2 );
    gtk_widget_show_all( GTK_DIALOG(main_dlg)->vbox );
    gtk_window_set_default_size( GTK_WINDOW(main_dlg), 480, 240 );

    gtk_widget_show( main_dlg );

    if( g_spawn_async_with_pipes( working_dir, argv, NULL,
                                  G_SPAWN_SEARCH_PATH, NULL, NULL,
                                  &data->pid, NULL/*&stdi*/,
                                  &data->stdo, &data->stde, &err ) )
    {
        /* fcntl(stdi,F_SETFL,O_NONBLOCK); */
        fcntl(data->stdo,F_SETFL,O_NONBLOCK);
        fcntl(data->stde,F_SETFL,O_NONBLOCK);

        data->chstdo = g_io_channel_unix_new( data->stdo );
        g_io_channel_set_encoding( data->chstdo, NULL, NULL );
        g_io_channel_set_buffered( data->chstdo, FALSE );
        g_io_add_watch( data->chstdo, G_IO_IN|G_IO_HUP, on_output, data );

        fcntl(data->stde,F_SETFL,O_NONBLOCK);
        data->chstde = g_io_channel_unix_new( data->stde );
        g_io_channel_set_encoding( data->chstde, NULL, NULL );
        g_io_channel_set_buffered( data->chstde, FALSE );
        g_io_add_watch( data->chstde, G_IO_IN|G_IO_HUP, on_output, data );
        g_signal_connect( main_dlg, "delete-event", G_CALLBACK(gtk_widget_destroy), NULL );
        g_signal_connect( main_dlg, "response", G_CALLBACK(on_response), data );
    }
    else
    {
        gtk_widget_destroy( main_dlg );
        ptk_show_error( parent_win, _("Error"), err->message );
        g_error_free( err );
        return 1;
    }
    return 0;
}
