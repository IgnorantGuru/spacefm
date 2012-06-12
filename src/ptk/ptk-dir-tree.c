/*
*  C Implementation: ptk-dir-tree
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "ptk-dir-tree.h"
#include <gdk/gdk.h>
#include <glib/gi18n.h>

#include <string.h>

#include "vfs-file-info.h"
#include "vfs-file-monitor.h"
#include "glib-mem.h"

struct _PtkDirTreeNode
{
    VFSFileInfo* file;
    PtkDirTreeNode* children;
    int n_children;
    VFSFileMonitor* monitor;
    int n_expand;
    PtkDirTreeNode* parent;
    PtkDirTreeNode* next;
    PtkDirTreeNode* prev;
    PtkDirTreeNode* last;
    PtkDirTree* tree;   /* FIXME: This is a waste of memory :-( */
};

static void ptk_dir_tree_init ( PtkDirTree *tree );

static void ptk_dir_tree_class_init ( PtkDirTreeClass *klass );

static void ptk_dir_tree_tree_model_init ( GtkTreeModelIface *iface );

static void ptk_dir_tree_drag_source_init ( GtkTreeDragSourceIface *iface );

static void ptk_dir_tree_drag_dest_init ( GtkTreeDragDestIface *iface );

static void ptk_dir_tree_finalize ( GObject *object );

static GtkTreeModelFlags ptk_dir_tree_get_flags ( GtkTreeModel *tree_model );

static gint ptk_dir_tree_get_n_columns ( GtkTreeModel *tree_model );

static GType ptk_dir_tree_get_column_type ( GtkTreeModel *tree_model,
                                            gint index );

static gboolean ptk_dir_tree_get_iter ( GtkTreeModel *tree_model,
                                        GtkTreeIter *iter,
                                        GtkTreePath *path );

static GtkTreePath *ptk_dir_tree_get_path ( GtkTreeModel *tree_model,
                                            GtkTreeIter *iter );

static void ptk_dir_tree_get_value ( GtkTreeModel *tree_model,
                                     GtkTreeIter *iter,
                                     gint column,
                                     GValue *value );

static gboolean ptk_dir_tree_iter_next ( GtkTreeModel *tree_model,
                                         GtkTreeIter *iter );

static gboolean ptk_dir_tree_iter_children ( GtkTreeModel *tree_model,
                                             GtkTreeIter *iter,
                                             GtkTreeIter *parent );

static gboolean ptk_dir_tree_iter_has_child ( GtkTreeModel *tree_model,
                                              GtkTreeIter *iter );

static gint ptk_dir_tree_iter_n_children ( GtkTreeModel *tree_model,
                                           GtkTreeIter *iter );

static gboolean ptk_dir_tree_iter_nth_child ( GtkTreeModel *tree_model,
                                              GtkTreeIter *iter,
                                              GtkTreeIter *parent,
                                              gint n );

static gboolean ptk_dir_tree_iter_parent ( GtkTreeModel *tree_model,
                                           GtkTreeIter *iter,
                                           GtkTreeIter *child );

static gint ptk_dir_tree_node_compare( PtkDirTree* tree,
                                       PtkDirTreeNode* a,
                                       PtkDirTreeNode* b );

static void ptk_dir_tree_insert_child( PtkDirTree* tree,
                                       PtkDirTreeNode* parent,
                                       const char* file_path,
                                       const char* name );

static void ptk_dir_tree_delete_child( PtkDirTree* tree,
                                       PtkDirTreeNode* child );
/* signal handlers */

static void on_file_monitor_event ( VFSFileMonitor* fm,
                                    VFSFileMonitorEvent event,
                                    const char* file_name,
                                    gpointer user_data );

static PtkDirTreeNode* ptk_dir_tree_node_new( PtkDirTree* tree,
                                              PtkDirTreeNode* parent,
                                              const char* path,
                                              const char* base_name );

static void ptk_dir_tree_node_free( PtkDirTreeNode* node );

static GObjectClass* parent_class = NULL;

static GType column_types[ N_DIR_TREE_COLS ];

