/* ptk-text-renderer.c
* Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Library General Public
* License as published by the Free Software Foundation; either
* version 2 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Library General Public License for more details.
*
* You should have received a copy of the GNU Library General Public
* License along with this library; if not, write to the
* Free Software Foundation, Inc., 59 Temple Place - Suite 330,
* Boston, MA 02111-1307, USA.
*/

/*
    This file is originally copied from gtkcellrenderertext.c of gtk+ library.
    2006.07.16 modified by Hong Jen Yee to produce a simplified text renderer
    which supports center alignment of text to be used in PCMan File Manager.
*/

#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "ptk-text-renderer.h"

static void ptk_text_renderer_init ( PtkTextRenderer *celltext );
static void ptk_text_renderer_class_init ( PtkTextRendererClass *class );
static void ptk_text_renderer_finalize ( GObject *object );

static void ptk_text_renderer_get_property ( GObject *object,
                                             guint param_id,
                                             GValue *value,
                                             GParamSpec *pspec );
static void ptk_text_renderer_set_property ( GObject *object,
                                             guint param_id,
                                             const GValue *value,
                                             GParamSpec *pspec );
static void ptk_text_renderer_get_size ( GtkCellRenderer *cell,
                                         GtkWidget *widget,
                                         GdkRectangle *cell_area,
                                         gint *x_offset,
                                         gint *y_offset,
                                         gint *width,
                                         gint *height );
static void ptk_text_renderer_render ( GtkCellRenderer *cell,
                                       GdkWindow *window,
                                       GtkWidget *widget,
                                       GdkRectangle *background_area,
                                       GdkRectangle *cell_area,
                                       GdkRectangle *expose_area,
                                       GtkCellRendererState flags );


enum {
    PROP_0,

    PROP_TEXT,
    PROP_WRAP_WIDTH,

    /* Style args */
    PROP_BACKGROUND,
    PROP_FOREGROUND,
    PROP_BACKGROUND_GDK,
    PROP_FOREGROUND_GDK,
    PROP_FONT,
    PROP_FONT_DESC,
    PROP_UNDERLINE,
    PROP_ELLIPSIZE,
    PROP_WRAP_MODE,

    /* Whether-a-style-arg-is-set args */
    PROP_BACKGROUND_SET,
    PROP_FOREGROUND_SET,
    PROP_UNDERLINE_SET,
    PROP_ELLIPSIZE_SET
};

static gpointer parent_class;

#define PTK_TEXT_RENDERER_PATH "ptk-cell-renderer-text-path"

GType
ptk_text_renderer_get_type ( void )
{
    static GType cell_text_type = 0;

    if ( !cell_text_type )
    {
        static const GTypeInfo cell_text_info =
            {
                sizeof ( PtkTextRendererClass ),
                NULL,        /* base_init */
                NULL,        /* base_finalize */
                ( GClassInitFunc ) ptk_text_renderer_class_init,
                NULL,        /* class_finalize */
                NULL,        /* class_data */
                sizeof ( PtkTextRenderer ),
                0,               /* n_preallocs */
                ( GInstanceInitFunc ) ptk_text_renderer_init,
            };

        cell_text_type =
            g_type_register_static ( GTK_TYPE_CELL_RENDERER, "PtkTextRenderer",
                                     &cell_text_info, 0 );
    }

    return cell_text_type;
}

static void
ptk_text_renderer_init ( PtkTextRenderer *celltext )
{
    GTK_CELL_RENDERER ( celltext ) ->xalign = 0.0;
    GTK_CELL_RENDERER ( celltext ) ->yalign = 0.5;
    GTK_CELL_RENDERER ( celltext ) ->xpad = 2;
    GTK_CELL_RENDERER ( celltext ) ->ypad = 2;
    celltext->font = pango_font_description_new ();

    celltext->wrap_width = -1;
}

