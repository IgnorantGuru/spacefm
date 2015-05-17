/*
 * SpaceFM mime-action.c
 * 
 * Copyright (C) 2013-2015 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2007 Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>
 * 
 * License: See COPYING file
 *
 * This file handles default applications for MIME types.  For changes it
 * makes to mimeapps.list, it is fully compliant with Freedeskop's:
 *   Association between MIME types and applications 1.0.1
 *   http://standards.freedesktop.org/mime-apps-spec/mime-apps-spec-latest.html
 * 
 * However, for reading the hierarchy and determining default and associated
 * applications, it uses a best-guess algorithm for better performance and
 * compatibility with older systems, and is NOT fully spec compliant.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mime-action.h"
#include "glib-utils.h"
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

gboolean save_to_file( const char* path, const char* data, gssize len )
{
    int fd = creat( path, 0644 );
    if( fd == -1 )
        return FALSE;

    if( write( fd, data, len ) == -1 )
    {
        close( fd );
        return FALSE;
    }
    close( fd );
    return TRUE;
}

const char group_desktop[] = "Desktop Entry";
const char key_mime_type[] = "MimeType";

typedef char* (*DataDirFunc)    ( const char* dir, const char* mime_type, gpointer user_data );

static char* data_dir_foreach( DataDirFunc func, const char* mime_type, gpointer user_data )
{
    char* ret = NULL;
    const gchar* const * dirs;
    const char* dir = g_get_user_data_dir();

    if( (ret = func( dir, mime_type, user_data )) )
        return ret;

    dirs = g_get_system_data_dirs();
    for( ; *dirs; ++dirs )
    {
        if( (ret = func( *dirs, mime_type, user_data )) )
            return ret;
    }
    return NULL;
}

static void update_desktop_database()
{
    char* argv[3];
    argv[0] = g_find_program_in_path( "update-desktop-database" );
    if( G_UNLIKELY( ! argv[0] ) )
        return;
    argv[1] = g_build_filename( g_get_user_data_dir(), "applications", NULL );
    argv[2] = NULL;
    g_spawn_sync( NULL, argv, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL);
    g_free( argv[0] );
    g_free( argv[1] );
}

static int strv_index( char** strv, const char* str )
{
    char**p;
    if( G_LIKELY( strv && str ) )
    {
        for( p = strv; *p; ++p )
        {
            if( 0 == strcmp( *p, str ) )
                return (p - strv);
        }
    }
    return -1;
}

/* Determine removed associations for this type */
static void remove_actions( const char* type, GArray* actions )
{   //sfm 0.7.7+ added
    char** removed = NULL;
    gsize n_removed = 0, r;
    int i;

//g_print( "remove_actions( %s )\n", type );
    char* path = g_build_filename( g_get_user_data_dir(),
                                            "applications/mimeapps.list", NULL );
    GKeyFile* file = g_key_file_new();
    if ( g_key_file_load_from_file( file, path, 0, NULL ) )
    {
        removed = g_key_file_get_string_list( file, "Removed Associations",
                                                        type, &n_removed, NULL );
        if ( removed )
        {
            for ( r = 0; r < n_removed; ++r )
            {
                g_strstrip( removed[r] );
//g_print( "    %s\n", removed[r] );
                i = strv_index( (char**)actions->data, removed[r] );
                if ( i != -1 )
{
//g_print(  "        ACTION-REMOVED\n" );
                    g_array_remove_index( actions, i );
}
            }
        }
        g_strfreev( removed );
    }
    g_key_file_free( file );
    g_free( path );
}

/*
 * Get applications associated with this mime-type
 * 
 * This is very roughly based on specs:
 * http://standards.freedesktop.org/mime-apps-spec/mime-apps-spec-latest.html
 *
 */
