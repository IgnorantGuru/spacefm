/*
*  C Implementation: ptkutils
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "ptk-utils.h"
#include <glib.h>
#include <glib/gi18n.h>
#include "working-area.h"

#include "settings.h"
#include "gtk2-compat.h"
#include <gdk/gdkkeysyms.h>

GtkWidget* ptk_menu_new_from_data( PtkMenuItemEntry* entries,
                                   gpointer cb_data,
                                   GtkAccelGroup* accel_group )
{
  GtkWidget* menu;
  menu = gtk_menu_new();
  ptk_menu_add_items_from_data( menu, entries, cb_data, accel_group );
  return menu;
}

void ptk_menu_add_items_from_data( GtkWidget* menu,
                                   PtkMenuItemEntry* entries,
                                   gpointer cb_data,
                                   GtkAccelGroup* accel_group )
{
  PtkMenuItemEntry* ent;
  GtkWidget* menu_item = NULL;
  GtkWidget* sub_menu;
  GtkWidget* image;
  GSList* radio_group = NULL;
  const char* signal;

  for( ent = entries; ; ++ent )
  {
    if( G_LIKELY( ent->label ) )
    {
      /* Stock item */
      signal = "activate";
      if( G_UNLIKELY( PTK_IS_STOCK_ITEM(ent) ) )  {
        menu_item = gtk_image_menu_item_new_from_stock( ent->label, accel_group );
      }
      else if( G_LIKELY(ent->stock_icon) )  {
        if( G_LIKELY( ent->stock_icon > (char *)2 ) )  {
          menu_item = gtk_image_menu_item_new_with_mnemonic(_(ent->label));
          image = gtk_image_new_from_stock( ent->stock_icon, GTK_ICON_SIZE_MENU );
          gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM(menu_item), image );
        }
        else if( G_UNLIKELY( PTK_IS_CHECK_MENU_ITEM(ent) ) )  {
          menu_item = gtk_check_menu_item_new_with_mnemonic(_(ent->label));
          signal = "toggled";
        }
        else if( G_UNLIKELY( PTK_IS_RADIO_MENU_ITEM(ent) ) )  {
          menu_item = gtk_radio_menu_item_new_with_mnemonic( radio_group, _(ent->label) );
          if( G_LIKELY( PTK_IS_RADIO_MENU_ITEM( (ent + 1) ) ) )
            radio_group = gtk_radio_menu_item_get_group( GTK_RADIO_MENU_ITEM(menu_item) );
          else
            radio_group = NULL;
          signal = "toggled";
        }
      }
      else  {
        menu_item = gtk_menu_item_new_with_mnemonic(_(ent->label));
      }

      if( G_LIKELY(accel_group) && ent->key ) {
        gtk_widget_add_accelerator (menu_item, "activate", accel_group,
                                    ent->key, ent->mod, GTK_ACCEL_VISIBLE);
      }

      if( G_LIKELY(ent->callback) )  { /* Callback */
        g_signal_connect( menu_item, signal, ent->callback, cb_data);
      }

      if( G_UNLIKELY( ent->sub_menu ) )  { /* Sub menu */
        sub_menu = ptk_menu_new_from_data( ent->sub_menu, cb_data, accel_group );
        gtk_menu_item_set_submenu( GTK_MENU_ITEM(menu_item), sub_menu );
        ent->menu = sub_menu;  //MOD
      }
    }
    else
    {
      if( ! ent->stock_icon ) /* End of menu */
        break;
        menu_item = gtk_separator_menu_item_new();      
    }

    gtk_menu_shell_append ( GTK_MENU_SHELL(menu), menu_item );
    if( G_UNLIKELY(ent->ret) ) {// Return
      *ent->ret = menu_item;
      ent->ret = NULL;
    }

  }
}

