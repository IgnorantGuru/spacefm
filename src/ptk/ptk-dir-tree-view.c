/*
*  C Implementation: ptkdirtreeview
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "ptk-dir-tree-view.h"
#include "ptk-file-icon-renderer.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include "glib-mem.h"

#include <string.h>

#include "ptk-dir-tree.h"
#include "ptk-file-menu.h"
#include "ptk-file-task.h"  //MOD

#include "vfs-file-info.h"
#include "vfs-file-monitor.h"

#include "gtk2-compat.h"

static GQuark dir_tree_view_data = 0;

static GtkTreeModel* get_dir_tree_model();

static void
on_dir_tree_view_row_expanded( GtkTreeView *treeview,
                               GtkTreeIter *iter,
                               GtkTreePath *path,
                               gpointer user_data );

static void
on_dir_tree_view_row_collapsed( GtkTreeView *treeview,
                                GtkTreeIter *iter,
                                GtkTreePath *path,
                                gpointer user_data );

static gboolean
on_dir_tree_view_button_press( GtkWidget* view,
                               GdkEventButton* evt,
                               PtkFileBrowser* browser );

static gboolean
on_dir_tree_view_key_press( GtkWidget* view,
                            GdkEventKey* evt,
                            gpointer user_data );

static gboolean sel_func ( GtkTreeSelection *selection,
                           GtkTreeModel *model,
                           GtkTreePath *path,
                           gboolean path_currently_selected,
                           gpointer data );

struct _DirTreeNode
{
    VFSFileInfo* file;
    GList* children;
    int n_children;
    VFSFileMonitor* monitor;
    int n_expand;
};

//MOD #if 0
/*  Drag & Drop/Clipboard targets  */
static GtkTargetEntry drag_targets[] =
    {
        { "text/uri-list", 0 , 0 }
    };
// #endif

//MOD drag n drop...
static void
on_dir_tree_view_drag_data_received ( GtkWidget *widget,
                            GdkDragContext *drag_context,
                            gint x,
                            gint y,
                            GtkSelectionData *sel_data,
                            guint info,
                            guint time,
                            gpointer user_data );
static gboolean
on_dir_tree_view_drag_motion ( GtkWidget *widget,
                            GdkDragContext *drag_context,
                            gint x,
                            gint y,
                            guint time,
                            PtkFileBrowser* file_browser );

static gboolean on_dir_tree_view_drag_leave ( GtkWidget *widget,
                            GdkDragContext *drag_context,
                            guint time,
                            PtkFileBrowser* file_browser );

static gboolean
on_dir_tree_view_drag_drop ( GtkWidget *widget,
                            GdkDragContext *drag_context,
                            gint x,
                            gint y,
                            guint time,
                            PtkFileBrowser* file_browser );

#define GDK_ACTION_ALL  (GDK_ACTION_MOVE|GDK_ACTION_COPY|GDK_ACTION_LINK)