static void
ptk_text_renderer_class_init ( PtkTextRendererClass *class )
{
    GObjectClass *object_class = G_OBJECT_CLASS ( class );
    GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS ( class );

    parent_class = g_type_class_peek_parent ( class );

    object_class->finalize = ptk_text_renderer_finalize;

    object_class->get_property = ptk_text_renderer_get_property;
    object_class->set_property = ptk_text_renderer_set_property;

    cell_class->get_size = ptk_text_renderer_get_size;
    cell_class->render = ptk_text_renderer_render;

    g_object_class_install_property ( object_class,
                                      PROP_TEXT,
                                      g_param_spec_string ( "text",
                                                            _( "Text" ),
                                                            _( "Text to render" ),
                                                            NULL,
                                                            G_PARAM_READABLE | G_PARAM_WRITABLE ) );

    g_object_class_install_property ( object_class,
                                      PROP_BACKGROUND,
                                      g_param_spec_string ( "background",
                                                            _( "Background color name" ),
                                                            _( "Background color as a string" ),
                                                            NULL,
                                                            G_PARAM_WRITABLE ) );

    g_object_class_install_property ( object_class,
                                      PROP_BACKGROUND_GDK,
                                      g_param_spec_boxed ( "background-gdk",
                                                           _( "Background color" ),
                                                           _( "Background color as a GdkColor" ),
                                                           GDK_TYPE_COLOR,
                                                           G_PARAM_READABLE | G_PARAM_WRITABLE ) );

    g_object_class_install_property ( object_class,
                                      PROP_FOREGROUND,
                                      g_param_spec_string ( "foreground",
                                                            _( "Foreground color name" ),
                                                            _( "Foreground color as a string" ),
                                                            NULL,
                                                            G_PARAM_WRITABLE ) );

    g_object_class_install_property ( object_class,
                                      PROP_FOREGROUND_GDK,
                                      g_param_spec_boxed ( "foreground-gdk",
                                                           _( "Foreground color" ),
                                                           _( "Foreground color as a GdkColor" ),
                                                           GDK_TYPE_COLOR,
                                                           G_PARAM_READABLE | G_PARAM_WRITABLE ) );


    g_object_class_install_property ( object_class,
                                      PROP_FONT,
                                      g_param_spec_string ( "font",
                                                            _( "Font" ),
                                                            _( "Font description as a string" ),
                                                            NULL,
                                                            G_PARAM_READABLE | G_PARAM_WRITABLE ) );

    g_object_class_install_property ( object_class,
                                      PROP_FONT_DESC,
                                      g_param_spec_boxed ( "font-desc",
                                                           _( "Font" ),
                                                           _( "Font description as a PangoFontDescription struct" ),
                                                           PANGO_TYPE_FONT_DESCRIPTION,
                                                           G_PARAM_READABLE | G_PARAM_WRITABLE ) );

    g_object_class_install_property ( object_class,
                                      PROP_UNDERLINE,
                                      g_param_spec_enum ( "underline",
                                                          _( "Underline" ),
                                                          _( "Style of underline for this text" ),
                                                          PANGO_TYPE_UNDERLINE,
                                                          PANGO_UNDERLINE_NONE,
                                                          G_PARAM_READABLE | G_PARAM_WRITABLE ) );


    /**
     * PtkTextRenderer:ellipsize:
       *
       * Specifies the preferred place to ellipsize the string, if the cell renderer
       * does not have enough room to display the entire string. Setting it to
       * %PANGO_ELLIPSIZE_NONE turns off ellipsizing. See the wrap-width property
       * for another way of making the text fit into a given width.
       *
       * Since: 2.6
     */
    g_object_class_install_property ( object_class,
                                      PROP_ELLIPSIZE,
                                      g_param_spec_enum ( "ellipsize",
                                                          _( "Ellipsize" ),
                                                          _( "The preferred place to ellipsize the string, "
                                                             "if the cell renderer does not have enough room "
                                                             "to display the entire string, if at all" ),
                                                          PANGO_TYPE_ELLIPSIZE_MODE,
                                                          PANGO_ELLIPSIZE_NONE,
                                                          G_PARAM_READABLE | G_PARAM_WRITABLE ) );

    /**
     * PtkTextRenderer:wrap-mode:
       *
       * Specifies how to break the string into multiple lines, if the cell
       * renderer does not have enough room to display the entire string.
       * This property has no effect unless the wrap-width property is set.
       *
       * Since: 2.8
     */
    g_object_class_install_property ( object_class,
                                      PROP_WRAP_MODE,
                                      g_param_spec_enum ( "wrap-mode",
                                                          _( "Wrap mode" ),
                                                          _( "How to break the string into multiple lines, "
                                                             "if the cell renderer does not have enough room "
                                                             "to display the entire string" ),
                                                          PANGO_TYPE_WRAP_MODE,
                                                          PANGO_WRAP_CHAR,
                                                          G_PARAM_READABLE | G_PARAM_WRITABLE ) );

    /**
     * PtkTextRenderer:wrap-width:
       *
       * Specifies the width at which the text is wrapped. The wrap-mode property can
       * be used to influence at what character positions the line breaks can be placed.
       * Setting wrap-width to -1 turns wrapping off.
       *
       * Since: 2.8
     */
    g_object_class_install_property ( object_class,
                                      PROP_WRAP_WIDTH,
                                      g_param_spec_int ( "wrap-width",
                                                         _( "Wrap width" ),
                                                         _( "The width at which the text is wrapped" ),
                                                         -1,
                                                         G_MAXINT,
                                                         -1,
                                                         G_PARAM_READABLE | G_PARAM_WRITABLE ) );


    /* Style props are set or not */

#define ADD_SET_PROP(propname, propval, nick, blurb) g_object_class_install_property (object_class, propval, g_param_spec_boolean (propname, nick, blurb, FALSE, G_PARAM_READABLE|G_PARAM_WRITABLE))

    ADD_SET_PROP ( "background-set", PROP_BACKGROUND_SET,
                   _( "Background set" ),
                   _( "Whether this tag affects the background color" ) );

    ADD_SET_PROP ( "foreground-set", PROP_FOREGROUND_SET,
                   _( "Foreground set" ),
                   _( "Whether this tag affects the foreground color" ) );

    ADD_SET_PROP ( "underline-set", PROP_UNDERLINE_SET,
                   _( "Underline set" ),
                   _( "Whether this tag affects underlining" ) );

    ADD_SET_PROP ( "ellipsize-set", PROP_ELLIPSIZE_SET,
                   _( "Ellipsize set" ),
                   _( "Whether this tag affects the ellipsize mode" ) );
}

