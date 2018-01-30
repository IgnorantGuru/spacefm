#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

//#include <string.h>
#include <gtk/gtk.h>
#include "exo-tree-view.h"
#include "gtk2-compat.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fnmatch.h>

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "item-prop.h"
#include "ptk-app-chooser.h"
#include "main-window.h"

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
    XSet* set;
    char* temp_cmd_line;
    struct stat64 script_stat;
    gboolean script_stat_valid;
    gboolean reset_command;
    
    // Menu Item Page
    GtkWidget* item_type;
    GtkWidget* item_name;
    GtkWidget* item_key;
    GtkWidget* item_icon;
    GtkWidget* target_vbox;
    GtkWidget* target_label;
    GtkWidget* item_target;
    GtkWidget* item_choose;
    GtkWidget* item_browse;
    GtkWidget* icon_choose_btn;
    
    // Context Page
    GtkWidget* vbox_context;
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
    GtkWidget* ignore_context;
    GtkWidget* hbox_opener;
    GtkWidget* opener;
    
    // Command Page
    GtkWidget* cmd_opt_line;
    GtkWidget* cmd_opt_script;
    GtkWidget* cmd_edit;
    GtkWidget* cmd_edit_root;
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
    GtkWidget* opt_task_pop;
    GtkWidget* opt_task_err;
    GtkWidget* opt_task_out;
    GtkWidget* opt_scroll;
    GtkWidget* opt_hbox_task;
    GtkWidget* open_browser;
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
    CONTEXT_COMP_GREATER,
    CONTEXT_COMP_MATCH,
    CONTEXT_COMP_NMATCH
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
    N_("is greater than"),
    N_("matches"),
    N_("doesn't match")
};

static const char* item_types[] = 
{
    N_("Bookmark"),
    N_("Application"),
    N_("Command"),
};

enum {
    ITEM_TYPE_BOOKMARK,
    ITEM_TYPE_APP,
    ITEM_TYPE_COMMAND
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
            case CONTEXT_COMP_MATCH:
            case CONTEXT_COMP_NMATCH:
                s = g_utf8_strdown( eleval, -1 );
                if ( g_strcmp0( eleval, s ) )
                {
                    // pattern contains uppercase chars - test case sensitive
                    test = fnmatch( eleval, context->var[sub], 0 );
                }
                else
                {
                    // case insensitive
                    char* str = g_utf8_strdown( context->var[sub], -1 );
                    test = fnmatch( s, str, 0 );
                    g_free( str );
                }
                g_free( s );
                if ( comp == CONTEXT_COMP_MATCH )
                    test = !test;
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
    //gtk_widget_set_sensitive( GTK_WIDGET( ctxt->hbox_match ),
    //                    gtk_tree_model_get_iter_first( 
    //                    gtk_tree_view_get_model( GTK_TREE_VIEW( ctxt->view ) ), &it ) );
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
            else if ( action == CONTEXT_SHOW && gtk_combo_box_get_active(
                                GTK_COMBO_BOX( ctxt->box_action ) ) ==
                                                            CONTEXT_DISABLE )
                text = _("Current: Enable");
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

void enable_options( ContextData* ctxt )
{
    gtk_widget_set_sensitive( ctxt->opt_keep_term, 
                    gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
                                                    ctxt->opt_terminal ) ) );
    gboolean as_task = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
                                                    ctxt->opt_task ) );
    gtk_widget_set_sensitive( ctxt->opt_task_pop, as_task );
    gtk_widget_set_sensitive( ctxt->opt_task_err, as_task );
    gtk_widget_set_sensitive( ctxt->opt_task_out, as_task );
    gtk_widget_set_sensitive( ctxt->opt_scroll, as_task );

    gtk_widget_set_sensitive( ctxt->cmd_vbox_msg, 
                    gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( 
                                                ctxt->cmd_opt_confirm ) ) ||
                    gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( 
                                                ctxt->cmd_opt_input ) ) );
    gtk_widget_set_sensitive( ctxt->item_icon,
                    !gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( 
                                                ctxt->cmd_opt_checkbox ) )
                    && ctxt->set->menu_style != XSET_MENU_SEP
                    && ctxt->set->menu_style != XSET_MENU_SUBMENU );
    
    if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( 
                                                ctxt->cmd_opt_confirm ) ) )
    {
        // add default msg
        GtkTextBuffer* buf = gtk_text_view_get_buffer( GTK_TEXT_VIEW(
                                                        ctxt->cmd_msg ) );
        if ( gtk_text_buffer_get_char_count( buf ) == 0 )
            gtk_text_buffer_set_text( buf, _("Are you sure?"), -1 );
    }
    else if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( 
                                                ctxt->cmd_opt_input ) ) )
    {
        // remove default msg
        GtkTextIter iter, siter;
        GtkTextBuffer* buf = gtk_text_view_get_buffer( GTK_TEXT_VIEW(
                                                        ctxt->cmd_msg ) );
        gtk_text_buffer_get_start_iter( buf, &siter );
        gtk_text_buffer_get_end_iter( buf, &iter );
        char* text = gtk_text_buffer_get_text( buf, &siter, &iter, FALSE );
        if ( text && !strcmp( text, _("Are you sure?") ) )
            gtk_text_buffer_set_text( buf, "", -1 );
        g_free( text );
    }
}

gboolean is_command_script_newer( ContextData* ctxt )
{
    struct stat64 statbuf;

    if ( !ctxt->script_stat_valid )
        return FALSE;
    char* script = xset_custom_get_script( ctxt->set, FALSE );
    if ( script && stat64( script, &statbuf ) == 0 )
    {
        if ( statbuf.st_mtime != ctxt->script_stat.st_mtime ||
             statbuf.st_size != ctxt->script_stat.st_size )
            return TRUE;
    }
    return FALSE;    
}

void command_script_stat( ContextData* ctxt )
{
    char* script = xset_custom_get_script( ctxt->set, FALSE );
    if ( script && stat64( script, &ctxt->script_stat ) == 0 )
        ctxt->script_stat_valid = TRUE;
    else
        ctxt->script_stat_valid = FALSE;
    g_free( script );
}

void load_text_view( GtkTextView* view, const char* line )
{
    GtkTextBuffer* buf = gtk_text_view_get_buffer( view );
    if ( !line )
    {
        gtk_text_buffer_set_text( buf, "", -1 );
        return;
    }
    char* lines = replace_string( line, "\\n", "\n", FALSE );
    char* tabs = replace_string( lines, "\\t", "\t", FALSE );
    gtk_text_buffer_set_text( buf, tabs, -1 );
    g_free( lines );
    g_free( tabs );
}

char* get_text_view( GtkTextView* view )
{
    GtkTextBuffer* buf = gtk_text_view_get_buffer( view );
    GtkTextIter iter, siter;
    gtk_text_buffer_get_start_iter( buf, &siter );
    gtk_text_buffer_get_end_iter( buf, &iter );
    char* text = gtk_text_buffer_get_text( buf, &siter, &iter, FALSE );
    if ( !( text && text[0] ) )
    {
        g_free( text );
        return NULL;
    }
    char* lines = replace_string( text, "\n", "\\n", FALSE );
    char* tabs = replace_string( lines, "\t", "\\t", FALSE );
    g_free( text );
    g_free( lines );
    return tabs;
}

void load_command_script( ContextData* ctxt, XSet* set )
{
    gboolean modified = FALSE;
    FILE* file = 0;
    GtkTextBuffer* buf = gtk_text_view_get_buffer( GTK_TEXT_VIEW( 
                                                ctxt->cmd_script ) );
    char* script = xset_custom_get_script( set, !set->plugin );
    if ( !script )
        gtk_text_buffer_set_text( buf, "", -1 );
    else
    {
        char line[ 4096 ];
    
        gtk_text_buffer_set_text( buf, "", -1 );
        file = fopen( script, "r" );
        if ( !file )
            g_warning( _("error reading file %s: %s"), script,
                                                g_strerror( errno ) );
        else
        {
            // read file one line at a time to prevent splitting UTF-8 characters
            while ( fgets( line, sizeof( line ), file ) )
            {
                if ( !g_utf8_validate( line, -1, NULL ) )
                {
                    fclose( file );
                    gtk_text_buffer_set_text( buf, "", -1 );
                    modified = TRUE;
                    g_warning( _("file '%s' contents are not valid UTF-8"),
                                                            script );
                    break;
                }
                gtk_text_buffer_insert_at_cursor( buf, line, -1 );
            }
            fclose( file );
        }
    }
    gboolean have_access = script && have_rw_access( script );
    gtk_text_view_set_editable( GTK_TEXT_VIEW( ctxt->cmd_script ),
                    !set->plugin && ( !file || have_access ) );
    gtk_text_buffer_set_modified( buf, modified );
    command_script_stat( ctxt );
    g_free( script );
    if ( have_access && geteuid() != 0 )
        gtk_widget_hide( ctxt->cmd_edit_root );
    else
        gtk_widget_show( ctxt->cmd_edit_root );
}