static gboolean filter_func( GtkTreeModel *model,
                             GtkTreeIter *iter,
                             gpointer data )
{
    VFSFileInfo * file;
    const char* name;
    GtkTreeView* view = ( GtkTreeView* ) data;
    gboolean show_hidden = GPOINTER_TO_INT( g_object_get_qdata( G_OBJECT( view ),
                                                            dir_tree_view_data ) );

    if ( show_hidden )
        return TRUE;

    gtk_tree_model_get( model, iter, COL_DIR_TREE_INFO, &file, -1 );
    if ( G_LIKELY( file ) )
    {
        name = vfs_file_info_get_name( file );
        if ( G_UNLIKELY( name && name[ 0 ] == '.' ) )
        {
            vfs_file_info_unref( file );
            return FALSE;
        }
        vfs_file_info_unref( file );
    }
    return TRUE;
}
static void on_destroy(GtkWidget* w)
{
    do{
    }while( g_source_remove_by_user_data(w) );
}
/* Create a new dir tree view */
GtkWidget* ptk_dir_tree_view_new( PtkFileBrowser* browser,
                                  gboolean show_hidden )
{
    GtkTreeView * dir_tree_view;
    GtkTreeViewColumn* col;
    GtkCellRenderer* renderer;
    GtkTreeModel* model;
    GtkTreeSelection* tree_sel;
    GtkTreePath* tree_path;
    GtkTreeModel* filter;

    dir_tree_view = GTK_TREE_VIEW( gtk_tree_view_new () );
    gtk_tree_view_set_headers_visible( dir_tree_view, FALSE );
    gtk_tree_view_set_enable_tree_lines(dir_tree_view, TRUE);
    
//MOD enabled DND   FIXME: Temporarily disable drag & drop since it doesn't work right now.
/*    exo_icon_view_enable_model_drag_dest (
            EXO_ICON_VIEW( dir_tree_view ),
            drag_targets, G_N_ELEMENTS( drag_targets ), GDK_ACTION_ALL ); */
    gtk_tree_view_enable_model_drag_dest ( dir_tree_view,
                                           drag_targets,
                                           sizeof( drag_targets ) / sizeof( GtkTargetEntry ),
                                           GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK );
/*
    gtk_tree_view_enable_model_drag_source ( dir_tree_view,
                                             ( GDK_CONTROL_MASK | GDK_BUTTON1_MASK | GDK_BUTTON3_MASK ),
                                             drag_targets,
                                             sizeof( drag_targets ) / sizeof( GtkTargetEntry ),
                                             GDK_ACTION_DEFAULT | GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK );
  */  

    col = gtk_tree_view_column_new ();

    renderer = ( GtkCellRenderer* ) ptk_file_icon_renderer_new();
    gtk_tree_view_column_pack_start( col, renderer, FALSE );
    gtk_tree_view_column_set_attributes( col, renderer, "pixbuf", COL_DIR_TREE_ICON,
                                         "info", COL_DIR_TREE_INFO, NULL );
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start( col, renderer, TRUE );
    gtk_tree_view_column_set_attributes( col, renderer, "text", COL_DIR_TREE_DISP_NAME, NULL );

    gtk_tree_view_append_column ( dir_tree_view, col );

    tree_sel = gtk_tree_view_get_selection( dir_tree_view );
    gtk_tree_selection_set_select_function( tree_sel, sel_func, NULL, NULL );

    if ( G_UNLIKELY( !dir_tree_view_data ) )
        dir_tree_view_data = g_quark_from_static_string( "show_hidden" );
    g_object_set_qdata( G_OBJECT( dir_tree_view ),
                        dir_tree_view_data, GINT_TO_POINTER( show_hidden ) );
    model = get_dir_tree_model();
    filter = gtk_tree_model_filter_new( model, NULL );
    g_object_unref( G_OBJECT( model ) );
    gtk_tree_model_filter_set_visible_func( GTK_TREE_MODEL_FILTER( filter ),
                                            filter_func, dir_tree_view, NULL );
    gtk_tree_view_set_model( dir_tree_view, filter );
    g_object_unref( G_OBJECT( filter ) );

    g_signal_connect ( dir_tree_view, "row-expanded",
                       G_CALLBACK ( on_dir_tree_view_row_expanded ),
                       model );

    g_signal_connect_data ( dir_tree_view, "row-collapsed",
                            G_CALLBACK ( on_dir_tree_view_row_collapsed ),
                            model, NULL, G_CONNECT_AFTER );

    g_signal_connect ( dir_tree_view, "button-press-event",
                       G_CALLBACK ( on_dir_tree_view_button_press ),
                       browser );

    g_signal_connect ( dir_tree_view, "key-press-event",
                       G_CALLBACK ( on_dir_tree_view_key_press ),
                       NULL );

    //MOD drag n drop
    g_signal_connect ( ( gpointer ) dir_tree_view, "drag-data-received",
                       G_CALLBACK ( on_dir_tree_view_drag_data_received ),
                       browser );
    g_signal_connect ( ( gpointer ) dir_tree_view, "drag-motion",
                       G_CALLBACK ( on_dir_tree_view_drag_motion ),
                       browser );

    g_signal_connect ( ( gpointer ) dir_tree_view, "drag-leave",
                       G_CALLBACK ( on_dir_tree_view_drag_leave ),
                       browser );

    g_signal_connect ( ( gpointer ) dir_tree_view, "drag-drop",
                       G_CALLBACK ( on_dir_tree_view_drag_drop ),
                       browser );


    tree_path = gtk_tree_path_new_first();
    gtk_tree_view_expand_row( dir_tree_view, tree_path, FALSE );
    gtk_tree_path_free( tree_path );

    g_signal_connect( dir_tree_view, "destroy", G_CALLBACK(on_destroy), NULL );
    return GTK_WIDGET( dir_tree_view );
}

