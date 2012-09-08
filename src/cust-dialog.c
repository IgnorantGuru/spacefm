/*
 *      cust-dialog.c
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#include "settings.h"
#include "cust-dialog.h"

#define DEFAULT_TITLE "SpaceFM Dialog"
#define DEFAULT_ICON "spacefm-48-pyramid-blue"
#define DEFAULT_PAD 4
#define DEFAULT_WIDTH 450
#define DEFAULT_HEIGHT 100
#define DEFAULT_LARGE_WIDTH 600
#define DEFAULT_LARGE_HEIGHT 400
#define MAX_LIST_COLUMNS 32
#define BASH_VALID "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"
#define DEFAULT_MANUAL "http://ignorantguru.github.com/spacefm/spacefm-manual-en.html#dialog"

static void update_element( CustomElement* el, GtkWidget* box, GSList** radio,
                                                                    int pad );
static char* replace_vars( CustomElement* el, char* value, char* xvalue );
static void fill_buffer_from_file( CustomElement* el, GtkTextBuffer* buf,
                                   char* path, gboolean watch );
static void write_source( GtkWidget* dlg, CustomElement* el_pressed,
                                                            FILE* out );
static gboolean destroy_dlg( GtkWidget* dlg );
static void on_button_clicked( GtkButton *button, CustomElement* el );
void on_combo_changed( GtkComboBox* box, CustomElement* el );
static gboolean on_timeout_timer( CustomElement* el );
static gboolean press_last_button( GtkWidget* dlg );
static void on_dlg_close( GtkDialog* dlg );

GtkWidget* signal_dialog = NULL;  // make this a list if supporting multiple dialogs

static void set_font( GtkWidget* w, const char* font )
{
    if ( w && font )
    {
        PangoFontDescription* font_desc = pango_font_description_from_string( font );
        gtk_widget_modify_font( w, font_desc );
        pango_font_description_free( font_desc );
    }
}

static void dlg_warn( const char* msg, const char* a, const char* b )
{
    char* str = g_strdup_printf( "** spacefm-dialog: %s\n", msg );
    fprintf( stderr, str, a, b );
    g_free( str );
}

static void get_window_size( GtkWidget* dlg, int* width, int* height )
{
    *width = dlg->allocation.width;
    *height = dlg->allocation.height;
}

static void get_width_height_pad( char* val, int* width, int* height, int* pad )
{   // modifies val
    char* str;
    char* sep;
    int i;
    
    *width = *height = -1;
    if ( val )
    {
        if ( sep = strchr( val, 'x' ) ) 
            sep[0] = '\0';
        else if ( sep = strchr( val, ' ' ) )
            sep[0] = '\0';
        *width = atoi( val );
        if ( sep )
        {
            sep[0] = 'x';
            str = sep + 1;
            if ( sep = strchr( str, 'x' ) ) 
                sep[0] = '\0';
            else if ( sep = strchr( str, ' ' ) )
                sep[0] = '\0';
            *height = atoi( str );
            if ( sep )
            {
                sep[0] = ' ';
                i = atoi( sep + 1 );
                // ignore pad == -1
                if ( i != -1 && pad )
                {
                    *pad = i;
                    if ( *pad < 0 ) *pad = 0;
                }
            }
        }
    }
    if ( *width <= 0 ) *width = -1;
    if ( *height <= 0 ) *height = -1;
}

static char* unescape( const char* t )
{
    if ( !t )
        return NULL;
    
    char* s = g_strdup( t );

    int i = 0, j = 0;    
    while ( t[i] )
    {
        switch ( t[i] )
        {
        case '\\':
            switch( t[++i] )
            {
            case 'n':
                s[j] = '\n';
                break;
            case 't':
                s[j] = '\t';
                break;                
            case '\\':
                s[j] = '\\';
                break;
            case '\"':
                s[j] = '\"';
                break;
            default:
                // copy
                s[j++] = '\\';
                s[j] = t[i];
            }
            break;            
        default:
            s[j] = t[i];
        }
        ++i;
        ++j;
    }
    s[j] = t[i];  // null char
    return s;
}

static void fill_combo_box( CustomElement* el, GList* arglist )
{
    GList* l;
    GList* args;
    char* arg;
    char* str;
    GtkTreeIter iter;
    GtkTreeModel* model;
    char* default_value = NULL;
    int default_row = -1;
    int set_default = -1;
    
    if ( !el->widgets->next )
        return;
    GtkWidget* combo = (GtkWidget*)el->widgets->next->data;
    if ( !GTK_IS_COMBO_BOX( combo ) )
        return;
        
    // prepare default value
    if ( el->val )
    {
        if ( el->val[0] == '+' && atoi( el->val + 1 ) >= 0 )
            default_row = atoi( el->val + 1 );
        else
            default_value = el->val;
    }

    // clear list
    model = gtk_combo_box_get_model( GTK_COMBO_BOX( combo ) );
    while ( gtk_tree_model_get_iter_first( model, &iter ) )
        gtk_list_store_remove( GTK_LIST_STORE( model ), &iter );
    if ( el->type == CDLG_COMBO )
        gtk_entry_set_text( GTK_ENTRY( gtk_bin_get_child( GTK_BIN( combo ) ) ),
                                                                        "" );

    // fill list
    args = arglist;
    int row = 0;
    while ( args )
    {
        arg = (char*)args->data;
        args = args->next;
        if ( !strcmp( arg, "--" ) )
            break;
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( combo ), arg );
        if ( row == default_row || !g_strcmp0( arg, default_value ) )
            set_default = row;
        row++;
    }
    
    // set default
    if ( set_default != -1 )
    {
        if ( el->type == CDLG_DROP )
            g_signal_handlers_block_matched( el->widgets->next->data,
                                            G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                            on_combo_changed, el );
        gtk_combo_box_set_active( GTK_COMBO_BOX( combo ), set_default );
        if ( el->type == CDLG_DROP )
            g_signal_handlers_unblock_matched( el->widgets->next->data,
                                            G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                            on_combo_changed, el );
    }
    else if ( default_value && el->type == CDLG_COMBO )
        gtk_entry_set_text( GTK_ENTRY( gtk_bin_get_child( GTK_BIN( combo ) ) ),
                                                            default_value );
}

static void select_in_combo_box( CustomElement* el, const char* value )
{
    GtkTreeIter iter;
    GtkTreeModel* model;
    char* str;
    
    if ( !el->widgets->next )
        return;
    GtkWidget* combo = (GtkWidget*)el->widgets->next->data;
    if ( !GTK_IS_COMBO_BOX( combo ) )
        return;
        
    model = gtk_combo_box_get_model( GTK_COMBO_BOX( combo ) );
    if ( !model )
        return;
    
    if ( !gtk_tree_model_get_iter_first( model, &iter ) )
        return;
    
    do
    {
        gtk_tree_model_get( model, &iter, 0, &str, -1 );
        if ( !g_strcmp0( str, value ) )
        {
            gtk_combo_box_set_active_iter( GTK_COMBO_BOX( combo ), &iter );
            g_free( str );
            break;
        }
        g_free( str );
    }
    while ( gtk_tree_model_iter_next( model, &iter ) );
}

char* get_column_value( GtkTreeModel* model, GtkTreeIter* iter, int col_index )
{
    char* str = NULL;
    gint64 i64;
    gdouble d;
    int i;
    switch ( gtk_tree_model_get_column_type( model, col_index ) )
    {
        case G_TYPE_INT64:
            gtk_tree_model_get( model, iter, col_index, &i64, -1 );
            str = g_strdup_printf( "%d", i64 );
            break;
        case G_TYPE_INT:
            gtk_tree_model_get( model, iter, col_index, &i, -1 );
            str = g_strdup_printf( "%d", i );
            break;
        case G_TYPE_DOUBLE:
            gtk_tree_model_get( model, iter, col_index, &d, -1 );
            str = g_strdup_printf( "%lf", d );
            break;
        case G_TYPE_STRING:
            gtk_tree_model_get( model, iter, col_index, &str, -1 );
    }
    return str;
}

char* get_tree_view_selected( CustomElement* el, const char* prefix )
{
    GtkTreeIter iter;
    GtkTreeModel* model;
    GtkTreeSelection* tree_sel;
    GtkTreePath* tree_path;
    char* selected = NULL;
    char* indices = NULL;
    char* str;

    if ( !el->widgets->next )
        goto _return_value;
    GtkWidget* view = (GtkWidget*)el->widgets->next->data;
    if ( !GTK_IS_TREE_VIEW( view ) )
        goto _return_value;
    
    tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( view ) );
    model = gtk_tree_view_get_model( GTK_TREE_VIEW( view ) );
    if ( !GTK_IS_TREE_MODEL( model ) )
        goto _return_value;

    int row = -1;
    char* value;
    gboolean valid_iter = gtk_tree_model_get_iter_first( model, &iter );
    while ( valid_iter )
    {
        row++;
        if ( gtk_tree_selection_iter_is_selected( tree_sel, &iter ) )
        {
            str = get_column_value( model, &iter, 0 );
            value = bash_quote( str );
            g_free( str );
            
            str = selected;
            selected = g_strdup_printf( "%s%s%s",
                                        str ? str : "",
                                        prefix ? ( str ? "\n" : "" ) : " ",
                                        value );
            g_free( value );
            g_free( str );
            
            str = indices;
            indices = g_strdup_printf( "%s%s%d",
                                        str ? str : "",
                                        str ? " " : "",
                                        row );
            g_free( str );
            if ( el->type == CDLG_LIST )
                break;
        }
        valid_iter = gtk_tree_model_iter_next( model, &iter );
    }
_return_value:
    if ( !prefix )
    {
        g_free( indices );
        return selected ? selected : g_strdup( "" );
    }
    if ( !selected )
        str = g_strdup_printf( "%s_%s=''\n%s_%s_index='%s'\n",
                                        prefix, el->name,
                                        prefix, el->name,
                                        el->type == CDLG_LIST ? "-1" : "" );
    else if ( el->type == CDLG_LIST )
        str = g_strdup_printf( "%s_%s=%s\n%s_%s_index=%s\n",
                                        prefix, el->name,
                                        selected,
                                        prefix, el->name,
                                        indices );
    else
        str = g_strdup_printf( "%s_%s=(\n%s )\n%s_%s_index=( %s )\n",
                                        prefix, el->name,
                                        selected,
                                        prefix, el->name,
                                        indices );
    g_free( selected );
    g_free( indices );
    return str;
}

static void fill_tree_view( CustomElement* el, GList* arglist )
{
    GList* l;
    GList* args;
    char* arg;
    char* sep;
    char* str;
    GtkTreeIter iter;
    GtkListStore* list;
    GtkTreeModel* model;
    GtkTreeViewColumn* col;
    GtkCellRenderer *renderer;
    GType coltypes[MAX_LIST_COLUMNS];
    int colcount;
    int i;
    gboolean headers = FALSE;
    
    if ( !el->widgets->next )
        return;
    GtkWidget* view = (GtkWidget*)el->widgets->next->data;
    if ( !GTK_IS_TREE_VIEW( view ) )
        return;
        
    // clear list
    model = gtk_tree_view_get_model( GTK_TREE_VIEW( view ) );
    if ( model )
        gtk_list_store_clear( GTK_LIST_STORE( model ) );
    // remove columns
    args = gtk_tree_view_get_columns( GTK_TREE_VIEW( view ) );
    for ( l = args; l; l = l->next )
    {
        gtk_tree_view_remove_column( GTK_TREE_VIEW( view ),
                        GTK_TREE_VIEW_COLUMN( (GtkTreeViewColumn*)l->data ) );
    }
    g_list_free( args );
    
    // fill list
    args = arglist;
    col = NULL;
    colcount = 0;
    while ( args )
    {
        arg = (char*)args->data;
        args = args->next;
        if ( !strcmp( arg, "--" ) )
        {
            el->cmd_args = args;
            break;
        }
        if ( g_str_has_prefix( arg, "--colwidth" ) )
        {
            str = NULL;
            if ( g_str_has_prefix( arg, "--colwidth=" ) )
                str = arg + strlen( "--colwidth=" );
            else if ( !strcmp( arg, "--colwidth" ) && args )
            {
                str = (char*)args->data;    // next argument
                args = args->next;          // skip next argument
            }
            if ( col && str && ( i = atoi( str ) ) > 0 )
                gtk_tree_view_column_set_fixed_width( col, i );
            continue;
        }
        if ( arg[0] == '^' || !col )
        {
            // new column start
            if ( colcount == MAX_LIST_COLUMNS )
            {
                str = g_strdup_printf( _("Too many columns (>%d) in %s"), 
                                                MAX_LIST_COLUMNS, el->name );
                dlg_warn( str, NULL, NULL );
                g_free( str );
                break;
            }
            col = gtk_tree_view_column_new();
            gtk_tree_view_column_set_sort_indicator( col, TRUE );
            gtk_tree_view_column_set_sort_column_id( col, colcount );
            gtk_tree_view_append_column( GTK_TREE_VIEW( view ), col );
            //if ( colcount == 0 )
                gtk_tree_view_column_set_expand ( col, TRUE );
            gtk_tree_view_column_set_sizing( col, GTK_TREE_VIEW_COLUMN_FIXED );
            gtk_tree_view_column_set_resizable( col, TRUE );
            gtk_tree_view_column_set_min_width( col, 50 );
            colcount++;
            coltypes[colcount - 1] = G_TYPE_STRING;
            if ( arg[0] == '^' )
            {
                // column header
                sep = strrchr( arg, ':' );
                if ( sep && sep[1] == '\0' )
                    sep = NULL;
                if ( sep )
                    sep[0] = '\0';
                gtk_tree_view_column_set_title( col, arg + 1 );
                if ( !strcmp( arg + 1, "HIDE" ) && colcount == 1 )
                    gtk_tree_view_column_set_visible( col, FALSE );
                if ( sep )
                {
                    sep[0] = ':';
                    sep++;
                    if  ( !strcmp( sep, "progress" )
                                    && gtk_tree_view_column_get_visible( col ) )
                        coltypes[colcount - 1] = G_TYPE_INT;
                    else if ( !strcmp( sep, "int" ) )
                        coltypes[colcount - 1] = G_TYPE_INT64;
                    else if ( !strcmp( sep, "double" ) )
                        coltypes[colcount - 1] = G_TYPE_DOUBLE;
                }
                headers = TRUE;
            }
            // pack renderer
            switch ( coltypes[colcount - 1] )
            {
                case G_TYPE_STRING:
                case G_TYPE_INT64:
                case G_TYPE_DOUBLE:
                    renderer = gtk_cell_renderer_text_new();
                    gtk_tree_view_column_pack_start( col, renderer, TRUE );
                    gtk_tree_view_column_set_attributes( col, renderer,
                                         "text", colcount - 1, NULL );
                    gtk_object_set( GTK_OBJECT( renderer ),
                                /*"editable", TRUE,*/
                                "ellipsize", PANGO_ELLIPSIZE_END, NULL );
                    break;
                case G_TYPE_INT:
                    renderer = gtk_cell_renderer_progress_new();
                    gtk_tree_view_column_pack_start( col, renderer, TRUE );
                    gtk_tree_view_column_set_attributes( col, renderer,
                                         "value", colcount - 1, NULL );
            }
        }
    }
    gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( view ), headers );
    if ( colcount == 0 )
        return;

    // list store
    list = gtk_list_store_newv( colcount, coltypes );
    gtk_tree_view_set_model( GTK_TREE_VIEW( view ), 
                                        GTK_TREE_MODEL( list ) );
    int colx = 0;
    gboolean start = FALSE;
    gboolean valid_iter = FALSE;
    args = arglist;
    while ( args )
    {
        arg = (char*)args->data;
        args = args->next;
        if ( !strcmp( arg, "--" ) )
            break;
        if ( arg[0] == '^' )
        {
            // new column start
            if ( start )
            {
                if ( colx == MAX_LIST_COLUMNS - 1 )
                    break;
                colx++;
                // set iter to first row  - false if no rows
                valid_iter = gtk_tree_model_get_iter_first( 
                                            GTK_TREE_MODEL( list ), &iter );
            }
        }
        else if ( g_str_has_prefix( arg, "--colwidth=" ) )
            continue;
        else if ( g_str_has_prefix( arg, "--colwidth" ) )
        {
            args = args->next;
            continue;
        }
        else
        {
            if ( colx == 0 )
            {
                // first column - add row
                start = TRUE;
                gtk_list_store_append( list, &iter );
            }
            else
            {
                // non-first column - add row if needed
                if ( !valid_iter )
                {
                    // no row for this data, so add a row
                    // non-first column was longer than first column
                    gtk_list_store_append( list, &iter );
                }
            }
            // set row data
            if ( coltypes[colx] == G_TYPE_INT )
            {
                i = atoi( arg );
                if ( i < 0 ) i = 0;
                if ( i > 100 ) i = 100;
                gtk_list_store_set( list, &iter, colx, i, -1 );
            }
            else if ( coltypes[colx] == G_TYPE_INT64 )
                gtk_list_store_set( list, &iter, colx, atoi( arg ), -1 );
            else if ( coltypes[colx] == G_TYPE_DOUBLE )
                gtk_list_store_set( list, &iter, colx, strtod( arg, NULL ), -1 );
            else
                gtk_list_store_set( list, &iter, colx, arg, -1 );
            if ( colx != 0 )
                valid_iter = gtk_tree_model_iter_next( GTK_TREE_MODEL( list ), &iter );
        }
    }
    
    // resize columns - none of this seems to do anything
    gtk_tree_view_columns_autosize( GTK_TREE_VIEW( view ) ); // doc: only works after realize