static char* get_actions( const char* dir, const char* type, GArray* actions )
{
//g_print( "get_actions( %s, %s )\n", dir, type );
    GKeyFile* file;
    gboolean opened;
    int n, k;
    char** apps = NULL;
    char** removed = NULL;
    gboolean is_removed;
    gsize n_removed = 0, r;
    gsize n_apps, i;
    
    const char* names[] = {
        "applications/mimeapps.list",
        "applications/mimeinfo.cache"
    };
    const char* groups[] = {
        "Default Applications",
        "Added Associations",
        "MIME Cache"
    };
//g_print("dir %s [%s]\n", dir, type );
    for ( n = 0; n < G_N_ELEMENTS( names ); n++ )
    {
        char* path = g_build_filename( dir, names[n], NULL );
//g_print( "    %s\n", path );
        file = g_key_file_new();
        opened = g_key_file_load_from_file( file, path, 0, NULL );
        g_free( path );
        if ( G_LIKELY( opened ) )
        {
            if ( n == 0 )
            {
                // get removed associations in this dir
                removed = g_key_file_get_string_list( file,
                                                "Removed Associations",
                                                type, &n_removed, NULL );
            }
            // mimeinfo.cache has only MIME Cache; others don't have it
            for ( k = ( n == 0 ? 0 : 2 ); k < ( n == 0 ? 2 : 3 ); k++ )
            {
//g_print( "        %s [%d]\n", groups[k], k );
                n_apps = 0;
                apps = g_key_file_get_string_list( file, groups[k], type,
                                                            &n_apps, NULL );
                for ( i = 0; i < n_apps; ++i )
                {
                    g_strstrip( apps[i] );
//g_print( "        %s\n", apps[i] );
                    // check if removed
                    is_removed = FALSE;
                    if ( removed && n > 0 )
                    {
                        for ( r = 0; r < n_removed; ++r )
                        {
                            g_strstrip( removed[r] );
                            if ( !strcmp( removed[r], apps[i] ) )
                            {
//g_print( "            REMOVED\n" );
                                is_removed = TRUE;
                                break;
                            }
                        }
                    }
                    if( !is_removed && -1 == strv_index( (char**)actions->data,
                                                                    apps[i] ) )
                    {
                        /* check for app existence */
                        path = mime_type_locate_desktop_file( NULL, apps[i] );
                        if ( G_LIKELY( path ) )
                        {
//g_print( "            EXISTS\n");
                            g_array_append_val( actions, apps[i] );
                            g_free( path );
                        }
                        else
                            g_free( apps[i] );
                        apps[i] = NULL; /* steal the string */
                    }
                    else
                    {
                        g_free( apps[i] );
                        apps[i] = NULL;
                    }
                }
                /* don't call g_strfreev since all strings in the array were
                 * stolen or freed. */
                g_free( apps );
            }
        }
        g_key_file_free( file );
    }
    g_strfreev( removed );
    return NULL;    /* return NULL so the for_each operation doesn't stop. */
}

/*
 *  Get a list of applications supporting this mime-type
 * The returned string array was newly allocated, and should be
 * freed with g_strfreev() when no longer used.
 */
char** mime_type_get_actions( const char* type )
{
    GArray* actions = g_array_sized_new( TRUE, FALSE, sizeof(char*), 10 );
    char* default_app = NULL;

    /* FIXME: actions of parent types should be added, too. */

    /* get all actions for this file type */
    data_dir_foreach( (DataDirFunc)get_actions, type, actions );

    /* remove actions for this file type */ //sfm
    remove_actions( type, actions );

    /* ensure default app is in the list */
    if( G_LIKELY( ( default_app = mime_type_get_default_action( type ) ) ) )
    {
        int i = strv_index( (char**)actions->data, default_app );
        if( i == -1 )   /* default app is not in the list, add it! */
        {
            g_array_prepend_val( actions, default_app );
        }
        else  /* default app is in the list, move it to the first. */
        {
            if( i != 0 )
            {
                char** pdata = (char**)actions->data;
                char* tmp = pdata[i];
                g_array_remove_index( actions, i );
                g_array_prepend_val( actions, tmp );
            }
            g_free( default_app );
        }
    }
    return (char**)g_array_free( actions, actions->len == 0 );
}

/*
 * NOTE:
 * This API is very time consuming, but unfortunately, due to the damn poor design of
 * Freedesktop.org spec, all the insane checks here are necessary.  Sigh...  :-(
 */