GType ptk_dir_tree_get_type ( void )
{
    static GType type = 0;
    if ( G_UNLIKELY( !type ) )
    {
        static const GTypeInfo type_info =
            {
                sizeof ( PtkDirTreeClass ),
                NULL,                                           /* base_init */
                NULL,                                           /* base_finalize */
                ( GClassInitFunc ) ptk_dir_tree_class_init,
                NULL,                                           /* class finalize */
                NULL,                                           /* class_data */
                sizeof ( PtkDirTree ),
                0,                                             /* n_preallocs */
                ( GInstanceInitFunc ) ptk_dir_tree_init
            };

        static const GInterfaceInfo tree_model_info =
            {
                ( GInterfaceInitFunc ) ptk_dir_tree_tree_model_init,
                NULL,
                NULL
            };

        static const GInterfaceInfo drag_src_info =
            {
                ( GInterfaceInitFunc ) ptk_dir_tree_drag_source_init,
                NULL,
                NULL
            };

        static const GInterfaceInfo drag_dest_info =
            {
                ( GInterfaceInitFunc ) ptk_dir_tree_drag_dest_init,
                NULL,
                NULL
            };

        type = g_type_register_static ( G_TYPE_OBJECT, "PtkDirTree",
                                        &type_info, ( GTypeFlags ) 0 );
        g_type_add_interface_static ( type, GTK_TYPE_TREE_MODEL, &tree_model_info );
        g_type_add_interface_static ( type, GTK_TYPE_TREE_DRAG_SOURCE, &drag_src_info );
        g_type_add_interface_static ( type, GTK_TYPE_TREE_DRAG_DEST, &drag_dest_info );
    }
    return type;
}

void ptk_dir_tree_init ( PtkDirTree *tree )
{
    PtkDirTreeNode* child;
    tree->root = g_slice_new0( PtkDirTreeNode );
    tree->root->tree = tree;
    tree->root->n_children = 1;
    child = ptk_dir_tree_node_new( tree, tree->root, "/", "/" );
    vfs_file_info_set_disp_name( child->file, _("File System") );
    tree->root->children = child;

    /*
    child = ptk_dir_tree_node_new( tree, tree->root, g_get_home_dir(), NULL );
    vfs_file_info_set_name( child->file, g_get_home_dir() );
    tree->root->children->next = child;
    */

    /* Random int to check whether an iter belongs to our model */
    tree->stamp = g_random_int();
}

void ptk_dir_tree_class_init ( PtkDirTreeClass *klass )
{
    GObjectClass * object_class;

    parent_class = ( GObjectClass* ) g_type_class_peek_parent ( klass );
    object_class = ( GObjectClass* ) klass;

    object_class->finalize = ptk_dir_tree_finalize;
}

void ptk_dir_tree_tree_model_init ( GtkTreeModelIface *iface )
{
    iface->get_flags = ptk_dir_tree_get_flags;
    iface->get_n_columns = ptk_dir_tree_get_n_columns;
    iface->get_column_type = ptk_dir_tree_get_column_type;
    iface->get_iter = ptk_dir_tree_get_iter;
    iface->get_path = ptk_dir_tree_get_path;
    iface->get_value = ptk_dir_tree_get_value;
    iface->iter_next = ptk_dir_tree_iter_next;
    iface->iter_children = ptk_dir_tree_iter_children;
    iface->iter_has_child = ptk_dir_tree_iter_has_child;
    iface->iter_n_children = ptk_dir_tree_iter_n_children;
    iface->iter_nth_child = ptk_dir_tree_iter_nth_child;
    iface->iter_parent = ptk_dir_tree_iter_parent;

    column_types [ COL_DIR_TREE_ICON ] = GDK_TYPE_PIXBUF;
    column_types [ COL_DIR_TREE_DISP_NAME ] = G_TYPE_STRING;
    column_types [ COL_DIR_TREE_INFO ] = G_TYPE_POINTER;
}

void ptk_dir_tree_drag_source_init ( GtkTreeDragSourceIface *iface )
{
    /* FIXME: Unused. Will this cause any problem? */
}

void ptk_dir_tree_drag_dest_init ( GtkTreeDragDestIface *iface )
{
    /* FIXME: Unused. Will this cause any problem? */
}