void save_command_script( ContextData* ctxt, gboolean query )
{
    GtkTextBuffer* buf = gtk_text_view_get_buffer( GTK_TEXT_VIEW( 
                                                ctxt->cmd_script ) );
    if ( !gtk_text_buffer_get_modified( buf ) )
        return;
    if ( query && xset_msg_dialog( ctxt->dlg, GTK_MESSAGE_QUESTION,
                                   _("Save Modified Script?"), NULL,
                                   GTK_BUTTONS_YES_NO,
                                   _("Save your changes to the command script?"),
                                   NULL, NULL ) == GTK_RESPONSE_NO )
        return;
    if ( is_command_script_newer( ctxt ) && 
                            xset_msg_dialog( ctxt->dlg, GTK_MESSAGE_QUESTION,
                                   _("Overwrite Script?"), NULL,
                                   GTK_BUTTONS_YES_NO,
                                   _("The command script on disk has changed.\n\nDo you want to overwrite it?"),
                                   NULL, NULL ) == GTK_RESPONSE_NO )
        return;

    char* script = xset_custom_get_script( ctxt->set, FALSE );
    if ( !script )
        return;
    
    GtkTextIter iter, siter;
    gtk_text_buffer_get_start_iter( buf, &siter );
    gtk_text_buffer_get_end_iter( buf, &iter );
    char* text = gtk_text_buffer_get_text( buf, &siter, &iter, FALSE );
    FILE* file = fopen( script, "w" );
    if ( file )
    {
        fputs( text, file );
        fclose( file );
    }
    g_free( text );
}

void on_script_toggled( GtkWidget* item, ContextData* ctxt )
{
    if ( !gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( item ) ) )
        return;
    if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
                                                    ctxt->cmd_opt_line ) ) )
    {
        // set to command line
        save_command_script( ctxt, TRUE );
        gtk_widget_show( ctxt->cmd_line_label );
        gtk_widget_show( ctxt->cmd_edit_root );
        load_text_view( GTK_TEXT_VIEW( ctxt->cmd_script ),
                                                    ctxt->temp_cmd_line );
    }
    else
    {
        // set to script
        gtk_widget_hide( ctxt->cmd_line_label );
        g_free( ctxt->temp_cmd_line );
        ctxt->temp_cmd_line = get_text_view( GTK_TEXT_VIEW(
                                                        ctxt->cmd_script ) );
        load_command_script( ctxt, ctxt->set );
        
#if GTK_CHECK_VERSION(2, 24, 0)
        // update Open In Browser file count - cosmetic only
        // should probably rebuild entire list on click to avoid gtk 2.24 dep
        char* path;
        if ( ctxt->set->plugin )
            path = g_build_filename( ctxt->set->plug_dir, ctxt->set->plug_name,
                                                                NULL );
        else
            path = g_build_filename( xset_get_config_dir(), "scripts",
                                                        ctxt->set->name, NULL );
        char* str = g_strdup_printf( "%s  $fm_cmd_dir  %s", _("Command Dir"), 
                                dir_has_files( path ) ? "" : _("(no files)") );
        gtk_combo_box_text_remove( GTK_COMBO_BOX_TEXT( ctxt->open_browser ), 0);
        gtk_combo_box_text_insert_text( GTK_COMBO_BOX_TEXT( ctxt->open_browser ),
                                                                    0, str );
        g_free( str );
        g_free( path );
#endif
    }
    GtkTextBuffer* buf = gtk_text_view_get_buffer( GTK_TEXT_VIEW( 
                                                    ctxt->cmd_script ) );
    GtkTextIter siter;
    gtk_text_buffer_get_start_iter( buf, &siter );
    gtk_text_buffer_place_cursor( buf, &siter );
    gtk_widget_grab_focus( ctxt->cmd_script );
}

void on_cmd_opt_toggled( GtkWidget* item, ContextData* ctxt )
{
    enable_options( ctxt );
    if ( ( item == ctxt->cmd_opt_confirm || item == ctxt->cmd_opt_input ) &&
                    gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( 
                                                item ) ) )
        gtk_widget_grab_focus( ctxt->cmd_msg );
    else if ( item == ctxt->opt_terminal &&
                    gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( item ) ) )
        // checking run in terminal unchecks run as task
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( ctxt->opt_task ),
                                                                FALSE );
}

void on_ignore_context_toggled( GtkWidget* item, ContextData* ctxt )
{
    gtk_widget_set_sensitive( ctxt->vbox_context,
                !gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( item ) ) );    
}

void on_edit_button_press( GtkWidget* btn, ContextData* ctxt )
{
    char* path;
    if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
                                                    ctxt->cmd_opt_line ) ) )
    {
        // set to command line - get path of first argument
        GtkTextBuffer* buf = gtk_text_view_get_buffer( GTK_TEXT_VIEW( 
                                                    ctxt->cmd_script ) );
        GtkTextIter iter, siter;
        gtk_text_buffer_get_start_iter( buf, &siter );
        gtk_text_buffer_get_end_iter( buf, &iter );
        char* text = gtk_text_buffer_get_text( buf, &siter, &iter, FALSE );
        if ( !( text && text[0] ) )
            path = NULL;
        else
        {
            char* str;
            if ( str = strchr( text, ' ' ) )
                str[0] = '\0';
            if ( str = strchr( text, '\n' ) )
                str[0] = '\0';
            path = g_strdup( g_strstrip( text ) );
            if ( path[0] == '\0' || ( path[0] != '/' &&
                                                !g_ascii_isalnum( path[0] ) ) )
            {
                g_free( path );
                path = NULL;
            }
            else if ( path[0] != '/' )
            {
                str = path;
                path = g_find_program_in_path( str );
                g_free( str );
            }
        }
        g_free( text );
        if ( !( path && mime_type_is_text_file( path, NULL ) ) )
        {
            xset_msg_dialog( GTK_WIDGET( ctxt->dlg ), GTK_MESSAGE_ERROR,
                                _("Error"), NULL, 0, 
                                _("The command line does not begin with a text file (script) to be opened, or the script was not found in your $PATH."), NULL,
                                NULL );
            g_free( path );
            return;
        }
    }
    else
    {
        // set to script
        save_command_script( ctxt, FALSE );
        path = xset_custom_get_script( ctxt->set, !ctxt->set->plugin );
    }
    if ( path && mime_type_is_text_file( path, NULL ) )
    {
        xset_edit( ctxt->dlg, path, btn == ctxt->cmd_edit_root,
                                    btn != ctxt->cmd_edit_root );
    }
    g_free( path );
}

void on_open_browser( GtkComboBox* box, ContextData* ctxt )
{
    char* folder = NULL;
    char* script;
    int job = gtk_combo_box_get_active( GTK_COMBO_BOX( box ) );
    gtk_combo_box_set_active( GTK_COMBO_BOX( box ), -1 );
    if ( job == 0 )
    {
        // Command Dir
        if ( ctxt->set->plugin )
        {
            folder = g_build_filename( ctxt->set->plug_dir, "files", NULL );
            if ( !g_file_test( folder, G_FILE_TEST_EXISTS ) )
            {
                g_free( folder );
                folder = g_build_filename( ctxt->set->plug_dir,
                                                ctxt->set->plug_name, NULL );
            }
        }
        else
        {
            script = xset_custom_get_script( ctxt->set, FALSE );  //backwards compat copy
            if ( script )
                g_free( script );
            folder = g_build_filename( xset_get_config_dir(), "scripts",
                                                    ctxt->set->name, NULL );
        }
        if ( !g_file_test( folder, G_FILE_TEST_EXISTS ) && !ctxt->set->plugin )
        {
            g_mkdir_with_parents( folder, 0700 );
            chmod( folder, 0700 );
        }
    }
    else if ( job == 1 )
    {
        // Data Dir
        if ( ctxt->set->plugin )
        {
            XSet* mset = xset_get_plugin_mirror( ctxt->set );
            folder = g_build_filename( xset_get_config_dir(), "plugin-data",
                                                        mset->name, NULL );
        }
        else
            folder = g_build_filename( xset_get_config_dir(), "plugin-data",
                                                        ctxt->set->name, NULL );
        if ( !g_file_test( folder, G_FILE_TEST_EXISTS ) )
        {
            g_mkdir_with_parents( folder, 0700 );
            chmod( folder, 0700 );
        }
    }
    else if ( job == 2 )
    {
        // Plugin Dir
        if ( ctxt->set->plugin && ctxt->set->plug_dir )
            folder = g_strdup( ctxt->set->plug_dir );
    }
    else
        return;
    if ( folder && g_file_test( folder, G_FILE_TEST_IS_DIR ) )
        open_in_prog( folder );
    g_free( folder );
}