gboolean mime_type_has_action( const char* type, const char* desktop_id )
{
    char** actions, **action;
    char *cmd = NULL, *name = NULL;
    gboolean found = FALSE;
    gboolean is_desktop = g_str_has_suffix( desktop_id, ".desktop" );

    if( is_desktop )
    {
        char** types;
        GKeyFile* kf = g_key_file_new();
        char* filename = mime_type_locate_desktop_file( NULL, desktop_id );
        if( filename && g_key_file_load_from_file( kf, filename, 0, NULL ) )
        {
            types = g_key_file_get_string_list( kf, group_desktop, key_mime_type, NULL, NULL );
            if( -1 != strv_index( types, type ) )
            {
                /* our mime-type is already found in the desktop file. no further check is needed */
                found = TRUE;
            }
            g_strfreev( types );

            if( ! found )   /* get the content of desktop file for comparison */
            {
                cmd = g_key_file_get_string( kf, group_desktop, "Exec", NULL );
                name = g_key_file_get_string( kf, group_desktop, "Name", NULL );
            }
        }
        g_free( filename );
        g_key_file_free( kf );
    }
    else
    {
        cmd = (char*)desktop_id;
    }

    actions = mime_type_get_actions( type );
    if( actions )
    {
        for( action = actions; ! found && *action; ++action )
        {
            /* Try to match directly by desktop_id first */
            if( is_desktop && 0 == strcmp( *action, desktop_id ) )
            {
                found = TRUE;
            }
            else /* Then, try to match by "Exec" and "Name" keys */
            {
                char *name2 = NULL, *cmd2 = NULL, *filename = NULL;
                GKeyFile* kf = g_key_file_new();
                filename = mime_type_locate_desktop_file( NULL, *action );
                if( filename && g_key_file_load_from_file( kf, filename, 0, NULL ) )
                {
                    cmd2 = g_key_file_get_string( kf, group_desktop, "Exec", NULL );
                    if( cmd && cmd2 && 0 == strcmp( cmd, cmd2 ) )   /* 2 desktop files have same "Exec" */
                    {
                        if( is_desktop )
                        {
                            name2 = g_key_file_get_string( kf, group_desktop, "Name", NULL );
                            /* Then, check if the "Name" keys of 2 desktop files are the same. */
                            if( name && name2 && 0 == strcmp( name, name2 ) )
                            {
                                /* Both "Exec" and "Name" keys of the 2 desktop files are
                                 *  totally the same. So, despite having different desktop id
                                 *  They actually refer to the same application. */
                                found = TRUE;
                            }
                            g_free( name2 );
                        }
                        else
                            found = TRUE;
                    }
                }
                g_free( filename );
                g_free( cmd2 );
                g_key_file_free( kf );
            }
        }
        g_strfreev( actions );
    }
    if( is_desktop )
    {
        g_free( cmd );
        g_free( name );
    }
    return found;
}

#if 0
static gboolean is_custom_desktop_file( const char* desktop_id )
{
    char* path = g_build_filename( g_get_user_data_dir(), "applications", desktop_id, NULL );
    gboolean ret = g_file_test( path, G_FILE_TEST_EXISTS );
    g_free( path );
    return ret;
}
#endif