gboolean ptk_dir_tree_view_chdir( GtkTreeView* dir_tree_view, const char* path )
{
    GtkTreeModel * model;
    GtkTreeIter it, parent_it;
    GtkTreePath* tree_path = NULL;
    gchar **dirs, **dir;
    gboolean found;
    VFSFileInfo* info;

    if ( !path || *path != '/' )
        return FALSE;

    dirs = g_strsplit( path + 1, "/", -1 );

    if ( !dirs )
        return FALSE;

    model = gtk_tree_view_get_model( dir_tree_view );

    if ( ! gtk_tree_model_iter_children ( model, &parent_it, NULL ) )
    {
        g_strfreev( dirs );
        return FALSE;
    }

    /* special case: root dir */
    if ( ! dirs[ 0 ] )
    {
        it = parent_it;
        tree_path = gtk_tree_model_get_path ( model, &parent_it );
        goto _found;
    }

    for ( dir = dirs; *dir; ++dir )
    {
        if ( ! gtk_tree_model_iter_children ( model, &it, &parent_it ) )
        {
            g_strfreev( dirs );
            return FALSE;
        }
        found = FALSE;
        do
        {
            gtk_tree_model_get( model, &it, COL_DIR_TREE_INFO, &info, -1 );
            if ( !info )
                continue;
            if ( 0 == strcmp( vfs_file_info_get_name( info ), *dir ) )
            {
                tree_path = gtk_tree_model_get_path( model, &it );

                if( dir[1] ) {
                    gtk_tree_view_expand_row ( dir_tree_view, tree_path, FALSE );
                    gtk_tree_model_get_iter( model, &parent_it, tree_path );
                }
                found = TRUE;
                vfs_file_info_unref( info );
                break;
            }
            vfs_file_info_unref( info );
        }
        while ( gtk_tree_model_iter_next( model, &it ) );

        if ( ! found )
            return FALSE; /* Error! */

        if ( tree_path && dir[ 1 ] )
        {
            gtk_tree_path_free( tree_path );
            tree_path = NULL;
        }
    }
_found:
    g_strfreev( dirs );
    gtk_tree_selection_select_path (
        gtk_tree_view_get_selection( dir_tree_view ), tree_path );

    gtk_tree_view_scroll_to_cell ( dir_tree_view, tree_path, NULL, FALSE, 0.5, 0.5 );

    gtk_tree_path_free( tree_path );

    return TRUE;
}

/* FIXME: should this API be put here? Maybe it belongs to prk-dir-tree.c */
char* ptk_dir_view_get_dir_path( GtkTreeModel* model, GtkTreeIter* it )
{
    GtkTreeModel * tree;
    GtkTreeIter real_it;

    gtk_tree_model_filter_convert_iter_to_child_iter(
        GTK_TREE_MODEL_FILTER( model ), &real_it, it );
    tree = gtk_tree_model_filter_get_model( GTK_TREE_MODEL_FILTER( model ) );
    return ptk_dir_tree_get_dir_path( PTK_DIR_TREE( tree ), &real_it );
}

