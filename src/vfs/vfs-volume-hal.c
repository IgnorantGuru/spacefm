/*
 *      vfs-volume-hal.c - Support voulme management with HAL
 *
 *      Copyright (C) 2004-2005 Red Hat, Inc, David Zeuthen <davidz@redhat.com>
 *      Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
 *      Copyright 2008 PCMan <pcman.tw@gmail.com>
 *
 *      This file contains source code from other projects:
 *
 *      The icon-name and volume display name parts are taken from
 *      gnome-vfs-hal-mounts.c of libgnomevfs by David Zeuthen <davidz@redhat.com>.
 *
 *      The HAL volume listing and updating-related parts are taken from
 *      thunar-vfs-volume-hal.c of Thunar by Benedikt Meurer <benny@xfce.org>.
 *
 *      The mount/umount/eject-related source code is modified from exo-mount.c
 *      taken from libexo. It's originally written by Benedikt Meurer <benny@xfce.org>.
 *
 *      The fstab-related mount/umount/eject code is modified from gnome-mount.c
 *      taken from gnome-mount. The original author is David Zeuthen, <david@fubar.dk>.
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

/*
 *  NOTE:
 *  Originally, I want to use source code from gnome-vfs-hal-mounts.c of gnomevfs
 *  because its implementation is more complete and *might* be more correct.
 *  Unfortunately, it's poorly-written, and thanks to the complicated and
 *  poorly-documented HAL, the readability of most source code in
 *  gnome-vfs-hal-mounts.c is very poor. So I decided to use the implementation
 *  provided in Thunar and libexo, which are much cleaner IMHO.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_HAL

/* uncomment to get helpful debug messages */
/* #define HAL_SHOW_DEBUG */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_SYSMACROS_H
#include <sys/sysmacros.h>
#endif
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>

#include <glib.h>

#include <libhal.h>
#include <libhal-storage.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "vfs-volume.h"
#include "glib-mem.h"
#include "vfs-file-info.h"
#include "vfs-volume-hal-options.h"
#include "vfs-utils.h"  /* for vfs_sudo_cmd() */
#include "main-window.h" //sfm for main_window_event

#include <glib/gi18n.h>
#include <errno.h>
#include <locale.h>

#ifndef PATHNAME_MAX
# define PATHNAME_MAX   1024
#endif

/* For fstab related things */
#if !defined(sun) && !defined(__FreeBSD__)
#include <mntent.h>
#elif defined(__FreeBSD__)
#include <fstab.h>
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#elif defined(sun)
#include <sys/mnttab.h>
#endif

typedef enum {
    HAL_ICON_DRIVE_REMOVABLE_DISK           = 0x10000,
    HAL_ICON_DRIVE_REMOVABLE_DISK_IDE       = 0x10001,
    HAL_ICON_DRIVE_REMOVABLE_DISK_SCSI      = 0x10002,
    HAL_ICON_DRIVE_REMOVABLE_DISK_USB       = 0x10003,
    HAL_ICON_DRIVE_REMOVABLE_DISK_IEEE1394  = 0x10004,
    HAL_ICON_DRIVE_REMOVABLE_DISK_CCW       = 0x10005,
    HAL_ICON_DRIVE_DISK                     = 0x10100,
    HAL_ICON_DRIVE_DISK_IDE                 = 0x10101,
    HAL_ICON_DRIVE_DISK_SCSI                = 0x10102,
    HAL_ICON_DRIVE_DISK_USB                 = 0x10103,
    HAL_ICON_DRIVE_DISK_IEEE1394            = 0x10104,
    HAL_ICON_DRIVE_DISK_CCW                 = 0x10105,
    HAL_ICON_DRIVE_CDROM                    = 0x10200,
    HAL_ICON_DRIVE_CDWRITER                 = 0x102ff,
    HAL_ICON_DRIVE_FLOPPY                   = 0x10300,
    HAL_ICON_DRIVE_TAPE                     = 0x10400,
    HAL_ICON_DRIVE_COMPACT_FLASH            = 0x10500,
    HAL_ICON_DRIVE_MEMORY_STICK             = 0x10600,
    HAL_ICON_DRIVE_SMART_MEDIA              = 0x10700,
    HAL_ICON_DRIVE_SD_MMC                   = 0x10800,
    HAL_ICON_DRIVE_CAMERA                   = 0x10900,
    HAL_ICON_DRIVE_PORTABLE_AUDIO_PLAYER    = 0x10a00,
    HAL_ICON_DRIVE_ZIP                      = 0x10b00,
        HAL_ICON_DRIVE_JAZ                      = 0x10c00,
        HAL_ICON_DRIVE_FLASH_KEY                = 0x10d00,

    HAL_ICON_VOLUME_REMOVABLE_DISK          = 0x20000,
    HAL_ICON_VOLUME_REMOVABLE_DISK_IDE      = 0x20001,
    HAL_ICON_VOLUME_REMOVABLE_DISK_SCSI     = 0x20002,
    HAL_ICON_VOLUME_REMOVABLE_DISK_USB      = 0x20003,
    HAL_ICON_VOLUME_REMOVABLE_DISK_IEEE1394 = 0x20004,
    HAL_ICON_VOLUME_REMOVABLE_DISK_CCW      = 0x20005,
    HAL_ICON_VOLUME_DISK                    = 0x20100,
    HAL_ICON_VOLUME_DISK_IDE                = 0x20101,
    HAL_ICON_VOLUME_DISK_SCSI               = 0x20102,
    HAL_ICON_VOLUME_DISK_USB                = 0x20103,
    HAL_ICON_VOLUME_DISK_IEEE1394           = 0x20104,
    HAL_ICON_VOLUME_DISK_CCW                = 0x20105,
    /* specifically left out as we use icons based on media type in the optical drive
    HAL_ICON_VOLUME_CDROM                   = 0x20200 */
    HAL_ICON_VOLUME_FLOPPY                  = 0x20300,
    HAL_ICON_VOLUME_TAPE                    = 0x20400,
    HAL_ICON_VOLUME_COMPACT_FLASH           = 0x20500,
    HAL_ICON_VOLUME_MEMORY_STICK            = 0x20600,
    HAL_ICON_VOLUME_SMART_MEDIA             = 0x20700,
    HAL_ICON_VOLUME_SD_MMC                  = 0x20800,
    HAL_ICON_VOLUME_CAMERA                  = 0x20900,
    HAL_ICON_VOLUME_PORTABLE_AUDIO_PLAYER   = 0x20a00,
    HAL_ICON_VOLUME_ZIP                     = 0x20b00,
        HAL_ICON_VOLUME_JAZ                     = 0x20c00,
        HAL_ICON_VOLUME_FLASH_KEY               = 0x20d00,

    HAL_ICON_DISC_CDROM                     = 0x30000,
    HAL_ICON_DISC_CDR                       = 0x30001,
    HAL_ICON_DISC_CDRW                      = 0x30002,
    HAL_ICON_DISC_DVDROM                    = 0x30003,
    HAL_ICON_DISC_DVDRAM                    = 0x30004,
    HAL_ICON_DISC_DVDR                      = 0x30005,
    HAL_ICON_DISC_DVDRW                     = 0x30006,
    HAL_ICON_DISC_DVDPLUSR                  = 0x30007,
    HAL_ICON_DISC_DVDPLUSRW                 = 0x30008,
    HAL_ICON_DISC_DVDPLUSR_DL               = 0x30009
} HalIcon;

typedef struct {
    HalIcon icon;
    const char *icon_path;
} HalIconPair;

static const char gnome_dev_removable[] = "gnome-dev-removable";
static const char gnome_dev_removable_usb[] = "gnome-dev-removable-usb";
static const char gnome_dev_removable_1394[] = "gnome-dev-removable-1394";
static const char gnome_dev_harddisk[] = "gnome-dev-harddisk";
static const char gnome_dev_harddisk_usb[] = "gnome-dev-harddisk-usb";
static const char gnome_dev_harddisk_1394[] = "gnome-dev-harddisk-1394";

/* by design, the enums are laid out so we can do easy computations */
static const HalIconPair hal_icon_mapping[] = {
    {HAL_ICON_DRIVE_REMOVABLE_DISK,           gnome_dev_removable},
    {HAL_ICON_DRIVE_REMOVABLE_DISK_IDE,       gnome_dev_removable},
    {HAL_ICON_DRIVE_REMOVABLE_DISK_SCSI,      gnome_dev_removable},
    {HAL_ICON_DRIVE_REMOVABLE_DISK_USB,       gnome_dev_removable_usb},
    {HAL_ICON_DRIVE_REMOVABLE_DISK_IEEE1394,  gnome_dev_removable_1394},
    {HAL_ICON_DRIVE_REMOVABLE_DISK_CCW,       gnome_dev_removable},
    {HAL_ICON_DRIVE_DISK,                     gnome_dev_removable},
    {HAL_ICON_DRIVE_DISK_IDE,                 gnome_dev_removable},
    {HAL_ICON_DRIVE_DISK_SCSI,                gnome_dev_removable},       /* TODO: gnome-dev-removable-scsi */
    {HAL_ICON_DRIVE_DISK_USB,                 gnome_dev_removable_usb},
    {HAL_ICON_DRIVE_DISK_IEEE1394,            gnome_dev_removable_1394},
    {HAL_ICON_DRIVE_DISK_CCW,                 gnome_dev_removable},
    {HAL_ICON_DRIVE_CDROM,                    gnome_dev_removable},       /* TODO: gnome-dev-removable-cdrom */
    {HAL_ICON_DRIVE_CDWRITER,                 gnome_dev_removable},       /* TODO: gnome-dev-removable-cdwriter */
    {HAL_ICON_DRIVE_FLOPPY,                   gnome_dev_removable},       /* TODO: gnome-dev-removable-floppy */
    {HAL_ICON_DRIVE_TAPE,                     gnome_dev_removable},       /* TODO: gnome-dev-removable-tape */
    {HAL_ICON_DRIVE_COMPACT_FLASH,            gnome_dev_removable},       /* TODO: gnome-dev-removable-cf */
    {HAL_ICON_DRIVE_MEMORY_STICK,             gnome_dev_removable},       /* TODO: gnome-dev-removable-ms */
    {HAL_ICON_DRIVE_SMART_MEDIA,              gnome_dev_removable},       /* TODO: gnome-dev-removable-sm */
    {HAL_ICON_DRIVE_SD_MMC,                   gnome_dev_removable},       /* TODO: gnome-dev-removable-sdmmc */
    {HAL_ICON_DRIVE_CAMERA,                   gnome_dev_removable},       /* TODO: gnome-dev-removable-camera */
    {HAL_ICON_DRIVE_PORTABLE_AUDIO_PLAYER,    gnome_dev_removable},       /* TODO: gnome-dev-removable-ipod */
    {HAL_ICON_DRIVE_ZIP,                      gnome_dev_removable},       /* TODO: gnome-dev-removable-zip */
    {HAL_ICON_DRIVE_JAZ,                      gnome_dev_removable},       /* TODO: gnome-dev-removable-jaz */
    {HAL_ICON_DRIVE_FLASH_KEY,                gnome_dev_removable},       /* TODO: gnome-dev-removable-pendrive */

    {HAL_ICON_VOLUME_REMOVABLE_DISK,          gnome_dev_harddisk},
    {HAL_ICON_VOLUME_REMOVABLE_DISK_IDE,      gnome_dev_harddisk},
    {HAL_ICON_VOLUME_REMOVABLE_DISK_SCSI,     gnome_dev_harddisk},        /* TODO: gnome-dev-harddisk-scsi */
    {HAL_ICON_VOLUME_REMOVABLE_DISK_USB,      gnome_dev_harddisk_usb},
    {HAL_ICON_VOLUME_REMOVABLE_DISK_IEEE1394, gnome_dev_harddisk_1394},
    {HAL_ICON_VOLUME_REMOVABLE_DISK_CCW,      gnome_dev_harddisk},
    {HAL_ICON_VOLUME_DISK,                    gnome_dev_harddisk},
    {HAL_ICON_VOLUME_DISK_IDE,                gnome_dev_harddisk},
    {HAL_ICON_VOLUME_DISK_SCSI,               gnome_dev_harddisk},
    {HAL_ICON_VOLUME_DISK_USB,                gnome_dev_harddisk_usb},
    {HAL_ICON_VOLUME_DISK_IEEE1394,           gnome_dev_harddisk_1394},
    {HAL_ICON_VOLUME_DISK_CCW,                gnome_dev_harddisk},
    {HAL_ICON_VOLUME_FLOPPY,                  "gnome-dev-floppy"},
    {HAL_ICON_VOLUME_TAPE,                    gnome_dev_harddisk},
    {HAL_ICON_VOLUME_COMPACT_FLASH,           "gnome-dev-media-cf"},
    {HAL_ICON_VOLUME_MEMORY_STICK,            "gnome-dev-media-ms"},
    {HAL_ICON_VOLUME_SMART_MEDIA,             "gnome-dev-media-sm"},
    {HAL_ICON_VOLUME_SD_MMC,                  "gnome-dev-media-sdmmc"},
    {HAL_ICON_VOLUME_CAMERA,                  "camera"},
    {HAL_ICON_VOLUME_PORTABLE_AUDIO_PLAYER,   "gnome-dev-ipod"},
    {HAL_ICON_VOLUME_ZIP,                     "gnome-dev-zipdisk"},
    {HAL_ICON_VOLUME_JAZ,                     "gnome-dev-jazdisk"},
    {HAL_ICON_VOLUME_FLASH_KEY,               gnome_dev_harddisk},        /* TODO: gnome-dev-pendrive */

    {HAL_ICON_DISC_CDROM,                     "gnome-dev-cdrom"},
    {HAL_ICON_DISC_CDR,                       "gnome-dev-disc-cdr"},
    {HAL_ICON_DISC_CDRW,                      "gnome-dev-disc-cdrw"},
    {HAL_ICON_DISC_DVDROM,                    "gnome-dev-disc-dvdrom"},
    {HAL_ICON_DISC_DVDRAM,                    "gnome-dev-disc-dvdram"},
    {HAL_ICON_DISC_DVDR,                      "gnome-dev-disc-dvdr"},
    {HAL_ICON_DISC_DVDRW,                     "gnome-dev-disc-dvdrw"},
    {HAL_ICON_DISC_DVDPLUSR,                  "gnome-dev-disc-dvdr-plus"},
    {HAL_ICON_DISC_DVDPLUSRW,                 "gnome-dev-disc-dvdrw"},      /* TODO: gnome-dev-disc-dvdrw-plus */
    {HAL_ICON_DISC_DVDPLUSR_DL,               "gnome-dev-disc-dvdr-plus"},  /* TODO: gnome-dev-disc-dvdr-plus-dl */

    {0x00, NULL}
};

