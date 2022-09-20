/*
*  C Interface: ptk-dir-tree
*
* Description: 
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef _PTK_DIR_TREE_H_
#define _PTK_DIR_TREE_H_

#include <gtk/gtk.h>
#include <glib.h>
#include <glib-object.h>

#include <sys/types.h>

G_BEGIN_DECLS

#define PTK_TYPE_DIR_TREE             (ptk_dir_tree_get_type())
#define PTK_DIR_TREE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),  PTK_TYPE_DIR_TREE, PtkDirTree))
#define PTK_DIR_TREE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass),  PTK_TYPE_DIR_TREE, PtkDirTreeClass))
#define PTK_IS_DIR_TREE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PTK_TYPE_DIR_TREE))
#define PTK_IS_DIR_TREE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass),  PTK_TYPE_DIR_TREE))
#define PTK_DIR_TREE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj),  PTK_TYPE_DIR_TREE, PtkDirTreeClass))

/* Columns of folder view */
enum{
    COL_DIR_TREE_ICON,
    COL_DIR_TREE_DISP_NAME,
    COL_DIR_TREE_INFO,
    N_DIR_TREE_COLS
};

typedef struct _PtkDirTree PtkDirTree;
typedef struct _PtkDirTreeClass PtkDirTreeClass;

typedef struct _PtkDirTreeNode PtkDirTreeNode;

struct _PtkDirTree
{
    GObject parent;
    /* <private> */

    PtkDirTreeNode* root;
    /* GtkSortType sort_order; */ /* I don't want to support this :-( */
    /* Random integer to check whether an iter belongs to our model */
    gint stamp;
};

struct _PtkDirTreeClass
{
    GObjectClass parent;
    /* Default signal handlers */
};

GType ptk_dir_tree_get_type (void);

PtkDirTree *ptk_dir_tree_new ();

void ptk_dir_tree_expand_row ( PtkDirTree* tree,
                               GtkTreeIter* iter,
                               GtkTreePath *path );

void ptk_dir_tree_collapse_row ( PtkDirTree* tree,
                                 GtkTreeIter* iter,
                                 GtkTreePath *path );

char* ptk_dir_tree_get_dir_path( PtkDirTree* tree, GtkTreeIter* iter );

G_END_DECLS

#endif
