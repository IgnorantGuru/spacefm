#ifndef _CUST_DLG_H_
#define _CUST_DLG_H_

#include <glib.h>
#include "vfs-file-monitor.h"

G_BEGIN_DECLS

static const char* cdlg_option[] =  // order must match ElementType order
{
    "title",        "TEXT|@FILE",
                    N_("Set window title"),
    "window-icon",  "ICON|@FILE",
                    N_("Set window icon"),
    "label",        "[--wrap|--nowrap] LABEL|@FILE",
                    N_("Add a text label"),
    "button",       "LABEL[:ICON]|STOCK|@FILE [COMMAND...]",
                    N_("Add STOCK dialog button, or LABEL button with ICON"),
    "free-button",  "LABEL[:ICON]|STOCK|@FILE [COMMAND...]",
                    N_("Add STOCK button, or LABEL button with ICON anywhere"),
    "input",        "[--select START[:END]] [TEXT|@FILE [COMMAND...]]",
                    N_("Add a text entry"),
    "input-large",  "[--select START[:END]] [TEXT|@FILE [COMMAND...]]",
                    N_("Add a large text entry"),
    "password",     "[TEXT|@FILE [COMMAND...]]",
                    N_("Add a password entry"),
    "viewer",       "[--scroll] FILE|PIPE [SAVEFILE]",
                    N_("Add a file or pipe viewer"),
    "editor",       "[FILE [SAVEFILE]]",
                    N_("Add multi-line text editor"),
    "check",        "LABEL [VALUE|@FILE [COMMAND...]]",
                    N_("Add a checkbox option"),
    "radio",        "LABEL [VALUE|@FILE [COMMAND...]]",
                    N_("Add a radio option"),
    "icon",         "ICON|@FILE [COMMAND...]",
                    N_("Add an icon"),
    "image",        "FILE|@FILE [COMMAND...]",
                    N_("Add an image"),
/*
    "status",       "TEXT|@FILE [COMMAND...]",
                    N_("Add a status bar"),
*/
    "progress",     "[VALUE|pulse|@FILE]",
                    N_("Add a progress bar"),
    "hsep",         "( no arguments )",
                    N_("Add a horizontal line separator"),
    "vsep",         "( no arguments )",
                    N_("Add a vertical line separator"),
    "timeout",      "[DELAY|@FILE]",
                    N_("Automatically close window after DELAY seconds"),
    "drop",         "{TEXT... --}|@FILE [DEFAULT|+N|@FILE [COMMAND...]]",
                    N_("Add a drop-down list.  COMMAND run when clicked."),
    "combo",        "{TEXT... --}|@FILE [DEFAULT|+N|@FILE [COMMAND...]]",
                    N_("Add a combo list.  COMMAND run when Enter pressed."),
    "list",         "{[^HEAD[:TYPE]] [--colwidth=W] TEXT... --}|@FILE [COMMAND...]]",
                    // ^HIDE   hidden column (must be first) for data return  (int or double or string no progress)
                    // use --colwidth=W inside column list
                    N_("Add a list box.  COMMAND run when double-clicked."),
    "mlist",        "{[^HEAD[:TYPE]] [--colwidth=W] TEXT... --}|@FILE [COMMAND...]]",
                    N_("Add a list box with multiple selections"),
    "chooser",      "[CHOOSER-OPTIONS] [DIR|FILE|@FILE [COMMAND...]]",
                    N_("Options: [--save] [--dir] [--multi] [--filter F[:F...]]"),
    "prefix",       "NAME|@FILE",
                    N_("Set base variable name  (Default: \"dialog\")"),
    "window-size",  "\"WIDTHxHEIGHT [PAD]\"|@FILE",
                    N_("Set minimum width, height, padding (-1 = don't change)"),
    "hbox",         "[--compact] [PAD|@FILE]",
                    N_("Add following widgets to a horizontal box"),
    "vbox",         "[--compact] [PAD|@FILE]",
                    N_("Add following widgets to a vertical box"),
    "close-box",    "",
                    N_("Close the current box of widgets"),
    "keypress",     "KEYCODE MODIFIER COMMAND...",
                    N_("Run COMMAND when a key combination is pressed"),
    "click",       "COMMAND...",
                    N_("Run COMMAND when an element is clicked or focused"),
    "window-close", "COMMAND...",
                    N_("Run COMMAND on window close attempt"),
    "command",      "FILE|PIPE [COMMAND...]",
                    N_("Read commands from FILE or PIPE.  COMMAND for init.")
};
// TEXT starts with ~ for pango
// COMMAND internal -- external -- internal ...
// scroll vbox?
// menu?

