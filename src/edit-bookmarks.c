/*
*  C Implementation: editbookmark
*
* Description:
*
*
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "pcmanfm.h"

#include "edit-bookmarks.h"
#include "ptk-bookmarks.h"

#include "private.h"

#include <gtk/gtk.h>
#include <glib.h>
#include <string.h>

enum{
    COL_ICON = 0,
    COL_NAME,
    COL_DIRPATH,
    NUM_COLS
};

static void on_add( GtkButton* btn, gpointer data )
{
    GtkWindow* parent = GTK_WINDOW(data);
    GtkTreeViewColumn* col;
    GtkTreeIter it, new_it, *pit;
    GtkTreePath* tree_path;
    GtkTreeView* view = (GtkTreeView*)g_object_get_data( G_OBJECT(data),
                                                         "list_view" );
    GtkTreeModel* model;
    GtkTreeSelection* sel = gtk_tree_view_get_selection( view );

    GdkPixbuf* icon = gtk_icon_theme_load_icon( gtk_icon_theme_get_default(),
                                                "gnome-fs-directory",
                                                20, 0, NULL );
    GtkWidget* dlg;
    char *path = NULL, *basename = NULL;

    if( gtk_tree_selection_get_selected ( sel, &model, &it ) )
    {
        tree_path = gtk_tree_model_get_path( model, &it );
        gtk_tree_path_next( tree_path );
        pit = &it;
    }
    else
    {
        tree_path = gtk_tree_path_new_first();
        pit = NULL;
    }

    dlg = gtk_file_chooser_dialog_new( NULL, parent,
                            GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK, GTK_RESPONSE_OK, NULL );
    if( gtk_dialog_run( (GtkDialog*) dlg ) == GTK_RESPONSE_OK )
    {
        path = gtk_file_chooser_get_filename( (GtkFileChooser*)dlg );
        basename = g_filename_display_basename( path );
    }
    gtk_widget_destroy( dlg );

    col = gtk_tree_view_get_column( view, 1 );
    gtk_list_store_insert_after( GTK_LIST_STORE(model), &new_it, pit );
    gtk_list_store_set( GTK_LIST_STORE(model), &new_it,
                        COL_ICON, icon,
                        COL_NAME, basename ? basename : _("New Item"),
                        COL_DIRPATH, path, -1);

    g_free( path );
    g_free( basename );

    if( tree_path )
    {
        gtk_tree_view_set_cursor_on_cell( view, tree_path, col, NULL, TRUE );
        gtk_tree_path_free( tree_path );
    }

    if( icon )
        g_object_unref( icon );
}

static void on_delete( GtkButton* btn, gpointer data )
{
    GtkTreeIter it;
    GtkTreeView* view = (GtkTreeView*)g_object_get_data( G_OBJECT(data),
                                                         "list_view" );
    GtkTreeModel* model;
    GtkTreeSelection* sel = gtk_tree_view_get_selection( view );
    if( gtk_tree_selection_get_selected ( sel, &model, &it ) )
    {
        gtk_list_store_remove( GTK_LIST_STORE(model), &it );
    }
    gtk_widget_grab_focus( GTK_WIDGET(view) );
}

static void on_name_edited (GtkCellRendererText *cell,
                            gchar               *path_string,
                            gchar               *new_text,
                            GtkListStore* list )
{
    GtkTreeIter it;
    gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(list),
                                        &it, path_string );
    if( new_text && *new_text )
        gtk_list_store_set(list, &it, COL_NAME, new_text, -1);
}

static void on_path_edited (GtkCellRendererText *cell,
                            gchar               *path_string,
                            gchar               *new_text,
                            GtkListStore* list )
{
    GtkTreeIter it;
    gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(list),
                                        &it, path_string );
    if( new_text && *new_text )
        gtk_list_store_set(list, &it, COL_DIRPATH, new_text, -1);
}


gboolean edit_bookmarks( GtkWindow* parent )
{
    GList* l;
    GtkWidget* dlg;
    GtkWidget* btn_box;
    GtkWidget* add_btn;
    GtkWidget* delete_btn;
    GtkWidget* scroll;
    GtkWidget* list_view;
    GtkListStore* list;
    GtkTreeViewColumn* col;
    GtkTreeIter it;
    GtkTreeSelection* sel;
    gchar *name, *path, *item;
    gboolean ret = FALSE;
    PtkBookmarks* bookmarks;
    GtkCellRenderer *renderer, *icon_renderer;
    GdkPixbuf* icon;

    dlg = gtk_dialog_new_with_buttons ( _("Edit Bookmarks"),
                                        parent,
                                        GTK_DIALOG_MODAL,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OK, GTK_RESPONSE_OK,
                                        NULL );
    gtk_dialog_set_alternative_button_order( GTK_DIALOG(dlg), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1 );

    list = gtk_list_store_new( NUM_COLS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING );

    icon = gtk_icon_theme_load_icon( gtk_icon_theme_get_default(),
                                     "gnome-fs-directory",
                                     20, 0, NULL );
    bookmarks = ptk_bookmarks_get();
    for( l = bookmarks->list; l; l = l->next )
    {
        gtk_list_store_append( list, &it );
        gtk_list_store_set( list, &it,
                            COL_ICON, icon,
                            COL_NAME, l->data,
                            COL_DIRPATH, ptk_bookmarks_item_get_path((char*)l->data),
                            -1);
    }
    if( icon )
        g_object_unref( icon );

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(scroll),
                                         GTK_SHADOW_IN);

    list_view = gtk_tree_view_new_with_model( GTK_TREE_MODEL(list) );
    g_object_set_data( G_OBJECT(dlg), "list_view", list_view );

    sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(list_view) );
    gtk_tree_selection_set_mode( sel, GTK_SELECTION_BROWSE );

    if( gtk_tree_model_get_iter_first ( GTK_TREE_MODEL(list), &it ) )
    {
        gtk_tree_selection_select_iter( sel, &it );
    }

    icon_renderer = gtk_cell_renderer_pixbuf_new();
    renderer = gtk_cell_renderer_text_new();
    g_object_set( G_OBJECT(renderer), "editable", TRUE, NULL );
    g_signal_connect( renderer, "edited", G_CALLBACK(on_name_edited), list );
    col = gtk_tree_view_column_new_with_attributes(NULL, icon_renderer, "pixbuf", COL_ICON, NULL );
    gtk_tree_view_append_column( GTK_TREE_VIEW(list_view), col );

    col = gtk_tree_view_column_new_with_attributes(_("Name"), renderer, "text", COL_NAME, NULL);
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_column_set_fixed_width(col, 160);
    gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_append_column( GTK_TREE_VIEW(list_view), col );

    renderer = gtk_cell_renderer_text_new();
    g_object_set( G_OBJECT(renderer), "editable", TRUE, NULL );
    g_signal_connect( renderer, "edited", G_CALLBACK(on_path_edited), list );
    col = gtk_tree_view_column_new_with_attributes(_("Path"),
                                                   renderer, "text", COL_DIRPATH, NULL);
    gtk_tree_view_column_set_resizable(col, TRUE);
    gtk_tree_view_append_column( GTK_TREE_VIEW(list_view), col );

    gtk_tree_view_set_reorderable ( GTK_TREE_VIEW(list_view), TRUE );

    gtk_container_add( GTK_CONTAINER(scroll), list_view);

    btn_box = gtk_hbutton_box_new();
    gtk_button_box_set_layout ( GTK_BUTTON_BOX(btn_box), GTK_BUTTONBOX_START );
    add_btn = gtk_button_new_from_stock ( GTK_STOCK_ADD );
    g_signal_connect( add_btn, "clicked", G_CALLBACK(on_add), dlg );
    gtk_box_pack_start_defaults ( GTK_BOX(btn_box), add_btn );

    delete_btn = gtk_button_new_from_stock ( GTK_STOCK_DELETE );
    g_signal_connect( delete_btn, "clicked", G_CALLBACK(on_delete), dlg );
    gtk_box_pack_start_defaults ( GTK_BOX(btn_box), delete_btn );

    gtk_box_pack_start( GTK_BOX(GTK_DIALOG(dlg)->vbox), btn_box,
                        FALSE, FALSE, 4 );
    gtk_box_pack_start_defaults( GTK_BOX(GTK_DIALOG(dlg)->vbox), scroll );
    gtk_box_pack_start( GTK_BOX(GTK_DIALOG(dlg)->vbox),
                        gtk_label_new(_("Use drag & drop to sort the items")),
                        FALSE, FALSE, 4 );

    gtk_window_set_default_size ( GTK_WINDOW(dlg), 480, 400 );

    gtk_widget_show_all( dlg );
    gtk_widget_grab_focus( list_view );

    pcmanfm_ref();

    if( gtk_dialog_run( GTK_DIALOG(dlg) ) == GTK_RESPONSE_OK )
    {
        l = NULL;
        if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL(list), &it ) )
        {
            do
            {
                gtk_tree_model_get( GTK_TREE_MODEL(list), &it,
                                    COL_NAME, &name,
                                    COL_DIRPATH, &path,
                                    -1 );
                if( ! name )
                    name = g_path_get_basename( path );
                item = ptk_bookmarks_item_new( name, strlen(name),
                                               path ? path : "", path ? strlen(path) : 0 );
                l = g_list_append( l, item );
                g_free(name);
                g_free(path);
            }
            while( gtk_tree_model_iter_next( GTK_TREE_MODEL(list), &it) );
        }

        ptk_bookmarks_set( l );

        ret = TRUE;
    }

    ptk_bookmarks_unref();

    gtk_widget_destroy( dlg );
    pcmanfm_unref();

    return ret;
}