/*
    args = gtk_tree_view_get_columns( GTK_TREE_VIEW( view ) );
    for ( l = args; l; l = l->next )
    {
        gtk_tree_view_column_queue_resize( 
                    GTK_TREE_VIEW_COLUMN( (GtkTreeViewColumn*)l->data ) );
    }
    g_list_free( args );
*/
}

static void select_in_tree_view( CustomElement* el, const char* value,
                                                            gboolean select )
{
    GtkTreeModel* model;
    GtkTreePath* tree_path;
    GtkTreeIter iter;
    GtkTreeSelection* tree_sel;
    char* str;

    if ( !el || !el->widgets->next || !value )
        return;

    GtkWidget* view = (GtkWidget*)el->widgets->next->data;
    if ( !GTK_IS_TREE_VIEW( view ) )
        return;
        
    model = gtk_tree_view_get_model( GTK_TREE_VIEW( view ) );
    if ( !model )
        return;
    
    tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( view ) );

    if ( value[0] == '\0' )
    {
        if ( select )
        {
            if ( el->type == CDLG_MLIST )
                gtk_tree_selection_select_all( tree_sel );
            else if ( gtk_tree_model_get_iter_first( model, &iter ) )
            {
                // select first
                tree_path = gtk_tree_model_get_path( model, &iter );
                gtk_tree_selection_select_path( tree_sel, tree_path );
                gtk_tree_view_set_cursor( GTK_TREE_VIEW( view ),
                                            tree_path, NULL, FALSE);
                gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW( view ),
                                            tree_path, NULL, TRUE, .25, 0 );
            }
        }
        else
            gtk_tree_selection_unselect_all( tree_sel );
        return;
    }

    if ( !gtk_tree_model_get_iter_first( model, &iter ) )
        return;
    
    do
    {
        str = get_column_value( model, &iter, 0 );
        if ( !g_strcmp0( str, value ) )
        {
            if ( !!gtk_tree_selection_iter_is_selected( tree_sel, &iter )
                                                                    != !!select )
            {
                tree_path = gtk_tree_model_get_path( model, &iter );
                if ( select )
                {
                    gtk_tree_selection_select_path( tree_sel, tree_path );
                    // scroll and set cursor
                    if ( el->type == CDLG_LIST )
                        gtk_tree_view_set_cursor( GTK_TREE_VIEW( view ),
                                                    tree_path, NULL, FALSE);
                    gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW( view ),
                                                    tree_path, NULL, TRUE, .25, 0 );
                }
                else
                    gtk_tree_selection_unselect_path( tree_sel, tree_path );
            }
            g_free( str );
            break;
        }
        g_free( str );
    }
    while ( gtk_tree_model_iter_next( model, &iter ) );
}

GList* args_from_file( const char* path )
{
    char line[ 2048 ];
    GList* args = NULL;

    FILE* file = fopen( path, "r" );
    if ( !file )
    {
        dlg_warn( _("error reading file %s: %s"), path,
                                                    g_strerror( errno ) );
        return NULL;
    }
    while ( fgets( line, sizeof( line ), file ) )
    {
        if ( !g_utf8_validate( line, -1, NULL ) )
        {
            dlg_warn( _("file '%s' contents are not valid UTF-8"), path, NULL );
            g_list_foreach( args, (GFunc)g_free, NULL );
            g_list_free( args );
            return NULL;
        }
        strtok( line, "\r\n" );
        if ( !strcmp( line, "--" ) )
            break;
        args = g_list_prepend( args, g_strdup( line ) );
    }
    fclose( file );
    return ( args = g_list_reverse( args ) );
}

static CustomElement* el_from_name( CustomElement* el, const char* name )
{
    GList* l;
    
    if ( !el || !name )
        return NULL;
        
    GList* elements = (GList*)g_object_get_data( G_OBJECT( el->widgets->data ),
                                                                "elements" );
    CustomElement* el_name = NULL;
    for ( l = elements; l; l = l->next )
    {
        if ( !strcmp( ((CustomElement*)l->data)->name, name ) )
        {
            el_name = (CustomElement*)l->data;
            break;
        }
    }
    return el_name;
}

static void set_element_value( CustomElement* el, const char* name,
                                                  char* value )
{
    GtkWidget* dlg = (GtkWidget*)el->widgets->data;
    GtkWidget* w;
    GdkPixbuf* pixbuf;
    GtkWidget* image_box;
    GtkTextBuffer* buf;
    char* sep;
    char* str;
    int i, width, height;
    GList* l;
    
    if ( !el || !name || !value )
        return;

    CustomElement* el_name = el_from_name( el, name );
    if ( !el_name )
    {
        dlg_warn( _("Cannot set missing element '%s'"), name, NULL );
        return;
    }
    switch ( el_name->type )
    {
    case CDLG_TITLE:
        gtk_window_set_title( GTK_WINDOW( dlg ), value );
        g_free( el_name->val );
        el_name->val = g_strdup( value );
        break;
    case CDLG_WINDOW_ICON:
        if ( value[0] != '\0' )
            pixbuf = gtk_icon_theme_load_icon( gtk_icon_theme_get_default(),
                            value, 16, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );
        else
            pixbuf = NULL;
        gtk_window_set_icon( GTK_WINDOW( dlg ), pixbuf );  
        g_free( el_name->val );
        el_name->val = g_strdup( value );
        break;
    case CDLG_LABEL:
        if ( el_name->widgets->next
                        && ( w = GTK_WIDGET( el_name->widgets->next->data ) ) )
        {
            g_free( el_name->val );
            el_name->val = unescape( value );
            if ( el_name->val[0] == '~' )
                gtk_label_set_markup_with_mnemonic( GTK_LABEL( w ),
                                                            el_name->val + 1 );
            else
                gtk_label_set_text( GTK_LABEL( w ), el_name->val );
        }
        break;
    case CDLG_BUTTON:
    case CDLG_FREE_BUTTON:
        if ( el_name->widgets->next
                        && ( w = GTK_WIDGET( el_name->widgets->next->data ) ) )
        {
            g_free( el_name->val );
            el_name->val = unescape( value );
            if ( sep = strrchr( el_name->val, ':' ) )
                sep[0] = '\0';
            else
                sep = NULL;
            gtk_button_set_image( GTK_BUTTON( w ), NULL );
            if ( !sep &&  ( !g_strcmp0( el_name->val, "ok" )
                         || !g_strcmp0( el_name->val, "cancel" )
                         || !g_strcmp0( el_name->val, "close" )
                         || !g_strcmp0( el_name->val, "open" )
                         || !g_strcmp0( el_name->val, "yes" )
                         || !g_strcmp0( el_name->val, "no" )
                         || !g_strcmp0( el_name->val, "apply" )
                         || !g_strcmp0( el_name->val, "delete" )
                         || !g_strcmp0( el_name->val, "edit" )
                         || !g_strcmp0( el_name->val, "save" )
                         || !g_strcmp0( el_name->val, "stop" ) ) )
            {
                // stock button
                gtk_button_set_use_stock( GTK_BUTTON( w ), TRUE );
                str = g_strdup_printf( "gtk-%s", el_name->val );
                gtk_button_set_label( GTK_BUTTON( w ), str );
                g_free( str );
            }
            else
            {
                // custom button
                gtk_button_set_use_stock( GTK_BUTTON( w ), FALSE );
                gtk_button_set_label( GTK_BUTTON( w ), el_name->val );
            }
            // set icon
            if ( sep && sep[1] != '\0' )
                gtk_button_set_image( GTK_BUTTON( w ), xset_get_image( sep + 1,
                                                        GTK_ICON_SIZE_BUTTON ) );
            if ( sep )
                sep[0] = ':';
        }
        break;
    case CDLG_ICON:
    case CDLG_IMAGE:
        // destroy old image
        if ( el_name->widgets->next && el_name->widgets->next->next && 
                        ( w = GTK_WIDGET( el_name->widgets->next->next->data ) ) )
        {
            gtk_widget_destroy( w );
            el_name->widgets = g_list_remove( el_name->widgets, w );
        }
        // add image
        if ( el_name->widgets->next && !el_name->widgets->next->next && value &&
                        ( image_box = GTK_WIDGET( el_name->widgets->next->data ) ) )
        {
            if ( el_name->type == CDLG_IMAGE )
                w = gtk_image_new_from_file( value );
            else
                w = gtk_image_new_from_icon_name( value, GTK_ICON_SIZE_DIALOG );
            gtk_container_add( GTK_CONTAINER( image_box ), GTK_WIDGET( w ) );
            el_name->widgets = g_list_append( el_name->widgets, w );
            gtk_widget_show( w );
            g_free( el_name->val );
            el_name->val = g_strdup( value );
        }
        break;
    case CDLG_INPUT:
    case CDLG_INPUT_LARGE:
    case CDLG_PASSWORD:
        if ( el_name->type == CDLG_INPUT_LARGE )
        {
            gtk_text_buffer_set_text( gtk_text_view_get_buffer( 
                                GTK_TEXT_VIEW( el_name->widgets->next->data ) ),
                                value, -1 );
            multi_input_select_region( el_name->widgets->next->data, 0, -1 );
        }
        else
        {
            gtk_entry_set_text( GTK_ENTRY( el_name->widgets->next->data ), value );
            gtk_editable_select_region( 
                                GTK_EDITABLE( el_name->widgets->next->data ),
                                0, -1 );                    
        }
        g_free( el_name->val );
        el_name->val = g_strdup( value );
        break;
    case CDLG_VIEWER:
    case CDLG_EDITOR:
        if ( !g_file_test( value, G_FILE_TEST_IS_REGULAR ) )
        {
            dlg_warn( _("file '%s' is not a regular file"), value, NULL );
            break;
        }
        if ( el_name->type == CDLG_VIEWER && el_name->widgets->next )
        {
            // viewer
            buf = gtk_text_view_get_buffer(
                                GTK_TEXT_VIEW( el_name->widgets->next->data ) );
            // update viewer from file
            fill_buffer_from_file( el_name, buf, value, FALSE );
            // scroll
            if ( el_name->option )
            {
                //scroll to end if scrollbar is mostly down or new
                GtkAdjustment* adj = gtk_scrolled_window_get_vadjustment( 
                        GTK_SCROLLED_WINDOW( el_name->widgets->next->next->data ) );
                if ( adj->upper - adj->value < adj->page_size + 40 )
                    gtk_text_view_scroll_to_mark( GTK_TEXT_VIEW( 
                                                el_name->widgets->next->data ),
                                          gtk_text_buffer_get_mark( buf, "end" ),
                                          0.0, FALSE, 0, 0 );
            }
            g_free( el_name->val );
            el_name->val = g_strdup( value );
        }
        else if ( el_name->type == CDLG_EDITOR && el_name->widgets->next )
        {
            // new editor            
            buf = gtk_text_view_get_buffer(
                                    GTK_TEXT_VIEW( el_name->widgets->next->data ) );
            fill_buffer_from_file( el_name, buf, value, FALSE );
            g_free( el_name->val );
            el_name->val = g_strdup( value );
        }
        break;
    case CDLG_CHECKBOX:
    case CDLG_RADIO:
        if ( el_name->widgets->next )
        {
            if ( !strcmp( value, "1") || !strcmp( value, "true" ) )
                gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(
                                            el_name->widgets->next->data ), TRUE );
            else if ( !strcmp( value, "0") || !strcmp( value, "false" ) )
                gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(
                                            el_name->widgets->next->data ), FALSE );
            else
            {
                // update option label
                str = unescape( value );
                gtk_button_set_label( GTK_BUTTON( el_name->widgets->next->data ),
                                                                        str );
                g_free( str );
            }
        }
        break;
    case CDLG_DROP:
    case CDLG_COMBO:
        if ( el_name->widgets->next )
        {
            if ( g_file_test( value, G_FILE_TEST_IS_REGULAR ) )
            {
                l = args_from_file( value );
                // fill list from args
                fill_combo_box( el_name, l );
                // free temp args
                g_list_foreach( l, (GFunc)g_free, NULL );
                g_list_free( l );
            }
            else if ( el_name->type == CDLG_COMBO )
                gtk_entry_set_text( GTK_ENTRY( gtk_bin_get_child( GTK_BIN(
                                    el_name->widgets->next->data ) ) ), value );
            else
                select_in_combo_box( el_name, value );
        }
        break;
    case CDLG_LIST:
    case CDLG_MLIST:
        if ( el_name->widgets->next )
        {
            if ( g_file_test( value, G_FILE_TEST_IS_REGULAR ) )
            {
                l = args_from_file( value );
                // fill list from args
                fill_tree_view( el_name, l );
                // free temp args
                g_list_foreach( l, (GFunc)g_free, NULL );
                g_list_free( l );
            }
            else
                dlg_warn( _("file '%s' is not a regular file"), value, NULL );
        }
        break;