static char* make_custom_desktop_file( const char* desktop_id, const char* mime_type )
{
    char *name = NULL, *cust_template = NULL, *cust = NULL, *path, *dir;
    char* file_content = NULL;
    gsize len = 0;
    guint i;

    if( G_LIKELY( g_str_has_suffix(desktop_id, ".desktop") ) )
    {
        GKeyFile *kf = g_key_file_new();
        char* name = mime_type_locate_desktop_file( NULL, desktop_id );
        if( G_UNLIKELY( ! name || ! g_key_file_load_from_file( kf, name,
                                                            G_KEY_FILE_KEEP_TRANSLATIONS, NULL ) ) )
        {
            g_free( name );
            return NULL; /* not a valid desktop file */
        }
        g_free( name );
/*
        FIXME: If the source desktop_id refers to a custom desktop file, and
                    value of the MimeType key equals to our mime-type, there is no
                    need to generate a new desktop file.
        if( G_UNLIKELY( is_custom_desktop_file( desktop_id ) ) )
        {
        }
*/
        /* set our mime-type */
        g_key_file_set_string_list( kf, group_desktop, key_mime_type, &mime_type, 1 );
        /* store id of original desktop file, for future use. */
        g_key_file_set_string( kf, group_desktop, "X-MimeType-Derived", desktop_id );
        g_key_file_set_string( kf, group_desktop, "NoDisplay", "true" );

        name = g_strndup( desktop_id, strlen(desktop_id) - 8 );
        cust_template = g_strdup_printf( "%s-usercustom-%%d.desktop", name );
        g_free( name );

        file_content = g_key_file_to_data( kf, &len, NULL );
        g_key_file_free( kf );
    }
   else  /* it's not a desktop_id, but a command */
    {
        char* p;
        const char file_templ[] =
            "[Desktop Entry]\n"
            "Encoding=UTF-8\n"
            "Name=%s\n"
            "Exec=%s\n"
            "MimeType=%s\n"
            "Icon=exec\n"
            "NoDisplay=true\n"; /* FIXME: Terminal? */
        /* Make a user-created desktop file for the command */
        name = g_path_get_basename( desktop_id );
        if( (p = strchr(name, ' ')) ) /* FIXME: skip command line arguments. is this safe? */
            *p = '\0';
        file_content = g_strdup_printf( file_templ, name, desktop_id, mime_type );
        len = strlen( file_content );
        cust_template = g_strdup_printf( "%s-usercreated-%%d.desktop", name );
        g_free( name );
    }

    /* generate unique file name */
    dir = g_build_filename( g_get_user_data_dir(), "applications", NULL );
    g_mkdir_with_parents( dir, 0700 );
    for( i = 0; ; ++i )
    {
        /* generate the basename */
        cust = g_strdup_printf( cust_template, i );
        path = g_build_filename( dir, cust, NULL ); /* test if the filename already exists */
        if( g_file_test( path, G_FILE_TEST_EXISTS ) )
        {
            g_free( cust );
            g_free( path );
        }
        else /* this generated filename can be used */
            break;
    }
    g_free( dir );
    if( G_LIKELY( path ) )
    {
        save_to_file( path, file_content, len );
        g_free( path );

        /* execute update-desktop-database" to update mimeinfo.cache */
        update_desktop_database();
    }
    return cust;
}

/*
 * Add an applications used to open this mime-type
 * desktop_id is the name of *.desktop file.
 *
 * custom_desktop: used to store name of the newly created user-custom desktop file, can be NULL.
 */
void mime_type_add_action( const char* type, const char* desktop_id, char** custom_desktop )
{
    char* cust;
    if( mime_type_has_action( type, desktop_id ) )
    {
        if( custom_desktop )
            *custom_desktop = g_strdup( desktop_id );
        return;
    }

    cust = make_custom_desktop_file( desktop_id, type );
    if( custom_desktop )
        *custom_desktop = cust;
    else
        g_free( cust );
}

static char* _locate_desktop_file_recursive( const char* path,
                                    const char* desktop_id, gboolean first )
{   // if first is true, just search for subdirs not desktop_id (already searched)
    const char* name;
    char* sub_path;
    
    GDir* dir = g_dir_open( path, 0, NULL );
    if ( !dir )
        return NULL;
    
    char* found = NULL;
    while ( name = g_dir_read_name( dir ) )
    {
        sub_path = g_build_filename( path, name, NULL );
        if ( g_file_test( sub_path, G_FILE_TEST_IS_DIR ) )
        {
            if ( found = _locate_desktop_file_recursive( sub_path, desktop_id,
                                                                    FALSE ) )
            {
                g_free( sub_path );
                break;
            }
        }
        else if ( !first && !strcmp( name, desktop_id ) && 
                                g_file_test( sub_path, G_FILE_TEST_IS_REGULAR ) )
        {
            found = sub_path;
            break;
        }
        g_free( sub_path );
    }
    g_dir_close( dir );
    return found;
}

static char* _locate_desktop_file( const char* dir, const char* unused,
                                                    const gpointer desktop_id )
{   //sfm 0.7.8 modified + 0.8.7 modified
    gboolean found = FALSE;

    char *path = g_build_filename( dir, "applications", (const char*)desktop_id,
                                                                        NULL );

    char* sep = strchr( (const char*)desktop_id, '-' );
    if( sep )
        sep = strrchr( path, '-' );

    do
    {
        if ( g_file_test( path, G_FILE_TEST_IS_REGULAR ) )
        {
            found = TRUE;
            break;
        }
        if ( sep )
        {
            *sep = '/';
            sep = strchr( sep + 1, '-' );
        }
        else
            break;
    } while( !found );

    if ( found )
        return path;
    g_free( path );
    
    //sfm 0.8.7 some desktop files listed by the app chooser are in subdirs
    path = g_build_filename( dir, "applications", NULL );
    sep = _locate_desktop_file_recursive( path, desktop_id, TRUE );
    g_free( path );
    return sep;
}

