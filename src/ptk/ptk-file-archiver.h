/*
*  C Interface: ptk-file-archiver
*
* Description: 
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
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

void ptk_file_archiver_config( PtkFileBrowser* file_browser );

void ptk_file_archiver_create( PtkFileBrowser* file_browser, GList* files,
											const char* cwd );
void ptk_file_archiver_extract( PtkFileBrowser* file_browser, GList* files,
                                            const char* cwd, const char* dest_dir );

// At least a mime type or extension is required - mime type preferred
gboolean ptk_file_archiver_is_format_supported( VFSMimeType* mime,
                                                char* extension,
                                                int operation );

G_END_DECLS
#endif

