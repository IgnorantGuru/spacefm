#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

//#include <string.h>
#include <gtk/gtk.h>
#include "exo-tree-view.h"
#include "gtk2-compat.h"

#include <stdlib.h>

#include <glib/gi18n.h>

#include "item-prop.h"

const char* enter_command_use = N_("Enter program or bash command line(s):\n\nUse:\n\t%F\tselected files  or  %f first selected file\n\t%N\tselected filenames  or  %n first selected filename\n\t%d\tcurrent directory\n\t%v\tselected device (eg /dev/sda1)\n\t%m\tdevice mount point (eg /media/dvd);  %l device label\n\t%b\tselected bookmark\n\t%t\tselected task directory;  %p task pid\n\t%a\tmenu item value\n\t$fm_panel, $fm_tab, $fm_command, etc");

enum {
    CONTEXT_COL_DISP,
    CONTEXT_COL_SUB,
    CONTEXT_COL_COMP,
    CONTEXT_COL_VALUE
};

typedef struct
{
    GtkWidget* dlg;
    GtkWidget* parent;
    GtkWidget* notebook;
    XSetContext* context;
    
    // Menu Item Page
    GtkWidget* item_type;
    GtkWidget* item_name;
    GtkWidget* item_key;
    GtkWidget* item_icon;
    GtkWidget* item_target_scroll;
    GtkWidget* item_target;
    GtkWidget* item_browse;
    
    // Context Page
    GtkWidget* view;
    GtkButton* btn_remove;
    GtkButton* btn_add;
    GtkButton* btn_apply;
    GtkButton* btn_ok;

    GtkWidget* box_sub;
    GtkWidget* box_comp;
    GtkWidget* box_value;
    GtkWidget* box_match;
    GtkWidget* box_action;
    GtkLabel* current_value;
    GtkLabel* test;
    
    GtkWidget* hbox_match;
    GtkFrame* frame;
    
    // Command Page
    GtkWidget* cmd_opt_line;
    GtkWidget* cmd_opt_cmd;
    GtkWidget* cmd_edit;
    GtkWidget* cmd_line_label;
    GtkWidget* cmd_scroll_script;
    GtkWidget* cmd_script;
    GtkWidget* cmd_opt_normal;
    GtkWidget* cmd_opt_checkbox;
    GtkWidget* cmd_opt_confirm;
    GtkWidget* cmd_opt_input;
    GtkWidget* cmd_vbox_msg;
    GtkWidget* cmd_scroll_msg;
    GtkWidget* cmd_msg;
    GtkWidget* opt_terminal;
    GtkWidget* opt_keep_term;
    GtkWidget* cmd_user;
    GtkWidget* opt_task;
    GtkWidget* opt_task_err;
    GtkWidget* opt_task_out;
    GtkWidget* opt_scroll;
    GtkWidget* opt_hbox_task;
} ContextData;

static const char* context_sub[] = 
{
    N_("MIME Type"),
    N_("Filename"),
    N_("Directory"),
    N_("Dir Write Access"),
    N_("File Is Text"),
    N_("File Is Dir"),
    N_("File Is Link"),
    N_("User Is Root"),
    N_("Multiple Selected"),
    N_("Clipboard Has Files"),
    N_("Clipboard Has Text"),
    N_("Current Panel"),
    N_("Panel Count"),
    N_("Current Tab"),
    N_("Tab Count"),
    N_("Bookmark"),
    N_("Device"),
    N_("Device Mount Point"),
    N_("Device Label"),
    N_("Device FSType"),
    N_("Device UDI"),
    N_("Device Properties"),
    N_("Task Count"),
    N_("Task Directory"),
    N_("Task Type"),
    N_("Task Name"),
    N_("Panel 1 Directory"),
    N_("Panel 2 Directory"),
    N_("Panel 3 Directory"),
    N_("Panel 4 Directory"),
    N_("Panel 1 Has Sel"),
    N_("Panel 2 Has Sel"),
    N_("Panel 3 Has Sel"),
    N_("Panel 4 Has Sel"),
    N_("Panel 1 Device"),
    N_("Panel 2 Device"),
    N_("Panel 3 Device"),
    N_("Panel 4 Device")
};

static const char* context_sub_list[] = 
{
    "4%%%%%application/%%%%%audio/%%%%%audio/ || video/%%%%%image/%%%%%inode/directory%%%%%text/%%%%%video/%%%%%application/x-bzip||application/x-bzip-compressed-tar||application/x-gzip||application/zip||application/x-7z-compressed||application/x-bzip2||application/x-bzip2-compressed-tar||application/x-xz-compressed-tar||application/x-compressed-tar||application/x-rar",  //"MIME Type",
    "6%%%%%archive_types || .gz || .bz2 || .7z || .xz || .txz || .tgz || .zip || .rar || .tar || .tar.gz || .tar.xz || .tar.bz2 || .tar.7z%%%%%audio_types || .mp3 || .MP3 || .m3u || .wav || .wma || .aac || .ac3 || .flac || .ram || .m4a || .ogg%%%%%image_types || .jpg || .jpeg || .gif || .png || .xpm%%%%%video_types || .mp4 || .MP4 || .avi || .AVI || .mkv || .mpeg || .mpg || .flv || .vob || .asf || .rm || .m2ts || .mov",  //"Filename",
    "0%%%%%",  //"Dir",
    "0%%%%%false%%%%%true",  //"Dir Write Access",
    "0%%%%%false%%%%%true",  //"File Is Text",
    "0%%%%%false%%%%%true",  //"File Is Dir",
    "0%%%%%false%%%%%true",  //"File Is Link",
    "0%%%%%false%%%%%true",  //"User Is Root",
    "0%%%%%false%%%%%true",  //"Multiple Selected",
    "0%%%%%false%%%%%true",  //"Clipboard Has Files",
    "0%%%%%false%%%%%true",  //"Clipboard Has Text",
    "0%%%%%1%%%%%2%%%%%3%%%%%4",  //"Current Panel",
    "0%%%%%1%%%%%2%%%%%3%%%%%4",  //"Panel Count",
    "0%%%%%1%%%%%2%%%%%3%%%%%4%%%%%5%%%%%6",  //"Current Tab",
    "0%%%%%1%%%%%2%%%%%3%%%%%4%%%%%5%%%%%6",  //"Tab Count",
    "0%%%%%",  //"Bookmark",
    "0%%%%%/dev/sdb1%%%%%/dev/sdc1%%%%%/dev/sdd1%%%%%/dev/sr0",  //"Device",
    "0%%%%%",  //"Device Mount Point",
    "0%%%%%",  //"Device Label",
    "0%%%%%btrfs%%%%%ext2%%%%%ext3%%%%%ext4%%%%%ext2 || ext3 || ext4%%%%%ntfs%%%%%reiser4%%%%%reiserfs%%%%%swap%%%%%ufs%%%%%vfat%%%%%xfs",  //Device FSType",
    "0%%%%%",  //"Device UDI",
    "2%%%%%audiocd%%%%%blank%%%%%dvd%%%%%dvd && blank%%%%%ejectable%%%%%floppy%%%%%internal%%%%%mountable%%%%%mounted%%%%%no_media%%%%%optical%%%%%optical && blank%%%%%optical && mountable%%%%%optical && mounted%%%%%removable%%%%%removable && mountable%%%%%removable && mounted%%%%%removable || optical%%%%%table%%%%%policy_hide%%%%%policy_noauto",  //"Device Properties",
    "8%%%%%0%%%%%1%%%%%2",  //"Task Count",
    "0%%%%%",  //"Task Dir",
    "0%%%%%change%%%%%copy%%%%%delete%%%%%link%%%%%move%%%%%run%%%%%trash",  //"Task Type",
    "0%%%%%",  //"Task Name",
    "0%%%%%",  //"Panel 1 Dir",
    "0%%%%%",  //"Panel 2 Dir",
    "0%%%%%",  //"Panel 3 Dir",
    "0%%%%%",  //"Panel 4 Dir",
    "0%%%%%false%%%%%true",  //"Panel 1 Has Sel",
    "0%%%%%false%%%%%true",  //"Panel 2 Has Sel",
    "0%%%%%false%%%%%true",  //"Panel 3 Has Sel",
    "0%%%%%false%%%%%true",  //"Panel 4 Has Sel",
    "0%%%%%dev/sdb1%%%%%/dev/sdc1%%%%%/dev/sdd1%%%%%/dev/sr0",  //"Panel 1 Device",
    "0%%%%%dev/sdb1%%%%%/dev/sdc1%%%%%/dev/sdd1%%%%%/dev/sr0",  //"Panel 2 Device",
    "0%%%%%dev/sdb1%%%%%/dev/sdc1%%%%%/dev/sdd1%%%%%/dev/sr0",  //"Panel 3 Device",
    "0%%%%%dev/sdb1%%%%%/dev/sdc1%%%%%/dev/sdd1%%%%%/dev/sr0"  //"Panel 4 Device"
};

