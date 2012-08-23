/*
 *      cust-dialog.c
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

//#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#include "settings.h"
#include "cust-dialog.h"

static void update_element( CustomElement* el, GtkWidget* box );

static void run_command( GList* args )
{
    char* str;
    char* cmd = NULL;
    GList* l;
    for ( l = args; l; l = l->next )
    {
        str = cmd;
        cmd = g_strdup_printf( "%s%s%s", str ? str : "", str ? " " : "",
                                                            (char*)l->data );
        g_free( str );
    }
    printf("CMD=%s\n", cmd );
    g_spawn_command_line_async( cmd, NULL );
    g_free( cmd );
}

static void write_file_value( const char* path, const char* val )
{
    int f;
    int add = 0;
    
    if ( path[0] == '@' )
        add = 1;

    if ( ( f = open( path + add, O_CREAT | O_WRONLY | O_TRUNC,
                                                    S_IRUSR | S_IWUSR ) ) == -1 )
    {
        g_warning( _("error writing file %s: %s\n"), path + add,
                                                    g_strerror( errno ) );
        return;
    }
    if ( val && write( f, val, strlen( val ) ) < strlen( val ) )
        g_warning( _("error writing file %s: %s\n"), path + add,
                                                    g_strerror( errno ) );
    close( f );
}

static char* read_file_value( const char* path, gboolean multi )
{
    FILE* file;
    int f, bytes;
    
    if ( !g_file_test( path, G_FILE_TEST_EXISTS ) )
    {
        // create file
        if ( ( f = open( path, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR ) ) == -1 )
        {
            g_warning( _("error creating file %s: %s\n"), path,
                                                        g_strerror( errno ) );
            return NULL;
        }
        close( f );
    }
    // read file
    char line[ 4096 ];
    if ( multi )
    {
        // read up to 4K of file
        if ( ( f = open( path, O_RDONLY ) ) == -1 )
        {
            g_warning( _("error reading file %s: %s\n"), path,
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
            g_warning( _("error reading file %s: %s\n"), path,
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
    if ( !g_utf8_validate( line, -1, NULL ) )
    {
        g_warning( _("file '%s' contents are not valid UTF-8\n"), path );
        return NULL;
    }        
    return line[0] != '\0' ? g_strdup( line ) : NULL;
}

static gboolean cb_pipe_watch( GIOChannel *channel, GIOCondition cond,
                               CustomElement* el )
{
/*
fprintf( stderr, "cb_pipe_watch %d\n", channel);
if ( cond & G_IO_IN )
    fprintf( stderr, "    G_IO_IN\n");
if ( cond & G_IO_OUT )
    fprintf( stderr, "    G_IO_OUT\n");
if ( cond & G_IO_PRI )
    fprintf( stderr, "    G_IO_PRI\n");
if ( cond & G_IO_ERR )
    fprintf( stderr, "    G_IO_ERR\n");
if ( cond & G_IO_HUP )
    fprintf( stderr, "    G_IO_HUP\n");
if ( cond & G_IO_NVAL )
    fprintf( stderr, "    G_IO_NVAL\n");

if ( !( cond & G_IO_NVAL ) )
{
    gint fd = g_io_channel_unix_get_fd( channel );
    fprintf( stderr, "    fd=%d\n", fd);
    if ( fcntl(fd, F_GETFL) != -1 || errno != EBADF )
    {
        int flags = g_io_channel_get_flags( channel );
        if ( flags & G_IO_FLAG_IS_READABLE )
            fprintf( stderr, "    G_IO_FLAG_IS_READABLE\n");
    }
    else
        fprintf( stderr, "    Invalid FD\n");
}
*/
    if ( ( cond & G_IO_NVAL ) ) 
    {
        g_io_channel_unref( channel );
        return FALSE;
    }
    else if ( !( cond & G_IO_IN ) )
    {
        if ( ( cond & G_IO_HUP ) )
        {
            g_io_channel_unref( channel );
            return FALSE;
        }
        else
            return TRUE;
    }
    else if ( !( fcntl( g_io_channel_unix_get_fd( channel ), F_GETFL ) != -1
                                                    || errno != EBADF ) )
    {
        // bad file descriptor
        g_io_channel_unref( channel );
        return FALSE;
    }

    //GError *error = NULL;
    gsize  size;
    gchar line[2048];
    if ( g_io_channel_read_chars( channel, line, sizeof( line ), &size, NULL ) ==
                                                G_IO_STATUS_NORMAL && size > 0 )
    {
        GtkTextIter iter, siter;
        GtkTextBuffer* buf = gtk_text_view_get_buffer(
                                    GTK_TEXT_VIEW( el->widgets->next->data ) );
        if ( !g_utf8_validate( line, size, NULL ) )
            g_warning( _("pipe '%s' data is not valid UTF-8\n"), (char*)el->args->data );
        else if ( buf )
        {
            gtk_text_buffer_get_end_iter( buf, &iter);            
            gtk_text_buffer_insert( buf, &iter, line, size );
            //scroll
            if ( el->args->next && !g_strcmp0( (char*)el->args->next->data, "--scroll" ) )
            {
                GtkAdjustment* adj = gtk_scrolled_window_get_vadjustment( 
                        GTK_SCROLLED_WINDOW( el->widgets->next->next->data ) );
                if ( adj->upper - adj->value < adj->page_size + 40 )
                    gtk_text_view_scroll_to_mark( GTK_TEXT_VIEW( el->widgets->next->data ),
                                          gtk_text_buffer_get_mark( buf, "end" ),
                                          0.0, FALSE, 0, 0 );
            }
            // trim
            if ( gtk_text_buffer_get_char_count( buf ) > 64000 ||
                            gtk_text_buffer_get_line_count( buf ) > 800 )
            {
                if ( gtk_text_buffer_get_char_count( buf ) > 64000 )
                {
                    // trim to 50000 characters - handles single line flood
                    gtk_text_buffer_get_iter_at_offset( buf, &iter,
                            gtk_text_buffer_get_char_count( buf ) - 50000 );
                }
                else
                    // trim to 700 lines
                    gtk_text_buffer_get_iter_at_line( buf, &iter, 
                            gtk_text_buffer_get_line_count( buf ) - 700 );
                gtk_text_buffer_get_start_iter( buf, &siter );
                gtk_text_buffer_delete( buf, &siter, &iter );
                gtk_text_buffer_get_start_iter( buf, &siter );
                gtk_text_buffer_insert( buf, &siter, _("[ SNIP - additional output above has been trimmed from this log ]\n"), -1 );
            }
        }
    }
    else
        g_warning( "cb_pipe_watch: g_io_channel_read_chars != G_IO_STATUS_NORMAL\n" );
    return TRUE;
}

