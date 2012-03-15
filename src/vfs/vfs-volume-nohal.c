/*
*  C Implementation: vfs-volume
*
* Description:
*
* //MOD this entire file has been rewritten in spacefm to accomodate udisks
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "vfs-volume.h"
#include "glib-mem.h"
#include <glib/gi18n.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "ptk-file-task.h"
#include "main-window.h"

// waitpid
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

#include <signal.h>  // kill
 
#ifdef HAVE_STATVFS
#include <sys/statvfs.h>
#endif
#include <vfs-file-info.h>

void vfs_volume_monitor_start();
VFSVolume* vfs_volume_read_by_device( char* device_file );
static void vfs_volume_device_added ( VFSVolume* volume, gboolean automount );
static void vfs_volume_device_removed ( char* device_file );
static void call_callbacks( VFSVolume* vol, VFSVolumeState state );


typedef struct _VFSVolumeCallbackData
{
    VFSVolumeCallback cb;
    gpointer user_data;
}VFSVolumeCallbackData;

static GList* volumes = NULL;
static GArray* callbacks = NULL;
GPid monpid = NULL;
gboolean global_inhibit_auto = FALSE;

static void cb_child_watch( GPid  pid, gint  status, char *data )
{
    g_spawn_close_pid( pid );
    if ( monpid == pid )
        monpid = NULL;
}

static gboolean cb_out_watch( GIOChannel *channel, GIOCondition cond,
                              char *data )
{
    gchar *line;
    gsize  size;
    char* parameter;
    char* value;
    char* valuec;
    char* linec;
    size_t colonx;
    VFSVolume* volume = NULL;


    if ( ( cond & G_IO_HUP ) || ( cond & G_IO_NVAL ) )
    {
        g_io_channel_unref( channel );
        return FALSE;
    }
    else if ( !( cond & G_IO_IN ) )
        return TRUE;

    if ( g_io_channel_read_line( channel, &line, &size, NULL, NULL )
                                            != G_IO_STATUS_NORMAL )
    {
        //printf("    !G_IO_STATUS_NORMAL\n");
        return TRUE;
    }
    
    //printf("umon: %s\n", line );
    // parse line to get parameter and value
        //added:     /org/freedesktop/UDisks/devices/sdd1
        //changed:     /org/freedesktop/UDisks/devices/sdd1
        //removed:   /org/freedesktop/UDisks/devices/sdd1
    colonx = strcspn( line, ":" );
    if ( colonx < strlen( line ) )
    {
        parameter = g_strndup(line, colonx );
        valuec = g_strdup( line + colonx + 1 );
        value = valuec;
        while ( value[0] == ' ' ) value++;
        while ( value[ strlen( value ) - 1 ] == '\n' )
            value[ strlen( value ) - 1 ] = '\0';
        if ( parameter && value )
        {
            if ( !strncmp( value, "/org/freedesktop/UDisks/devices/", 32) )
            {
                value += 27;
                value[0] = '/';
                value[1] = 'd';
                value[2] = 'e';
                value[3] = 'v';
                value[4] = '/';
                if ( !strcmp( parameter, "added" ) || !strcmp( parameter, "changed" ) )
                {
                    //printf( "umon add/change %s\n", value );
                    volume = vfs_volume_read_by_device( value );
                    vfs_volume_device_added( volume, TRUE );  //frees volume if needed
                }
                else if ( !strcmp( parameter, "removed" ) )
                {
                    //printf( "umon remove %s\n", value );
                    vfs_volume_device_removed( value );
                }
            }
            g_free( parameter );
            g_free( valuec );
        }
    }
    g_free( line );
    return TRUE;
}

void vfs_volume_monitor_start()
{
    GPid        pid;
    gchar      *argv[] = { "/usr/bin/udisks", "--monitor", NULL };
    gint        out,
                err;
    GIOChannel *out_ch,
               *err_ch;
    gboolean    ret;

    //printf("vfs_volume_monitor_start\n");
    if ( monpid )
    {
        //printf("monpid non-null - skipping start\n");
        return;
    }
    
    // start polling on /dev/sr0 - this also starts the udisks daemon
    char* stdout = NULL;
    char* stderr = NULL;

    g_spawn_command_line_sync( "/usr/bin/udisks --poll-for-media /dev/sr0",
                                                &stdout, &stderr, NULL, NULL );
    if ( stdout )
        g_free( stdout );
    if ( stderr )
        g_free( stderr );

    // start udisks monitor
    ret = g_spawn_async_with_pipes( NULL, argv, NULL,
                                    G_SPAWN_DO_NOT_REAP_CHILD   // | G_SPAWN_SEARCH_PATH
                                    | G_SPAWN_STDERR_TO_DEV_NULL,
                                    NULL, NULL, &pid, NULL, &out, NULL, NULL );
    if( ! ret )
    {
        //g_warning( "udisks monitor not available - is /usr/bin/udisks installed?" );
        return;
    }

    monpid = pid;
    
    // catch termination
    g_child_watch_add( pid, (GChildWatchFunc)cb_child_watch, NULL );

    // create channels for output
    out_ch = g_io_channel_unix_new( out );
    g_io_channel_set_close_on_unref( out_ch, TRUE );
    
    // if enable err channel, need to add &err to g_spawn_async_with_pipes
    //    and remove G_SPAWN_STDERR_TO_DEV_NULL flag
    //err_ch = g_io_channel_unix_new( err );
    //g_io_channel_set_close_on_unref( err_ch, TRUE );

    // Add watches to channels
    g_io_add_watch( out_ch, G_IO_IN | G_IO_HUP | G_IO_NVAL, (GIOFunc)cb_out_watch, NULL );
    //g_io_add_watch( err_ch, G_IO_IN | G_IO_HUP | G_IO_NVAL, (GIOFunc)cb_out_watch, NULL );

    //printf("started pid %d\n", pid);
}

void vfs_free_volume_members( VFSVolume* volume )
{
    if ( volume->device_file )
        g_free ( volume->device_file );
    if ( volume->udi )
        g_free ( volume->udi );
    if ( volume->label )
        g_free ( volume->label );
    if ( volume->mount_point )
        g_free ( volume->mount_point );
    if ( volume->disp_name )
        g_free ( volume->disp_name );
    if ( volume->icon )
        g_free ( volume->icon );
}

char* free_slash_total( const char* dir )
{
    guint64 total_size;
    char size_str[ 64 ];
#ifdef HAVE_STATVFS
    struct statvfs fs_stat = {0};
#endif

#ifdef HAVE_STATVFS
    if( statvfs( dir, &fs_stat ) == 0 )
    {
        char total_size_str[ 64 ];
        vfs_file_size_to_string_format( size_str, fs_stat.f_bsize * fs_stat.f_bavail,
                                                                                NULL );
        vfs_file_size_to_string_format( total_size_str,
                                        fs_stat.f_frsize * fs_stat.f_blocks, NULL );
        return g_strdup_printf( "%s/%s", size_str, total_size_str );
    }
#endif
    return g_strdup_printf( "" );
}

void vfs_volume_set_info( VFSVolume* volume )
{
    char* parameter;
    char* value;
    char* valuec;
    char* lastcomma;
    char* disp_device;
    char* disp_label;
    char* disp_size;
    char* disp_mount;
    char* disp_fstype;
    char* disp_id = NULL;
    char size_str[ 64 ];
    
    if ( !volume )
        return;

    // set device icon
    if ( volume->is_audiocd )
        volume->icon = g_strdup_printf( "dev_icon_audiocd" );
    else if ( volume->is_optical )
    {
        if ( volume->is_mounted )
            volume->icon = g_strdup_printf( "dev_icon_optical_mounted" );
        else if ( volume->is_mountable )
            volume->icon = g_strdup_printf( "dev_icon_optical_media" );
        else
            volume->icon = g_strdup_printf( "dev_icon_optical_nomedia" );
    }
    else if ( volume->is_floppy )
    {
        if ( volume->is_mounted )
            volume->icon = g_strdup_printf( "dev_icon_floppy_mounted" );
        else
            volume->icon = g_strdup_printf( "dev_icon_floppy_unmounted" );
        volume->is_mountable = TRUE;
    }
    else if ( volume->is_removable )
    {
        if ( volume->is_mounted )
            volume->icon = g_strdup_printf( "dev_icon_remove_mounted" );
        else
            volume->icon = g_strdup_printf( "dev_icon_remove_unmounted" );
    }
    else
    {
        if ( volume->is_mounted )
            volume->icon = g_strdup_printf( "dev_icon_internal_mounted" );
        else
            volume->icon = g_strdup_printf( "dev_icon_internal_unmounted" );
    }
    // set disp_id using by-id
    if ( volume->is_floppy && !volume->udi )
        disp_id = g_strdup_printf( _(":floppy") );
    else if ( volume->udi )
    {
        if ( lastcomma = strrchr( volume->udi, '/' ) )
        {
            lastcomma++;
            if ( !strncmp( lastcomma, "usb-", 4 ) )
                lastcomma += 4;
            else if ( !strncmp( lastcomma, "ata-", 4 ) )
                lastcomma += 4;
            else if ( !strncmp( lastcomma, "scsi-", 5 ) )
                lastcomma += 5;
        }
        else
            lastcomma = volume->udi;
        if ( lastcomma[0] != '\0' )
        {
            disp_id = g_strdup_printf( ":%.16s", lastcomma );
        }
    }
    else if ( volume->is_optical )
        disp_id = g_strdup_printf( _(":optical") );
    if ( !disp_id )
        disp_id = g_strdup_printf( "" );
    // table type
    if ( volume->is_table )
    {
        if ( volume->fs_type && volume->fs_type[0] == '\0' )
        {
            g_free( volume->fs_type );
            volume->fs_type = NULL;
        }
        if ( !volume->fs_type )
            volume->fs_type = g_strdup( "table" );
    }
    // set display name
    if ( volume->is_mounted )
    {
        if ( volume->label && volume->label[0] != '\0' )
            disp_label = g_strdup_printf( "%s", volume->label );
        else
            disp_label = g_strdup_printf( "" );
        /* if ( volume->mount_point && volume->mount_point[0] != '\0' )
        {
            // causes GUI hang during mount due to fs access
            disp_size = free_slash_total( volume->mount_point );
        }
        else */ if ( volume->size > 0 )
        {
            vfs_file_size_to_string_format( size_str, volume->size, NULL );
            disp_size = g_strdup_printf( "%s", size_str );
        }
        else
            disp_size = g_strdup_printf( "" );
        if ( volume->mount_point && volume->mount_point[0] != '\0' )
            disp_mount = g_strdup_printf( "%s", volume->mount_point );
        else
            disp_mount = g_strdup_printf( "???" );
    }
    else if ( volume->is_mountable )  //has_media
    {
        if ( volume->is_blank )
            disp_label = g_strdup_printf( _("[blank]") );
        else if ( volume->label && volume->label[0] != '\0' )
            disp_label = g_strdup_printf( "%s", volume->label );
        else if ( volume->is_audiocd )
            disp_label = g_strdup_printf( _("[audio]") );
        else
            disp_label = g_strdup_printf( "" );
        if ( volume->size > 0 )
        {
            vfs_file_size_to_string_format( size_str, volume->size, NULL );
            disp_size = g_strdup_printf( "%s", size_str );
        }
        else
            disp_size = g_strdup_printf( "" );
        disp_mount = g_strdup_printf( "---" );
    }
    else
    {
        disp_label = g_strdup_printf( _("[no media]") );
        disp_size = g_strdup_printf( "" );
        disp_mount = g_strdup_printf( "" );
    }
    if ( !strncmp( volume->device_file, "/dev/", 5 ) )
        disp_device = g_strdup( volume->device_file + 5 );
    else
        disp_device = g_strdup( volume->device_file );
    if ( volume->fs_type && volume->fs_type[0] != '\0' )
        disp_fstype = g_strdup( volume->fs_type );// g_strdup_printf( "-%s", volume->fs_type );
    else
        disp_fstype = g_strdup( "" );
 
    char* fmt = xset_get_s( "dev_dispname" );
    if ( !fmt )
        parameter = g_strdup_printf( "%s %s %s %s %s %s", 
                            disp_device, disp_size, disp_fstype, disp_label,
                            disp_mount, disp_id );
    else
    {
        value = replace_string( fmt, "%v", disp_device, FALSE );
        valuec = replace_string( value, "%s", disp_size, FALSE );
        g_free( value );
        value = replace_string( valuec, "%t", disp_fstype, FALSE );
        g_free( valuec );
        valuec = replace_string( value, "%l", disp_label, FALSE );
        g_free( value );
        value = replace_string( valuec, "%m", disp_mount, FALSE );
        g_free( valuec );
        parameter = replace_string( value, "%i", disp_id, FALSE );
        g_free( value );
    }
    
    //volume->disp_name = g_filename_to_utf8( parameter, -1, NULL, NULL, NULL );
    while ( strstr( parameter, "  " ) )
    {
        value = parameter;
        parameter = replace_string( value, "  ", " ", FALSE );
        g_free( value );
    }
    while ( parameter[0] == ' ' )
    {
        value = parameter;
        parameter = g_strdup( parameter + 1 );
        g_free( value );
    }
    volume->disp_name = g_filename_display_name( parameter );

    g_free( parameter );
    g_free( disp_label );
    g_free( disp_size );
    g_free( disp_mount );
    g_free( disp_device );
    g_free( disp_fstype );
    g_free( disp_id );

    if ( !volume->udi )
        volume->udi = g_strdup( volume->device_file );
}

