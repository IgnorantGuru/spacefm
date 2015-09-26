/*
 *      vfs-async-task.c
 *
 *      Copyright 2015 IgnorantGuru <ignorantguru@gmx.com>
 *      Copyright 2008 PCMan <pcman.tw@gmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 3 of the License, or
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
    task->cancel_cond = NULL;
    task->stale = task->cancel = task->finished = task->cancelled = FALSE;
//printf("vfs_async_task_NEW  task=%p\n", task );
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
    task = (VFSAsyncTask*)object;
//printf("vfs_async_task_finalize  task=%p\n", task);
    // cancel/wait for thread to finish
    vfs_async_task_cancel( task );
    
    if( task->idle_id )
    {
        g_source_remove( task->idle_id );
        task->idle_id = 0;
    }

    // wait for unlock vfs_async_task_thread - race condition ?
    // This lock+unlock is probably no longer needed - race fixed
    g_mutex_lock( task->lock );
    g_mutex_unlock( task->lock );
    g_mutex_free( task->lock );
    task->lock = NULL;

    if (G_OBJECT_CLASS(parent_class)->finalize)
        (* G_OBJECT_CLASS(parent_class)->finalize)(object);
}

gboolean on_idle( gpointer _task )
{
    // This function runs when the async thread exits to emit finish signal
    // in main glib loop thread
    VFSAsyncTask *task = VFS_ASYNC_TASK(_task);
//printf("(vfs_async_task)on_idle  task=%p\n", task );
    if ( task->idle_id )
    {
        g_source_remove( task->idle_id );
        task->idle_id = 0;
    }
    g_signal_emit( task, signals[ FINISH_SIGNAL ], 0, task->cancelled );
    return FALSE;
}

gpointer vfs_async_task_thread( gpointer _task )
{
    VFSAsyncTask *task = VFS_ASYNC_TASK(_task);
    gpointer ret = NULL;
    // run async function
    ret = task->func( task, task->user_data );

//printf("vfs_async_task_thread EXIT  task=%p  thread=%p  self=%p\n", task, task->thread, g_thread_self() );

    // thread cleanup
    vfs_async_task_lock( task );
//printf("vfs_async_task_thread LOCK  task=%p  thread=%p\n", task, task->thread );
    task->finished = TRUE;
    task->thread = NULL;
    task->ret_val = ret;
    task->cancelled = task->cancel;
    if ( task->cancel_cond )
    {
//printf("   cancel_cond\n"); 
        // there is a thread waiting for this task to cancel.  Since the thread
        // function has exited, release it.
        g_cond_broadcast( task->cancel_cond );
        // find files requires finish signal even if cancel
        task->idle_id = g_idle_add( on_idle, task );  // runs in main loop thread
        // unlock must come after g_idle_add if cancel or causes race ?
        vfs_async_task_unlock( task );
    }
    else
    {
        // unlock must come before g_idle_add if not cancel or finalize tries
        // to free mutex while locked
        vfs_async_task_unlock( task );
        task->idle_id = g_idle_add( on_idle, task );  // runs in main loop thread
    }
//printf("vfs_async_task_thread UNLOCK  task=%p  thread=%p\n", task, task->thread );

    return ret;
}

void vfs_async_task_execute( VFSAsyncTask* task )
{
    task->thread = g_thread_create( vfs_async_task_thread, task, TRUE, NULL );
//printf("vfs_async_task_execute  task=%p  thread=%p\n", task, task->thread );
}

void vfs_async_task_cancel( VFSAsyncTask* task )
{
//printf("vfs_async_task_cancel  task=%p  thread=%p  self=%p\n", task, task->thread, g_thread_self() );
    if( ! task->thread )
        return;
    /* This function sets cancel and waits for the async thread to exit.
     * 
     * This function may need to be called like this:
     *      GDK_THREADS_LEAVE();
     *      vfs_async_task_cancel( task );
     *      GDK_THREADS_ENTER();
     * 
     * Otherwise the main glib loop will stop here waiting for the async task
     * to finish, yet the async task may already be waiting at a
     * GDK_THREADS_ENTER(), eg for signal emission.
     */
    task->cancel = TRUE;
    g_mutex_lock( task->lock );
    // wait for thread exit
    task->cancel_cond = g_cond_new();
    g_cond_wait( task->cancel_cond, task->lock );
/*
    gint64 end_time = g_get_monotonic_time() + 1 * G_TIME_SPAN_SECOND;
    guint x = 0;
    while ( !g_cond_wait_until( task->cancel_cond, task->lock, end_time ) )
    {
        printf("    !g_cond_wait_until  task=%p  %u\n", task, x++ );
        g_mutex_unlock( task->lock );
        g_thread_yield();
        g_mutex_lock( task->lock );
        end_time = g_get_monotonic_time() + 1 * G_TIME_SPAN_SECOND;
    }
*/
    // resume
    g_cond_free( task->cancel_cond );
    task->cancel_cond = NULL;
    g_mutex_unlock( task->lock );
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
//printf("vfs_async_task_finish  task=%p\n", task);
}

gboolean vfs_async_task_is_finished( VFSAsyncTask* task )
{
    return task ? task->finished : TRUE;
}

gboolean vfs_async_task_is_cancelled( VFSAsyncTask* task )
{
    return task ? task->cancel : TRUE;
}