static gboolean delayed_update( CustomElement* el )
{
    update_element( el, NULL );
    return FALSE;
}

static void cb_file_value_change( VFSFileMonitor* fm,
                                        VFSFileMonitorEvent event,
                                        const char* file_name,
                                        CustomElement* el )
{
    //printf( "cb_file_value_change %d %s\n", event, file_name );
    switch( event )
    {
    case VFS_FILE_MONITOR_DELETE:
        //printf ("    DELETE\n");
        if ( el->monitor )
            vfs_file_monitor_remove( el->monitor, el->callback, el );
        el->monitor = NULL; 
        // this will add a new monitor if file re-created
        // use g_idle_add since cb_file_value_change is called from another thread
        // otherwise segfault in vfs-file-monitor.c:351
        g_idle_add( ( GSourceFunc ) delayed_update, el );
        break;
    case VFS_FILE_MONITOR_CHANGE:
    case VFS_FILE_MONITOR_CREATE:
    default:
        //printf ("    CREATE/CHANGE\n");
        g_idle_add( ( GSourceFunc ) delayed_update, el );
        break;
    }
}

static void fill_buffer_from_file( CustomElement* el, GtkTextBuffer* buf,
                                   char* path, gboolean watch )
{
    int f, bytes;
    char line[ 2048 ];

    char* pathx = path;
    if ( pathx[0] == '@' )
        pathx++;
    
    gtk_text_buffer_set_text( buf, "", -1 );
    
    if ( ( f = open( pathx, O_RDONLY ) ) == -1 )
    {
        g_warning( _("error reading file %s: %s\n"), pathx,
                                                    g_strerror( errno ) );
        return;
    }
    while ( ( bytes = read( f, line, sizeof( line ) ) ) > 0 )
    {
        if ( !g_utf8_validate( line, bytes, NULL ) )
        {
            close( f );
            if ( watch )
                gtk_text_buffer_set_text( buf, _("( file contents are not valid UTF-8 )"), -1 );
            else
                gtk_text_buffer_set_text( buf, "", -1 );
            g_warning( _("file '%s' contents are not valid UTF-8\n"), path );
            return;
        }        
        gtk_text_buffer_insert_at_cursor( buf, line, bytes );
    }
    close( f );
    if ( watch && !el->monitor )
    {
        // start monitoring file
        el->callback = (VFSFileMonitorCallback)cb_file_value_change;
        el->monitor = vfs_file_monitor_add( pathx, FALSE,
                                                    el->callback, el );
    }
}