VFSVolume* vfs_volume_read_by_device( char* device_file )
{
    // uses udisks to read device parameters into returned volume
    FILE *fp;
    char line[1024];
    char* value;
    char* valuec;
    char* linec;
    char* lastcomma;
    char* parameter;
    VFSVolume* volume = NULL;
    size_t colonx;
    char* device_file_delayed;
    
    //printf( ">>> udisks --show-info %s\n", device_file );
    fp = popen(g_strdup_printf( "/usr/bin/udisks --show-info %s", device_file ), "r");
    if (fp)
    {
        volume = g_slice_new0( VFSVolume );
        volume->device_file = NULL;
        volume->udi = NULL;
        volume->is_optical = FALSE;
        volume->is_table = FALSE;
        volume->is_floppy = FALSE;
        volume->is_removable = FALSE;
        volume->requires_eject = FALSE;
        volume->is_mountable = FALSE;
        volume->is_audiocd = FALSE;
        volume->is_dvd = FALSE;
        volume->is_blank = FALSE;
        volume->is_mounted = FALSE;
        volume->is_user_visible = TRUE;
        volume->ever_mounted = FALSE;
        volume->open_main_window = NULL;
        volume->nopolicy = FALSE;
        volume->mount_point = NULL;
        volume->size = 0;
        volume->label = NULL;
        volume->fs_type = NULL;
        volume->disp_name = NULL;
        volume->icon = NULL;
        volume->automount_time = 0;
        volume->inhibit_auto = FALSE;
        while (fgets(line, sizeof(line)-1, fp) != NULL)
        {
            // parse line to get parameter and value
            colonx = strcspn( line, ":" );
            if ( colonx < strlen( line ) )
            {
                parameter = g_strndup(line, colonx );
                valuec = g_strdup( line + colonx + 1 );
                value = valuec;
                while ( value[0] == ' ' ) value++;
                while ( value[ strlen( value ) - 1 ] == '\n' )
                    value[ strlen( value ) - 1 ] = '\0';
                if ( parameter && value )
                {
                    // set volume from parameters
                    //printf( "    (%s)=%s\n", parameter, value );
                    if ( !strcmp( parameter, "  device-file" ) )
                        volume->device_file = g_strdup( value );
                    else if ( !strcmp( parameter, "  system internal" ) )
                        volume->is_removable = !atoi( value );
                    else if ( !strcmp( parameter, "    ejectable" ) )
                        volume->requires_eject = atoi( value );
                    else if ( !strcmp( parameter, "  is mounted" ) )
                        volume->is_mounted = atoi( value );
                    else if ( !strcmp( parameter, "  has media" ) )
                        volume->is_mountable = atoi( value );
                    else if ( !strcmp( parameter, "    blank" ) && atoi( value ) )
                        volume->is_blank = TRUE;
                    else if ( !strcmp( parameter, "    num audio tracks" ) )
                        volume->is_audiocd = atoi( value ) > 0;
                    else if ( !strcmp( parameter, "    media" )
                                            && strstr( value, "optical_dvd" ) )
                        volume->is_dvd = TRUE;
                    else if ( !strcmp( parameter, "  presentation hide" ) )
                        volume->is_user_visible = !atoi( value );
                    else if ( !strcmp( parameter, "  presentation nopolicy" ) )
                        volume->nopolicy = atoi( value );
                    // use last mount path as mount point
                    else if ( !strcmp( parameter, "  mount paths" ) )
                    {
                        lastcomma = strrchr( value, ',' );
                        if ( lastcomma )
                        {
                            lastcomma++;
                            while ( lastcomma[0] == ' ' ) lastcomma++;
                            volume->mount_point = g_strdup( lastcomma );
                        }
                        else
                            volume->mount_point = g_strdup( value );
                    }
                    else if ( !strcmp( parameter, "    by-id" ) && !volume->udi )
                        volume->udi = g_strdup( value );
                    else if ( !strcmp( parameter, "  size" ) )
                        volume->size = atoll( value );
                    else if ( !strcmp( parameter, "  label" ) )
                        volume->label = g_strdup( value );
                    else if ( !strcmp( parameter, "      compat" ) )
                    {
                        if ( strstr( value, "optical" ) != NULL )
                            volume->is_optical = TRUE;
                        if ( !strcmp( value, "floppy" ) )
                            volume->is_floppy = TRUE;
                    }
                    else if ( !strcmp( parameter, "  type" ) )
                        volume->fs_type = g_strdup( value );
                    else if ( !strcmp( parameter, "  partition table" ) )
                        volume->is_table = TRUE;
                    else if ( !strcmp( parameter, "    type" ) && !strcmp( value, "0x05" ) )
                        volume->is_table = TRUE;  //extended partition
                    
                    g_free( parameter );
                    g_free( valuec );
                }
            }
        }
        pclose(fp);

        volume->ever_mounted = volume->is_mounted;
        //if ( volume->is_blank )
        //    volume->is_mountable = FALSE;  //has_media is now used for showing
        if ( volume->is_dvd )
            volume->is_audiocd = FALSE;        

        if ( volume->device_file && !strcmp( volume->device_file, device_file ) )
        {
            vfs_volume_set_info( volume );
/*
            printf( "====device_file=%s\n", volume->device_file );
            printf( "    udi=%s\n", volume->udi );
            printf( "    label=%s\n", volume->label );
            printf( "    icon=%s\n", volume->icon );
            printf( "    is_mounted=%d\n", volume->is_mounted );
            printf( "    is_mountable=%d\n", volume->is_mountable );
            printf( "    is_optical=%d\n", volume->is_optical );
            printf( "    is_audiocd=%d\n", volume->is_audiocd );
            printf( "    is_blank=%d\n", volume->is_blank );
            printf( "    is_floppy=%d\n", volume->is_floppy );
            printf( "    is_table=%d\n", volume->is_table );
            printf( "    is_removable=%d\n", volume->is_removable );
            printf( "    requires_eject=%d\n", volume->requires_eject );
            printf( "    is_user_visible=%d\n", volume->is_user_visible );
            printf( "    mount_point=%s\n", volume->mount_point );
            printf( "    size=%u\n", volume->size );
            printf( "    disp_name=%s\n", volume->disp_name );
*/
        }
        else
        {
            vfs_free_volume_members( volume );
            g_slice_free( VFSVolume, volume );
            volume = NULL;
        }
    }
//printf( ">>> udisks --show-info %s DONE\n", device_file );
    return volume;
}