void on_key_button_clicked( GtkWidget* widget, ContextData* ctxt )
{
    xset_set_key( ctxt->dlg, ctxt->set );

    XSet* keyset;
    if ( ctxt->set->shared_key )
        keyset = xset_get( ctxt->set->shared_key );
    else
        keyset = ctxt->set;
    char* str = xset_get_keyname( keyset, 0, 0 );
    gtk_button_set_label( GTK_BUTTON( ctxt->item_key ), str );
    g_free( str );    
}

void on_type_changed( GtkComboBox* box, ContextData* ctxt )
{
    XSet* rset = ctxt->set;
    XSet* mset = xset_get_plugin_mirror( rset );
    int job = gtk_combo_box_get_active( GTK_COMBO_BOX( box ) );
    if ( job < ITEM_TYPE_COMMAND )
    {
        // Bookmark or App
        gtk_widget_show( ctxt->target_vbox );
        gtk_widget_hide( gtk_notebook_get_nth_page(
                                    GTK_NOTEBOOK( ctxt->notebook ), 2 ) );
        gtk_widget_hide( gtk_notebook_get_nth_page(
                                    GTK_NOTEBOOK( ctxt->notebook ), 3 ) );

        if ( job == ITEM_TYPE_BOOKMARK )
        {
            gtk_widget_hide( ctxt->item_choose );
            gtk_widget_hide( ctxt->hbox_opener );
            gtk_button_set_image( GTK_BUTTON( ctxt->item_browse ),
                                  xset_get_image( GTK_STOCK_OPEN,
                                                  GTK_ICON_SIZE_BUTTON ) );
            gtk_label_set_text( GTK_LABEL( ctxt->target_label ),
                    _("Targets:  (a semicolon-separated list of paths or URLs)") );
        }
        else
        {
            gtk_widget_show( ctxt->item_choose );
            gtk_widget_show( ctxt->hbox_opener );
            gtk_button_set_image( GTK_BUTTON( ctxt->item_browse ),
                                  xset_get_image( GTK_STOCK_EXECUTE,
                                                  GTK_ICON_SIZE_BUTTON ) );
            gtk_label_set_text( GTK_LABEL( ctxt->target_label ),
                    _("Target:  (a .desktop or executable file)") );
        }
    }
    else
    {
        // Command
        gtk_widget_hide( ctxt->target_vbox );
        gtk_widget_show( ctxt->hbox_opener );
        gtk_widget_show( gtk_notebook_get_nth_page(
                                    GTK_NOTEBOOK( ctxt->notebook ), 2 ) );
        gtk_widget_show( gtk_notebook_get_nth_page(
                                    GTK_NOTEBOOK( ctxt->notebook ), 3 ) );
    }

    // load command data
    if ( rset->x && atoi( rset->x ) == XSET_CMD_SCRIPT )
    {
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(
                                            ctxt->cmd_opt_script ), TRUE );
        gtk_widget_hide( ctxt->cmd_line_label );
        load_command_script( ctxt, rset );
    }
    else
        load_text_view( GTK_TEXT_VIEW( ctxt->cmd_script ), rset->line );
    GtkTextBuffer* buf = gtk_text_view_get_buffer( GTK_TEXT_VIEW( 
                                                    ctxt->cmd_script ) );
    GtkTextIter siter;
    gtk_text_buffer_get_start_iter( buf, &siter );
    gtk_text_buffer_place_cursor( buf, &siter );

    // command options
    // if ctxt->reset_command is TRUE, user may be switching from bookmark to
    // command, so reset the command options to defaults (they are not stored
    // for bookmarks/applications)
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( ctxt->opt_terminal ),
                                    mset->in_terminal == XSET_B_TRUE
                                    && !ctxt->reset_command );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( ctxt->opt_keep_term ),
                                    mset->keep_terminal == XSET_B_TRUE
                                    || ctxt->reset_command );
    gtk_entry_set_text( GTK_ENTRY( ctxt->cmd_user ),
                                                rset->y ? rset->y : "" );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( ctxt->opt_task ),
                                    mset->task == XSET_B_TRUE
                                    || ctxt->reset_command );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( ctxt->opt_task_pop ),
                                    mset->task_pop == XSET_B_TRUE
                                    && !ctxt->reset_command );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( ctxt->opt_task_err ),
                                    mset->task_err == XSET_B_TRUE
                                    || ctxt->reset_command );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( ctxt->opt_task_out ),
                                    mset->task_out == XSET_B_TRUE
                                    || ctxt->reset_command );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( ctxt->opt_scroll ),
                                    mset->scroll_lock != XSET_B_TRUE
                                    || ctxt->reset_command );
    if ( rset->menu_style == XSET_MENU_CHECK )
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( 
                                                ctxt->cmd_opt_checkbox ),
                                      TRUE );
    else if ( rset->menu_style == XSET_MENU_CONFIRM )
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( 
                                                ctxt->cmd_opt_confirm ),
                                      TRUE );
    else if ( rset->menu_style == XSET_MENU_STRING )
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( 
                                                ctxt->cmd_opt_input ),
                                      TRUE );
    else
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( 
                                                ctxt->cmd_opt_normal ),
                                      TRUE );
    load_text_view( GTK_TEXT_VIEW( ctxt->cmd_msg ), rset->desc );
    enable_options( ctxt );
    if ( geteuid() == 0 )
    {
        // running as root
        gtk_widget_hide( ctxt->cmd_edit );
    }
    
    if ( job < ITEM_TYPE_COMMAND )
    {
        // Bookmark or App
        buf = gtk_text_view_get_buffer( GTK_TEXT_VIEW( ctxt->item_target ) );
        gtk_text_buffer_set_text( buf, "", -1 );
        gtk_entry_set_text( GTK_ENTRY( ctxt->item_name ), "" );
        gtk_entry_set_text( GTK_ENTRY( ctxt->item_icon ), "" );

        gtk_widget_grab_focus( ctxt->item_target );
        // click Browse
        //gtk_button_clicked( GTK_BUTTON( ctxt->item_browse ) );
    }
    
    if ( job == ITEM_TYPE_COMMAND || job == ITEM_TYPE_APP )
    {
        // Opener
        if ( mset->opener > 2 || mset->opener < 0 )
            // forwards compat
            gtk_combo_box_set_active( GTK_COMBO_BOX( ctxt->opener ), -1 );
        else
            gtk_combo_box_set_active( GTK_COMBO_BOX( ctxt->opener ),
                                                        mset->opener );
    }
}

void on_browse_button_clicked( GtkWidget* widget, ContextData* ctxt )
{
    int job = gtk_combo_box_get_active( GTK_COMBO_BOX( ctxt->item_type ) );
    if ( job == ITEM_TYPE_BOOKMARK )
    {
        // Bookmark Browse
        char* add_path = xset_file_dialog( ctxt->dlg,
                            GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                            _("Choose Folder"),
                            ctxt->context->var[CONTEXT_DIR], NULL );
        if ( add_path && add_path[0] )
        {
            char* old_path = multi_input_get_text( ctxt->item_target );
            char* new_path = g_strdup_printf( "%s%s%s",
                        old_path && old_path[0] ? old_path : "",
                        old_path && old_path[0] ? "; " : "",
                        add_path );
            GtkTextBuffer* buf = gtk_text_view_get_buffer( GTK_TEXT_VIEW( 
                                                    ctxt->item_target ) );
            gtk_text_buffer_set_text( buf, new_path, -1 );
            g_free( new_path );
            g_free( add_path );
            g_free( old_path );
        }
    }
    else
    {
        // Application
        if ( widget == ctxt->item_choose )
        {
            // Choose
            VFSMimeType* mime_type = vfs_mime_type_get_from_type( 
                        ctxt->context->var[CONTEXT_MIME] &&
                        ctxt->context->var[CONTEXT_MIME][0] ?
                        ctxt->context->var[CONTEXT_MIME] : XDG_MIME_TYPE_UNKNOWN );
            char* app = (char*)ptk_choose_app_for_mime_type(
                            GTK_WINDOW( gtk_widget_get_toplevel( 
                                                    GTK_WIDGET( ctxt->dlg ) ) ),
                            mime_type, TRUE, FALSE, FALSE, FALSE );
            if ( app && app[0] )
            {
                GtkTextBuffer* buf = gtk_text_view_get_buffer( GTK_TEXT_VIEW( 
                                                        ctxt->item_target ) );
                gtk_text_buffer_set_text( buf, app, -1 );
            }
            g_free( app );
            vfs_mime_type_unref( mime_type );
        }
        else
        {
            // Browse
            char* exec_path = xset_file_dialog( ctxt->dlg,
                                GTK_FILE_CHOOSER_ACTION_OPEN,
                                _("Choose Executable"), "/usr/bin", NULL );
            if ( exec_path && exec_path[0] )
            {
                GtkTextBuffer* buf = gtk_text_view_get_buffer( GTK_TEXT_VIEW( 
                                                        ctxt->item_target ) );
                gtk_text_buffer_set_text( buf, exec_path, -1 );
                g_free( exec_path );
            }
        }
    }
}