static void
ptk_text_renderer_finalize ( GObject *object )
{
    PtkTextRenderer * celltext = PTK_TEXT_RENDERER ( object );

    pango_font_description_free ( celltext->font );

    g_free ( celltext->text );

    ( * G_OBJECT_CLASS ( parent_class ) ->finalize ) ( object );
}

static void
ptk_text_renderer_get_property ( GObject *object,
                                 guint param_id,
                                 GValue *value,
                                 GParamSpec *pspec )
{
    PtkTextRenderer * celltext = PTK_TEXT_RENDERER ( object );

    switch ( param_id )
    {
    case PROP_TEXT:
        g_value_set_string ( value, celltext->text );
        break;

    case PROP_BACKGROUND_GDK:
        {
            GdkColor color;

            color.red = celltext->background.red;
            color.green = celltext->background.green;
            color.blue = celltext->background.blue;

            g_value_set_boxed ( value, &color );
        }
        break;

    case PROP_FOREGROUND_GDK:
        {
            GdkColor color;

            color.red = celltext->foreground.red;
            color.green = celltext->foreground.green;
            color.blue = celltext->foreground.blue;

            g_value_set_boxed ( value, &color );
        }
        break;

    case PROP_FONT:
        {
            /* FIXME GValue imposes a totally gratuitous string copy
                * here, we could just hand off string ownership
            */
            gchar *str = pango_font_description_to_string ( celltext->font );
            g_value_set_string ( value, str );
            g_free ( str );
        }
        break;

    case PROP_FONT_DESC:
        g_value_set_boxed ( value, celltext->font );
        break;

    case PROP_UNDERLINE:
        g_value_set_enum ( value, celltext->underline_style );
        break;

    case PROP_ELLIPSIZE:
        g_value_set_enum ( value, celltext->ellipsize );
        break;

    case PROP_WRAP_MODE:
        g_value_set_enum ( value, celltext->wrap_mode );
        break;

    case PROP_WRAP_WIDTH:
        g_value_set_int ( value, celltext->wrap_width );
        break;

    case PROP_BACKGROUND_SET:
        g_value_set_boolean ( value, celltext->background_set );
        break;

    case PROP_FOREGROUND_SET:
        g_value_set_boolean ( value, celltext->foreground_set );
        break;

    case PROP_UNDERLINE_SET:
        g_value_set_boolean ( value, celltext->underline_set );
        break;

    case PROP_ELLIPSIZE_SET:
        g_value_set_boolean ( value, celltext->ellipsize_set );
        break;

    case PROP_BACKGROUND:
    case PROP_FOREGROUND:
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID ( object, param_id, pspec );
        break;
    }
}