gboolean vfs_volume_is_automount( VFSVolume* vol )
{   // determine if volume should be automounted or auto-unmounted
    int i, j;
    char* test;
    char* value;
    
    if ( !vol->is_mountable || vol->is_blank )
        return FALSE;

    char* showhidelist = g_strdup_printf( " %s ", xset_get_s( "dev_automount_volumes" ) );
    for ( i = 0; i < 3; i++ )
    {
        for ( j = 0; j < 2; j++ )
        {
            if ( i == 0 )
                value = vol->device_file;
            else if ( i == 1 )
                value = vol->label;
            else
            {
                value = vol->udi;
                value = strrchr( value, '/' );
                if ( value )
                    value++;
            }
            if ( j == 0 )
                test = g_strdup_printf( " +%s ", value );
            else
                test = g_strdup_printf( " -%s ", value );
            if ( strstr( showhidelist, test ) )
            {
                g_free( test );
                g_free( showhidelist );
                return ( j == 0 );
            }
            g_free( test );
        }
    }
    g_free( showhidelist );

    // udisks no?
    if ( vol->nopolicy && !xset_get_b( "dev_ignore_udisks_nopolicy" ) )
        return FALSE;

    // table?
    if ( vol->is_table )
        return FALSE;

    // optical
    if ( vol->is_optical )
        return xset_get_b( "dev_automount_optical" );

    // internal?
    if ( vol->is_removable && xset_get_b( "dev_automount_removable" ) )
        return TRUE;

    return FALSE;
}

