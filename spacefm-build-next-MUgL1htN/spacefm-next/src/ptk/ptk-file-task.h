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

enum {
    PTASK_ERROR_FIRST,
    PTASK_ERROR_ANY,
    PTASK_ERROR_CONT
};
    
struct _PtkFileTask
{
    VFSFileTask* task;

    GtkWidget* progress_dlg;
    GtkWidget* progress_btn_close;
    GtkWidget* progress_btn_stop;
    GtkWidget* progress_btn_pause;
    GtkWindow* parent_window;
    GtkWidget* task_view;
    GtkLabel* from;
    GtkLabel* to;
    GtkLabel* src_dir;
    GtkLabel* current;
    GtkProgressBar* progress_bar;
    GtkLabel* errors;
    GtkWidget* error_view;
    GtkScrolledWindow* scroll;
    GtkWidget* overwrite_combo;
    GtkWidget* error_combo;

    GtkTextBuffer* log_buf;
    GtkTextMark* log_end;
    gboolean log_appended;
    guint err_count;
    char err_mode;
    
/*
    int percent;
    off64_t total_size;     // Total size of the files to be processed, in bytes
    off64_t progress;       // Total size of current processed files, in btytes 
    guint item_count;
    guint old_err_count;
*/
    gboolean complete;
    gboolean aborted;
    gboolean pause_change;
    gboolean pause_change_view;
    gboolean force_scroll;
    
    /* <private> */
    guint timeout;
    gboolean restart_timeout;
    guint progress_timer;
    char progress_count;
    GFunc complete_notify;
    gpointer user_data;
    gboolean keep_dlg;
    gboolean pop_detail;
    char* pop_handler;
    
    GCond* query_cond;
    GCond* query_cond_last;
    char** query_new_dest;
    GtkEntry* query_entry;
    gboolean query_ret;

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

gboolean ptk_file_task_cancel( PtkFileTask* ptask );

void ptk_file_task_pause( PtkFileTask* ptask, int state );

void ptk_file_task_progress_open( PtkFileTask* ptask );

#endif

