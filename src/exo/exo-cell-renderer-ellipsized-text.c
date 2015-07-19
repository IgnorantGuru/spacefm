/*-
 * Copyright (c) 2004-2006 os-cillation e.K.
 * Copyright (c) 2015 OmegaPhil (OmegaPhil@startmail.com)
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "exo-cell-renderer-ellipsized-text.h"
#include "exo-private.h"
#include "exo-common.h"

/* Taken from exo v0.10.2 (Debian package libexo-1-0), according to changelog
 * commit f455681554ca205ffe49bd616310b19f5f9f8ef1 Dec 27 13:50:21 2012 */

/**
 * SECTION: exo-cell-renderer-ellipsized-text
 * @title: ExoCellRendererEllipsizedText
 * @short_description: Renders text in a cell
 * @include: exo/exo.h
 * @see_also: <ulink url="http://library.gnome.org/devel/gtk/stable/GtkCellRendererText.html"
 *            type="http">GtkCellRendererText</ulink>,
 *            <link linkend="ExoIconView">ExoIconView</link>
 *
 * The #ExoCellRendererEllipsizedText renders a given text in its cell,
 * using the font, color and style information provided by its properties
 * (which are actually inherited from #GtkCellRendererText).
 *
 * Despite the rather confusing name of this class, it is mainly useful
 * to render text in an #ExoIconView (or a #GtkIconView), which require
 * the renderers to actually draw the state indicators. State indicators
 * will be drawn only if the
 * <link linkend="ExoCellRendererEllipsizedText--follow-state">follow-state</link>
 * property is %TRUE.
 **/

#define EXO_CELL_RENDERER_ELLIPSIZED_TEXT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
    EXO_TYPE_CELL_RENDERER_ELLIPSIZED_TEXT, ExoCellRendererEllipsizedTextPrivate))

/* Property identifiers */
enum
{
    PROP_0,
    PROP_FOLLOW_STATE,
};



static void exo_cell_renderer_ellipsized_text_get_property  (GObject      *object,
                                                             guint        prop_id,
                                                             GValue       *value,
                                                             GParamSpec   *pspec);
static void exo_cell_renderer_ellipsized_text_set_property  (GObject      *object,
                                                             guint        prop_id,
                                                             const GValue *value,
                                                             GParamSpec   *pspec);
static void exo_cell_renderer_ellipsized_text_get_size      (GtkCellRenderer *renderer,
                                                             GtkWidget    *widget,

//sfm-gtk3
#if GTK_CHECK_VERSION (3, 0, 0)
                                                             const GdkRectangle *cell_area,
#else
                                                             GdkRectangle *cell_area,
#endif

                                                             gint         *x_offset,
                                                             gint         *y_offset,
                                                             gint         *width,
                                                             gint         *height);

//sfm-gtk3
#if GTK_CHECK_VERSION (3, 0, 0)
static void exo_cell_renderer_ellipsized_text_get_preferred_width (GtkCellRenderer *renderer,
                                                                   GtkWidget       *widget,
                                                                   gint            *minimal_size,
                                                                   gint           *natural_size);
static void exo_cell_renderer_ellipsized_text_get_preferred_height (GtkCellRenderer *renderer,
                                                                   GtkWidget       *widget,
                                                                   gint            *minimal_size,
                                                                   gint           *natural_size);
static void exo_cell_renderer_ellipsized_text_get_preferred_height_for_width (
                                                  GtkCellRenderer *renderer,
                                                  GtkWidget       *widget,
                                                  gint            width,
                                                  gint            *minimal_size,
                                                  gint            *natural_size);
#endif

static void exo_cell_renderer_ellipsized_text_render        (GtkCellRenderer *renderer,

//sfm-gtk3
#if GTK_CHECK_VERSION (3, 0, 0)
                                                             cairo_t      *cr,
#else
                                                             GdkWindow    *window,
#endif

                                                             GtkWidget    *widget,

//sfm-gtk3
#if GTK_CHECK_VERSION (3, 0, 0)
                                                             const GdkRectangle *background_area,
                                                             const GdkRectangle *cell_area,
#else
                                                             GdkRectangle *background_area,
                                                             GdkRectangle *cell_area,
#endif

//sfm-gtk3
#if GTK_CHECK_VERSION (3, 0, 0)
#else
                                                             GdkRectangle *expose_area,
#endif

                                                             GtkCellRendererState flags);



