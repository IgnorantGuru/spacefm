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
    HANDLER_INFO
};

enum {
    HANDLER_MODE_ARC,
    HANDLER_MODE_FS,
    HANDLER_MODE_NET
};

void ptk_handler_show_config( int mode, PtkFileBrowser* file_browser );
void ptk_handler_add_defaults( int mode, gboolean overwrite,
                                         gboolean add_missing );
gboolean ptk_handler_val_in_list( const char* list, const char* val1,
                                  const char* val2, const char* val3,
                                  const char* val4 );


G_END_DECLS
#endif