enum {
    CONTEXT_COMP_EQUALS,
    CONTEXT_COMP_NEQUALS,
    CONTEXT_COMP_CONTAINS,
    CONTEXT_COMP_NCONTAINS,
    CONTEXT_COMP_BEGINS,
    CONTEXT_COMP_NBEGINS,
    CONTEXT_COMP_ENDS,
    CONTEXT_COMP_NENDS,
    CONTEXT_COMP_LESS,
    CONTEXT_COMP_GREATER
};

static const char* context_comp[] = 
{
    N_("equals"),
    N_("doesn't equal"),
    N_("contains"),
    N_("doesn't contain"),
    N_("begins with"),
    N_("doesn't begin with"),
    N_("ends with"),
    N_("doesn't end with"),
    N_("is less than"),
    N_("is greater than")
};

static const char* item_types[] = 
{
    N_("Bookmark"),
    N_("Application"),
    N_("Command"),
    N_("Built-In"),
    N_("Sub Menu"),
    N_("Separator")
};

static char* get_element_next( char** s )
{
    char* ret;
    
    if ( !*s )
        return NULL;
    char* sep = strstr( *s, "%%%%%" );
    if ( !sep )
    {
        if ( *s[0] == '\0' )
            return ( *s = NULL );
        ret = g_strdup( *s );
        *s = NULL;
        return ret;
    }
    ret = g_strndup( *s, sep - *s );
    *s = sep + 5;
    return ret;
}

gboolean get_rule_next( char** s, int* sub, int* comp, char** value )
{
    char* vs;
    vs = get_element_next( s );
    if ( !vs )
        return FALSE;
    *sub = atoi( vs );
    g_free( vs );
    if ( *sub < 0 || *sub >= G_N_ELEMENTS( context_sub ) )
        return FALSE;
    vs = get_element_next( s );
    *comp = atoi( vs );
    g_free( vs );
    if ( *comp < 0 || *comp >= G_N_ELEMENTS( context_comp ) )
        return FALSE;
    if ( !( *value = get_element_next( s ) ) )
        *value = g_strdup( "" );
    return TRUE;
}

int xset_context_test( XSetContext* context, char* rules, gboolean def_disable )
{
    // assumes valid xset_context and rules != NULL and no global ignore
    int i, sep_type, sub, comp;
    char* value;
    int match, action;
    char* s;
    char* eleval;
    char* sep;
    gboolean test;
    enum { ANY, ALL, NANY, NALL };

    // get valid action and match
    char* elements = rules;
    if ( !( s = get_element_next( &elements ) ) )
        return 0;
    action = atoi( s );
    g_free( s );
    if ( action < 0 || action > 3 )
        return 0;

    if ( !( s = get_element_next( &elements ) ) )
        return 0;
    match = atoi( s );
    g_free( s );
    if ( match < 0 || match > 3 )
        return 0;
    
    if ( action != CONTEXT_HIDE && action != CONTEXT_SHOW && def_disable )
        return CONTEXT_DISABLE;

    // parse rules
    gboolean is_rules = FALSE;
    gboolean all_match = TRUE;
    gboolean no_match = TRUE;
    gboolean any_match = FALSE;
    while ( get_rule_next( &elements, &sub, &comp, &value ) )
    {
        is_rules = TRUE;

        eleval = value;
        do
        {
            if ( sep = strstr( eleval, "||" ) )
                sep_type = 1;
            else if ( sep = strstr( eleval, "&&" ) )
                sep_type = 2;

            if ( sep )
            {
                sep[0] = '\0';
                i = -1;
                // remove trailing spaces from eleval
                while ( sep + i >= eleval && sep[i] == ' ' )
                {
                    sep[i] = '\0';
                    i--;
                }
            }
            
            switch ( comp )
            {
            case CONTEXT_COMP_EQUALS:
                test = !strcmp( context->var[sub], eleval );
                break;
            case CONTEXT_COMP_NEQUALS:
                test = strcmp( context->var[sub], eleval );        
                break;
            case CONTEXT_COMP_CONTAINS:
                test = !!strstr( context->var[sub], eleval );
                break;
            case CONTEXT_COMP_NCONTAINS:
                test = !strstr( context->var[sub], eleval );
                break;
            case CONTEXT_COMP_BEGINS:
                test = g_str_has_prefix( context->var[sub], eleval );
                break;
            case CONTEXT_COMP_NBEGINS:
                test = !g_str_has_prefix( context->var[sub], eleval );
                break;
            case CONTEXT_COMP_ENDS:
                test = g_str_has_suffix( context->var[sub], eleval );
                break;
            case CONTEXT_COMP_NENDS:
                test = !g_str_has_suffix( context->var[sub], eleval );
                break;
            case CONTEXT_COMP_LESS:
                test = atoi( context->var[sub] ) < atoi( eleval );
                break;
            case CONTEXT_COMP_GREATER:
                test = atoi( context->var[sub] ) > atoi( eleval );
                break;
            default:
                test = match == NANY || match == NALL;  //failsafe
            }
        
            if ( sep )
            {
                if ( test )
                {
                    if ( sep_type == 1 ) // ||
                        break;
                }
                else
                {
                    if ( sep_type == 2 ) // &&
                        break;
                }
                eleval = sep + 2;
                while ( eleval[0] == ' ' )
                    eleval++;
            }
            else
                eleval[0] = '\0';
        } while ( eleval[0] != '\0' );
        g_free( value );

        if ( test )
        {
            any_match = TRUE;
            no_match = FALSE;
            if ( match == ANY || match == NANY || match == NALL )
                break;
        }
        else
        {
            all_match = FALSE;
            if ( match == ALL )
                break;
        }
    }

    if ( !is_rules )
        return CONTEXT_SHOW;

    gboolean is_match;
    if ( match == ALL )
        is_match = all_match;
    else if ( match == NALL )
        is_match = !any_match;
    else if ( match == NANY )
        is_match = no_match;
    else // ANY
        is_match = !no_match;

    if ( action == CONTEXT_SHOW )
        return is_match ? CONTEXT_SHOW : CONTEXT_HIDE;
    if ( action == CONTEXT_ENABLE )
        return is_match ? CONTEXT_SHOW : CONTEXT_DISABLE;
    if ( action == CONTEXT_DISABLE )
        return is_match ? CONTEXT_DISABLE : CONTEXT_SHOW;
    // CONTEXT_HIDE
    if ( is_match )
        return CONTEXT_HIDE;
    return def_disable ? CONTEXT_DISABLE : CONTEXT_SHOW;
}