/*
        if ( el_name->widgets->next )
        {
            gtk_statusbar_push( GTK_STATUSBAR( el_name->widgets->next->data ), 0,
            *                                                           value );
        }
        break;
*/
    case CDLG_PROGRESS:
        if ( el_name->widgets->next )
        {
            if ( !g_strcmp0( value, "pulse" ) || value[0] == '\0' )
            {
                gtk_progress_bar_pulse( GTK_PROGRESS_BAR( el_name->widgets->next->data ) );
                gtk_progress_bar_set_text( 
                                    GTK_PROGRESS_BAR( el_name->widgets->next->data ),
                                    NULL );
            }
            else
            {
                if ( el_name->timeout )
                {
                    g_source_remove( el_name->timeout );
                    el_name->timeout = 0;
                }
                i = value ? atoi( value ) : 0;
                if ( i < 0 ) i = 0;
                if ( i > 100 ) i = 100;
                str = g_strdup_printf( "%d %%", i );
                gtk_progress_bar_set_fraction( 
                                    GTK_PROGRESS_BAR( el_name->widgets->next->data ),
                                    (gdouble)i / 100 );
                gtk_progress_bar_set_text( 
                                    GTK_PROGRESS_BAR( el_name->widgets->next->data ),
                                    str );
                g_free( str );
            }
        }
        break;
    case CDLG_WINDOW_SIZE:
        width = -1;
        height = -1;
        get_width_height_pad( value, &width, &height, NULL );
        if ( width > 0 && height > 0 )
        {
            gtk_window_resize( GTK_WINDOW( dlg ), width, height );
            gtk_window_set_position( GTK_WINDOW( dlg ),
                                                    GTK_WIN_POS_CENTER_ALWAYS );
        }
        else
            dlg_warn( _("Dynamic resize requires width and height > 0"), NULL, NULL );
        break;
    case CDLG_TIMEOUT:
        if ( el_name->widgets->next && el_name->timeout )
        {
            g_source_remove( el_name->timeout );
            el_name->option = atoi( value ) + 1;
            if ( el_name->option <= 1 )
                el_name->option = 21;
            on_timeout_timer( el_name );
            el_name->timeout = g_timeout_add( 1000, (GSourceFunc)on_timeout_timer,
                                                                        el_name );
        }
        break;
    case CDLG_CHOOSER:
        if ( el_name->widgets->next )
        {
            i = gtk_file_chooser_get_action( GTK_FILE_CHOOSER( 
                                        el_name->widgets->next->data ) );
            if ( i == GTK_FILE_CHOOSER_ACTION_SAVE ||
                                i == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER )
            {
                if ( strchr( value, '/' ) )
                {
                    str = g_path_get_dirname( value );
                    gtk_file_chooser_set_current_folder( 
                                    GTK_FILE_CHOOSER( el_name->widgets->next->data ),
                                                                    str );
                    g_free( str );
                    str = g_path_get_basename( value );
                    gtk_file_chooser_set_current_name( 
                                    GTK_FILE_CHOOSER( el_name->widgets->next->data ),
                                                                    str );
                    g_free( str );
                }
                else
                    gtk_file_chooser_set_current_name( 
                                    GTK_FILE_CHOOSER( el_name->widgets->next->data ),
                                                                    value );
            }
            else if ( g_file_test( value, G_FILE_TEST_IS_DIR ) )
                gtk_file_chooser_set_current_folder( 
                                GTK_FILE_CHOOSER( el_name->widgets->next->data ),
                                                                    value );
            else
                gtk_file_chooser_set_filename( 
                                    GTK_FILE_CHOOSER( el_name->widgets->next->data ),
                                                                    value );
        }
        break;
    }
}

static char* get_element_value( CustomElement* el, const char* name )
{
    int width, height, pad;
    char* str;
    char* str2;
    GList* l;
    CustomElement* el_name;

    if ( !g_strcmp0( el->name, name ) )
        el_name = el;
    else
        el_name = el_from_name( el, name );
    if ( !el_name )
        return g_strdup( "" );
    
    char* ret = NULL;
    switch ( el_name->type )
    {
    case CDLG_PREFIX:
        ret = g_strdup( el_name->args ? el_name->args->data : "dialog" );
        break;
    case CDLG_TITLE:
    case CDLG_WINDOW_ICON:
    case CDLG_LABEL:
    case CDLG_IMAGE:
    case CDLG_ICON:
    case CDLG_BUTTON:
    case CDLG_FREE_BUTTON:
    case CDLG_VIEWER:
    case CDLG_EDITOR:
        ret = g_strdup( el_name->val );
        break;
    case CDLG_TIMEOUT:
        ret = g_strdup_printf( "%d", el_name->option );
        break;
    case CDLG_INPUT:
    case CDLG_INPUT_LARGE:
    case CDLG_PASSWORD:
        if ( el_name->type == CDLG_INPUT_LARGE )
            ret = multi_input_get_text( el_name->widgets->next->data );
        else
            ret = g_strdup( gtk_entry_get_text( 
                                    GTK_ENTRY( el_name->widgets->next->data ) ) );
        break;
    case CDLG_CHECKBOX:
    case CDLG_RADIO:
        ret = g_strdup( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
                                            el_name->widgets->next->data ) ) ?
                                            "1" : "0" );
        break;
    case CDLG_DROP:
    case CDLG_COMBO:
        // write text value
        ret = gtk_combo_box_text_get_active_text( 
                            GTK_COMBO_BOX_TEXT( el_name->widgets->next->data ) );
        break;
    case CDLG_LIST:
    case CDLG_MLIST:
        ret = get_tree_view_selected( el_name, NULL );
        break;
    case CDLG_PROGRESS:
        ret = g_strdup( gtk_progress_bar_get_text( 
                        GTK_PROGRESS_BAR( el_name->widgets->next->data ) ) );
        break;
    case CDLG_WINDOW_SIZE:
        pad = -1;
        get_width_height_pad( el_name->val, &width, &height, &pad ); // get pad
        get_window_size( el_name->widgets->data, &width, &height );
        if ( pad == -1 )
            ret = g_strdup_printf( "%dx%d", width, height );
        else
            ret = g_strdup_printf( "%dx%d %d", width, height, pad );
        break;
    case CDLG_CHOOSER:
        if ( gtk_file_chooser_get_select_multiple( GTK_FILE_CHOOSER ( 
                                                el_name->widgets->next->data ) ) )
        {
            GSList* files = gtk_file_chooser_get_filenames( GTK_FILE_CHOOSER(
                                                el_name->widgets->next->data ) );
            GSList* sl;
            if ( files )
            {
                for ( sl = files; sl; sl = sl->next )
                {
                    str = ret;
                    str2 = bash_quote( (char*)sl->data );
                    ret = g_strdup_printf( "%s%s%s", str ? str : "",
                                                     str ? " " : "",
                                                     str2 );
                    g_free( str );
                    g_free( str2 );
                    g_free( sl->data );
                }
                g_slist_free( files );
            }
        }
        else
            ret = g_strdup( gtk_file_chooser_get_filename( GTK_FILE_CHOOSER ( 
                                                el_name->widgets->next->data ) ) );
        break;
    }
    return ret ? ret : g_strdup( "" );
}

static char* get_command_value( CustomElement* el, char* cmdline, char* xvalue )
{
    char* stdout = NULL;
    gboolean ret;
    gint exit_status;
    GError* error = NULL;

    char* line = replace_vars( el, cmdline, xvalue );
    if ( line[0] == '\0' )
        return line;
    
    //fprintf( stderr, "spacefm-dialog: SYNC=%s\n", line );
    ret = g_spawn_command_line_sync( line, &stdout, NULL, NULL, &error );
    g_free( line );

    if ( !ret )
    {
        dlg_warn( "%s", error->message, NULL );
        g_error_free( error );
    }
    return ret ? stdout : g_strdup( "" );    
}

static char* replace_vars( CustomElement* el, char* value, char* xvalue )
{
    char* str;
    char* str2;
    char* str3;
    char* ptr;
    char* sep;
    char c;

    if ( !el || !value )
        return g_strdup( value );
    
    char* newval = NULL;
    ptr = value;
    while ( sep = strchr( ptr, '%' ) )
    {
        sep[0] = '\0';
        if ( ptr[0] != '\0' )
        {
            str = newval;
            newval = g_strdup_printf( "%s%s", str ? str : "", ptr );
            g_free( str );
        }
        sep[0] = '%';
        str = sep + 1;
        while ( str[0] != '\0' && strchr( BASH_VALID, str[0] ) )
            str++;
        if ( sep[1] == '%' )
        {
            // %%
            ptr = sep + 2;
            str2 = newval;
            newval = g_strdup_printf( "%s%s", str2 ? str2 : "", "%" );
            g_free( str2 );
        }
        else if ( sep[1] == '(' )
        {
            // %(line)
            ptr = strrchr( sep, ')' );
            if ( !ptr )
                break;
            ptr[0] = '\0';
            str3 = get_command_value( el, sep + 2, xvalue );
            ptr[0] = ')';
            ptr++;
            
            str2 = newval;
            newval = g_strdup_printf( "%s%s", str2 ? str2 : "", str3 );
            g_free( str2 );
            g_free( str3 );
        }
        else if ( str == sep + 1 )
        {
            // %
            ptr = sep + 1;
            str2 = newval;
            newval = g_strdup_printf( "%s%s", str2 ? str2 : "", "%" );
            g_free( str2 );
        }
        else
        {
            // %VAR
            ptr = str;
            c = str[0];
            str[0] = '\0';
            if ( !strcmp( sep + 1, "n" ) )
            {
                // %n
                str2 = newval;
                newval = g_strdup_printf( "%s%s", str2 ? str2 : "", el->name );
                g_free( str2 );
            }
            else
            {
                if ( !strcmp( sep + 1, "v" ) )
                    // %v
                    str3 = xvalue ? g_strdup( xvalue ) :
                                                get_element_value( el, el->name );
                else
                    // %NAME
                    str3 = xvalue ? g_strdup( xvalue ) :
                                                get_element_value( el, sep + 1 );
                str2 = newval;
                newval = g_strdup_printf( "%s%s", str2 ? str2 : "", str3 );
                g_free( str2 );
                g_free( str3 );
            
            }
            str[0] = c;
        }
    }
    str = newval;
    newval = g_strdup_printf( "%s%s", str ? str : "", ptr );
    g_free( str );
    return newval;
}

