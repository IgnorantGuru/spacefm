/*
*  C Implementation: ptk-clipboard
*
* Description: 
*
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "ptk-clipboard.h"

#include <string.h>

#include "vfs-file-info.h"
#include "ptk-file-task.h"
#include "vfs-file-task.h"
#include "ptk-utils.h"  //MOD

static GdkDragAction clipboard_action = GDK_ACTION_DEFAULT;
static GList* clipboard_file_list = NULL;

static void clipboard_get_data ( GtkClipboard *clipboard,
                                 GtkSelectionData *selection_data,
                                 guint info,
                                 gpointer user_data )
{
    GdkAtom uri_list_target = gdk_atom_intern( "text/uri-list", FALSE );
    GdkAtom gnome_target = gdk_atom_intern( "x-special/gnome-copied-files", FALSE );
    GList* l;
    gchar* file_name;
    gchar* action;
    gboolean use_uri = FALSE;

    GString* list;

    if ( ! clipboard_file_list )
        return ;

    list = g_string_sized_new( 8192 );

    if ( selection_data->target == gnome_target )
    {
        action = clipboard_action == GDK_ACTION_MOVE ? "cut\n" : "copy\n";
        g_string_append( list, action );
        use_uri = TRUE;
    }
    else if ( selection_data->target == uri_list_target )
        use_uri = TRUE;

    for ( l = clipboard_file_list; l; l = l->next )
    {
        if ( use_uri )
        {
            file_name = g_filename_to_uri( ( char* ) l->data, NULL, NULL );
        }
        else
        {
            file_name = g_filename_display_name( ( char* ) l->data );
        }
        g_string_append( list, file_name );
        g_free( file_name );

        if ( selection_data->target != uri_list_target )
            g_string_append_c( list, '\n' );
        else
            g_string_append( list, "\r\n" );
    }

    gtk_selection_data_set ( selection_data, selection_data->target, 8,
                             ( guchar* ) list->str, list->len + 1 );
    /* g_debug( "clipboard data:\n%s\n\n", list->str ); */
    g_string_free( list, TRUE );
}

static void clipboard_clean_data ( GtkClipboard *clipboard,
                                   gpointer user_data )
{
    /* g_debug( "clean clipboard!\n" ); */
    if ( clipboard_file_list )
    {
        g_list_foreach( clipboard_file_list, ( GFunc ) g_free, NULL );
        g_list_free( clipboard_file_list );
        clipboard_file_list = NULL;
    }
    clipboard_action = GDK_ACTION_DEFAULT;
}

void ptk_clipboard_copy_as_text( const char* working_dir,
                                      GList* files )  //MOD added
{   // aka copy path
    GtkClipboard* clip = gtk_clipboard_get( GDK_SELECTION_CLIPBOARD );
    GtkClipboard* clip_primary = gtk_clipboard_get( GDK_SELECTION_PRIMARY );
    GList *l;
    VFSFileInfo* file;
    char* file_path;
    char* file_text;
    char* str;
    char* quoted;

    file_text = g_strdup( "" );
    for ( l = files; l; l = l->next )
    {
        file = ( VFSFileInfo* ) l->data;
        file_path = g_build_filename( working_dir,
                                      vfs_file_info_get_name( file ), NULL );
        quoted = bash_quote( file_path );
        str = file_text;
        file_text = g_strdup_printf( "%s %s", str, quoted );
        g_free( str );
        g_free( quoted );
        g_free( file_path );
    }
    gtk_clipboard_set_text ( clip, file_text , -1 );
    gtk_clipboard_set_text ( clip_primary, file_text , -1 );
    g_free( file_text );
}

void ptk_clipboard_copy_name( const char* working_dir,
                                      GList* files )  //MOD added
{
    GtkClipboard* clip = gtk_clipboard_get( GDK_SELECTION_CLIPBOARD );
    GtkClipboard* clip_primary = gtk_clipboard_get( GDK_SELECTION_PRIMARY );
    GList *l;
    VFSFileInfo* file;
    char* file_text;
    gint fcount = 0;
    char* str;
    
    file_text = g_strdup( "" );
    for ( l = files; l; l = l->next )
    {
        file = ( VFSFileInfo* ) l->data;
        str = file_text;
        if ( fcount == 0 )
            file_text = g_strdup_printf( "%s", vfs_file_info_get_name( file ) );
        else if ( fcount == 1 )
            file_text = g_strdup_printf( "%s\n%s\n", file_text, 
                                         vfs_file_info_get_name( file ) );
        else
            file_text = g_strdup_printf( "%s%s\n", file_text, 
                                         vfs_file_info_get_name( file ) );
        fcount++;
        g_free( str );
    }
    gtk_clipboard_set_text( clip, file_text , -1 );
    gtk_clipboard_set_text( clip_primary, file_text , -1 );
    g_free( file_text );
}

