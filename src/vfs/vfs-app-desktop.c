/*
*  C Implementation: vfs-app-desktop
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "vfs-app-desktop.h"
#include <glib/gi18n.h>
#include "glib-mem.h"

#include <string.h>

#include "vfs-execute.h"

#include "vfs-utils.h"  /* for vfs_load_icon */

#include "ptk-file-task.h"  //sfm breaks vfs independence for exec_in_terminal

const char desktop_entry_name[] = "Desktop Entry";

/*
* If file_name is not a full path, this function searches default paths
* for the desktop file.
*/
VFSAppDesktop* vfs_app_desktop_new( const char* file_name )
{
    GKeyFile* file;
    gboolean load;
    char* relative_path;
    
    VFSAppDesktop* app = g_slice_new0( VFSAppDesktop );
    app->n_ref = 1;

    file = g_key_file_new();

    if( !file_name )
    {
        g_key_file_free( file );
        return app;
    }

    if( g_path_is_absolute( file_name ) )
    {
        app->file_name = g_path_get_basename( file_name );
        app->full_path = g_strdup( file_name );
        load = g_key_file_load_from_file( file, file_name,
                                          G_KEY_FILE_NONE, NULL );
    }
    else
    {
        app->file_name = g_strdup( file_name );
        relative_path = g_build_filename( "applications",
                                          app->file_name, NULL );
        load = g_key_file_load_from_data_dirs( file, relative_path,
                                               &app->full_path,
                                               G_KEY_FILE_NONE, NULL );
        g_free( relative_path );

        if ( !load )
        {
            // some desktop files are in subdirs of data dirs (out of spec)
            if ( app->full_path = mime_type_locate_desktop_file(
                                                            NULL, file_name ) )
                load = g_key_file_load_from_file( file, app->full_path,
                                          G_KEY_FILE_NONE, NULL );
        }
    }

    if( load )
    {
        app->disp_name = g_key_file_get_locale_string ( file,
                                                        desktop_entry_name,
                                                        "Name", NULL, NULL);
        app->comment = g_key_file_get_locale_string ( file, desktop_entry_name,
                                                      "Comment", NULL, NULL);
        app->exec = g_key_file_get_string ( file, desktop_entry_name,
                                            "Exec", NULL);
        app->icon_name = g_key_file_get_string ( file, desktop_entry_name,
                                                 "Icon", NULL);
        app->terminal = g_key_file_get_boolean( file, desktop_entry_name,
                                                "Terminal", NULL );
        app->hidden = g_key_file_get_boolean( file, desktop_entry_name,
                                                "NoDisplay", NULL );
        app->path = g_key_file_get_string ( file, desktop_entry_name,
                                                 "Path", NULL);
    }

    g_key_file_free( file );

    return app;
}

static void vfs_app_desktop_free( VFSAppDesktop* app )
{
    g_free( app->disp_name );
    g_free( app->comment );
    g_free( app->exec );
    g_free( app->icon_name );
    g_free( app->path );
    g_free( app->full_path );

    g_slice_free( VFSAppDesktop, app );
}

void vfs_app_desktop_ref( VFSAppDesktop* app )
{
    g_atomic_int_inc( &app->n_ref );
}

void vfs_app_desktop_unref( gpointer data )
{
    VFSAppDesktop* app = (VFSAppDesktop*)data;
    if( g_atomic_int_dec_and_test(&app->n_ref) )
        vfs_app_desktop_free( app );
}