static void internal_command( CustomElement* el, int icmd, GList* args, char* xvalue )
{
    char* cname = NULL;
    char* cvalue = NULL;
    CustomElement* el_name = NULL;
    FILE* out;
    gboolean reverse = FALSE;
    
    if ( args->next )
    {
        cname = replace_vars( el, (char*)args->next->data, xvalue );
        if ( args->next->next && strcmp( (char*)args->next->next->data, "--" ) )
        {
            cvalue = replace_vars( el, (char*)args->next->next->data, xvalue );
            if ( cvalue[0] == '\0' || !strcmp( cvalue, "0" )
                                                || !strcmp( cvalue, "false" ) )
                reverse = TRUE;
        }
    }
    if ( icmd != CMD_NOOP && icmd != CMD_CLOSE && icmd != CMD_SOURCE && !cname )
    {
        dlg_warn( _("internal command %s requires an argument"),
                                                            cdlg_cmd[icmd*3], NULL );
        return;
    }

    if ( !cvalue )
        cvalue = g_strdup( "" );

    if ( icmd == CMD_SET && ( !strcmp( cname, "title" )
                           || !strcmp( cname, "windowtitle" ) 
                           || !strcmp( cname, "windowsize" ) 
                           || !strcmp( cname, "windowicon" ) ) )
    {
        // generic - no element
        if ( !strcmp( cname, "title" ) || !strcmp( cname, "windowtitle" ) )
            gtk_window_set_title( GTK_WINDOW( el->widgets->data ), cvalue );
        else if ( !strcmp( cname, "windowsize" ) )
        {
            int width = -1, height = -1;
            get_width_height_pad( cvalue, &width, &height, NULL );
            if ( width > 0 && height > 0 )
            {
                gtk_window_resize( GTK_WINDOW( el->widgets->data ), width, height );
                gtk_window_set_position( GTK_WINDOW( el->widgets->data ),
                                                    GTK_WIN_POS_CENTER_ALWAYS );
            }
            else
                dlg_warn( _("Dynamic resize requires width and height > 0"),
                                                                    NULL, NULL );
        }
        else if ( !strcmp( cname, "windowicon" ) )
        {
            GdkPixbuf* pixbuf;
            if ( cvalue[0] != '\0' )
                pixbuf = gtk_icon_theme_load_icon( gtk_icon_theme_get_default(),
                                cvalue, 16, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );
            else
                pixbuf = NULL;
            gtk_window_set_icon( GTK_WINDOW( el->widgets->data ), pixbuf );  
        }
        g_free( cname );
        g_free( cvalue );
        return;
    }
    if ( icmd != CMD_NOOP && icmd != CMD_SOURCE && cname
                                    && !( el_name = el_from_name( el, cname ) ) )
    {
        if ( cname[0] != '\0' )
            dlg_warn( _("element '%s' does not exist"), cname, NULL );
        g_free( cname );
        g_free( cvalue );
        return;
    }
    // reversal of function
    if ( reverse )
    {
        switch ( icmd )
        {
        case CMD_FOCUS:
            icmd = -1;
            break;
        case CMD_HIDE:
            icmd = CMD_SHOW;
            break;
        case CMD_SHOW:
            icmd = CMD_HIDE;
            break;
        case CMD_DISABLE:
            icmd = CMD_ENABLE;
            break;
        case CMD_ENABLE:
            icmd = CMD_DISABLE;
            break;
        }
    }

    //fprintf( stderr, "spacefm-dialog: INTERNAL=%s %s %s\n", cdlg_cmd[icmd*3],
    //                                                        cname, cvalue );
    switch ( icmd )
    {
    case CMD_CLOSE:
        write_source( el->widgets->data, NULL, stdout );
        g_idle_add( (GSourceFunc)destroy_dlg, el->widgets->data );
        break;
    case CMD_SET:
        set_element_value( el, cname, cvalue );
        break;
    case CMD_PRESS:
        if ( el_name->type == CDLG_BUTTON || el_name->type == CDLG_FREE_BUTTON )
            on_button_clicked( NULL, el_name );
        else
            dlg_warn( _("internal command press is invalid for non-button %s"), 
                                                                cname, NULL );
        break;
    case CMD_SELECT:
    case CMD_UNSELECT:
        if ( el_name->type == CDLG_LIST || el_name->type == CDLG_MLIST )
            select_in_tree_view( el_name, cvalue, icmd == CMD_SELECT );
        else if ( el_name->type == CDLG_DROP )
        {
            if ( icmd == CMD_SELECT )
                select_in_combo_box( el_name, cvalue );
            else
                gtk_combo_box_set_active( GTK_COMBO_BOX( 
                                    el_name->widgets->next->data ), -1 );
        }
        else if ( el_name->type == CDLG_COMBO )
        {
            if ( icmd == CMD_SELECT )
                gtk_entry_set_text( GTK_ENTRY( gtk_bin_get_child( GTK_BIN(
                                    el_name->widgets->next->data ) ) ), cvalue );
            else
                gtk_entry_set_text( GTK_ENTRY( gtk_bin_get_child( GTK_BIN(
                                    el_name->widgets->next->data ) ) ), "" );                
        }
        else if ( el_name->type == CDLG_CHOOSER )
        {
            if ( icmd == CMD_SELECT )
                gtk_file_chooser_select_filename( GTK_FILE_CHOOSER(
                                        el_name->widgets->next->data ), cvalue );
            else
                gtk_file_chooser_unselect_filename( GTK_FILE_CHOOSER(
                                        el_name->widgets->next->data ), cvalue );
        }
        else
            dlg_warn( _("internal command un/select is invalid for %s"),
                                        cdlg_option[el_name->type * 3], NULL );
        break;
    case CMD_HIDE:
        if ( el_name->widgets->next )
            gtk_widget_hide( el_name->widgets->next->data );
        break;
    case CMD_SHOW:
        if ( el_name->widgets->next )
            gtk_widget_show( el_name->widgets->next->data );
        break;
    case CMD_FOCUS:
        if ( el_name->widgets->next )
            gtk_widget_grab_focus( el_name->widgets->next->data );
        break;
    case CMD_DISABLE:
        if ( el_name->widgets->next )
            gtk_widget_set_sensitive( el_name->widgets->next->data, FALSE );
        break;
    case CMD_ENABLE:
        if ( el_name->widgets->next )
            gtk_widget_set_sensitive( el_name->widgets->next->data, TRUE );
        break;
    case CMD_SOURCE:
        if ( !cname || ( cname && cname[0] == '\0' ) )
            out = stderr;
        else
            out = fopen( cname, "w" );
        if ( !out )
        {
            dlg_warn( _("error writing file %s: %s"), cname,
                                                        g_strerror( errno ) );
            break;
        }
        write_source( el->widgets->data, NULL, out );
        if ( out != stderr )
            fclose( out );
        break;
    }
    g_free( cname );
    g_free( cvalue );
}

static void run_command( CustomElement* el, GList* argslist, char* xvalue )
{
    char* str;
    char* line;
    char* arg;
    GList* l;
    int i, icmd = -1;
    GList* args;
    GError* error;

    if ( !argslist )
        return;

    args = argslist;
    while ( args )
    {
        icmd = -1;
        for ( i = 0; i < G_N_ELEMENTS( cdlg_cmd ) / 3; i++ )
        {
            if ( !strcmp( (char*)args->data, cdlg_cmd[i*3] ) )
            {
                icmd = i;
                break;
            }
        }
        if ( icmd == -1 )
        {
            // external command
            gchar* argv[g_list_length( args ) + 1];
            int a = 0;
            while ( args && strcmp( (char*)args->data, "--" ) )
            {
                if ( a == 0 )
                {
                    if ( ((char*)args->data)[0] == '\0' )
                        break;
                    argv[a++] = g_strdup( (char*)args->data );
                }
                else
                    argv[a++] = replace_vars( el, (char*)args->data, xvalue );
                args = args->next;
            }
            if ( a != 0 )
            {
                argv[a++] = NULL;
                /*
                fprintf( stderr, "spacefm-dialog: ASYNC=" );
                for ( i = 0; i < a - 1; i++ )
                    fprintf( stderr, "%s%s", i == 0 ? "" : "  ", argv[i] );
                fprintf( stderr, "\n" );
                */
                error = NULL;
                if ( !g_spawn_async( NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL,
                                                        NULL, NULL, &error ) )
                {
                    dlg_warn( "%s", error->message, NULL );
                    g_error_free( error );
                }
            }
        }
        else
        {
            // internal command
            internal_command( el, icmd, args, xvalue );
            while ( args && strcmp( (char*)args->data, "--" ) )
                args = args->next;
        }
        while ( args && !strcmp( (char*)args->data, "--" ) )
            args = args->next;
    }
}

static void run_command_line( CustomElement* el, char* line )
{
    char* sep;
    GList* args = NULL;
    char* str = line;
    int i = 0;
    // read internal command line into temp args
    while ( str )
    {
        if ( i < 2 )
        {
            if ( sep = strchr( str, ' ' ) )
                sep[0] = '\0';
            args = g_list_append( args, g_strdup( str ) );
            if ( sep )
            {
                sep[0] = ' ';
                str = sep + 1;
            }
            else
                str = NULL;
        }
        else
        {
            args = g_list_append( args, g_strdup( str ) );
            str = NULL;
        }
        i++;                
    }
    if ( args )
    {
        int icmd = -1;
        for ( i = 0; i < G_N_ELEMENTS( cdlg_cmd ) / 3; i++ )
        {
            if ( !strcmp( (char*)args->data, cdlg_cmd[i*3] ) )
            {
                icmd = i;
                break;
            }
        }
        if ( icmd != -1 )
            run_command( el, args, NULL );
        else
            dlg_warn( _("'%s' is not an internal command"), (char*)args->data,
                                                                        NULL );
        g_list_foreach( args, (GFunc)g_free, NULL );
        g_list_free( args );
    }
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
        dlg_warn( _("error writing file %s: %s"), path + add,
                                                    g_strerror( errno ) );
        return;
    }
    if ( val && write( f, val, strlen( val ) ) < strlen( val ) )
        dlg_warn( _("error writing file %s: %s"), path + add,
                                                    g_strerror( errno ) );
    if ( !strchr( val, '\n' ) )
        write( f, "\n", 1 );
    close( f );
}