struct _ExoCellRendererEllipsizedTextPrivate
{
    gboolean follow_state;
};



G_DEFINE_TYPE (ExoCellRendererEllipsizedText, exo_cell_renderer_ellipsized_text, GTK_TYPE_CELL_RENDERER_TEXT)



static void
exo_cell_renderer_ellipsized_text_class_init (ExoCellRendererEllipsizedTextClass *klass)
{
    GtkCellRendererClass *gtkcell_renderer_class;
    GObjectClass         *gobject_class;

    /* add our private data to the type's instances */
    g_type_class_add_private (klass, sizeof (ExoCellRendererEllipsizedTextPrivate));

    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->get_property = exo_cell_renderer_ellipsized_text_get_property;
    gobject_class->set_property = exo_cell_renderer_ellipsized_text_set_property;

    // Note that get_size is never called directly by GTK3
    gtkcell_renderer_class = GTK_CELL_RENDERER_CLASS (klass);
    gtkcell_renderer_class->get_size = exo_cell_renderer_ellipsized_text_get_size;
    gtkcell_renderer_class->render = exo_cell_renderer_ellipsized_text_render;

    //sfm-gtk3
#if GTK_CHECK_VERSION (3, 0, 0)
    gtkcell_renderer_class->get_preferred_width =
            exo_cell_renderer_ellipsized_text_get_preferred_width;
    gtkcell_renderer_class->get_preferred_height_for_width =
            exo_cell_renderer_ellipsized_text_get_preferred_height_for_width;
    gtkcell_renderer_class->get_preferred_height =
            exo_cell_renderer_ellipsized_text_get_preferred_height;
#endif

    /**
   * ExoCellRendererEllipsizedText:follow-state:
   *
   * Specifies whether the text renderer should render the text based on
   * the selection state of the items. This is necessary for #ExoIconView
   * which doesn't draw any item state indicators itself.
   *
   * Since: 0.3.1.9
   **/
    g_object_class_install_property (gobject_class,
                                     PROP_FOLLOW_STATE,
                                     g_param_spec_boolean ("follow-state",
                                                           _("Follow state"),
                                                           _("Render differently based on the selection state."),
                                                           FALSE,
                                                           EXO_PARAM_READWRITE));
}



static void
exo_cell_renderer_ellipsized_text_init (ExoCellRendererEllipsizedText *text)
{
}



