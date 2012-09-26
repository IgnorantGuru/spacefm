#ifndef __GTK2_COMPAT_H
#define __GTK2_COMPAT_H

#if GTK_CHECK_VERSION(2, 24, 0)
#else
#define gtk_combo_box_text_new_with_entry gtk_combo_box_new_text
#define gtk_combo_box_text_new gtk_combo_box_new_text
#define gtk_combo_box_text_get_active_text gtk_combo_box_get_active_text
#define gtk_combo_box_text_append_text gtk_combo_box_append_text
#define gtk_combo_box_set_entry_text_column gtk_combo_box_entry_set_text_column
#define gtk_combo_box_text_prepend_text gtk_combo_box_prepend_text
#define GTK_COMBO_BOX_TEXT GTK_COMBO_BOX
static inline gint gdk_window_get_width (GdkWindow *window) { gint width; gdk_drawable_get_size(GDK_DRAWABLE(window), &width, NULL); return width;}
static inline gint gdk_window_get_height (GdkWindow *window) { gint height; gdk_drawable_get_size(GDK_DRAWABLE(window), NULL, &height); return height;}
#endif

#if GTK_CHECK_VERSION(2, 22, 0)
#else
#define gdk_drag_context_get_suggested_action(dc) dc->suggested_action
#define gdk_drag_context_get_selected_action(dc) dc->action
#define gdk_drag_context_get_actions(dc) dc->actions
#define gdk_drag_context_list_targets(dc) dc->targets
#define gtk_window_has_group(window) (window->group != NULL)
#endif

#if GTK_CHECK_VERSION(2, 20, 0)
#else
#define gtk_statusbar_get_message_area(widget) widget->frame
#define gtk_widget_get_realized GTK_WIDGET_REALIZED
#define gtk_widget_get_mapped GTK_WIDGET_MAPPED
static inline void gtk_widget_set_realized(GtkWidget *widget, gboolean realized) { if (realized) GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED); else GTK_WIDGET_UNSET_FLAGS (widget, GTK_REALIZED); }
#endif

#endif /* __GTK2_COMPAT_H */