void replace_item_props( ContextData* ctxt )
{
    int item_type, x;
    XSet* rset = ctxt->set;
    XSet* mset = xset_get_plugin_mirror( rset );

    if ( !rset->lock && rset->menu_style != XSET_MENU_SUBMENU &&
                        rset->menu_style != XSET_MENU_SEP &&
                        rset->tool <= XSET_TOOL_CUSTOM )
    {
        // custom bookmark, app, or command
        gboolean is_bookmark_or_app = FALSE;
        int item_type = gtk_combo_box_get_active( GTK_COMBO_BOX( ctxt->item_type ) );
        if ( item_type == ITEM_TYPE_COMMAND )
        {
            if ( gtk_toggle_button_get_active(
                                    GTK_TOGGLE_BUTTON( ctxt->cmd_opt_line ) ) )
                // line
                x = XSET_CMD_LINE;
            else
            {
                // script
                x = XSET_CMD_SCRIPT;
                save_command_script( ctxt, FALSE );
            }
        }
        else if ( item_type ==  ITEM_TYPE_APP)
        {
            x = XSET_CMD_APP;
            is_bookmark_or_app = TRUE;
        }
        else if ( item_type == ITEM_TYPE_BOOKMARK )
        {
            x = XSET_CMD_BOOKMARK;
            is_bookmark_or_app = TRUE;
        }
        else
            x = -1;
        if ( x >= 0 )
        {
            g_free( rset->x );
            if ( x == 0 )
                rset->x = NULL;
            else
                rset->x = g_strdup_printf( "%d", x );
        }
        if ( !rset->plugin )
        {
            // target
            char* str = multi_input_get_text( ctxt->item_target );
            g_free( rset->z );
            rset->z = str ? g_strstrip( str ) : NULL;
            // run as user
            g_free( rset->y );
            rset->y = g_strdup( gtk_entry_get_text( GTK_ENTRY(
                                                        ctxt->cmd_user ) ) );
            // menu style
            if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
                                                ctxt->cmd_opt_checkbox ) ) )
                rset->menu_style = XSET_MENU_CHECK;
            else if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
                                                ctxt->cmd_opt_confirm ) ) )
                rset->menu_style = XSET_MENU_CONFIRM;
            else if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
                                                ctxt->cmd_opt_input ) ) )
                rset->menu_style = XSET_MENU_STRING;
            else
                rset->menu_style = XSET_MENU_NORMAL;
            // style msg
            g_free( rset->desc );
            rset->desc = get_text_view( GTK_TEXT_VIEW( ctxt->cmd_msg ) );
        }
        // command line
        g_free( rset->line );
        if ( x == XSET_CMD_LINE )
        {
            rset->line = get_text_view( GTK_TEXT_VIEW( ctxt->cmd_script ) );
            if ( rset->line && strlen( rset->line ) > 2000 )
                xset_msg_dialog( ctxt->dlg, GTK_MESSAGE_WARNING,
                   _("Command Line Too Long"), NULL,
                   GTK_BUTTONS_OK,
                   _("Your command line is greater than 2000 characters and may be truncated when saved.  Consider using a command script instead by selecting Script on the Command tab."),
                   NULL, NULL );
        }
        else
            rset->line = g_strdup( ctxt->temp_cmd_line );

        // run options
        mset->in_terminal = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
                                                    ctxt->opt_terminal ) )
                                                && !is_bookmark_or_app ?
                            XSET_B_TRUE : XSET_B_UNSET;
        mset->keep_terminal = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
                                                    ctxt->opt_keep_term ) )
                                                && !is_bookmark_or_app ?
                            XSET_B_TRUE : XSET_B_UNSET;
        mset->task = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
                                                    ctxt->opt_task ) )
                                                && !is_bookmark_or_app ?
                            XSET_B_TRUE : XSET_B_UNSET;
        mset->task_pop = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
                                                    ctxt->opt_task_pop ) )
                                                && !is_bookmark_or_app ?
                            XSET_B_TRUE : XSET_B_UNSET;
        mset->task_err = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
                                                    ctxt->opt_task_err ) )
                                                && !is_bookmark_or_app ?
                            XSET_B_TRUE : XSET_B_UNSET;
        mset->task_out = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
                                                    ctxt->opt_task_out ) )
                                                && !is_bookmark_or_app ?
                            XSET_B_TRUE : XSET_B_UNSET;
        mset->scroll_lock = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
                                                    ctxt->opt_scroll ) )
                                                || is_bookmark_or_app ?
                            XSET_B_UNSET : XSET_B_TRUE;

        // Opener
        if ( ( item_type == ITEM_TYPE_COMMAND || item_type == ITEM_TYPE_APP ) )
        {
            if ( gtk_combo_box_get_active( GTK_COMBO_BOX( ctxt->opener ) ) > -1 )
                mset->opener = gtk_combo_box_get_active( GTK_COMBO_BOX(
                                                    ctxt->opener ) );
            // otherwise don't change for forward compat
        }
        else
            // reset if not applicable
            mset->opener = 0;
    }
    if ( rset->menu_style != XSET_MENU_SEP && !rset->plugin )
    {
        // name
        if ( rset->lock && rset->in_terminal == XSET_B_UNSET &&
                    g_strcmp0( rset->menu_label, gtk_entry_get_text(
                                            GTK_ENTRY( ctxt->item_name ) ) ) )
            // built-in label has been changed from default, save it
            rset->in_terminal = XSET_B_TRUE;

        g_free( rset->menu_label );
        if ( rset->tool > XSET_TOOL_CUSTOM && !g_strcmp0( gtk_entry_get_text(
                                            GTK_ENTRY( ctxt->item_name ) ),
                             xset_get_builtin_toolitem_label( rset->tool ) ) )
            // don't save default label of builtin toolitems
            rset->menu_label = NULL;
        else
            rset->menu_label = g_strdup( gtk_entry_get_text(
                                            GTK_ENTRY( ctxt->item_name ) ) );
    }
    // icon
    if ( rset->menu_style != XSET_MENU_RADIO &&
         rset->menu_style != XSET_MENU_SEP )
         // checkbox items in 1.0.1 allow icon due to bookmark list showing
         // toolbar checkbox items have icon
         //( rset->menu_style != XSET_MENU_CHECK || rset->tool ) )
    {
        char* old_icon = g_strdup( mset->icon );
        g_free( mset->icon );
        const char* icon_name = gtk_entry_get_text(
                                            GTK_ENTRY( ctxt->item_icon ) );
        if ( icon_name && icon_name[0] )
            mset->icon = g_strdup( icon_name );
        else
            mset->icon = NULL;

        if ( rset->lock && rset->keep_terminal == XSET_B_UNSET &&
                                        g_strcmp0( old_icon, mset->icon ) )
            // built-in icon has been changed from default, save it
            rset->keep_terminal = XSET_B_TRUE;
        g_free( old_icon );
    }

    // Ignore Context
    xset_set_b( "context_dlg",
            gtk_toggle_button_get_active(
                                GTK_TOGGLE_BUTTON( ctxt->ignore_context ) ) );
}

void on_script_font_change( GtkMenuItem* item, GtkTextView *input )
{
    char* fontname = xset_get_s( "context_dlg" );
    if ( fontname )
    {
        PangoFontDescription* font_desc = pango_font_description_from_string(
                                                                fontname );
        gtk_widget_modify_font( GTK_WIDGET( input ), font_desc );
        pango_font_description_free( font_desc );
    }
    else
        gtk_widget_modify_font( GTK_WIDGET( input ), NULL );
}

void on_script_popup( GtkTextView *input, GtkMenu *menu, gpointer user_data )
{
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    XSet* set = xset_get( "sep_ctxt" );
    set->menu_style = XSET_MENU_SEP;
    set->browser = NULL;
    set->desktop = NULL;
    xset_add_menuitem( NULL, NULL, GTK_WIDGET( menu ), accel_group, set );
    set = xset_set_cb( "context_dlg", on_script_font_change, input );
    set->browser = NULL;
    set->desktop = NULL;
    xset_add_menuitem( NULL, NULL, GTK_WIDGET( menu ), accel_group, set );
    
    gtk_widget_show_all( GTK_WIDGET( menu ) );
}