/* Return a newly allocated string containing path of current selected dir. */
char* ptk_dir_tree_view_get_selected_dir( GtkTreeView* dir_tree_view )
{
    GtkTreeModel * model;
    GtkTreeIter it;
    GtkTreeSelection* tree_sel;

    tree_sel = gtk_tree_view_get_selection( dir_tree_view );
    if ( gtk_tree_selection_get_selected( tree_sel, &model, &it ) )
        return ptk_dir_view_get_dir_path( model, &it );
    return NULL;
}

GtkTreeModel* get_dir_tree_model()
{
    static PtkDirTree * dir_tree_model = NULL;

    if ( G_UNLIKELY( ! dir_tree_model ) )
    {
        dir_tree_model = ptk_dir_tree_new( TRUE );
        g_object_add_weak_pointer( G_OBJECT( dir_tree_model ),
                                   ( gpointer * ) (GtkWidget *) & dir_tree_model );
    }
    else
    {
        g_object_ref( G_OBJECT( dir_tree_model ) );
    }
    return GTK_TREE_MODEL( dir_tree_model );
}

gboolean sel_func ( GtkTreeSelection *selection,
                    GtkTreeModel *model,
                    GtkTreePath *path,
                    gboolean path_currently_selected,
                    gpointer data )
{
    GtkTreeIter it;
    VFSFileInfo* file;

    if ( ! gtk_tree_model_get_iter( model, &it, path ) )
        return FALSE;
    gtk_tree_model_get( model, &it, COL_DIR_TREE_INFO, &file, -1 );
    if ( !file )
        return FALSE;
    vfs_file_info_unref( file );
    return TRUE;
}

void ptk_dir_tree_view_show_hidden_files( GtkTreeView* dir_tree_view,
                                          gboolean show_hidden )
{
    GtkTreeModel * filter;
    g_object_set_qdata( G_OBJECT( dir_tree_view ),
                        dir_tree_view_data, GINT_TO_POINTER( show_hidden ) );
    filter = gtk_tree_view_get_model( dir_tree_view );
    gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER( filter ) );
}

void on_dir_tree_view_row_expanded( GtkTreeView *treeview,
                                    GtkTreeIter *iter,
                                    GtkTreePath *path,
                                    gpointer user_data )
{
    GtkTreeIter real_it;
    GtkTreePath* real_path;
    GtkTreeModel* filter = gtk_tree_view_get_model( treeview );
    PtkDirTree* tree = PTK_DIR_TREE( user_data );
    gtk_tree_model_filter_convert_iter_to_child_iter(
        GTK_TREE_MODEL_FILTER( filter ), &real_it, iter );
    real_path = gtk_tree_model_filter_convert_path_to_child_path(
                    GTK_TREE_MODEL_FILTER( filter ), path );
    ptk_dir_tree_expand_row( tree, &real_it, real_path );
    gtk_tree_path_free( real_path );
}

void on_dir_tree_view_row_collapsed( GtkTreeView *treeview,
                                     GtkTreeIter *iter,
                                     GtkTreePath *path,
                                     gpointer user_data )
{
    GtkTreeIter real_it;
    GtkTreePath* real_path;
    GtkTreeModel* filter = gtk_tree_view_get_model( treeview );
    PtkDirTree* tree = PTK_DIR_TREE( user_data );
    gtk_tree_model_filter_convert_iter_to_child_iter(
        GTK_TREE_MODEL_FILTER( filter ), &real_it, iter );
    real_path = gtk_tree_model_filter_convert_path_to_child_path(
                    GTK_TREE_MODEL_FILTER( filter ), path );
    ptk_dir_tree_collapse_row( tree, &real_it, real_path );
    gtk_tree_path_free( real_path );
}