void ptk_clipboard_copy_text( const char* text )  //MOD added
{
    GtkClipboard * clip = gtk_clipboard_get( GDK_SELECTION_CLIPBOARD );
    GtkClipboard* clip_primary = gtk_clipboard_get( GDK_SELECTION_PRIMARY );
    gtk_clipboard_set_text( clip, text, -1 );
    gtk_clipboard_set_text( clip_primary, text, -1 );
}

void ptk_clipboard_cut_or_copy_files( const char* working_dir,
                                      GList* files,
                                      gboolean copy )
{
    GtkClipboard * clip = gtk_clipboard_get( GDK_SELECTION_CLIPBOARD );
    GtkTargetList* target_list = gtk_target_list_new( NULL, 0 );
    GList* target;
    gint i, n_targets;
    GtkTargetEntry* targets;
    GtkTargetPair* pair;
    GList *l;
    VFSFileInfo* file;
    char* file_path;
    GList* file_list = NULL;

    gtk_target_list_add_text_targets( target_list, 0 );
    n_targets = g_list_length( target_list->list ) + 2;

    targets = g_new0( GtkTargetEntry, n_targets );
    target = target_list->list;
    for ( i = 0; target; ++i, target = g_list_next( target ) )
    {
        pair = ( GtkTargetPair* ) target->data;
        targets[ i ].target = gdk_atom_name ( pair->target );
    }
    targets[ i ].target = "x-special/gnome-copied-files";
    targets[ i + 1 ].target = "text/uri-list";

    gtk_target_list_unref ( target_list );

    for ( l = g_list_last( files ); l; l = l->prev )  //sfm was reverse order
    {
        file = ( VFSFileInfo* ) l->data;
        file_path = g_build_filename( working_dir,
                                      vfs_file_info_get_name( file ), NULL );
        file_list = g_list_prepend( file_list, file_path );
    }

    gtk_clipboard_set_with_data ( clip, targets, n_targets,
                                  clipboard_get_data,
                                  clipboard_clean_data,
                                  NULL );

    g_free( targets );

    clipboard_file_list = file_list;
    clipboard_action = copy ? GDK_ACTION_COPY : GDK_ACTION_MOVE;
}

void ptk_clipboard_paste_files( GtkWindow* parent_win,
                                const char* dest_dir, GtkTreeView* task_view )
{
    GtkClipboard * clip = gtk_clipboard_get( GDK_SELECTION_CLIPBOARD );
    GdkAtom gnome_target;
    GdkAtom uri_list_target;
    gchar **uri_list, **puri;
    GtkSelectionData* sel_data = NULL;
    GList* files = NULL;
    gchar* file_path;

    PtkFileTask* task;
    VFSFileTaskType action;
    char* uri_list_str;

    gnome_target = gdk_atom_intern( "x-special/gnome-copied-files", FALSE );
    sel_data = gtk_clipboard_wait_for_contents( clip, gnome_target );
    if ( sel_data )
    {
        if ( sel_data->length <= 0 || sel_data->format != 8 )
            return ;

        uri_list_str = ( char* ) sel_data->data;
        if ( 0 == strncmp( ( char* ) sel_data->data, "cut", 3 ) )
            action = VFS_FILE_TASK_MOVE;
        else
            action = VFS_FILE_TASK_COPY;

        if ( uri_list_str )
        {
            while ( *uri_list_str && *uri_list_str != '\n' )
                ++uri_list_str;
        }
    }
    else
    {
        uri_list_target = gdk_atom_intern( "text/uri-list", FALSE );
        sel_data = gtk_clipboard_wait_for_contents( clip, uri_list_target );
        if ( ! sel_data )
            return ;
        if ( sel_data->length <= 0 || sel_data->format != 8 )
            return ;
        uri_list_str = ( char* ) sel_data->data;

        if ( clipboard_action == GDK_ACTION_MOVE )
            action = VFS_FILE_TASK_MOVE;
        else
            action = VFS_FILE_TASK_COPY;
    }

    if ( uri_list_str )
    {
        puri = uri_list = g_uri_list_extract_uris( uri_list_str );
        while ( *puri )
        {
            file_path = g_filename_from_uri( *puri, NULL, NULL );
            if ( file_path )
            {
                files = g_list_prepend( files, file_path );
            }
            ++puri;
        }
        g_strfreev( uri_list );
        gtk_selection_data_free( sel_data );

        //sfm
        if ( files )
            files = g_list_reverse( files );

        /*
        * If only one item is selected and the item is a
        * directory, paste the files in that directory;
        * otherwise, paste the file in current directory.
        */

        task = ptk_file_task_new( action,
                                  files,
                                  dest_dir,
                                  GTK_WINDOW( parent_win ),
                                  task_view );
        ptk_file_task_run( task );
    }
}