char* context_build( ContextData* ctxt )
{
    GtkTreeIter it;
    char* value;
    int sub, comp;
    char* new_context = NULL;
    char* old_context;
    
    GtkTreeModel* model = gtk_tree_view_get_model( GTK_TREE_VIEW( ctxt->view ) );
    if ( gtk_tree_model_get_iter_first( model, &it ) )
    {
        new_context = g_strdup_printf( "%d%%%%%%%%%%%d",
                        gtk_combo_box_get_active( GTK_COMBO_BOX( ctxt->box_action ) ),
                        gtk_combo_box_get_active( GTK_COMBO_BOX( ctxt->box_match ) ) );
        do
        {
            gtk_tree_model_get( model, &it, 
                                CONTEXT_COL_VALUE, &value,
                                CONTEXT_COL_SUB, &sub,
                                CONTEXT_COL_COMP, &comp,
                                -1 );
            old_context = new_context;
            new_context = g_strdup_printf( "%s%%%%%%%%%%%d%%%%%%%%%%%d%%%%%%%%%%%s",
                                                old_context, sub, comp, value );
            g_free( old_context );
        }
        while ( gtk_tree_model_iter_next( model, &it ) );
    }
    return new_context;
}

void enable_context( ContextData* ctxt )
{
    GtkTreeIter it;
    gboolean is_sel = gtk_tree_selection_get_selected( 
                        gtk_tree_view_get_selection( GTK_TREE_VIEW( ctxt->view ) ),
                        NULL, NULL );
    gtk_widget_set_sensitive( GTK_WIDGET( ctxt->btn_remove ), is_sel );
    gtk_widget_set_sensitive( GTK_WIDGET( ctxt->btn_apply ), is_sel );
    gtk_widget_set_sensitive( GTK_WIDGET( ctxt->hbox_match ),
                        gtk_tree_model_get_iter_first( 
                        gtk_tree_view_get_model( GTK_TREE_VIEW( ctxt->view ) ), &it ) );
    if ( ctxt->context && ctxt->context->valid )
    {
        char* rules = context_build( ctxt );
        char* text = _("Current: Show");
        if ( rules )
        {
            int action = xset_context_test( ctxt->context, rules, FALSE );
            if ( action == CONTEXT_HIDE )
                text = _("Current: Hide");
            else if ( action == CONTEXT_DISABLE )
                text = _("Current: Disable");
        }
        gtk_label_set_text( ctxt->test, text );
    }
}

void on_context_action_changed( GtkComboBox* box, ContextData* ctxt )
{
    enable_context( ctxt );
}

char* context_display( int sub, int comp, char* value )
{
    char* disp;
    if ( value[0] == '\0' || value[0] == ' ' || g_str_has_suffix( value, " " ) )
        disp = g_strdup_printf( "%s %s \"%s\"", _(context_sub[sub]), _(context_comp[comp]),
                                                                            value );
    else
        disp = g_strdup_printf( "%s %s %s", _(context_sub[sub]), _(context_comp[comp]),
                                                                            value );
    return disp;
}

void on_context_button_press( GtkWidget* widget, ContextData* ctxt )
{
    GtkTreeIter it;
    GtkTreeSelection* tree_sel;
    GtkTreeModel* model;

    if ( widget == GTK_WIDGET( ctxt->btn_add ) || 
                                        widget == GTK_WIDGET( ctxt->btn_apply ) )
    {
        int sub = gtk_combo_box_get_active( GTK_COMBO_BOX( ctxt->box_sub ) );
        int comp = gtk_combo_box_get_active( GTK_COMBO_BOX( ctxt->box_comp ) );
        if ( sub < 0 || comp < 0 )
            return;
        model = gtk_tree_view_get_model( GTK_TREE_VIEW( ctxt->view ) );
        if ( widget == GTK_WIDGET( ctxt->btn_add ) )
            gtk_list_store_append( GTK_LIST_STORE( model ), &it );
        else
        {
            tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( ctxt->view ) );
            if ( !gtk_tree_selection_get_selected( tree_sel, NULL, &it ) )
                return;
        }
        char* value = gtk_combo_box_text_get_active_text( 
                                        GTK_COMBO_BOX_TEXT( ctxt->box_value ) );
        char* disp = context_display( sub, comp, value );
        gtk_list_store_set( GTK_LIST_STORE( model ), &it,
                                    CONTEXT_COL_DISP, disp,
                                    CONTEXT_COL_SUB, sub,
                                    CONTEXT_COL_COMP, comp,
                                    CONTEXT_COL_VALUE, value,
                                    -1 );
        g_free( disp );
        g_free( value );
        gtk_widget_set_sensitive( GTK_WIDGET( ctxt->btn_ok ), TRUE );
        if ( widget == GTK_WIDGET( ctxt->btn_add ) )
            gtk_tree_selection_select_iter( gtk_tree_view_get_selection(
                                        GTK_TREE_VIEW( ctxt->view ) ), &it );
        enable_context( ctxt );
        return;
    }
    
    //remove
    model = gtk_tree_view_get_model( GTK_TREE_VIEW( ctxt->view ) );
    tree_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW( ctxt->view ) );
    if ( gtk_tree_selection_get_selected( tree_sel, NULL, &it ) )
        gtk_list_store_remove( GTK_LIST_STORE( model ), &it );
        
    enable_context( ctxt );
}

