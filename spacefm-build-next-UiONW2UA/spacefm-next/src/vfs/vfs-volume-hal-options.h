#ifndef _VFS_VOLUME_HAL_OPTIONS_H_
#define _VFS_VOLUME_HAL_OPTIONS_H_

#include <glib.h>

G_BEGIN_DECLS

typedef struct _VFSVolumeOptions {
    char** mount_options;
    char* fstype_override;
}VFSVolumeOptions;

 gboolean vfs_volume_hal_get_options( const char* fs, VFSVolumeOptions* ret );

G_END_DECLS

#endif