static void
set_bg_color ( PtkTextRenderer *celltext,
               GdkColor *color )
{
    if ( color )
    {
        if ( !celltext->background_set )
        {
            celltext->background_set = TRUE;
            g_object_notify ( G_OBJECT ( celltext ), "background-set" );
        }

        celltext->background.red = color->red;
        celltext->background.green = color->green;
        celltext->background.blue = color->blue;
    }
    else
    {
        if ( celltext->background_set )
        {
            celltext->background_set = FALSE;
            g_object_notify ( G_OBJECT ( celltext ), "background-set" );
        }
    }
}


static void
set_fg_color ( PtkTextRenderer *celltext,
               GdkColor *color )
{
    if ( color )
    {
        if ( !celltext->foreground_set )
        {
            celltext->foreground_set = TRUE;
            g_object_notify ( G_OBJECT ( celltext ), "foreground-set" );
        }

        celltext->foreground.red = color->red;
        celltext->foreground.green = color->green;
        celltext->foreground.blue = color->blue;
    }
    else
    {
        if ( celltext->foreground_set )
        {
            celltext->foreground_set = FALSE;
            g_object_notify ( G_OBJECT ( celltext ), "foreground-set" );
        }
    }
}

#if 0
static PangoFontMask
set_font_desc_fields ( PangoFontDescription *desc,
                       PangoFontMask to_set )
{
    PangoFontMask changed_mask = 0;

    if ( to_set & PANGO_FONT_MASK_FAMILY )
    {
        const char * family = pango_font_description_get_family ( desc );
        if ( !family )
        {
            family = "sans";
            changed_mask |= PANGO_FONT_MASK_FAMILY;
        }

        pango_font_description_set_family ( desc, family );
    }
    if ( to_set & PANGO_FONT_MASK_STYLE )
        pango_font_description_set_style ( desc, pango_font_description_get_style ( desc ) );
    if ( to_set & PANGO_FONT_MASK_VARIANT )
        pango_font_description_set_variant ( desc, pango_font_description_get_variant ( desc ) );
    if ( to_set & PANGO_FONT_MASK_WEIGHT )
        pango_font_description_set_weight ( desc, pango_font_description_get_weight ( desc ) );
    if ( to_set & PANGO_FONT_MASK_STRETCH )
        pango_font_description_set_stretch ( desc, pango_font_description_get_stretch ( desc ) );
    if ( to_set & PANGO_FONT_MASK_SIZE )
    {
        gint size = pango_font_description_get_size ( desc );
        if ( size <= 0 )
        {
            size = 10 * PANGO_SCALE;
            changed_mask |= PANGO_FONT_MASK_SIZE;
        }

        pango_font_description_set_size ( desc, size );
    }

    return changed_mask;
}
#endif