const char ERR_BUSY[] = "org.freedesktop.Hal.Device.Volume.Busy";
const char ERR_PERM_DENIED[] = "org.freedesktop.Hal.Device.Volume.PermissionDenied";
const char ERR_UNKNOWN_FAILURE[] = "org.freedesktop.Hal.Device.Volume.UnknownFailure";
const char ERR_INVALID_MOUNT_OPT[] = "org.freedesktop.Hal.Device.Volume.InvalidMountOption";
const char ERR_UNKNOWN_FS[] = "org.freedesktop.Hal.Device.Volume.UnknownFilesystemType";
const char ERR_NOT_MOUNTED[] = "org.freedesktop.Hal.Device.Volume.NotMounted";
const char ERR_NOT_MOUNTED_BY_HAL[] = "org.freedesktop.Hal.Device.Volume.NotMountedByHal";
const char ERR_ALREADY_MOUNTED[] = "org.freedesktop.Hal.Device.Volume.AlreadyMounted";
const char ERR_INVALID_MOUNT_POINT[] = "org.freedesktop.Hal.Device.Volume.InvalidMountpoint";
const char ERR_MOUNT_POINT_NOT_AVAILABLE[] = "org.freedesktop.Hal.Device.Volume.MountPointNotAvailable";

struct _VFSVolume
{
    char* udi;
    char* disp_name;
    const char* icon;
    char* mount_point;
    char* device_file;
    gboolean is_mounted : 1;
    /* gboolean is_hotpluggable : 1; */
    gboolean is_removable : 1;
    gboolean is_mountable : 1;
    gboolean requires_eject : 1;
    gboolean is_user_visible : 1;
};

typedef struct _VFSVolumeCallbackData
{
    VFSVolumeCallback cb;
    gpointer user_data;
}VFSVolumeCallbackData;

/*------------------------------------------------------------------------*/

/* BEGIN: Added by Hong Jen Yee on 2008-02-02 */
static LibHalContext* hal_context = NULL;
static DBusConnection * dbus_connection = NULL;

static GList* volumes = NULL;
static GArray* callbacks = NULL;

static VFSVolume *vfs_volume_get_volume_by_udi (const gchar *udi);
static void                vfs_volume_update_volume_by_udi ( const gchar *udi);
static void                vfs_volume_device_added             (LibHalContext                  *context,
                                                                                   const gchar                    *udi);
static void                vfs_volume_device_removed           (LibHalContext                  *context,
                                                                                   const gchar                    *udi);
static void                vfs_volume_device_new_capability    (LibHalContext                  *context,
                                                                                   const gchar                    *udi,
                                                                                   const gchar                    *capability);
static void                vfs_volume_device_lost_capability   (LibHalContext                  *context,
                                                                                   const gchar                    *udi,
                                                                                   const gchar                    *capability);
static void                vfs_volume_device_property_modified (LibHalContext                  *context,
                                                                                   const gchar                    *udi,
                                                                                   const gchar                    *key,
                                                                                   dbus_bool_t                     is_removed,
                                                                                   dbus_bool_t                     is_added);
static void                vfs_volume_device_condition         (LibHalContext                  *context,
                                                                                   const gchar                    *udi,
                                                                                   const gchar                    *name,
                                                                                   const gchar                    *details);


static gboolean vfs_volume_mount_by_udi_as_root( const char* udi );
static gboolean vfs_volume_umount_by_udi_as_root( const char* udi );
static gboolean vfs_volume_eject_by_udi_as_root( const char* udi );

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
    if ( evt_device->s || evt_device->ob2_data )
        main_window_event( NULL, NULL, "evt_device", 0, 0, vol->device_file, 0,
                                                        0, state, FALSE );
}
/* END: Added by Hong Jen Yee on 2008-02-02 */

static const char *
_hal_lookup_icon (HalIcon icon)
{
    int i;
    const char *result;

    result = NULL;

    /* TODO: could make lookup better than O(n) */
    for (i = 0; hal_icon_mapping[i].icon_path != NULL; i++) {
        if (hal_icon_mapping[i].icon == icon) {
            result = hal_icon_mapping[i].icon_path;
            break;
        }
    }

    return result;
}

/* hal_volume may be NULL */
static char *
_hal_drive_policy_get_icon (LibHalDrive *hal_drive, LibHalVolume *hal_volume)
{
    const char *name;
    LibHalDriveBus bus;
    LibHalDriveType drive_type;

    name = libhal_drive_get_dedicated_icon_drive (hal_drive);
    if (name != NULL)
        goto out;

    bus        = libhal_drive_get_bus (hal_drive);
    drive_type = libhal_drive_get_type (hal_drive);

    /* by design, the enums are laid out so we can do easy computations */

    switch (drive_type) {
    case LIBHAL_DRIVE_TYPE_REMOVABLE_DISK:
    case LIBHAL_DRIVE_TYPE_DISK:
        name = _hal_lookup_icon (0x10000 + drive_type*0x100 + bus);
        break;

    case LIBHAL_DRIVE_TYPE_CDROM:
    {
        LibHalDriveCdromCaps cdrom_caps;
        gboolean cdrom_can_burn;

        /* can burn if other flags than cdrom and dvdrom */
        cdrom_caps = libhal_drive_get_cdrom_caps (hal_drive);
        cdrom_can_burn = ((cdrom_caps & (LIBHAL_DRIVE_CDROM_CAPS_CDROM|
                         LIBHAL_DRIVE_CDROM_CAPS_DVDROM)) == cdrom_caps);

        name = _hal_lookup_icon (0x10000 + drive_type*0x100 + (cdrom_can_burn ? 0xff : 0x00));
        break;
    }

    default:
        name = _hal_lookup_icon (0x10000 + drive_type*0x100);
    }

out:
    if (name != NULL)
        return g_strdup (name);
    else {
        g_warning ("_hal_drive_policy_get_icon : error looking up icon; defaulting to gnome-dev-removable");
        return g_strdup (gnome_dev_removable);
    }
}

static char *
_hal_volume_policy_get_icon (LibHalDrive *hal_drive, LibHalVolume *hal_volume)
{
    const char *name;
    LibHalDriveBus bus;
    LibHalDriveType drive_type;
    LibHalVolumeDiscType disc_type;

    name = libhal_drive_get_dedicated_icon_volume (hal_drive);
    if (name != NULL)
        goto out;

    /* by design, the enums are laid out so we can do easy computations */

    if (libhal_volume_is_disc (hal_volume)) {
        disc_type = libhal_volume_get_disc_type (hal_volume);
        name = _hal_lookup_icon (0x30000 + disc_type);
        goto out;
    }

    if (hal_drive == NULL) {
        name = _hal_lookup_icon (HAL_ICON_VOLUME_REMOVABLE_DISK);
        goto out;
    }

    bus        = libhal_drive_get_bus (hal_drive);
    drive_type = libhal_drive_get_type (hal_drive);

    switch (drive_type) {
    case LIBHAL_DRIVE_TYPE_REMOVABLE_DISK:
    case LIBHAL_DRIVE_TYPE_DISK:
        name = _hal_lookup_icon (0x20000 + drive_type*0x100 + bus);
        break;

    default:
        name = _hal_lookup_icon (0x20000 + drive_type*0x100);
    }
out:
    if (name != NULL)
        return g_strdup (name);
    else {
        g_warning ("_hal_volume_policy_get_icon : error looking up icon; defaulting to gnome-dev-harddisk");
        return g_strdup (gnome_dev_harddisk);
    }
}

