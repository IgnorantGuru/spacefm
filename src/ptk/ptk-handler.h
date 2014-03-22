/*
 * SpaceFM ptk-handler.h
 * 
 * Copyright (C) 2013-2014 OmegaPhil <OmegaPhil+SpaceFM@gmail.com>
 * Copyright (C) 2014 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>
 * 
 * License: See COPYING file
 * 
*/

#ifndef _PTK_HANDLER_H_
#define _PTK_HANDLER_H_

#include <gtk/gtk.h>
#include <glib.h>

#include "settings.h"
#include "ptk-file-browser.h"

G_BEGIN_DECLS

enum {
    HANDLER_COMPRESS,
    HANDLER_EXTRACT,
    HANDLER_LIST
};

enum {
    HANDLER_MOUNT,
    HANDLER_UNMOUNT,
    HANDLER_PROP
};

enum {
    HANDLER_MODE_ARC,
    HANDLER_MODE_FS,
    HANDLER_MODE_NET,
    HANDLER_MODE_FILE
};

void ptk_handler_add_defaults( int mode, gboolean overwrite,
                                         gboolean add_missing );
gboolean ptk_handler_equals_default( XSet* set );
void ptk_handler_show_config( int mode, PtkFileBrowser* file_browser,
                              XSet* def_handler_set );
gboolean ptk_handler_values_in_list( const char* list, GSList* values,
                                     char** msg );
XSet* add_new_handler( int mode );  // for settings.c upgrade
char* ptk_handler_load_script( int mode, int cmd, XSet* handler_set,
                               GtkTextView* view, char** text );
char* ptk_handler_save_script( int mode, int cmd, XSet* handler_set,
                               GtkTextView* view, const char* command );
char* ptk_handler_get_command( int mode, int cmd, XSet* handler_set );
gboolean ptk_handler_command_is_empty( const char* command );
void ptk_handler_load_text_view( GtkTextView* view, const char* text );
GSList* ptk_handler_file_has_handlers( int mode, int cmd,
                                       const char* path,
                                       VFSMimeType* mime_type,
                                       gboolean test_cmd,
                                       gboolean multiple,
                                       gboolean enabled_only );


G_END_DECLS
#endif