#if 0
GtkWidget* ptk_toolbar_add_items_from_data( GtkWidget* toolbar,
                                            PtkToolItemEntry* entries,
                                            gpointer cb_data,
                                            GtkTooltips* tooltips )
{
  GtkWidget* btn;
  PtkToolItemEntry* ent;
  GtkWidget* image;
  GtkWidget* menu;
  GtkIconSize icon_size = gtk_toolbar_get_icon_size (GTK_TOOLBAR (toolbar));
  GSList* radio_group = NULL;

  for( ent = entries; ; ++ent )
  {
    /* Normal tool item */
    if( G_LIKELY( ent->stock_icon || ent->tooltip || ent->label ) )
    {
      /* Stock item */
      if( G_LIKELY(ent->stock_icon) )
        image = gtk_image_new_from_stock( ent->stock_icon, icon_size );
      else
        image = NULL;

      if( G_LIKELY( ! ent->menu ) )  { /* Normal button */
        if( G_UNLIKELY( PTK_IS_STOCK_ITEM(ent) ) )
          btn = GTK_WIDGET(gtk_tool_button_new_from_stock ( ent->label ));
        else
          btn = GTK_WIDGET(gtk_tool_button_new ( image, _(ent->label) ));
      }
      else if( G_UNLIKELY( PTK_IS_CHECK_TOOL_ITEM(ent) ) )  {
        if( G_UNLIKELY( PTK_IS_STOCK_ITEM(ent) ) )
          btn = GTK_WIDGET(gtk_toggle_tool_button_new_from_stock(ent->label));
        else {
          btn = GTK_WIDGET(gtk_toggle_tool_button_new ());
          gtk_tool_button_set_icon_widget( GTK_TOOL_BUTTON(btn), image );
          gtk_tool_button_set_label(GTK_TOOL_BUTTON(btn), _(ent->label));
        }
      }
      else if( G_UNLIKELY( PTK_IS_RADIO_TOOL_ITEM(ent) ) )  {
        if( G_UNLIKELY( PTK_IS_STOCK_ITEM(ent) ) )
          btn = GTK_WIDGET(gtk_radio_tool_button_new_from_stock( radio_group, ent->label ));
        else {
          btn = GTK_WIDGET(gtk_radio_tool_button_new( radio_group ));
          if( G_LIKELY( PTK_IS_RADIO_TOOL_ITEM( (ent + 1) ) ) )
            radio_group = gtk_radio_tool_button_get_group( GTK_RADIO_TOOL_BUTTON(btn) );
          else
            radio_group = NULL;
          gtk_tool_button_set_icon_widget( GTK_TOOL_BUTTON(btn), image );
          gtk_tool_button_set_label(GTK_TOOL_BUTTON(btn), _(ent->label));
        }
      }
      else if( ent->menu )  {
        if( G_UNLIKELY( PTK_IS_STOCK_ITEM(ent) ) )
          btn = GTK_WIDGET(gtk_menu_tool_button_new_from_stock ( ent->label ));
        else {
          btn = GTK_WIDGET(gtk_menu_tool_button_new ( image, _(ent->label) ));
          if( G_LIKELY( 3 < (int)ent->menu ) )  { /* Sub menu */
            menu = ptk_menu_new_from_data( ent->menu, cb_data, NULL );
            gtk_menu_tool_button_set_menu( GTK_MENU_TOOL_BUTTON(btn), menu );
          }
        }
      }

      if( G_LIKELY(ent->callback) )  { /* Callback */
        if( G_LIKELY( ent->menu == NULL || ent->menu == PTK_EMPTY_MENU) )
          g_signal_connect( btn, "clicked", ent->callback, cb_data);
        else
          g_signal_connect( btn, "toggled", ent->callback, cb_data);
      }

      if( G_LIKELY(ent->tooltip) )
        gtk_tool_item_set_tooltip (GTK_TOOL_ITEM (btn), tooltips, _(ent->tooltip), NULL);
    }
    else
    {
      if( ! PTK_IS_SEPARATOR_TOOL_ITEM(ent) ) /* End of menu */
        break;
      btn = (GtkWidget*)gtk_separator_tool_item_new ();
    }

    gtk_toolbar_insert ( GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(btn), -1 );

    if( G_UNLIKELY(ent->ret) ) {/* Return */
      *ent->ret = btn;
      ent->ret = NULL;
    }
  }
  return NULL;
}
#endif

void ptk_show_error(GtkWindow* parent, const char* title, const char* message )
{
    char* msg = replace_string( message, "%", "%%", FALSE );
    GtkWidget* dlg = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL,
                        GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, msg, NULL);
    g_free( msg );
    if( title )
        gtk_window_set_title( (GtkWindow*)dlg, title );
    xset_set_window_icon( GTK_WINDOW( dlg ) );
    gtk_dialog_run( GTK_DIALOG(dlg) );
    gtk_widget_destroy( dlg );
}