gboolean vfs_app_desktop_rename( char* desktop_file_path, char* new_name )   //sfm
{
    if ( !desktop_file_path || !new_name )
        return FALSE;
        
    GKeyFile* kfile = g_key_file_new();
    
    // load
    if ( !g_key_file_load_from_file( kfile, desktop_file_path,
                                        G_KEY_FILE_KEEP_COMMENTS
                                        | G_KEY_FILE_KEEP_TRANSLATIONS, NULL ) )
    {
        g_key_file_free( kfile );
        return FALSE;
    }

    // get user's locale
    const char* locale = NULL;
    const char* const * langs = g_get_language_names();
    char* dot = strchr( langs[0], '.' );
    if( dot )
        locale = g_strndup( langs[0], (size_t)(dot - langs[0]) );
    else
        locale = langs[0];
    if ( !locale || locale[0] == '\0' )
        locale = "en";
    char* l = g_strdup( locale );
    char* ll = strchr( l, '_' );
    if ( ll )
        ll[0] = '\0';

    // update keyfile
    g_key_file_set_locale_string( kfile, desktop_entry_name, "Name", l, new_name );
    if ( strcmp( l, locale ) )
        g_key_file_set_locale_string( kfile, desktop_entry_name, "Name", locale, new_name );
    g_free( l );

    // get keyfile as string
    char* data = g_key_file_to_data( kfile, NULL, NULL );
    g_key_file_free( kfile );
    
    // overwrite desktop file
    if ( data )
    {
        FILE* file = fopen( desktop_file_path, "w" );
        if ( file )
        {
            int result = fputs( data, file );
            g_free( data );
            fclose( file );
            if ( result < 0 )
                return FALSE;
        }
        else
        {
            g_free( data );
            return FALSE;
        }
    }
    else
        return FALSE;
    return TRUE;
}

const char* vfs_app_desktop_get_name( VFSAppDesktop* app )
{
    return app->file_name;
}

const char* vfs_app_desktop_get_disp_name( VFSAppDesktop* app )
{
    if( G_LIKELY(app->disp_name) )
        return app->disp_name;
    return app->file_name;
}

const char* vfs_app_desktop_get_exec( VFSAppDesktop* app )
{
    return app->exec;
}

const char* vfs_app_desktop_get_icon_name( VFSAppDesktop* app )
{
    return app->icon_name;
}

static GdkPixbuf* load_icon_file( const char* file_name, int size )
{
    GdkPixbuf* icon = NULL;
    char* file_path;
    const gchar** dirs = (const gchar**) g_get_system_data_dirs();
    const gchar** dir;
    for( dir = dirs; *dir; ++dir )
    {
        file_path = g_build_filename( *dir, "pixmaps", file_name, NULL );
        icon = gdk_pixbuf_new_from_file_at_scale( file_path, size, size, TRUE, NULL );
        g_free( file_path );
        if( icon )
            break;
    }
    return icon;
}

GdkPixbuf* vfs_app_desktop_get_icon( VFSAppDesktop* app, int size, gboolean use_fallback )
{
    GtkIconTheme* theme;
    char *icon_name = NULL, *suffix;
    GdkPixbuf* icon = NULL;

    if( app->icon_name )
    {
        if( g_path_is_absolute( app->icon_name) )
        {
            icon = gdk_pixbuf_new_from_file_at_scale( app->icon_name,
                                                     size, size, TRUE, NULL );
        }
        else
        {
            theme = gtk_icon_theme_get_default();
            suffix = strchr( app->icon_name, '.' );
            if( suffix ) /* has file extension, it's a basename of icon file */
            {
                /* try to find it in pixmaps dirs */
                icon = load_icon_file( app->icon_name, size );
                if( G_UNLIKELY( ! icon ) )  /* unfortunately, not found */
                {
                    /* Let's remove the suffix, and see if this name can match an icon
                         in current icon theme */
                    icon_name = g_strndup( app->icon_name,
                                           (suffix - app->icon_name) );
                    icon = vfs_load_icon( theme, icon_name, size );
                    g_free( icon_name );
                }
            }
            else  /* no file extension, it could be an icon name in the icon theme */
            {
                icon = vfs_load_icon( theme, app->icon_name, size );
            }
        }
    }
    if( G_UNLIKELY( ! icon ) && use_fallback )  /* fallback to generic icon */
    {
        theme = gtk_icon_theme_get_default();
        icon = vfs_load_icon( theme, "application-x-executable", size );
        if( G_UNLIKELY( ! icon ) )  /* fallback to generic icon */
        {
            icon = vfs_load_icon( theme, "gnome-mime-application-x-executable", size );
        }
    }
    return icon;
}