static char* read_file_value( const char* path, gboolean multi )
{
    FILE* file;
    int f, bytes;
    const gchar* end;
    
    if ( !g_file_test( path, G_FILE_TEST_EXISTS ) )
    {
        // create file
        if ( ( f = open( path, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR ) ) == -1 )
        {
            dlg_warn( _("error creating file %s: %s"), path,
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
            dlg_warn( _("error reading file %s: %s"), path,
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
            dlg_warn( _("error reading file %s: %s"), path,
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
    if ( !g_utf8_validate( line, -1, &end ) )
    {
        if ( multi && end > line )
            ((char*)end)[0] = '\0';
        else
        {
            dlg_warn( _("file '%s' contents are not valid UTF-8"), path, NULL );
            return NULL;
        }
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
        if ( !g_utf8_validate( line, size, NULL ) )
            dlg_warn( _("pipe '%s' data is not valid UTF-8"),
                                                (char*)el->args->data, NULL );
        else if ( el->type == CDLG_VIEWER && el->widgets->next )
        {
            GtkTextIter iter, siter;
            GtkTextBuffer* buf = gtk_text_view_get_buffer(
                                        GTK_TEXT_VIEW( el->widgets->next->data ) );
            gtk_text_buffer_get_end_iter( buf, &iter);            
            gtk_text_buffer_insert( buf, &iter, line, size );
            //scroll
            if ( el->option )
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
        else if ( el->type == CDLG_COMMAND )
        {
            char* str = g_strndup( line, size );
            while ( g_str_has_suffix( str, "\n" ) )
                str[strlen( str )-1] = '\0';
            run_command_line( el, str );
            g_free( str );
        }
    }
    else
        g_warning( "cb_pipe_watch: g_io_channel_read_chars != G_IO_STATUS_NORMAL" );
    return TRUE;
}

static gboolean delayed_update_false( CustomElement* el )
{
    if ( el->timeout )
    {
        g_source_remove( el->timeout );
        el->timeout = 0;
    }
    return FALSE;
}

static gboolean delayed_update( CustomElement* el )
{
    if ( el->timeout )
    {
        g_source_remove( el->timeout );
        el->timeout = 0;
    }
    update_element( el, NULL, NULL, 0 );
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
        // update_element will add a new monitor if file re-created
        // use low priority since cb_file_value_change is called from another thread
        // otherwise segfault in vfs-file-monitor.c:351
        break;
    case VFS_FILE_MONITOR_CHANGE:
    case VFS_FILE_MONITOR_CREATE:
    default:
        //printf ("    CREATE/CHANGE\n");
        break;
    }
    if ( !el->timeout )
    {
        // don't update element more than once every 50ms - when a file is
        // changed multiple events are reported
        el->timeout = g_timeout_add_full( G_PRIORITY_DEFAULT_IDLE,
                                          50,
                                          (GSourceFunc)delayed_update,
                                          el, NULL );
    }
}

static void fill_buffer_from_file( CustomElement* el, GtkTextBuffer* buf,
                                   char* path, gboolean watch )
{
    char line[ 4096 ];
    FILE* file;
    
    char* pathx = path;
    if ( pathx[0] == '@' )
        pathx++;
    
    gtk_text_buffer_set_text( buf, "", -1 );
    
    file = fopen( pathx, "r" );
    if ( !file )
    {
        dlg_warn( _("error reading file %s: %s"), pathx,
                                                    g_strerror( errno ) );
        return;
    }
    // read file one line at a time to prevent splitting UTF-8 characters
    while ( fgets( line, sizeof( line ), file ) )
    {
        if ( !g_utf8_validate( line, -1, NULL ) )
        {
            fclose( file );
            if ( watch )
                gtk_text_buffer_set_text( buf, _("( file contents are not valid UTF-8 )"), -1 );
            else
                gtk_text_buffer_set_text( buf, "", -1 );
            dlg_warn( _("file '%s' contents are not valid UTF-8"), pathx, NULL );
            return;
        }
        gtk_text_buffer_insert_at_cursor( buf, line, -1 );
    }
    fclose( file );

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
        if ( multi )
            // strip trailing linefeeds
            while ( g_str_has_suffix( el->val, "\n" ) )
                el->val[strlen( el->val ) - 1] = '\0';
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

static gboolean destroy_dlg( GtkWidget* dlg )
{
    GList* elements = (GList*)g_object_get_data( G_OBJECT( dlg ), "elements" );

    // remove destroy signal connect
    g_signal_handlers_disconnect_by_func( G_OBJECT( dlg ),
                                            G_CALLBACK( on_dlg_close ), NULL );
    gtk_widget_destroy( GTK_WIDGET( dlg ) );
    free_elements( elements );
    gtk_main_quit();
    return FALSE;
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
    if ( strchr( quoted, '\t' ) )
    {
        str = quoted;
        quoted = replace_string( str, "\t", "'$'\\t''", FALSE );
        g_free( str );
    }
    fprintf( file, "%s_%s%s%s=%s\n", prefix, name, sub ? "_" : "", sub ? sub : "",
                                                                    quoted );
    g_free( quoted );
}

static void write_source( GtkWidget* dlg, CustomElement* el_pressed,
                                                            FILE* out )
{
    GList* l;
    CustomElement* el;
    char* str;
    char* prefix = "dialog";
    int width, height, pad = -1;
    
    GList* elements = (GList*)g_object_get_data( G_OBJECT( dlg ), "elements" );


    // get custom prefix
    for ( l = elements; l; l = l->next )
    {
        if ( ((CustomElement*)l->data)->type == CDLG_PREFIX )
        {
            el = (CustomElement*)l->data;
            if ( el->args )
            {
                get_text_value( el, (char*)el->args->data, FALSE, FALSE );
                if ( el->val && el->val[0] != '\0' )
                {
                    str = g_strdup( el->val );
                    g_strcanon( str, BASH_VALID, ' ' );
                    if ( strcmp( str, el->val ) )
                        dlg_warn( _("prefix '%s' is not a valid variable name"),
                                                                el->val, NULL );
                    else
                        prefix = el->val;
                    g_free( str );
                }
            }
            break;
        }
    }

    // write values
    int button_count = 0;
    fprintf( out, "#!/bin/bash\n# SpaceFM Dialog source output - execute this output to set variables\n# Example:  eval \"`spacefm --dialog --label \"Message\" --button ok`\"\n\n" );
    if ( !el_pressed )
    {
        // no button press caused dialog closure
        write_value( out, prefix, "pressed", NULL, NULL );
        write_value( out, prefix, "pressed", "index", "-2" );
        write_value( out, prefix, "pressed", "label", NULL );
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
        case CDLG_COMMAND:
            write_value( out, prefix, el->name, NULL, el->val );
            break;
        case CDLG_BUTTON:
            button_count++;
        case CDLG_FREE_BUTTON:
        case CDLG_TIMEOUT:
            if ( el == el_pressed )
            {
                // dialog was closed by user pressing this button
                if ( el->type == CDLG_BUTTON )
                {
                    write_value( out, prefix, "pressed", NULL, el->name );
                    str = g_strdup_printf( "%d", button_count - 1 );
                    write_value( out, prefix, "pressed", "index", str );
                    g_free( str );
                    write_value( out, prefix, "pressed", "label", el->val );
                }
                else if ( el->type == CDLG_TIMEOUT )
                {    
                    write_value( out, prefix, "pressed", NULL, el->name );
                    write_value( out, prefix, "pressed", "index", "-3" );
                    write_value( out, prefix, "pressed", "label", NULL );
                }
                else
                {
                    write_value( out, prefix, "pressed", NULL, el->name );
                    write_value( out, prefix, "pressed", "index", "-1" );
                    write_value( out, prefix, "pressed", "label", el->val );
                }
            }
            if ( el->type == CDLG_TIMEOUT )
            {
                str = g_strdup_printf( "%d", el->option );
                write_value( out, prefix, el->name, NULL, str );
                g_free( str );
            }
            else
                write_value( out, prefix, el->name, NULL, el->val );
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
            {
                // save file
                // skip detection of updates while saving file
                if ( el->timeout )
                    g_source_remove( el->timeout );
                el->timeout = g_timeout_add_full( G_PRIORITY_DEFAULT_IDLE,
                                          300,
                                          (GSourceFunc)delayed_update_false,
                                          el, NULL );
                write_file_value( (char*)el->args->data, str );
            }
            write_value( out, prefix, el->name, "default",
                                    el->args ? (char*)el->args->data : NULL );
            write_value( out, prefix, el->name, NULL, str );
            if ( el->type == CDLG_INPUT_LARGE )
                g_free( str );
            break;
        case CDLG_VIEWER:
            write_value( out, prefix, el->name, NULL, el->val );
            break;
        case CDLG_EDITOR:
            write_value( out, prefix, el->name, NULL, el->val );
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
            else
                write_value( out, prefix, el->name, "saved", NULL );
            break;
        case CDLG_CHECKBOX:
        case CDLG_RADIO:
            write_value( out, prefix, el->name, "label", gtk_button_get_label( 
                                        GTK_BUTTON( el->widgets->next->data ) ) );
            write_value( out, prefix, el->name, NULL,
                    gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
                                                el->widgets->next->data ) ) ?
                                                "1" : "0" );
            // save file
            if ( el->args && el->args->next 
                                    && ((char*)el->args->next->data)[0] == '@' )
            {
                write_file_value( (char*)el->args->next->data + 1, 
                    gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
                                                el->widgets->next->data ) ) ?
                                                "1" : "0" );
                write_value( out, prefix, el->name, "saved",
                                                (char*)el->args->next->data + 1 );
            }
            break;
        case CDLG_DROP:
        case CDLG_COMBO:
            // write text value
            str = gtk_combo_box_text_get_active_text( 
                                GTK_COMBO_BOX_TEXT( el->widgets->next->data ) );
            write_value( out, prefix, el->name, NULL, str );
            if ( el->def_val && el->def_val[0] == '@' )
            {
                // save file
                write_file_value( el->def_val + 1, str );
                write_value( out, prefix, el->name, "saved", el->def_val + 1 );
            }
            g_free( str );
            // write index
            str = g_strdup_printf( "%d", 
                                    gtk_combo_box_get_active( GTK_COMBO_BOX( 
                                    el->widgets->next->data ) ) );
            write_value( out, prefix, el->name, "index", str );
            g_free( str );
            break;
        case CDLG_LIST:
        case CDLG_MLIST:
            str = get_tree_view_selected( el, prefix );
            fprintf( out, str );
            g_free( str );
            break;
        case CDLG_PROGRESS:
            write_value( out, prefix, el->name, NULL,
                            gtk_progress_bar_get_text( 
                            GTK_PROGRESS_BAR( el->widgets->next->data ) ) );
            break;
        case CDLG_WINDOW_SIZE:
            get_width_height_pad( el->val, &width, &height, &pad ); // get pad
            if ( el->args && el->args->next
                                    && atoi( (char*)el->args->next->data ) > 0 )
                pad = atoi( (char*)el->args->next->data );
            if ( el->args && ((char*)el->args->data)[0] == '@' )
            {
                // save file
                get_window_size( el->widgets->data, &width, &height );
                if ( pad == -1 )
                    str = g_strdup_printf( "%dx%d", width, height );
                else
                    str = g_strdup_printf( "%dx%d %d", width, height, pad );
                // skip detection of updates while saving file
                if ( el->timeout )
                    g_source_remove( el->timeout );
                el->timeout = g_timeout_add_full( G_PRIORITY_DEFAULT_IDLE,
                                          300,
                                          (GSourceFunc)delayed_update_false,
                                          el, NULL );
                write_file_value( (char*)el->args->data + 1, str );
                write_value( out, prefix, el->name, "saved",
                                                (char*)el->args->data + 1 );
                g_free( str );
            }
            break;
        case CDLG_CHOOSER:
            if ( gtk_file_chooser_get_select_multiple( GTK_FILE_CHOOSER ( 
                                                    el->widgets->next->data ) ) )
            {
                GSList* files = gtk_file_chooser_get_filenames( GTK_FILE_CHOOSER(
                                                    el->widgets->next->data ) );
                GSList* sl;
                if ( !files )
                    fprintf( out, "%s_%s=''\n", prefix, el->name );
                else
                {
                    fprintf( out, "%s_%s=(", prefix, el->name );
                    for ( sl = files; sl; sl = sl->next )
                    {
                        str = bash_quote( (char*)sl->data );
                        fprintf( out, "\n%s", str );
                        g_free( str );
                        g_free( sl->data );
                    }
                    fprintf( out, ")\n" );
                    g_slist_free( files );
                }
            }
            else
            {
                str = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER ( 
                                                    el->widgets->next->data ) );
                write_value( out, prefix, el->name, NULL, str );
                g_free( str );
            }
            str = gtk_file_chooser_get_current_folder( GTK_FILE_CHOOSER ( 
                                                    el->widgets->next->data ) );
            write_value( out, prefix, el->name, "dir", str );
            if ( el->args && ((char*)el->args->data)[0] == '@' )
            {
                // save file
                write_value( out, prefix, el->name, "saved",
                                                (char*)el->args->data + 1 );
                // skip detection of updates while saving file
                if ( el->timeout )
                    g_source_remove( el->timeout );
                el->timeout = g_timeout_add_full( G_PRIORITY_DEFAULT_IDLE,
                                          300,
                                          (GSourceFunc)delayed_update_false,
                                          el, NULL );
                write_file_value( (char*)el->args->data + 1, str );
            }
            g_free( str );
            break;
        }
    }
    // write window size
    get_window_size( dlg, &width, &height );
    if ( pad == -1 )
        str = g_strdup_printf( "%dx%d", width, height );
    else
        str = g_strdup_printf( "%dx%d %d", width, height, pad );
    write_value( out, prefix, "windowsize", NULL, str );
    g_free( str );
}

static void on_dlg_close( GtkDialog* dlg )
{
    write_source( GTK_WIDGET( dlg ), NULL, stdout );
    destroy_dlg( GTK_WIDGET( dlg ) );
}

static gboolean on_progress_timer( CustomElement* el )
{
    gtk_progress_bar_pulse( GTK_PROGRESS_BAR( el->widgets->next->data ) );
    return TRUE;
}

static gboolean on_timeout_timer( CustomElement* el )
{
    el->option--;
    if ( el->option <= 0 )
    {
        write_source( el->widgets->data, el, stdout );
        g_idle_add( (GSourceFunc)destroy_dlg, el->widgets->data );
        return FALSE;
    }
    g_free( el->val );
    el->val = g_strdup_printf( "%s %d", _("Pause"), el->option );
    gtk_button_set_label( GTK_BUTTON( el->widgets->next->data ), el->val );
    return TRUE;
}

/*
static gboolean on_status_button_press( GtkWidget *widget,
                                        GdkEventButton *evt,
                                        CustomElement* el )
{
    if ( evt->type == GDK_BUTTON_PRESS && evt->button < 4 && el->args
                                                            && el->args->next )
    {
        char* num = g_strdup_printf( "%d", evt->button );
        run_command( el->args->next, el->name, num );
        g_free( num );
        return TRUE;
    }
    return TRUE;
}
*/

void on_combo_changed( GtkComboBox* box, CustomElement* el )
{
    if ( el->type != CDLG_DROP || !el->cmd_args )
        return;
    run_command( el, el->cmd_args, NULL );
}

/*
gboolean on_list_button_press( GtkTreeView* view, GdkEventButton* evt,
                                CustomElement* el )
{
    printf("on_list_button_press\n");
    if ( evt->type == GDK_2BUTTON_PRESS && evt->button == 1 )
    {
        gtk_tree_view_row_activated( view, NULL, NULL );
        return TRUE;
    }
    return FALSE;
}
*/

static void on_list_row_activated( GtkTreeView *view,
                                   GtkTreePath *tree_path,
                                   GtkTreeViewColumn* col,
                                   CustomElement* el )
{
    GtkTreeIter iter;
    
    if ( !el->cmd_args )
    {
        press_last_button( el->widgets->data );
        return;
    }
    
    // get iter
    GtkTreeModel* model = gtk_tree_view_get_model( GTK_TREE_VIEW( view ) );
    if ( !gtk_tree_model_get_iter( model, &iter, tree_path ) )
        return;

/*
    // get clicked column index
    int colx = 0;
    int x = -1;
    GList* l;
    GList* cols = gtk_tree_view_get_columns( GTK_TREE_VIEW( view ) );
    for ( l = cols; l; l = l->next )
    {
        if ( l->data == col )
        {
            x = colx;
            break;
        }
        colx++;
    }
    g_list_free( cols );
    if ( x == -1 )
        return;
*/
    run_command( el, el->cmd_args, NULL );
}

static gboolean on_widget_button_press_event( GtkWidget *widget,
                                              GdkEventButton *evt,
                                              CustomElement* el )
{
    if ( evt->type == GDK_BUTTON_PRESS )
    {
        if ( evt->button < 4 && el->cmd_args )
        {
            char* num = g_strdup_printf( "%d %dx%d", evt->button, (uint)evt->x,
                                                                  (uint)evt->y );
            run_command( el, el->cmd_args, num );
            g_free( num );
            return TRUE;
        }
    }
    return FALSE;
}

void on_option_toggled( GtkToggleButton *togglebutton, CustomElement* el )
{
    if ( el->type == CDLG_TIMEOUT )
    {
        if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
                                                el->widgets->next->data ) ) )
        {
            if ( el->timeout )
            {
                g_source_remove( el->timeout );
                el->timeout = 0;
            }
        }
        else
        {
             if ( !el->timeout )
                el->timeout = g_timeout_add( 1000, (GSourceFunc)on_timeout_timer, el );
        }
    }
    else if ( el->cmd_args )
    {
        if ( el->type == CDLG_RADIO )
        {
            if ( gtk_toggle_button_get_active( togglebutton ) )
                run_command( el, el->cmd_args, "1" );
        }
        else if  ( el->type == CDLG_CHECKBOX )
        {
            run_command( el, el->cmd_args, 
                    gtk_toggle_button_get_active( togglebutton ) ? "1" : "0" );
        }
    }
}

static void on_button_clicked( GtkButton *button, CustomElement* el ) 
{
    if ( el->cmd_args )
        // button has a command
        run_command( el, el->cmd_args, NULL );
    else
    {
        // no command
        write_source( el->widgets->data, el, stdout );
        g_idle_add( (GSourceFunc)destroy_dlg, el->widgets->data );
    }
}

static gboolean press_last_button( GtkWidget* dlg )
{
    // find last (default) button and press it
    GList* elements = (GList*)g_object_get_data( G_OBJECT( dlg ),
                                                                "elements" );
    if ( !elements )
        return FALSE;
    GList* l;
    CustomElement* el;
    CustomElement* el_button = NULL;
    for ( l = elements; l; l = l->next )
    {
        el = (CustomElement*)l->data;
        if ( el->type == CDLG_BUTTON )
            el_button = el;
    }
    if ( el_button )
    {
        on_button_clicked( NULL, el_button );
        return TRUE;
    }
    return FALSE;
}

void on_chooser_activated( GtkFileChooser* chooser, CustomElement* el )
{
    if ( el->cmd_args )
    {
        char* str = gtk_file_chooser_get_filename( chooser );
        if ( str )
            run_command( el, el->cmd_args, str );
        g_free( str );
    }
    else
        press_last_button( el->widgets->data );
}