void ptk_dir_tree_finalize ( GObject *object )
{
    PtkDirTree *tree = ( PtkDirTree* ) object;

    if( tree->root )
        ptk_dir_tree_node_free( tree->root );

    /* must chain up - finalize parent */
    ( * parent_class->finalize ) ( object );
}

PtkDirTree *ptk_dir_tree_new ()
{
    PtkDirTree * tree;
    tree = ( PtkDirTree* ) g_object_new ( PTK_TYPE_DIR_TREE, NULL );
    return tree;
}

GtkTreeModelFlags ptk_dir_tree_get_flags ( GtkTreeModel *tree_model )
{
    g_return_val_if_fail ( PTK_IS_DIR_TREE( tree_model ), ( GtkTreeModelFlags ) 0 );
    return GTK_TREE_MODEL_ITERS_PERSIST;
}

gint ptk_dir_tree_get_n_columns ( GtkTreeModel *tree_model )
{
    return N_DIR_TREE_COLS;
}

GType ptk_dir_tree_get_column_type ( GtkTreeModel *tree_model,
                                     gint index )
{
    g_return_val_if_fail ( PTK_IS_DIR_TREE( tree_model ), G_TYPE_INVALID );
    g_return_val_if_fail ( index < G_N_ELEMENTS( column_types ) && index >= 0, G_TYPE_INVALID );
    return column_types[ index ];
}

static PtkDirTreeNode* get_nth_node( PtkDirTreeNode* parent, int n )
{
    PtkDirTreeNode* node;
    if ( n >= parent->n_children || n < 0 )
        return NULL;

    node = parent->children;
    while( n > 0 && node )
    {
        node = node->next;
        --n;
    }
    return node;
}

gboolean ptk_dir_tree_get_iter ( GtkTreeModel *tree_model,
                                 GtkTreeIter *iter,
                                 GtkTreePath *path )
{
    PtkDirTree *tree;
    gint *indices, i, depth;
    PtkDirTreeNode* node;
    g_assert(PTK_IS_DIR_TREE(tree_model));
    g_assert(path!=NULL);

    tree = PTK_DIR_TREE(tree_model);
    if( !tree || !tree->root )
        return FALSE;

    indices = gtk_tree_path_get_indices(path);
    depth   = gtk_tree_path_get_depth(path);

    node = tree->root;
    for( i = 0; i < depth; ++i )
    {
        node = get_nth_node( node, indices[i] );
        if( !node )
            return FALSE;
    }

    /* We simply store a pointer in the iter */
    iter->stamp = tree->stamp;
    iter->user_data  = node;
    iter->user_data2 = NULL;
    iter->user_data3 = NULL;   /* unused */

    return TRUE;
}

static int get_node_index( PtkDirTreeNode* parent, PtkDirTreeNode* child )
{
    PtkDirTreeNode* node;
    int i;
    if( !parent || !child )
        return -1;

    for( i = 0, node = parent->children; node; node = node->next )
    {
        if( node == child )
        {
            return i;
        }
        ++i;
    }
    return -1;
}

GtkTreePath *ptk_dir_tree_get_path ( GtkTreeModel *tree_model,
                                     GtkTreeIter *iter )
{
    PtkDirTreeNode* node;
    GtkTreePath* path;
    int i;
    PtkDirTree* tree = PTK_DIR_TREE(tree_model);

    g_return_val_if_fail (tree, NULL);
    g_return_val_if_fail (iter->stamp == tree->stamp, NULL);
    g_return_val_if_fail (iter != NULL, NULL);
    g_return_val_if_fail (iter->user_data != NULL, NULL);

    path = gtk_tree_path_new();
    node = (PtkDirTreeNode*)iter->user_data;
    g_return_val_if_fail( node->parent != NULL, (GtkTreePath *)(-1) );

    while( node != tree->root )
    {
        i = get_node_index( node->parent, node );
        if( i == -1 )
        {
            gtk_tree_path_free( path );
            return NULL;
        }
        gtk_tree_path_prepend_index( path, i );
        node = node->parent;
    }
    return path;
}