char* vfs_volume_get_mount_options( VFSVolume* vol, char* options )
{
    if ( !options )
        return NULL;
    // change spaces to commas
    gboolean leading = TRUE;
    gboolean trailing = FALSE;
    int j = -1;
    int i;
    char news[ strlen( options ) + 1 ];
    for ( i = 0; i < strlen ( options ); i++ )
    {
        if ( leading && ( options[ i ] == ' ' || options[ i ] == ',' ) )
            continue;
        if ( options[ i ] != ' ' && options[ i ] != ',' )
        {
            j++;
            news[ j ] = options[ i ];
            trailing = TRUE;
            leading = FALSE;
        }
        else if ( trailing )
        {
            j++;
            news[ j ] = ',';
            trailing = FALSE;
        }
    }
    if ( news[ j ] == ',' )
        news[ j ] = '\0';
    else
    {
        j++;
        news[ j ] = '\0';
    }

    // no options
    if ( news[0] == '\0' )
        return NULL;
        
    // parse options with fs type
    // nosuid,sync+vfat,utf+vfat,nosuid-ext4
    char* opts = g_strdup_printf( ",%s,", news );
    char* fstype = vfs_volume_get_fstype( vol );
    char newo[ strlen( opts ) + 1 ];
    newo[0] = ',';
    newo[1] = '\0';
    char* newoptr = newo + 1;
    char* ptr = opts + 1;
    char* comma;
    char* single;
    char* singlefs;
    char* plus;
    char* test;
    while ( ptr[0] != '\0' )
    {
        comma = strchr( ptr, ',' );
        single = g_strndup( ptr, comma - ptr );
        if ( !strchr( single, '+' ) && !strchr( single, '-' ) )
        {
            // pure option, check for -fs option
            test = g_strdup_printf( ",%s-%s,", single, fstype );
            if ( !strstr( opts, test ) )
            {   
                // add option
                strcpy( newoptr, single );
                newoptr = newo + strlen( newo );
                newoptr[0] = ',';
                newoptr[1] = '\0';
                newoptr++;    
            }
            g_free( test );
        }
        else if ( plus = strchr( single, '+' ) )
        {
            //opt+fs
            plus[0] = '\0';  //set single to just option
            singlefs = g_strdup_printf( "%s+%s", single, fstype );
            plus[0] = '+';   //restore single to option+fs
            if ( !strcmp( singlefs, single ) )
            {
                // correct fstype, check if already in options
                plus[0] = '\0';  //set single to just option
                test = g_strdup_printf( ",%s,", single );
                if ( !strstr( newo, test ) )
                {
                    // add +fs option
                    strcpy( newoptr, single );
                    newoptr = newo + strlen( newo );
                    newoptr[0] = ',';
                    newoptr[1] = '\0';
                    newoptr++;    
                }
                g_free( test );
            }
            g_free( singlefs );
        }
        g_free( single );
        ptr = comma + 1;
    }
    newoptr--;
    newoptr[0] = '\0';
    g_free( opts );
    if ( newo[1] == '\0' )
        return NULL;
    else
        return g_strdup( newo + 1 );
}