static gboolean on_dlg_key_press( GtkWidget *entry, GdkEventKey* evt,
                                                      CustomElement* el )
{
    int keymod = ( evt->state & ( GDK_SHIFT_MASK | GDK_CONTROL_MASK |
                 GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK ) );
    int keycode = strtol( (char*)el->args->data, NULL, 0 );
    int modifier = strtol( (char*)el->args->next->data, NULL, 0 );
    if ( keycode == evt->keyval && modifier == keymod )
    {
        char* str = g_strdup_printf( "%s %s", (char*)el->args->data,
                                     (char*)el->args->next->data );
        run_command( el, el->cmd_args, str );
        g_free( str );
        return TRUE;
    }
    return FALSE;
}

static gboolean on_input_key_press( GtkWidget *entry, GdkEventKey* evt,
                                                      CustomElement* el )
{
    int keymod = ( evt->state & ( GDK_SHIFT_MASK | GDK_CONTROL_MASK |
                 GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK ) );

    if ( !( !keymod && ( evt->keyval == GDK_Return || evt->keyval == GDK_KP_Enter ) ) )
        return FALSE;  // Enter key not pressed

    if ( ( el->type == CDLG_INPUT || el->type == CDLG_INPUT_LARGE )
                                        && el->cmd_args )
    {
        // input has a command
        if ( el->type == CDLG_INPUT_LARGE )
            run_command( el, el->cmd_args, NULL );
        else if ( el->type == CDLG_INPUT )
            run_command( el, el->cmd_args, NULL );
        return TRUE;
    }
    else if ( el->type == CDLG_COMBO && el->cmd_args )
    {
        run_command( el, el->cmd_args, NULL );
        return TRUE;
    }
    else if ( el->type == CDLG_PASSWORD && el->cmd_args )
    {
        // password has a command
        run_command( el, el->cmd_args, NULL );
        return TRUE;
    }
    else
    {
        // no command - find last (default) button and press it
        return press_last_button( (GtkWidget*)el->widgets->data );
    }
    return FALSE;
}

static void update_element( CustomElement* el, GtkWidget* box, GSList** radio,
                                                                    int pad )
{
    GtkWidget* w;
    GdkPixbuf* pixbuf;
    GtkWidget* dlg = (GtkWidget*)el->widgets->data;
    char* str;
    char* sep;
    struct stat64 statbuf;
    GtkTextBuffer* buf;
    GtkTextIter iter;
    int i;
    GList* l;
    char* font = NULL;
    gboolean viewer_scroll = FALSE;
    gboolean chooser_save = FALSE;
    gboolean chooser_dir = FALSE;
    gboolean chooser_multi = FALSE;
    GList* chooser_filters = NULL;
    gboolean box_compact = FALSE;
    int selstart = -1;
    int selend = -1;
    
    GList* args = el->args;
    
    // get element options
    while ( args && g_str_has_prefix( (char*)args->data, "--" ) )
    {
        if ( !strcmp( (char*)args->data, "--font" ) )
        {
            if ( args->next && !g_str_has_prefix( (char*)args->next->data, "--" ) )
            {
                args = args->next;
                font = (char*)args->data;
            }
        }
        else if ( ( el->type == CDLG_INPUT || el->type == CDLG_INPUT_LARGE )
                                    && !strcmp( (char*)args->data, "--select" )
                                    && args->next )
        {
            args = args->next;
            sep = strchr( (char*)args->data, ':' );
            if ( !sep )
                sep = strchr( (char*)args->data, ' ' );
            if ( sep )
                sep[0] = '\0';
            selstart = atoi( (char*)args->data );
            if ( sep )
            {
                selend = atoi( sep + 1 );
                if ( selend > 0 ) selend++;
                sep[0] = ':';
            }
        }
        else if ( el->type == CDLG_VIEWER
                                    && !strcmp( (char*)args->data, "--scroll" ) )
            viewer_scroll = TRUE;
        else if ( ( el->type == CDLG_HBOX || el->type == CDLG_VBOX ) 
                                    && !strcmp( (char*)args->data, "--compact" ) )
            box_compact = TRUE;
        else if ( el->type == CDLG_CHOOSER )
        {
            if ( !strcmp( (char*)args->data, "--save" ) )
                chooser_save = TRUE;
            else if ( !strcmp( (char*)args->data, "--dir" ) )
                chooser_dir = TRUE;
            else if ( !strcmp( (char*)args->data, "--multi" ) )
                chooser_multi = TRUE;
            else if ( !strcmp( (char*)args->data, "--filter" ) )
            {
                if ( args->next && !g_str_has_prefix( (char*)args->next->data, "--" ) )
                {
                    args = args->next;
                    chooser_filters = g_list_append( chooser_filters,
                                                    (char*)args->data );
                }
            }
        }
        args = args->next;
    }
    el->args = args;  // only parse options once
    
    switch ( el->type )
    {
    case CDLG_TITLE:
        if ( args )
        {
            get_text_value( el, (char*)args->data, FALSE, TRUE );
            if ( el->val )
                gtk_window_set_title( GTK_WINDOW( dlg ), el->val );
            else
                gtk_window_set_title( GTK_WINDOW( dlg ), DEFAULT_TITLE );
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
            el->cmd_args = el->args->next; 
        }
        break;
    case CDLG_LABEL:
        if ( args )
        {
            get_text_value( el, (char*)args->data, TRUE, TRUE );
            str = el->val;
            el->val = unescape( str );
            g_free( str );
        }
        // add label
        if ( !el->widgets->next && box )
        {
            w = gtk_label_new( NULL );
            gtk_label_set_line_wrap( GTK_LABEL( w ), TRUE );
            gtk_label_set_line_wrap_mode( GTK_LABEL( w ), PANGO_WRAP_WORD_CHAR );
            gtk_misc_set_alignment( GTK_MISC ( w ), 0.0, 0.5 );
            gtk_label_set_selectable( GTK_LABEL( w ), TRUE );
            set_font( w, font );
            el->widgets = g_list_append( el->widgets, w );
            gtk_box_pack_start( GTK_BOX( box ), GTK_WIDGET( w ), FALSE, FALSE, pad );
            if ( radio ) *radio = NULL;
            //if ( args )
            //    el->cmd_args = el->args->next; 
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
        {
            get_text_value( el, (char*)args->data, FALSE, TRUE );
            str = el->val;
            el->val = unescape( str );
            g_free( str );
        }
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
                                            GTK_WIDGET( w ), FALSE, FALSE, pad );
                gtk_widget_grab_focus( w );
            }
            else
                gtk_box_pack_start( GTK_BOX( box ),
                                            GTK_WIDGET( w ), FALSE, FALSE, pad );
            g_signal_connect( G_OBJECT( w ), "clicked",
                                        G_CALLBACK( on_button_clicked ), el );
            if ( radio ) *radio = NULL;
            if ( args )
                el->cmd_args = el->args->next; 
        }
        // set label and icon
        if ( el->widgets->next && ( w = GTK_WIDGET( el->widgets->next->data ) ) )
        {
            if ( el->val && ( sep = strrchr( el->val, ':' ) ) )
                sep[0] = '\0';
            else
                sep = NULL;
            gtk_button_set_image( GTK_BUTTON( w ), NULL );
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
                sep[0] = ':';
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
        // add event to hold image widget and get events
        GtkWidget* image_box;
        if ( !el->widgets->next && box )
        {
            image_box = gtk_event_box_new();
            el->widgets = g_list_append( el->widgets, image_box );
            gtk_box_pack_start( GTK_BOX( box ), GTK_WIDGET( image_box ),
                                                            FALSE, FALSE, pad );
            g_signal_connect ( G_OBJECT( image_box ), "button-press-event",
                                   G_CALLBACK ( on_widget_button_press_event ),
                                   el );
            if ( radio ) *radio = NULL;
            if ( args )
                el->cmd_args = el->args->next; 
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
            gtk_container_add( GTK_CONTAINER( image_box ), GTK_WIDGET( w ) );
            el->widgets = g_list_append( el->widgets, w );
            gtk_widget_show( w );
        }
        break;
    case CDLG_INPUT:
    case CDLG_INPUT_LARGE:
    case CDLG_PASSWORD:
        if ( !el->widgets->next && box )
        {
            el->option = -1;
            // add input
            if ( args )
            {
                // default text
                get_text_value( el, (char*)args->data, FALSE, TRUE );
                el->cmd_args = args->next;
            }
            if ( el->type == CDLG_INPUT_LARGE )
            {
                // multi-input
                GtkWidget* scroll = gtk_scrolled_window_new( NULL, NULL );
                w = GTK_WIDGET( multi_input_new( GTK_SCROLLED_WINDOW( scroll ),
                                                                el->val, FALSE ) );
                set_font( w, font );
                if ( selstart >= 0 )
                    multi_input_select_region( w, selstart, selend );
                gtk_box_pack_start( GTK_BOX( box ), GTK_WIDGET( scroll ),
                                                            TRUE, TRUE, pad );
            }
            else
            {
                // entry
                w = gtk_entry_new();
                gtk_entry_set_visibility( GTK_ENTRY( w ), el->type != CDLG_PASSWORD );
                set_font( w, font );
                if ( el->val )
                {
                    gtk_entry_set_text( GTK_ENTRY( w ), el->val );
                    if ( selstart >= 0 && el->type != CDLG_PASSWORD )
                    {
                        gtk_editable_select_region( GTK_EDITABLE( w ), selstart, selend );
                        el->option = selstart;
                        el->option2 = selend;
                    }
                    else
                        gtk_editable_select_region( GTK_EDITABLE( w ), 0, -1 );                    
                }
                gtk_box_pack_start( GTK_BOX( box ), GTK_WIDGET( w ), FALSE, TRUE, pad );
            }
            el->widgets = g_list_append( el->widgets, w );
            g_signal_connect( G_OBJECT( w ), "key-press-event",
                                            G_CALLBACK( on_input_key_press), el );
            if ( radio ) *radio = NULL;
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
            set_font( w, font );
            gtk_container_add ( GTK_CONTAINER ( scroll ), w );
            el->widgets = g_list_append( el->widgets, w );
            el->widgets = g_list_append( el->widgets, scroll );
            gtk_box_pack_start( GTK_BOX( box ), GTK_WIDGET( scroll ), TRUE, TRUE, pad );
            // place mark at end
            buf = gtk_text_view_get_buffer( GTK_TEXT_VIEW( w ) );
            gtk_text_buffer_get_end_iter( buf, &iter);
            gtk_text_buffer_create_mark( buf, "end", &iter, FALSE );
            el->option = viewer_scroll ? 1 : 0;
            selstart = 1;  // indicates new
            if ( radio ) *radio = NULL;
        }
        if ( args && ((char*)args->data)[0] != '\0' && el->type == CDLG_VIEWER
                                                    && el->widgets->next )
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
                if ( channel )
                {
                    gint fd = g_io_channel_unix_get_fd( channel );
                    //int fd = fcntl( g_io_channel_unix_get_fd( channel ), F_GETFL );
                    if ( fd > 0 )
                    {
                        fcntl( fd, F_SETFL,O_NONBLOCK );
                        g_io_add_watch_full( channel, G_PRIORITY_LOW,
                                        G_IO_IN	| G_IO_HUP | G_IO_NVAL | G_IO_ERR,
                                        (GIOFunc)cb_pipe_watch, el, NULL );
                        g_free( el->val );
                        el->val = g_strdup( (char*)args->data );
                    }
                }
            }
            else if ( g_file_test( (char*)args->data, G_FILE_TEST_IS_REGULAR ) )
            {
                // update viewer from file
                fill_buffer_from_file( el, buf, (char*)args->data, TRUE );
                g_free( el->val );
                el->val = g_strdup( (char*)args->data );
            }
            else
            {
                dlg_warn( _("file '%s' is not a regular file or a pipe"),
                                                    (char*)args->data, NULL );
            }
            // scroll
            if ( el->option )
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
        else if ( args && ((char*)args->data)[0] != '\0' && selstart &&
                                el->type == CDLG_EDITOR && el->widgets->next )
        {
            // new editor            
            buf = gtk_text_view_get_buffer(
                                    GTK_TEXT_VIEW( el->widgets->next->data ) );
            fill_buffer_from_file( el, buf, (char*)args->data, FALSE );
            g_free( el->val );
            el->val = g_strdup( (char*)args->data );
        }
        break;
    case CDLG_COMMAND:
        if ( !el->option && args )
        {
            if ( ((char*)args->data)[0] != '\0'
                                && stat64( (char*)args->data, &statbuf ) != -1 
                                && S_ISFIFO( statbuf.st_mode ) )
            {
                // watch pipe
                GIOChannel* channel = g_io_channel_new_file( (char*)args->data,
                                                                "r+", NULL );
                if ( channel )
                {
                    gint fd = g_io_channel_unix_get_fd( channel );
                    //int fd = fcntl( g_io_channel_unix_get_fd( channel ), F_GETFL );
                    if ( fd > 0 )
                    {
                        fcntl( fd, F_SETFL,O_NONBLOCK );
                        g_io_add_watch_full( channel, G_PRIORITY_LOW,
                                        G_IO_IN	| G_IO_HUP | G_IO_NVAL | G_IO_ERR,
                                        (GIOFunc)cb_pipe_watch, el, NULL );
                        g_free( el->val );
                        el->val = g_strdup( (char*)args->data );
                    }
                }
            }
            else if ( ((char*)args->data)[0] != '\0' )
            {
                if ( ((char*)args->data)[0] == '@' )
                    str = g_strdup( (char*)args->data );
                else
                    str = g_strdup_printf( "@%s", (char*)args->data );
                get_text_value( el, str, FALSE, TRUE );
                g_free( str );
            }
            el->option = 1;
            // init COMMAND
            el->cmd_args = args->next;
        }
        else if ( el->option && args )
        {
            if ( ((char*)args->data)[0] == '@' )
                str = g_strdup( (char*)args->data );
            else
                str = g_strdup_printf( "@%s", (char*)args->data );
            get_text_value( el, str, FALSE, TRUE );
            g_free( str );
            run_command_line( el, el->val );
        }
        break;
    case CDLG_CHECKBOX:
    case CDLG_RADIO:
        // add item
        if ( !el->widgets->next && box && radio )
        {
            str = unescape( el->args ? (char*)el->args->data : "" );
            if ( el->type == CDLG_CHECKBOX )
            {
                w = gtk_check_button_new_with_mnemonic( str );
                *radio = NULL;
            }
            else
            {
                /*
                GSList* l;
                printf("LIST-BEFORE %#x\n", *radio );
                for ( l = *radio; l; l = l->next )
                    printf( "    button=%#x\n", l->data );
                */
                w = gtk_radio_button_new_with_mnemonic( *radio, str );
                //printf("BUTTON=%#x\n", w );
                if ( *radio == NULL )
                    *radio = gtk_radio_button_get_group( GTK_RADIO_BUTTON( w ) );
                else if ( !g_slist_find( *radio, w ) )
                    // add to group manually if not added - gtk bug?
                    *radio = g_slist_append( *radio, w );
                /*
                printf("LIST-AFTER %#x\n", *radio );
                for ( l = *radio; l; l = l->next )
                    printf( "    button=%#x\n", l->data );
                */
            }
            g_free( str );
            gtk_button_set_focus_on_click( GTK_BUTTON( w ), FALSE );
            
            // set font of label
            l = gtk_container_get_children( GTK_CONTAINER( w ) );            
            if ( l )
                set_font( GTK_WIDGET( l->data ), font );
            g_list_free( l );
            
            el->widgets = g_list_append( el->widgets, w );
            gtk_box_pack_start( GTK_BOX( box ), GTK_WIDGET( w ), FALSE, FALSE, pad );
            // default value
            if ( args && args->next )
            {
                get_text_value( el, (char*)args->next->data, FALSE, TRUE );
                if ( !g_strcmp0( el->val, "1" ) ||
                     !g_strcmp0( el->val, "true" ) )
                    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( w ), TRUE );
                el->cmd_args = el->args->next->next; 
            }
            g_signal_connect( G_OBJECT( w ), "toggled",
                                        G_CALLBACK( on_option_toggled ), el );
        }
        else if ( el->widgets->next && args && args->next )
        {
            get_text_value( el, (char*)args->next->data, FALSE, TRUE );
            if ( !g_strcmp0( el->val, "1") || !g_strcmp0( el->val, "true" ) )
                gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(
                                            el->widgets->next->data ), TRUE );
            else if ( !g_strcmp0( el->val, "0") || !g_strcmp0( el->val, "false" ) )
                gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(
                                            el->widgets->next->data ), FALSE );
        }
        break;
    case CDLG_DROP:
    case CDLG_COMBO:
        // add list widget
        if ( !el->widgets->next && box )
        {
            if ( el->type == CDLG_DROP )
            {
                w = gtk_combo_box_text_new();
                g_signal_connect( G_OBJECT( w ), "changed",
                                    G_CALLBACK( on_combo_changed ), el );
            }
            else
            {
                w = gtk_combo_box_text_new_with_entry();
                g_signal_connect( G_OBJECT( gtk_bin_get_child( GTK_BIN( w ) ) ),
                                    "key-press-event",
                                    G_CALLBACK( on_input_key_press ), el );
            }
            gtk_combo_box_set_focus_on_click( GTK_COMBO_BOX( w ), FALSE );
            set_font( w, font );
            gtk_box_pack_start( GTK_BOX( box ), w, FALSE, FALSE, pad );
            el->widgets = g_list_append( el->widgets, w );
            if ( radio ) *radio = NULL;
        }
        if ( el->widgets->next && args )
        {
            if ( ((char*)args->data)[0] == '@' )
            {
                // list from file
                if ( args->next )
                {
                    // get default value and command
                    if ( !strcmp( (char*)args->next->data, "--" ) )
                    {
                        // forgive extra --
                        if ( args->next->next )
                        {
                            // default value
                            el->def_val = (char*)args->next->next->data;
                            el->cmd_args = args->next->next->next;
                        }
                    }
                    else
                    {
                        // default value
                        el->def_val = (char*)args->next->data;
                        el->cmd_args = args->next->next;
                    }
                    if ( el->def_val )
                        get_text_value( el, el->def_val, FALSE, FALSE );
                }
                // read file into temp args
                str = (char*)args->data + 1;
                args = args_from_file( (char*)args->data + 1 );
                // fill combo from args
                fill_combo_box( el, args );
                if ( !el->monitor && args )
                {
                    // start monitoring file
                    el->callback = (VFSFileMonitorCallback)cb_file_value_change;
                    el->monitor = vfs_file_monitor_add( str, FALSE,
                                                                el->callback, el );
                }
                // free temp args
                g_list_foreach( args, (GFunc)g_free, NULL );
                g_list_free( args );
            }
            else
            {
                // get default value
                l = args;
                while ( l )
                {
                    if ( !strcmp( (char*)l->data, "--" ) )
                    {
                        if ( l->next )
                        {
                            // default value
                            el->def_val = (char*)l->next->data;
                            get_text_value( el, el->def_val, FALSE, FALSE );
                            el->cmd_args = l->next->next;
                        }
                        break;
                    }
                    l = l->next;
                }
                // fill combo from args
                fill_combo_box( el, args );
            }
        }
        break;
    case CDLG_LIST:
    case CDLG_MLIST:
        if ( !el->widgets->next && box )
        {
            w = gtk_tree_view_new();
            gtk_tree_view_set_rules_hint ( GTK_TREE_VIEW( w ), TRUE );
            gtk_tree_view_set_enable_search( GTK_TREE_VIEW( w ), TRUE );
            set_font( w, font );
            GtkTreeSelection* tree_sel = gtk_tree_view_get_selection( 
                                                    GTK_TREE_VIEW( w ) );
            gtk_tree_selection_set_mode( tree_sel, 
                        el->type == CDLG_MLIST ? 
                        GTK_SELECTION_MULTIPLE : GTK_SELECTION_SINGLE );
            GtkWidget* scroll = gtk_scrolled_window_new( NULL, NULL );
            gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( scroll ),
                                        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
            gtk_container_add ( GTK_CONTAINER ( scroll ), w );
            gtk_box_pack_start( GTK_BOX( box ), GTK_WIDGET( scroll ), TRUE, TRUE, pad );
            el->widgets = g_list_append( el->widgets, w );
            el->widgets = g_list_append( el->widgets, scroll );
            g_signal_connect ( G_OBJECT( w ), "row-activated",
                                    G_CALLBACK ( on_list_row_activated ),
                                    el );  // renderer cannot be editable
            if ( radio ) *radio = NULL;
        }
        if ( el->widgets->next && args )
        {
            if ( ((char*)args->data)[0] == '@' )
            {
                // list from file
                if ( args->next )
                {
                    // set command args
                    if ( !strcmp( (char*)args->next->data, "--" ) )
                        // forgive extra --
                        el->cmd_args = args->next->next;
                    else
                        el->cmd_args = args->next;
                }
                // read file into temp args
                str = (char*)args->data + 1;
                args = args_from_file( (char*)args->data + 1 );
                // fill list from args
                fill_tree_view( el, args );
                if ( !el->monitor && args )
                {
                    // start monitoring file
                    el->callback = (VFSFileMonitorCallback)cb_file_value_change;
                    el->monitor = vfs_file_monitor_add( str, FALSE,
                                                                el->callback, el );
                }
                // free temp args
                g_list_foreach( args, (GFunc)g_free, NULL );
                g_list_free( args );
            }
            else
                // fill list from args
                fill_tree_view( el, args );
        }

        break;