void on_context_sub_changed( GtkComboBox* box, ContextData* ctxt )
{
    GtkTreeIter it;
    char* value;
    
    GtkTreeModel* model = gtk_combo_box_get_model( GTK_COMBO_BOX( ctxt->box_value ) );
    while ( gtk_tree_model_get_iter_first( model, &it ) )
        gtk_list_store_remove( GTK_LIST_STORE( model ), &it );
    
    int sub = gtk_combo_box_get_active( GTK_COMBO_BOX( ctxt->box_sub ) );
    if ( sub < 0 )
        return;
    char* elements = (char*)context_sub_list[sub];
    char* def_comp = get_element_next( &elements );
    if ( def_comp )
    {
        gtk_combo_box_set_active( GTK_COMBO_BOX( ctxt->box_comp ), atoi( def_comp ) );
        g_free( def_comp );
    }
    while ( value = get_element_next( &elements ) )
    {
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( ctxt->box_value ), value );    
        g_free( value );
    }
    gtk_entry_set_text( GTK_ENTRY( gtk_bin_get_child( GTK_BIN( ctxt->box_value ) ) ), "" );
    if ( ctxt->context && ctxt->context->valid )
        gtk_label_set_text( ctxt->current_value, ctxt->context->var[sub] );
}

void on_context_row_activated( GtkTreeView* view, GtkTreePath* tree_path,
                                        GtkTreeViewColumn* col, ContextData* ctxt )
{
    GtkTreeIter it;
    char* value;
    int sub, comp;

    GtkTreeModel* model = gtk_tree_view_get_model( GTK_TREE_VIEW( ctxt->view ) );
    if ( !gtk_tree_model_get_iter( model, &it, tree_path ) )
        return;
    gtk_tree_model_get( model, &it, 
                                    CONTEXT_COL_VALUE, &value,
                                    CONTEXT_COL_SUB, &sub,
                                    CONTEXT_COL_COMP, &comp,
                                    -1 );
    gtk_combo_box_set_active( GTK_COMBO_BOX( ctxt->box_sub ), sub );
    gtk_combo_box_set_active( GTK_COMBO_BOX( ctxt->box_comp ), comp );
    gtk_entry_set_text( GTK_ENTRY( gtk_bin_get_child( GTK_BIN( ctxt->box_value ) ) ), value );
    gtk_widget_grab_focus( ctxt->box_value );
    //enable_context( ctxt );
}

gboolean on_current_value_button_press( GtkWidget *widget,
                                    GdkEventButton *event,
                                    ContextData* ctxt )
{
    if ( event->type == GDK_2BUTTON_PRESS && event->button == 1 )
    {
        gtk_entry_set_text( GTK_ENTRY( 
                                gtk_bin_get_child( GTK_BIN( ctxt->box_value ) ) ),
                                gtk_label_get_text( ctxt->current_value ) );
        gtk_widget_grab_focus( ctxt->box_value );
        return TRUE;
    }
    return FALSE;
}

void on_context_entry_insert( GtkEntryBuffer *buf, guint position, gchar *chars,
                                            guint n_chars, gpointer user_data )
{   // remove linefeeds from pasted text
    if ( !strchr( gtk_entry_buffer_get_text( buf ), '\n' ) )
        return;

    char* new_text = replace_string( gtk_entry_buffer_get_text( buf ), "\n", "", FALSE );
    gtk_entry_buffer_set_text( buf, new_text, -1 );
    g_free( new_text );
}

gboolean on_context_selection_change( GtkTreeSelection* tree_sel,
                                      ContextData* ctxt )
{
    enable_context( ctxt );
    return FALSE;
}

static gboolean on_context_entry_keypress( GtkWidget *entry, GdkEventKey* event,
                                                            ContextData* ctxt )
{    
    if ( event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter )
    {
        if ( gtk_widget_get_sensitive( GTK_WIDGET( ctxt->btn_apply ) ) )
            on_context_button_press( GTK_WIDGET( ctxt->btn_apply ), ctxt );
        else
            on_context_button_press( GTK_WIDGET( ctxt->btn_add ), ctxt );
        return TRUE;
    }
    return FALSE;
}