static void
exo_cell_renderer_ellipsized_text_get_property (GObject    *object,
                                                guint       prop_id,
                                                GValue     *value,
                                                GParamSpec *pspec)
{
    ExoCellRendererEllipsizedTextPrivate *priv = EXO_CELL_RENDERER_ELLIPSIZED_TEXT_GET_PRIVATE (object);

    switch (prop_id)
    {
    case PROP_FOLLOW_STATE:
        g_value_set_boolean (value, priv->follow_state);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}



static void
exo_cell_renderer_ellipsized_text_set_property (GObject      *object,
                                                guint         prop_id,
                                                const GValue *value,
                                                GParamSpec   *pspec)
{
    ExoCellRendererEllipsizedTextPrivate *priv = EXO_CELL_RENDERER_ELLIPSIZED_TEXT_GET_PRIVATE (object);

    switch (prop_id)
    {
    case PROP_FOLLOW_STATE:
        priv->follow_state = g_value_get_boolean (value);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}



static void
exo_cell_renderer_ellipsized_text_get_size (GtkCellRenderer *renderer,
                                            GtkWidget       *widget,

//sfm-gtk3
#if GTK_CHECK_VERSION (3, 0, 0)
                                            const GdkRectangle *cell_area,
#else
                                            GdkRectangle    *cell_area,
#endif

                                            gint            *x_offset,
                                            gint            *y_offset,
                                            gint            *width,
                                            gint            *height)
{
    ExoCellRendererEllipsizedTextPrivate *priv = EXO_CELL_RENDERER_ELLIPSIZED_TEXT_GET_PRIVATE (renderer);
    gint                                  focus_line_width;
    gint                                  focus_padding;
    gint                                  text_height;
    gint                                  text_width;

    /* Determine the dimensions of the text from the GtkCellRendererText - see
     * in exo_cell_renderer_ellipsized_text_render for commentary on the GTK2
     * approach being bricked in GTK3
     * gtk_cell_renderer_get_size call here results in recursion now that I'm
     * implementing get_preferred_width
     * get_preferred_size is not made available so I have to combine
     * get_preferred_width and get_preferred_height_for_width - minimum_size
     * is sought in both cases */
    //sfm-gtk3
#if GTK_CHECK_VERSION (3, 0, 0)
    (*GTK_CELL_RENDERER_CLASS (exo_cell_renderer_ellipsized_text_parent_class)->get_preferred_width) (renderer, widget, &text_width, NULL);
    (*GTK_CELL_RENDERER_CLASS (exo_cell_renderer_ellipsized_text_parent_class)->get_preferred_height_for_width) (renderer, widget, text_width, &text_height,
                                  NULL);
#else
    (*GTK_CELL_RENDERER_CLASS (exo_cell_renderer_ellipsized_text_parent_class)->get_size) (renderer, widget, NULL, NULL, NULL, &text_width, &text_height);
#endif

    /* If we have to follow the state manually, we'll need
     * to reserve some space to render the indicator to.
     */
    if (G_LIKELY (priv->follow_state))
    {
        /* Determine the focus-padding and focus-line-width style properties from the widget */
        gtk_widget_style_get (widget, "focus-padding", &focus_padding, "focus-line-width", &focus_line_width, NULL);

        /* Add the focus widget to the text dimensions */
        text_width += 2 * (focus_line_width + focus_padding);
        text_height += 2 * (focus_line_width + focus_padding);
    }

    /* Update width/height */
    if (G_LIKELY (width))
        *width = text_width;
    if (G_LIKELY (height))
        *height = text_height;

    /* Update the x/y offsets */
    if (G_LIKELY (cell_area))
    {
        gfloat xalign, yalign;
        gtk_cell_renderer_get_alignment (renderer, &xalign, &yalign);

        if (G_LIKELY (x_offset))
        {
            *x_offset = ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ?
                             1.0 - xalign : xalign)
                        * (cell_area->width - text_width);
            *x_offset = MAX (*x_offset, 0);
        }

        if (G_LIKELY (y_offset))
        {
            *y_offset = yalign * (cell_area->height - text_height);
            *y_offset = MAX (*y_offset, 0);
        }
    }
}

//sfm-gtk3
#if GTK_CHECK_VERSION (3, 0, 0)
static void exo_cell_renderer_ellipsized_text_get_preferred_width (GtkCellRenderer *renderer,
                                                                   GtkWidget       *widget,
                                                                   gint            *minimal_size,
                                                                   gint            *natural_size)
{
    // Calling the normal code to get width
    exo_cell_renderer_ellipsized_text_get_size (renderer, widget, NULL, NULL,
                                                NULL, minimal_size, NULL);

    // If natural size was requested, duplicate
    if (minimal_size && natural_size)
        *natural_size = *minimal_size;
}

static void exo_cell_renderer_ellipsized_text_get_preferred_height (GtkCellRenderer *renderer,
                                                                   GtkWidget       *widget,
                                                                   gint            *minimal_size,
                                                                   gint            *natural_size)
{
    // Calling the normal code to get height
    exo_cell_renderer_ellipsized_text_get_size (renderer, widget, NULL, NULL,
                                                NULL, NULL, minimal_size);

    // If natural size was requested, duplicate
    if (minimal_size && natural_size)
        *natural_size = *minimal_size;
}

static void exo_cell_renderer_ellipsized_text_get_preferred_height_for_width (
                                                  GtkCellRenderer *renderer,
                                                  GtkWidget       *widget,
                                                  gint            width,
                                                  gint            *minimal_size,
                                                  gint            *natural_size)
{
    // Ignoring specified width and just returning the normal height
    exo_cell_renderer_ellipsized_text_get_preferred_height(renderer, widget,
                                                           minimal_size,
                                                           natural_size);
}
#endif


static void
exo_cell_renderer_ellipsized_text_render (GtkCellRenderer     *renderer,

//sfm-gtk3
#if GTK_CHECK_VERSION (3, 0, 0)
                                          cairo_t               *cr,
#else
                                          GdkWindow             *window,
#endif

                                          GtkWidget             *widget,

//sfm-gtk3
#if GTK_CHECK_VERSION (3, 0, 0)
                                          const GdkRectangle  *background_area,
                                          const GdkRectangle  *cell_area,
#else
                                          GdkRectangle        *background_area,
                                          GdkRectangle        *cell_area,
#endif

//sfm-gtk3
#if GTK_CHECK_VERSION (3, 0, 0)
#else
                                          GdkRectangle             *expose_area,
#endif

                                          GtkCellRendererState      flags)
{
    ExoCellRendererEllipsizedTextPrivate *priv = EXO_CELL_RENDERER_ELLIPSIZED_TEXT_GET_PRIVATE (renderer);
    GdkRectangle                          text_area;
    GtkStateType                          state;

//sfm-gtk3
#if GTK_CHECK_VERSION (3, 0, 0)
#else
    cairo_t                               *cr;
#endif

    gint                                  focus_line_width;
    gint                                  focus_padding;
    gint                                  text_height;
    gint                                  text_width;
    gint                                  x0, x1;
    gint                                  y0, y1;

    //sfm-gtk3
    // This parameter isn't passed in the GTK3 call, so creating a replacement
#if GTK_CHECK_VERSION (3, 0, 0)
    GdkRectangle                     *expose_area = NULL;
#endif

    /* Determine the text cell areas */
    if (G_UNLIKELY (!priv->follow_state))
    {
        /* No state indicator */
        text_area = *cell_area;
    }
    else
    {
        /* Determine the widget state */
        if ((flags & GTK_CELL_RENDERER_SELECTED) == GTK_CELL_RENDERER_SELECTED)
        {
            if (gtk_widget_has_focus (widget))
                state = GTK_STATE_SELECTED;
            else
                state = GTK_STATE_ACTIVE;
        }
        else if ((flags & GTK_CELL_RENDERER_PRELIT) == GTK_CELL_RENDERER_PRELIT
                 && gtk_widget_get_state (widget) == GTK_STATE_PRELIGHT)
        {
            state = GTK_STATE_PRELIGHT;
        }
        else
        {
            if (gtk_widget_get_state (widget) == GTK_STATE_INSENSITIVE)
                state = GTK_STATE_INSENSITIVE;
            else
                state = GTK_STATE_NORMAL;
        }

        /* Determine the focus-padding and focus-line-width style properties
         * from the widget */
        gtk_widget_style_get (widget, "focus-padding", &focus_padding,
                              "focus-line-width", &focus_line_width,
                              NULL);

        /* Determine the text cell area */
        text_area.x = cell_area->x + (focus_line_width + focus_padding);
        text_area.y = cell_area->y + (focus_line_width + focus_padding);
        text_area.width = cell_area->width - 2 * (focus_line_width + focus_padding);
        text_area.height = cell_area->height - 2 * (focus_line_width + focus_padding);

        /* Check if we need to draw any state indicator */
        if ((flags & (GTK_CELL_RENDERER_FOCUSED | GTK_CELL_RENDERER_SELECTED)) != 0)
        {
            /* Determine the real text dimensions */
            //sfm-gtk3
#if GTK_CHECK_VERSION (3, 0, 0)
            /* GTK3 completely breaks the GTK2 get_size call here as GtkCellRenderer
             * NULLs the function pointer in its gtk_cell_renderer_class_init
             * (doesn't set it to the available get_size function), and
             * GtkCellRendererText does not implement its own get_size - so just
             * using the normal function call here
             * The GTK3 get_size call basically calls gtk_cell_renderer_get_preferred_size
             * and then does cell offset calculation if cell_area is specified,
             * so its not different enough to warrant reworking stuff to use it */
            gtk_cell_renderer_get_size (renderer, widget, &text_area, &x0, &y0,
                                        &text_width, &text_height);
#else
            (*GTK_CELL_RENDERER_CLASS (exo_cell_renderer_ellipsized_text_parent_class)->get_size)(
                                       renderer, widget, &text_area, &x0, &y0,
                                       &text_width, &text_height);
#endif

            /* Adjust the offsets appropriately */
            x0 += text_area.x;
            y0 += text_area.y;

            GtkStyle *style = gtk_widget_get_style (widget);

            /* Render the selected state indicator */
            if ((flags & GTK_CELL_RENDERER_SELECTED) == GTK_CELL_RENDERER_SELECTED)
            {
                /* Calculate the text bounding box (including the focus padding/
                 * width) */
                x1 = x0 + text_width;
                y1 = y0 + text_height;

                /* Cairo produces nicer results than using a polygon and so we
                 * use it directly if possible
                 * In GTK3, the cairo context has already been provided */
                //sfm-gtk3
#if GTK_CHECK_VERSION (3, 0, 0)
#else
                cr = gdk_cairo_create (window);
#endif
                cairo_move_to (cr, x0 + 5, y0);
                cairo_line_to (cr, x1 - 5, y0);
                cairo_curve_to (cr, x1 - 5, y0, x1, y0, x1, y0 + 5);
                cairo_line_to (cr, x1, y1 - 5);
                cairo_curve_to (cr, x1, y1 - 5, x1, y1, x1 - 5, y1);
                cairo_line_to (cr, x0 + 5, y1);
                cairo_curve_to (cr, x0 + 5, y1, x0, y1, x0, y1 - 5);
                cairo_line_to (cr, x0, y0 + 5);
                cairo_curve_to (cr, x0, y0 + 5, x0, y0, x0 + 5, y0);
                gdk_cairo_set_source_color (cr, &style->base[state]);
                cairo_fill (cr);

                //sfm-gtk3
#if GTK_CHECK_VERSION (3, 0, 0)
#else
                cairo_destroy (cr);
#endif
            }

            /* Draw the focus indicator */
            if ((flags & GTK_CELL_RENDERER_FOCUSED) != 0)
            {
                gtk_paint_focus (style,

//sfm-gtk3
#if GTK_CHECK_VERSION (3, 0, 0)
                                 cr,
#else
                                 window,
#endif

                                 gtk_widget_get_state (widget),

//sfm-gtk3
#if GTK_CHECK_VERSION (3, 0, 0)
#else
                                 NULL,
#endif

                                 widget,
                                 "icon_view",
                                 x0,
                                 y0,
                                 text_width,
                                 text_height);
                flags &= ~GTK_CELL_RENDERER_FOCUSED;
            }
        }
    }

    /* Render the text using the GtkCellRendererText */
    (*GTK_CELL_RENDERER_CLASS (exo_cell_renderer_ellipsized_text_parent_class)->render) (
                               renderer,

//sfm-gtk3
#if GTK_CHECK_VERSION (3, 0, 0)
                               cr,
#else
                               window,
#endif

                               widget,
                               background_area,
                               &text_area,

//sfm-gtk3
#if GTK_CHECK_VERSION (3, 0, 0)
#else
                               expose_area,
#endif

                               flags);
}



/**
 * exo_cell_renderer_ellipsized_text_new:
 *
 * Creates a new #ExoCellRendererEllipsizedText. Adjust rendering parameters using gobject properties,
 * which can be set globally via g_object_set(). Also, with #GtkCellLayout and #GtkTreeViewColumn, you
 * can bind a property to a value in a #GtkTreeModel.
 *
 * Returns: the newly allocated #ExoCellRendererEllipsizedText.
 **/
GtkCellRenderer*
exo_cell_renderer_ellipsized_text_new (void)
{
    return g_object_new (EXO_TYPE_CELL_RENDERER_ELLIPSIZED_TEXT, NULL);
}


#define __EXO_CELL_RENDERER_ELLIPSIZED_TEXT_C__