/*
    case CDLG_STATUS:
        if ( !el->widgets->next && box )
        {
            w =  gtk_statusbar_new();
            //gtk_box_pack_start( GTK_BOX( GTK_DIALOG( dlg )->action_area ),
            //                                GTK_WIDGET( w ), TRUE, TRUE, pad );
            gtk_box_pack_start( GTK_BOX( box ), w, FALSE, FALSE, pad );
            el->widgets = g_list_append( el->widgets, w );
            GList* children = gtk_container_get_children( 
                                GTK_CONTAINER( gtk_statusbar_get_message_area( 
                                    GTK_STATUSBAR( w ) ) ) );
            w = children->data; // status bar label
            el->widgets = g_list_append( el->widgets, w );
            g_list_free( children );
            gtk_label_set_selectable( GTK_LABEL( w ), TRUE ); // required for button event
            gtk_widget_set_can_focus( w, FALSE );
            g_signal_connect( G_OBJECT( w ), "button-press-event",
                                G_CALLBACK( on_status_button_press ), el );
            //g_signal_connect( G_OBJECT( w ), "populate-popup",
            //                  G_CALLBACK( on_status_bar_popup ), el );
        }
        if ( el->widgets->next && args )
        {
            get_text_value( el, (char*)args->data, FALSE, TRUE );
            gtk_statusbar_push( GTK_STATUSBAR( el->widgets->next->data ), 0, el->val );
        }
        break;
*/
    case CDLG_PROGRESS:
        if ( !el->widgets->next && box )
        {
            w = gtk_progress_bar_new();
            gtk_progress_bar_set_pulse_step( GTK_PROGRESS_BAR( w ), 0.08 );
            set_font( w, font );
            gtk_box_pack_start( GTK_BOX( box ), w, FALSE, FALSE, pad );
            el->widgets = g_list_append( el->widgets, w );
            if ( !args || ( args && !strcmp( (char*)args->data, "pulse" ) ) )
                el->timeout = g_timeout_add( 200, (GSourceFunc)on_progress_timer, el );
            if ( radio ) *radio = NULL;
       }
        if ( el->widgets->next && args )
        {
            get_text_value( el, (char*)args->data, FALSE, TRUE );
            if ( !g_strcmp0( el->val, "pulse" ) )
            {
                gtk_progress_bar_pulse( GTK_PROGRESS_BAR( el->widgets->next->data ) );
                gtk_progress_bar_set_text( 
                                    GTK_PROGRESS_BAR( el->widgets->next->data ),
                                    NULL );
            }
            else
            {
                if ( el->timeout )
                {
                    g_source_remove( el->timeout );
                    el->timeout = 0;
                }
                i = el->val ? atoi( el->val ) : 0;
                if ( i < 0 ) i = 0;
                if ( i > 100 ) i = 100;
                g_free( el->val );
                el->val = g_strdup_printf( "%d %%", i );
                gtk_progress_bar_set_fraction( 
                                    GTK_PROGRESS_BAR( el->widgets->next->data ),
                                    (gdouble)i / 100 );
                gtk_progress_bar_set_text( 
                                    GTK_PROGRESS_BAR( el->widgets->next->data ),
                                    el->val );
            }
        }
        break;
    case CDLG_HSEP:
    case CDLG_VSEP:
        if ( !el->widgets->next && box )
        {
            if ( el->type == CDLG_HSEP )
                w = gtk_hseparator_new();
            else
                w = gtk_vseparator_new();
            gtk_box_pack_start( GTK_BOX( box ), w, FALSE, FALSE, pad );
            el->widgets = g_list_append( el->widgets, w );
            if ( radio ) *radio = NULL;
        }
        break;
    case CDLG_HBOX:
    case CDLG_VBOX:
        if ( !el->widgets->next && box )
        {
            if ( args )
                get_text_value( el, (char*)args->data, FALSE, FALSE );
            if ( el->val )
                i = atoi( el->val );
            else
                i = 0;
            if ( i < 0 ) i = 0;
            if ( i > 400 ) i = 400;
            if ( el->type == CDLG_HBOX )
                w =   gtk_hbox_new( FALSE, i );
            else
                w =   gtk_vbox_new( FALSE, i );
            gtk_box_pack_start( GTK_BOX( box ), w, !box_compact, TRUE, pad );
            el->widgets = g_list_append( el->widgets, w );
            if ( radio ) *radio = NULL;
        }
        break;
    case CDLG_WINDOW_SIZE:
        if ( el->option && args && ((char*)args->data)[0] == '@' )
        {
            int width = -1, height = -1;
            get_text_value( el, (char*)args->data, FALSE, TRUE );
            get_width_height_pad( el->val, &width, &height, NULL );
            if ( width > 0 && height > 0 )
            {
                gtk_window_resize( GTK_WINDOW( el->widgets->data ), width, height );
                gtk_window_set_position( GTK_WINDOW( el->widgets->data ),
                                                    GTK_WIN_POS_CENTER_ALWAYS );
            }
            else
                dlg_warn( _("Dynamic resize requires width and height > 0"),
                                                                NULL, NULL );
        }
        break;
    case CDLG_TIMEOUT:
        if ( !el->widgets->next && box )
        {
            if ( args )
                get_text_value( el, (char*)args->data, FALSE, FALSE );
            el->option = el->val ? atoi( el->val ) : 20;
            if ( el->option <= 0 )
                el->option = 20;
        }
        break;
    case CDLG_CHOOSER:
        if ( !el->widgets->next && box )
        {
            if ( chooser_dir )
                i = chooser_save ? GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER :
                                  GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
            else if ( chooser_save )
                i = GTK_FILE_CHOOSER_ACTION_SAVE;
            else
                i = GTK_FILE_CHOOSER_ACTION_OPEN;
            w = gtk_file_chooser_widget_new( i );
            if ( chooser_multi )
                gtk_file_chooser_set_select_multiple( GTK_FILE_CHOOSER( w ),
                                                                        TRUE );
            gtk_box_pack_start( GTK_BOX( box ), w, TRUE, TRUE, pad );
            el->widgets = g_list_append( el->widgets, w );
            g_signal_connect( G_OBJECT( w ), "file-activated",
                                    G_CALLBACK( on_chooser_activated ), el );
            if ( radio ) *radio = NULL;
            if ( args && args->next )
                el->cmd_args = args->next;
            // filters
            if ( chooser_filters )
            {
                for ( l = chooser_filters; l; l = l->next )
                {
                    GtkFileFilter* filter = gtk_file_filter_new();
                    str = (char*)l->data;
                    while ( str )
                    {
                        if ( sep = strchr( str, ':' ) )
                            sep[0] = '\0';
                        if ( strchr( str, '/' ) )
                            gtk_file_filter_add_mime_type( filter, str );
                        else
                            gtk_file_filter_add_pattern( filter, str );
                        if ( sep )
                        {
                            sep[0] = ':';
                            str = sep + 1;
                        }
                        else
                            str = NULL;
                    }
                    gtk_file_filter_set_name( filter, (char*)l->data );
                    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER( w ), filter );
                    if ( l == chooser_filters )
                        // note: set_filter only works if gtk_file_chooser_set_filename
                        // is NOT used
                        gtk_file_chooser_set_filter( GTK_FILE_CHOOSER( w ), filter );
                }
                g_list_free( chooser_filters );
            }
        }
        // change dir/file
        if ( args && el->widgets->next )
        {
            get_text_value( el, (char*)args->data, FALSE, TRUE );
            if ( el->val )
            {
                if ( chooser_save )
                {
                    if ( strchr( el->val, '/' ) )
                    {
                        str = g_path_get_dirname( el->val );
                        gtk_file_chooser_set_current_folder( 
                                        GTK_FILE_CHOOSER( el->widgets->next->data ),
                                                                        str );
                        g_free( str );
                        str = g_path_get_basename( el->val );
                        gtk_file_chooser_set_current_name( 
                                        GTK_FILE_CHOOSER( el->widgets->next->data ),
                                                                        str );
                        g_free( str );
                    }
                    else
                        gtk_file_chooser_set_current_name( 
                                        GTK_FILE_CHOOSER( el->widgets->next->data ),
                                                                        el->val );
                }
                else if ( g_file_test( el->val, G_FILE_TEST_IS_DIR ) )
                    gtk_file_chooser_set_current_folder( 
                                    GTK_FILE_CHOOSER( el->widgets->next->data ),
                                                                        el->val );
                else
                    gtk_file_chooser_set_filename( 
                                        GTK_FILE_CHOOSER( el->widgets->next->data ),
                                                                        el->val );
            }
        }
        break;
    case CDLG_KEYPRESS:
        if ( !el->cmd_args && args && args->next && args->next->next )
        {
            int keycode = strtol( args->data, NULL, 0 );
            int modifier = strtol( args->next->data, NULL, 0 );
            if ( keycode != 0 )
                g_signal_connect( G_OBJECT( el->widgets->data ), "key-press-event",
                                                G_CALLBACK( on_dlg_key_press), el );
            el->cmd_args = args->next->next;
            if ( radio ) *radio = NULL;
        }
        break;
    }
}