gboolean vfs_app_desktop_open_multiple_files( VFSAppDesktop* app )
{
    char* p;
    if( app->exec )
    {
        if ( strstr( app->exec, "%U" ) || strstr( app->exec, "%F" ) ||
             strstr( app->exec, "%N" ) || strstr( app->exec, "%D" ) )
            return TRUE;
        
        /*  this is broken
        for( p = app->exec; *p; ++p )
        {
            if( *p == '%' )
            {
                ++p;
                switch( *p )
                {
                case 'U':
                case 'F':
                case 'N':
                case 'D':
                    return TRUE;
                case '\0':
                    return FALSE;
                }
            }
            return TRUE;
        }
        */
    }
    return FALSE;
}

gboolean vfs_app_desktop_open_in_terminal( VFSAppDesktop* app )
{
    return app->terminal;
}

gboolean vfs_app_desktop_is_hidden( VFSAppDesktop* app )
{
    return app->hidden;
}

/*
* Parse Exec command line of app desktop file, and translate
* it into a real command which can be passed to g_spawn_command_line_async().
* file_list is a null-terminated file list containing full
* paths of the files passed to app.
* returned char* should be freed when no longer needed.
*/
static char* translate_app_exec_to_command_line( VFSAppDesktop* app,
                                                 GList* file_list )
{
    const char* pexec = vfs_app_desktop_get_exec( app );
    char* file;
    GList* l;
    gchar *tmp;
    GString* cmd = g_string_new("");
    gboolean add_files = FALSE;

    for( ; *pexec; ++pexec )
    {
        if( *pexec == '%' )
        {
            ++pexec;
            switch( *pexec )
            {
            case 'U':
                for( l = file_list; l; l = l->next )
                {
                    tmp = g_filename_to_uri( (char*)l->data, NULL, NULL );
                    file = g_shell_quote( tmp );
                    g_free( tmp );
                    g_string_append( cmd, file );
                    g_string_append_c( cmd, ' ' );
                    g_free( file );
                }
                add_files = TRUE;
                break;
            case 'u':
                if( file_list && file_list->data )
                {
                    file = (char*)file_list->data;
                    tmp = g_filename_to_uri( file, NULL, NULL );
                    file = g_shell_quote( tmp );
                    g_free( tmp );
                    g_string_append( cmd, file );
                    g_free( file );
                    add_files = TRUE;
                }
                break;
            case 'F':
            case 'N':
                for( l = file_list; l; l = l->next )
                {
                    file = (char*)l->data;
                    tmp = g_shell_quote( file );
                    g_string_append( cmd, tmp );
                    g_string_append_c( cmd, ' ' );
                    g_free( tmp );
                }
                add_files = TRUE;
                break;
            case 'f':
            case 'n':
                if( file_list && file_list->data )
                {
                    file = (char*)file_list->data;
                    tmp = g_shell_quote( file );
                    g_string_append( cmd, tmp );
                    g_free( tmp );
                    add_files = TRUE;
                }
                break;
            case 'D':
                for( l = file_list; l; l = l->next )
                {
                    tmp = g_path_get_dirname( (char*)l->data );
                    file = g_shell_quote( tmp );
                    g_free( tmp );
                    g_string_append( cmd, file );
                    g_string_append_c( cmd, ' ' );
                    g_free( file );
                }
                add_files = TRUE;
                break;
            case 'd':
                if( file_list && file_list->data )
                {
                    tmp = g_path_get_dirname( (char*)file_list->data );
                    file = g_shell_quote( tmp );
                    g_free( tmp );
                    g_string_append( cmd, file );
                    g_free( tmp );
                    add_files = TRUE;
                }
                break;
            case 'c':
                g_string_append( cmd, vfs_app_desktop_get_disp_name( app ) );
                break;
            case 'i':
                /* Add icon name */
                if( vfs_app_desktop_get_icon_name( app ) )
                {
                    g_string_append( cmd, "--icon " );
                    g_string_append( cmd, vfs_app_desktop_get_icon_name( app ) );
                }
                break;
            case 'k':
                /* Location of the desktop file */
                break;
            case 'v':
                /* Device name */
                break;
            case '%':
                g_string_append_c ( cmd, '%' );
                break;
            case '\0':
                goto _finish;
                break;
            }
        }
        else  /* not % escaped part */
        {
            g_string_append_c ( cmd, *pexec );
        }
    }
_finish:
    if( ! add_files )
    {
        g_string_append_c ( cmd, ' ' );
        for( l = file_list; l; l = l->next )
        {
            file = (char*)l->data;
            tmp = g_shell_quote( file );
            g_string_append( cmd, tmp );
            g_string_append_c( cmd, ' ' );
            g_free( tmp );
        }
    }

    return g_string_free( cmd, FALSE );
}

