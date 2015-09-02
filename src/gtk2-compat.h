#ifndef __GTK2_COMPAT_H
#define __GTK2_COMPAT_H

#if GTK_CHECK_VERSION(3, 0, 0)
#else
#define gtk_menu_shell_get_selected_item(mc) mc->active_menu_item
#endif

#if GTK_CHECK_VERSION(2, 24, 0)
#else
#define gtk_combo_box_text_new_with_entry gtk_combo_box_entry_new_text
#define gtk_combo_box_text_new gtk_combo_box_new_text
#define gtk_combo_box_text_get_active_text gtk_combo_box_get_active_text
#define gtk_combo_box_text_append_text gtk_combo_box_append_text
static inline void gtk_combo_box_set_entry_text_column( 
                              GtkComboBox *combo_box, gint text_column )
{
    gtk_combo_box_entry_set_text_column( GTK_COMBO_BOX_ENTRY( combo_box ),
                                                   text_column );
}
#define gtk_combo_box_text_prepend_text gtk_combo_box_prepend_text
#define GTK_COMBO_BOX_TEXT GTK_COMBO_BOX
static inline gint gdk_window_get_width (GdkWindow *window)
{ 
    gint width;
    gdk_drawable_get_size(GDK_DRAWABLE(window), &width, NULL);
    return width;
}
static inline gint gdk_window_get_height (GdkWindow *window)
{
    gint height;
    gdk_drawable_get_size(GDK_DRAWABLE(window), NULL, &height);
    return height;
}
#define gdk_x11_window_lookup_for_display gdk_window_lookup_for_display
#endif

#if GTK_CHECK_VERSION(2, 22, 0)
#else
#define gdk_drag_context_get_suggested_action(dc) dc->suggested_action
#define gdk_drag_context_get_selected_action(dc) dc->action
#define gdk_drag_context_get_actions(dc) dc->actions
#define gdk_drag_context_list_targets(dc) dc->targets
#define gtk_window_has_group(window) (window->group != NULL)

#define GDK_KEY_space GDK_space
#define GDK_KEY_Return GDK_Return
#define GDK_KEY_ISO_Enter GDK_ISO_Enter
#define GDK_KEY_KP_Enter GDK_KP_Enter
#define GDK_KEY_Up GDK_Up
#define GDK_KEY_KP_Up GDK_KP_Up
#define GDK_KEY_Down GDK_Down
#define GDK_KEY_KP_Down GDK_KP_Down
#define GDK_KEY_Home GDK_Home
#define GDK_KEY_KP_Home GDK_KP_Home
#define GDK_KEY_End GDK_End
#define GDK_KEY_KP_End GDK_KP_End
#define GDK_KEY_Page_Up GDK_Page_Up
#define GDK_KEY_KP_Page_Up GDK_KP_Page_Up
#define GDK_KEY_Page_Down GDK_Page_Down
#define GDK_KEY_KP_Page_Down GDK_KP_Page_Down
#define GDK_KEY_Right GDK_Right
#define GDK_KEY_KP_Right GDK_KP_Right
#define GDK_KEY_Left GDK_Left
#define GDK_KEY_KP_Left GDK_KP_Left
#define GDK_KEY_Escape GDK_Escape
#define GDK_KEY_Delete GDK_Delete
#define GDK_KEY_BackSpace GDK_BackSpace
#define GDK_KEY_Menu GDK_Menu
#define GDK_KEY_Tab GDK_Tab
#define GDK_KEY_F1 GDK_F1
#define GDK_KEY_F2 GDK_F2
#define GDK_KEY_F3 GDK_F3
#define GDK_KEY_F4 GDK_F4
#define GDK_KEY_Insert GDK_Insert
#define GDK_KEY_0 GDK_0
#define GDK_KEY_9 GDK_9
#define GDK_KEY_a GDK_a
#define GDK_KEY_c GDK_c
#define GDK_KEY_e GDK_e
#define GDK_KEY_g GDK_g
#define GDK_KEY_i GDK_i
#define GDK_KEY_k GDK_k
#define GDK_KEY_n GDK_n
#define GDK_KEY_p GDK_p
#define GDK_KEY_v GDK_v
#define GDK_KEY_w GDK_w
#define GDK_KEY_x GDK_x
#define GDK_KEY_z GDK_z
#define GDK_KEY_A GDK_A
#define GDK_KEY_F GDK_F
#define GDK_KEY_G GDK_G
#define GDK_KEY_W GDK_W
#define GDK_KEY_Z GDK_Z

#endif

#if GTK_CHECK_VERSION(2, 20, 0)
#else
#define gtk_statusbar_get_message_area(widget) widget->frame
#define gtk_widget_get_realized GTK_WIDGET_REALIZED
#define gtk_widget_get_mapped GTK_WIDGET_MAPPED
static inline void gtk_widget_set_realized( GtkWidget *widget,
                                            gboolean realized )
{
    if ( realized )
        GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
    else
        GTK_WIDGET_UNSET_FLAGS (widget, GTK_REALIZED);
}
#endif

#endif /* __GTK2_COMPAT_H */
