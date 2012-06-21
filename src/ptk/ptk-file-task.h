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
    GtkWidget* task_view;
    GtkLabel* from;
    GtkLabel* to;
    GtkLabel* current;
    GtkProgressBar* progress_bar;
    GtkLabel* errors;
    GtkWidget* error_view;
    GtkScrolledWindow* scroll;

    GtkTextBuffer* log_buf;
    GtkTextMark* log_end;
    gboolean log_appended;

    int percent;
    off64_t total_size; /* Total size of the files to be processed, in bytes */
    off64_t progress; /* Total size of current processed files, in btytes */
    guint item_count;
    guint err_count;
    guint old_err_count;
    gboolean complete;
    gboolean aborted;

    /* <private> */
    guint timeout;
    guint progress_timer;
    guint destroy_timer;
    GFunc complete_notify;
    gpointer user_data;
    const char* current_file;
    //const char* old_dest_file;
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
                                GtkWidget* task_view );
PtkFileTask* ptk_file_exec_new( const char* item_name, const char* dir,
                                    GtkWidget* parent, GtkWidget* task_view );

void ptk_file_task_destroy( PtkFileTask* ptask );

void ptk_file_task_set_complete_notify( PtkFileTask* ptask,
                                        GFunc callback,
                                        gpointer user_data );

void ptk_file_task_set_chmod( PtkFileTask* ptask,
                              guchar* chmod_actions );

void ptk_file_task_set_chown( PtkFileTask* ptask,
                              uid_t uid, gid_t gid );

void ptk_file_task_set_recursive( PtkFileTask* ptask, gboolean recursive );

void ptk_file_task_run( PtkFileTask* ptask );

void ptk_file_task_cancel( PtkFileTask* ptask );

void ptk_file_task_progress_open( PtkFileTask* ptask );

#endif