void ptk_dir_tree_get_value ( GtkTreeModel *tree_model,
                              GtkTreeIter *iter,
                              gint column,
                              GValue *value )
{
    VFSFileInfo* info;
    GdkPixbuf* icon;
    PtkDirTreeNode* node;

    g_return_if_fail (PTK_IS_DIR_TREE (tree_model));
    g_return_if_fail (iter != NULL);
    g_return_if_fail (column < G_N_ELEMENTS(column_types) );

    g_value_init (value, column_types[column] );

    node = (PtkDirTreeNode*) iter->user_data;
    g_return_if_fail ( node != NULL );
    info = node->file;
    switch(column)
    {
    case COL_DIR_TREE_ICON:
        if( G_UNLIKELY( !info ) )
            return;
        icon = vfs_file_info_get_small_icon( info );
        if( icon )
        {
            g_value_set_object( value, icon );
            g_object_unref( icon );
        }
        break;
    case COL_DIR_TREE_DISP_NAME:
        if( G_LIKELY( info ) )
            g_value_set_string( value, vfs_file_info_get_disp_name(info) );
        else
            g_value_set_string( value, _("( no subfolder )") );  // no sub folder
        break;
    case COL_DIR_TREE_INFO:
        if( G_UNLIKELY( !info ) )
            return;
        g_value_set_pointer( value, vfs_file_info_ref( info ) );
        break;
    }
}

gboolean ptk_dir_tree_iter_next ( GtkTreeModel *tree_model,
                                  GtkTreeIter *iter )
{
    PtkDirTreeNode* node;
    PtkDirTree* tree;

    g_return_val_if_fail (PTK_IS_DIR_TREE (tree_model), FALSE);

    if (iter == NULL || iter->user_data == NULL)
        return FALSE;

    tree = PTK_DIR_TREE(tree_model);
    node = (PtkDirTreeNode *) iter->user_data;

    /* Is this the last child in the parent node? */
    if ( ! node->next )
        return FALSE;

    iter->stamp = tree->stamp;
    iter->user_data = node->next;
    iter->user_data2 = NULL;
    iter->user_data3 = NULL;

    return TRUE;
}

gboolean ptk_dir_tree_iter_children ( GtkTreeModel *tree_model,
                                      GtkTreeIter *iter,
                                      GtkTreeIter *parent )
{
    PtkDirTree* tree;
    PtkDirTreeNode* parent_node;
    g_return_val_if_fail ( parent == NULL || parent->user_data != NULL, FALSE );

    g_return_val_if_fail ( PTK_IS_DIR_TREE ( tree_model ), FALSE );
    tree = PTK_DIR_TREE( tree_model );

    if ( parent )
        parent_node = (PtkDirTreeNode*)parent->user_data;
    else
    {
        /* parent == NULL is a special case; we need to return the first top-level row */
        parent_node = tree->root;
    }
    /* No rows => no first row */
    if ( parent_node->n_children == 0 )
        return FALSE;

    /* Set iter to first item in tree */
    iter->stamp = tree->stamp;
    iter->user_data = parent_node->children;
    iter->user_data2 = iter->user_data3 = NULL;

    return TRUE;
}

gboolean ptk_dir_tree_iter_has_child ( GtkTreeModel *tree_model,
                                       GtkTreeIter *iter )
{
    PtkDirTreeNode* node;
    g_return_val_if_fail( iter != NULL, FALSE );
    node = (PtkDirTreeNode*)iter->user_data;
    return node->n_children != 0;
}

gint ptk_dir_tree_iter_n_children ( GtkTreeModel *tree_model,
                                    GtkTreeIter *iter )
{
    PtkDirTreeNode* node;
    PtkDirTree* tree;
    g_return_val_if_fail ( PTK_IS_DIR_TREE ( tree_model ), -1 );

    tree = PTK_DIR_TREE( tree_model );

    /* special case: if iter == NULL, return number of top-level rows */
    if ( !iter )
        node = tree->root;
    else
        node = (PtkDirTreeNode*)iter->user_data;
    g_return_val_if_fail ( node != NULL, -1 );
    return node->n_children;
}