/* Make the size of dialogs smaller by breaking GNOME HIG
 * http://library.gnome.org/devel/hig-book/stable/design-window.html.en
 * According to GNOME HIG, spacings are increased by the multiples of 6.
 * Change them to the multiples of 1 can greatly reduce the required size
 * while still keeping some degree of spacing.
 */
static void break_gnome_hig( GtkWidget* w, gpointer _factor )
{
    int factor = GPOINTER_TO_INT(_factor);

    /* g_debug( G_OBJECT_TYPE_NAME(w) ); */
    if( GTK_IS_CONTAINER(w) )
    {
        int val;
        val = gtk_container_get_border_width( (GtkContainer*)w );

        /* border of dialog defaults to 12 under gnome */
        if( GTK_IS_DIALOG(w) )
        {
            if( val > 0 )
                gtk_container_set_border_width( (GtkContainer*)w, val / factor );
        }
        else
        {
            if( GTK_IS_BOX(w) ) /* boxes, spacing defaults to 6, 12, 18 under gnome */
            {
                int spacing = gtk_box_get_spacing( (GtkBox*)w );
                gtk_box_set_spacing( (GtkBox*)w, spacing / factor );
            }
            else if( GTK_IS_TABLE(w) ) /* tables, spacing defaults to 6, 12, 18 under gnome */
            {
                int spacing;
                int col, row;
                g_object_get( w, "n-columns", &col, "n-rows", &row, NULL );
                if( col > 1 )
                {
                    --col;
                    while( --col >= 0 )
                    {
                        spacing = gtk_table_get_col_spacing( (GtkTable*)w, col );
                        if( spacing > 0 )
                            gtk_table_set_col_spacing( (GtkTable*)w, col, spacing / factor );
                    }
                }
                if( row > 1 )
                {
                    --row;
                    while( --row >= 0 )
                    {
                        spacing = gtk_table_get_row_spacing( (GtkTable*)w, row );
                        if( spacing > 0 )
                            gtk_table_set_row_spacing( (GtkTable*)w, row, spacing / factor );
                    }
                }
                /* FIXME: handle default spacings */
            }
            else if( GTK_IS_ALIGNMENT(w) ) /* groups, has 12 px indent by default */
            {
                int t, b, l, r;
                gtk_alignment_get_padding( (GtkAlignment*)w, &t, &b, &l, &r );
                if( l > 0 )
                {
                    l /= (factor / 2); /* groups still need proper indent not to hurt usability */
                    gtk_alignment_set_padding( (GtkAlignment*)w, t, b, l, r );
                }
            }
            if( val > 0 )
                gtk_container_set_border_width( (GtkContainer*)w, val * 2 / factor );
        }
        gtk_container_foreach( (GtkContainer*)w, break_gnome_hig, GINT_TO_POINTER(factor) );
    }
}

/* Because GNOME HIG causes some usability problems under limited screen size,
 * this API is provided to adjust the dialogs, and try to fit them into
 * small screens via totally breaking GNOME HIG and compress spacings.
 */
void ptk_dialog_fit_small_screen( GtkDialog* dlg )
{
    GtkRequisition req;
    GdkRectangle wa;
    GtkAllocation allocation;
    int dw, dh, i;

    get_working_area( gtk_widget_get_screen((GtkWidget*)dlg), &wa );
    gtk_widget_size_request( GTK_WIDGET(dlg), &req );

    /* Try two times, so we won't be too aggrassive if mild shinkage can do the job.
     * First time shrink all spacings to their 1/3.
     * If this is not enough, shrink them again by dividing all spacings by 2. (1/6 size now)
     */
    for( i =0; (req.width > wa.width || req.height > wa.height) && i < 2; ++i )
    {
        break_gnome_hig( GTK_WIDGET(dlg), GINT_TO_POINTER((i == 0 ? 3 : 2)) );
        gtk_widget_size_request( GTK_WIDGET(dlg), &req );
        /* g_debug("%d, %d", req.width, req.height ); */
    }

    if( gtk_widget_get_realized( GTK_WIDGET(dlg) ) )
    {
        gtk_widget_get_allocation ( (GtkWidget*)dlg, &allocation);
        gboolean changed = FALSE;
        if( allocation.width > wa.width )
        {
            dw = wa.width;
            changed = TRUE;
        }
        if( allocation.height > wa.width )
        {
            dh = wa.height;
            changed = TRUE;
        }
        if( changed )
            gtk_window_resize( (GtkWindow*)dlg, dw, dh );
        /* gtk_window_move( dlg, 0, 0 ); */
    }
    else
    {
        gtk_window_get_default_size( (GtkWindow*)dlg, &dw, &dh );
        if( dw > wa.width )
            dw = wa.width;
        if( dh > wa.height )
            dh = wa.height;
        gtk_window_set_default_size( GTK_WINDOW(dlg), dw, dh );
    }
}

