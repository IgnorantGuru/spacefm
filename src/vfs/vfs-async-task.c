/*
 *      vfs-async-task.c
 *
 *      Copyright 2008 PCMan <pcman.tw@gmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#include "vfs-async-task.h"
#include <gtk/gtk.h>

static void vfs_async_task_class_init           (VFSAsyncTaskClass *klass);
static void vfs_async_task_init             (VFSAsyncTask *task);
static void vfs_async_task_finalize         (GObject *object);

static void vfs_async_task_finish( VFSAsyncTask* task, gboolean is_cancelled );
static void vfs_async_thread_cleanup( VFSAsyncTask* task, gboolean finalize );

void vfs_async_task_real_cancel( VFSAsyncTask* task, gboolean finalize );

/* Local data */
static GObjectClass *parent_class = NULL;

enum {
    FINISH_SIGNAL,
    N_SIGNALS
};

static guint signals[ N_SIGNALS ] = { 0 };

GType vfs_async_task_get_type(void)
{
    static GType self_type = 0;
    if (! self_type)
    {
        static const GTypeInfo self_info =
        {
            sizeof(VFSAsyncTaskClass),
            NULL, /* base_init */
            NULL, /* base_finalize */
            (GClassInitFunc)vfs_async_task_class_init,
            NULL, /* class_finalize */
            NULL, /* class_data */
            sizeof(VFSAsyncTask),
            0,
            (GInstanceInitFunc)vfs_async_task_init,
            NULL /* value_table */
        };

        self_type = g_type_register_static(G_TYPE_OBJECT, "VFSAsyncTask", &self_info, 0); }

    return self_type;
}

static void vfs_async_task_class_init(VFSAsyncTaskClass *klass)
{
    GObjectClass *g_object_class;
    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = vfs_async_task_finalize;
    parent_class = (GObjectClass*)g_type_class_peek(G_TYPE_OBJECT);

    klass->finish = vfs_async_task_finish;

    signals[ FINISH_SIGNAL ] =
        g_signal_new ( "finish",
                       G_TYPE_FROM_CLASS ( klass ),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET ( VFSAsyncTaskClass, finish ),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__BOOLEAN,
                       G_TYPE_NONE, 1, G_TYPE_BOOLEAN );
}

static void vfs_async_task_init(VFSAsyncTask *task)
{
    task->lock = g_mutex_new();
}


VFSAsyncTask* vfs_async_task_new( VFSAsyncFunc task_func, gpointer user_data )
{
    VFSAsyncTask* task = (VFSAsyncTask*)g_object_new(VFS_ASYNC_TASK_TYPE, NULL);
    task->func = task_func;
    task->user_data = user_data;
    return (VFSAsyncTask*)task;
}

gpointer vfs_async_task_get_data( VFSAsyncTask* task )
{
    return task->user_data;
}

void vfs_async_task_set_data( VFSAsyncTask* task, gpointer user_data )
{
    task->user_data = user_data;
}

gpointer vfs_async_task_get_return_value( VFSAsyncTask* task )
{
    return task->ret_val;
}

void vfs_async_task_finalize(GObject *object)
{
    VFSAsyncTask *task;
    /* FIXME: destroying the object without calling vfs_async_task_cancel
     currently induces unknown errors. */
    task = (VFSAsyncTask*)object;

    /* finalize = TRUE, inhibit the emission of signals */
    vfs_async_task_real_cancel( task, TRUE );
    vfs_async_thread_cleanup( task, TRUE );

    g_mutex_free( task->lock );
    task->lock = NULL;

    if (G_OBJECT_CLASS(parent_class)->finalize)
        (* G_OBJECT_CLASS(parent_class)->finalize)(object);
}

gboolean on_idle( gpointer _task )
{
    VFSAsyncTask *task = VFS_ASYNC_TASK(_task);
    GDK_THREADS_ENTER();
    vfs_async_thread_cleanup( task, FALSE );
    GDK_THREADS_LEAVE();
    return TRUE;    /* the idle handler is removed in vfs_async_thread_cleanup. */
}

gpointer vfs_async_task_thread( gpointer _task )
{
    VFSAsyncTask *task = VFS_ASYNC_TASK(_task);
    gpointer ret = NULL;
    ret = task->func( task, task->user_data );

    vfs_async_task_lock( task );
    task->idle_id = g_idle_add( on_idle, task );
    task->ret_val = ret;
    task->finished = TRUE;
    vfs_async_task_unlock( task );

    return ret;
}

void vfs_async_task_execute( VFSAsyncTask* task )
{
    task->thread = g_thread_create( vfs_async_task_thread, task, TRUE, NULL );
}

void vfs_async_thread_cleanup( VFSAsyncTask* task, gboolean finalize )
{
    if( task->idle_id )
    {
        g_source_remove( task->idle_id );
        task->idle_id = 0;
    }
    if( G_LIKELY( task->thread ) )
    {
        g_thread_join( task->thread );
        task->thread = NULL;
        task->finished = TRUE;
        /* Only emit the signal when we are not finalizing.
            Emitting signal on an object during destruction is not allowed. */
        if( G_LIKELY( ! finalize ) )
            g_signal_emit( task, signals[ FINISH_SIGNAL ], 0, task->cancelled );
    }
}

void vfs_async_task_real_cancel( VFSAsyncTask* task, gboolean finalize )
{
    if( ! task->thread )
        return;

    /*
     * NOTE: Well, this dirty hack is needed. Since the function is always
     * called from main thread, the GTK+ main loop may have this gdk lock locked
     * when this function gets called.  However, our task running in another thread
     * might need to use GTK+, too. If we don't release the gdk lock in main thread
     * temporarily, the task in another thread will be blocked due to waiting for
     * the gdk lock locked by our main thread, and hence cannot be finished.
     * Then we'll end up in endless waiting for that thread to finish, the so-called deadlock.
     *
     * The doc of GTK+ really sucks. GTK+ use this GTK_THREADS_ENTER everywhere internally,
     * but the behavior of the lock is not well-documented. So it's very difficult for use
     * to get things right.
     */
    GDK_THREADS_LEAVE();

    vfs_async_task_lock( task );
    task->cancel = TRUE;
    vfs_async_task_unlock( task );

    vfs_async_thread_cleanup( task, finalize );
    task->cancelled = TRUE;

    GDK_THREADS_ENTER();
}

void vfs_async_task_cancel( VFSAsyncTask* task )
{
    vfs_async_task_real_cancel( task, FALSE );
}

void vfs_async_task_lock( VFSAsyncTask* task )
{
    g_mutex_lock( task->lock );
}

void vfs_async_task_unlock( VFSAsyncTask* task )
{
    g_mutex_unlock( task->lock );
}

void vfs_async_task_finish( VFSAsyncTask* task, gboolean is_cancelled )
{
    /* default handler of "finish" signal. */
}

gboolean vfs_async_task_is_finished( VFSAsyncTask* task )
{
    return task->finished;
}

gboolean vfs_async_task_is_cancelled( VFSAsyncTask* task )
{
    return task->cancel;
}