gboolean ptk_dir_tree_iter_nth_child ( GtkTreeModel *tree_model,
                                       GtkTreeIter *iter,
                                       GtkTreeIter *parent,
                                       gint n )
{
    PtkDirTreeNode* parent_node;
    PtkDirTreeNode* node;
    PtkDirTree* tree;

    g_return_val_if_fail (PTK_IS_DIR_TREE (tree_model), FALSE);
    tree = PTK_DIR_TREE(tree_model);

    if( G_LIKELY(parent) )
    {
        parent_node = ( PtkDirTreeNode* )parent->user_data;
        g_return_val_if_fail( parent_node, FALSE );
    }
    else
    {
        /* special case: if parent == NULL, set iter to n-th top-level row */
        parent_node = tree->root;
    }

    if( n >= parent_node->n_children || n < 0 )
        return FALSE;

    node = get_nth_node( parent_node, n );

    iter->stamp = tree->stamp;
    iter->user_data = node;
    iter->user_data2 = iter->user_data3 = NULL;

    return TRUE;
}

gboolean ptk_dir_tree_iter_parent ( GtkTreeModel *tree_model,
                                    GtkTreeIter *iter,
                                    GtkTreeIter *child )
{
    PtkDirTreeNode* node;
    PtkDirTree* tree;
    g_return_val_if_fail( iter != NULL && child != NULL, FALSE );

    tree = PTK_DIR_TREE( tree_model );
    node = (PtkDirTreeNode*)child->user_data;
    if( G_LIKELY( node->parent != tree->root ) )
    {
        iter->user_data = node->parent;
        iter->user_data2 = iter->user_data3 = NULL;
        return TRUE;
    }
    return FALSE;
}

gint ptk_dir_tree_node_compare( PtkDirTree* tree,
                                PtkDirTreeNode* a,
                                PtkDirTreeNode* b )
{
    VFSFileInfo* file1 = a->file;
    VFSFileInfo* file2 = b->file;
    int ret;

    if( ! file1 || !file2 )
        return 0;
    /* FIXME: UTF-8 strings should not be treated as ASCII when sorted  */
    ret = g_ascii_strcasecmp( vfs_file_info_get_disp_name(file2),
                              vfs_file_info_get_disp_name(file1) );
    return ret;
}

PtkDirTreeNode* ptk_dir_tree_node_new( PtkDirTree* tree,
                                       PtkDirTreeNode* parent,
                                       const char* path,
                                       const char* base_name )
{
    PtkDirTreeNode* node;
    node = g_slice_new0( PtkDirTreeNode );
    node->tree = tree;
    node->parent = parent;
    if( path )
    {
        node->file = vfs_file_info_new();
        vfs_file_info_get( node->file, path, base_name );
        node->n_children = 1;
        node->children = ptk_dir_tree_node_new( tree, node, NULL, NULL );
        node->last = node->children;
    }
    return node;
}

void ptk_dir_tree_node_free( PtkDirTreeNode* node )
{
    PtkDirTreeNode* child;
    if( node->file )
        vfs_file_info_unref( node->file );
    for( child = node->children; child; child = child->next )
        ptk_dir_tree_node_free( child );
    if( node->monitor )
    {
        vfs_file_monitor_remove( node->monitor,
                                 &on_file_monitor_event,
                                 node );
    }
    g_slice_free( PtkDirTreeNode, node );
}

static char* dir_path_from_tree_node( PtkDirTree* tree, PtkDirTreeNode* node )
{
    GSList* names = NULL, *l;
    const char* name;
    int len;
    char* dir_path, *p;

    if( !node )
        return NULL;

    while( node != tree->root )
    {
        if( !node->file || ! (name = vfs_file_info_get_name( node->file )) )
        {
            g_slist_free( names );
            return NULL;
        }
        names = g_slist_prepend( names, (gpointer) name );
        node = node->parent;
    }
    for( len = 1, l = names; l; l = l->next )
        len += strlen((char*)l->data) + 1;
    dir_path = g_malloc( len );

    for( p = dir_path, l = names; l; l = l->next )
    {
        name = (char*)l->data;
        len = strlen( name );
        memcpy( p, name, len * sizeof(char) );
        p += len;
        if( l->next && strcmp( name, "/" ) )
        {
            *p = '/';
            ++p;
        }
    }
    *p = '\0';
    g_slist_free( names );
    return dir_path;
}

