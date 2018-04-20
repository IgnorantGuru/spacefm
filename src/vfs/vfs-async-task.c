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
    task->thread = NULL;
    task->ret_val = NULL;
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
    
    task->cancel = TRUE;
    g_mutex_free( task->lock );
    task->lock = NULL;

    if (G_OBJECT_CLASS(parent_class)->finalize)
        (* G_OBJECT_CLASS(parent_class)->finalize)(object);
}

gpointer vfs_async_task_thread( gpointer _task )
{
    VFSAsyncTask *task = VFS_ASYNC_TASK(_task);

    // run async function
    task->ret_val = task->func( task, task->user_data );
    gpointer ret_val = task->ret_val;

    // emit finish signal
    task->cancelled = task->cancel;

    task->finished = TRUE;

//printf("vfs_async_task_thread  task=%p  EMIT FINISH\n", task);
    // NOTE: finish signal handler is run in task thread not main loop
    GDK_THREADS_ENTER();
    g_signal_emit( task, signals[ FINISH_SIGNAL ], 0, task->cancelled );
    GDK_THREADS_LEAVE();

//printf("vfs_async_task_thread  task=%p  RETURN\n", task);
    return ret_val;
}

void vfs_async_task_execute( VFSAsyncTask* task )
{
    task->thread = g_thread_create( vfs_async_task_thread, task, TRUE, NULL );
//printf("vfs_async_task_execute  task=%p  thread=%p\n", task, task->thread );
}

void vfs_async_task_cancel( VFSAsyncTask* task )
{
//printf("vfs_async_task_cancel  task=%p  thread=%p  self=%p\n", task, task->thread, g_thread_self() );
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
    if ( task->thread )
    {
        g_thread_join( task->thread );
        task->thread = NULL;
        task->finished = TRUE;
    }
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
