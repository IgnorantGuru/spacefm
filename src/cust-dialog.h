#ifndef _CUST_DLG_H_
#define _CUST_DLG_H_

#include <glib.h>
#include "vfs-file-monitor.h"

G_BEGIN_DECLS

static const char* cdlg_option[] =  // order must match ElementType order
{
    "prefix",       "NAME|@FILE",
                    N_("Set bash variable name  (Default: \"dialog\")"),
    "title",        "TEXT|@FILE",
                    N_("Set window title"),
    "window-icon",  "ICON|@FILE",
                    N_("Set window icon"),
    "icon",         "ICON|@FILE [COMMAND...]",
                    N_("Add an icon"),
    "image",        "FILE|@FILE [COMMAND...]",
                    N_("Add an image"),
    "label",        "TEXT|@FILE [COMMAND...]",
                    N_("Add a text label"),
    "input",        "[TEXT|@FILE [SELSTART [SELEND [COMMAND...]]]]",
                    N_("Add a text entry"),
    "input-large",  "[TEXT|@FILE [SELSTART [SELEND [COMMAND...]]]]",
                    N_("Add a large text entry"),
    "password",     "[TEXT|@FILE [COMMAND...]]",
                    N_("Add a password entry"),
    "viewer",       "FILE|PIPE [--scroll]",
                    N_("Add a file or pipe viewer"),
    "editor",       "[FILE [SAVEFILE]]",
                    N_("Add multi-line text editor"),
    "checkbox",     "LABEL|@FILE [DEFAULT [COMMAND...]]",
                    N_("Add a checkbox option"),
    "radio",        "{LABEL [...] \"\"}|@FILE [DEFAULT|+INDEX [COMMAND...]]",
                    N_("Add a row of radio options"),
    "drop",         "{TEXT [...] \"\"}|@FILE [DEFAULT|+INDEX [COMMAND...]]",
                    N_("Add a drop-down list"),
    "combo",        "{TEXT [...] \"\"}|@FILE [DEFAULT|+INDEX [COMMAND...]]",
                    N_("Add a combo list"),
    "list",         "{TEXT [...] \"\"}|@FILE [DEFAULT|+INDEX [COMMAND...]]",
                    N_("Add a list box"),
    "mlist",        "{TEXT [...] \"\"}|@FILE [DEFAULT|+INDEX [COMMAND...]]",
                    N_("Add a list box with multiple selections"),
    "status",       "TEXT|@FILE [COMMAND...]",
                    N_("Add a status bar"),
    "progress",     "[@FILE]",
                    N_("Add a progress bar"),
    "hsep",         "[WIDTH|@FILE]",
                    N_("Add a horizontal line separator"),
    "vsep",         "[WIDTH|@FILE]",
                    N_("Add a vertical line separator"),
    "button",       "LABEL[;ICON]|STOCK|@FILE [COMMAND...]",
                    N_("Add STOCK dialog button, or LABEL button with ICON"),
    "free-button",  "LABEL[;ICON]|STOCK|@FILE [COMMAND...]",
                    N_("Add STOCK button, or LABEL button with ICON anywhere"),
    "hbox",         "[PAD|@FILE]",
                    N_("Add following widgets to a horizontal box"),
    "vbox",         "[PAD|@FILE]",
                    N_("Add following widgets to a vertical box"),
    "close-box",    "",
                    N_("Close the current box of widgets"),
    "width",        "WIDTH|@FILE",
                    N_("Set window width"),
    "height",       "HEIGHT|@FILE",
                    N_("Set window height"),
    "pad",          "PAD|@FILE",
                    N_("Set padding around widgets"),
    "font",         "FONT|@FILE",
                    N_("Set font"),
    "rcfile",       "FILE|@FILE",
                    N_("Use GTK RC theme file"),
    "timeout",      "[DELAY|@FILE]",
                    N_("Close window after DELAY seconds"),
    "browse",       "[DIR|FILE|@FILE [save] [dir] [multi] [confirm]]",
                    N_("Show a file/folder chooser or save dialog"),
    "browse-filter","FILTER|@FILE",
                    N_("Add a filename filter"),
    "command",      "FILE",
                    N_("Read commands from FILE")   // quit; focus; press
};
// TEXT starts with ~ for pango
// scroll vbox?
// menu?

typedef enum {
    CDLG_PREFIX,
    CDLG_TITLE,
    CDLG_WINDOW_ICON,
    CDLG_ICON,
    CDLG_IMAGE,
    CDLG_LABEL,
    CDLG_INPUT,
    CDLG_INPUT_LARGE,
    CDLG_PASSWORD,
    CDLG_VIEWER,
    CDLG_EDITOR,
    CDLG_CHECKBOX,
    CDLG_RADIO,
    CDLG_DROP,
    CDLG_COMBO,
    CDLG_LIST,
    CDLG_MLIST,
    CDLG_STATUS,
    CDLG_PROGRESS,
    CDLG_HSEP,
    CDLG_VSEP,
    CDLG_BUTTON,
    CDLG_FREE_BUTTON,
    CDLG_HBOX,
    CDLG_VBOX,
    CDLG_CLOSE_BOX,
    CDLG_WIDTH,
    CDLG_HEIGHT,
    CDLG_PAD,
    CDLG_FONT,
    CDLG_RCFILE,
    CDLG_TIMEOUT,
    CDLG_BROWSE,
    CDLG_BROWSE_FILTER,
    CDLG_COMMAND
} ElementType;

typedef struct
{
    ElementType type;
    char* name;
    GList* args;
    const char* def_val;
    char* val;
    GList* widgets;
    VFSFileMonitor* monitor;
    VFSFileMonitorCallback callback;
    const char* watch_file;
} CustomElement;


int custom_dialog_init( int argc, char *argv[] );



G_END_DECLS


#endif