void ptk_dir_tree_insert_child( PtkDirTree* tree,
                                PtkDirTreeNode* parent,
                                const char* file_path,
                                const char* name )
{
    PtkDirTreeNode *child_node;
    PtkDirTreeNode *node;
    GtkTreeIter it;
    GtkTreePath* tree_path;

    child_node = ptk_dir_tree_node_new( tree, parent, file_path, name );
    for( node = parent->children; node; node = node->next )
    {
        if( ptk_dir_tree_node_compare( tree, child_node, node ) >= 0 )
            break;
    }
    if( node )
    {
        if( node->prev )
        {
            child_node->prev = node->prev;
            node->prev->next = child_node;
        }
        child_node->next = node;
        if( node == parent->children )
            parent->children = child_node;
        node->prev = child_node;
    }
    else
    {
        if( parent->children )
        {
            child_node->prev = parent->last;
            parent->last->next = child_node;
            parent->last = child_node;
        }
        else
        {
            parent->children = parent->last = child_node;
        }
    }
    ++parent->n_children;

    it.stamp = tree->stamp;
    it.user_data = child_node;
    it.user_data2 = it.user_data3 = NULL;

    tree_path = ptk_dir_tree_get_path( GTK_TREE_MODEL(tree), &it );
    gtk_tree_model_row_inserted( GTK_TREE_MODEL(tree), tree_path, &it );
    gtk_tree_model_row_has_child_toggled( GTK_TREE_MODEL(tree), tree_path, &it );
    gtk_tree_path_free( tree_path );
}

void ptk_dir_tree_delete_child( PtkDirTree* tree,
                                PtkDirTreeNode* child )
{
    GtkTreeIter child_it;
    GtkTreePath* tree_path;
    PtkDirTreeNode* parent;

    if( !child )
        return;

    child_it.stamp = tree->stamp;
    child_it.user_data = child;
    child_it.user_data2 = child_it.user_data3 = NULL;

    tree_path = ptk_dir_tree_get_path( GTK_TREE_MODEL(tree), &child_it );
    gtk_tree_model_row_deleted( GTK_TREE_MODEL(tree), tree_path );
    gtk_tree_path_free( tree_path );

    parent = child->parent;
    --parent->n_children;

    if( child == parent->children )
        parent->children = parent->last = child->next;
    else if( child == parent->last )
        parent->last = child->prev;

    if( child->prev )
        child->prev->next = child->next;

    if( child->next )
        child->next->prev = child->prev;

    ptk_dir_tree_node_free( child );

    if( parent->n_children == 0 )
    {
        /* add place holder */
        ptk_dir_tree_insert_child( tree, parent, NULL,NULL );
    }
}

void ptk_dir_tree_expand_row ( PtkDirTree* tree,
                               GtkTreeIter* iter,
                               GtkTreePath *tree_path )
{
    PtkDirTreeNode *node, *place_holder;
    GDir *dir;
    char *path, *file_path;
    const char *name=NULL;

    node = (PtkDirTreeNode*)iter->user_data;
    ++node->n_expand;
    if( node->n_expand > 1 || node->n_children > 1 )
        return;

    place_holder = node->children;
    path = dir_path_from_tree_node( tree, node );

    dir = g_dir_open( path, 0, NULL );
    if( dir )
    {
        node->monitor = vfs_file_monitor_add_dir( path,
                                              &on_file_monitor_event,
                                              node );
        while( (name = g_dir_read_name( dir )) )
        {
            file_path = g_build_filename( path, name, NULL );
            if( g_file_test( file_path, G_FILE_TEST_IS_DIR ) )
            {
                ptk_dir_tree_insert_child( tree, node, file_path, name );
            }
            g_free( file_path );
        }
        g_dir_close( dir );

        if( node->n_children > 1 )
        {
            ptk_dir_tree_delete_child( tree, place_holder );
        }
    }
    g_free( path );
}