void exec_in_terminal( const char* app_name, const char* cwd, const char* cmd )
{
    // task
    PtkFileTask* task = ptk_file_exec_new( app_name, cwd, NULL, NULL );

    task->task->exec_command = strdup( cmd );

    task->task->exec_terminal = TRUE;
    // task->task->exec_keep_terminal = TRUE;  // for test only
    task->task->exec_sync = FALSE;
    task->task->exec_export = FALSE;

    ptk_file_task_run( task );
}

gboolean vfs_app_desktop_open_files( GdkScreen* screen,
                                     const char* working_dir,
                                     VFSAppDesktop* app,
                                     GList* file_paths,
                                     GError** err )
{
    char* exec = NULL;
    char* cmd;
    GList* l;
    gchar** argv = NULL;
    gint argc = 0;
    const char* sn_desc;

    if( vfs_app_desktop_get_exec( app ) )
    {
        if ( ! strchr( vfs_app_desktop_get_exec( app ), '%' ) )
        { /* No filename parameters */
            exec = g_strconcat( vfs_app_desktop_get_exec( app ), " %f", NULL );
        }
        else
        {
            exec = g_strdup( vfs_app_desktop_get_exec( app ) );
        }
    }

    if ( exec )
    {
        if( !screen )
            screen = gdk_screen_get_default();

        sn_desc = vfs_app_desktop_get_disp_name( app );
        if( !sn_desc )
            sn_desc = exec;

        if( vfs_app_desktop_open_multiple_files( app ) )
        {
            cmd = translate_app_exec_to_command_line( app, file_paths );
            if ( cmd )
            {
                if ( vfs_app_desktop_open_in_terminal( app ) )
                    exec_in_terminal( sn_desc,
                                      app->path && app->path[0] ? app->path :
                                                                  working_dir,
                                      cmd );
                else
                {
                    /* g_debug( "Execute %s\n", cmd ); */
                    if( g_shell_parse_argv( cmd, &argc, &argv, NULL ) )
                    {
                        vfs_exec_on_screen( screen,
                                            app->path && app->path[0] ?
                                                app->path : working_dir,
                                            argv, NULL,
                                            sn_desc,
                                            VFS_EXEC_DEFAULT_FLAGS,
                                            err );
                        g_strfreev( argv );
                    }
                }
                g_free( cmd );
            }
        }
        else
        {
            // app does not accept multiple files, so run multiple times
            GList* single;
            l = file_paths;
            do
            {
                if ( l )
                {
                    // just pass a single file path to translate
                    single = g_list_append( NULL, l->data );
                }
                else
                {
                    // there are no files being passed, just run once
                    single = NULL;
                }
                cmd = translate_app_exec_to_command_line( app, single );
                g_list_free( single );
                if ( cmd )
                {
                    if ( vfs_app_desktop_open_in_terminal( app ) )
                        exec_in_terminal( sn_desc, 
                                          app->path && app->path[0] ? app->path
                                                                    : working_dir,
                                          cmd );
                    else
                    {
                        /* g_debug( "Execute %s\n", cmd ); */
                        if( g_shell_parse_argv( cmd, &argc, &argv, NULL ) )
                        {
                            vfs_exec_on_screen( screen,
                                                app->path && app->path[0] ?
                                                    app->path : working_dir,
                                                argv, NULL, sn_desc,
                                                G_SPAWN_SEARCH_PATH|
                                                G_SPAWN_STDOUT_TO_DEV_NULL|
                                                G_SPAWN_STDERR_TO_DEV_NULL,
                                    err );
                            g_strfreev( argv );
                        }
                    }
                    g_free( cmd );
                }
            } while ( l = l ? l->next : NULL );
        }
        g_free( exec );
        return TRUE;
    }

    g_set_error( err, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED, "%s\n\n%s",
                                    _("Command not found"), app->file_name );
    return FALSE;
}
