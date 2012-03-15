/*
*  C Implementation: ptk-input-dialog
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2005
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "ptk-input-dialog.h"
#include <gtk/gtk.h>

/*
* Create a dialog used to prompt the user to input a string.
* title: the title of dialog.
* prompt: prompt showed to the user
*/
GtkWidget* ptk_input_dialog_new( const char* title,
                                 const char* prompt,
                                 const char* default_text,
                                 GtkWindow* parent )
{
    GtkWidget * dlg;
    GtkWidget* box;
    GtkWidget* label;
    GtkWidget* entry;
    dlg = gtk_dialog_new_with_buttons( title,
                                       parent,
                                       0,
                                       GTK_STOCK_CANCEL,
                                       GTK_RESPONSE_CANCEL,
                                       GTK_STOCK_OK,
                                       GTK_RESPONSE_OK,
                                       NULL );
    gtk_dialog_set_alternative_button_order( GTK_DIALOG(dlg), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1 );

    box = ( ( GtkDialog* ) dlg )->vbox;
    label = gtk_label_new( prompt );
    gtk_box_pack_start( GTK_BOX( box ), label, FALSE, FALSE, 4 );

    entry = gtk_entry_new();
    gtk_entry_set_text( GTK_ENTRY( entry ),
                        default_text ? default_text : "" );
    gtk_box_pack_start( GTK_BOX( box ), entry, FALSE, FALSE, 4 );

    g_object_set_data( G_OBJECT( dlg ), "prompt", label );
    g_object_set_data( G_OBJECT( dlg ), "entry", entry );

    gtk_dialog_set_default_response( ( GtkDialog* ) dlg,
                                     GTK_RESPONSE_OK );
    gtk_entry_set_activates_default ( GTK_ENTRY( entry ), TRUE );

    gtk_widget_show_all( box );
    return dlg;
}

/*
* Get user input from the text entry of the input dialog.
* The returned string should be freed when no longer needed.
* input_dialog: the input dialog
*/
gchar* ptk_input_dialog_get_text( GtkWidget* input_dialog )
{
    GtkWidget * entry = ptk_input_dialog_get_entry( input_dialog );
    return g_strdup( gtk_entry_get_text( GTK_ENTRY( entry ) ) );
}

/*
* Get the prompt label of the input dialog.
* input_dialog: the input dialog
*/
GtkWidget* ptk_input_dialog_get_label( GtkWidget* input_dialog )
{
    return GTK_WIDGET( g_object_get_data(
                           G_OBJECT( input_dialog ), "prompt" ) );
}


/*
* Get the text entry widget of the input dialog.
* input_dialog: the input dialog
*/
GtkWidget* ptk_input_dialog_get_entry( GtkWidget* input_dialog )
{
    return GTK_WIDGET( g_object_get_data(
                           G_OBJECT( input_dialog ), "entry" ) );
}

