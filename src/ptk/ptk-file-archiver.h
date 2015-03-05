/*
 * SpaceFM ptk-file-archiver.h
 * 
 * Copyright (C) 2013-2014 OmegaPhil <OmegaPhil+SpaceFM@gmail.com>
 * Copyright (C) 2014 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>
 * 
 * License: See COPYING file
 * 
*/

#ifndef _PTK_FILE_ARCHIVER_H_
#define _PTK_FILE_ARCHIVER_H_

#include <gtk/gtk.h>
#include <glib.h>

#include "vfs-mime-type.h"
#include "ptk-file-browser.h"

G_BEGIN_DECLS


// Archive operations enum
enum {
    ARC_COMPRESS,
    ARC_EXTRACT,
    ARC_LIST
};

// Pass file_browser or desktop depending on where you're calling from
void ptk_file_archiver_create( DesktopWindow *desktop,
                               PtkFileBrowser *file_browser, GList *files,
                               const char *cwd );
void ptk_file_archiver_extract( DesktopWindow *desktop,
                                PtkFileBrowser *file_browser,
                                GList *files, const char *cwd,
                                const char *dest_dir, int job );

// At least a mime type or extension is required - mime type preferred
gboolean ptk_file_archiver_is_format_supported( VFSMimeType* mime,
                                                const char* extension,
                                                int operation );

G_END_DECLS
#endif