void xset_context_dlg( XSetContext* context, XSet* set )
{
    GtkTreeViewColumn* col;
    GtkCellRenderer* renderer;
    int i;

    ContextData* ctxt = g_slice_new0( ContextData );
    ctxt->context = context;
    ctxt->parent = NULL;
    if ( set->browser )
        ctxt->parent = gtk_widget_get_toplevel( GTK_WIDGET( set->browser ) );
    else if ( set->desktop )
        ctxt->parent = gtk_widget_get_toplevel( GTK_WIDGET( set->desktop ) );

    // Dialog
    ctxt->dlg = gtk_dialog_new_with_buttons( _("Menu Item Properties"),
                                GTK_WINDOW( ctxt->parent ),
                                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                NULL, NULL );
    xset_set_window_icon( GTK_WINDOW( ctxt->dlg ) );
    gtk_window_set_role( GTK_WINDOW( ctxt->dlg ), "context_dialog" );

    int width = xset_get_int( "context_dlg", "x" );
    int height = xset_get_int( "context_dlg", "y" );
    if ( width && height )
        gtk_window_set_default_size( GTK_WINDOW( ctxt->dlg ), width, height );

    gtk_button_set_focus_on_click( GTK_BUTTON( gtk_dialog_add_button( 
                                                GTK_DIALOG( ctxt->dlg ),
                                                GTK_STOCK_HELP,
                                                GTK_RESPONSE_HELP ) ), FALSE );
    gtk_dialog_add_button( GTK_DIALOG( ctxt->dlg ), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL );
    ctxt->btn_ok = GTK_BUTTON( gtk_dialog_add_button( GTK_DIALOG( ctxt->dlg ),
                                                GTK_STOCK_OK, GTK_RESPONSE_OK ) );

    // Notebook
    ctxt->notebook = gtk_notebook_new();
    gtk_notebook_set_show_border( GTK_NOTEBOOK( ctxt->notebook ), TRUE );
    gtk_notebook_set_scrollable ( GTK_NOTEBOOK( ctxt->notebook ), TRUE );
    gtk_box_pack_start( GTK_BOX( gtk_dialog_get_content_area( 
                                                    GTK_DIALOG( ctxt->dlg ) ) ),
                                 ctxt->notebook, TRUE, TRUE, 0 );
    
    // Menu Item Page  =====================================================
    GtkWidget* align = gtk_alignment_new( 0, 0, 1, 1 );
    gtk_alignment_set_padding( GTK_ALIGNMENT( align ), 8, 0, 8, 8 );
    GtkWidget* vbox = gtk_vbox_new( FALSE, 4 );
    gtk_container_add ( GTK_CONTAINER ( align ), vbox );
    gtk_notebook_append_page( GTK_NOTEBOOK( ctxt->notebook ),
                              align,
                              gtk_label_new_with_mnemonic( _("_Menu Item") ) );
    
    GtkTable* table = GTK_TABLE( gtk_table_new( 9, 2, FALSE ) );
    gtk_container_set_border_width( GTK_CONTAINER ( table ), 0 );
    gtk_table_set_row_spacings( table, 6 );
    gtk_table_set_col_spacings( table, 8 );
    int row = 0;
    
    GtkWidget* label = gtk_label_new_with_mnemonic( _("Type:") );
    gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0.5 );
    gtk_table_attach( table, label, 0, 1, row, row + 1,
                                    GTK_FILL, GTK_SHRINK, 0, 0 );
    ctxt->item_type = gtk_combo_box_text_new();
    gtk_combo_box_set_focus_on_click( GTK_COMBO_BOX( ctxt->item_type ), FALSE );
    for ( i = 0; i < G_N_ELEMENTS( item_types ); i++ )
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( ctxt->item_type ),
                                                            _(item_types[i]) );
    //g_signal_connect( G_OBJECT( ctxt->item_type ), "changed",
    //                  G_CALLBACK( on_item_type_changed ), ctxt );
    align = gtk_alignment_new( 0, 0.5, 0.3 ,1 );
    gtk_container_add ( GTK_CONTAINER ( align ), GTK_WIDGET( ctxt->item_type ) );
    gtk_table_attach( table, align, 1, 2, row, row + 1,
                                    GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0 );
    
    label = gtk_label_new_with_mnemonic( _("Name:") );
    gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0.5 );
    row++;
    gtk_table_attach( table, label, 0, 1, row, row + 1,
                                    GTK_FILL, GTK_SHRINK, 0, 0 );
    ctxt->item_name = gtk_entry_new();
    gtk_table_attach( table, ctxt->item_name, 1, 2, row, row + 1,
                                    GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0 );
    
    label = gtk_label_new_with_mnemonic( _("Key:") );
    gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0.5 );
    row++;
    gtk_table_attach( table, label, 0, 1, row, row + 1,
                                    GTK_FILL, GTK_SHRINK, 0, 0 );
    ctxt->item_key = gtk_button_new_with_label( "Ctrl+Shift+K" );
    gtk_table_attach( table, ctxt->item_key, 1, 2, row, row + 1,
                                    GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0 );

    label = gtk_label_new_with_mnemonic( _("Icon:") );
    gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0.5 );
    row++;
    gtk_table_attach( table, label, 0, 1, row, row + 1, 
                                    GTK_FILL, GTK_SHRINK, 0, 0 );
    ctxt->item_icon = gtk_entry_new();
    gtk_table_attach( table, ctxt->item_icon, 1, 2, row, row + 1,
                                    GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0 );

    label = gtk_label_new_with_mnemonic( _("Target:") );
    gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0 );
    row++;
    gtk_table_attach( table, label, 0, 1, row, row + 1, 
                                    GTK_FILL, GTK_FILL, 0, 0 );

    ctxt->item_target_scroll = gtk_scrolled_window_new( NULL, NULL );
    ctxt->item_target = GTK_WIDGET( multi_input_new( 
                            GTK_SCROLLED_WINDOW( ctxt->item_target_scroll ),
                                                            NULL, FALSE ) );
    gtk_widget_set_size_request( GTK_WIDGET( ctxt->item_target ), -1, 100 );
    gtk_widget_set_size_request( GTK_WIDGET( ctxt->item_target_scroll ), -1, 100 );
    gtk_table_attach( table, ctxt->item_target_scroll, 1, 2, row, row + 3, 
                        GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0 );
    
    ctxt->item_browse = gtk_button_new_with_mnemonic( _("_Browse") );
    align = gtk_alignment_new( 1, 0.5, 0.2, 1 );
    gtk_container_add ( GTK_CONTAINER ( align ), ctxt->item_browse );
    row += 3;
    gtk_table_attach( table, align, 1, 2, row, row + 1,
                                    GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0 );

    gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( table ), FALSE, TRUE, 0 );

    // Context Page  =======================================================
    vbox = gtk_vbox_new( FALSE, 4 );
    gtk_notebook_append_page( GTK_NOTEBOOK( ctxt->notebook ),
                              vbox, gtk_label_new_with_mnemonic( _("Con_text") ) );

    GtkListStore* list = gtk_list_store_new( 4, G_TYPE_STRING, G_TYPE_INT, 
                                                G_TYPE_INT, G_TYPE_STRING );

    // Listview
    ctxt->view = exo_tree_view_new();
    gtk_tree_view_set_model( GTK_TREE_VIEW( ctxt->view ), GTK_TREE_MODEL( list ) );
    exo_tree_view_set_single_click( (ExoTreeView*)ctxt->view, TRUE );
    gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( ctxt->view ), FALSE );

    GtkWidget* scroll = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( scroll ),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER( scroll ), ctxt->view );    
    g_signal_connect( G_OBJECT( ctxt->view ), "row-activated",
                          G_CALLBACK( on_context_row_activated ), ctxt );
    g_signal_connect( G_OBJECT( gtk_tree_view_get_selection( 
                            GTK_TREE_VIEW( ctxt->view ) ) ),
                            "changed",
                            G_CALLBACK( on_context_selection_change ), ctxt );

    // col display
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_sizing( col, GTK_TREE_VIEW_COLUMN_AUTOSIZE );
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start( col, renderer, TRUE );
    gtk_tree_view_column_set_attributes( col, renderer,
                                         "text", CONTEXT_COL_DISP, NULL );
    gtk_tree_view_append_column ( GTK_TREE_VIEW( ctxt->view ), col );
    gtk_tree_view_column_set_expand ( col, TRUE );

    // list buttons
    ctxt->btn_remove = GTK_BUTTON( gtk_button_new_with_mnemonic( _("_Remove") ) );
    gtk_button_set_image( ctxt->btn_remove, xset_get_image( "GTK_STOCK_REMOVE",
                                                        GTK_ICON_SIZE_BUTTON ) );
    gtk_button_set_focus_on_click( ctxt->btn_remove, FALSE );
    g_signal_connect( G_OBJECT( ctxt->btn_remove ), "clicked",
                          G_CALLBACK( on_context_button_press ), ctxt );

    ctxt->btn_add = GTK_BUTTON( gtk_button_new_with_mnemonic( _("_Add") ) );
    gtk_button_set_image( ctxt->btn_add, xset_get_image( "GTK_STOCK_ADD",
                                                        GTK_ICON_SIZE_BUTTON ) );
    gtk_button_set_focus_on_click( ctxt->btn_add, FALSE );
    g_signal_connect( G_OBJECT( ctxt->btn_add ), "clicked",
                          G_CALLBACK( on_context_button_press ), ctxt );

    ctxt->btn_apply = GTK_BUTTON( gtk_button_new_with_mnemonic( _("A_pply") ) );
    gtk_button_set_image( ctxt->btn_apply, xset_get_image( "GTK_STOCK_APPLY",
                                                        GTK_ICON_SIZE_BUTTON ) );
    gtk_button_set_focus_on_click( ctxt->btn_apply, FALSE );
    g_signal_connect( G_OBJECT( ctxt->btn_apply ), "clicked",
                          G_CALLBACK( on_context_button_press ), ctxt );

    // boxes
    ctxt->box_sub = gtk_combo_box_text_new();
    gtk_combo_box_set_focus_on_click( GTK_COMBO_BOX( ctxt->box_sub ), FALSE );
    for ( i = 0; i < G_N_ELEMENTS( context_sub ); i++ )
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( ctxt->box_sub ), _(context_sub[i]) );
    g_signal_connect( G_OBJECT( ctxt->box_sub ), "changed",
                      G_CALLBACK( on_context_sub_changed ), ctxt );

    ctxt->box_comp = gtk_combo_box_text_new();
    gtk_combo_box_set_focus_on_click( GTK_COMBO_BOX( ctxt->box_comp ), FALSE );
    for ( i = 0; i < G_N_ELEMENTS( context_comp ); i++ )
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( ctxt->box_comp ),
                                                            _(context_comp[i]) );
    
    ctxt->box_value = gtk_combo_box_text_new_with_entry();
    gtk_combo_box_set_focus_on_click( GTK_COMBO_BOX( ctxt->box_value ), FALSE );
