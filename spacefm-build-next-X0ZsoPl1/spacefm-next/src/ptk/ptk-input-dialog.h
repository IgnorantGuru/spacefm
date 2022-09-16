/*
*  C Interface: ptk-input-dialog
*
* Description: 
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2005
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef _PTK_INPUT_DIALOG_H_
#define _PTK_INPUT_DIALOG_H_

#include <gtk/gtk.h>

/*
* Create a dialog used to prompt the user to input a string.
* title: the title of dialog.
* prompt: prompt showed to the user
*/
GtkWidget* ptk_input_dialog_new( const char* title,
                                 const char* prompt,
                                 const char* default_text,
                                 GtkWindow* parent );

/*
* Get user input from the text entry of the input dialog.
* The returned string should be freed when no longer needed.
* widget: the input dialog
*/
gchar* ptk_input_dialog_get_text( GtkWidget* input_dialog );

/*
* Get the prompt label of the input dialog.
* input_dialog: the input dialog
*/
GtkWidget* ptk_input_dialog_get_label( GtkWidget* input_dialog );

/*
* Get the text entry widget of the input dialog.
* input_dialog: the input dialog
*/
GtkWidget* ptk_input_dialog_get_entry( GtkWidget* input_dialog );

/*
* Used to prompt the user to input a string.
* The returned string should be freed when no longer needed.
*/ 
/*
char* ptk_input_dialog_get_user_input( const char* title,
                                   const char* prompt );
*/

#endif