static gboolean delayed_focus( GtkWidget* widget )
{
    if ( GTK_IS_WIDGET( widget ) )
        gtk_widget_grab_focus( widget );
    return FALSE;
}

void on_prop_notebook_switch_page( GtkNotebook *notebook,
                                   GtkWidget *page,
                                   guint page_num,
                                   ContextData* ctxt )
{
    GtkWidget* widget;
    if ( page_num == 0 )
        widget = ctxt->set->plugin ? ctxt->item_icon : ctxt->item_name;
    else if ( page_num == 2 )
        widget = ctxt->cmd_script;
    else
        widget = NULL;
    g_idle_add( (GSourceFunc)delayed_focus, widget );
}

static void on_icon_choose_button_clicked( GtkWidget* widget, ContextData* ctxt )
{
    // get current icon
    char* new_icon;
    const char* icon = gtk_entry_get_text( GTK_ENTRY( ctxt->item_icon ) );

    new_icon = xset_icon_chooser_dialog( GTK_WINDOW( ctxt->dlg ), icon );
    
    if ( new_icon )
    {
        gtk_entry_set_text( GTK_ENTRY( ctxt->item_icon ), new_icon );
        g_free( new_icon );
    }
}

static void on_entry_buffer_inserted_text( GtkEntryBuffer* buf,
                                           guint position,
                                           gchar* chars,
                                           guint n_chars,
                                           ContextData* ctxt )
{
    // update icon of icon choose button
    const char* icon = gtk_entry_get_text( GTK_ENTRY( ctxt->item_icon ) );
    gtk_button_set_image( GTK_BUTTON( ctxt->icon_choose_btn ),
                        xset_get_image(
                        icon && icon[0] ? icon :
                        GTK_STOCK_OPEN,
                        GTK_ICON_SIZE_BUTTON ) );
}

static void on_entry_buffer_deleted_text( GtkEntryBuffer* buf,
                                           guint position,
                                           guint n_chars,
                                           ContextData* ctxt )
{
    on_entry_buffer_inserted_text( buf, position, NULL, n_chars, ctxt );
}
               
void on_entry_activate( GtkWidget* entry, ContextData* ctxt )
{
    gtk_button_clicked( GTK_BUTTON( ctxt->btn_ok ) );
}

static gboolean on_target_keypress( GtkWidget *widget, GdkEventKey *event,
                                                        ContextData* ctxt )
{
    if ( event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter )
    {
        gtk_button_clicked( GTK_BUTTON( ctxt->btn_ok ) );
        return TRUE;
    }
    return FALSE;
}

static gboolean on_dlg_keypress( GtkWidget *widget, GdkEventKey *event,
                                                            ContextData* ctxt )
{
    int keymod = ( event->state & ( GDK_SHIFT_MASK | GDK_CONTROL_MASK |
             GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK ) );

    if ( event->keyval == GDK_KEY_F1 && keymod == 0 )
    {
        gtk_dialog_response( GTK_DIALOG( ctxt->dlg ), GTK_RESPONSE_HELP );
        return TRUE;
    }
    return FALSE;
}

void xset_item_prop_dlg( XSetContext* context, XSet* set, int page )
{
    GtkTreeViewColumn* col;
    GtkCellRenderer* renderer;
    int i, x;
    char* str;

    if ( !context || !set )
        return;
    ContextData* ctxt = g_slice_new0( ContextData );
    ctxt->context = context;
    ctxt->set = set;
    ctxt->script_stat_valid = ctxt->reset_command = FALSE;
    ctxt->parent = NULL;
    if ( set->browser )
        ctxt->parent = gtk_widget_get_toplevel( GTK_WIDGET( set->browser ) );
    else if ( set->desktop )
        ctxt->parent = gtk_widget_get_toplevel( GTK_WIDGET( set->desktop ) );

    // Dialog
    ctxt->dlg = gtk_dialog_new_with_buttons( 
                                set->tool ? _("Toolbar Item Properties") : 
                                            _("Menu Item Properties"),
                                GTK_WINDOW( ctxt->parent ),
                                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                NULL, NULL );
    xset_set_window_icon( GTK_WINDOW( ctxt->dlg ) );
    gtk_window_set_role( GTK_WINDOW( ctxt->dlg ), "context_dialog" );

    int width = xset_get_int( "context_dlg", "x" );
    int height = xset_get_int( "context_dlg", "y" );
    if ( width && height )
        gtk_window_set_default_size( GTK_WINDOW( ctxt->dlg ), width, height );
    else
        gtk_window_set_default_size( GTK_WINDOW( ctxt->dlg ), 800, 600 );
    
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
    GtkWidget* align = gtk_alignment_new( 0, 0, 0.4, 1 );
    gtk_alignment_set_padding( GTK_ALIGNMENT( align ), 8, 0, 8, 8 );
    GtkWidget* vbox = gtk_vbox_new( FALSE, 4 );
    gtk_container_add ( GTK_CONTAINER ( align ), vbox );
    gtk_notebook_append_page( GTK_NOTEBOOK( ctxt->notebook ),
                              align,
                              gtk_label_new_with_mnemonic( 
                              set->tool ? _("_Toolbar Item") :
                                          _("_Menu Item") ) );
    
    GtkTable* table = GTK_TABLE( gtk_table_new( 4, 2, FALSE ) );
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
    //align = gtk_alignment_new( 0, 0.5, 1, 1 );
    //gtk_container_add ( GTK_CONTAINER ( align ), GTK_WIDGET( ctxt->item_type ) );
    gtk_table_attach( table, ctxt->item_type, 1, 2, row, row + 1,
                                    GTK_FILL, GTK_SHRINK, 0, 0 );
    
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
    ctxt->item_key = gtk_button_new_with_label( " " );
    gtk_button_set_focus_on_click( GTK_BUTTON( ctxt->item_key ), FALSE );
    gtk_table_attach( table, ctxt->item_key, 1, 2, row, row + 1,
                                    GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0 );

    label = gtk_label_new_with_mnemonic( _("Icon:") );
    gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0.5 );
    row++;
    gtk_table_attach( table, label, 0, 1, row, row + 1, 
                                    GTK_FILL, GTK_SHRINK, 0, 0 );
    GtkWidget* hbox = gtk_hbox_new( FALSE, 0 );
    ctxt->icon_choose_btn = gtk_button_new_with_mnemonic( _("C_hoose") );
    gtk_button_set_image( GTK_BUTTON( ctxt->icon_choose_btn ),
                                  xset_get_image( GTK_STOCK_OPEN,
                                                  GTK_ICON_SIZE_BUTTON ) );
    gtk_button_set_focus_on_click( GTK_BUTTON( ctxt->icon_choose_btn ), FALSE );
#if GTK_CHECK_VERSION (3, 6, 0)
    // keep this
    gtk_button_set_always_show_image( GTK_BUTTON( ctxt->icon_choose_btn ), TRUE );