static void
notify_set_changed ( GObject *object,
                     PangoFontMask changed_mask )
{
    if ( changed_mask & PANGO_FONT_MASK_FAMILY )
        g_object_notify ( object, "family-set" );
    if ( changed_mask & PANGO_FONT_MASK_STYLE )
        g_object_notify ( object, "style-set" );
    if ( changed_mask & PANGO_FONT_MASK_VARIANT )
        g_object_notify ( object, "variant-set" );
    if ( changed_mask & PANGO_FONT_MASK_WEIGHT )
        g_object_notify ( object, "weight-set" );
    if ( changed_mask & PANGO_FONT_MASK_STRETCH )
        g_object_notify ( object, "stretch-set" );
    if ( changed_mask & PANGO_FONT_MASK_SIZE )
        g_object_notify ( object, "size-set" );
}

#if 0
static void
notify_fields_changed ( GObject *object,
                        PangoFontMask changed_mask )
{
    if ( changed_mask & PANGO_FONT_MASK_FAMILY )
        g_object_notify ( object, "family" );
    if ( changed_mask & PANGO_FONT_MASK_STYLE )
        g_object_notify ( object, "style" );
    if ( changed_mask & PANGO_FONT_MASK_VARIANT )
        g_object_notify ( object, "variant" );
    if ( changed_mask & PANGO_FONT_MASK_WEIGHT )
        g_object_notify ( object, "weight" );
    if ( changed_mask & PANGO_FONT_MASK_STRETCH )
        g_object_notify ( object, "stretch" );
    if ( changed_mask & PANGO_FONT_MASK_SIZE )
        g_object_notify ( object, "size" );
}
#endif

static void
set_font_description ( PtkTextRenderer *celltext,
                       PangoFontDescription *font_desc )
{
    GObject * object = G_OBJECT ( celltext );
    PangoFontDescription *new_font_desc;
    PangoFontMask old_mask, new_mask, changed_mask, set_changed_mask;

    if ( font_desc )
        new_font_desc = pango_font_description_copy ( font_desc );
    else
        new_font_desc = pango_font_description_new ();

    old_mask = pango_font_description_get_set_fields ( celltext->font );
    new_mask = pango_font_description_get_set_fields ( new_font_desc );

    changed_mask = old_mask | new_mask;
    set_changed_mask = old_mask ^ new_mask;

    pango_font_description_free ( celltext->font );
    celltext->font = new_font_desc;

    g_object_freeze_notify ( object );

    g_object_notify ( object, "font-desc" );
    g_object_notify ( object, "font" );

    if ( changed_mask & PANGO_FONT_MASK_FAMILY )
        g_object_notify ( object, "family" );
    if ( changed_mask & PANGO_FONT_MASK_STYLE )
        g_object_notify ( object, "style" );
    if ( changed_mask & PANGO_FONT_MASK_VARIANT )
        g_object_notify ( object, "variant" );
    if ( changed_mask & PANGO_FONT_MASK_WEIGHT )
        g_object_notify ( object, "weight" );
    if ( changed_mask & PANGO_FONT_MASK_STRETCH )
        g_object_notify ( object, "stretch" );
    if ( changed_mask & PANGO_FONT_MASK_SIZE )
    {
        g_object_notify ( object, "size" );
        g_object_notify ( object, "size-points" );
    }

    notify_set_changed ( object, set_changed_mask );

    g_object_thaw_notify ( object );
}