/*------------------------------------------------------------------------*/
/* hal_volume may be NULL */
static char *
_hal_drive_policy_get_display_name (LibHalDrive *hal_drive, LibHalVolume *hal_volume)
{
    const char *model;
    const char *vendor;
    LibHalDriveType drive_type;
    char *name;
    char *vm_name;
    gboolean may_prepend_external;

    name = NULL;
    may_prepend_external = FALSE;

    drive_type = libhal_drive_get_type (hal_drive);

    /* Handle disks without removable media */
    if ((drive_type == LIBHAL_DRIVE_TYPE_DISK) &&
        !libhal_drive_uses_removable_media (hal_drive) &&
        hal_volume != NULL) {
        const char *label;
        char size_str[64];

        /* use label if available */
        label = libhal_volume_get_label (hal_volume);
        if (label != NULL && strlen (label) > 0) {
            name = g_strdup (label);
            goto out;
        }

        /* Otherwise, just use volume size */
        vfs_file_size_to_string (size_str, libhal_volume_get_size (hal_volume));
        name = g_strdup_printf (_("%s Volume"), size_str);

        goto out;
    }

    /* removable media and special drives */

    /* drives we know the type of */
    if (drive_type == LIBHAL_DRIVE_TYPE_CDROM) {
        const char *first;
        const char *second;
        LibHalDriveCdromCaps drive_cdrom_caps;

        drive_cdrom_caps = libhal_drive_get_cdrom_caps (hal_drive);

        first = _("CD-ROM");
        if (drive_cdrom_caps & LIBHAL_DRIVE_CDROM_CAPS_CDR)
            first = _("CD-R");
        if (drive_cdrom_caps & LIBHAL_DRIVE_CDROM_CAPS_CDRW)
            first = _("CD-RW");

        second = NULL;
        if (drive_cdrom_caps & LIBHAL_DRIVE_CDROM_CAPS_DVDROM)
            second = _("DVD-ROM");
        if (drive_cdrom_caps & LIBHAL_DRIVE_CDROM_CAPS_DVDPLUSR)
            second = _("DVD+R");
        if (drive_cdrom_caps & LIBHAL_DRIVE_CDROM_CAPS_DVDPLUSRW)
            second = _("DVD+RW");
        if (drive_cdrom_caps & LIBHAL_DRIVE_CDROM_CAPS_DVDR)
            second = _("DVD-R");
        if (drive_cdrom_caps & LIBHAL_DRIVE_CDROM_CAPS_DVDRW)
            second = _("DVD-RW");
        if (drive_cdrom_caps & LIBHAL_DRIVE_CDROM_CAPS_DVDRAM)
            second = _("DVD-RAM");
        if ((drive_cdrom_caps & LIBHAL_DRIVE_CDROM_CAPS_DVDR) &&
            (drive_cdrom_caps & LIBHAL_DRIVE_CDROM_CAPS_DVDPLUSR))
            second = ("DVD±R");
        if ((drive_cdrom_caps & LIBHAL_DRIVE_CDROM_CAPS_DVDRW) &&
            (drive_cdrom_caps & LIBHAL_DRIVE_CDROM_CAPS_DVDPLUSRW))
            second = ("DVD±RW");

        if (second != NULL) {
            name = g_strdup_printf (_("%s/%s Drive"), first, second);
        } else {
            name = g_strdup_printf (_("%s Drive"), first);
        }

        may_prepend_external = TRUE;
    } else if (drive_type == LIBHAL_DRIVE_TYPE_FLOPPY) {
        name = g_strdup (_("Floppy Drive"));
        may_prepend_external = TRUE;
    } else if (drive_type == LIBHAL_DRIVE_TYPE_COMPACT_FLASH) {
        name = g_strdup (_("Compact Flash Drive"));
    } else if (drive_type == LIBHAL_DRIVE_TYPE_MEMORY_STICK) {
        name = g_strdup (_("Memory Stick Drive"));
    } else if (drive_type == LIBHAL_DRIVE_TYPE_SMART_MEDIA) {
        name = g_strdup (_("Smart Media Drive"));
    } else if (drive_type == LIBHAL_DRIVE_TYPE_SD_MMC) {
        name = g_strdup (_("SD/MMC Drive"));
    } else if (drive_type == LIBHAL_DRIVE_TYPE_ZIP) {
        name = g_strdup (_("Zip Drive"));
        may_prepend_external = TRUE;
    } else if (drive_type == LIBHAL_DRIVE_TYPE_JAZ) {
        name = g_strdup (_("Jaz Drive"));
        may_prepend_external = TRUE;
    } else if (drive_type == LIBHAL_DRIVE_TYPE_FLASHKEY) {
        name = g_strdup (_("Pen Drive"));
    } else if (drive_type == LIBHAL_DRIVE_TYPE_PORTABLE_AUDIO_PLAYER) {
        const char *vendor;
        const char *model;
        vendor = libhal_drive_get_vendor (hal_drive);
        model = libhal_drive_get_model (hal_drive);
        name = g_strdup_printf (_("%s %s Music Player"),
                    vendor != NULL ? vendor : "",
                    model != NULL ? model : "");
    } else if (drive_type == LIBHAL_DRIVE_TYPE_CAMERA) {
        const char *vendor;
        const char *model;
        vendor = libhal_drive_get_vendor (hal_drive);
        model = libhal_drive_get_model (hal_drive);
        name = g_strdup_printf (_("%s %s Digital Camera"),
                    vendor != NULL ? vendor : "",
                    model != NULL ? model : "");
    }

    if (name != NULL)
        goto out;

    /* model and vendor at last resort */
    model = libhal_drive_get_model (hal_drive);
    vendor = libhal_drive_get_vendor (hal_drive);

    vm_name = NULL;
    if (vendor == NULL || strlen (vendor) == 0) {
        if (model != NULL && strlen (model) > 0)
            vm_name = g_strdup (model);
    } else {
        if (model == NULL || strlen (model) == 0)
            vm_name = g_strdup (vendor);
        else {
            vm_name = g_strdup_printf ("%s %s", vendor, model);
        }
    }

    if (vm_name != NULL) {
        name = vm_name;
    }

out:
    /* lame fallback */
    if (name == NULL)
        name = g_strdup (_("Drive"));

    if (may_prepend_external) {
        if (libhal_drive_is_hotpluggable (hal_drive)) {
            char *tmp = name;
            name = g_strdup_printf (_("External %s"), name);
            g_free (tmp);
        }
    }

    return name;
}

static char *
_hal_volume_policy_get_display_name (LibHalDrive *hal_drive, LibHalVolume *hal_volume)
{
    LibHalDriveType drive_type;
    const char *volume_label;
    char *name;
    char size_str[64];

    name = NULL;

    drive_type = libhal_drive_get_type (hal_drive);
    volume_label = libhal_volume_get_label (hal_volume);

    /* Use volume label if available */
    if (volume_label != NULL) {
        name = g_strdup (volume_label);
        goto out;
    }
    /* Handle media in optical drives */
    if (drive_type == LIBHAL_DRIVE_TYPE_CDROM) {
        switch (libhal_volume_get_disc_type (hal_volume)) {

        default:
            /* explict fallthrough */

        case LIBHAL_VOLUME_DISC_TYPE_CDROM:
            name = g_strdup (_("CD-ROM Disc"));
            break;

        case LIBHAL_VOLUME_DISC_TYPE_CDR:
            if (libhal_volume_disc_is_blank (hal_volume))
                name = g_strdup (_("Blank CD-R Disc"));
            else
                name = g_strdup (_("CD-R Disc"));
            break;

        case LIBHAL_VOLUME_DISC_TYPE_CDRW:
            if (libhal_volume_disc_is_blank (hal_volume))
                name = g_strdup (_("Blank CD-RW Disc"));
            else
                name = g_strdup (_("CD-RW Disc"));
            break;

        case LIBHAL_VOLUME_DISC_TYPE_DVDROM:
            name = g_strdup (_("DVD-ROM Disc"));
            break;

        case LIBHAL_VOLUME_DISC_TYPE_DVDRAM:
            if (libhal_volume_disc_is_blank (hal_volume))
                name = g_strdup (_("Blank DVD-RAM Disc"));
            else
                name = g_strdup (_("DVD-RAM Disc"));
            break;

        case LIBHAL_VOLUME_DISC_TYPE_DVDR:
            if (libhal_volume_disc_is_blank (hal_volume))
                name = g_strdup (_("Blank DVD-R Disc"));
            else
                name = g_strdup (_("DVD-R Disc"));
            break;

        case LIBHAL_VOLUME_DISC_TYPE_DVDRW:
            if (libhal_volume_disc_is_blank (hal_volume))
                name = g_strdup (_("Blank DVD-RW Disc"));
            else
                name = g_strdup (_("DVD-RW Disc"));

            break;

        case LIBHAL_VOLUME_DISC_TYPE_DVDPLUSR:
            if (libhal_volume_disc_is_blank (hal_volume))
                name = g_strdup (_("Blank DVD+R Disc"));
            else
                name = g_strdup (_("DVD+R Disc"));
            break;

        case LIBHAL_VOLUME_DISC_TYPE_DVDPLUSRW:
            if (libhal_volume_disc_is_blank (hal_volume))
                name = g_strdup (_("Blank DVD+RW Disc"));
            else
                name = g_strdup (_("DVD+RW Disc"));
            break;
        }

        /* Special case for pure audio disc */
        if (libhal_volume_disc_has_audio (hal_volume) && !libhal_volume_disc_has_data (hal_volume)) {
            free (name);
            name = g_strdup (_("Audio Disc"));
        }

        goto out;
    }

    /* Fallback: size of media */

    vfs_file_size_to_string (size_str, libhal_volume_get_size (hal_volume));
    if (libhal_drive_uses_removable_media (hal_drive)) {
        name = g_strdup_printf (_("%s Removable Volume"), size_str);
    } else {
        name = g_strdup_printf (_("%s Volume"), size_str);
    }

out:
    /* lame fallback */
    if (name == NULL)
        name = g_strdup (_("Volume"));

    return name;
}

/*------------------------------------------------------------------------*/
#if 0
static gboolean
_hal_volume_temp_udi (LibHalDrive *hal_drive, LibHalVolume *hal_volume)
{
        const char *volume_udi;
        gboolean ret;

        ret = FALSE;

        volume_udi = libhal_volume_get_udi (hal_volume);

        if (strncmp (volume_udi, "/org/freedesktop/Hal/devices/temp",
                     strlen ("/org/freedesktop/Hal/devices/temp")) == 0)
                ret = TRUE;

        return ret;
}

static gboolean
_hal_volume_policy_show_on_desktop (LibHalDrive *hal_drive, LibHalVolume *hal_volume)
{
    gboolean ret;

    ret = TRUE;

    /* Right now we show everything on the desktop as there is no setting
     * for this.. potentially we could hide fixed drives..
     */

    return ret;
}

#endif
/*------------------------------------------------------------------------*/

static VFSVolume* vfs_volume_new( const char* udi )
{
    VFSVolume * volume;
    volume = g_slice_new0( VFSVolume );
    volume->udi = g_strdup( udi );
    return volume;
}

void vfs_volume_free( VFSVolume* volume )
{
    g_free( volume->udi );
    g_free( volume->disp_name );
    g_free( volume->mount_point );
    g_free( volume->device_file );
    g_slice_free( VFSVolume, volume );
}

