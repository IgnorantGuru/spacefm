/*
 *      cust-dialog.c
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

//#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#include "settings.h"
#include "cust-dialog.h"

static void update_element( CustomElement* el, GtkWidget* box );

int type_count[ G_N_ELEMENTS( cdlg_option ) ] = { 0 };


static char* read_file_value( const char* path, gboolean multi )
{
    FILE* file;
    int f, bytes;
    
    if ( !g_file_test( path, G_FILE_TEST_EXISTS ) )
    {
        // create file
        if ( ( f = open( path, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR ) ) == -1 )
        {
            g_warning( _("spacefm: error creating file %s: %s\n"), path,
                                                        g_strerror( errno ) );
            return NULL;
        }
        close( f );
    }
    // read file
    char line[ 2048 ];
    if ( multi )
    {
        // read up to 2K of file
        if ( ( f = open( path, O_RDONLY ) ) == -1 )
        {
            g_warning( _("spacefm: error reading file %s: %s\n"), path,
                                                        g_strerror( errno ) );
            return NULL;
        }
        bytes = read( f, line, sizeof( line ) - 1 );
        close(f);
        line[bytes] = '\0';
    }
    else
    {
        // read first line of file
        file = fopen( path, "r" );
        if ( !file )
        {
            g_warning( _("spacefm: error reading file %s: %s\n"), path,
                                                        g_strerror( errno ) );
            return NULL;
        }
        if ( !fgets( line, sizeof( line ), file ) )
        {
            fclose( file );
            return NULL;
        }
        fclose( file );
        strtok( line, "\r\n" );
    }
    return line[0] != '\0' ? g_strdup( line ) : NULL;
}

static void cb_file_value_change( VFSFileMonitor* fm,
                                        VFSFileMonitorEvent event,
                                        const char* file_name,
                                        CustomElement* el )
{
    //printf( "cb_file_value_change %d %s\n", event, el->watch_file );
    switch( event )
    {
    case VFS_FILE_MONITOR_DELETE:
        //printf ("    DELETE\n");
        vfs_file_monitor_remove( el->monitor, 
                            (VFSFileMonitorCallback)cb_file_value_change, el );
        el->monitor = NULL;
        update_element( el, NULL );  // this will add a new monitor if file re-created
        break;
    case VFS_FILE_MONITOR_CHANGE:
    case VFS_FILE_MONITOR_CREATE:
    default:
        //printf ("    CREATE/CHANGE\n");
        update_element( el, NULL );
        break;
    }
}

static void get_text_value( CustomElement* el, const char* val, gboolean multi )
{
    if ( !val )
        return;
    if ( val[0] == '@' )
    {
        // get value from file
        g_free( el->val );
        el->val = read_file_value( val + 1, multi );
        if ( !el->monitor && g_file_test( val + 1, G_FILE_TEST_IS_REGULAR ) )
        {
            // start monitoring file
            el->monitor = vfs_file_monitor_add( (char*)val + 1, FALSE,
                                (VFSFileMonitorCallback)cb_file_value_change, el );
            el->watch_file = val + 1;
        }
    }
    else
    {
        // get static value
        if ( !el->val )
            el->val = g_strdup( val );
    }
}

static void on_button_clicked( GtkButton *button, CustomElement* el ) 
{
    printf("click\n");
}

static void update_element( CustomElement* el, GtkWidget* box )
{
    GtkWidget* w;
    GdkPixbuf* pixbuf;
    GtkWidget* dlg = (GtkWidget*)el->widgets->data;
    char* str;
    char* sep;
    
    GList* args = el->args;
    switch ( el->type )
    {
    case CDLG_TITLE:
        if ( args )
        {
            get_text_value( el, (char*)args->data, FALSE );
            if ( el->val )
                gtk_window_set_title( GTK_WINDOW( dlg ), el->val );     
        }
        break;
    case CDLG_WINDOW_ICON:
        if ( args )
        {
            get_text_value( el, (char*)args->data, FALSE );
            if ( el->val )
                pixbuf = gtk_icon_theme_load_icon( gtk_icon_theme_get_default(),
                                el->val, 16, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );
            else
                pixbuf = NULL;
            gtk_window_set_icon( GTK_WINDOW( dlg ), pixbuf );  
        }
        break;
    case CDLG_LABEL:
        if ( args )
            get_text_value( el, (char*)args->data, TRUE );
        // add label
        if ( !el->widgets->next && box )
        {
            w = gtk_label_new( NULL );
            gtk_label_set_line_wrap( GTK_LABEL( w ), TRUE );
            gtk_label_set_line_wrap_mode( GTK_LABEL( w ), PANGO_WRAP_WORD_CHAR );
            gtk_misc_set_alignment( GTK_MISC ( w ), 0.1, 0.5 );
            el->widgets = g_list_append( el->widgets, w );
            gtk_box_pack_start( GTK_BOX( box ), GTK_WIDGET( w ), TRUE, TRUE, 4 );
        }
        // set label
        if ( el->widgets->next && ( w = GTK_WIDGET( el->widgets->next->data ) ) )
        {
            if ( el->val && el->val[0] == '~' )
                gtk_label_set_markup_with_mnemonic( GTK_LABEL( w ), el->val + 1 );
            else
                gtk_label_set_text( GTK_LABEL( w ), el->val );
        }
        break;
    case CDLG_BUTTON:
        if ( args )
            get_text_value( el, (char*)args->data, FALSE );
        // add button
        if ( !el->widgets->next )
        {
            w = gtk_button_new();
            gtk_button_set_use_underline( GTK_BUTTON( w ), TRUE );
            gtk_button_set_focus_on_click( GTK_BUTTON( w ), FALSE );
            el->widgets = g_list_append( el->widgets, w );
            gtk_box_pack_start( GTK_BOX( GTK_DIALOG( dlg )->action_area ),
                                            GTK_WIDGET( w ), TRUE, TRUE, 0 );
            gtk_widget_grab_focus( w );
            g_signal_connect( G_OBJECT( w ), "clicked",
                                        G_CALLBACK( on_button_clicked ), el );
        }
        // set label and icon
        if ( el->widgets->next && ( w = GTK_WIDGET( el->widgets->next->data ) ) )
        {
            if ( el->val && ( sep = strchr( el->val, ';' ) ) )
                sep[0] = '\0';
            else
                sep = NULL;
            if ( !sep &&  ( !g_strcmp0( el->val, "ok" )
                         || !g_strcmp0( el->val, "cancel" )
                         || !g_strcmp0( el->val, "close" )
                         || !g_strcmp0( el->val, "open" )
                         || !g_strcmp0( el->val, "yes" )
                         || !g_strcmp0( el->val, "no" )
                         || !g_strcmp0( el->val, "apply" )
                         || !g_strcmp0( el->val, "delete" )
                         || !g_strcmp0( el->val, "edit" )
                         || !g_strcmp0( el->val, "save" )
                         || !g_strcmp0( el->val, "stop" ) ) )
            {
                // stock button
                gtk_button_set_use_stock( GTK_BUTTON( w ), TRUE );
                str = g_strdup_printf( "gtk-%s", el->val );
                gtk_button_set_label( GTK_BUTTON( w ), str );
                g_free( str );
            }
            else
            {
                // custom button
                gtk_button_set_use_stock( GTK_BUTTON( w ), FALSE );
                gtk_button_set_label( GTK_BUTTON( w ), el->val );
            }
            // set icon
            if ( sep && sep[1] != '\0' )
                gtk_button_set_image( GTK_BUTTON( w ), xset_get_image( sep + 1,
                                                        GTK_ICON_SIZE_BUTTON ) );
            if ( sep )
                sep[0] = ';';
        }
        break;
    }
}

static void build_dialog( GList* elements )
{
    GList* l;
    CustomElement* el;
    const char* arg;
    GList* args;
    char* str;
    
    // create dialog
    GtkWidget* dlg = gtk_dialog_new();
    GList* boxes = g_list_append( NULL, GTK_DIALOG( dlg )->vbox );
    GtkWidget* box = (GtkWidget*)boxes->data;

    // add elements
    for ( l = elements; l; l = l->next )
    {
        el = (CustomElement*)l->data;
        el->widgets = g_list_append( NULL, dlg );
        update_element( el, box );
    }
    gtk_widget_show_all( dlg );
}

int custom_dialog_init( int argc, char *argv[] )
{
    int ac, i, j;
    GList* elements = NULL;
    CustomElement* el = NULL;
    GList* l;
    char* num;
    char* str;
    
    for ( ac = 2; ac < argc; ac++ )
    {
        if ( g_str_has_prefix( argv[ac], "--" ) )
        {
            j = 0;
            for ( i = 0; i < G_N_ELEMENTS( cdlg_option ); i += 3 )
            {
                if ( !strcmp( argv[ac] + 2, cdlg_option[i] ) )
                {
                    el = g_slice_new( CustomElement );
                    el->type = j;
                    if ( type_count[j]++ == 0 )
                        num = NULL;
                    else
                        num = g_strdup_printf( "%d", type_count[j] );
                    str = replace_string( cdlg_option[i], "-", "_", FALSE );
                    el->name = g_strdup_printf( "%s%s", str,
                                                        num ? num : "" );
                    g_free( num );
                    g_free( str );
                    el->args = NULL;
                    el->def_val = NULL;
                    el->val = NULL;
                    el->command = NULL;
                    el->widgets = NULL;
                    el->monitor = NULL;
                    el->watch_file = NULL;
                    elements = g_list_append( elements, el );
                    break;
                }
                j++;
            }
            if ( i < G_N_ELEMENTS( cdlg_option ) )
                continue;
        }
        if ( !el )
        {
            fprintf( stderr, "spacefm: %s '%s'\n", _("invalid dialog option"), argv[ac] );
            return 1;
        }
        el->args = g_list_append( el->args, argv[ac] );
    }
    build_dialog( elements );
    return 0;
_done_err:
    if ( elements )
    {
        for ( l = elements; l; l = l->next )
        {
            el = (CustomElement*)l->data;
            g_list_free( el->args );
            g_slice_free( CustomElement, el );
        }
        g_list_free( elements );
    }
    //vfs_file_monitor_remove
    return 1;
}