static void
ptk_text_renderer_set_property ( GObject *object,
                                 guint param_id,
                                 const GValue *value,
                                 GParamSpec *pspec )
{
    PtkTextRenderer * celltext = PTK_TEXT_RENDERER ( object );

    switch ( param_id )
    {
    case PROP_TEXT:
        g_free ( celltext->text );

        celltext->text = g_strdup ( g_value_get_string ( value ) );
        g_object_notify ( object, "text" );
        break;

    case PROP_BACKGROUND:
        {
            GdkColor color;

            if ( !g_value_get_string ( value ) )
                set_bg_color ( celltext, NULL );       /* reset to backgrounmd_set to FALSE */
            else if ( gdk_color_parse ( g_value_get_string ( value ), &color ) )
                set_bg_color ( celltext, &color );
            else
                g_warning ( "Don't know color `%s'", g_value_get_string ( value ) );

            g_object_notify ( object, "background-gdk" );
        }
        break;

    case PROP_FOREGROUND:
        {
            GdkColor color;

            if ( !g_value_get_string ( value ) )
                set_fg_color ( celltext, NULL );       /* reset to foreground_set to FALSE */
            else if ( gdk_color_parse ( g_value_get_string ( value ), &color ) )
                set_fg_color ( celltext, &color );
            else
                g_warning ( "Don't know color `%s'", g_value_get_string ( value ) );

            g_object_notify ( object, "foreground-gdk" );
        }
        break;

    case PROP_BACKGROUND_GDK:
        /* This notifies the GObject itself. */
        set_bg_color ( celltext, g_value_get_boxed ( value ) );
        break;

    case PROP_FOREGROUND_GDK:
        /* This notifies the GObject itself. */
        set_fg_color ( celltext, g_value_get_boxed ( value ) );
        break;

    case PROP_FONT:
        {
            PangoFontDescription *font_desc = NULL;
            const gchar *name;

            name = g_value_get_string ( value );

            if ( name )
                font_desc = pango_font_description_from_string ( name );

            set_font_description ( celltext, font_desc );

            pango_font_description_free ( font_desc );

        }
        break;

    case PROP_FONT_DESC:
        set_font_description ( celltext, g_value_get_boxed ( value ) );

        break;

    case PROP_UNDERLINE:
        celltext->underline_style = g_value_get_enum ( value );
        celltext->underline_set = TRUE;
        g_object_notify ( object, "underline-set" );

        break;

    case PROP_ELLIPSIZE:
        celltext->ellipsize = g_value_get_enum ( value );
        celltext->ellipsize_set = TRUE;
        g_object_notify ( object, "ellipsize-set" );
        break;

    case PROP_WRAP_MODE:
        celltext->wrap_mode = g_value_get_enum ( value );
        break;

    case PROP_WRAP_WIDTH:
        celltext->wrap_width = g_value_get_int ( value );
        break;

    case PROP_BACKGROUND_SET:
        celltext->background_set = g_value_get_boolean ( value );
        break;

    case PROP_FOREGROUND_SET:
        celltext->foreground_set = g_value_get_boolean ( value );
        break;

    case PROP_UNDERLINE_SET:
        celltext->underline_set = g_value_get_boolean ( value );
        break;

    case PROP_ELLIPSIZE_SET:
        celltext->ellipsize_set = g_value_get_boolean ( value );
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID ( object, param_id, pspec );
        break;
    }
}

/**
 * ptk_text_renderer_new:
 *
 * Creates a new #PtkTextRenderer. Adjust how text is drawn using
 * object properties. Object properties can be
 * set globally (with g_object_set()). Also, with #GtkTreeViewColumn,
 * you can bind a property to a value in a #GtkTreeModel. For example,
 * you can bind the "text" property on the cell renderer to a string
 * value in the model, thus rendering a different string in each row
 * of the #GtkTreeView
 *
 * Return value: the new cell renderer
 **/
GtkCellRenderer *
ptk_text_renderer_new ( void )
{
    return g_object_new ( PTK_TYPE_TEXT_RENDERER, NULL );
}

static void
add_attr ( PangoAttrList *attr_list,
           PangoAttribute *attr )
{
    attr->start_index = 0;
    attr->end_index = G_MAXINT;

    pango_attr_list_insert ( attr_list, attr );
}