char* vfs_volume_get_mount_command( VFSVolume* vol, char* default_options )
{
    char* command;
    
    char* options = vfs_volume_get_mount_options( vol, default_options );
    if ( options )
    {
        command = g_strdup_printf( "/usr/bin/udisks --mount %s --mount-options %s",
                                                    vol->device_file, options );
        g_free( options );
    }
    else
        command = g_strdup_printf( "/usr/bin/udisks --mount %s", vol->device_file );
    return command;
}

void vfs_volume_exec( VFSVolume* vol, char* command )
{
//printf( "vfs_volume_exec %s %s\n", vol->device_file, command );
    if ( !command || command[0] == '\0' )
        return;

    char *s1;
    char *s2;
    
    s1 = replace_string( command, "%m", vol->mount_point, TRUE );
    s2 = replace_string( s1, "%l", vol->label, TRUE );
    g_free( s1 );
    s1 = replace_string( s2, "%v", vol->device_file, FALSE );
    g_free( s2 );

    GList* files = NULL;
    files = g_list_prepend( files, g_strdup_printf( "autoexec" ) );

    printf( _("Autoexec: %s\n"), s1 );
    PtkFileTask* task = ptk_file_task_new( VFS_FILE_TASK_EXEC, files, "/", NULL,
                                                                        NULL );
    task->task->exec_action = g_strdup_printf( "autoexec" );
    task->task->exec_command = s1;
    task->task->exec_sync = FALSE;
    task->task->exec_export = FALSE;
task->task->exec_keep_tmp = FALSE;
    ptk_file_task_run( task );
    
}