gboolean on_dir_tree_view_button_press( GtkWidget* view,
                                        GdkEventButton* evt,
                                        PtkFileBrowser* browser )
{
    if ( evt->type == GDK_BUTTON_PRESS && evt->button == 3 )
    {
        GtkTreeModel * model;
        GtkTreePath* tree_path;
        GtkTreeIter it;

        model = gtk_tree_view_get_model( GTK_TREE_VIEW( view ) );
        if ( gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW( view ),
                                            evt->x, evt->y, &tree_path, NULL, NULL, NULL ) )
        {
            if ( gtk_tree_model_get_iter( model, &it, tree_path ) )
            {
                VFSFileInfo * file;
                gtk_tree_model_get( model, &it,
                                    COL_DIR_TREE_INFO,
                                    &file, -1 );
                if ( file )
                {
                    GtkWidget * popup;
                    char* file_path;
                    GList* sel_files;
                    char* dir_name;
                    file_path = ptk_dir_view_get_dir_path( model, &it );

                    sel_files = g_list_prepend( NULL, vfs_file_info_ref(file) );
                    dir_name = g_path_get_dirname( file_path );
                    popup = ptk_file_menu_new( NULL, browser,
                                file_path, file,
                                dir_name, sel_files );
                    g_free( dir_name );
                    g_free( file_path );
                    if ( popup )
                        gtk_menu_popup( GTK_MENU( popup ), NULL, NULL,
                                    NULL, NULL, 3, evt->time );

                    vfs_file_info_unref( file );
                }
            }
            gtk_tree_path_free( tree_path );
        }
    }
    return FALSE;
}

gboolean on_dir_tree_view_key_press( GtkWidget* view,
                                     GdkEventKey* evt,
                                     gpointer user_data )
{
    switch(evt->keyval) {
    case GDK_Left:
    case GDK_Right:
        break;
    default:
        return FALSE;
    }


    GtkTreeSelection *select = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    GtkTreeModel *model;
    GtkTreeIter iter;
    if(!gtk_tree_selection_get_selected(select, &model, &iter))
        return FALSE;

    GtkTreePath *path = gtk_tree_model_get_path(model, &iter);

    switch( evt->keyval ) {
    case GDK_Left:
        if(gtk_tree_view_row_expanded(GTK_TREE_VIEW(view), path)) {
            gtk_tree_view_collapse_row(GTK_TREE_VIEW(view), path);
        } else if(gtk_tree_path_up(path)) {
            gtk_tree_selection_select_path(select, path);
            gtk_tree_view_set_cursor(GTK_TREE_VIEW(view), path, NULL, FALSE);
        } else {
            return FALSE;
        }
        break;
    case GDK_Right:
        if(!gtk_tree_view_row_expanded(GTK_TREE_VIEW(view), path)) {
            gtk_tree_view_expand_row(GTK_TREE_VIEW(view), path, FALSE);
        } else {
            gtk_tree_path_down(path);
            gtk_tree_selection_select_path(select, path);
            gtk_tree_view_set_cursor(GTK_TREE_VIEW(view), path, NULL, FALSE);
        }
        break;
    }
    return TRUE;  
}

//MOD drag n drop
static char* dir_tree_view_get_drop_dir( GtkWidget* view, int x, int y )
{
    GtkTreePath *tree_path = NULL;
    GtkTreeModel *model;
    GtkTreeIter it;
    VFSFileInfo* file;
    char* dest_path = NULL;

    // if drag is in progress, get the dest row path
    gtk_tree_view_get_drag_dest_row( GTK_TREE_VIEW( view ), &tree_path, NULL );
    if ( !tree_path )
    {
        // no drag in progress, get drop path
        if ( !gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW( view ),
                                        x, y, &tree_path, NULL, NULL, NULL ) )
            tree_path = NULL;
    }
    if ( tree_path )
    {
        model = gtk_tree_view_get_model( GTK_TREE_VIEW( view ) );
        if ( gtk_tree_model_get_iter( model, &it, tree_path ) )
        {
            gtk_tree_model_get( model, &it,
                                COL_DIR_TREE_INFO,
                                &file, -1 );
            if ( file )
            {
                dest_path = ptk_dir_view_get_dir_path( model, &it );
                vfs_file_info_unref( file );
            }
        }
        gtk_tree_path_free( tree_path );
    }