static void
vfs_volume_update (VFSVolume *volume,
                              LibHalContext      *context,
                              LibHalVolume       *hv,
                              LibHalDrive        *hd)
{
  gchar *desired_mount_point;
  gchar *mount_root;
  gchar *basename;
  gchar *filename;

  /* reset the volume status */
  // volume->status = 0;

  /* determine the new device file */
  g_free (volume->device_file);
  volume->device_file = g_strdup ((hv != NULL) ? libhal_volume_get_device_file (hv) : libhal_drive_get_device_file (hd));

  /* release the previous mount point (if any) */
  if (G_LIKELY (volume->mount_point != NULL))
    {
      g_free(volume->mount_point);
      volume->mount_point = NULL;
    }

    if (libhal_drive_uses_removable_media (hd)
        || libhal_drive_is_hotpluggable (hd))
    {
      volume->is_removable = TRUE;
    }
    else
        volume->is_removable = FALSE;

#if 0
  /* determine the type of the volume */
  switch (libhal_drive_get_type (hd))
    {
    case LIBHAL_DRIVE_TYPE_CDROM:
      /* check if we have a pure audio CD without any data track */
      if (libhal_volume_disc_has_audio (hv) && !libhal_volume_disc_has_data (hv))
        {
          /* special treatment for pure audio CDs */
          volume->kind = VFS_VOLUME_KIND_AUDIO_CD;
        }
      else
        {
          /* check which kind of CD-ROM/DVD we have */
          switch (libhal_volume_get_disc_type (hv))
            {
            case LIBHAL_VOLUME_DISC_TYPE_CDROM:
              volume->kind = VFS_VOLUME_KIND_CDROM;
              break;

            case LIBHAL_VOLUME_DISC_TYPE_CDR:
              volume->kind = VFS_VOLUME_KIND_CDR;
              break;

            case LIBHAL_VOLUME_DISC_TYPE_CDRW:
              volume->kind = VFS_VOLUME_KIND_CDRW;
              break;

            case LIBHAL_VOLUME_DISC_TYPE_DVDROM:
              volume->kind = VFS_VOLUME_KIND_DVDROM;
              break;

            case LIBHAL_VOLUME_DISC_TYPE_DVDRAM:
              volume->kind = VFS_VOLUME_KIND_DVDRAM;
              break;

            case LIBHAL_VOLUME_DISC_TYPE_DVDR:
              volume->kind = VFS_VOLUME_KIND_DVDR;
              break;

            case LIBHAL_VOLUME_DISC_TYPE_DVDRW:
              volume->kind = VFS_VOLUME_KIND_DVDRW;
              break;

            case LIBHAL_VOLUME_DISC_TYPE_DVDPLUSR:
              volume->kind = VFS_VOLUME_KIND_DVDPLUSR;
              break;

            case LIBHAL_VOLUME_DISC_TYPE_DVDPLUSRW:
              volume->kind = VFS_VOLUME_KIND_DVDPLUSRW;
              break;

            default:
              /* unsupported disc type */
              volume->kind = VFS_VOLUME_KIND_UNKNOWN;
              break;
            }
        }
      break;

    case LIBHAL_DRIVE_TYPE_FLOPPY:
      volume->kind = VFS_VOLUME_KIND_FLOPPY;
      break;

    case LIBHAL_DRIVE_TYPE_PORTABLE_AUDIO_PLAYER:
      volume->kind = VFS_VOLUME_KIND_AUDIO_PLAYER;
      break;

    case LIBHAL_DRIVE_TYPE_SMART_MEDIA:
    case LIBHAL_DRIVE_TYPE_SD_MMC:
      volume->kind = VFS_VOLUME_KIND_MEMORY_CARD;
      break;

    default:
      /* check if the drive is connected to the USB bus */
      if (libhal_drive_get_bus (hd) == LIBHAL_DRIVE_BUS_USB)
        {
          /* we consider the drive to be an USB stick */
          volume->kind = VFS_VOLUME_KIND_USBSTICK;
        }
      else if (libhal_drive_uses_removable_media (hd)
            || libhal_drive_is_hotpluggable (hd))
        {
          /* fallback to generic removable disk */
          volume->kind = VFS_VOLUME_KIND_REMOVABLE_DISK;
        }
      else
        {
          /* fallback to harddisk drive */
          volume->kind = VFS_VOLUME_KIND_HARDDISK;
        }
      break;
    }
#endif

  /* either we have a volume, which means we have media, or
   * a drive, which means non-pollable then, so it's present
   */
  // volume->status |= VFS_VOLUME_STATUS_PRESENT;

  /* figure out if the volume is mountable */
  if(hv != NULL && libhal_volume_get_fsusage (hv) == LIBHAL_VOLUME_USAGE_MOUNTABLE_FILESYSTEM)
    volume->is_mountable = TRUE;

  /* check if the drive requires eject */
  volume->requires_eject = libhal_drive_requires_eject (hd);

  /* check if the volume is currently mounted */
  if (hv != NULL && libhal_volume_is_mounted (hv))
    {
      /* try to determine the new mount point */
      volume->mount_point = g_strdup(libhal_volume_get_mount_point (hv));

      /* we only mark the volume as mounted if we have a valid mount point */
      if (G_LIKELY (volume->mount_point != NULL))
        volume->is_mounted = TRUE;
    }
  else
    {
      /* we don't trust HAL, so let's see what the kernel says about the volume */
       volume->mount_point = NULL;
      // volume->mount_point = vfs_volume_find_active_mount_point (volume);
        volume->is_mounted = FALSE;
      /* we must have been mounted successfully if we have a mount point */
      //if (G_LIKELY (volume->mount_point != NULL))
      //  volume->status |= VFS_VOLUME_STATUS_MOUNTED | VFS_VOLUME_STATUS_PRESENT;
    }

  /* check if we have to figure out the mount point ourself */
  if (G_UNLIKELY (volume->mount_point == NULL))
    {
      /* ask HAL for the default mount root (falling back to /media otherwise) */
      mount_root = libhal_device_get_property_string (context, "/org/freedesktop/Hal/devices/computer", "storage.policy.default.mount_root", NULL);
      if (G_UNLIKELY (mount_root == NULL || !g_path_is_absolute (mount_root)))
        {
          /* fallback to /media (seems to be sane) */
          g_free (mount_root);
          mount_root = g_strdup ("/media");
        }

      /* lets see, maybe /etc/fstab knows where to mount */
      //FIXME: volume->mount_point = vfs_volume_find_fstab_mount_point (volume);
        volume->mount_point = NULL;

      /* if we still don't have a mount point, ask HAL */
      if (G_UNLIKELY (volume->mount_point == NULL))
        {
          /* determine the desired mount point and prepend the mount root */
          desired_mount_point = libhal_device_get_property_string (context, volume->udi, "volume.policy.desired_mount_point", NULL);
          if (G_LIKELY (desired_mount_point != NULL && *desired_mount_point != '\0'))
            {
              filename = g_build_filename (mount_root, desired_mount_point, NULL);
              volume->mount_point = filename;
            }
          libhal_free_string (desired_mount_point);
        }

      /* ok, last fallback, just use <mount-root>/<device> */
      if (G_UNLIKELY (volume->mount_point == NULL))
        {
          /* <mount-root>/<device> looks like a good idea */
          basename = g_path_get_basename (volume->device_file);
          filename = g_build_filename (mount_root, basename, NULL);
          volume->mount_point = filename;
          g_free (basename);
        }

      /* release the mount root */
      g_free (mount_root);
    }

  /* if we get here, we must have a valid mount point */
  g_assert (volume->mount_point != NULL);

  /* compute a usable display name for the volume/drive */
  g_free( volume->disp_name );
  volume->disp_name =  (volume->is_mounted && hv )
    ? _hal_volume_policy_get_display_name(hd, hv)
    : _hal_drive_policy_get_display_name(hd, hv);

  if (G_UNLIKELY (volume->disp_name == NULL))
    {
      /* use the basename of the device file as label */
      volume->disp_name = g_path_get_basename (volume->device_file);
    }

  /* compute a usable list of icon names for the volume/drive */

  volume->icon = (volume->is_mounted && hv )
    ? _hal_volume_policy_get_icon( hd, hv)
    : _hal_drive_policy_get_icon( hd, hv );

  /* emit the "changed" signal */
  //vfs_volume_is_changed (THUNAR_VFS_VOLUME (volume));
  call_callbacks( volume, VFS_VOLUME_CHANGED );
}

gboolean vfs_volume_init ()
{
  LibHalDrive *hd;
  DBusError    error;
  gchar      **drive_udis;
  gchar      **udis;
  gint         n_drive_udis;
  gint         n_udis;
  gint         n, m;

  /* initialize the D-BUS error */
  dbus_error_init (&error);

  /* allocate a HAL context */
  hal_context = libhal_ctx_new ();
  if (G_UNLIKELY (hal_context == NULL))
    return  FALSE;

  /* try to connect to the system bus */
  dbus_connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
  if (G_UNLIKELY (dbus_connection == NULL))
    goto failed;

  /* setup the D-BUS connection for the HAL context */
  libhal_ctx_set_dbus_connection (hal_context, dbus_connection);

  /* connect our manager object to the HAL context */
  // libhal_ctx_set_user_data (hal_context, manager_hal);

  /* setup callbacks */
  libhal_ctx_set_device_added (hal_context, vfs_volume_device_added);
  libhal_ctx_set_device_removed (hal_context, vfs_volume_device_removed);
  libhal_ctx_set_device_new_capability (hal_context, vfs_volume_device_new_capability);
  libhal_ctx_set_device_lost_capability (hal_context, vfs_volume_device_lost_capability);
  libhal_ctx_set_device_property_modified (hal_context, vfs_volume_device_property_modified);
  libhal_ctx_set_device_condition (hal_context, vfs_volume_device_condition);

  /* try to initialize the HAL context */
  if (!libhal_ctx_init (hal_context, &error))
    goto failed;

  /* setup the D-BUS connection with the GLib main loop */
  dbus_connection_setup_with_g_main (dbus_connection, NULL);

  /* lookup all drives currently known to HAL */
  drive_udis = libhal_find_device_by_capability (hal_context, "storage", &n_drive_udis, &error);
  if (G_LIKELY (drive_udis != NULL))
    {
      /* process all drives UDIs */
      for (m = 0; m < n_drive_udis; ++m)
        {
          /* determine the LibHalDrive for the drive UDI */
          hd = libhal_drive_from_udi (hal_context, drive_udis[m]);
          if (G_UNLIKELY (hd == NULL))
            continue;

          /* check if we have a floppy disk here */
          if (libhal_drive_get_type (hd) == LIBHAL_DRIVE_TYPE_FLOPPY)
            {
              /* add the drive based on the UDI */
              vfs_volume_device_added (hal_context, drive_udis[m]);
            }
          else
            {
              /* determine all volumes for the given drive */
              udis = libhal_drive_find_all_volumes (hal_context, hd, &n_udis);
              if (G_LIKELY (udis != NULL))
                {
                  /* add volumes for all given UDIs */
                  for (n = 0; n < n_udis; ++n)
                    {
                      /* add the volume based on the UDI */
                      vfs_volume_device_added (hal_context, udis[n]);

                      /* release the UDI (HAL bug #5279) */
                      free (udis[n]);
                    }

                  /* release the UDIs array (HAL bug #5279) */
                  free (udis);
                }
            }

          /* release the hal drive */
          libhal_drive_free (hd);
        }

      /* release the drive UDIs */
      libhal_free_string_array (drive_udis);
    }

  /* watch all devices for changes */
  if (!libhal_device_property_watch_all (hal_context, &error))
    goto failed;

  return TRUE;

failed:
  /* release the HAL context */
  if (G_LIKELY (hal_context != NULL))
    {
      libhal_ctx_free (hal_context);
      hal_context = NULL;
    }

  /* print a warning message */
  if (dbus_error_is_set (&error))
    {
      g_warning (_("Failed to connect to the HAL daemon: %s"), error.message);
      dbus_error_free (&error);
    }
    return FALSE;
}

static VFSVolume*
vfs_volume_get_volume_by_udi ( const gchar *udi)
{
  GList *l;
  for (l = volumes; l != NULL; l = l->next)
    if ( ! strcmp( ((VFSVolume*)l->data)->udi, udi) )
      return (VFSVolume*)l->data;
  return NULL;
}

static void
vfs_volume_update_volume_by_udi ( const gchar *udi)
{
  VFSVolume *volume;
  LibHalVolume       *hv = NULL;
  LibHalDrive        *hd = NULL;
  const gchar        *drive_udi;

  /* check if we have a volume for the UDI */
  volume = vfs_volume_get_volume_by_udi (udi);
  if (G_UNLIKELY (volume == NULL))
    return;

  /* check if we have a volume here */
  hv = libhal_volume_from_udi (hal_context, udi);
  if (G_UNLIKELY (hv == NULL))
    {
      /* check if we have a drive here */
      hd = libhal_drive_from_udi (hal_context, udi);
      if (G_UNLIKELY (hd == NULL))
        {
          /* the device is no longer a drive or volume, so drop it */
          vfs_volume_device_removed (hal_context, udi);
          return;
        }

      /* update the drive with the new HAL drive/volume */
      vfs_volume_update (volume, hal_context, NULL, hd);

      /* release the drive */
      libhal_drive_free (hd);
    }
  else
    {
      /* determine the UDI of the drive to which this volume belongs */
      drive_udi = libhal_volume_get_storage_device_udi (hv);
      if (G_LIKELY (drive_udi != NULL))
        {
          /* determine the drive for the volume */
          hd = libhal_drive_from_udi (hal_context, drive_udi);
        }

      /* check if we have the drive for the volume */
      if (G_LIKELY (hd != NULL))
        {
          /* update the volume with the new HAL drive/volume */
          vfs_volume_update (volume, hal_context, hv, hd);

          /* release the drive */
          libhal_drive_free (hd);
        }
      else
        {
          /* unable to determine the drive, volume gone? */
          vfs_volume_device_removed (hal_context, udi);
        }

      /* release the volume */
      libhal_volume_free (hv);
    }
}

static void vfs_volume_add( VFSVolume* volume )
{
    volumes = g_list_append( volumes, volume );
    call_callbacks( volume, VFS_VOLUME_ADDED );
}

static void vfs_volume_remove( VFSVolume* volume )
{
    volumes = g_list_remove( volumes, volume );
    call_callbacks( volume, VFS_VOLUME_REMOVED );
    vfs_volume_free( volume );
}

static void
vfs_volume_device_added (LibHalContext *context,
                                            const gchar   *udi)
{
  VFSVolume        *volume;
  LibHalVolume              *hv;
  LibHalDrive               *hd;
  const gchar               *drive_udi;
  /* check if we have a volume here */
  hv = libhal_volume_from_udi (context, udi);

  /* HAL might want us to ignore this volume for some reason */
  if (G_UNLIKELY (hv != NULL && libhal_volume_should_ignore (hv)))
    {
      libhal_volume_free (hv);
      return;
    }

  /* emit the "device-added" signal (to support thunar-volman) */
  // g_signal_emit_by_name (G_OBJECT (manager_hal), "device-added", udi);

  if (G_LIKELY (hv != NULL))
    {
      /* check if we have a mountable file system here */
      if (libhal_volume_get_fsusage (hv) == LIBHAL_VOLUME_USAGE_MOUNTABLE_FILESYSTEM)
        {
          /* determine the UDI of the drive to which this volume belongs */
          drive_udi = libhal_volume_get_storage_device_udi (hv);
          if (G_LIKELY (drive_udi != NULL))
            {
              /* determine the drive for the volume */
              hd = libhal_drive_from_udi (context, drive_udi);
              if (G_LIKELY (hd != NULL))
                {
                  /* check if we already have a volume object for the UDI */
                  volume = vfs_volume_get_volume_by_udi (udi);
                  if (G_LIKELY (volume == NULL))
                    {
                      /* otherwise, we allocate a new volume object */
                      volume = vfs_volume_new( udi );
                    }

                  /* update the volume object with the new data from the HAL volume/drive */
                  vfs_volume_update (volume, context, hv, hd);

                  /* add the volume object to our list if we allocated a new one */
                  if (g_list_find (volumes, volume) == NULL)
                    {
                      /* add the volume to the volume manager */
                      vfs_volume_add (volume);
                    }
                  /* release the HAL drive */
                  libhal_drive_free (hd);
                }
            }
        }
      /* release the HAL volume */
      libhal_volume_free (hv);
    }
  else
    {
      /* but maybe we have a floppy disk drive here */
      hd = libhal_drive_from_udi (context, udi);
      if (G_UNLIKELY (hd == NULL))
        return;

      /* check if we have a floppy disk drive */
      if (G_LIKELY (libhal_drive_get_type (hd) == LIBHAL_DRIVE_TYPE_FLOPPY))
        {
          /* check if we already have a volume object for the UDI */
          volume = vfs_volume_get_volume_by_udi (udi);
          if (G_LIKELY (volume == NULL))
            {
              /* otherwise, we allocate a new volume object */
              volume = vfs_volume_new( udi );
            }

          /* update the volume object with the new data from the HAL volume/drive */
          vfs_volume_update (volume, context, NULL, hd);

          /* add the volume object to our list if we allocated a new one */
          if (g_list_find (volumes, volume) == NULL)
            {
              /* add the volume to the volume manager */
              vfs_volume_add (volume);
            }
        }
      /* release the HAL drive */
      libhal_drive_free (hd);
    }
}

