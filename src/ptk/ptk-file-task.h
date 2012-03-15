/*
*  C Interface: ptk-file-task
*
* Description: 
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "vfs-file-task.h"
#include <gtk/gtk.h>
#include "settings.h"


#ifndef _PTK_FILE_TASK_
#define _PTK_FILE_TASK_

typedef struct _PtkFileTask PtkFileTask;

struct _PtkFileTask
{
    VFSFileTask* task;

    GtkWidget* progress_dlg;
    GtkWidget* progress_btn_close;
    GtkWidget* progress_btn_stop;
    GtkWindow* parent_window;
    GtkTreeView* task_view;
    GtkLabel* from;
    GtkLabel* to;
    GtkLabel* current;
    GtkProgressBar* progress;
    GtkLabel* errors;
    GtkWidget* error_view;
    int old_err_count;
    gboolean complete;
    gboolean aborted;
    GtkTextBuffer* err_buf;
    GtkTextMark* mark_end;
    GtkScrolledWindow* scroll;

    /* <private> */
    guint timeout;
    guint exec_timer;
    guint destroy_timer;
    GFunc complete_notify;
    gpointer user_data;
    const char* old_src_file;
    const char* old_dest_file;
    int old_percent;
    gboolean keep_dlg;
    
    char* dsp_file_count;
    char* dsp_size_tally;
    char* dsp_elapsed;
    char* dsp_curspeed;
    char* dsp_curest;
    char* dsp_avgspeed;
    char* dsp_avgest;

};

PtkFileTask* ptk_file_task_new( VFSFileTaskType type,
                                GList* src_files,
                                const char* dest_dir,
                                GtkWindow* parent_window,
                                GtkTreeView* task_view );
PtkFileTask* ptk_file_exec_new( char* item_name, char* dir, GtkWidget* parent,
                                                    GtkTreeView* task_view );

void ptk_file_task_destroy( PtkFileTask* task );

void ptk_file_task_set_complete_notify( PtkFileTask* task,
                                        GFunc callback,
                                        gpointer user_data );

void ptk_file_task_set_chmod( PtkFileTask* task,
                              guchar* chmod_actions );

void ptk_file_task_set_chown( PtkFileTask* task,
                              uid_t uid, gid_t gid );

void ptk_file_task_set_recursive( PtkFileTask* task, gboolean recursive );

void ptk_file_task_run( PtkFileTask* task );

void ptk_file_task_cancel( PtkFileTask* task );

void ptk_file_task_progress_open( PtkFileTask* task );

#endif