char* mime_type_locate_desktop_file( const char* dir, const char* desktop_id )
{
    if( dir )
        return _locate_desktop_file( dir, NULL, (gpointer) desktop_id );
    return data_dir_foreach( _locate_desktop_file, NULL, (gpointer) desktop_id );
}

static char* get_default_action( const char* dir, const char* type, gpointer user_data )
{
    GKeyFile* file;
    char* path;
    char** apps;
    gsize n_apps, i;
    int n, k;
    gboolean opened;
    
//g_print( "get_default_action( %s, %s )\n", dir, type );
    // search these files in dir for the first existing default app
    char* names[] = {
        "applications/mimeapps.list",
        "applications/defaults.list"
    };
    char* groups[] = {
        "Default Applications",
        "Added Associations"
    };
    
    for ( n = 0; n < G_N_ELEMENTS( names ); n++ )
    {
        char* path = g_build_filename( dir, names[n], NULL );
//g_print( "    path = %s\n", path );
        file = g_key_file_new();
        opened = g_key_file_load_from_file( file, path, 0, NULL );
        g_free( path );
        if ( opened )
        {
            for ( k = 0; k < G_N_ELEMENTS( groups ); k++ )
            {
                apps = g_key_file_get_string_list( file, groups[k], type,
                                                            &n_apps, NULL );
                if ( apps )
                {
                    for ( i = 0; i < n_apps; ++i )
                    {
                        g_strstrip( apps[i] );
                        if ( apps[i][0] != '\0' )
                        {
//g_print( "        %s\n", apps[i] );
                            if ( path = mime_type_locate_desktop_file( NULL,
                                                                apps[i] ) )
                            {
//g_print( "            EXISTS\n" );
                                g_free( path );
                                path = g_strdup( apps[i] );
                                g_strfreev( apps );
                                g_key_file_free( file );
                                return path;
                            }
                        }
                    }
                    g_strfreev( apps );
                }
                if ( n == 1 )
                    break;  // defaults.list doesn't have Added Associations
            }
        }
        g_key_file_free( file );
    }
    return NULL;
}

/*
 * Get default applications used to open this mime-type
 * 
 * The returned string was newly allocated, and should be freed when no longer
 * used.  If NULL is returned, that means a default app is not set for this
 * mime-type.  This is very roughly based on specs:
 * http://standards.freedesktop.org/mime-apps-spec/mime-apps-spec-latest.html
 *
 * The old defaults.list is also checked.
 */
char* mime_type_get_default_action( const char* type )
{
    /* FIXME: need to check parent types if default action of current type is not set. */
    return data_dir_foreach( (DataDirFunc)get_default_action, type, NULL );
}

/*
 * Set applications used to open or never used to open this mime-type
 * desktop_id is the name of *.desktop file.
 * action ==
 *     MIME_TYPE_ACTION_DEFAULT - make desktop_id the default app
 *     MIME_TYPE_ACTION_APPEND  - add desktop_id to Default and Added apps
 *     MIME_TYPE_ACTION_REMOVE  - add desktop id to Removed apps
 *
 * http://standards.freedesktop.org/mime-apps-spec/mime-apps-spec-latest.html
 */