static void build_dialog( GList* elements )
{
    GList* l;
    GtkWidget* dlg;
    CustomElement* el;
    CustomElement* focus_el = NULL;
    char* str;
    char* sep;
    GSList* radio = NULL;
    GtkWidget* box;
    int pad = DEFAULT_PAD;
    int width = DEFAULT_WIDTH;
    int height = DEFAULT_HEIGHT;
    gboolean timeout_added = FALSE;
    gboolean is_sized = FALSE;
    gboolean is_large = FALSE;
    
    // create dialog
    dlg = gtk_dialog_new();
    gtk_window_set_default_size( GTK_WINDOW( dlg ), width, height );
    gtk_window_set_title( GTK_WINDOW( dlg ), DEFAULT_TITLE );
    gtk_window_set_position( GTK_WINDOW( dlg ), GTK_WIN_POS_CENTER_ALWAYS );
    GdkPixbuf* pixbuf = gtk_icon_theme_load_icon( gtk_icon_theme_get_default(),
                    DEFAULT_ICON, 16, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );
    if ( pixbuf )
        gtk_window_set_icon( GTK_WINDOW( dlg ), pixbuf ); 
    g_object_set_data( G_OBJECT( dlg ), "elements", elements );
    g_signal_connect( G_OBJECT( dlg ), "destroy", G_CALLBACK( on_dlg_close ), NULL );

    // pack some boxes to create horizonal padding at edges of window
    GtkWidget* hbox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( GTK_DIALOG( dlg )->vbox ), hbox, TRUE, TRUE, 0 );
    box = gtk_vbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( hbox ), box, TRUE, TRUE, 8 );  // <- hpad 
    GList* boxes = g_list_append( NULL, box );

    // pack timeout button first
    GtkWidget* timeout_toggle = gtk_toggle_button_new_with_label( _("Pause") );
    gtk_box_pack_start( GTK_BOX( GTK_DIALOG( dlg )->action_area ),
                                            timeout_toggle, FALSE, FALSE, pad );
    gtk_button_set_image( GTK_BUTTON( timeout_toggle ),
                                            xset_get_image( "GTK_STOCK_MEDIA_PAUSE",
                                            GTK_ICON_SIZE_BUTTON ) );
    gtk_button_set_focus_on_click( GTK_BUTTON( timeout_toggle ), FALSE );

    // add elements
    for ( l = elements; l; l = l->next )
    {
        el = (CustomElement*)l->data;
        el->widgets = g_list_append( NULL, dlg );
        update_element( el, box, &radio, pad );
        if ( !focus_el && ( el->type == CDLG_INPUT
                         || el->type == CDLG_INPUT_LARGE
                         || el->type == CDLG_EDITOR
                         || el->type == CDLG_COMBO
                         || el->type == CDLG_PASSWORD ) )
            focus_el = el;
        else if ( ( el->type == CDLG_HBOX || el->type == CDLG_VBOX )
                                        && el->widgets->next )
        {
            box = el->widgets->next->data;
            boxes = g_list_append( boxes, box );
        }
        else if ( el->type == CDLG_CLOSE_BOX && g_list_length( boxes ) > 1 )
        {
            box = g_list_last( boxes )->prev->data;
            boxes = g_list_delete_link( boxes, g_list_last( boxes ) );
        }
        else if ( el->type == CDLG_WINDOW_SIZE && el->args )
        {
            get_text_value( el, (char*)el->args->data, FALSE, TRUE );
            get_width_height_pad( el->val, &width, &height, &pad );
            if ( el->args->next && atoi( (char*)el->args->next->data ) > 0 )
                pad = atoi( (char*)el->args->next->data );
            gtk_window_set_default_size( GTK_WINDOW( dlg ), width, height );
            el->option = 1; // activates auto resize from @FILE
            is_sized = TRUE;
        }
        else if ( el->type == CDLG_TIMEOUT && el->option && !el->widgets->next
                                                      && !timeout_added )
        {
            el->widgets = g_list_append( el->widgets, timeout_toggle );
            el->timeout = g_timeout_add( 1000, (GSourceFunc)on_timeout_timer, el );
            g_signal_connect( G_OBJECT( timeout_toggle ), "toggled",
                                        G_CALLBACK( on_option_toggled ), el );
            g_free( el->val );
            el->val = g_strdup_printf( "%s %d", _("Pause"), el->option );
            gtk_button_set_label( GTK_BUTTON( el->widgets->next->data ), el->val );
            timeout_added = TRUE;
        }
        if ( !is_large && el->widgets->next && (
                                                el->type == CDLG_CHOOSER || 
                                                el->type == CDLG_MLIST || 
                                                el->type == CDLG_EDITOR || 
                                                el->type == CDLG_VIEWER || 
                                                el->type == CDLG_LIST ) )
            is_large = TRUE;
    }
    g_list_free( boxes );

    // resize window
    if ( is_large && !is_sized )
        gtk_window_set_default_size( GTK_WINDOW( dlg ), DEFAULT_LARGE_WIDTH,
                                                        DEFAULT_LARGE_HEIGHT );
    
    // show dialog
    gtk_widget_show_all( dlg );
    if ( !timeout_added )
        gtk_widget_hide( timeout_toggle );

    // focus input
    if ( focus_el && focus_el->widgets->next )
    {
        gtk_widget_grab_focus( focus_el->widgets->next->data );
        if ( focus_el->type == CDLG_INPUT && focus_el->option >= 0 )
        {
            // grab_focus causes all text to be selected, so re-select
            gtk_editable_select_region( 
                        GTK_EDITABLE( focus_el->widgets->next->data ),
                        focus_el->option, focus_el->option2 );
        }
    }
    signal_dialog = dlg;
    
    // run init COMMMAND(s)
    for ( l = elements; l; l = l->next )
    {
        if ( ((CustomElement*)l->data)->type == CDLG_COMMAND
                                    && ((CustomElement*)l->data)->cmd_args )
            run_command( (CustomElement*)l->data,
                                    ((CustomElement*)l->data)->cmd_args, NULL );
    }
}

static void show_help()
{
    int i, j;
    FILE* f = stdout;
    
    fprintf( f, _("SpaceFM Dialog creates a custom GTK dialog based on the GUI elements you\nspecify on the command line, features run-time internal/external commands which\ncan modify elements, and outputs evaluatable/parsable results.\n") );
    fprintf( f, _("Usage:\n") );
    fprintf( f, _("    spacefm --dialog|-g {ELEMENT [OPTIONS] [ARGUMENTS...]} ...\n") );
    fprintf( f, _("Example:\n") );
    fprintf( f, _("    spacefm -g --label \"A message\" --button ok\n") );
    fprintf( f, _("\nELEMENT:       OPTIONS & ARGUMENTS:\n") );
    fprintf( f, _(  "--------       --------------------\n") );

    for ( i = 0; i < G_N_ELEMENTS( cdlg_option ) / 3; i++ )
    {
        fprintf( f, "--%s", cdlg_option[i*3] );
        for ( j = 1; j <= 13 - strlen( cdlg_option[i*3] ); j++ )
            fprintf( f, " " );
        fprintf( f, "%s\n", cdlg_option[i*3 + 1] );
        fprintf( f, "               %s\n", cdlg_option[i*3 + 2] );
    }

    fprintf( f, _("\nThe following arguments may be used as shown above:\n") );
    fprintf( f, _("    STOCK    One of: %s\n"), "ok cancel close open yes no apply delete edit save stop" );
    fprintf( f, _("    ICON     An icon name, eg:  gtk-open\n") );
    fprintf( f, _("    @FILE    A text file from which to read a value.  In some cases this file\n             is monitored, so writing a new value to the file will update the\n             element.  In other cases, the file specifies an initial value.\n") );
    fprintf( f, _("    SAVEFILE An editor's contents are saved to this file if specified.\n") );
    fprintf( f, _("    COMMAND  An internal command or executable followed by arguments. Separate\n             multiple commands with a -- argument.  eg: echo '#1' -- echo '#2'\n             The following substitutions may be used in COMMANDs:\n                 %%n           Name of the current element\n                 %%v           Value of the current element\n                 %%NAME        Value of element named NAME (eg: %%input1)\n                 %%(command)   stdout from a command\n                 %%%%           %%\n") );
    fprintf( f, _("    LABEL    The following escape sequences in LABEL are unescaped:\n                 \\n   newline\n                 \\t   tab\n                 \\\"   \"\n                 \\\\   \\\n             In --label elements only, if the first character in LABEL is a\n             tilde (~), pango markup may be used.  For example:\n                 --label '~This is plain. <b>This is bold.</b>'\n") );
    
    fprintf( f, _("\nIn addition to the OPTIONS listed above, a --font option may be used with most\nelement types to change the element's font and font size.  For example:\n    --input --font \"Times New Roman 16\" \"Default Text\"\n") );
    
    fprintf( f, _("\nINTERNAL COMMANDS:\n") );

    for ( i = 0; i < G_N_ELEMENTS( cdlg_cmd ) / 3; i++ )
    {
        fprintf( f, "    %s", cdlg_cmd[i*3] );
        for ( j = 1; j <= 11 - strlen( cdlg_cmd[i*3] ); j++ )
            fprintf( f, " " );
        fprintf( f, "%s\n", cdlg_cmd[i*3 + 1] );
        fprintf( f, "               %s\n", cdlg_cmd[i*3 + 2] );
    }

    fprintf( f, _("\nEXAMPLE WITH COMMANDS:\n") );
    fprintf( f, _("    spacefm -g --label \"Enter some text and press Enter:\" \\\n               --input \"\" set label2 %%v -- echo '# %%n = %%v' \\\n               --label \\\n               --button ok\n") );
    
    fprintf( f, _("\nEXAMPLE SCRIPT:\n") );
    fprintf( f, _("    #!/bin/bash\n    # This script shows a Yes/No dialog\n    # Use QUOTED eval to read variables output by SpaceFM Dialog:\n    eval \"`spacefm -g --label \"Are you sure?\" --button yes --button no`\"\n    if [[ \"$dialog_pressed\" == \"button1\" ]]; then\n        echo \"User pressed Yes - take some action\"\n    else\n        echo \"User did NOT press Yes - abort\"\n    fi\n") );
    fprintf( f, _("\nFor full documentation and examples see the SpaceFM User's Manual:\n") );
    fprintf( f, "    %s\n\n", DEFAULT_MANUAL );
}

void signal_handler()
{
    if ( signal_dialog )
    {
        write_source( signal_dialog, NULL, stdout );
        destroy_dlg( signal_dialog );
    }
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
        else if ( ac == 2 && ( !strcmp( argv[ac], "--help" )
                            || !strcmp( argv[ac], "help" ) ) )
        {
            show_help();
            return 1;
        }
        else if ( g_str_has_prefix( argv[ac], "--" ) )
        {
            j = 0;
            for ( i = 0; i < G_N_ELEMENTS( cdlg_option ); i += 3 )
            {
                if ( !strcmp( argv[ac] + 2, cdlg_option[i] ) )
                {
                    el = g_slice_new( CustomElement );
                    el->type = j;
                    type_count[j]++;
                    num = g_strdup_printf( "%d", type_count[j] );
                    str = replace_string( cdlg_option[i], "-", "", FALSE );
                    el->name = g_strdup_printf( "%s%s", str,
                                                        num ? num : "" );
                    g_free( num );
                    g_free( str );
                    el->args = NULL;
                    el->def_val = NULL;
                    el->cmd_args = NULL;
                    el->val = NULL;
                    el->widgets = NULL;
                    el->monitor = NULL;
                    el->callback = NULL;
                    el->timeout = 0;
                    el->watch_file = NULL;
                    el->option = 0;
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
    
    signal( SIGHUP, signal_handler );
    signal( SIGINT, signal_handler );
    signal( SIGTERM, signal_handler );
    signal( SIGQUIT, signal_handler );
    return 0;
}