/*  this isn't needed?
    // dest_path is a link? resolve
    if ( dest_path && g_file_test( dest_path, G_FILE_TEST_IS_SYMLINK ) )
    {
        char* old_dest = dest_path;
        dest_path = g_file_read_link( old_dest, NULL );
        g_free( old_dest );
    }
*/
    return dest_path;
}

void on_dir_tree_view_drag_data_received ( GtkWidget *widget,
                                         GdkDragContext *drag_context,
                                         gint x,
                                         gint y,
                                         GtkSelectionData *sel_data,
                                         guint info,
                                         guint time,
                                         gpointer user_data )  //MOD added
{
    gchar **list, **puri;
    GList* files = NULL;
    PtkFileTask* task;
    VFSFileTaskType file_action = VFS_FILE_TASK_MOVE;
    PtkFileBrowser* file_browser = ( PtkFileBrowser* ) user_data;
    char* dest_dir;
    char* file_path;
    GtkWidget* parent_win;

    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name( widget, "drag-data-received" );

    if ( ( gtk_selection_data_get_length( sel_data ) >= 0 ) && ( gtk_selection_data_get_format( sel_data ) == 8 ) )
    {
        dest_dir = dir_tree_view_get_drop_dir( widget, x, y );
        if ( dest_dir )
        {
            puri = list = gtk_selection_data_get_uris( sel_data );
            if( file_browser->pending_drag_status_tree )
            {
                // We only want to update drag status, not really want to drop
                dev_t dest_dev;
                struct stat statbuf;    // skip stat64
                if( stat( dest_dir, &statbuf ) == 0 )
                {
                    dest_dev = statbuf.st_dev;
                    if( 0 == file_browser->drag_source_dev_tree )
                    {
                        file_browser->drag_source_dev_tree = dest_dev;
                        for( ; *puri; ++puri )
                        {
                            file_path = g_filename_from_uri( *puri, NULL, NULL );
                            if( stat( file_path, &statbuf ) == 0 && statbuf.st_dev != dest_dev )
                            {
                                file_browser->drag_source_dev_tree = statbuf.st_dev;
                                g_free( file_path );
                                break;
                            }
                            g_free( file_path );
                        }
                    }
                    if( file_browser->drag_source_dev_tree != dest_dev )
                        // src and dest are on different devices */
                        gdk_drag_status( drag_context, GDK_ACTION_COPY, time);
                    else
                        gdk_drag_status( drag_context, GDK_ACTION_MOVE, time);
                }
                else
                    // stat failed
                    gdk_drag_status( drag_context, GDK_ACTION_COPY, time);

                g_free( dest_dir );
                g_strfreev( list );
                file_browser->pending_drag_status_tree = 0;
                return;
            }

            if ( puri )
            {
                if ( 0 == ( gdk_drag_context_get_selected_action ( drag_context ) &
                            ( GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK ) ) )
                {
                    gdk_drag_status( drag_context, GDK_ACTION_MOVE, time);
                }
                gtk_drag_finish ( drag_context, TRUE, FALSE, time );

                while ( *puri )
                {
                    if ( **puri == '/' )
                        file_path = g_strdup( *puri );
                    else
                        file_path = g_filename_from_uri( *puri, NULL, NULL );

                    if ( file_path )
                        files = g_list_prepend( files, file_path );
                    ++puri;
                }
                g_strfreev( list );

                switch ( gdk_drag_context_get_selected_action ( drag_context ) )
                {
                case GDK_ACTION_COPY:
                    file_action = VFS_FILE_TASK_COPY;
                    break;
                case GDK_ACTION_LINK:
                    file_action = VFS_FILE_TASK_LINK;
                    break;
                    /* FIXME:
                      GDK_ACTION_DEFAULT, GDK_ACTION_PRIVATE, and GDK_ACTION_ASK are not handled */
                default:
                    break;
                }
                if ( files )
                {
                    /* Accept the drop and perform file actions */
                    {
                        parent_win = gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) );
                        task = ptk_file_task_new( file_action,
                                                  files,
                                                  dest_dir,
                                                  GTK_WINDOW( parent_win ),
                                                  file_browser->task_view );
                        ptk_file_task_run( task );
                    }
                }
                g_free( dest_dir );
                gtk_drag_finish ( drag_context, TRUE, FALSE, time );
                return ;
            }
            g_free( dest_dir );
        }
        //else
        //    g_warning ("bad dest_dir in on_dir_tree_view_drag_data_received");
    }
    /* If we are only getting drag status, not finished. */
    if( file_browser->pending_drag_status_tree )
    {
        gdk_drag_status ( drag_context, GDK_ACTION_COPY, time );
        file_browser->pending_drag_status_tree = 0;
        return;
    }
    gtk_drag_finish ( drag_context, FALSE, FALSE, time );
}