static void
vfs_volume_device_removed (LibHalContext *context,
                                              const gchar   *udi)
{
  VFSVolume        *volume;
  /* emit the "device-removed" signal (to support thunar-volman) */
  //g_signal_emit_by_name (G_OBJECT (manager_hal), "device-removed", udi);

  /* check if we already have a volume object for the UDI */
  volume = vfs_volume_get_volume_by_udi (udi);
  if (G_LIKELY (volume != NULL))
    {
      /* remove the volume from the volume manager */
      vfs_volume_remove (volume);
    }
}

static void
vfs_volume_device_new_capability (LibHalContext *context,
                                                     const gchar   *udi,
                                                     const gchar   *capability)
{
  /* update the volume for the device (if any) */
  vfs_volume_update_volume_by_udi (udi);
}

static void
vfs_volume_device_lost_capability (LibHalContext *context,
                                                      const gchar   *udi,
                                                      const gchar   *capability)
{
  /* update the volume for the device (if any) */
  vfs_volume_update_volume_by_udi (udi);
}

static void
vfs_volume_device_property_modified (LibHalContext *context,
                                                        const gchar   *udi,
                                                        const gchar   *key,
                                                        dbus_bool_t    is_removed,
                                                        dbus_bool_t    is_added)
{
  /* update the volume for the device (if any) */
  vfs_volume_update_volume_by_udi (udi);
}

static void
vfs_volume_device_condition (LibHalContext *context,
                                                const gchar   *udi,
                                                const gchar   *name,
                                                const gchar   *details)
{
#if 0
  VFSVolume        *volume;
  DBusError                  derror;
  GList                     *eject_volumes = NULL;
  GList                     *lp;
  gchar                    **volume_udis;
  gint                       n_volume_udis;
  gint                       n;
#endif

// FIXME: What's this?
#if 0
  /* check if the device should be ejected */
  if (G_LIKELY (strcmp (name, "EjectPressed") == 0))
    {
      /* check if we have a volume for the device */
      volume = vfs_volume_get_volume_by_udi (udi);
      if (G_LIKELY (volume == NULL))
        {
          /* initialize D-Bus error */
          dbus_error_init (&derror);

          /* the UDI most probably identifies the drive of the volume */
          volume_udis = libhal_manager_find_device_string_match (context, "info.parent", udi, &n_volume_udis, &derror);
          if (G_LIKELY (volume_udis != NULL))
            {
              /* determine the volumes for the UDIs */
              for (n = 0; n < n_volume_udis; ++n)
                {
                  /* check if we have a mounted volume for this UDI */
                  volume = vfs_volume_get_volume_by_udi (volume_udis[n]);
                  if (vfs_volume_is_mounted (volume))
                    eject_volumes = g_list_prepend (volumes, volume));
                }
              libhal_free_string_array (volume_udis);
            }

          /* free D-Bus error */
          dbus_error_free (&derror);
        }
      else if (vfs_volume_is_mounted (volume))
        {
          eject_volumes = g_list_prepend (volumes, volume));
        }

      /* check there are any mounted volumes on the device */
      if (G_LIKELY (volumes != NULL))
        {
          /* tell everybody, that we're about to unmount those volumes */
          for (lp = volumes; lp != NULL; lp = lp->next)
            {
              vfs_volume_is_pre_unmount (lp->data);
            }
          g_list_free (volumes);

          /* emit the "device-eject" signal and let Thunar eject the device */
          // g_signal_emit_by_name (G_OBJECT (manager_hal), "device-eject", udi);
          call_callbacks( vol, VFS_VOLUME_EJECT );
        }
    }
#endif
}

gboolean vfs_volume_finalize()
{
    if ( callbacks )
        g_array_free( callbacks, TRUE );

    if ( G_LIKELY( volumes ) )
    {
        g_list_foreach( volumes, (GFunc)vfs_volume_free, NULL );
        g_list_free( volumes );
        volumes = NULL;
    }

  /* shutdown the HAL context */
  if (G_LIKELY (hal_context != NULL))
    {
      libhal_ctx_shutdown (hal_context, NULL);
      libhal_ctx_free (hal_context);
    }

  /* shutdown the D-BUS connection */
  if (G_LIKELY (dbus_connection != NULL))
    dbus_connection_unref (dbus_connection);

    return TRUE;
}

const GList* vfs_volume_get_all_volumes()
{
    return volumes;
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

const char* vfs_volume_get_disp_name( VFSVolume *vol )
{
    if( G_UNLIKELY( NULL == vol) )
        return NULL;
    return vol->disp_name;
}

const char* vfs_volume_get_mount_point( VFSVolume *vol )
{
    if( G_UNLIKELY( NULL == vol) )
        return NULL;
    return vol->mount_point;
}

const char* vfs_volume_get_device( VFSVolume *vol )
{
    if( G_UNLIKELY( NULL == vol) )
        return NULL;
    return vol->device_file;
}

const char* vfs_volume_get_fstype( VFSVolume *vol )
{
    return NULL;
}

const char* vfs_volume_get_icon( VFSVolume *vol )
{
    if( G_UNLIKELY( NULL == vol) )
        return NULL;
    return vol->icon;
}

gboolean vfs_volume_is_removable( VFSVolume *vol )
{
    if( G_UNLIKELY( NULL == vol) )
        return FALSE;
    return vol->is_removable;
}

gboolean vfs_volume_is_mounted( VFSVolume *vol )
{
    if( G_UNLIKELY( NULL == vol) )
        return FALSE;
    return vol->is_mounted;
}

gboolean vfs_volume_requires_eject( VFSVolume *vol )
{
    if( G_UNLIKELY( NULL == vol) )
        return FALSE;
    return vol->requires_eject;
}


/* Following fstab code is taken from gnome-mount and modified */

static gboolean
fstab_open (gpointer *handle)
{
#ifdef __FreeBSD__
    return setfsent () == 1;
#else
    *handle = fopen ("/etc/fstab", "r");
    return *handle != NULL;
#endif
}

static char *
fstab_next (gpointer handle, char **mount_point)
{
#ifdef __FreeBSD__
    struct fstab *fstab;

    fstab = getfsent ();

    /* TODO: fill out mount_point */
    if (mount_point != NULL && fstab != NULL) {
        *mount_point = fstab->fs_file;
    }

    return fstab ? fstab->fs_spec : NULL;
#else
    struct mntent *mnt;

    mnt = getmntent (handle);

    if (mount_point != NULL && mnt != NULL) {
        *mount_point = mnt->mnt_dir;
    }

    return mnt ? mnt->mnt_fsname : NULL;
#endif
}


static void
fstab_close (gpointer handle)
{
#ifdef __FreeBSD__
    endfsent ();
#else
    fclose (handle);
#endif
}


/* borrowed from gtk/gtkfilesystemunix.c in GTK+ on 02/23/2006 */
static void
canonicalize_filename (gchar *filename)
{
    gchar *p, *q;
    gboolean last_was_slash = FALSE;

    p = filename;
    q = filename;

    while (*p)
    {
        if (*p == G_DIR_SEPARATOR)
        {
            if (!last_was_slash)
                *q++ = G_DIR_SEPARATOR;

            last_was_slash = TRUE;
        }
        else
        {
            if (last_was_slash && *p == '.')
            {
                if (*(p + 1) == G_DIR_SEPARATOR ||
                    *(p + 1) == '\0')
                {
                    if (*(p + 1) == '\0')
                        break;

                    p += 1;
                }
                else if (*(p + 1) == '.' &&
                     (*(p + 2) == G_DIR_SEPARATOR ||
                      *(p + 2) == '\0'))
                {
                    if (q > filename + 1)
                    {
                        q--;
                        while (q > filename + 1 &&
                               *(q - 1) != G_DIR_SEPARATOR)
                            q--;
                    }

                    if (*(p + 2) == '\0')
                        break;

                    p += 2;
                }
                else
                {
                    *q++ = *p;
                    last_was_slash = FALSE;
                }
            }
            else
            {
                *q++ = *p;
                last_was_slash = FALSE;
            }
        }

        p++;
    }

    if (q > filename + 1 && *(q - 1) == G_DIR_SEPARATOR)
        q--;

    *q = '\0';
}

static char *
resolve_symlink (const char *file)
{
    GError *error;
    char *dir;
    char *link;
    char *f;
    char *f1;

    f = g_strdup (file);

    while (g_file_test (f, G_FILE_TEST_IS_SYMLINK)) {
        link = g_file_read_link (f, &error);
        if (link == NULL) {
            g_warning ("Cannot resolve symlink %s: %s", f, error->message);
            g_error_free (error);
            g_free (f);
            f = NULL;
            goto out;
        }

        dir = g_path_get_dirname (f);
        f1 = g_strdup_printf ("%s/%s", dir, link);
        g_free (dir);
        g_free (link);
        g_free (f);
        f = f1;
    }

out:
    if (f != NULL)
        canonicalize_filename (f);
    return f;
}

static LibHalVolume *
volume_findby (LibHalContext *hal_ctx, const char *property, const char *value)
{
    int i;
    char **hal_udis;
    int num_hal_udis;
    LibHalVolume *result = NULL;
    char *found_udi = NULL;
    DBusError error;

    dbus_error_init (&error);
    if ((hal_udis = libhal_manager_find_device_string_match (hal_ctx, property,
                                 value, &num_hal_udis, &error)) == NULL)
        goto out;

    for (i = 0; i < num_hal_udis; i++) {
        char *udi;
        udi = hal_udis[i];
        if (libhal_device_query_capability (hal_ctx, udi, "volume", &error)) {
            found_udi = strdup (udi);
            break;
        }
    }

    libhal_free_string_array (hal_udis);

    if (found_udi != NULL)
        result = libhal_volume_from_udi (hal_ctx, found_udi);

    free (found_udi);
out:
    return result;
}

static gboolean
is_in_fstab (const char *device_file, const char *label, const char *uuid, char **mount_point)
{
    gboolean ret;
    gpointer handle;
    char *entry;
    char *_mount_point;

    ret = FALSE;

    /* check if /etc/fstab mentions this device... (with symlinks etc) */
    if (!fstab_open (&handle)) {
        handle = NULL;
        goto out;
    }

    while ((entry = fstab_next (handle, &_mount_point)) != NULL) {
        char *resolved;

        if (label != NULL && g_str_has_prefix (entry, "LABEL=")) {
            if (strcmp (entry + 6, label) == 0) {
                gboolean skip_fstab_entry;

                skip_fstab_entry = FALSE;

                /* OK, so what's if someone attaches an external disk with the label '/' and
                 * /etc/fstab has
                 *
                 *    LABEL=/    /    ext3    defaults    1 1
                 *
                 * in /etc/fstab as most Red Hat systems do? Bugger, this is a very common use
                 * case; suppose that you take the disk from your Fedora server and attaches it
                 * to your laptop. Bingo, you now have two disks with the label '/'. One must
                 * seriously wonder if using things like LABEL=/ for / is a good idea; just
                 * what happens if you boot in this configuration? (answer: the initrd gets
                 * it wrong most of the time.. sigh)
                 *
                 * To work around this, check if the listed entry in /etc/fstab is already mounted,
                 * if it is, then check if it's the same device_file as the given one...
                 */

                /* see if a volume is mounted at this mount point  */
                if (_mount_point != NULL) {
                    LibHalVolume *mounted_vol;

                    mounted_vol = volume_findby (hal_context, "volume.mount_point", _mount_point);
                    if (mounted_vol != NULL) {
                        const char *mounted_vol_device_file;

                        mounted_vol_device_file = libhal_volume_get_device_file (mounted_vol);
                        /* no need to resolve symlinks, hal uses the canonical device file */
                        /* g_debug ("device_file = '%s'", device_file); */
                        /* g_debug ("mounted_vol_device_file = '%s'", mounted_vol_device_file); */
                        if (mounted_vol_device_file != NULL &&
                            strcmp (mounted_vol_device_file, device_file) !=0) {

                            /* g_debug ("Wanting to mount %s that has label %s, but /etc/fstab says LABEL=%s is to be mounted at mount point '%s'. However %s (that also has label %s), is already mounted at said mount point. So, skipping said /etc/fstab entry.\n",
                                   device_file, label, label, _mount_point, mounted_vol_device_file, _mount_point); */
                            skip_fstab_entry = TRUE;
                        }
                        libhal_volume_free (mounted_vol);
                    }

                }

                if (!skip_fstab_entry) {
                    ret = TRUE;
                    if (mount_point != NULL)
                        *mount_point = g_strdup (_mount_point);
                    goto out;
                }
            }
        }

        if (uuid != NULL && g_str_has_prefix (entry, "UUID=")) {
            if (strcmp (entry + 5, uuid) == 0) {
                ret = TRUE;
                if (mount_point != NULL)
                    *mount_point = g_strdup (_mount_point);
                goto out;
            }
        }

        resolved = resolve_symlink (entry);
        if (strcmp (device_file, resolved) == 0) {
            ret = TRUE;
            g_free (resolved);
            if (mount_point != NULL)
                *mount_point = g_strdup (_mount_point);
            goto out;
        }

        g_free (resolved);
    }

out:
    if (handle != NULL)
        fstab_close (handle);

    return ret;
}

#ifdef __FreeBSD__
#define MOUNT       "/sbin/mount"
#define UMOUNT      "/sbin/umount"
#else
#define MOUNT       "/bin/mount"
#define UMOUNT      "/bin/umount"
#endif

/* Following mount/umount/eject-related source code is modified from exo-mount.c
 *  included in libexo originally written by Benedikt Meurer <benny@xfce.org>.
*/

struct _ExoMountHalDevice
{
  gchar            *udi;
  LibHalDrive      *drive;
  LibHalVolume     *volume;

  /* device internals */
  gchar            *file;
  gchar            *name;

  /* file system options */
  gchar           **fsoptions;
  const gchar      *fstype;
  LibHalVolumeUsage fsusage;
};
typedef struct _ExoMountHalDevice   ExoMountHalDevice;

typedef enum{
    VA_MOUNT,
    VA_UMOUNT,
    VA_EJECT
}VolumnAction;

static gboolean translate_hal_error( GError** error, ExoMountHalDevice *device, const char* hal_err, VolumnAction action );

void
vfs_volume_hal_free (ExoMountHalDevice *device)
{
  /* check if we have a device */
  if (G_LIKELY (device != NULL))
    {
      libhal_free_string_array (device->fsoptions);
      libhal_volume_free (device->volume);
      libhal_drive_free (device->drive);
      g_free (device->file);
      g_free (device->name);
      g_free (device->udi);
      g_slice_free ( ExoMountHalDevice, device );
    }
}

static void
exo_mount_hal_propagate_error (GError   **error,
                               DBusError *derror)
{
  g_return_if_fail (error == NULL || *error == NULL);

  /* check if we need to propragate an error */
  if (G_LIKELY (derror != NULL && dbus_error_is_set (derror)))
    {
      /* propagate the error */
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s", derror->message);

      /* reset the D-Bus error */
      dbus_error_free (derror);
    }
}

ExoMountHalDevice*
vfs_volume_hal_from_udi (const gchar *udi,
                               GError     **error)
{
  ExoMountHalDevice *device = NULL;
  DBusError          derror;
  gchar            **interfaces;
  gchar            **volume_udis;
  gchar             *volume_udi = NULL;
  gint               n_volume_udis;
  gint               n;

  g_return_val_if_fail (udi != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* initialize D-Bus error */
  dbus_error_init (&derror);

again:
  /* determine the info.interfaces property of the device */
  interfaces = libhal_device_get_property_strlist (hal_context, udi, "info.interfaces", &derror);
  if (G_UNLIKELY (interfaces == NULL))
    {
      /* reset D-Bus error */
      dbus_error_free (&derror);

      /* release any previous volume UDI */
      g_free (volume_udi);
      volume_udi = NULL;

      /* ok, but maybe we have a volume whose parent is identified by the udi */
      volume_udis = libhal_manager_find_device_string_match (hal_context, "info.parent", udi, &n_volume_udis, &derror);
      if (G_UNLIKELY (volume_udis == NULL))
        {
err0:     exo_mount_hal_propagate_error (error, &derror);
          goto out;
        }
      else if (G_UNLIKELY (n_volume_udis < 1))
        {
          /* no match, we cannot handle that device */
          libhal_free_string_array (volume_udis);
          goto err1;
        }

      /* use the first volume UDI... */
      volume_udi = g_strdup (volume_udis[0]);
      libhal_free_string_array (volume_udis);

      /* ..and try again using that UDI */
      udi = (const gchar *) volume_udi;
      goto again;
    }

  /* verify that we have a mountable device here */
  for (n = 0; interfaces[n] != NULL; ++n)
    if (strcmp (interfaces[n], "org.freedesktop.Hal.Device.Volume") == 0)
      break;
  if (G_UNLIKELY (interfaces[n] == NULL))
    {
      /* definitely not a device that we're able to mount, eject or unmount */
err1: g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, _("Given device \"%s\" is not a volume or drive"), udi);
      goto out;
    }

  /* setup the device struct */
  device = g_slice_new0 (ExoMountHalDevice);
  device->udi = g_strdup (udi);

  /* check if we have a volume here */
  device->volume = libhal_volume_from_udi (hal_context, udi);
  if (G_LIKELY (device->volume != NULL))
    {
      /* determine the storage drive for the volume */
      device->drive = libhal_drive_from_udi (hal_context, libhal_volume_get_storage_device_udi (device->volume));
      if (G_LIKELY (device->drive != NULL))
        {
          /* setup the device internals */
          device->file = g_strdup (libhal_volume_get_device_file (device->volume));
          device->name = _hal_volume_policy_get_display_name( device->drive, device->volume );

          /* setup the file system internals */
          device->fstype = libhal_volume_get_fstype (device->volume);
          device->fsusage = libhal_volume_get_fsusage (device->volume);
        }
    }
  else
    {
      /* check if we have a drive here (i.e. floppy) */
      device->drive = libhal_drive_from_udi (hal_context, udi);
      if (G_LIKELY (device->drive != NULL))
        {
          /* setup the device internals */
          device->file = g_strdup (libhal_drive_get_device_file (device->drive));
          device->name = _hal_drive_policy_get_display_name( device->drive, NULL );

          /* setup the file system internals */
          device->fstype = "";
          device->fsusage = LIBHAL_VOLUME_USAGE_MOUNTABLE_FILESYSTEM;
        }
    }

    /* determine the valid mount options from the UDI */
    device->fsoptions = libhal_device_get_property_strlist (hal_context, udi, "volume.mount.valid_options", &derror);

  /* sanity checking */
  if (G_UNLIKELY (device->file == NULL || device->name == NULL))
    {
      vfs_volume_hal_free (device);
      device = NULL;
      goto err0;
    }

  /* check if we failed */
  if (G_LIKELY (device->drive == NULL))
    {
      /* definitely not a device that we're able to mount, eject or unmount */
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, _("Given device \"%s\" is not a volume or drive"), udi);
      vfs_volume_hal_free (device);
      device = NULL;
    }

out:
  /* cleanup */
  libhal_free_string_array (interfaces);
  g_free (volume_udi);

  return device;
}