#endif
    ctxt->item_icon = gtk_entry_new();
    g_signal_connect( G_OBJECT(
                    gtk_entry_get_buffer( GTK_ENTRY( ctxt->item_icon ) ) ),
                    "inserted-text",
                    G_CALLBACK( on_entry_buffer_inserted_text ), ctxt );
    g_signal_connect( G_OBJECT(
                    gtk_entry_get_buffer( GTK_ENTRY( ctxt->item_icon ) ) ),
                    "deleted-text",
                    G_CALLBACK( on_entry_buffer_deleted_text ), ctxt );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( ctxt->item_icon ), TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( ctxt->icon_choose_btn ), FALSE, TRUE, 0 );
    gtk_table_attach( table, hbox, 1, 2, row, row + 1,
                                    GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0 );

    gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( table ), FALSE, TRUE, 0 );

    // Target
    ctxt->target_vbox = gtk_vbox_new( FALSE, 0 );

    ctxt->target_label = gtk_label_new( NULL );
    gtk_misc_set_alignment( GTK_MISC ( ctxt->target_label ), 0, 0.5 );

    GtkWidget* scroll = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( scroll ),
                                     GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW( scroll ),
                                                    GTK_SHADOW_ETCHED_IN );
    ctxt->item_target = GTK_WIDGET( multi_input_new( 
                            GTK_SCROLLED_WINDOW( scroll ), NULL, FALSE ) );
    gtk_widget_set_size_request( GTK_WIDGET( ctxt->item_target ), -1, 100 );
    gtk_widget_set_size_request( GTK_WIDGET( scroll ), -1, 100 );
    
    gtk_box_pack_start( GTK_BOX( ctxt->target_vbox ),
                        GTK_WIDGET( ctxt->target_label ), FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( ctxt->target_vbox ),
                        GTK_WIDGET( scroll ), FALSE, TRUE, 0 );

    hbox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( gtk_label_new( NULL ) ), TRUE, TRUE, 0 );
    ctxt->item_choose = gtk_button_new_with_mnemonic( _("C_hoose") );
    gtk_button_set_image( GTK_BUTTON( ctxt->item_choose ),
                                  xset_get_image( GTK_STOCK_OPEN,
                                                  GTK_ICON_SIZE_BUTTON ) );
    gtk_button_set_focus_on_click( GTK_BUTTON( ctxt->item_choose ), FALSE );
    //align = gtk_alignment_new( 1, 0.5, 0.2, 1 );
    //gtk_container_add ( GTK_CONTAINER ( align ), ctxt->item_choose );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( ctxt->item_choose ), FALSE, TRUE, 12 );

    ctxt->item_browse = gtk_button_new_with_mnemonic( _("_Browse") );
    gtk_button_set_focus_on_click( GTK_BUTTON( ctxt->item_browse ), FALSE );
    //align = gtk_alignment_new( 1, 0.5, 0.2, 1 );
    //gtk_container_add ( GTK_CONTAINER ( align ), ctxt->item_browse );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( ctxt->item_browse ), FALSE, TRUE, 0 );

    gtk_box_pack_start( GTK_BOX( ctxt->target_vbox ),
                        GTK_WIDGET( hbox ), FALSE, TRUE, 0 );

    gtk_box_pack_start( GTK_BOX( vbox ),
                        GTK_WIDGET( ctxt->target_vbox ), FALSE, TRUE, 4 );

    //ctxt->show_tool = gtk_check_button_new_with_mnemonic(
    //                                                _("S_how In Toolbar") );
    //gtk_box_pack_start( GTK_BOX( vbox ),
    //                    GTK_WIDGET( ctxt->show_tool ), FALSE, TRUE, 16 );


    // Context Page  =======================================================
    align = gtk_alignment_new( 0, 0, 1, 1 );
    gtk_alignment_set_padding( GTK_ALIGNMENT( align ), 8, 0, 8, 8 );
    vbox = gtk_vbox_new( FALSE, 0 );
    gtk_container_add ( GTK_CONTAINER ( align ), vbox );
    gtk_notebook_append_page( GTK_NOTEBOOK( ctxt->notebook ),
                        align, gtk_label_new_with_mnemonic( _("Con_text") ) );

    GtkListStore* list = gtk_list_store_new( 4, G_TYPE_STRING, G_TYPE_INT, 
                                                G_TYPE_INT, G_TYPE_STRING );

    // Listview
    ctxt->view = exo_tree_view_new();
    gtk_tree_view_set_model( GTK_TREE_VIEW( ctxt->view ), GTK_TREE_MODEL( list ) );
    // gtk_tree_view_set_model adds a ref
    g_object_unref( list );
    exo_tree_view_set_single_click( (ExoTreeView*)ctxt->view, TRUE );
    gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( ctxt->view ), FALSE );

    scroll = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( scroll ),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW( scroll ),
                                                    GTK_SHADOW_ETCHED_IN );
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

    ctxt->vbox_context = gtk_vbox_new( FALSE, 0 );
    ctxt->hbox_match = gtk_hbox_new( FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( ctxt->hbox_match ),
                        GTK_WIDGET( ctxt->box_action ), FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( ctxt->hbox_match ),
                        GTK_WIDGET( gtk_label_new( _("item if context") ) ), FALSE,
                                                                        TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( ctxt->hbox_match ),
                        GTK_WIDGET( ctxt->box_match ), FALSE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( ctxt->vbox_context ),
                        GTK_WIDGET( ctxt->hbox_match ), FALSE, TRUE, 4 );

