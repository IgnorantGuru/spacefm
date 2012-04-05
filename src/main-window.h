#ifndef _MAIN_WINDOW_H_
#define _MAIN_WINDOW_H_

#include <gtk/gtk.h>
#include "ptk-file-browser.h"
#include "ptk-file-task.h"  //MOD

G_BEGIN_DECLS

#define FM_TYPE_MAIN_WINDOW             (fm_main_window_get_type())
#define FM_MAIN_WINDOW(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),  FM_TYPE_MAIN_WINDOW, FMMainWindow))
#define FM_MAIN_WINDOW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass),  FM_TYPE_MAIN_WINDOW, FMMainWindowClass))
#define FM_IS_MAIN_WINDOW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FM_TYPE_MAIN_WINDOW))
#define FM_IS_MAIN_WINDOW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass),  FM_TYPE_MAIN_WINDOW))
#define FM_MAIN_WINDOW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj),  FM_TYPE_MAIN_WINDOW, FMMainWindowClass))

typedef struct _FMMainWindow
{
    /* Private */
    GtkWindow parent;

    /* protected */
    GtkWidget *main_vbox;
    GtkWidget *menu_bar;
    //MOD
    GtkMenu* file_menu_item;
    GtkMenu* view_menu_item;
    GtkMenu* book_menu_item;
    GtkMenu* plug_menu_item;
    GtkMenu* tool_menu_item;
    GtkMenu* help_menu_item;
    GtkMenu* book_menu;
    GtkMenu* plug_menu;
    GtkNotebook* notebook;  //MOD changed use to current panel
    GtkNotebook* panel[4];
    int panel_slide_x[4];
    int panel_slide_y[4];
    int panel_slide_s[4];
    GtkToolbar* panelbar;
    GtkWidget* panel_btn[4];
    GtkImage* panel_image[4];
    int curpanel;
    GtkHPaned* hpane_top;
    GtkHPaned* hpane_bottom;
    GtkVPaned* vpane;
    GtkVPaned* task_vpane;
    GtkScrolledWindow* task_scroll;
    GtkTreeView* task_view;
    
    guint autosave_timer;



//  GtkWidget* toolbar;
//  GtkEntry* address_bar;
//  GtkWidget *bookmarks;
//  GtkWidget *status_bar;
  //gint splitter_pos;
    
  /* Check menu items & tool items */
/*
GtkCheckMenuItem* open_side_pane_menu;
  GtkCheckMenuItem* show_location_menu;
  GtkCheckMenuItem* show_dir_tree_menu;
  GtkCheckMenuItem* show_location_bar_menu;
  GtkCheckMenuItem* show_hidden_files_menu;

  GtkCheckMenuItem* view_as_icon;
  GtkCheckMenuItem* view_as_compact_list;
  GtkCheckMenuItem* view_as_list;

  GtkCheckMenuItem* sort_by_name;
  GtkCheckMenuItem* sort_by_size;
  GtkCheckMenuItem* sort_by_mtime;
  GtkCheckMenuItem* sort_by_type;
  GtkCheckMenuItem* sort_by_perm;
  GtkCheckMenuItem* sort_by_owner;
  GtkCheckMenuItem* sort_ascending;
  GtkCheckMenuItem* sort_descending;

  GtkToggleToolButton* open_side_pane_btn;
  GtkWidget* back_btn;
  GtkWidget* forward_btn;
*/

  GtkAccelGroup *accel_group;
  GtkTooltips *tooltips;

  GtkWindowGroup* wgroup;
  int n_busy_tasks;
}FMMainWindow;

typedef struct _FMMainWindowClass
{
  GtkWindowClass parent;

}FMMainWindowClass;

GType fm_main_window_get_type (void);

GtkWidget* fm_main_window_new();

/* Utility functions */
GtkWidget* fm_main_window_get_current_file_browser( FMMainWindow* mainWindow );

void fm_main_window_add_new_tab( FMMainWindow* main_window,
                                 const char* folder_path );


GtkWidget* fm_main_window_create_tab_label( FMMainWindow* main_window,
                                            PtkFileBrowser* file_browser );

void fm_main_window_update_tab_label( FMMainWindow* main_window,
                                      PtkFileBrowser* file_browser,
                                      const char * path );


void fm_main_window_preference( FMMainWindow* main_window );

/* get last active window */
FMMainWindow* fm_main_window_get_last_active();

/* get all windows
 * The returned GList is owned and used internally by FMMainWindow, and
 * should not be freed.
*/
const GList* fm_main_window_get_all();

void fm_main_window_open_terminal( GtkWindow* parent,
                                   const char* path );
void main_task_view_update_task( PtkFileTask* task );
void main_task_view_remove_task( PtkFileTask* task );
void on_close_notebook_page( GtkButton* btn, PtkFileBrowser* file_browser );
void show_panels( GtkMenuItem* item, FMMainWindow* main_window );
void show_panels_all_windows( GtkMenuItem* item, FMMainWindow* main_window );
void update_views_all_windows( GtkWidget* item, PtkFileBrowser* file_browser );
void rebuild_toolbar_all_windows( int job, PtkFileBrowser* file_browser );
gboolean main_write_exports( VFSFileTask* vtask, const char* value, FILE* file );
void main_update_fonts( GtkWidget* widget, PtkFileBrowser* file_browser );
void on_reorder( GtkWidget* item, GtkWidget* parent );
char* main_window_get_tab_cwd( PtkFileBrowser* file_browser, int tab_num );
char* main_window_get_panel_cwd( PtkFileBrowser* file_browser, int panel_num );
void main_window_get_counts( PtkFileBrowser* file_browser, int* panel_count,
                                                int* tab_count, int* tab_num );
gboolean main_window_panel_is_visible( PtkFileBrowser* file_browser, int panel );
void main_window_open_in_panel( PtkFileBrowser* file_browser, int panel_num,
                                                        char* file_path );
void main_window_autosave( PtkFileBrowser* file_browser );
void main_window_on_plugins_change( FMMainWindow* main_window );
void main_window_root_bar_all();
void main_window_rubberband_all();
void main_window_update_bookmarks();
void main_context_fill( PtkFileBrowser* file_browser, XSetContext* c );
void set_panel_focus( FMMainWindow* main_window, PtkFileBrowser* file_browser );
void focus_panel( GtkMenuItem* item, gpointer mw, int p );


G_END_DECLS

#endif