#if GTK_CHECK_VERSION (3, 0, 0)
    // see https://github.com/IgnorantGuru/spacefm/issues/43
    // this seems to have no effect
    gtk_combo_box_set_popup_fixed_width( GTK_COMBO_BOX( ctxt->box_value ), TRUE );
#endif

    ctxt->box_match = gtk_combo_box_text_new();
    gtk_combo_box_set_focus_on_click( GTK_COMBO_BOX( ctxt->box_match ), FALSE );
    gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( ctxt->box_match ),
                                                    _("matches any rule:") );
    gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( ctxt->box_match ),
                                                    _("matches all rules:") );
    gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( ctxt->box_match ),
                                                    _("doesn't match any rule:") );
    gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( ctxt->box_match ),
                                                    _("doesn't match all rules:") );
    g_signal_connect( G_OBJECT( ctxt->box_match ), "changed",
                      G_CALLBACK( on_context_action_changed ), ctxt );

    ctxt->box_action = gtk_combo_box_text_new();
    gtk_combo_box_set_focus_on_click( GTK_COMBO_BOX( ctxt->box_action ), FALSE );
    gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( ctxt->box_action ),
                                                    _("Show") );
    gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( ctxt->box_action ),
                                                    _("Enable") );
    gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( ctxt->box_action ), 
                                                    _("Hide") );
    gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( ctxt->box_action ),
                                                    _("Disable") );
    g_signal_connect( G_OBJECT( ctxt->box_action ), "changed",
                      G_CALLBACK( on_context_action_changed ), ctxt );

    ctxt->current_value = GTK_LABEL( gtk_label_new( NULL ) );
    gtk_label_set_ellipsize( ctxt->current_value, PANGO_ELLIPSIZE_MIDDLE );
    gtk_label_set_selectable( ctxt->current_value, TRUE );
    gtk_misc_set_alignment( GTK_MISC( ctxt->current_value ), 0, 0 );
    g_signal_connect( G_OBJECT( ctxt->current_value ), "button-press-event",
                          G_CALLBACK( on_current_value_button_press ), ctxt );
    g_signal_connect_after( G_OBJECT( gtk_entry_get_buffer( GTK_ENTRY( 
                                    gtk_bin_get_child( 
                                    GTK_BIN( ctxt->box_value ) ) ) ) ),
                                    "inserted-text",
                                    G_CALLBACK( on_context_entry_insert ), NULL );
    g_signal_connect( G_OBJECT( GTK_ENTRY( 
                                    gtk_bin_get_child( GTK_BIN( ctxt->box_value  ) ) ) ),
                                    "key-press-event",
                                    G_CALLBACK( on_context_entry_keypress ), ctxt );

    ctxt->test = GTK_LABEL( gtk_label_new( NULL ) );

    //PACK
    gtk_container_set_border_width( GTK_CONTAINER ( ctxt->dlg ), 10 );

    ctxt->hbox_match = gtk_hbox_new( FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( ctxt->hbox_match ),
                        GTK_WIDGET( ctxt->box_action ), FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( ctxt->hbox_match ),
                        GTK_WIDGET( gtk_label_new( _("item if context") ) ), FALSE,
                                                                        TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( ctxt->hbox_match ),
                        GTK_WIDGET( ctxt->box_match ), FALSE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( vbox ),
                        GTK_WIDGET( ctxt->hbox_match ), FALSE, TRUE, 4 );