ExoMountHalDevice*
vfs_volume_hal_from_file (const gchar *file,
                                GError     **error)
{
  ExoMountHalDevice *device = NULL;
  DBusError          derror;
  gchar            **interfaces;
  gchar            **udis;
  gint               n_udis;
  gint               n, m;

  g_return_val_if_fail (g_path_is_absolute (file), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* initialize D-Bus error */
  dbus_error_init (&derror);

  /* query matching UDIs from HAL */
  udis = libhal_manager_find_device_string_match (hal_context, "block.device", file, &n_udis, &derror);
  if (G_UNLIKELY (udis == NULL))
    {
      /* propagate the error */
      exo_mount_hal_propagate_error (error, &derror);
      return NULL;
    }

  /* look for an UDI that specifies the Volume interface */
  for (n = 0; n < n_udis; ++n)
    {
      /* check if we should ignore this device */
      if (libhal_device_get_property_bool (hal_context, udis[n], "info.ignore", NULL))
        continue;

      /* determine the info.interfaces property of the device */
      interfaces = libhal_device_get_property_strlist (hal_context, udis[n], "info.interfaces", NULL);
      if (G_UNLIKELY (interfaces == NULL))
        continue;

      /* check if we have a mountable device here */
      for (m = 0; interfaces[m] != NULL; ++m)
        if (strcmp (interfaces[m], "org.freedesktop.Hal.Device.Volume") == 0)
          break;

      /* check if it's a usable device */
      if (interfaces[m] != NULL)
        {
          libhal_free_string_array (interfaces);
          break;
        }

      /* next one, please */
      libhal_free_string_array (interfaces);
    }

  /* check if we have an UDI */
  if (G_LIKELY (n < n_udis))
    {
      /* try to query the device from the HAL daemon */
      device = vfs_volume_hal_from_udi (udis[n], error);
    }
  else
    {
      /* tell the caller that no matching device was found */
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Device \"%s\" not found in file system device table"), file);
    }

  /* cleanup */
  libhal_free_string_array (udis);

  return device;
}

gboolean
vfs_volume_hal_is_readonly (VFSVolume* volume)
{
    LibHalDrive* drive;
  g_return_val_if_fail (volume != NULL, FALSE);

  /* the "volume.is_mounted_read_only" property might be a good start */
  if (libhal_device_get_property_bool (hal_context, volume->udi, "volume.is_mounted_read_only", NULL))
    return TRUE;

    drive = libhal_drive_from_udi( hal_context, volume->udi );
    if( drive )
    {
      /* otherwise guess based on the drive type */
      switch (libhal_drive_get_type (drive))
        {
        /* CD-ROMs and floppies are read-only... */
        case LIBHAL_DRIVE_TYPE_CDROM:
        case LIBHAL_DRIVE_TYPE_FLOPPY:
          return TRUE;

        /* ...everything else is writable */
        default:
          return FALSE;
        }
        libhal_drive_free( drive );
    }
    return FALSE;
}