void ptk_dir_tree_collapse_row ( PtkDirTree* tree,
                                 GtkTreeIter* iter,
                                 GtkTreePath *path )
{
    PtkDirTreeNode *node, *child, *next;

    node = (PtkDirTreeNode*)iter->user_data;
    --node->n_expand;

    /* cache nodes containing more than 128 children */
    /* FIXME: Is this useful? The nodes containing childrens
              with 128+ children are still not cached. */
    if( node->n_children > 128 || node->n_expand > 0 )
        return;

    if( node->n_children > 0 )
    {
        /* place holder */
        if( node->n_children == 1 && ! node->children->file )
            return;
        if( G_LIKELY( node->monitor ) )
        {
            vfs_file_monitor_remove( node->monitor,
                                     &on_file_monitor_event,
                                     node );
            node->monitor = NULL;
        }
        for( child = node->children; child; child = next )
        {
            next = child->next;
            ptk_dir_tree_delete_child( tree, child );
        }
    }
}

char* ptk_dir_tree_get_dir_path( PtkDirTree* tree, GtkTreeIter* iter )
{
    g_return_val_if_fail( iter->user_data != NULL, NULL );
    return dir_path_from_tree_node( tree, (PtkDirTreeNode*)iter->user_data );
}

static PtkDirTreeNode* find_node( PtkDirTreeNode* parent, const char* name )
{
    PtkDirTreeNode* child;
    for( child = parent->children; child; child = child->next )
    {
        if( G_LIKELY(child->file )
                && 0 == strcmp( vfs_file_info_get_name(child->file), name ) )
        {
            return child;
        }
    }
    return NULL;
}

void on_file_monitor_event ( VFSFileMonitor* fm,
                             VFSFileMonitorEvent event,
                             const char* file_name,
                             gpointer user_data )
{
    PtkDirTreeNode* node = (PtkDirTreeNode*)user_data;
    PtkDirTreeNode* child;
    GtkTreeIter it;
    GtkTreePath* tree_path;
    char* file_path;
    g_return_if_fail( node );
    GDK_THREADS_ENTER();

    child = find_node( node, file_name );
    switch( event )
    {
    case VFS_FILE_MONITOR_CREATE:
        if( G_LIKELY( !child ) )
        {
            /* remove place holder */
            if( node->n_children == 1 && !node->children->file )
                child = node->children;
            else
                child = NULL;
            file_path = g_build_filename( fm->path, file_name, NULL );
            if( g_file_test( file_path, G_FILE_TEST_IS_DIR ) )
            {
                ptk_dir_tree_insert_child( node->tree,
                                        node, fm->path,
                                        file_name );
                if( child )
                    ptk_dir_tree_delete_child( node->tree, child );
            }
            g_free( file_path );
        }
        break;
    case VFS_FILE_MONITOR_DELETE:
        if( G_LIKELY( child ) )
        {
            ptk_dir_tree_delete_child( node->tree, child );
        }
        break;
/* //MOD Change isn't needed?  Creates this warning and triggers subsequent
 * errors and causes visible redrawing problems:
Gtk-CRITICAL **: /tmp/buildd/gtk+2.0-2.24.3/gtk/gtktreeview.c:6072 (validate_visible_area): assertion `has_next' failed.
There is a disparity between the internal view of the GtkTreeView,
and the GtkTreeModel.  This generally means that the model has changed
without letting the view know.  Any display from now on is likely to
be incorrect.
*/
    case VFS_FILE_MONITOR_CHANGE:
/*        if( G_LIKELY( child && child->file ) )
        {
            file_path = g_build_filename( fm->path, file_name, NULL );
            if( ! g_file_test( file_path, G_FILE_TEST_IS_DIR ) )
            {
                g_free( file_path );
                break;
            }
            vfs_file_info_get( child->file, file_path, file_name );
            g_free( file_path );
            it.stamp = node->tree->stamp;
            it.user_data = child;
            it.user_data2 = it.user_data3 = NULL;
            tree_path = ptk_dir_tree_get_path(GTK_TREE_MODEL(node->tree), &it);

            gtk_tree_model_row_changed( GTK_TREE_MODEL( node->tree ),
                                        tree_path, &it );
            gtk_tree_model_row_has_child_toggled( GTK_TREE_MODEL( node->tree ),
                                                  tree_path, &it );
            gtk_tree_path_free( tree_path );
        }
*/
        break;
    }
    GDK_THREADS_LEAVE();
}