typedef enum {
    CDLG_TITLE,
    CDLG_WINDOW_ICON,
    CDLG_LABEL,
    CDLG_BUTTON,
    CDLG_FREE_BUTTON,
    CDLG_INPUT,
    CDLG_INPUT_LARGE,
    CDLG_PASSWORD,
    CDLG_VIEWER,
    CDLG_EDITOR,
    CDLG_CHECKBOX,
    CDLG_RADIO,
    CDLG_ICON,
    CDLG_IMAGE,
    //CDLG_STATUS,
    CDLG_PROGRESS,
    CDLG_HSEP,
    CDLG_VSEP,
    CDLG_TIMEOUT,
    CDLG_DROP,
    CDLG_COMBO,
    CDLG_LIST,
    CDLG_MLIST,
    CDLG_CHOOSER,
    CDLG_PREFIX,
    CDLG_WINDOW_SIZE,
    CDLG_HBOX,
    CDLG_VBOX,
    CDLG_CLOSE_BOX,
    CDLG_KEYPRESS,
    CDLG_CLICK,
    CDLG_WINDOW_CLOSE,
    CDLG_COMMAND
} ElementType;

typedef struct
{
    ElementType type;
    char* name;
    GList* args;
    char* val;
    GList* widgets;
    GList* cmd_args;
    const char* def_val;
    VFSFileMonitor* monitor;
    VFSFileMonitorCallback callback;
    guint timeout;
    guint update_timeout;
    const char* watch_file;
    int option;
    int option2;
} CustomElement;

static const char* cdlg_cmd[] =
{
    "noop",         "( any arguments )",
                    N_("No operation - does nothing but evaluate arguments"),
    "close",        "[REVERSE]",     // exit status ?
                    N_("Close the dialog"),
    "press",        "BUTTON-NAME",
                    N_("Press button named BUTTON-NAME"),
    "set",          "NAME VALUE",
                    N_("Set element NAME to VALUE"),
    "select",       "NAME [VALUE]",  // also do for inputs?
                    N_("Select item VALUE (or first/all) in element NAME"),
    "unselect",     "NAME [VALUE]",
                    N_("Unselect item VALUE (or all) in element NAME"),
    "focus",        "[NAME [REVERSE]]",
                    N_("Focus element NAME, or raise dialog window"),
    "hide",         "NAME [REVERSE]",
                    N_("Hide element NAME"),
    "show",         "[NAME [REVERSE]]",
                    N_("Show element NAME if previously hidden"),
    "disable",      "NAME [REVERSE]",
                    N_("Disable element NAME"),
    "enable",       "NAME [REVERSE]",
                    N_("Enable element NAME if previously disabled"),
    "source",       "FILE",
                    N_("Save files and write source output to FILE")
};
// special NAME = title windowtitle windowicon windowsize

enum {
    CMD_NOOP,
    CMD_CLOSE,
    CMD_PRESS,
    CMD_SET,
    CMD_SELECT,  //allow chooser ?
    CMD_UNSELECT,
    CMD_FOCUS,
    CMD_HIDE,
    CMD_SHOW,
    CMD_DISABLE,
    CMD_ENABLE,
    CMD_SOURCE
};


int custom_dialog_init( int argc, char *argv[] );



G_END_DECLS


#endif