//    GtkLabel* label = gtk_label_new( "Rules:" );
//    gtk_misc_set_alignment( label, 0, 1 );
//    gtk_box_pack_start( GTK_BOX( GTK_DIALOG( ctxt->dlg )->vbox ),
//                        GTK_WIDGET( label ), FALSE, TRUE, 8 );
    gtk_box_pack_start( GTK_BOX( ctxt->vbox_context ),
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
    gtk_box_pack_start( GTK_BOX( ctxt->vbox_context ),
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
    hbox = gtk_hbox_new( FALSE, 4 );
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
    gtk_box_pack_start( GTK_BOX( ctxt->vbox_context ),
                        GTK_WIDGET( ctxt->frame ), FALSE, TRUE, 16 );
    gtk_box_pack_start( GTK_BOX( vbox ),
                        GTK_WIDGET( ctxt->vbox_context ), TRUE, TRUE, 0 );
    
    // Opener
    ctxt->hbox_opener = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( ctxt->hbox_opener ),
                        GTK_WIDGET( gtk_label_new( _("If enabled, use as handler for:") ) ),
                        FALSE, TRUE, 0 );
    ctxt->opener = gtk_combo_box_text_new();
    gtk_combo_box_set_focus_on_click( GTK_COMBO_BOX( ctxt->opener ), FALSE );
    gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( ctxt->opener ),
                                                    _("none") );
    gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( ctxt->opener ),
                                                    _("files") );
    gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( ctxt->opener ),
                                                    _("devices") );    
    gtk_box_pack_start( GTK_BOX( ctxt->hbox_opener ),
                        GTK_WIDGET( ctxt->opener ), FALSE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( vbox ),
                        GTK_WIDGET( ctxt->hbox_opener ), FALSE, TRUE, 0 );
    
    // Ignore Context
    ctxt->ignore_context = gtk_check_button_new_with_mnemonic(
                            _("_Ignore Context / Show All  (global setting)") );
    gtk_box_pack_start( GTK_BOX( vbox ),
                        GTK_WIDGET( ctxt->ignore_context ), FALSE, TRUE, 0 );
    if ( xset_get_b( "context_dlg" ) )
    {
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(
                                            ctxt->ignore_context ), TRUE );
        
        gtk_widget_set_sensitive( ctxt->vbox_context, FALSE );
    }
    g_signal_connect( G_OBJECT( ctxt->ignore_context ), "toggled",
                            G_CALLBACK( on_ignore_context_toggled ), ctxt );
    
    // plugin?
    XSet* mset;  // mirror set or set
    XSet* rset;  // real set
    if ( set->plugin )
    {
        // set is plugin
        mset = xset_get_plugin_mirror( set );
        rset = set;
    }
    else if ( !set->lock && set->desc && !strcmp( set->desc, "@plugin@mirror@" )
                                                            && set->shared_key )
    {
        // set is plugin mirror
        mset = set;
        rset = xset_get( set->shared_key );
        rset->browser = set->browser;
        rset->desktop = set->desktop;
    }
    else
    {
        mset = set;
        rset = set;
    }
    ctxt->set = rset;
    
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
    //gtk_widget_set_sensitive( GTK_WIDGET( ctxt->btn_ok ), is_rules );
    
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
    ctxt->cmd_opt_script = gtk_radio_button_new_with_mnemonic_from_widget(
                            GTK_RADIO_BUTTON( ctxt->cmd_opt_line ), _("_Script") );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( ctxt->cmd_opt_line ), FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( ctxt->cmd_opt_script ), FALSE, TRUE, 0 );
    ctxt->cmd_edit = gtk_button_new_with_mnemonic( _("Open In _Editor") );
    gtk_button_set_focus_on_click( GTK_BUTTON( ctxt->cmd_edit ), FALSE );
    g_signal_connect( G_OBJECT( ctxt->cmd_edit ), "clicked",
                          G_CALLBACK( on_edit_button_press ), ctxt );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( ctxt->cmd_edit ), FALSE, TRUE, 24 );
    ctxt->cmd_edit_root = gtk_button_new_with_mnemonic( _("_Root Editor") );
    gtk_button_set_focus_on_click( GTK_BUTTON( ctxt->cmd_edit_root ), FALSE );
    g_signal_connect( G_OBJECT( ctxt->cmd_edit_root ), "clicked",
                          G_CALLBACK( on_edit_button_press ), ctxt );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( ctxt->cmd_edit_root ), FALSE, TRUE, 24 );
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
    gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW(
                                                ctxt->cmd_scroll_script ),
                                                    GTK_SHADOW_ETCHED_IN );
    ctxt->cmd_script = GTK_WIDGET( gtk_text_view_new() );
    // ubuntu shows input too small so use mininum height
    gtk_widget_set_size_request( GTK_WIDGET( ctxt->cmd_script ), -1, 50 );
    gtk_widget_set_size_request( GTK_WIDGET( ctxt->cmd_scroll_script ), -1, 50 );
    gtk_text_view_set_wrap_mode( GTK_TEXT_VIEW( ctxt->cmd_script ),
                                            GTK_WRAP_WORD_CHAR );
    on_script_font_change( NULL, GTK_TEXT_VIEW( ctxt->cmd_script ) );
    g_signal_connect_after( G_OBJECT( ctxt->cmd_script ), "populate-popup",
                        G_CALLBACK( on_script_popup ), NULL );
    gtk_container_add ( GTK_CONTAINER ( ctxt->cmd_scroll_script ),  
                                            GTK_WIDGET( ctxt->cmd_script ) );
    gtk_box_pack_start( GTK_BOX( vbox ),
                        GTK_WIDGET( ctxt->cmd_scroll_script ), TRUE, TRUE, 4 );

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

    ctxt->opt_task = gtk_check_button_new_with_mnemonic( _("Run As Task") );
    gtk_box_pack_start( GTK_BOX( vbox_frame ),
                        GTK_WIDGET( ctxt->opt_task ), FALSE, TRUE, 0 );
    ctxt->opt_hbox_task = gtk_hbox_new( FALSE, 8 );
    ctxt->opt_task_pop = gtk_check_button_new_with_mnemonic( _("Popup Task") );
    ctxt->opt_task_err = gtk_check_button_new_with_mnemonic( _("Popup Error") );
    ctxt->opt_task_out = gtk_check_button_new_with_mnemonic( _("Popup Output") );
    ctxt->opt_scroll = gtk_check_button_new_with_mnemonic( _("Scroll Output") );
    gtk_box_pack_start( GTK_BOX( ctxt->opt_hbox_task ),
                        GTK_WIDGET( ctxt->opt_task_pop ), FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( ctxt->opt_hbox_task ),
                        GTK_WIDGET( ctxt->opt_task_err ), FALSE, TRUE, 6 );
    gtk_box_pack_start( GTK_BOX( ctxt->opt_hbox_task ),
                        GTK_WIDGET( ctxt->opt_task_out ), FALSE, TRUE, 6 );
    gtk_box_pack_start( GTK_BOX( ctxt->opt_hbox_task ),
                        GTK_WIDGET( ctxt->opt_scroll ), FALSE, TRUE, 6 );
    gtk_box_pack_start( GTK_BOX( vbox_frame ),
                        GTK_WIDGET( ctxt->opt_hbox_task ), FALSE, TRUE, 8 );

    hbox = gtk_hbox_new( FALSE, 8 );
    ctxt->opt_terminal = gtk_check_button_new_with_mnemonic( _("Run In Terminal") );
    ctxt->opt_keep_term = gtk_check_button_new_with_mnemonic( _("Keep Terminal Open") );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( ctxt->opt_terminal ), FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( ctxt->opt_keep_term ), FALSE, TRUE, 6 );
    gtk_box_pack_start( GTK_BOX( vbox_frame ),
                        GTK_WIDGET( hbox ), FALSE, TRUE, 0 );

    hbox = gtk_hbox_new( FALSE, 0 );
    label = gtk_label_new( _("Run As User:") );
    gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0.5 );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( label ), FALSE, TRUE, 2 );
    ctxt->cmd_user = gtk_entry_new();
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( ctxt->cmd_user ), FALSE, TRUE, 8 );
    label = gtk_label_new( _("( leave blank for current user )") );
    gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0.5 );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( label ), FALSE, TRUE, 8 );
    gtk_box_pack_start( GTK_BOX( vbox_frame ),
                        GTK_WIDGET( hbox ), FALSE, TRUE, 4 );

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
    gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW( ctxt->cmd_scroll_msg ),
                                                    GTK_SHADOW_ETCHED_IN );
    ctxt->cmd_msg = GTK_WIDGET( gtk_text_view_new() );
    // ubuntu shows input too small so use mininum height
    gtk_widget_set_size_request( GTK_WIDGET( ctxt->cmd_msg ), -1, 50 );
    gtk_widget_set_size_request( GTK_WIDGET( ctxt->cmd_scroll_msg ), -1, 50 );
    gtk_text_view_set_wrap_mode( GTK_TEXT_VIEW( ctxt->cmd_msg ),
                                            GTK_WRAP_WORD_CHAR );
    gtk_container_add ( GTK_CONTAINER ( ctxt->cmd_scroll_msg ),  
                                            GTK_WIDGET( ctxt->cmd_msg ) );
    gtk_box_pack_start( GTK_BOX( ctxt->cmd_vbox_msg ),
                        GTK_WIDGET( ctxt->cmd_scroll_msg ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( vbox_frame ),
                        GTK_WIDGET( ctxt->cmd_vbox_msg ), TRUE, TRUE, 0 );

    // open folder
    hbox = gtk_hbox_new( FALSE, 0 );
    label = gtk_label_new( _("Open In Browser:") );
    gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0.5 );
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( label ), FALSE, TRUE, 0 );
    ctxt->open_browser = gtk_combo_box_text_new();
    gtk_combo_box_set_focus_on_click( GTK_COMBO_BOX( ctxt->open_browser ), FALSE );

    char* path;
    if ( rset->plugin )
        path = g_build_filename( rset->plug_dir, rset->plug_name, NULL );
    else
        path = g_build_filename( xset_get_config_dir(), "scripts",
                                                    rset->name, NULL );
    str = g_strdup_printf( "%s  $fm_cmd_dir  %s", _("Command Dir"), 
                            dir_has_files( path ) ? "" : _("(no files)") );
    gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( ctxt->open_browser ),
                                                                    str );
    g_free( str );
    g_free( path );

    path = g_build_filename( xset_get_config_dir(), "plugin-data",
                            rset->plugin ? mset->name : rset->name, NULL );
    str = g_strdup_printf( "%s  $fm_cmd_data  %s", _("Data Dir"), 
                            dir_has_files( path ) ? "" : _("(no files)") );
    gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( ctxt->open_browser ),
                                                                    str );
    g_free( str );
    g_free( path );

    if ( rset->plugin )
    {
        str = g_strdup_printf( "%s  $fm_plugin_dir", _("Plugin Dir") );
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( ctxt->open_browser ),
                                                                    str );
        g_free( str );
    }
    gtk_box_pack_start( GTK_BOX( hbox ),
                        GTK_WIDGET( ctxt->open_browser ), FALSE, TRUE, 8 );
    gtk_box_pack_start( GTK_BOX( vbox ),
                        GTK_WIDGET( hbox ), FALSE, TRUE, 0 );

    // show all
    gtk_widget_show_all( GTK_WIDGET( ctxt->dlg ) );

    // load values  ========================================================
    // type
    int item_type = -1;

    char* item_type_str = NULL;
    if ( set->tool > XSET_TOOL_CUSTOM )
        item_type_str = g_strdup_printf( "%s: %s", _("Built-In Toolbar Item"),
                            xset_get_builtin_toolitem_label( set->tool ) );
    else if ( rset->menu_style == XSET_MENU_SUBMENU )
        item_type_str = g_strdup( _("Submenu") );
    else if  ( rset->menu_style == XSET_MENU_SEP )
        item_type_str = g_strdup( _("Separator") );
    else if ( set->lock )
    {
        // built-in
        item_type_str = g_strdup( _("Built-In Command") );
    }
    else
    {
        // custom command
        for ( i = 0; i < G_N_ELEMENTS( item_types ); i++ )
            gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(
                                                        ctxt->item_type ),
                                                        _(item_types[i]) );
        x = rset->x ? atoi( rset->x ) : 0;
        if ( x < XSET_CMD_APP )  // line or script
            item_type = ITEM_TYPE_COMMAND;
        else if ( x == XSET_CMD_APP )
            item_type = ITEM_TYPE_APP;
        else if ( x == XSET_CMD_BOOKMARK )
            item_type = ITEM_TYPE_BOOKMARK;
        else
            item_type = -1;
        gtk_combo_box_set_active( GTK_COMBO_BOX ( ctxt->item_type ),
                                                            item_type );
        //g_signal_connect( G_OBJECT( ctxt->item_type ), "changed",
        //                  G_CALLBACK( on_item_type_changed ), ctxt );
    }
    if ( item_type_str )
    {
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( ctxt->item_type ),
                                                            item_type_str );
        gtk_combo_box_set_active( GTK_COMBO_BOX ( ctxt->item_type ), 0 );
        gtk_widget_set_sensitive( ctxt->item_type, FALSE );
        g_free( item_type_str );
    }

    ctxt->temp_cmd_line = !set->lock ? g_strdup( rset->line ) : NULL;
    if ( set->lock || rset->menu_style == XSET_MENU_SUBMENU ||
                      rset->menu_style == XSET_MENU_SEP ||
                      set->tool > XSET_TOOL_CUSTOM )
    {
        gtk_widget_hide( gtk_notebook_get_nth_page(
                                    GTK_NOTEBOOK( ctxt->notebook ), 2 ) );
        gtk_widget_hide( gtk_notebook_get_nth_page(
                                    GTK_NOTEBOOK( ctxt->notebook ), 3 ) );
        gtk_widget_hide( ctxt->target_vbox );
        gtk_widget_hide( ctxt->hbox_opener );
    }
    else
    {
        // load command values
        on_type_changed( GTK_COMBO_BOX( ctxt->item_type ), ctxt );
        if ( rset->z )
        {
            GtkTextBuffer* buf = gtk_text_view_get_buffer(
                                            GTK_TEXT_VIEW( ctxt->item_target ) );
            gtk_text_buffer_set_text( buf, rset->z, -1 );
        }
    }
    ctxt->reset_command = TRUE;

    // name
    if ( rset->menu_style != XSET_MENU_SEP )
    {
        if ( set->menu_label )
            gtk_entry_set_text( GTK_ENTRY( ctxt->item_name ), set->menu_label );
        else if ( set->tool > XSET_TOOL_CUSTOM )
            gtk_entry_set_text( GTK_ENTRY( ctxt->item_name ),
                                xset_get_builtin_toolitem_label( set->tool ) );
    }
    else
        gtk_widget_set_sensitive( ctxt->item_name, FALSE );
    // key
    if ( rset->menu_style < XSET_MENU_SUBMENU ||
                            set->tool == XSET_TOOL_BACK_MENU ||
                            set->tool == XSET_TOOL_FWD_MENU )
    {
        XSet* keyset;
        if ( set->shared_key )
            keyset = xset_get( set->shared_key );
        else
            keyset = set;
        str = xset_get_keyname( keyset, 0, 0 );
        gtk_button_set_label( GTK_BUTTON( ctxt->item_key ), str );
        g_free( str );
    }
    else
        gtk_widget_set_sensitive( ctxt->item_key, FALSE );
    // icon
    if ( rset->icon || mset->icon )
        gtk_entry_set_text( GTK_ENTRY( ctxt->item_icon ),
                                    mset->icon ? mset->icon : rset->icon );
    gtk_widget_set_sensitive( ctxt->item_icon,
                    rset->menu_style != XSET_MENU_RADIO &&
                    rset->menu_style != XSET_MENU_SEP );
                    // toolbar checkbox items have icon
                    //( rset->menu_style != XSET_MENU_CHECK || rset->tool ) );
    gtk_widget_set_sensitive( ctxt->icon_choose_btn, 
                    rset->menu_style != XSET_MENU_RADIO &&
                    rset->menu_style != XSET_MENU_SEP );

    if ( set->plugin )
    {
        gtk_widget_set_sensitive( ctxt->item_type, FALSE );
        gtk_widget_set_sensitive( ctxt->item_name, FALSE );
        gtk_widget_set_sensitive( ctxt->item_target, FALSE );
        gtk_widget_set_sensitive( ctxt->item_browse, FALSE );
        gtk_widget_set_sensitive( ctxt->cmd_opt_normal, FALSE );
        gtk_widget_set_sensitive( ctxt->cmd_opt_checkbox, FALSE );
        gtk_widget_set_sensitive( ctxt->cmd_opt_confirm, FALSE );
        gtk_widget_set_sensitive( ctxt->cmd_opt_input, FALSE );
        gtk_widget_set_sensitive( ctxt->cmd_user, FALSE );
        gtk_widget_set_sensitive( ctxt->cmd_msg, FALSE );
        gtk_widget_set_sensitive( ctxt->cmd_opt_script, FALSE );
        gtk_widget_set_sensitive( ctxt->cmd_opt_line, FALSE );
    }
    if ( set->tool )
    {
        // Hide Context tab
        gtk_widget_hide( gtk_notebook_get_nth_page(
                                    GTK_NOTEBOOK( ctxt->notebook ), 1 ) );
        //gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( ctxt->show_tool ),
        //                              set->tool == XSET_B_TRUE );
    }
    //else
    //    gtk_widget_hide( ctxt->show_tool );
    
    // signals
    g_signal_connect( G_OBJECT( ctxt->opt_terminal ), "toggled",
                                G_CALLBACK( on_cmd_opt_toggled ), ctxt );
    g_signal_connect( G_OBJECT( ctxt->opt_task ), "toggled",
                                G_CALLBACK( on_cmd_opt_toggled ), ctxt );
    g_signal_connect( G_OBJECT( ctxt->cmd_opt_normal ), "toggled",
                                G_CALLBACK( on_cmd_opt_toggled ), ctxt );
    g_signal_connect( G_OBJECT( ctxt->cmd_opt_checkbox ), "toggled",
                                G_CALLBACK( on_cmd_opt_toggled ), ctxt );
    g_signal_connect( G_OBJECT( ctxt->cmd_opt_confirm ), "toggled",
                                G_CALLBACK( on_cmd_opt_toggled ), ctxt );
    g_signal_connect( G_OBJECT( ctxt->cmd_opt_input ), "toggled",
                                G_CALLBACK( on_cmd_opt_toggled ), ctxt );

    g_signal_connect( G_OBJECT( ctxt->cmd_opt_line ), "toggled",
                                G_CALLBACK( on_script_toggled ), ctxt );
    g_signal_connect( G_OBJECT( ctxt->cmd_opt_script ), "toggled",
                                G_CALLBACK( on_script_toggled ), ctxt );

    g_signal_connect( G_OBJECT( ctxt->item_target ), "key-press-event",
                          G_CALLBACK( on_target_keypress ), ctxt );
    g_signal_connect( G_OBJECT( ctxt->item_key ), "clicked",
                                G_CALLBACK( on_key_button_clicked ), ctxt );
    g_signal_connect( G_OBJECT( ctxt->icon_choose_btn ), "clicked",
                            G_CALLBACK( on_icon_choose_button_clicked ), ctxt );
    g_signal_connect( G_OBJECT( ctxt->item_choose ), "clicked",
                                G_CALLBACK( on_browse_button_clicked ), ctxt );
    g_signal_connect( G_OBJECT( ctxt->item_browse ), "clicked",
                                G_CALLBACK( on_browse_button_clicked ), ctxt );
    g_signal_connect( G_OBJECT( ctxt->item_name ), "activate",
                                G_CALLBACK( on_entry_activate ), ctxt );
    g_signal_connect( G_OBJECT( ctxt->item_icon ), "activate",
                                G_CALLBACK( on_entry_activate ), ctxt );

    g_signal_connect( G_OBJECT( ctxt->open_browser ), "changed",
                                G_CALLBACK( on_open_browser ), ctxt );
    g_signal_connect( G_OBJECT( ctxt->item_type ), "changed",
                                G_CALLBACK( on_type_changed ), ctxt );
    
    g_signal_connect( ctxt->notebook, "switch-page",
                        G_CALLBACK ( on_prop_notebook_switch_page ), ctxt );

    g_signal_connect( G_OBJECT( ctxt->dlg ), "key-press-event",
                                G_CALLBACK( on_dlg_keypress ), ctxt );

    // run
    enable_context( ctxt );
    if ( page && gtk_widget_get_visible( gtk_notebook_get_nth_page( 
                                GTK_NOTEBOOK( ctxt->notebook ), page ) ) )
        gtk_notebook_set_current_page( GTK_NOTEBOOK( ctxt->notebook ), page );
    else
        gtk_widget_grab_focus( ctxt->set->plugin ? ctxt->item_icon :
                                                   ctxt->item_name );
    
    int response;
    while ( response = gtk_dialog_run( GTK_DIALOG( ctxt->dlg ) ) )
    {
        if ( response == GTK_RESPONSE_OK )
        {
            if ( mset->context )
                g_free( mset->context );
            mset->context = context_build( ctxt );
            replace_item_props( ctxt );
            break;
        }
        else if ( response == GTK_RESPONSE_HELP )
        {
            const char* help;
            switch ( gtk_notebook_get_current_page(
                                        GTK_NOTEBOOK( ctxt->notebook ) ) ) {
            case 1:
                help = "#designmode-props-context";
                break;
            case 2:
                help = "#designmode-props-command";
                break;
            case 3:
                help = "#designmode-props-opts";
                break;
            default:
                help = "#designmode-props";
                break;
            }
            xset_show_help( ctxt->dlg, NULL, help );
        }
        else
            break;
    }

    GtkAllocation allocation;
    gtk_widget_get_allocation (GTK_WIDGET(ctxt->dlg), &allocation);
    width = allocation.width;
    height = allocation.height;
    if ( width && height )
    {
        str = g_strdup_printf( "%d", width );
        xset_set( "context_dlg", "x", str );
        g_free( str );
        str = g_strdup_printf( "%d", height );
        xset_set( "context_dlg", "y", str );
        g_free( str );
    }

    gtk_widget_destroy( ctxt->dlg );
    g_free( ctxt->temp_cmd_line );
    g_slice_free( ContextData, ctxt );
}