void vfs_volume_autoexec( VFSVolume* vol )
{
    char* command = NULL;
    char* path;

    // Note: audiocd is is_mountable
    if ( !vol->is_mountable || global_inhibit_auto ||
                                                !vfs_volume_is_automount( vol ) )
        return;

    if ( vol->is_audiocd )
    {
        command = xset_get_s( "dev_exec_audio" );
    }
    else if ( vol->is_mounted && vol->mount_point && vol->mount_point[0] !='\0' )
    {
        if ( vol->inhibit_auto )
        {
            // user manually mounted this vol, so no autoexec this time
            vol->inhibit_auto = FALSE;
            return;
        }
        else
        {
            path = g_build_filename( vol->mount_point, "VIDEO_TS", NULL );
            if ( vol->is_dvd && g_file_test( path, G_FILE_TEST_IS_DIR ) )
                command = xset_get_s( "dev_exec_video" );
            else
            {
                if ( xset_get_b( "dev_auto_open" ) )
                {
                    FMMainWindow* main_window = fm_main_window_get_last_active();
                    if ( main_window )
                    {
                        printf( _("Auto Open Tab for %s in %s\n"), vol->device_file,
                                                                vol->mount_point );
                        //PtkFileBrowser* file_browser = 
                        //        (PtkFileBrowser*)fm_main_window_get_current_file_browser(
                        //                                                main_window );
                        //if ( file_browser )
                        //    ptk_file_browser_emit_open( file_browser, vol->mount_point,
                        //                                        PTK_OPEN_DIR ); //PTK_OPEN_NEW_TAB
                        //fm_main_window_add_new_tab causes hang without GDK_THREADS_ENTER
                        GDK_THREADS_ENTER();
                        fm_main_window_add_new_tab( main_window, vol->mount_point );
                        GDK_THREADS_LEAVE();
                        //printf("DONE Auto Open Tab for %s in %s\n", vol->device_file,
                        //                                        vol->mount_point );
                    }
                    else
                    {
                        char* prog = g_find_program_in_path( g_get_prgname() );
                        if ( !prog )
                            prog = g_strdup( g_get_prgname() );
                        if ( !prog )
                            prog = g_strdup( "spacefm" );
                        char* quote_path = bash_quote( vol->mount_point );
                        char* line = g_strdup_printf( "%s -t %s", prog, quote_path );
                        g_spawn_command_line_async( line, NULL );
                        g_free( prog );
                        g_free( quote_path );
                        g_free( line );
                    }
                }
                command = xset_get_s( "dev_exec_fs" );
            }
            g_free( path );
        }
    }
    vfs_volume_exec( vol, command );
}

void vfs_volume_autounmount( VFSVolume* vol )
{
    if ( !vol->is_mounted || !vfs_volume_is_automount( vol ) )
        return;

    char* line = g_strdup_printf( "/usr/bin/udisks --unmount %s", vol->device_file );
    printf( _("Auto-Unmount: %s\n"), line );
    g_spawn_command_line_async( line, NULL );
    g_free( line );
}

void vfs_volume_automount( VFSVolume* vol )
{
    if ( vol->is_mounted || vol->ever_mounted || vol->is_audiocd
                                        || !vfs_volume_is_automount( vol ) )
        return;

    if ( vol->automount_time && time( NULL ) - vol->automount_time < 5 )
        return;
    vol->automount_time = time( NULL );

    char* line = vfs_volume_get_mount_command( vol,
                                            xset_get_s( "dev_mount_options" ) );
    printf( _("Automount: %s\n"), line );
    g_spawn_command_line_async( line, NULL );
    g_free( line );
}

