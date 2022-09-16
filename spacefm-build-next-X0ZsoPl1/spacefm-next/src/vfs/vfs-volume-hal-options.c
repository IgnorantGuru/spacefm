#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "vfs-volume-hal-options.h"

#include <string.h>
#include <unistd.h> /* for getuid() */
#include <sys/types.h>

#include <locale.h> /* for setlocale() */

#define CONFIG_FILE     PACKAGE_DATA_DIR "/mount.rules"

gboolean vfs_volume_hal_get_options( const char* fs, VFSVolumeOptions* ret )
{
    GKeyFile* f;
    if( fs == NULL || ! *fs)
        return FALSE;
    g_return_val_if_fail( ret != NULL, FALSE );

    f = g_key_file_new();
    if( g_key_file_load_from_file( f, CONFIG_FILE, 0, NULL) )
    {
        gsize n = 0;
	int i;
        ret->mount_options = g_key_file_get_string_list( f, fs, "mount_options", &n, NULL );
        ret->fstype_override = g_key_file_get_string(f, fs, "fstype_override", NULL );

        for( i = 0; i < n; ++i )
        {
            /* replace "uid=" with "uid=<actual uid>" */
#ifndef __FreeBSD__
            if (strcmp (ret->mount_options[i], "uid=") == 0) {
                g_free (ret->mount_options[i]);
                ret->mount_options[i] = g_strdup_printf ("uid=%u", getuid ());
            }
#else
            if (strcmp (ret->mount_options[i], "-u=") == 0) {
                g_free (ret->mount_options[i]);
                ret->mount_options[i] = g_strdup_printf ("-u=%u", getuid ());
            }
#endif
            /* for ntfs-3g */
            if (strcmp (ret->mount_options[i], "locale=") == 0) {
                g_free (ret->mount_options[i]);
                ret->mount_options[i] = g_strdup_printf ("locale=%s", setlocale (LC_ALL, ""));
            }
        }
    }
    else
    {
        ret->mount_options = NULL;
        ret->fstype_override = NULL;
    }
    g_key_file_free(f);
    return (ret->mount_options || ret->fstype_override);
}