void mime_type_update_association( const char* type, const char* desktop_id,
                                   int action )
{
    const char* groups[] = {
        "Default Applications",
        "Added Associations",
        "Removed Associations" };
    GKeyFile* file;
    gsize len = 0;
    char* data = NULL;
    char** apps;
    gsize n_apps, i, k;
    char* str;
    char* new_action;
    gboolean is_present;
    gboolean data_changed = FALSE;
    
    if ( !( type && type[0] != '\0' && desktop_id && desktop_id[0] != '\0' ) )
    {
        g_warning( "mime_type_update_association invalid type or desktop_id" );
        return;
    }
    if ( action > MIME_TYPE_ACTION_REMOVE || action < MIME_TYPE_ACTION_DEFAULT )
    {
        g_warning( "mime_type_update_association invalid action" );
        return;
    }
    
    char* dir = g_build_filename( g_get_user_data_dir(), "applications", NULL );
    char* path = g_build_filename( dir, "mimeapps.list", NULL );

    g_mkdir_with_parents( dir, 0700 );
    g_free( dir );

    // Load old mimeapps.list content, if available
    file = g_key_file_new();
    g_key_file_load_from_file( file, path, 0, NULL );
    
    for ( k = 0; k < G_N_ELEMENTS( groups ); k++ )
    {
        new_action = NULL;
        is_present = FALSE;
        apps = g_key_file_get_string_list( file, groups[k], type,
                                                            &n_apps, NULL );
        if ( apps )
        {
            for ( i = 0; i < n_apps; ++i )
            {
                g_strstrip( apps[i] );
                if ( apps[i][0] != '\0' )
                {
                    if ( !strcmp( apps[i], desktop_id ) )
                    {
                        // found desktop_id already in groups[k] list
                        if ( action == MIME_TYPE_ACTION_DEFAULT )
                        {
                            if ( k < 2 )
                            {
                                // Default Applications or Added Associations
                                if ( i == 0 )
                                {
                                    // is already first - skip change
                                    is_present = TRUE;
                                    break;
                                }
                                // in later position - remove it
                                continue;
                            }
                            else
                            {
                                // Removed Associations - remove it
                                is_present = TRUE;
                                continue;
                            }
                        }
                        else if ( action == MIME_TYPE_ACTION_APPEND )
                        {
                            if ( k < 2 )
                            {
                                // Default or Added - already present, skip change
                                is_present = TRUE;
                                break;
                            }
                            else
                            {
                                // Removed Associations - remove it
                                is_present = TRUE;
                                continue;
                            }
                        }
                        else //if ( action == MIME_TYPE_ACTION_REMOVE )
                        {
                            if ( k < 2 )
                            {
                                // Default or Added - remove it
                                is_present = TRUE;
                                continue;
                            }
                            else
                            {
                                // Removed Associations - already present
                                is_present = TRUE;
                                break;
                            }
                        }
                    }
                    // copy other apps to new list preserving order
                    str = new_action;
                    new_action = g_strdup_printf( "%s%s;", str ? str : "",
                                                                    apps[i] );
                    g_free( str );
                }
            }
            g_strfreev( apps );
        }
        
        // update key string if needed
        if ( action < MIME_TYPE_ACTION_REMOVE )
        {
            if ( ( k < 2 && !is_present ) || ( k == 2 && is_present ) )
            {
                if ( k < 2 )
                {
                    // add to front of Default or Added list
                    str = new_action;
                    if ( action == MIME_TYPE_ACTION_DEFAULT )
                        new_action = g_strdup_printf( "%s;%s", desktop_id,
                                              new_action ? new_action : "" );
                    else //if ( action == MIME_TYPE_ACTION_APPEND )
                        new_action = g_strdup_printf( "%s%s;",
                                              new_action ? new_action : "",
                                              desktop_id );
                    g_free( str );
                }
                if ( new_action )
                    g_key_file_set_string( file, groups[k], type, new_action );
                else
                    g_key_file_remove_key( file, groups[k], type, NULL );
                data_changed = TRUE;
            }
        }
        else //if ( action == MIME_TYPE_ACTION_REMOVE )
        {
            if ( ( k < 2 && is_present ) || ( k == 2 && !is_present ) )
            {
                if ( k == 2 )
                {
                    // add to end of Removed list
                    str = new_action;
                    new_action = g_strdup_printf( "%s%s;",
                                              new_action ? new_action : "",
                                              desktop_id );
                    g_free( str );
                }
                if ( new_action )
                    g_key_file_set_string( file, groups[k], type, new_action );
                else
                    g_key_file_remove_key( file, groups[k], type, NULL );
                data_changed = TRUE;
            }
        }
        g_free( new_action );
    }
    
    // save updated mimeapps.list
    if ( data_changed )
    {
        data = g_key_file_to_data( file, &len, NULL );
        save_to_file( path, data, len );
        g_free( data );
    }
    g_key_file_free( file );
    g_free( path );
}