static PangoLayout*
get_layout ( PtkTextRenderer *celltext,
             GtkWidget *widget,
             gboolean will_render,
             GtkCellRendererState flags )
{
    PangoAttrList * attr_list;
    PangoLayout *layout;
    PangoUnderline uline;

    layout = gtk_widget_create_pango_layout ( widget, celltext->text );
    pango_layout_set_alignment( layout, PANGO_ALIGN_CENTER );

    attr_list = pango_attr_list_new ();

    if ( will_render )
    {
        /* Add options that affect appearance but not size */

        /* note that background doesn't go here, since it affects
          * background_area not the PangoLayout area
        */

        if ( celltext->foreground_set
                && ( flags & GTK_CELL_RENDERER_SELECTED ) == 0 )
        {
            PangoColor color;

            color = celltext->foreground;

            add_attr ( attr_list,
                       pango_attr_foreground_new ( color.red, color.green, color.blue ) );
        }

    }

    add_attr ( attr_list, pango_attr_font_desc_new ( celltext->font ) );

    if ( celltext->underline_set )
        uline = celltext->underline_style;
    else
        uline = PANGO_UNDERLINE_NONE;

    if ( ( flags & GTK_CELL_RENDERER_PRELIT ) == GTK_CELL_RENDERER_PRELIT )
    {
        switch ( uline )
        {
        case PANGO_UNDERLINE_NONE:
            uline = PANGO_UNDERLINE_SINGLE;
            break;

        case PANGO_UNDERLINE_SINGLE:
            uline = PANGO_UNDERLINE_DOUBLE;
            break;

        default:
            break;
        }
    }

    if ( uline != PANGO_UNDERLINE_NONE )
        add_attr ( attr_list, pango_attr_underline_new ( celltext->underline_style ) );

    if ( celltext->ellipsize_set )
        pango_layout_set_ellipsize ( layout, celltext->ellipsize );
    else
        pango_layout_set_ellipsize ( layout, PANGO_ELLIPSIZE_NONE );

    if ( celltext->wrap_width != -1 )
    {
        pango_layout_set_width ( layout, celltext->wrap_width * PANGO_SCALE );
        pango_layout_set_wrap ( layout, celltext->wrap_mode );

        if ( pango_layout_get_line_count ( layout ) == 1 )
        {
            pango_layout_set_width ( layout, -1 );
            pango_layout_set_wrap ( layout, PANGO_WRAP_CHAR );
        }
    }
    else
    {
        pango_layout_set_width ( layout, -1 );
        pango_layout_set_wrap ( layout, PANGO_WRAP_CHAR );
    }

    pango_layout_set_attributes ( layout, attr_list );

    pango_attr_list_unref ( attr_list );

    return layout;
}

static void
get_size ( GtkCellRenderer *cell,
           GtkWidget *widget,
           GdkRectangle *cell_area,
           PangoLayout *layout,
           gint *x_offset,
           gint *y_offset,
           gint *width,
           gint *height )
{
    PtkTextRenderer * celltext = ( PtkTextRenderer * ) cell;
    PangoRectangle rect;

    if ( layout )
    {
        g_object_ref ( layout );
        pango_layout_set_alignment( layout, PANGO_ALIGN_CENTER );
    }
    else
        layout = get_layout ( celltext, widget, FALSE, 0 );

    pango_layout_get_pixel_extents ( layout, NULL, &rect );

    if ( height )
        * height = cell->ypad * 2 + rect.height;

    /* The minimum size for ellipsized labels is ~ 3 chars */
    if ( width )
    {
        if ( celltext->ellipsize )
        {
            PangoContext * context;
            PangoFontMetrics *metrics;
            gint char_width;

            context = pango_layout_get_context ( layout );
            metrics = pango_context_get_metrics ( context, widget->style->font_desc, pango_context_get_language ( context ) );

            char_width = pango_font_metrics_get_approximate_char_width ( metrics );
            pango_font_metrics_unref ( metrics );

            *width = cell->xpad * 2 + ( PANGO_PIXELS ( char_width ) * 3 );
        }
        else
        {
            *width = cell->xpad * 2 + rect.x + rect.width;
        }
    }

    if ( cell_area )
    {
        if ( x_offset )
        {
            if ( gtk_widget_get_direction ( widget ) == GTK_TEXT_DIR_RTL )
                * x_offset = ( 1.0 - cell->xalign ) * ( cell_area->width - ( rect.x + rect.width + ( 2 * cell->xpad ) ) );
            else
                *x_offset = cell->xalign * ( cell_area->width - ( rect.x + rect.width + ( 2 * cell->xpad ) ) );

            if ( celltext->ellipsize_set || celltext->wrap_width != -1 )
                * x_offset = MAX( *x_offset, 0 );
        }
        if ( y_offset )
        {
            *y_offset = cell->yalign * ( cell_area->height - ( rect.height + ( 2 * cell->ypad ) ) );
            *y_offset = MAX ( *y_offset, 0 );
        }
    }

    g_object_unref ( layout );
}


