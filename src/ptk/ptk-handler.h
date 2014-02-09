/*
 * SpaceFM ptk-handler.h
 * 
 * Copyright (C) 2014 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2014 OmegaPhil <omegaphil@gmail.com>
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
    HAND_MOUNT,
    HAND_UNMOUNT,
    HAND_SHOW
};

void ptk_handler_show_config( PtkFileBrowser* file_browser );


G_END_DECLS
#endif