static void vfs_volume_device_added( VFSVolume* volume, gboolean automount )
{           //frees volume if needed
    GList* l;
    gboolean was_mounted, was_audiocd, was_mountable;
    
    if ( !volume || !volume->udi || !volume->device_file )
        return;

    // check if we already have this volume device file
    for ( l = volumes; l; l = l->next )
    {
        if ( !strcmp( ((VFSVolume*)l->data)->device_file, volume->device_file ) )
        {
            // update existing volume
            was_mounted = ((VFSVolume*)l->data)->is_mounted;
            was_audiocd = ((VFSVolume*)l->data)->is_audiocd;
            was_mountable = ((VFSVolume*)l->data)->is_mountable;
            vfs_free_volume_members( (VFSVolume*)l->data );
            ((VFSVolume*)l->data)->udi = g_strdup( volume->udi );
            ((VFSVolume*)l->data)->device_file = g_strdup( volume->device_file );
            ((VFSVolume*)l->data)->label = g_strdup( volume->label );
            ((VFSVolume*)l->data)->mount_point = g_strdup( volume->mount_point );
            ((VFSVolume*)l->data)->icon = g_strdup( volume->icon );
            ((VFSVolume*)l->data)->disp_name = g_strdup( volume->disp_name );
            ((VFSVolume*)l->data)->is_mounted = volume->is_mounted;
            ((VFSVolume*)l->data)->is_mountable = volume->is_mountable;
            ((VFSVolume*)l->data)->is_optical = volume->is_optical;
            ((VFSVolume*)l->data)->requires_eject = volume->requires_eject;
            ((VFSVolume*)l->data)->is_removable = volume->is_removable;
            ((VFSVolume*)l->data)->is_user_visible = volume->is_user_visible;
            ((VFSVolume*)l->data)->size = volume->size;
            ((VFSVolume*)l->data)->is_table = volume->is_table;
            ((VFSVolume*)l->data)->is_floppy = volume->is_floppy;
            ((VFSVolume*)l->data)->nopolicy = volume->nopolicy;
            ((VFSVolume*)l->data)->fs_type = volume->fs_type;
            ((VFSVolume*)l->data)->is_blank = volume->is_blank;
            ((VFSVolume*)l->data)->is_audiocd = volume->is_audiocd;
            ((VFSVolume*)l->data)->is_dvd = volume->is_dvd;

            // Mount and ejection detect for automount
            if ( volume->is_mounted )
            {
                ((VFSVolume*)l->data)->ever_mounted = TRUE;
                ((VFSVolume*)l->data)->automount_time = 0;
            }
            else
            {
                if ( volume->is_removable && !volume->is_mountable )  // ejected
                {
                    ((VFSVolume*)l->data)->ever_mounted = FALSE;
                    ((VFSVolume*)l->data)->automount_time = 0;
                    ((VFSVolume*)l->data)->inhibit_auto = FALSE;
                }
            }
                
            call_callbacks( (VFSVolume*)l->data, VFS_VOLUME_CHANGED );

            vfs_free_volume_members( volume );
            g_slice_free( VFSVolume, volume );

            volume = (VFSVolume*)l->data;
            if ( automount )
            {
                vfs_volume_automount( volume );
                if ( !was_mounted && volume->is_mounted )
                    vfs_volume_autoexec( volume );
                else if ( was_mounted && !volume->is_mounted )
                    vfs_volume_exec( volume, xset_get_s( "dev_exec_unmount" ) );
                else if ( !was_audiocd && volume->is_audiocd )
                    vfs_volume_autoexec( volume );
                
                if ( !was_mountable && volume->is_mountable )   //media inserted
                    vfs_volume_exec( volume, xset_get_s( "dev_exec_insert" ) );
            }
            return;
        }
    }

    // add as new volume
    volumes = g_list_append( volumes, volume );
    call_callbacks( volume, VFS_VOLUME_ADDED );
    if ( automount )
    {
        vfs_volume_automount( volume );
        vfs_volume_exec( volume, xset_get_s( "dev_exec_insert" ) );
        if ( volume->is_audiocd )
            vfs_volume_autoexec( volume );
    }
}

static void vfs_volume_device_removed( char* device_file )
{
    GList* l;
    VFSVolume* volume;
    
    if ( !device_file )
        return;

    for ( l = volumes; l; l = l->next )
    {
        if ( !strcmp( ((VFSVolume*)l->data)->device_file, device_file ) )
        {
            // remove volume
            //printf("remove volume %s\n", device_file );
            volume = (VFSVolume*)l->data;
            vfs_volume_exec( volume, xset_get_s( "dev_exec_remove" ) );
            volumes = g_list_remove( volumes, volume );
            call_callbacks( volume, VFS_VOLUME_REMOVED );
            vfs_free_volume_members( volume );
            g_slice_free( VFSVolume, volume );
            return;
        }
    }
}