void ptk_clipboard_paste_links( GtkWindow* parent_win,
                                const char* dest_dir,
                                GtkTreeView* task_view )   //MOD added
{
    GtkClipboard * clip = gtk_clipboard_get( GDK_SELECTION_CLIPBOARD );
    GdkAtom gnome_target;
    GdkAtom uri_list_target;
    gchar **uri_list, **puri;
    GtkSelectionData* sel_data = NULL;
    GList* files = NULL;
    gchar* file_path;

    PtkFileTask* task;
    VFSFileTaskType action;
    char* uri_list_str;

    gnome_target = gdk_atom_intern( "x-special/gnome-copied-files", FALSE );
    sel_data = gtk_clipboard_wait_for_contents( clip, gnome_target );
    if ( sel_data )
    {
        if ( sel_data->length <= 0 || sel_data->format != 8 )
            return ;

        uri_list_str = ( char* ) sel_data->data;
        action = VFS_FILE_TASK_LINK;
        if ( uri_list_str )
        {
            while ( *uri_list_str && *uri_list_str != '\n' )
                ++uri_list_str;
        }
    }
    else
    {
        uri_list_target = gdk_atom_intern( "text/uri-list", FALSE );
        sel_data = gtk_clipboard_wait_for_contents( clip, uri_list_target );
        if ( ! sel_data )
            return ;
        if ( sel_data->length <= 0 || sel_data->format != 8 )
            return ;
        uri_list_str = ( char* ) sel_data->data;
        action = VFS_FILE_TASK_LINK;
    }

    if ( uri_list_str )
    {
        puri = uri_list = g_uri_list_extract_uris( uri_list_str );
        while ( *puri )
        {
            if ( file_path = g_filename_from_uri( *puri, NULL, NULL ) )
                files = g_list_prepend( files, file_path );
            ++puri;
        }
        g_strfreev( uri_list );
        gtk_selection_data_free( sel_data );

        //sfm
        if ( files )
            files = g_list_reverse( files );

        task = ptk_file_task_new( action,
                                  files,
                                  dest_dir,
                                  GTK_WINDOW( parent_win ),
                                  task_view );
        ptk_file_task_run( task );
    }
}

void ptk_clipboard_paste_targets( GtkWindow* parent_win,
                                const char* dest_dir,
                                GtkTreeView* task_view )   //MOD added
{
    GtkClipboard * clip = gtk_clipboard_get( GDK_SELECTION_CLIPBOARD );
    GdkAtom gnome_target;
    GdkAtom uri_list_target;
    gchar **uri_list, **puri;
    GtkSelectionData* sel_data = NULL;
    GList* files = NULL;
    gchar* file_path;
    gint missing_targets = 0;
    char* str;
    
    PtkFileTask* task;
    VFSFileTaskType action;
    char* uri_list_str;

    gnome_target = gdk_atom_intern( "x-special/gnome-copied-files", FALSE );
    sel_data = gtk_clipboard_wait_for_contents( clip, gnome_target );
    if ( sel_data )
    {
        if ( sel_data->length <= 0 || sel_data->format != 8 )
            return ;

        uri_list_str = ( char* ) sel_data->data;
        action = VFS_FILE_TASK_COPY;
        if ( uri_list_str )
        {
            while ( *uri_list_str && *uri_list_str != '\n' )
                ++uri_list_str;
        }
    }
    else
    {
        uri_list_target = gdk_atom_intern( "text/uri-list", FALSE );
        sel_data = gtk_clipboard_wait_for_contents( clip, uri_list_target );
        if ( ! sel_data )
            return ;
        if ( sel_data->length <= 0 || sel_data->format != 8 )
            return ;
        uri_list_str = ( char* ) sel_data->data;
        action = VFS_FILE_TASK_COPY;
    }

    if ( uri_list_str )
    {
        puri = uri_list = g_uri_list_extract_uris( uri_list_str );
        while ( *puri )
        {
            file_path = g_filename_from_uri( *puri, NULL, NULL );
            if ( file_path )
            {
                if ( g_file_test( file_path, G_FILE_TEST_IS_SYMLINK ) )
                {
                    str = file_path;
                    file_path = g_file_read_link ( file_path, NULL );
                    g_free( str );
                }
                if ( file_path )
                {
                    if ( g_file_test( file_path, G_FILE_TEST_EXISTS ) )             
                        files = g_list_prepend( files, file_path );
                    else
                        missing_targets++;
                }
            }
            ++puri;
        }
        g_strfreev( uri_list );
        gtk_selection_data_free( sel_data );

        //sfm
        if ( files )
            files = g_list_reverse( files );

        task = ptk_file_task_new( action,
                                  files,
                                  dest_dir,
                                  GTK_WINDOW( parent_win ),
                                  task_view );
        ptk_file_task_run( task );
        
        if ( missing_targets > 0 )
            ptk_show_error( GTK_WINDOW( parent_win ),
                            g_strdup_printf ( "Error" ),
                            g_strdup_printf ( "%i target%s missing",
                            missing_targets, 
                            missing_targets > 1 ? g_strdup_printf ( "s are" ) : 
                            g_strdup_printf ( " is" ) ) );
    }
}