typedef struct
{
    GMainLoop* lp;
    int response;
}DlgRunData;

static gboolean on_dlg_delete_event( GtkWidget* dlg, GdkEvent* evt, DlgRunData* data )
{
    return TRUE;
}

static void on_dlg_response( GtkDialog* dlg, int response, DlgRunData* data )
{
    data->response = response;
    if( g_main_loop_is_running( data->lp ) )
        g_main_loop_quit( data->lp );
}

int ptk_dialog_run_modaless( GtkDialog* dlg )
{
    DlgRunData data = {0};
    data.lp = g_main_loop_new( NULL, FALSE );

    guint deh = g_signal_connect( dlg, "delete_event", G_CALLBACK(on_dlg_delete_event), &data );
    guint rh = g_signal_connect( dlg, "response", G_CALLBACK(on_dlg_response), &data );

    gtk_window_present( (GtkWindow*)dlg );

    GDK_THREADS_LEAVE();
    g_main_loop_run(data.lp);
    GDK_THREADS_ENTER();

    g_main_loop_unref(data.lp);

    g_signal_handler_disconnect( dlg, deh );
    g_signal_handler_disconnect( dlg, rh );

    return data.response;
}

GtkBuilder* _gtk_builder_new_from_file( const char* file, GError** err )
{
    GtkBuilder* builder = gtk_builder_new();
    if( G_UNLIKELY( ! gtk_builder_add_from_file( builder, file, err ) ) )
    {
        g_object_unref( builder );
        return NULL;
    }
    return builder;
}

void transpose_nonlatin_keypress( GdkEventKey* event )
{
    if ( !( event && event->keyval != 0 ) )
        return;
    
    // is already a latin key?
    if ( ( GDK_KEY_0 <= event->keyval && event->keyval <= GDK_KEY_9 ) ||
         ( GDK_KEY_A <= event->keyval && event->keyval <= GDK_KEY_Z ) ||
         ( GDK_KEY_a <= event->keyval && event->keyval <= GDK_KEY_z ) )
        return;
    
    // We have a non-latin char, try other keyboard groups
    GdkKeymapKey* keys = NULL;
    guint *keyvals;
    gint n_entries;
    gint level;
    gint n;

    if ( gdk_keymap_translate_keyboard_state( NULL,
                                              event->hardware_keycode,
                                              (GdkModifierType)event->state,
                                              event->group,
                                              NULL, NULL, &level, NULL )
        && gdk_keymap_get_entries_for_keycode( NULL,
                                               event->hardware_keycode,
                                               &keys, &keyvals,
                                               &n_entries ) )
    {
        for ( n = 0; n < n_entries; n++ )
        {
            if ( keys[n].group == event->group )
                // Skip keys from the same group
                continue;
            if ( keys[n].level != level )
                // Allow only same level keys
                continue;
            if ( ( GDK_KEY_0 <= keyvals[n] && keyvals[n] <= GDK_KEY_9 ) ||
                 ( GDK_KEY_A <= keyvals[n] && keyvals[n] <= GDK_KEY_Z ) ||
                 ( GDK_KEY_a <= keyvals[n] && keyvals[n] <= GDK_KEY_z ) )
            {
                // Latin character found
                event->keyval = keyvals[n];
                break;
            }
        }
        g_free( keys );
        g_free( keyvals );
    }
}