gboolean on_dir_tree_view_drag_drop ( GtkWidget *widget,
                                    GdkDragContext *drag_context,
                                    gint x,
                                    gint y,
                                    guint time,
                                    PtkFileBrowser* file_browser )  //MOD added
{
    GdkAtom target = gdk_atom_intern( "text/uri-list", FALSE );

    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name( widget, "drag-drop" );

    gtk_drag_get_data ( widget, drag_context, target, time );
    return TRUE;
}

gboolean on_dir_tree_view_drag_motion ( GtkWidget *widget,
                                      GdkDragContext *drag_context,
                                      gint x,
                                      gint y,
                                      guint time,
                                      PtkFileBrowser* file_browser )  //MOD added
{
    GdkDragAction suggested_action;
    GdkAtom target;
    GtkTargetList* target_list;

    target_list = gtk_target_list_new( drag_targets, G_N_ELEMENTS(drag_targets) );
    target = gtk_drag_dest_find_target( widget, drag_context, target_list );
    gtk_target_list_unref( target_list );

    if (target == GDK_NONE)
        gdk_drag_status( drag_context, 0, time);
    else
    {
        // Need to set suggested_action because default handler assumes copy
        /* Only 'move' is available. The user force move action by pressing Shift key */
        if( (gdk_drag_context_get_actions ( drag_context ) & GDK_ACTION_ALL) == GDK_ACTION_MOVE )
            suggested_action = GDK_ACTION_MOVE;
        /* Only 'copy' is available. The user force copy action by pressing Ctrl key */
        else if( (gdk_drag_context_get_actions ( drag_context ) & GDK_ACTION_ALL) == GDK_ACTION_COPY )
            suggested_action = GDK_ACTION_COPY;
        /* Only 'link' is available. The user force link action by pressing Shift+Ctrl key */
        else if( (gdk_drag_context_get_actions ( drag_context ) & GDK_ACTION_ALL) == GDK_ACTION_LINK )
            suggested_action = GDK_ACTION_LINK;
        /* Several different actions are available. We have to figure out a good default action. */
        else
        {
            int drag_action = xset_get_int( "drag_action", "x" );
            if ( drag_action == 1 )
                suggested_action = GDK_ACTION_COPY;
            else if ( drag_action == 2 )
                suggested_action = GDK_ACTION_MOVE;
            else if ( drag_action == 3 )
                suggested_action = GDK_ACTION_LINK;
            else
            {
                // automatic
                file_browser->pending_drag_status_tree = 1;
                gtk_drag_get_data (widget, drag_context, target, time);
                suggested_action = gdk_drag_context_get_suggested_action( drag_context );
            }
        }
        gdk_drag_status( drag_context, suggested_action, gtk_get_current_event_time() );
    }
    return FALSE;
}

static gboolean on_dir_tree_view_drag_leave ( GtkWidget *widget,
                                     GdkDragContext *drag_context,
                                     guint time,
                                     PtkFileBrowser* file_browser )
{
    file_browser->drag_source_dev_tree = 0;
    return FALSE;
}
