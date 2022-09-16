/*
*  C Interface: ptkfileiconrenderer
*
* Description: PtkFileIconRenderer is used to render file icons
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/


#ifndef _PTK_FILE_ICON_RENDERER_H_
#define _PTK_FILE_ICON_RENDERER_H_

#include <gtk/gtk.h>
#include "vfs-file-info.h"

G_BEGIN_DECLS

#define PTK_TYPE_FILE_ICON_RENDERER             (ptk_file_icon_renderer_get_type())
#define PTK_FILE_ICON_RENDERER(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),  PTK_TYPE_FILE_ICON_RENDERER, PtkFileIconRenderer))
#define PTK_FILE_ICON_RENDERER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass),  PTK_TYPE_FILE_ICON_RENDERER, PtkFileIconRendererClass))
#define PTK_IS_FILE_ICON_RENDERER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PTK_TYPE_FILE_ICON_RENDERER))
#define PTK_IS_FILE_ICON_RENDERER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass),  PTK_TYPE_FILE_ICON_RENDERER))
#define PTK_FILE_ICON_RENDERER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj),  PTK_TYPE_FILE_ICON_RENDERER, PtkFileIconRendererClass))

typedef struct _PtkFileIconRenderer PtkFileIconRenderer;
typedef struct _PtkFileIconRendererClass PtkFileIconRendererClass;

struct _PtkFileIconRenderer
{
    GtkCellRendererPixbuf parent;

    /* Private */
    /* FIXME: draw some additional marks for symlinks */
    VFSFileInfo* info;
    /* long flags; */
    gboolean follow_state;
};

struct _PtkFileIconRendererClass
{
    GtkCellRendererPixbufClass  parent_class;
};

GType                ptk_file_icon_renderer_get_type (void);

GtkCellRenderer     *ptk_file_icon_renderer_new (void);


G_END_DECLS

#endif /* _PTK_FILE_ICON_RENDERER_H_ */