static void
ptk_text_renderer_get_size ( GtkCellRenderer *cell,
                             GtkWidget *widget,
                             GdkRectangle *cell_area,
                             gint *x_offset,
                             gint *y_offset,
                             gint *width,
                             gint *height )
{
    get_size ( cell, widget, cell_area, NULL,
               x_offset, y_offset, width, height );
}

static void
ptk_text_renderer_render ( GtkCellRenderer *cell,
                           GdkDrawable *window,
                           GtkWidget *widget,
                           GdkRectangle *background_area,
                           GdkRectangle *cell_area,
                           GdkRectangle *expose_area,
                           GtkCellRendererState flags )

{
    PtkTextRenderer * celltext = ( PtkTextRenderer * ) cell;
    PangoLayout *layout;
    GtkStateType state;
    gint x_offset;
    gint y_offset;
    gint width, height;
    gint focus_pad, focus_width;
    gint x, y;

    /* get focus width and padding */
    gtk_widget_style_get ( widget,
                           "focus-line-width", &focus_width,
                           "focus-padding", &focus_pad,
                           NULL );

    /* get text extent */
    layout = get_layout ( celltext, widget, TRUE, flags );
    get_size ( cell, widget, cell_area, layout, &x_offset, &y_offset, &width, &height );

    if ( !cell->sensitive )
    {
        state = GTK_STATE_INSENSITIVE;
    }
    else if ( ( flags & GTK_CELL_RENDERER_SELECTED ) == GTK_CELL_RENDERER_SELECTED )
    {
        if ( GTK_WIDGET_HAS_FOCUS ( widget ) )
            state = GTK_STATE_SELECTED;
        else
            state = GTK_STATE_ACTIVE;
    }
    else if ( ( flags & GTK_CELL_RENDERER_PRELIT ) == GTK_CELL_RENDERER_PRELIT &&
              GTK_WIDGET_STATE ( widget ) == GTK_STATE_PRELIGHT )
    {
        state = GTK_STATE_PRELIGHT;
    }
    else
    {
        if ( GTK_WIDGET_STATE ( widget ) == GTK_STATE_INSENSITIVE )
            state = GTK_STATE_INSENSITIVE;
        else
            state = GTK_STATE_NORMAL;
    }

    if(flags & (GTK_CELL_RENDERER_FOCUSED|GTK_CELL_RENDERER_SELECTED))
    {
        /* draw background color for selected state if needed */
        if( flags & GTK_CELL_RENDERER_SELECTED )
        {
            gdk_draw_rectangle ( window,
                                 widget->style->base_gc[ state ], TRUE,
                                 cell_area->x + x_offset, cell_area->y + y_offset,
                                 width, height );
        }

        /* draw the focus */
        if(flags & GTK_CELL_RENDERER_FOCUSED)
        {
            gtk_paint_focus( widget->style, window, GTK_WIDGET_STATE (widget),
                           NULL, widget, "icon_view",
                           cell_area->x + x_offset - focus_width,
                           cell_area->y + y_offset - focus_width,
                           width + focus_width * 2, height + focus_width * 2);
            flags &= ~GTK_CELL_RENDERER_FOCUSED;
        }
    }

    if ( celltext->ellipsize_set )
        pango_layout_set_width ( layout,
                                 ( cell_area->width - x_offset - 2 * cell->xpad ) * PANGO_SCALE );
    else if ( celltext->wrap_width == -1 )
        pango_layout_set_width ( layout, -1 );

    gtk_paint_layout ( widget->style,
                       window,
                       state,
                       TRUE,
                       expose_area,
                       widget,
                       "cellrenderertext",
                       cell_area->x + x_offset + cell->xpad,
                       cell_area->y + y_offset + cell->ypad,
                       layout );

    g_object_unref ( layout );
}