gboolean
vfs_volume_hal_eject (VFSVolume* volume,
                            GError           **error)
{
  const gchar **options = { NULL };
  const guint   n_options = 0;
  DBusMessage  *message;
  DBusMessage  *result;
  DBusError     derror;
  const gchar       *uuid = NULL, *label = NULL;
  ExoMountHalDevice* device;

  g_return_val_if_fail (volume != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    device = vfs_volume_hal_from_file ( volume->device_file, error );
    if( ! device )
        return FALSE;

    /* check if it's in /etc/fstab */
    if( device->volume != NULL) {
        label = libhal_volume_get_label (device->volume);
        uuid = libhal_volume_get_uuid (device->volume);
    }

    if( device->file != NULL) {
        char *mount_point = NULL;

        if (is_in_fstab (device->file, label, uuid, &mount_point)) {
            gboolean ret = FALSE;
            GError *err = NULL;
            char *sout = NULL;
            char *serr = NULL;
            int exit_status;
            char *args[3] = {"eject", NULL, NULL};
            char **envp = {NULL};

            //g_print (_("Device %s is in /etc/fstab with mount point \"%s\"\n"),
             //    device_file, mount_point);
            args[1] = mount_point;
            if (!g_spawn_sync ("/",
                       args,
                       envp,
                       G_SPAWN_SEARCH_PATH,
                       NULL,
                       NULL,
                       &sout,
                       &serr,
                       &exit_status,
                       &err)) {
                /* g_warning ("Cannot execute %s\n", "eject"); */
                g_free (mount_point);
                goto out;
            }

            if (exit_status != 0) {
                 /* g_warning ("%s said error %d, stdout='%s', stderr='%s'\n",
                       "eject", exit_status, sout, serr); */
                if (strstr (serr, "is busy") != NULL) {
                    translate_hal_error( error, device, ERR_BUSY, VA_EJECT);
                } else if (strstr (serr, "only root") != NULL|| strstr (serr, "unable to open") != NULL ) {
                    /* Let's try to do it as root */
                    if( vfs_volume_eject_by_udi_as_root( volume->udi ) )
                        ret = TRUE;
                    else
                        translate_hal_error( error, device, ERR_PERM_DENIED, VA_EJECT);
                } else {
                    translate_hal_error( error, device, ERR_UNKNOWN_FAILURE, VA_EJECT);
                }
                goto out;
            }
            /* g_print (_("Ejected %s (using /etc/fstab).\n"), device->file); */
            ret = TRUE;
        out:
                g_free (mount_point);
                vfs_volume_hal_free( device );
                return ret;
        }
        g_free (mount_point);
    }

  /* allocate the D-Bus message for the "Eject" method */
  message = dbus_message_new_method_call ("org.freedesktop.Hal", volume->udi, "org.freedesktop.Hal.Device.Volume", "Eject");
  if (G_UNLIKELY (message == NULL))
    {
      /* out of memory */
oom:  g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_NOMEM, g_strerror (ENOMEM));
      vfs_volume_hal_free( device );
      return FALSE;
    }

  /* append the (empty) eject options array */
  if (!dbus_message_append_args (message, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &options, n_options, DBUS_TYPE_INVALID))
    {
      dbus_message_unref (message);
      goto oom;
    }

  /* initialize D-Bus error */
  dbus_error_init (&derror);

  /* send the message to the HAL daemon and block for the reply */
  result = dbus_connection_send_with_reply_and_block (dbus_connection, message, -1, &derror);
  if (G_LIKELY (result != NULL))
    {
      /* check if an error was returned */
      if (dbus_message_get_type (result) == DBUS_MESSAGE_TYPE_ERROR)
        dbus_set_error_from_message (&derror, result);

      /* release the result */
      dbus_message_unref (result);
    }

  /* release the message */
  dbus_message_unref (message);

  /* check if we failed */
  if (G_UNLIKELY (dbus_error_is_set (&derror)))
    {
        if( 0 == strcmp( derror.name, ERR_PERM_DENIED ) ) /* permission denied */
        {
            if( vfs_volume_eject_by_udi_as_root( volume->udi ) ) /* try to eject as root */
            {
              dbus_error_free (&derror);
              vfs_volume_hal_free( device );
              return TRUE;
            }
        }
      /* try to translate the error appropriately */
      if( ! translate_hal_error( error, device, derror.name, VA_EJECT ) )
        {
          /* no precise error message, use the HAL one */
          exo_mount_hal_propagate_error (error, &derror);
        }
      /* release the DBus error */
      dbus_error_free (&derror);
      vfs_volume_hal_free( device );
      return FALSE;
    }
    vfs_volume_hal_free( device );
  return TRUE;
}

static const char* not_privileged[]={
    N_("You are not privileged to mount the volume \"%s\""),
    N_("You are not privileged to unmount the volume \"%s\""),
    N_("You are not privileged to eject the volume \"%s\"")
};

gboolean translate_hal_error( GError** error, ExoMountHalDevice *device, const char* hal_err, VolumnAction action )
{
    /* try to translate the error appropriately */
    if (strcmp (hal_err, ERR_PERM_DENIED) == 0)
    {
        /* TRANSLATORS: User tried to mount a volume, but is not privileged to do so. */
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, _(not_privileged[action]), device->name);
    } else if (strcmp (hal_err, ERR_INVALID_MOUNT_OPT) == 0) {
        /* TODO: slim down mount options to what is allowed, cf. volume.mount.valid_options */
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, _("Invalid mount option when attempting to mount the volume \"%s\""), device->name);
    } else if (strcmp (hal_err, ERR_UNKNOWN_FS) == 0) {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, _("The volume \"%s\" uses the <i>%s</i> file system which is not supported by your system"), device->name, device->fstype);
    } else if( strcmp(hal_err, ERR_BUSY) == 0 ) {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, _("An application is preventing the volume \"%s\" from being unmounted"), device->name);
    } else if( strcmp( hal_err, ERR_NOT_MOUNTED ) == 0 ) {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, _("The volume \"%s\" is not mounted"), device->name);
    } else if( strcmp (hal_err, ERR_UNKNOWN_FAILURE ) == 0 ) {
        g_set_error( error, G_FILE_ERROR, G_FILE_ERROR_FAILED, _("Error <i>%s</i>"), hal_err );
    } else if(strcmp ( hal_err, ERR_NOT_MOUNTED_BY_HAL ) == 0 ) {
          /* TRANSLATORS: HAL can only unmount volumes that were mounted via HAL. */
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, _("The volume \"%s\" was probably mounted manually on the command line"), device->name);
    } else {
      /* try to come up with a useful error message */
      if( action == VA_MOUNT ) {
          if (device->volume != NULL && libhal_volume_is_disc (device->volume) && libhal_volume_disc_is_blank (device->volume))
            {
              /* TRANSLATORS: User tried to mount blank disc, which is not going to work. */
              g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                           _("Blank discs cannot be mounted, use a CD "
                             "recording application to "
                             "record audio or data on the disc"));
            }
          else if (device->volume != NULL
              && libhal_volume_is_disc (device->volume)
              && !libhal_volume_disc_has_data (device->volume)
              && libhal_volume_disc_has_audio (device->volume))
            {
              /* TRANSLATORS: User tried to mount an Audio CD that doesn't contain a data track. */
              g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                           _("Audio CDs cannot be mounted, use "
                             "music players to play the audio tracks"));
            }
            else
                return FALSE;
        }
        else
            return FALSE;
    }
    return TRUE;
}

gboolean
vfs_volume_hal_mount (ExoMountHalDevice *device,
                            GError           **error)
{
  DBusMessage *message;
  DBusMessage *result;
  DBusError    derror;
  gchar       *mount_point = NULL;
  gchar      **options = NULL;
  gchar       *fstype = NULL;
  gchar       *s;
  const gchar       *uuid = NULL, *label = NULL;
  gint         m, n = 0;
    VFSVolumeOptions opts;

  g_return_val_if_fail (device != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    if( device->volume != NULL) {
        label = libhal_volume_get_label (device->volume);
        uuid = libhal_volume_get_uuid (device->volume);
        // device_file = libhal_volume_get_device_file (volume);
    } else if ( device->drive != NULL) {
        // device_file = libhal_drive_get_device_file (drive);
    }

    if (is_in_fstab( device->file, label, uuid, &mount_point) ) {
        GError *err = NULL;
        char *sout = NULL;
        char *serr = NULL;
        int exit_status;
        char *args[3] = {MOUNT, NULL, NULL};
        char **envp = {NULL};

        /* g_print (_("Device %s is in /etc/fstab with mount point \"%s\"\n"),
             device->file, mount_point); */
        args[1] = mount_point;
        if (!g_spawn_sync ("/",
                   args,
                   envp,
                   0,
                   NULL,
                   NULL,
                   &sout,
                   &serr,
                   &exit_status,
                   &err)) {
            g_warning ("Cannot execute %s\n", MOUNT);
            g_free (mount_point);
            mount_point = NULL;
            return FALSE;
        }

        if (exit_status != 0) {
            // char errstr[] = "mount: unknown filesystem type";
            /*
            g_warning ("%s said error %d, stdout='%s', stderr='%s'\n",
                   MOUNT, exit_status, sout, serr);
            */
            if (strstr (serr, "unknown filesystem type") != NULL) {
                translate_hal_error( error, device, ERR_UNKNOWN_FS, VA_MOUNT );
            } else if (strstr (serr, "already mounted") != NULL) {
                g_free( mount_point );
                return TRUE;
            } else if (strstr (serr, "only root") != NULL) {
                /* Let's try to do it as root */
                if( vfs_volume_mount_by_udi_as_root( device->udi ) )
                {
                    g_free( mount_point );
                    return TRUE;
                }
                else
                    translate_hal_error( error, device, ERR_PERM_DENIED, VA_MOUNT );
            } else if (strstr (serr, "bad option") != NULL) {
                 translate_hal_error( error, device, ERR_INVALID_MOUNT_OPT, VA_MOUNT);
            } else {
                 translate_hal_error( error, device, ERR_UNKNOWN_FAILURE, VA_MOUNT);
            }
            g_free (mount_point);
            return FALSE;
        }
        return TRUE;
    }
  /* determine the required mount options */

    /* get mount options set by pcmanfm first */
    if( vfs_volume_hal_get_options( device->fstype, &opts ) )
    {
        char** popts = opts.mount_options;
        n = g_strv_length( popts );
        if( n > 0)
        {
            int i;
            /* We have to allocate a new larger array bacause we might need to
             * append new options to the array later */
            options = g_new0 (gchar *, n + 4);
            for( i = 0; i < n; ++i )
            {
                options[i] = popts[i];
                popts[i] = NULL;
                /* steal the string */
            }
            /* the strings in the array are already stolen, so strfreev is not needed. */
        }
        g_free( opts.mount_options );

        fstype = opts.fstype_override;
    }

    if( G_UNLIKELY( ! options ) )
    {
      options = g_new0 (gchar *, 20);

      /* check if we know any valid mount options */
      if (G_LIKELY (device->fsoptions != NULL))
        {
          /* process all valid mount options */
          for (m = 0; device->fsoptions[m] != NULL; ++m)
            {
              /* this is currently mostly Linux specific noise */
              if (
#ifndef __FreeBSD__
                    strcmp (device->fsoptions[m], "uid=") == 0
#else
                    strcmp (ret->mount_options[i], "-u=") == 0
#endif
                  && (strcmp (device->fstype, "vfat") == 0
                   || strcmp (device->fstype, "iso9660") == 0
                   || strcmp (device->fstype, "udf") == 0
                   || device->volume == NULL))
                {
#ifndef __FreeBSD__
                  options[n++] = g_strdup_printf ("uid=%u", (guint) getuid ());
#else
                  options[n++] = g_strdup_printf ("-u=%u", (guint) getuid ());
#endif
                }
              else if (strcmp (device->fsoptions[m], "shortname=") == 0
                    && strcmp (device->fstype, "vfat") == 0)
                {
                  options[n++] = g_strdup_printf ("shortname=winnt");
                }
              else if (strcmp (device->fsoptions[m], "sync") == 0
                    && device->volume == NULL)
                {
                  /* non-pollable drive... */
                  options[n++] = g_strdup ("sync");
                }
              else if (strcmp (device->fsoptions[m], "longnames") == 0
                    && strcmp (device->fstype, "vfat") == 0)
                {
                  /* however this one is FreeBSD specific */
                  options[n++] = g_strdup ("longnames");
                }
              else if (strcmp (device->fsoptions[m], "locale=") == 0
                    && strcmp (device->fstype, "ntfs-3g") == 0)
                {
                  options[n++] = g_strdup_printf ("locale=%s", setlocale (LC_ALL, ""));
                }
            }
        }
    }

  /* try to determine a usable mount point */
  if (G_LIKELY (device->volume != NULL))
    {
      /* maybe we can use the volume's label... */
      mount_point = g_strdup( libhal_volume_get_label (device->volume) );
    }
  else
    {
      /* maybe we can use the the textual type... */
      mount_point = g_strdup( libhal_drive_get_type_textual (device->drive) );
    }

    /* However, the label may contain G_DIR_SEPARATOR so just replace these
     * with underscores. Pretty typical use-case, suppose you hotplug a disk
     * from a server, then you get e.g. two ext3 fs'es with labels '/' and
     * '/home' - typically seen on Red Hat systems...
     */
  /* make sure that the mount point is usable (i.e. does not contain G_DIR_SEPARATOR's) */
/*
  mount_point = (mount_point != NULL && *mount_point != '\0')
              ? exo_str_replace (mount_point, G_DIR_SEPARATOR_S, "_")
              : g_strdup ("");
*/
    if( mount_point )
    {
        char* pmp = mount_point;
        for ( ;*pmp; ++pmp) {
            if (*pmp == G_DIR_SEPARATOR) {
                *pmp = '_';
            }
        }
    }
    else
        mount_point = g_strdup ("");

    if( ! fstype )
    {
      /* let HAL guess the fstype */
      fstype = g_strdup ("");
    }
  /* setup the D-Bus error */
  dbus_error_init (&derror);

  /* now several times... */
  for (;;)
    {
      /* prepare the D-Bus message for the "Mount" method */
      message = dbus_message_new_method_call ("org.freedesktop.Hal", device->udi, "org.freedesktop.Hal.Device.Volume", "Mount");
      if (G_UNLIKELY (message == NULL))
        {
oom:      g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_NOMEM,
                                                    g_strerror (ENOMEM));
          g_strfreev (options);
          g_free (mount_point);
          g_free (fstype);
          return FALSE;
        }

      /* append the message parameters */
      if (!dbus_message_append_args (message,
                                     DBUS_TYPE_STRING, &mount_point,
                                     DBUS_TYPE_STRING, &fstype,
                                     DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &options, n,
                                             DBUS_TYPE_INVALID))
        {
          dbus_message_unref (message);
          goto oom;
        }

      /* send the message to the HAL daemon */
      result = dbus_connection_send_with_reply_and_block (dbus_connection, message, -1, &derror);
      if (G_LIKELY (result != NULL))
        {
          /* check if an error was returned */
          if (dbus_message_get_type (result) == DBUS_MESSAGE_TYPE_ERROR)
            dbus_set_error_from_message (&derror, result);

          /* release the result */
          dbus_message_unref (result);
        }

      /* release the messages */
      dbus_message_unref (message);

      /* check if we succeed */
      if (!dbus_error_is_set (&derror))
        break;

      /* check if the device was already mounted */
      if (strcmp (derror.name, ERR_ALREADY_MOUNTED) == 0)
        {
          dbus_error_free (&derror);
          break;
        }

      /* check if the specified mount point was invalid */
      if (strcmp (derror.name, ERR_INVALID_MOUNT_POINT) == 0 && *mount_point != '\0')
        {
          /* try again without a mount point */
          g_free (mount_point);
          mount_point = g_strdup ("");

          /* reset the error */
          dbus_error_free (&derror);
          continue;
        }

      /* check if the specified mount point is not available */
      if (strcmp (derror.name, ERR_MOUNT_POINT_NOT_AVAILABLE) == 0 && *mount_point != '\0')
        {
          /* try again with a new mount point */
          s = g_strconcat (mount_point, "_", NULL);
          g_free (mount_point);
          mount_point = s;

          /* reset the error */
          dbus_error_free (&derror);
          continue;
        }

#if defined(__FreeBSD__)
      /* check if an unknown error occurred while trying to mount a floppy */
      if (strcmp (derror.name, "org.freedesktop.Hal.Device.UnknownError") == 0
          && libhal_drive_get_type (device->drive) == LIBHAL_DRIVE_TYPE_FLOPPY)
        {
          /* check if no file system type was specified */
          if (G_LIKELY (*fstype == '\0'))
            {
              /* try again with msdosfs */
              g_free (fstype);
              fstype = g_strdup ("msdosfs");

              /* reset the error */
              dbus_error_free (&derror);
              continue;
            }
        }
#endif

      /* it's also possible that we need to include "ro" in the options */
      for (n = 0; options[n] != NULL; ++n)
        if (strcmp (options[n], "ro") == 0)
          break;
      if (G_UNLIKELY (options[n] != NULL))
        {
          /* we already included "ro" in the options, no way
           * to mount that device then... we simply give up.
           */
          break;
        }

      /* add "ro" to the options and try again */
      options[n++] = g_strdup ("ro");

      /* reset the error */
      dbus_error_free (&derror);
    }
  /* cleanup */
  g_strfreev (options);
  g_free (mount_point);
  g_free (fstype);

  /* check if we failed */
  if (dbus_error_is_set (&derror))
    {
        if (strcmp (derror.name, ERR_ALREADY_MOUNTED) == 0)
        {
          /* Ups, already mounted, we succeed! */
          dbus_error_free (&derror);
          return TRUE;
        }

        if( 0 == strcmp( derror.name, ERR_PERM_DENIED ) ) /* permission denied */
        {
            if( vfs_volume_mount_by_udi_as_root( device->udi ) ) /* try to eject as root */
            {
              dbus_error_free (&derror);
              return TRUE;
            }
        }

        if( ! translate_hal_error( error, device, derror.name, VA_MOUNT ) )
        {
          /* unknown error, use HAL's message */
          exo_mount_hal_propagate_error (error, &derror);
        }
      /* release D-Bus error */
      dbus_error_free (&derror);
      return FALSE;
    }

  return TRUE;
}