static void get_text_value( CustomElement* el, const char* val, gboolean multi,
                                                                gboolean watch )
{
    if ( !val )
        return;
    if ( val[0] == '@' )
    {
        // get value from file
        g_free( el->val );
        el->val = read_file_value( val + 1, multi );
        if ( watch && !el->monitor && g_file_test( val + 1, G_FILE_TEST_IS_REGULAR ) )
        {
            // start monitoring file
            el->callback = (VFSFileMonitorCallback)cb_file_value_change;
            el->monitor = vfs_file_monitor_add( (char*)val + 1, FALSE,
                                                        el->callback, el );
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

static void free_elements( GList* elements )
{
    GList* l;
    CustomElement* el;
    
    for ( l = elements; l; l = l->next )
    {
        el = (CustomElement*)l->data;
        g_free( el->name );
        g_free( el->val );
        if ( el->monitor )
            vfs_file_monitor_remove( el->monitor, el->callback, el );
        g_list_free( el->widgets );
        g_list_free( el->args );
    }
    g_list_free( elements );
}

static void destroy_dlg( GtkWidget* dlg )
{
    GList* elements = (GList*)g_object_get_data( G_OBJECT( dlg ), "elements" );

    // remove destroy signal connect
    g_signal_handlers_disconnect_by_data( dlg, NULL );
    gtk_widget_destroy( GTK_WIDGET( dlg ) );
    free_elements( elements );
    gtk_main_quit();
}

static void write_value( FILE* file, const char* prefix, const char* name,
                                     const char* sub,    const char* val )
{
    char* str;
    char* quoted = bash_quote( val );
    if ( strchr( quoted, '\n' ) )
    {
        str = quoted;
        quoted = replace_string( str, "\n", "'$'\\n''", FALSE );
        g_free( str );
    }
    fprintf( file, "%s_%s%s%s=%s\n", prefix, name, sub ? "_" : "", sub ? sub : "",
                                                                    quoted );
    g_free( quoted );
}

static void write_output( GtkWidget* dlg, CustomElement* el_pressed )
{
    GList* l;
    CustomElement* el;
    char* str;
    FILE* out = stdout;
    char* prefix = "dialog";
    
    GList* elements = (GList*)g_object_get_data( G_OBJECT( dlg ), "elements" );

    // get custom prefix
    for ( l = elements; l; l = l->next )
    {
        if ( ((CustomElement*)l->data)->type == CDLG_PREFIX )
        {
            el = (CustomElement*)l->data;
            if ( el->args )
            {
                get_text_value( el, (char*)el->args->data, FALSE, TRUE );
                if ( el->val && el->val[0] != '\0' )
                {
                    str = g_strdup( el->val );
                    g_strcanon( str, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_", ' ' );
                    if ( strcmp( str, el->val ) )
                        g_warning( _("prefix '%s' is not a valid bash variable name"), el->val );
                    else
                        prefix = el->val;
                    g_free( str );
                }
            }
            break;
        }
    }
    
    // write values
    int button_count = 1;
    fprintf( out, "#!/bin/bash\n# spacefm dialog source output - execute this output to set variables\n\n" );
    if ( !el_pressed )
    {
        // no button press caused dialog closure
        write_value( out, prefix, "pressed", NULL, NULL );
        write_value( out, prefix, "pressed", "label", NULL );
        write_value( out, prefix, "pressed", "name", NULL );
        write_value( out, prefix, "pressed", "index", "0" );
    }
    for ( l = elements; l; l = l->next )
    {
        el = (CustomElement*)l->data;
        switch ( el->type )
        {
        case CDLG_TITLE:
        case CDLG_WINDOW_ICON:
        case CDLG_LABEL:
        case CDLG_IMAGE:
        case CDLG_ICON:
            write_value( out, prefix, el->name, NULL,
                                    el->args ? (char*)el->args->data : NULL );
            write_value( out, prefix, el->name, "value",
                                    el->val );
            break;
        case CDLG_BUTTON:
            if ( el == el_pressed )
            {
                // dialog was closed by user pressing this button
                write_value( out, prefix, "pressed", NULL,
                                    el->args ? (char*)el->args->data : NULL );
                write_value( out, prefix, "pressed", "label", el->val );
                write_value( out, prefix, "pressed", "name", el->name );
                str = g_strdup_printf( "%d", button_count );
                write_value( out, prefix, "pressed", "index", str );
                g_free( str );
            }
            button_count++;
        case CDLG_FREE_BUTTON:
            write_value( out, prefix, el->name, NULL,
                                    el->args ? (char*)el->args->data : NULL );
            write_value( out, prefix, el->name, "label", el->val );
            break;
        case CDLG_INPUT:
        case CDLG_INPUT_LARGE:
        case CDLG_PASSWORD:
            if ( el->type == CDLG_INPUT_LARGE )
                str = multi_input_get_text( el->widgets->next->data );
            else
                // do not free
                str = (char*)gtk_entry_get_text( GTK_ENTRY( el->widgets->next->data ) );
            if ( el->args && ((char*)el->args->data)[0] == '@' )
                write_file_value( (char*)el->args->data, str );
            write_value( out, prefix, el->name, "default",
                                    el->args ? (char*)el->args->data : NULL );
            write_value( out, prefix, el->name, NULL, str );
            if ( el->type == CDLG_INPUT_LARGE )
                g_free( str );
            break;
        case CDLG_VIEWER:
            write_value( out, prefix, el->name, NULL,
                                    el->args ? (char*)el->args->data : NULL );
            break;
        case CDLG_EDITOR:
            write_value( out, prefix, el->name, NULL,
                                    el->args ? (char*)el->args->data : NULL );
            if ( el->args && el->args->next )
            {
                // save file
                write_value( out, prefix, el->name, "saved",
                                                    (char*)el->args->next->data );
                GtkTextIter iter, siter;
                GtkTextBuffer* buf = gtk_text_view_get_buffer(
                                GTK_TEXT_VIEW( el->widgets->next->data ) );
                gtk_text_buffer_get_start_iter( buf, &siter );
                gtk_text_buffer_get_end_iter( buf, &iter );
                str = gtk_text_buffer_get_text( buf, &siter, &iter, FALSE );
                write_file_value( (char*)el->args->next->data, str );
                g_free( str );
            }
            break;
        }
    }
}

static void on_dlg_close( GtkDialog* dlg )
{
    write_output( GTK_WIDGET( dlg ), NULL );
    destroy_dlg( GTK_WIDGET( dlg ) );
}

static void on_button_clicked( GtkButton *button, CustomElement* el ) 
{
    if ( el->args && el->args->next )
        // button has a command
        run_command( el->args->next );
    else
    {
        // no command
        write_output( el->widgets->data, el );
        destroy_dlg( el->widgets->data );
    }
}

static gboolean on_input_key_press( GtkWidget *entry, GdkEventKey* evt,
                                                      CustomElement* el )
{    
    int keymod = ( evt->state & ( GDK_SHIFT_MASK | GDK_CONTROL_MASK |
                 GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK ) );
                 
    if ( !keymod && ( evt->keyval == GDK_Return || evt->keyval == GDK_KP_Enter )
                        && el->type != CDLG_PASSWORD && el->args && el->args->next
                        && el->args->next->next && el->args->next->next->next )
    {
        // input has a command
        run_command( el->args->next->next->next );
        return TRUE;
    }
    else if ( !keymod && ( evt->keyval == GDK_Return || evt->keyval == GDK_KP_Enter )
                        && el->type == CDLG_PASSWORD && el->args && el->args->next )
    {
        // password has a command
        run_command( el->args->next );
        return TRUE;
    }
    else if ( !keymod && ( evt->keyval == GDK_Return || evt->keyval == GDK_KP_Enter ) )
    {
        // no command - find last (default) button and press it
        GList* elements = (GList*)g_object_get_data( G_OBJECT( el->widgets->data ),
                                                                    "elements" );
        GList* l;
        CustomElement* ell;
        CustomElement* el_button = NULL;
        for ( l = elements; l; l = l->next )
        {
            ell = (CustomElement*)l->data;
            if ( ell->type == CDLG_BUTTON )
                el_button = ell;
        }
        if ( el_button )
        {
            on_button_clicked( NULL, el_button );
            return TRUE;
        }
    }
    return FALSE;
}

static void update_element( CustomElement* el, GtkWidget* box )
{
    GtkWidget* w;
    GdkPixbuf* pixbuf;
    GtkWidget* dlg = (GtkWidget*)el->widgets->data;
    char* str;
    char* sep;
    int selstart, selend;
    struct stat64 statbuf;
    GtkTextBuffer* buf;
    GtkTextIter iter;

    GList* args = el->args;
    switch ( el->type )
    {
    case CDLG_TITLE:
        if ( args )
        {
            get_text_value( el, (char*)args->data, FALSE, TRUE );
            if ( el->val )
                gtk_window_set_title( GTK_WINDOW( dlg ), el->val );     
        }
        break;
    case CDLG_WINDOW_ICON:
        if ( args )
        {
            get_text_value( el, (char*)args->data, FALSE, TRUE );
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
            get_text_value( el, (char*)args->data, TRUE, TRUE );
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
    case CDLG_FREE_BUTTON:
        if ( args )
            get_text_value( el, (char*)args->data, FALSE, TRUE );
        // add button
        if ( !el->widgets->next )
        {
            w = gtk_button_new();
            gtk_button_set_use_underline( GTK_BUTTON( w ), TRUE );
            gtk_button_set_focus_on_click( GTK_BUTTON( w ), FALSE );
            el->widgets = g_list_append( el->widgets, w );
            if ( el->type == CDLG_BUTTON )
            {
                gtk_box_pack_start( GTK_BOX( GTK_DIALOG( dlg )->action_area ),
                                            GTK_WIDGET( w ), TRUE, TRUE, 0 );
                gtk_widget_grab_focus( w );
            }
            else
                gtk_box_pack_start( GTK_BOX( box ),
                                            GTK_WIDGET( w ), TRUE, TRUE, 0 );
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
    case CDLG_ICON:
    case CDLG_IMAGE:
        if ( args )
        {
            str = g_strdup( el->val );
            get_text_value( el, (char*)args->data, FALSE, TRUE );
            // if no change, don't update image if image_box present
            if ( !g_strcmp0( str, el->val ) && el->widgets->next )
            {
                g_free( str );
                break;
            }
            g_free( str );
        }
        // add hbox to hold image widget
        GtkWidget* image_box;
        if ( !el->widgets->next && box )
        {
            image_box = gtk_hbox_new( FALSE, 0 );
            el->widgets = g_list_append( el->widgets, image_box );
            gtk_box_pack_start( GTK_BOX( box ), GTK_WIDGET( image_box ), TRUE, TRUE, 4 );
        }
        // destroy old image
        if ( el->widgets->next && el->widgets->next->next && 
                                ( w = GTK_WIDGET( el->widgets->next->next->data ) ) )
        {
            gtk_widget_destroy( w );
            el->widgets = g_list_remove( el->widgets, w );
        }
        // add image
        if ( el->widgets->next && !el->widgets->next->next && el->val &&
                        ( image_box = GTK_WIDGET( el->widgets->next->data ) ) )
        {
            if ( el->type == CDLG_IMAGE )
                w = gtk_image_new_from_file( el->val );
            else
                w = gtk_image_new_from_icon_name( el->val, GTK_ICON_SIZE_DIALOG );
            gtk_box_pack_start( GTK_BOX( image_box ), GTK_WIDGET( w ), TRUE, TRUE, 0 );
            el->widgets = g_list_append( el->widgets, w );
            gtk_widget_show( w );
        }
        break;
    case CDLG_INPUT:
    case CDLG_INPUT_LARGE:
    case CDLG_PASSWORD:
        if ( !el->widgets->next && box )
        {
            // add input
            selstart = -1;
            selend = -1;
            if ( args )
            {
                // default text
                get_text_value( el, (char*)args->data, FALSE, TRUE );
                if ( args->next && el->type != CDLG_PASSWORD )
                {
                    // custom select region
                    selstart = atoi( (char*)args->next->data );
                    if ( args->next->next )
                        selend = atoi( (char*)args->next->next->data );
                }
            }
            if ( el->type == CDLG_INPUT_LARGE )
            {
                // multi-input
                GtkWidget* scroll = gtk_scrolled_window_new( NULL, NULL );
                w = GTK_WIDGET( multi_input_new( GTK_SCROLLED_WINDOW( scroll ),
                                                                el->val, FALSE ) );
                if ( selstart >= 0 )
                    multi_input_select_region( w, selstart, selend );
                gtk_box_pack_start( GTK_BOX( box ), GTK_WIDGET( scroll ), TRUE, TRUE, 4 );
            }
            else
            {
                // entry
                w = gtk_entry_new();
                gtk_entry_set_visibility( GTK_ENTRY( w ), el->type != CDLG_PASSWORD );
                if ( el->val )
                {
                    gtk_entry_set_text( GTK_ENTRY( w ), el->val );
                    if ( selstart >= 0 )
                        gtk_editable_select_region( GTK_EDITABLE( w ), selstart, selend );
                    else
                        gtk_editable_select_region( GTK_EDITABLE( w ), 0, -1 );                    
                }
                //PangoFontDescription* font_desc = pango_font_description_from_string(
                //                xset_get_s_panel( file_browser->mypanel, "font_path" ) );
                //gtk_widget_modify_font( entry, font_desc );
                //pango_font_description_free( font_desc );
                gtk_box_pack_start( GTK_BOX( box ), GTK_WIDGET( w ), TRUE, TRUE, 4 );
            }
            el->widgets = g_list_append( el->widgets, w );
            g_signal_connect( G_OBJECT( w ), "key-press-event",
                                            G_CALLBACK( on_input_key_press), el );
        }
        else if ( el->widgets->next && args && ((char*)args->data)[0] == '@' )
        {
            // update from file
            str = g_strdup( el->val );
            get_text_value( el, (char*)args->data, FALSE, TRUE );
            if ( g_strcmp0( str, el->val ) )
            {
                // value has changed from initial default, so update contents
                if ( el->type == CDLG_INPUT_LARGE )
                {
                    gtk_text_buffer_set_text( gtk_text_view_get_buffer( 
                                        GTK_TEXT_VIEW( el->widgets->next->data ) ),
                                        el->val ? el->val : "", -1 );
                    multi_input_select_region( el->widgets->next->data, 0, -1 );
                }
                else
                {
                    gtk_entry_set_text( GTK_ENTRY( el->widgets->next->data ), el->val );
                    gtk_editable_select_region( 
                                        GTK_EDITABLE( el->widgets->next->data ),
                                        0, -1 );                    
                }
            }
            g_free( str );
        }
        break;
    case CDLG_VIEWER:
    case CDLG_EDITOR:
        selstart = 0;
        // add text box
        if ( !el->widgets->next && box )
        {
            GtkWidget* scroll = gtk_scrolled_window_new( NULL, NULL );
            gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( scroll ),
                                        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
            w = gtk_text_view_new();
            gtk_text_view_set_wrap_mode( GTK_TEXT_VIEW( w ), GTK_WRAP_WORD_CHAR );
            gtk_text_view_set_editable( GTK_TEXT_VIEW( w ), el->type == CDLG_EDITOR );
            gtk_container_add ( GTK_CONTAINER ( scroll ), w );
            el->widgets = g_list_append( el->widgets, w );
            el->widgets = g_list_append( el->widgets, scroll );
            gtk_box_pack_start( GTK_BOX( box ), GTK_WIDGET( scroll ), TRUE, TRUE, 4 );
            // place mark at end
            buf = gtk_text_view_get_buffer( GTK_TEXT_VIEW( w ) );
            gtk_text_buffer_get_end_iter( buf, &iter);
            gtk_text_buffer_create_mark( buf, "end", &iter, FALSE );
            selstart = 1;  // indicates new
        }
        if ( args && el->type == CDLG_VIEWER && el->widgets->next )
        {
            // viewer
            buf = gtk_text_view_get_buffer(
                                GTK_TEXT_VIEW( el->widgets->next->data ) );
            if ( selstart && stat64( (char*)args->data, &statbuf ) != -1 
                                             && S_ISFIFO( statbuf.st_mode ) )
            {
                // watch pipe
                GIOChannel* channel = g_io_channel_new_file( (char*)args->data,
                                                                "r+", NULL );
                gint fd = g_io_channel_unix_get_fd( channel );
                //int fd = fcntl( g_io_channel_unix_get_fd( channel ), F_GETFL );
                if ( fd > 0 )
                {
                    fcntl( fd, F_SETFL,O_NONBLOCK );
                    g_io_add_watch_full( channel, G_PRIORITY_LOW,
                                    G_IO_IN	| G_IO_HUP | G_IO_NVAL | G_IO_ERR,
                                    (GIOFunc)cb_pipe_watch, el, NULL );
                }
            }
            else if ( g_file_test( (char*)args->data, G_FILE_TEST_IS_REGULAR ) )
            {
                // update viewer from file
                fill_buffer_from_file( el, buf, (char*)args->data, TRUE );
            }
            else
            {
                g_warning( _("file '%s' is not a regular file or a pipe"),
                                                            (char*)args->data );
            }
            // scroll
            if ( args->next && !g_strcmp0( (char*)args->next->data, "--scroll" ) )
            {
                //scroll to end if scrollbar is mostly down or new
                GtkAdjustment* adj = gtk_scrolled_window_get_vadjustment( 
                        GTK_SCROLLED_WINDOW( el->widgets->next->next->data ) );
                if ( selstart || adj->upper - adj->value < adj->page_size + 40 )
                    gtk_text_view_scroll_to_mark( GTK_TEXT_VIEW( el->widgets->next->data ),
                                          gtk_text_buffer_get_mark( buf, "end" ),
                                          0.0, FALSE, 0, 0 );
            }
        }
        else if ( args && selstart && el->type == CDLG_EDITOR && el->widgets->next )
        {
            // new editor            
            buf = gtk_text_view_get_buffer(
                                    GTK_TEXT_VIEW( el->widgets->next->data ) );
            fill_buffer_from_file( el, buf, (char*)args->data, FALSE );
        }
        break;
    }
}

static void build_dialog( GList* elements )
{
    GList* l;
    CustomElement* el;
    CustomElement* focus_el = NULL;
    const char* arg;
    GList* args;
    char* str;
    
    // create dialog
    GtkWidget* dlg = gtk_dialog_new();
    GList* boxes = g_list_append( NULL, GTK_DIALOG( dlg )->vbox );
    GtkWidget* box = (GtkWidget*)boxes->data;
    g_object_set_data( G_OBJECT( dlg ), "elements", elements );
    g_signal_connect( G_OBJECT( dlg ), "destroy", G_CALLBACK( on_dlg_close ), NULL );

    // add elements
    for ( l = elements; l; l = l->next )
    {
        el = (CustomElement*)l->data;
        el->widgets = g_list_append( NULL, dlg );
        update_element( el, box );
        if ( !focus_el && ( el->type == CDLG_INPUT
                         || el->type == CDLG_INPUT_LARGE
                         || el->type == CDLG_EDITOR
                         || el->type == CDLG_PASSWORD ) )
            focus_el = el;
    }
    // focus input
    if ( focus_el && focus_el->widgets->next )
    {
        gtk_widget_grab_focus( focus_el->widgets->next->data );
        if ( focus_el->type == CDLG_INPUT && focus_el->args && focus_el->args->next )
        {
            // grab_focus causes all text to be selected, so re-select region
            int selstart = 0;
            int selend = 0;
            selstart = atoi( (char*)focus_el->args->next->data );
            if ( focus_el->args->next->next )
                selend = atoi( (char*)focus_el->args->next->next->data );
            if ( selstart >= 0 )
                gtk_editable_select_region( 
                            GTK_EDITABLE( focus_el->widgets->next->data ),
                            selstart, selend );
        }
    }
    gtk_widget_show_all( dlg );    
    g_list_free( boxes );
}

int custom_dialog_init( int argc, char *argv[] )
{
    int ac, i, j;
    GList* elements = NULL;
    CustomElement* el = NULL;
    GList* l;
    char* num;
    char* str;
    int type_count[ G_N_ELEMENTS( cdlg_option ) / 3 ] = { 0 };

    for ( ac = 2; ac < argc; ac++ )
    {
        if ( !g_utf8_validate( argv[ac], -1, NULL ) )
        {
            fprintf( stderr, _("spacefm: argument is not valid UTF-8\n") );
            free_elements( elements );
            return 1;
        }        
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
                    el->widgets = NULL;
                    el->monitor = NULL;
                    el->callback = NULL;
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
}