//    GtkLabel* label = gtk_label_new( "Rules:" );
//    gtk_misc_set_alignment( label, 0, 1 );
//    gtk_box_pack_start( GTK_BOX( GTK_DIALOG( ctxt->dlg )->vbox ),
//                        GTK_WIDGET( label ), FALSE, TRUE, 8 );
    gtk_box_pack_start( GTK_BOX( vbox ),
                        GTK_WIDGET( scroll ), TRUE, TRUE, 4 );

    GtkWidget* hbox_btns = gtk_hbox_new( FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox_btns ),
                        GTK_WIDGET( ctxt->btn_remove ), FALSE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox_btns ),
                        GTK_WIDGET( gtk_vseparator_new() ), FALSE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox_btns ),
                        GTK_WIDGET( ctxt->btn_add ), FALSE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox_btns ),
                        GTK_WIDGET( ctxt->btn_apply ), FALSE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox_btns ),
                        GTK_WIDGET( ctxt->test ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( vbox ),
                        GTK_WIDGET( hbox_btns ), FALSE, TRUE, 4 );
                        
    ctxt->frame = GTK_FRAME( gtk_frame_new( _("Edit Rule") ) );
    GtkWidget* vbox_frame = gtk_vbox_new( FALSE, 4 );
    gtk_container_add ( GTK_CONTAINER ( ctxt->frame ), vbox_frame );
    GtkWidget* hbox_frame = gtk_hbox_new( FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox_frame ),
                        GTK_WIDGET( ctxt->box_sub ), FALSE, TRUE, 8 );
    gtk_box_pack_start( GTK_BOX( hbox_frame ),
                        GTK_WIDGET( ctxt->box_comp ), FALSE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( vbox_frame ),
                        GTK_WIDGET( hbox_frame ), FALSE, TRUE, 4 );
    GtkWidget* hbox = gtk_hbox_new( FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( ctxt->box_value ), TRUE, TRUE, 8 );
    gtk_box_pack_start( GTK_BOX( vbox_frame ),
                        GTK_WIDGET( hbox ), TRUE, TRUE, 4 );
    hbox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( gtk_label_new( _("Value:") ) ), FALSE, TRUE, 8 );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( ctxt->current_value ), TRUE, TRUE, 2 );    
    gtk_box_pack_start( GTK_BOX( vbox_frame ),
                        GTK_WIDGET( hbox ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( vbox ),
                        GTK_WIDGET( ctxt->frame ), FALSE, TRUE, 16 );
    
    // plugin?
    XSet* mset = xset_get_plugin_mirror( set );

    // set match / action
    char* elements = mset->context;
    char* action = get_element_next( &elements );
    char* match = get_element_next( &elements );
    if ( match && action )
    {
        i = atoi( match );
        if ( i < 0 || i > 3 )
            i = 0;
        gtk_combo_box_set_active( GTK_COMBO_BOX( ctxt->box_match ), i );
        i = atoi( action );
        if ( i < 0 || i > 3 )
            i = 0;
        gtk_combo_box_set_active( GTK_COMBO_BOX( ctxt->box_action ), i );
        g_free( match );
        g_free( action );
    }
    else
    {
        gtk_combo_box_set_active( GTK_COMBO_BOX( ctxt->box_match ), 0 );
        gtk_combo_box_set_active( GTK_COMBO_BOX( ctxt->box_action ), 0 );        
        if ( match )
            g_free( match );
        if ( action )
            g_free( action );
    }
    // set rules
    int sub, comp;
    char* value;
    char* disp;
    GtkTreeIter it;
    gboolean is_rules = FALSE;
    while ( get_rule_next( &elements, &sub, &comp, &value ) )
    {
        disp = context_display( sub, comp, value );
        gtk_list_store_append( GTK_LIST_STORE( list ), &it );
        gtk_list_store_set( GTK_LIST_STORE( list ), &it,
                                            CONTEXT_COL_DISP, disp,
                                            CONTEXT_COL_SUB, sub,
                                            CONTEXT_COL_COMP, comp,
                                            CONTEXT_COL_VALUE, value,
                                            -1 );
        g_free( disp );
        if ( value )
            g_free( value );
        is_rules = TRUE;
    }
    gtk_combo_box_set_active( GTK_COMBO_BOX( ctxt->box_sub ), 0 );
    gtk_widget_set_sensitive( GTK_WIDGET( ctxt->btn_ok ), is_rules );
    
    // Command Page  =====================================================
    align = gtk_alignment_new( 0, 0, 1, 1 );
    gtk_alignment_set_padding( GTK_ALIGNMENT( align ), 8, 0, 8, 8 );
    vbox = gtk_vbox_new( FALSE, 4 );
    gtk_container_add ( GTK_CONTAINER ( align ), vbox );
    gtk_notebook_append_page( GTK_NOTEBOOK( ctxt->notebook ),
                              align,
                              gtk_label_new_with_mnemonic( _("Comm_and") ) );

    hbox = gtk_hbox_new( FALSE, 8 );
    ctxt->cmd_opt_line = gtk_radio_button_new_with_mnemonic( NULL,
                            _("Command _Line") );
    ctxt->cmd_opt_cmd = gtk_radio_button_new_with_mnemonic_from_widget(
                            GTK_RADIO_BUTTON( ctxt->cmd_opt_line ), _("_Script") );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( ctxt->cmd_opt_line ), FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( ctxt->cmd_opt_cmd ), FALSE, TRUE, 0 );
    ctxt->cmd_edit = gtk_button_new_with_mnemonic( _("Open In _Editor") );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( ctxt->cmd_edit ), FALSE, TRUE, 24 );
    gtk_box_pack_start( GTK_BOX( vbox ),
                        GTK_WIDGET( hbox ), FALSE, TRUE, 8 );

    // Line
    ctxt->cmd_line_label = gtk_label_new( _(enter_command_use) );
    gtk_misc_set_alignment( GTK_MISC ( ctxt->cmd_line_label ), 0, 0 );
    gtk_box_pack_start( GTK_BOX( vbox ),
                        GTK_WIDGET( ctxt->cmd_line_label ), FALSE, TRUE, 8 );

    // Script
    ctxt->cmd_scroll_script = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( ctxt->cmd_scroll_script ),
                                     GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    ctxt->cmd_script = GTK_WIDGET( gtk_text_view_new() );
    // ubuntu shows input too small so use mininum height
    gtk_widget_set_size_request( GTK_WIDGET( ctxt->cmd_script ), -1, 50 );
    gtk_widget_set_size_request( GTK_WIDGET( ctxt->cmd_scroll_script ), -1, 50 );
    gtk_text_view_set_wrap_mode( GTK_TEXT_VIEW( ctxt->cmd_script ),
                                            GTK_WRAP_WORD_CHAR );
    gtk_container_add ( GTK_CONTAINER ( ctxt->cmd_scroll_script ),  
                                            GTK_WIDGET( ctxt->cmd_script ) );
    gtk_box_pack_start( GTK_BOX( vbox ),
                        GTK_WIDGET( ctxt->cmd_scroll_script ), TRUE, TRUE, 0 );

    // Option Page  =====================================================
    align = gtk_alignment_new( 0, 0, 1, 1 );
    gtk_alignment_set_padding( GTK_ALIGNMENT( align ), 8, 0, 8, 8 );
    vbox = gtk_vbox_new( FALSE, 4 );
    gtk_container_add ( GTK_CONTAINER ( align ), vbox );
    gtk_notebook_append_page( GTK_NOTEBOOK( ctxt->notebook ),
                              align,
                              gtk_label_new_with_mnemonic( _("Optio_ns") ) );

    GtkWidget* frame = gtk_frame_new( _("Run Options") );
    align = gtk_alignment_new( 0, 0, 1, 1 );
    gtk_alignment_set_padding( GTK_ALIGNMENT( align ), 8, 0, 8, 8 );
    vbox_frame = gtk_vbox_new( FALSE, 8 );
    gtk_container_add ( GTK_CONTAINER ( align ), vbox_frame );
    gtk_container_add ( GTK_CONTAINER ( frame ), align );
    gtk_box_pack_start( GTK_BOX( vbox ),
                        GTK_WIDGET( frame ), FALSE, TRUE, 8 );

    hbox = gtk_hbox_new( FALSE, 0 );
    ctxt->opt_terminal = gtk_check_button_new_with_mnemonic( _("Run In Terminal") );
    ctxt->opt_keep_term = gtk_check_button_new_with_mnemonic( _("Keep Terminal Open") );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( ctxt->opt_terminal ), FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( ctxt->opt_keep_term ), FALSE, TRUE, 8 );
    gtk_box_pack_start( GTK_BOX( vbox_frame ),
                        GTK_WIDGET( hbox ), FALSE, TRUE, 8 );
    hbox = gtk_hbox_new( FALSE, 0 );
    label = gtk_label_new( _("Run As User:") );
    gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0.5 );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( label ), FALSE, TRUE, 0 );
    ctxt->cmd_user = gtk_entry_new();
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( ctxt->cmd_user ), FALSE, TRUE, 8 );
    label = gtk_label_new( _("( leave blank for current user )") );
    gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0.5 );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( label ), FALSE, TRUE, 8 );
    gtk_box_pack_start( GTK_BOX( vbox_frame ),
                        GTK_WIDGET( hbox ), FALSE, TRUE, 0 );
    ctxt->opt_task = gtk_check_button_new_with_mnemonic( _("Run As Task") );
    gtk_box_pack_start( GTK_BOX( vbox_frame ),
                        GTK_WIDGET( ctxt->opt_task ), FALSE, TRUE, 0 );
    ctxt->opt_hbox_task = gtk_hbox_new( FALSE, 8 );
    ctxt->opt_task_err = gtk_check_button_new_with_mnemonic( _("Popup Error") );
    ctxt->opt_task_out = gtk_check_button_new_with_mnemonic( _("Popup Output") );
    ctxt->opt_scroll = gtk_check_button_new_with_mnemonic( _("Scroll Output") );
    gtk_box_pack_start( GTK_BOX( ctxt->opt_hbox_task ),
                        GTK_WIDGET( ctxt->opt_task_err ), FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( ctxt->opt_hbox_task ),
                        GTK_WIDGET( ctxt->opt_task_out ), FALSE, TRUE, 8 );
    gtk_box_pack_start( GTK_BOX( ctxt->opt_hbox_task ),
                        GTK_WIDGET( ctxt->opt_scroll ), FALSE, TRUE, 8 );
    gtk_box_pack_start( GTK_BOX( vbox_frame ),
                        GTK_WIDGET( ctxt->opt_hbox_task ), FALSE, TRUE, 8 );

    frame = gtk_frame_new( _("Style") );
    align = gtk_alignment_new( 0, 0, 1, 1 );
    gtk_alignment_set_padding( GTK_ALIGNMENT( align ), 8, 0, 8, 8 );
    vbox_frame = gtk_vbox_new( FALSE, 8 );
    gtk_container_add ( GTK_CONTAINER ( align ), vbox_frame );
    gtk_container_add ( GTK_CONTAINER ( frame ), align );
    gtk_box_pack_start( GTK_BOX( vbox ),
                        GTK_WIDGET( frame ), TRUE, TRUE, 8 );

    hbox = gtk_hbox_new( FALSE, 8 );
    ctxt->cmd_opt_normal = gtk_radio_button_new_with_mnemonic( NULL,
                            _("Normal") );
    ctxt->cmd_opt_checkbox = gtk_radio_button_new_with_mnemonic_from_widget(
                            GTK_RADIO_BUTTON( ctxt->cmd_opt_normal ),
                            _("Checkbox") );
    ctxt->cmd_opt_confirm = gtk_radio_button_new_with_mnemonic_from_widget(
                            GTK_RADIO_BUTTON( ctxt->cmd_opt_normal ),
                            _("Confirmation") );
    ctxt->cmd_opt_input = gtk_radio_button_new_with_mnemonic_from_widget(
                            GTK_RADIO_BUTTON( ctxt->cmd_opt_normal ),
                            _("Input") );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( ctxt->cmd_opt_normal ), FALSE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( ctxt->cmd_opt_checkbox ), FALSE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( ctxt->cmd_opt_confirm ), FALSE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( ctxt->cmd_opt_input ), FALSE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( vbox_frame ),
                        GTK_WIDGET( hbox ), FALSE, TRUE, 0 );

    // message box
    ctxt->cmd_vbox_msg = gtk_vbox_new( FALSE, 4 );
    label = gtk_label_new( _("Confirmation/Input Message:") );
    gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0 );
    gtk_box_pack_start( GTK_BOX( ctxt->cmd_vbox_msg ),
                        GTK_WIDGET( label ), FALSE, TRUE, 8 );
    ctxt->cmd_scroll_msg = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( ctxt->cmd_scroll_msg ),
                                     GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    ctxt->cmd_msg = GTK_WIDGET( gtk_text_view_new() );
    // ubuntu shows input too small so use mininum height
    gtk_widget_set_size_request( GTK_WIDGET( ctxt->cmd_msg ), -1, 50 );
    gtk_widget_set_size_request( GTK_WIDGET( ctxt->cmd_scroll_msg ), -1, 50 );
    gtk_text_view_set_wrap_mode( GTK_TEXT_VIEW( ctxt->cmd_msg ),
                                            GTK_WRAP_WORD_CHAR );
    gtk_container_add ( GTK_CONTAINER ( ctxt->cmd_scroll_msg ),  
                                            GTK_WIDGET( ctxt->cmd_msg ) );
    gtk_box_pack_start( GTK_BOX( ctxt->cmd_vbox_msg ),
                        GTK_WIDGET( ctxt->cmd_scroll_msg ), TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( vbox_frame ),
                        GTK_WIDGET( ctxt->cmd_vbox_msg ), TRUE, TRUE, 0 );


    // run
    gtk_widget_show_all( GTK_WIDGET( ctxt->dlg ) );
    enable_context( ctxt );
    int response;
    while ( response = gtk_dialog_run( GTK_DIALOG( ctxt->dlg ) ) )
    {
        if ( response == GTK_RESPONSE_OK )
        {
            if ( mset->context )
                g_free( mset->context );
            mset->context = context_build( ctxt );
            break;
        }
        else if ( response == GTK_RESPONSE_HELP )
            xset_show_help( ctxt->dlg, NULL, "#designmode-style-context" );
        else
            break;
    }

    GtkAllocation allocation;
    gtk_widget_get_allocation (GTK_WIDGET(ctxt->dlg), &allocation);
    width = allocation.width;
    height = allocation.height;
    if ( width && height )
    {
        char* str = g_strdup_printf( "%d", width );
        xset_set( "context_dlg", "x", str );
        g_free( str );
        str = g_strdup_printf( "%d", height );
        xset_set( "context_dlg", "y", str );
        g_free( str );
    }

    gtk_widget_destroy( ctxt->dlg );
    g_slice_free( ContextData, ctxt );
}