gboolean
vfs_volume_hal_unmount (VFSVolume* volume,
                              GError           **error)
{
  const gchar **options = { NULL };
  const guint   n_options = 0;
  DBusMessage  *message;
  DBusMessage  *result;
  DBusError     derror;
  const char *label = NULL, *uuid=NULL;
  ExoMountHalDevice* device = NULL;

  g_return_val_if_fail (volume != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

/* FIXME: volume->udi might be a udi to hal_drive, not hal_volume. */

    /* check if it's in /etc/fstab */
    device = vfs_volume_hal_from_file ( volume->device_file, NULL );
    if( ! device )
        return FALSE;

    /* check if it's in /etc/fstab */
    if( device->volume != NULL) {
        label = libhal_volume_get_label (device->volume);
        uuid = libhal_volume_get_uuid (device->volume);
    }
    if (device->file != NULL) {
        char *mount_point = NULL;

        if (is_in_fstab (device->file, label, uuid, &mount_point)) {
            gboolean ret = FALSE;
            GError *err = NULL;
            char *sout = NULL;
            char *serr = NULL;
            int exit_status;
            char *args[3] = {UMOUNT, NULL, NULL};
            char **envp = {NULL};

            /* g_print (_("Device %s is in /etc/fstab with mount point \"%s\"\n"),
                 device->file, mount_point); */

            args[1] = mount_point;
            if (!g_spawn_sync ("/",
                       args,
                       envp,
                       0,
                       NULL,
                       NULL,
                       &sout,
                       &serr,
                       &exit_status,
                       &err)) {
                /* g_warning ("Cannot execute %s\n", UMOUNT); */
                g_free (mount_point);
                goto out;
            }

            if (exit_status != 0) {
                /* g_warning ("%s said error %d, stdout='%s', stderr='%s'\n",
                       UMOUNT, exit_status, sout, serr); */

                if (strstr (serr, "is busy") != NULL) {
                    translate_hal_error( error, device, ERR_BUSY, VA_UMOUNT);
                } else if (strstr (serr, "not mounted") != NULL) {
                    translate_hal_error( error, device, ERR_NOT_MOUNTED, VA_UMOUNT);
                } else if (strstr (serr, "only root") != NULL) {
                    /* Let's try to do it as root */
                    if( vfs_volume_umount_by_udi_as_root( volume->udi ) )
                        ret = TRUE;
                    else
                        translate_hal_error( error, device, ERR_PERM_DENIED, VA_UMOUNT);
                } else {
                    translate_hal_error( error, device, ERR_UNKNOWN_FAILURE, VA_UMOUNT);
                }
                goto out;
            }
            /* g_print (_("Unmounted %s (using /etc/fstab)\n"), device_file); */
            ret = TRUE;
    out:
            g_free (mount_point);
            vfs_volume_hal_free( device );
            return ret;
        }
        g_free (mount_point);
    }

  /* allocate the D-Bus message for the "Unmount" method */
  message = dbus_message_new_method_call ("org.freedesktop.Hal", volume->udi, "org.freedesktop.Hal.Device.Volume", "Unmount");
  if (G_UNLIKELY (message == NULL))
    {
      /* out of memory */
oom:  g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_NOMEM,
                                                g_strerror (ENOMEM));
    vfs_volume_hal_free( device );
      return FALSE;
    }

  /* append the (empty) eject options array */
  if (!dbus_message_append_args (message, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &options, n_options, DBUS_TYPE_INVALID))
    {
      dbus_message_unref (message);
      goto oom;
    }

  /* initialize D-Bus error */
  dbus_error_init (&derror);

  /* send the message to the HAL daemon and block for the reply */
  result = dbus_connection_send_with_reply_and_block (dbus_connection, message, -1, &derror);
  if (G_LIKELY (result != NULL))
    {
      /* check if an error was returned */
      if (dbus_message_get_type (result) == DBUS_MESSAGE_TYPE_ERROR)
        dbus_set_error_from_message (&derror, result);

      /* release the result */
      dbus_message_unref (result);
    }

  /* release the message */
  dbus_message_unref (message);

  /* check if we failed */
  if (G_UNLIKELY (dbus_error_is_set (&derror)))
    {
        /* try to translate the error appropriately */
        if (strcmp (derror.name, ERR_NOT_MOUNTED) == 0)
        {
          /* Ups, volume not mounted, we succeed! */
          dbus_error_free (&derror);
          vfs_volume_hal_free( device );
          return TRUE;
        }

        if( 0 == strcmp( derror.name, ERR_PERM_DENIED ) ) /* permission denied */
        {
            if( vfs_volume_umount_by_udi_as_root( device->udi ) ) /* try to eject as root */
            {
              dbus_error_free (&derror);
              vfs_volume_hal_free( device );
              return TRUE;
            }
        }

        if( ! translate_hal_error( error, device, derror.name, VA_UMOUNT ) )
        {
          /* unknown error, use the HAL one */
          exo_mount_hal_propagate_error (error, &derror);
        }
      /* release the DBus error */
      dbus_error_free (&derror);
      vfs_volume_hal_free( device );
      return FALSE;
    }
    vfs_volume_hal_free( device );
    return TRUE;
}

gboolean vfs_volume_mount( VFSVolume* vol, GError** err )
{
    ExoMountHalDevice* device;
    gboolean ret = FALSE;
    device = vfs_volume_hal_from_udi( vol->udi, err );
    if( device )
    {
        ret = vfs_volume_hal_mount( device, err );
        vfs_volume_hal_free( device );
    }
    return ret;
}

gboolean vfs_volume_umount( VFSVolume *vol, GError** err )
{
    return vfs_volume_hal_unmount( vol, err );
}

gboolean vfs_volume_eject( VFSVolume *vol, GError** err )
{
    return vfs_volume_hal_eject( vol, err );
}

gboolean vfs_volume_mount_by_udi( const char* udi, GError** err )
{
    ExoMountHalDevice* device;
    gboolean ret = FALSE;
    device = vfs_volume_hal_from_udi( udi, err );
    if( device )
    {
        ret = vfs_volume_hal_mount( device, err );
        vfs_volume_hal_free( device );
    }
    return ret;
}

gboolean vfs_volume_umount_by_udi( const char* udi, GError** err )
{
    VFSVolume* volume = vfs_volume_get_volume_by_udi( udi );
    return volume ? vfs_volume_hal_unmount( volume, err ) : FALSE;
}

gboolean vfs_volume_eject_by_udi( const char* udi, GError** err )
{
    VFSVolume* volume = vfs_volume_get_volume_by_udi( udi );
    return volume ? vfs_volume_hal_eject( volume, err ) : FALSE;
}

/* helper functions to mount/umount/eject devices as root */
gboolean vfs_volume_mount_by_udi_as_root( const char* udi )
{
    int ret;
    char* argv[4];  //MOD

    if ( G_UNLIKELY( geteuid() == 0 ) )  /* we are already root */
        return FALSE;

    //MOD separate arguments for ktsuss compatibility
    //cmd = g_strdup_printf( "%s --mount '%s'", g_get_prgname(), udi );
    argv[0] = g_strdup( g_get_prgname() );
    argv[1] = g_strdup_printf ( "--mount" );
    argv[2] = g_strdup( udi );
    argv[3] = NULL;

    if ( !argv[0] )
        return FALSE;

    vfs_sudo_cmd_sync( NULL, argv, &ret, NULL,NULL, NULL );  //MOD
    return (ret == 0);
}

gboolean vfs_volume_umount_by_udi_as_root( const char* udi )
{
    int ret;
    char* argv[4];  //MOD

    if ( G_UNLIKELY( geteuid() == 0 ) )  /* we are already root */
        return FALSE;

    //MOD separate arguments for ktsuss compatibility
    //cmd = g_strdup_printf( "%s --umount '%s'", g_get_prgname(), udi );
    argv[0] = g_strdup( g_get_prgname() );
    argv[1] = g_strdup_printf ( "--umount" );
    argv[2] = g_strdup( udi );
    argv[3] = NULL;

    if ( !argv[0] )
        return FALSE;

    vfs_sudo_cmd_sync( NULL, argv, &ret, NULL,NULL, NULL );
    return (ret == 0);
}

gboolean vfs_volume_eject_by_udi_as_root( const char* udi )
{
    int ret;
    char* argv[4];  //MOD

    if ( G_UNLIKELY( geteuid() == 0 ) )  /* we are already root */
        return FALSE;

    //MOD separate arguments for ktsuss compatibility
    //cmd = g_strdup_printf( "%s --eject '%s'", g_get_prgname(), udi );
    argv[0] = g_strdup( g_get_prgname() );
    argv[1] = g_strdup_printf ( "--eject" );
    argv[2] = g_strdup( udi );
    argv[3] = NULL;

    if ( !argv[0] )
        return FALSE;

    vfs_sudo_cmd_sync( NULL, argv, &ret, NULL,NULL, NULL );
    return (ret == 0);
}

#endif /* HAVE_HAL */