gboolean on_cancel_inhibit_timer( gpointer user_data )
{
    global_inhibit_auto = FALSE;
    return FALSE;
}

gboolean vfs_volume_init ()
{
    FILE *fp;
    char line[1024];
    VFSVolume* volume;
    GList* l;
    
    if ( !g_file_test( "/usr/bin/udisks", G_FILE_TEST_EXISTS ) )
        return TRUE;

    // lookup all devices currently known to udisks
    fp = popen("/usr/bin/udisks --enumerate-device-files", "r");
    if ( fp )
    {
        while (fgets( line, sizeof( line )-1, fp ) != NULL)
        {
            if ( strncmp( line, "/dev/disk/", 10 ) && !strncmp( line, "/dev/", 5 ) )
            {
                line[ strlen( line ) - 1 ] = '\0';  // removes trailing \n
                if ( volume = vfs_volume_read_by_device( line ) )
                    vfs_volume_device_added( volume, FALSE );  // frees volume if needed
            }
        }
        pclose( fp );
    }
    global_inhibit_auto = TRUE; // don't autoexec during startup
    vfs_volume_monitor_start();
    for ( l = volumes; l; l = l->next )
        vfs_volume_automount( (VFSVolume*)l->data );
    g_timeout_add_seconds( 3, ( GSourceFunc ) on_cancel_inhibit_timer, NULL );
    return TRUE;
}


gboolean vfs_volume_finalize()
{
    GList* l;

    // stop udisks monitor
    if ( monpid )
    {
        if ( !waitpid( monpid, NULL, WNOHANG ) )
        {
            kill( monpid, SIGUSR1 );
            monpid = NULL;
        }
    }

    if ( callbacks )
        g_array_free( callbacks, TRUE );

    gboolean unmount_all = xset_get_b( "dev_unmount_quit" );
    if ( G_LIKELY( volumes ) )
    {
        for ( l = volumes; l; l = l->next )
        {
            if ( unmount_all )
                vfs_volume_autounmount( (VFSVolume*)l->data );
            vfs_free_volume_members( (VFSVolume*)l->data );
            g_slice_free( VFSVolume, l->data );
        }
    }
    volumes = NULL;
    return TRUE;
}

const GList* vfs_volume_get_all_volumes()
{
    return volumes;
}

static void call_callbacks( VFSVolume* vol, VFSVolumeState state )
{
    int i;
    VFSVolumeCallbackData* e;

    if ( !callbacks )
        return ;

    e = ( VFSVolumeCallbackData* ) callbacks->data;
    for ( i = 0; i < callbacks->len; ++i )
    {
        ( *e[ i ].cb ) ( vol, state, e[ i ].user_data );
    }
}

void vfs_volume_add_callback( VFSVolumeCallback cb, gpointer user_data )
{
    VFSVolumeCallbackData e;
    if ( !cb )
        return;

    if ( !callbacks )
        callbacks = g_array_sized_new( FALSE, FALSE, sizeof( VFSVolumeCallbackData ), 8 );
    e.cb = cb;
    e.user_data = user_data;
    callbacks = g_array_append_val( callbacks, e );
}

void vfs_volume_remove_callback( VFSVolumeCallback cb, gpointer user_data )
{
    int i;
    VFSVolumeCallbackData* e;

    if ( !callbacks )
        return ;

    e = ( VFSVolumeCallbackData* ) callbacks->data;
    for ( i = 0; i < callbacks->len; ++i )
    {
        if ( e[ i ].cb == cb && e[ i ].user_data == user_data )
        {
            callbacks = g_array_remove_index_fast( callbacks, i );
            if ( callbacks->len > 8 )
                g_array_set_size( callbacks, 8 );
            break;
        }
    }
}

gboolean vfs_volume_mount( VFSVolume* vol, GError** err )
{
    return TRUE;
}

gboolean vfs_volume_umount( VFSVolume *vol, GError** err )
{
    return TRUE;
}

gboolean vfs_volume_eject( VFSVolume *vol, GError** err )
{
    return TRUE;
}

const char* vfs_volume_get_disp_name( VFSVolume *vol )
{
    return vol->disp_name;
}

const char* vfs_volume_get_mount_point( VFSVolume *vol )
{
    return vol->mount_point;
}

const char* vfs_volume_get_device( VFSVolume *vol )
{
    return vol->device_file;
}
const char* vfs_volume_get_fstype( VFSVolume *vol )
{
    return vol->fs_type;
}

const char* vfs_volume_get_icon( VFSVolume *vol )
{
    if ( !vol->icon )
        return NULL;        
    XSet* set = xset_get( vol->icon );
    return set->icon;
}

gboolean vfs_volume_is_removable( VFSVolume *vol )
{
    return vol->is_removable;
}

gboolean vfs_volume_is_mounted( VFSVolume *vol )
{
    return vol->is_mounted;
}

gboolean vfs_volume_requires_eject( VFSVolume *vol )
{
    return vol->requires_eject;
}


