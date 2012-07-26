/*
*  C Implementation: vfs-volume
*
*  udev & mount monitor code by IgnorantGuru
*  device info code uses code excerpts from freedesktop's udisks v1.0.4
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

// udev
#include <libudev.h>
#include <fcntl.h>
#include <errno.h>

// waitpid
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

#include <signal.h>  // kill
 
#ifdef HAVE_STATVFS
#include <sys/statvfs.h>
#endif

#include <vfs-file-info.h>
#include "ptk-file-task.h"
#include "main-window.h"

void vfs_volume_monitor_start();
VFSVolume* vfs_volume_read_by_device( struct udev_device *udevice );
VFSVolume* vfs_volume_read_by_mount( const char* mount_points );
static void vfs_volume_device_added ( VFSVolume* volume, gboolean automount );
static void vfs_volume_device_removed ( char* device_file );
static gboolean vfs_volume_nonblock_removed( const char* mount_points );
static void call_callbacks( VFSVolume* vol, VFSVolumeState state );
void vfs_volume_special_unmounted( const char* device_file );
void unmount_if_mounted( const char* device_file );


typedef struct _VFSVolumeCallbackData
{
    VFSVolumeCallback cb;
    gpointer user_data;
}VFSVolumeCallbackData;

static GList* volumes = NULL;
static GList* special_mounts = NULL;
static GArray* callbacks = NULL;
GPid monpid = 0;
gboolean global_inhibit_auto = FALSE;

typedef struct devmount_t {
    guint major;
    guint minor;
    char* mount_points;
    char* fstype;
    GList* mounts;
} devmount_t;

GList* devmounts = NULL;
struct udev         *udev = NULL;
struct udev_monitor *umonitor = NULL;
GIOChannel* uchannel = NULL;
GIOChannel* mchannel = NULL;


/* *************************************************************************
 * device info
************************************************************************** */

typedef struct device_t  {
    struct udev_device *udevice;    
    char *devnode;
    char *native_path;
    char *major;
    char *minor;
    char *mount_points;

    gboolean device_is_system_internal;
    gboolean device_is_partition;
    gboolean device_is_partition_table;
    gboolean device_is_removable;
    gboolean device_is_media_available;
    gboolean device_is_read_only;
    gboolean device_is_drive;
    gboolean device_is_optical_disc;
    gboolean device_is_mounted;
    char *device_presentation_hide;
    char *device_presentation_nopolicy;
    char *device_presentation_name;
    char *device_presentation_icon_name;
    char *device_automount_hint;
    char *device_by_id;
    guint64 device_size;
    guint64 device_block_size;
    char *id_usage;
    char *id_type;
    char *id_version;
    char *id_uuid;
    char *id_label;
    
    char *drive_vendor;
    char *drive_model;
    char *drive_revision;
    char *drive_serial;
    char *drive_wwn;
    char *drive_connection_interface;
    guint64 drive_connection_speed;
    char *drive_media_compatibility;
    char *drive_media;
    gboolean drive_is_media_ejectable;
    gboolean drive_can_detach;

    char *partition_scheme;
    char *partition_number;
    char *partition_type;
    char *partition_label;
    char *partition_uuid;
    char *partition_flags;
    char *partition_offset;
    char *partition_size;
    char *partition_alignment_offset;

    char *partition_table_scheme;
    char *partition_table_count;

    gboolean optical_disc_is_blank;
    gboolean optical_disc_is_appendable;
    gboolean optical_disc_is_closed;
    char *optical_disc_num_tracks;
    char *optical_disc_num_audio_tracks;
    char *optical_disc_num_sessions;
} device_t;

static char *
_dupv8 (const char *s)
{
  const char *end_valid;

  if (!g_utf8_validate (s, -1, &end_valid))
    {
      g_print ("**** NOTE: The string '%s' is not valid UTF-8. Invalid characters begins at '%s'\n", s, end_valid);
      return g_strndup (s, end_valid - s);
    }
  else
    {
      return g_strdup (s);
    }
}

/* unescapes things like \x20 to " " and ensures the returned string is valid UTF-8.
 *
 * see volume_id_encode_string() in extras/volume_id/lib/volume_id.c in the
 * udev tree for the encoder
 */
static gchar *
decode_udev_encoded_string (const gchar *str)
{
  GString *s;
  gchar *ret;
  const gchar *end_valid;
  guint n;

  s = g_string_new (NULL);
  for (n = 0; str[n] != '\0'; n++)
    {
      if (str[n] == '\\')
        {
          gint val;

          if (str[n + 1] != 'x' || str[n + 2] == '\0' || str[n + 3] == '\0')
            {
              g_print ("**** NOTE: malformed encoded string '%s'\n", str);
              break;
            }

          val = (g_ascii_xdigit_value (str[n + 2]) << 4) | g_ascii_xdigit_value (str[n + 3]);

          g_string_append_c (s, val);

          n += 3;
        }
      else
        {
          g_string_append_c (s, str[n]);
        }
    }

  if (!g_utf8_validate (s->str, -1, &end_valid))
    {
      g_print ("**** NOTE: The string '%s' is not valid UTF-8. Invalid characters begins at '%s'\n", s->str, end_valid);
      ret = g_strndup (s->str, end_valid - s->str);
      g_string_free (s, TRUE);
    }
  else
    {
      ret = g_string_free (s, FALSE);
    }

  return ret;
}

static gint
ptr_str_array_compare (const gchar **a,
                       const gchar **b)
{
  return g_strcmp0 (*a, *b);
}

static double
sysfs_get_double (const char *dir,
                  const char *attribute)
{
  double result;
  char *contents;
  char *filename;

  result = 0.0;
  filename = g_build_filename (dir, attribute, NULL);
  if (g_file_get_contents (filename, &contents, NULL, NULL))
    {
      result = atof (contents);
      g_free (contents);
    }
  g_free (filename);

  return result;
}

static char *
sysfs_get_string (const char *dir,
                  const char *attribute)
{
  char *result;
  char *filename;

  result = NULL;
  filename = g_build_filename (dir, attribute, NULL);
  if (!g_file_get_contents (filename, &result, NULL, NULL))
    {
      result = g_strdup ("");
    }
  g_free (filename);

  return result;
}

static int
sysfs_get_int (const char *dir,
               const char *attribute)
{
  int result;
  char *contents;
  char *filename;

  result = 0;
  filename = g_build_filename (dir, attribute, NULL);
  if (g_file_get_contents (filename, &contents, NULL, NULL))
    {
      result = strtol (contents, NULL, 0);
      g_free (contents);
    }
  g_free (filename);

  return result;
}

static guint64
sysfs_get_uint64 (const char *dir,
                  const char *attribute)
{
  guint64 result;
  char *contents;
  char *filename;

  result = 0;
  filename = g_build_filename (dir, attribute, NULL);
  if (g_file_get_contents (filename, &contents, NULL, NULL))
    {
      result = strtoll (contents, NULL, 0);
      g_free (contents);
    }
  g_free (filename);

  return result;
}

static gboolean
sysfs_file_exists (const char *dir,
                   const char *attribute)
{
  gboolean result;
  char *filename;

  result = FALSE;
  filename = g_build_filename (dir, attribute, NULL);
  if (g_file_test (filename, G_FILE_TEST_EXISTS))
    {
      result = TRUE;
    }
  g_free (filename);

  return result;
}

static char *
sysfs_resolve_link (const char *sysfs_path,
                    const char *name)
{
  char *full_path;
  char link_path[PATH_MAX];
  char resolved_path[PATH_MAX];
  ssize_t num;
  gboolean found_it;

  found_it = FALSE;

  full_path = g_build_filename (sysfs_path, name, NULL);

  //g_debug ("name='%s'", name);
  //g_debug ("full_path='%s'", full_path);
  num = readlink (full_path, link_path, sizeof(link_path) - 1);
  if (num != -1)
    {
      char *absolute_path;

      link_path[num] = '\0';

      //g_debug ("link_path='%s'", link_path);
      absolute_path = g_build_filename (sysfs_path, link_path, NULL);
      //g_debug ("absolute_path='%s'", absolute_path);
      if (realpath (absolute_path, resolved_path) != NULL)
        {
          //g_debug ("resolved_path='%s'", resolved_path);
          found_it = TRUE;
        }
      g_free (absolute_path);
    }
  g_free (full_path);

  if (found_it)
    return g_strdup (resolved_path);
  else
    return NULL;
}

gboolean info_is_system_internal( device_t *device )
{
    const char *value;
    
    if ( value = udev_device_get_property_value( device->udevice, "UDISKS_SYSTEM_INTERNAL" ) )
        return atoi( value ) != 0;

    /* A Linux MD device is system internal if, and only if
    *
    * - a single component is system internal
    * - there are no components
    * SKIP THIS TEST
    */

    /* a partition is system internal only if the drive it belongs to is system internal */
    //TODO

    /* a LUKS cleartext device is system internal only if the underlying crypto-text
    * device is system internal
    * SKIP THIS TEST
    */

    // devices with removable media are never system internal
    if ( device->device_is_removable )
        return FALSE;

    /* devices on certain buses are never system internal */
    if ( device->drive_connection_interface != NULL )
    {
        if (strcmp (device->drive_connection_interface, "ata_serial_esata") == 0
          || strcmp (device->drive_connection_interface, "sdio") == 0
          || strcmp (device->drive_connection_interface, "usb") == 0
          || strcmp (device->drive_connection_interface, "firewire") == 0)
            return FALSE;
    }
    return TRUE;
}

void info_drive_connection( device_t *device )
{
  char *s;
  char *p;
  char *q;
  char *model;
  char *vendor;
  char *subsystem;
  char *serial;
  char *revision;
  const char *connection_interface;
  guint64 connection_speed;

  connection_interface = NULL;
  connection_speed = 0;
  
  /* walk up the device tree to figure out the subsystem */
  s = g_strdup (device->native_path);
  do
    {
      p = sysfs_resolve_link (s, "subsystem");
      if ( !device->device_is_removable && sysfs_get_int( s, "removable") != 0 )
            device->device_is_removable = TRUE;
      if (p != NULL)
        {
          subsystem = g_path_get_basename (p);
          g_free (p);

          if (strcmp (subsystem, "scsi") == 0)
            {
              connection_interface = "scsi";
              connection_speed = 0;

              /* continue walking up the chain; we just use scsi as a fallback */

              /* grab the names from SCSI since the names from udev currently
               *  - replaces whitespace with _
               *  - is missing for e.g. Firewire
               */
              vendor = sysfs_get_string (s, "vendor");
              if (vendor != NULL)
                {
                  g_strstrip (vendor);
                  /* Don't overwrite what we set earlier from ID_VENDOR */
                  if (device->drive_vendor == NULL)
                    {
                      device->drive_vendor = _dupv8 (vendor);
                    }
                  g_free (vendor);
                }

              model = sysfs_get_string (s, "model");
              if (model != NULL)
                {
                  g_strstrip (model);
                  /* Don't overwrite what we set earlier from ID_MODEL */
                  if (device->drive_model == NULL)
                    {
                      device->drive_model = _dupv8 (model);
                    }
                  g_free (model);
                }

              /* TODO: need to improve this code; we probably need the kernel to export more
               *       information before we can properly get the type and speed.
               */

              if (device->drive_vendor != NULL && strcmp (device->drive_vendor, "ATA") == 0)
                {
                  connection_interface = "ata";
                  break;
                }

            }
          else if (strcmp (subsystem, "usb") == 0)
            {
              double usb_speed;

              /* both the interface and the device will be 'usb'. However only
               * the device will have the 'speed' property.
               */
              usb_speed = sysfs_get_double (s, "speed");
              if (usb_speed > 0)
                {
                  connection_interface = "usb";
                  connection_speed = usb_speed * (1000 * 1000);
                  break;

                }
            }
          else if (strcmp (subsystem, "firewire") == 0 || strcmp (subsystem, "ieee1394") == 0)
            {

              /* TODO: krh has promised a speed file in sysfs; theoretically, the speed can
               *       be anything from 100, 200, 400, 800 and 3200. Till then we just hardcode
               *       a resonable default of 400 Mbit/s.
               */

              connection_interface = "firewire";
              connection_speed = 400 * (1000 * 1000);
              break;

            }
          else if (strcmp (subsystem, "mmc") == 0)
            {

              /* TODO: what about non-SD, e.g. MMC? Is that another bus? */
              connection_interface = "sdio";

              /* Set vendor name. According to this MMC document
               *
               * http://www.mmca.org/membership/IAA_Agreement_10_12_06.pdf
               *
               *  - manfid: the manufacturer id
               *  - oemid: the customer of the manufacturer
               *
               * Apparently these numbers are kept secret. It would be nice
               * to map these into names for setting the manufacturer of the drive,
               * e.g. Panasonic, Sandisk etc.
               */

              model = sysfs_get_string (s, "name");
              if (model != NULL)
                {
                  g_strstrip (model);
                  /* Don't overwrite what we set earlier from ID_MODEL */
                  if (device->drive_model == NULL)
                    {
                      device->drive_model = _dupv8 (model);
                    }
                  g_free (model);
                }

              serial = sysfs_get_string (s, "serial");
              if (serial != NULL)
                {
                  g_strstrip (serial);
                  /* Don't overwrite what we set earlier from ID_SERIAL */
                  if (device->drive_serial == NULL)
                    {
                      /* this is formatted as a hexnumber; drop the leading 0x */
                      device->drive_serial = _dupv8 (serial + 2);
                    }
                  g_free (serial);
                }

              /* TODO: use hwrev and fwrev files? */
              revision = sysfs_get_string (s, "date");
              if (revision != NULL)
                {
                  g_strstrip (revision);
                  /* Don't overwrite what we set earlier from ID_REVISION */
                  if (device->drive_revision == NULL)
                    {
                      device->drive_revision = _dupv8 (revision);
                    }
                  g_free (revision);
                }

              /* TODO: interface speed; the kernel driver knows; would be nice
               * if it could export it */

            }
          else if (strcmp (subsystem, "platform") == 0)
            {
              const gchar *sysfs_name;

              sysfs_name = g_strrstr (s, "/");
              if (g_str_has_prefix (sysfs_name + 1, "floppy.")
                                            && device->drive_vendor == NULL )
                {
                  device->drive_vendor = g_strdup( "Floppy Drive" );
                  connection_interface = "platform";
                }
            }

          g_free (subsystem);
        }

      /* advance up the chain */
      p = g_strrstr (s, "/");
      if (p == NULL)
        break;
      *p = '\0';

      /* but stop at the root */
      if (strcmp (s, "/sys/devices") == 0)
        break;

    }
  while (TRUE);

  if (connection_interface != NULL)
    {
        device->drive_connection_interface = g_strdup( connection_interface );
        device->drive_connection_speed = connection_speed;
    }

  g_free (s);
}

static const struct
{
  const char *udev_property;
  const char *media_name;
} drive_media_mapping[] =
  {
    { "ID_DRIVE_FLASH", "flash" },
    { "ID_DRIVE_FLASH_CF", "flash_cf" },
    { "ID_DRIVE_FLASH_MS", "flash_ms" },
    { "ID_DRIVE_FLASH_SM", "flash_sm" },
    { "ID_DRIVE_FLASH_SD", "flash_sd" },
    { "ID_DRIVE_FLASH_SDHC", "flash_sdhc" },
    { "ID_DRIVE_FLASH_MMC", "flash_mmc" },
    { "ID_DRIVE_FLOPPY", "floppy" },
    { "ID_DRIVE_FLOPPY_ZIP", "floppy_zip" },
    { "ID_DRIVE_FLOPPY_JAZ", "floppy_jaz" },
    { "ID_CDROM", "optical_cd" },
    { "ID_CDROM_CD_R", "optical_cd_r" },
    { "ID_CDROM_CD_RW", "optical_cd_rw" },
    { "ID_CDROM_DVD", "optical_dvd" },
    { "ID_CDROM_DVD_R", "optical_dvd_r" },
    { "ID_CDROM_DVD_RW", "optical_dvd_rw" },
    { "ID_CDROM_DVD_RAM", "optical_dvd_ram" },
    { "ID_CDROM_DVD_PLUS_R", "optical_dvd_plus_r" },
    { "ID_CDROM_DVD_PLUS_RW", "optical_dvd_plus_rw" },
    { "ID_CDROM_DVD_PLUS_R_DL", "optical_dvd_plus_r_dl" },
    { "ID_CDROM_DVD_PLUS_RW_DL", "optical_dvd_plus_rw_dl" },
    { "ID_CDROM_BD", "optical_bd" },
    { "ID_CDROM_BD_R", "optical_bd_r" },
    { "ID_CDROM_BD_RE", "optical_bd_re" },
    { "ID_CDROM_HDDVD", "optical_hddvd" },
    { "ID_CDROM_HDDVD_R", "optical_hddvd_r" },
    { "ID_CDROM_HDDVD_RW", "optical_hddvd_rw" },
    { "ID_CDROM_MO", "optical_mo" },
    { "ID_CDROM_MRW", "optical_mrw" },
    { "ID_CDROM_MRW_W", "optical_mrw_w" },
    { NULL, NULL }, };

static const struct
{
  const char *udev_property;
  const char *media_name;
} media_mapping[] =
  {
    { "ID_DRIVE_MEDIA_FLASH", "flash" },
    { "ID_DRIVE_MEDIA_FLASH_CF", "flash_cf" },
    { "ID_DRIVE_MEDIA_FLASH_MS", "flash_ms" },
    { "ID_DRIVE_MEDIA_FLASH_SM", "flash_sm" },
    { "ID_DRIVE_MEDIA_FLASH_SD", "flash_sd" },
    { "ID_DRIVE_MEDIA_FLASH_SDHC", "flash_sdhc" },
    { "ID_DRIVE_MEDIA_FLASH_MMC", "flash_mmc" },
    { "ID_DRIVE_MEDIA_FLOPPY", "floppy" },
    { "ID_DRIVE_MEDIA_FLOPPY_ZIP", "floppy_zip" },
    { "ID_DRIVE_MEDIA_FLOPPY_JAZ", "floppy_jaz" },
    { "ID_CDROM_MEDIA_CD", "optical_cd" },
    { "ID_CDROM_MEDIA_CD_R", "optical_cd_r" },
    { "ID_CDROM_MEDIA_CD_RW", "optical_cd_rw" },
    { "ID_CDROM_MEDIA_DVD", "optical_dvd" },
    { "ID_CDROM_MEDIA_DVD_R", "optical_dvd_r" },
    { "ID_CDROM_MEDIA_DVD_RW", "optical_dvd_rw" },
    { "ID_CDROM_MEDIA_DVD_RAM", "optical_dvd_ram" },
    { "ID_CDROM_MEDIA_DVD_PLUS_R", "optical_dvd_plus_r" },
    { "ID_CDROM_MEDIA_DVD_PLUS_RW", "optical_dvd_plus_rw" },
    { "ID_CDROM_MEDIA_DVD_PLUS_R_DL", "optical_dvd_plus_r_dl" },
    { "ID_CDROM_MEDIA_DVD_PLUS_RW_DL", "optical_dvd_plus_rw_dl" },
    { "ID_CDROM_MEDIA_BD", "optical_bd" },
    { "ID_CDROM_MEDIA_BD_R", "optical_bd_r" },
    { "ID_CDROM_MEDIA_BD_RE", "optical_bd_re" },
    { "ID_CDROM_MEDIA_HDDVD", "optical_hddvd" },
    { "ID_CDROM_MEDIA_HDDVD_R", "optical_hddvd_r" },
    { "ID_CDROM_MEDIA_HDDVD_RW", "optical_hddvd_rw" },
    { "ID_CDROM_MEDIA_MO", "optical_mo" },
    { "ID_CDROM_MEDIA_MRW", "optical_mrw" },
    { "ID_CDROM_MEDIA_MRW_W", "optical_mrw_w" },
    { NULL, NULL }, };

void info_drive_properties ( device_t *device )
{
    GPtrArray *media_compat_array;
    const char *media_in_drive;
    gboolean drive_is_ejectable;
    gboolean drive_can_detach;
    char *decoded_string;
    guint n;
    const char *value;

    // drive identification 
    device->device_is_drive = sysfs_file_exists( device->native_path, "range" );

    // vendor
    if ( value = udev_device_get_property_value( device->udevice, "ID_VENDOR_ENC" ) )
    {
        decoded_string = decode_udev_encoded_string ( value );
        g_strstrip (decoded_string);
        device->drive_vendor = decoded_string;
    }
    else if ( value = udev_device_get_property_value( device->udevice, "ID_VENDOR" ) )
    {
        device->drive_vendor = g_strdup( value );
    }
    
    // model
    if ( value = udev_device_get_property_value( device->udevice, "ID_MODEL_ENC" ) )
    {
        decoded_string = decode_udev_encoded_string ( value );
        g_strstrip (decoded_string);
        device->drive_model = decoded_string;
    }
    else if ( value = udev_device_get_property_value( device->udevice, "ID_MODEL" ) )
    {
        device->drive_model = g_strdup( value );
    }
    
    // revision
    device->drive_revision = g_strdup( udev_device_get_property_value( 
                                                    device->udevice, "ID_REVISION" ) );
    
    // serial
    if ( value = udev_device_get_property_value( device->udevice, "ID_SCSI_SERIAL" ) )
    {
        /* scsi_id sometimes use the WWN as the serial - annoying - see
        * http://git.kernel.org/?p=linux/hotplug/udev.git;a=commit;h=4e9fdfccbdd16f0cfdb5c8fa8484a8ba0f2e69d3
        * for details
        */
        device->drive_serial = g_strdup( value );
    }
    else if ( value = udev_device_get_property_value( device->udevice, "ID_SERIAL_SHORT" ) )
    {
        device->drive_serial = g_strdup( value );
    }

    // wwn
    if ( value = udev_device_get_property_value( device->udevice, "ID_WWN_WITH_EXTENSION" ) )
    {
        device->drive_wwn = g_strdup( value + 2 );
    }
    else if ( value = udev_device_get_property_value( device->udevice, "ID_WWN" ) )
    {
        device->drive_wwn = g_strdup( value + 2 );
    }
    
    /* pick up some things (vendor, model, connection_interface, connection_speed)
    * not (yet) exported by udev helpers
    */
    //update_drive_properties_from_sysfs (device);
    info_drive_connection( device );

    // is_ejectable
    if ( value = udev_device_get_property_value( device->udevice, "ID_DRIVE_EJECTABLE" ) )
    {
        drive_is_ejectable = atoi( value ) != 0;
    }
    else
    {
      drive_is_ejectable = FALSE;
      drive_is_ejectable |= ( udev_device_get_property_value( 
                                    device->udevice, "ID_CDROM" ) != NULL );
      drive_is_ejectable |= ( udev_device_get_property_value( 
                                    device->udevice, "ID_DRIVE_FLOPPY_ZIP" ) != NULL );
      drive_is_ejectable |= ( udev_device_get_property_value( 
                                    device->udevice, "ID_DRIVE_FLOPPY_JAZ" ) != NULL );
    }
    device->drive_is_media_ejectable = drive_is_ejectable;

    // drive_media_compatibility
    media_compat_array = g_ptr_array_new ();
    for (n = 0; drive_media_mapping[n].udev_property != NULL; n++)
    {
        if ( udev_device_get_property_value( device->udevice, 
                                drive_media_mapping[n].udev_property ) == NULL )
            continue;

      g_ptr_array_add (media_compat_array, (gpointer) drive_media_mapping[n].media_name);
    }
    /* special handling for SDIO since we don't yet have a sdio_id helper in udev to set properties */
    if (g_strcmp0 (device->drive_connection_interface, "sdio") == 0)
    {
      gchar *type;

      type = sysfs_get_string (device->native_path, "../../type");
      g_strstrip (type);
      if (g_strcmp0 (type, "MMC") == 0)
        {
          g_ptr_array_add (media_compat_array, "flash_mmc");
        }
      else if (g_strcmp0 (type, "SD") == 0)
        {
          g_ptr_array_add (media_compat_array, "flash_sd");
        }
      else if (g_strcmp0 (type, "SDHC") == 0)
        {
          g_ptr_array_add (media_compat_array, "flash_sdhc");
        }
      g_free (type);
    }
    g_ptr_array_sort (media_compat_array, (GCompareFunc) ptr_str_array_compare);
    g_ptr_array_add (media_compat_array, NULL);
    device->drive_media_compatibility = g_strjoinv( " ", (gchar**)media_compat_array->pdata );

    // drive_media
    media_in_drive = NULL;
    if (device->device_is_media_available)
    {
        for (n = 0; media_mapping[n].udev_property != NULL; n++)
        {
            if ( udev_device_get_property_value( device->udevice, 
                                    media_mapping[n].udev_property ) == NULL )
                continue;
            // should this be media_mapping[n] ?  doesn't matter, same?
            media_in_drive = drive_media_mapping[n].media_name;
            break;
        }
      /* If the media isn't set (from e.g. udev rules), just pick the first one in media_compat - note
       * that this may be NULL (if we don't know what media is compatible with the drive) which is OK.
       */
        if (media_in_drive == NULL)
            media_in_drive = ((const gchar **) media_compat_array->pdata)[0];
    }
    device->drive_media = g_strdup( media_in_drive );
    g_ptr_array_free (media_compat_array, TRUE);

    // drive_can_detach
    // right now, we only offer to detach USB devices
    drive_can_detach = FALSE;
    if (g_strcmp0 (device->drive_connection_interface, "usb") == 0)
    {
      drive_can_detach = TRUE;
    }
    if ( value = udev_device_get_property_value( device->udevice, "ID_DRIVE_DETACHABLE" ) )
    {
        drive_can_detach = atoi( value ) != 0;
    }
    device->drive_can_detach = drive_can_detach;
}

void info_device_properties( device_t *device )
{
    const char* value;
    
    device->native_path = g_strdup( udev_device_get_syspath( device->udevice ) );
    device->devnode = g_strdup( udev_device_get_devnode( device->udevice ) );
    device->major = g_strdup( udev_device_get_property_value( device->udevice, "MAJOR") );
    device->minor = g_strdup( udev_device_get_property_value( device->udevice, "MINOR") );
    if ( !device->native_path || !device->devnode || !device->major || !device->minor )
    {
        if ( device->native_path )
            g_free( device->native_path );
        device->native_path = NULL;
        return;
    }
    
    //by id - would need to read symlinks in /dev/disk/by-id
    
    // is_removable may also be set in info_drive_connection walking up sys tree
    device->device_is_removable = sysfs_get_int( device->native_path, "removable");

    device->device_presentation_hide = g_strdup( udev_device_get_property_value(
                                            device->udevice, "UDISKS_PRESENTATION_HIDE") );
    device->device_presentation_nopolicy = g_strdup( udev_device_get_property_value(
                                            device->udevice, "UDISKS_PRESENTATION_NOPOLICY") );
    device->device_presentation_name = g_strdup( udev_device_get_property_value(
                                            device->udevice, "UDISKS_PRESENTATION_NAME") );
    device->device_presentation_icon_name = g_strdup( udev_device_get_property_value(
                                            device->udevice, "UDISKS_PRESENTATION_ICON_NAME") );
    device->device_automount_hint = g_strdup( udev_device_get_property_value(
                                            device->udevice, "UDISKS_AUTOMOUNT_HINT") );

    // filesystem properties
    gchar *decoded_string;
    const gchar *partition_scheme;
    gint partition_type = 0;
    
    partition_scheme = udev_device_get_property_value( device->udevice, "UDISKS_PARTITION_SCHEME");
    if ( value = udev_device_get_property_value( device->udevice, "UDISKS_PARTITION_TYPE") )
        partition_type = atoi( value );
    if (g_strcmp0 (partition_scheme, "mbr") == 0 && (partition_type == 0x05 || 
                                                   partition_type == 0x0f || 
                                                   partition_type == 0x85))
    {
    }
    else
    {
        device->id_usage = g_strdup( udev_device_get_property_value( device->udevice,
                                                        "ID_FS_USAGE" ) );
        device->id_type = g_strdup( udev_device_get_property_value( device->udevice,
                                                        "ID_FS_TYPE" ) );
        device->id_version = g_strdup( udev_device_get_property_value( device->udevice,
                                                        "ID_FS_VERSION" ) );
        device->id_uuid = g_strdup( udev_device_get_property_value( device->udevice,
                                                        "ID_FS_UUID" ) );

        if ( value = udev_device_get_property_value( device->udevice, "ID_FS_LABEL_ENC" ) )
        {
            decoded_string = decode_udev_encoded_string ( value );
            g_strstrip (decoded_string);
            device->id_label = decoded_string;
        }
        else if ( value = udev_device_get_property_value( device->udevice, "ID_FS_LABEL" ) )
        {
            device->id_label = g_strdup( value );
        }
    }

    // device_is_media_available
    gboolean media_available = FALSE;
    
    if ( ( device->id_usage && device->id_usage[0] != '\0' ) ||
         ( device->id_type  && device->id_type[0]  != '\0' ) ||
         ( device->id_uuid  && device->id_uuid[0]  != '\0' ) ||
         ( device->id_label && device->id_label[0] != '\0' ) )
    {
         media_available = TRUE;
    }
    else if ( g_str_has_prefix( device->devnode, "/dev/loop" ) )
        media_available = FALSE;
    else if ( device->device_is_removable )
    {
        gboolean is_cd, is_floppy;
        if ( value = udev_device_get_property_value( device->udevice, "ID_CDROM" ) )
            is_cd = atoi( value ) != 0;
        else
            is_cd = FALSE;

        if ( value = udev_device_get_property_value( device->udevice, "ID_DRIVE_FLOPPY" ) )
            is_floppy = atoi( value ) != 0;
        else
            is_floppy = FALSE;

        if ( !is_cd && !is_floppy )
        {
            // this test is limited for non-root - user may not have read
            // access to device file even if media is present
            int fd;
            fd = open( device->devnode, O_RDONLY );
            if ( fd >= 0 )
            {
                media_available = TRUE;
                close( fd );
            }
        }
        else if ( value = udev_device_get_property_value( device->udevice, "ID_CDROM_MEDIA" ) )
            media_available = ( atoi( value ) == 1 );
    }
    else if ( value = udev_device_get_property_value( device->udevice, "ID_CDROM_MEDIA" ) )
        media_available = ( atoi( value ) == 1 );
    else
        media_available = TRUE;
    device->device_is_media_available = media_available;

    /* device_size, device_block_size and device_is_read_only properties */
    if (device->device_is_media_available)
    {
        guint64 block_size;

        device->device_size = sysfs_get_uint64( device->native_path, "size")
                                                                * ((guint64) 512);
        device->device_is_read_only = (sysfs_get_int (device->native_path,
                                                                    "ro") != 0);
        /* This is not available on all devices so fall back to 512 if unavailable.
        *
        * Another way to get this information is the BLKSSZGET ioctl but we don't want
        * to open the device. Ideally vol_id would export it.
        */
        block_size = sysfs_get_uint64 (device->native_path, "queue/hw_sector_size");
        if (block_size == 0)
            block_size = 512;
        device->device_block_size = block_size;
    }
    else
    {
        device->device_size = device->device_block_size = 0;
        device->device_is_read_only = FALSE;
    }
    
    // links
    struct udev_list_entry *entry = udev_device_get_devlinks_list_entry( 
                                                                device->udevice );    
    while ( entry )
    {
        const char *entry_name = udev_list_entry_get_name( entry );
        if ( entry_name && ( g_str_has_prefix( entry_name, "/dev/disk/by-id/" )
                || g_str_has_prefix( entry_name, "/dev/disk/by-uuid/" ) ) )
        {
            device->device_by_id = g_strdup( entry_name );
            break;
        }
        entry = udev_list_entry_get_next( entry );
    }
}

gchar* info_mount_points( device_t *device )
{
    gchar *contents;
    gchar **lines;
    GError *error;
    guint n;
    GList* mounts = NULL;
    
    if ( !device->major || !device->minor )
        return NULL;
    guint dmajor = atoi( device->major );
    guint dminor = atoi( device->minor );

    // if we have the mount point list, use this instead of reading mountinfo
    if ( devmounts )
    {
        GList* l;
        for ( l = devmounts; l; l = l->next )
        {
            if ( ((devmount_t*)l->data)->major == dmajor && 
                                        ((devmount_t*)l->data)->minor == dminor )
            {
                return g_strdup( ((devmount_t*)l->data)->mount_points );
            }
        }
        return NULL;
    }

    contents = NULL;
    lines = NULL;

    error = NULL;
    if (!g_file_get_contents ("/proc/self/mountinfo", &contents, NULL, &error))
    {
        g_warning ("Error reading /proc/self/mountinfo: %s", error->message);
        g_error_free (error);
        return NULL;
    }

    /* See Documentation/filesystems/proc.txt for the format of /proc/self/mountinfo
    *
    * Note that things like space are encoded as \020.
    */

  lines = g_strsplit (contents, "\n", 0);
  for (n = 0; lines[n] != NULL; n++)
    {
      guint mount_id;
      guint parent_id;
      guint major, minor;
      gchar encoded_root[PATH_MAX];
      gchar encoded_mount_point[PATH_MAX];
      gchar *mount_point;
      //dev_t dev;
      
      if (strlen (lines[n]) == 0)
        continue;

      if (sscanf (lines[n],
                  "%d %d %d:%d %s %s",
                  &mount_id,
                  &parent_id,
                  &major,
                  &minor,
                  encoded_root,
                  encoded_mount_point) != 6)
        {
          g_warning ("Error reading /proc/self/mountinfo: Error parsing line '%s'", lines[n]);
          continue;
        }

        if ( major != dmajor || minor != dminor )
            continue;
            
      /* ignore mounts where only a subtree of a filesystem is mounted */
      if (g_strcmp0 (encoded_root, "/") != 0)
        continue;

      mount_point = g_strcompress (encoded_mount_point);
      if ( mount_point && mount_point[0] != '\0' )
      {
        if ( !g_list_find( mounts, mount_point ) )
        {
            mounts = g_list_prepend( mounts, mount_point );
        }
        else
            g_free (mount_point);
      }
      
    }
  g_free (contents);
  g_strfreev (lines);

    if ( mounts )
    {
        gchar *points, *old_points;
        GList* l;
        // Sort the list to ensure that shortest mount paths appear first
        mounts = g_list_sort( mounts, (GCompareFunc) g_strcmp0 );
        points = g_strdup( (gchar*)mounts->data );
        l = mounts;
        while ( l = l->next )
        {
            old_points = points;
            points = g_strdup_printf( "%s, %s", old_points, (gchar*)l->data );
            g_free( old_points );
        }
        g_list_foreach( mounts, (GFunc)g_free, NULL );
        g_list_free( mounts );
        return points;
    }
    else
        return NULL;
}

void info_partition_table( device_t *device )
{
  gboolean is_partition_table = FALSE;
  const char* value;
  
    /* Check if udisks-part-id identified the device as a partition table.. this includes
    * identifying partition tables set up by kpartx for multipath etc.
    */
    if ( ( value = udev_device_get_property_value( device->udevice, "UDISKS_PARTITION_TABLE" ) ) 
                                                    && atoi( value ) == 1 )
    {
        device->partition_table_scheme = g_strdup( udev_device_get_property_value( 
                                                device->udevice,
                                                "UDISKS_PARTITION_TABLE_SCHEME" ) );
        device->partition_table_count = g_strdup( udev_device_get_property_value( 
                                                device->udevice,
                                                "UDISKS_PARTITION_TABLE_COUNT" ) );
        is_partition_table = TRUE;
    }

  /* Note that udisks-part-id might not detect all partition table
   * formats.. so in the negative case, also double check with
   * information in sysfs.
   *
   * The kernel guarantees that all childs are created before the
   * uevent for the parent is created. So if we have childs, we must
   * be a partition table.
   *
   * To detect a child we check for the existance of a subdir that has
   * the parents name as a prefix (e.g. for parent sda then sda1,
   * sda2, sda3 ditto md0, md0p1 etc. etc. will work).
   */
  if (!is_partition_table)
    {
      gchar *s;
      GDir *dir;

      s = g_path_get_basename (device->native_path);
      if ((dir = g_dir_open (device->native_path, 0, NULL)) != NULL)
        {
          guint partition_count;
          const gchar *name;

          partition_count = 0;
          while ((name = g_dir_read_name (dir)) != NULL)
            {
              if (g_str_has_prefix (name, s))
                {
                  partition_count++;
                }
            }
          g_dir_close (dir);

          if (partition_count > 0)
            {
              device->partition_table_scheme = g_strdup( "" );
              device->partition_table_count = g_strdup_printf( "%d", partition_count );
              is_partition_table = TRUE;
            }
        }
      g_free (s);
    }

  device->device_is_partition_table = is_partition_table;
  if (!is_partition_table)
    {
        if ( device->partition_table_scheme )
            g_free( device->partition_table_scheme );
        device->partition_table_scheme = NULL;
        if ( device->partition_table_count )
            g_free( device->partition_table_count );
        device->partition_table_count = NULL;
    }
}

void info_partition( device_t *device )
{
  gboolean is_partition = FALSE;

  /* Check if udisks-part-id identified the device as a partition.. this includes
   * identifying partitions set up by kpartx for multipath
   */
  if ( udev_device_get_property_value( device->udevice,"UDISKS_PARTITION" ) )
    {
      const gchar *size;
      const gchar *scheme;
      const gchar *type;
      const gchar *label;
      const gchar *uuid;
      const gchar *flags;
      const gchar *offset;
      const gchar *alignment_offset;
      const gchar *slave_sysfs_path;
      const gchar *number;

      scheme = udev_device_get_property_value( device->udevice, "UDISKS_PARTITION_SCHEME");
      size = udev_device_get_property_value( device->udevice, "UDISKS_PARTITION_SIZE");
      type = udev_device_get_property_value( device->udevice, "UDISKS_PARTITION_TYPE");
      label = udev_device_get_property_value( device->udevice, "UDISKS_PARTITION_LABEL");
      uuid = udev_device_get_property_value( device->udevice, "UDISKS_PARTITION_UUID");
      flags = udev_device_get_property_value( device->udevice, "UDISKS_PARTITION_FLAGS");
      offset = udev_device_get_property_value( device->udevice, "UDISKS_PARTITION_OFFSET");
      alignment_offset = udev_device_get_property_value( device->udevice, 
                                                "UDISKS_PARTITION_ALIGNMENT_OFFSET");
      number = udev_device_get_property_value( device->udevice, "UDISKS_PARTITION_NUMBER");
      slave_sysfs_path = udev_device_get_property_value( device->udevice, "UDISKS_PARTITION_SLAVE");

      if (slave_sysfs_path != NULL && scheme != NULL && number != NULL && atoi( number ) > 0)
        {
          device->partition_scheme = g_strdup( scheme );
          device->partition_size = g_strdup( size );
          device->partition_type = g_strdup( type );
          device->partition_label = g_strdup( label );
          device->partition_uuid = g_strdup( uuid );
          device->partition_flags = g_strdup( flags );
          device->partition_offset = g_strdup( offset );
          device->partition_alignment_offset = g_strdup( alignment_offset );
          device->partition_number = g_strdup( number );
          is_partition = TRUE;
        }
    }

  /* Also handle the case where we are partitioned by the kernel and don't have
   * any UDISKS_PARTITION_* properties.
   *
   * This works without any udev UDISKS_PARTITION_* properties and is
   * there for maximum compatibility since udisks-part-id only knows a
   * limited set of partition table formats.
   */
  if (!is_partition && sysfs_file_exists (device->native_path, "start"))
    {
      guint64 size;
      guint64 offset;
      guint64 alignment_offset;
      gchar *s;
      guint n;

      size = sysfs_get_uint64 (device->native_path, "size");
      alignment_offset = sysfs_get_uint64 (device->native_path, "alignment_offset");

      device->partition_size = g_strdup_printf( "%d", size * 512 );
      device->partition_alignment_offset = g_strdup_printf( "%d", alignment_offset );

      offset = sysfs_get_uint64 (device->native_path, "start") * device->device_block_size;
      device->partition_offset = g_strdup_printf( "%d", offset );

      s = device->native_path;
      for (n = strlen (s) - 1; n >= 0 && g_ascii_isdigit (s[n]); n--)
        ;
      device->partition_number = g_strdup_printf( "%d", strtol (s + n + 1, NULL, 0) );
        /*
      s = g_strdup (device->priv->native_path);
      for (n = strlen (s) - 1; n >= 0 && s[n] != '/'; n--)
        s[n] = '\0';
      s[n] = '\0';
      device_set_partition_slave (device, compute_object_path (s));
      g_free (s);
        */
      is_partition = TRUE;
    }

    device->device_is_partition = is_partition;

  if (!is_partition)
    {
      device->partition_scheme = NULL;
      device->partition_size = NULL;
      device->partition_type = NULL;
      device->partition_label = NULL;
      device->partition_uuid = NULL;
      device->partition_flags = NULL;
      device->partition_offset = NULL;
      device->partition_alignment_offset = NULL;
      device->partition_number = NULL;
    }
  else
    {
        device->device_is_drive = FALSE;
    }
}

void info_optical_disc( device_t *device )
{
    const char *cdrom_disc_state;

    if ( udev_device_get_property_value( device->udevice, "ID_CDROM_MEDIA") )
    {
        device->device_is_optical_disc = TRUE;

        device->optical_disc_num_tracks = g_strdup( udev_device_get_property_value(
                                    device->udevice, "ID_CDROM_MEDIA_TRACK_COUNT") );
        device->optical_disc_num_audio_tracks = g_strdup( udev_device_get_property_value(
                                    device->udevice, "ID_CDROM_MEDIA_TRACK_COUNT_AUDIO") );
        device->optical_disc_num_sessions = g_strdup( udev_device_get_property_value(
                                    device->udevice, "ID_CDROM_MEDIA_SESSION_COUNT") );
                                    
        cdrom_disc_state = udev_device_get_property_value( device->udevice, "ID_CDROM_MEDIA_STATE");

        device->optical_disc_is_blank = ( g_strcmp0( cdrom_disc_state,
                                                            "blank" ) == 0);
        device->optical_disc_is_appendable = ( g_strcmp0( cdrom_disc_state,
                                                            "appendable" ) == 0);
        device->optical_disc_is_closed = ( g_strcmp0( cdrom_disc_state,
                                                            "complete" ) == 0);
    }
    else
        device->device_is_optical_disc = FALSE;
}

static void device_free( device_t *device )
{
    if ( !device )
        return;
        
    g_free( device->native_path );
    g_free( device->major );
    g_free( device->minor );
    g_free( device->mount_points );
    g_free( device->devnode );

    g_free( device->device_presentation_hide );
    g_free( device->device_presentation_nopolicy );
    g_free( device->device_presentation_name );
    g_free( device->device_presentation_icon_name );
    g_free( device->device_automount_hint );
    g_free( device->device_by_id );
    g_free( device->id_usage );
    g_free( device->id_type );
    g_free( device->id_version );
    g_free( device->id_uuid );
    g_free( device->id_label );

    g_free( device->drive_vendor );
    g_free( device->drive_model );
    g_free( device->drive_revision );
    g_free( device->drive_serial );
    g_free( device->drive_wwn );
    g_free( device->drive_connection_interface );
    g_free( device->drive_media_compatibility );
    g_free( device->drive_media );

    g_free( device->partition_scheme );
    g_free( device->partition_number );
    g_free( device->partition_type );
    g_free( device->partition_label );
    g_free( device->partition_uuid );
    g_free( device->partition_flags );
    g_free( device->partition_offset );
    g_free( device->partition_size );
    g_free( device->partition_alignment_offset );

    g_free( device->partition_table_scheme );
    g_free( device->partition_table_count );

    g_free( device->optical_disc_num_tracks );
    g_free( device->optical_disc_num_audio_tracks );
    g_free( device->optical_disc_num_sessions );
    g_slice_free( device_t, device );
}

device_t *device_alloc( struct udev_device *udevice )
{
    device_t *device = g_slice_new0( device_t );
    device->udevice = udevice;

    device->native_path = NULL;
    device->major = NULL;
    device->minor = NULL;
    device->mount_points = NULL;
    device->devnode = NULL;

    device->device_is_system_internal = TRUE;
    device->device_is_partition = FALSE;
    device->device_is_partition_table = FALSE;
    device->device_is_removable = FALSE;
    device->device_is_media_available = FALSE;
    device->device_is_read_only = FALSE;
    device->device_is_drive = FALSE;
    device->device_is_optical_disc = FALSE;
    device->device_is_mounted = FALSE;
    device->device_presentation_hide = NULL;
    device->device_presentation_nopolicy = NULL;
    device->device_presentation_name = NULL;
    device->device_presentation_icon_name = NULL;
    device->device_automount_hint = NULL;
    device->device_by_id = NULL;
    device->device_size = 0;
    device->device_block_size = 0;
    device->id_usage = NULL;
    device->id_type = NULL;
    device->id_version = NULL;
    device->id_uuid = NULL;
    device->id_label = NULL;

    device->drive_vendor = NULL;
    device->drive_model = NULL;
    device->drive_revision = NULL;
    device->drive_serial = NULL;
    device->drive_wwn = NULL;
    device->drive_connection_interface = NULL;
    device->drive_connection_speed = 0;
    device->drive_media_compatibility = NULL;
    device->drive_media = NULL;
    device->drive_is_media_ejectable = FALSE;
    device->drive_can_detach = FALSE;

    device->partition_scheme = NULL;
    device->partition_number = NULL;
    device->partition_type = NULL;
    device->partition_label = NULL;
    device->partition_uuid = NULL;
    device->partition_flags = NULL;
    device->partition_offset = NULL;
    device->partition_size = NULL;
    device->partition_alignment_offset = NULL;

    device->partition_table_scheme = NULL;
    device->partition_table_count = NULL;

    device->optical_disc_is_blank = FALSE;
    device->optical_disc_is_appendable = FALSE;
    device->optical_disc_is_closed = FALSE;
    device->optical_disc_num_tracks = NULL;
    device->optical_disc_num_audio_tracks = NULL;
    device->optical_disc_num_sessions = NULL;
    
    return device;
}

gboolean device_get_info( device_t *device )
{
    info_device_properties( device );
    if ( !device->native_path )
        return FALSE;
    info_drive_properties( device );
    device->device_is_system_internal = info_is_system_internal( device );
    device->mount_points = info_mount_points( device );
    device->device_is_mounted = ( device->mount_points != NULL );
    if ( device->device_is_mounted )
        device->device_is_media_available = TRUE;
    info_partition_table( device );
    info_partition( device );
    info_optical_disc( device );
    return TRUE;
}

char* device_show_info( device_t *device )
{
    gchar* line[140];
    int i = 0;
    
    line[i++] = g_strdup_printf("Showing information for %s\n", device->devnode );
    line[i++] = g_strdup_printf("  native-path:                 %s\n", device->native_path );
    line[i++] = g_strdup_printf("  device:                      %s:%s\n", device->major, device->minor );
    line[i++] = g_strdup_printf("  device-file:                 %s\n", device->devnode );
    line[i++] = g_strdup_printf("    presentation:              %s\n", device->devnode );
    if ( device->device_by_id )
        line[i++] = g_strdup_printf("    by-id:                     %s\n", device->device_by_id );
    line[i++] = g_strdup_printf("  system internal:             %d\n", device->device_is_system_internal );
    line[i++] = g_strdup_printf("  removable:                   %d\n", device->device_is_removable);
    line[i++] = g_strdup_printf("  has media:                   %d\n", device->device_is_media_available);
    line[i++] = g_strdup_printf("  is read only:                %d\n", device->device_is_read_only );
    line[i++] = g_strdup_printf("  is mounted:                  %d\n", device->device_is_mounted );
    line[i++] = g_strdup_printf("  mount paths:                 %s\n", device->mount_points ? device->mount_points : "" );
    line[i++] = g_strdup_printf("  presentation hide:           %s\n", device->device_presentation_hide ?
                                                device->device_presentation_hide : "0" );
    line[i++] = g_strdup_printf("  presentation nopolicy:       %s\n", device->device_presentation_nopolicy ?
                                                device->device_presentation_nopolicy : "0" );
    line[i++] = g_strdup_printf("  presentation name:           %s\n", device->device_presentation_name ?
                                                device->device_presentation_name : "" );
    line[i++] = g_strdup_printf("  presentation icon:           %s\n", device->device_presentation_icon_name ?
                                                device->device_presentation_icon_name : "" );
    line[i++] = g_strdup_printf("  automount hint:              %s\n", device->device_automount_hint ?
                                                device->device_automount_hint : "" );
    line[i++] = g_strdup_printf("  size:                        %" G_GUINT64_FORMAT "\n", device->device_size);
    line[i++] = g_strdup_printf("  block size:                  %" G_GUINT64_FORMAT "\n", device->device_block_size);
    line[i++] = g_strdup_printf("  usage:                       %s\n", device->id_usage ? device->id_usage : "" );
    line[i++] = g_strdup_printf("  type:                        %s\n", device->id_type ? device->id_type : "" );
    line[i++] = g_strdup_printf("  version:                     %s\n", device->id_version ? device->id_version : "" );
    line[i++] = g_strdup_printf("  uuid:                        %s\n", device->id_uuid ? device->id_uuid : "" );
    line[i++] = g_strdup_printf("  label:                       %s\n", device->id_label ? device->id_label : "" );
    if (device->device_is_partition_table)
    {
        line[i++] = g_strdup_printf("  partition table:\n");
        line[i++] = g_strdup_printf("    scheme:                    %s\n", device->partition_table_scheme ? 
                                                    device->partition_table_scheme : "" );
        line[i++] = g_strdup_printf("    count:                     %s\n", device->partition_table_count ?
                                                    device->partition_table_count : "0" );
    }
    if (device->device_is_partition)
    {
        line[i++] = g_strdup_printf("  partition:\n");
        line[i++] = g_strdup_printf("    scheme:                    %s\n", device->partition_scheme ?
                                                    device->partition_scheme : "" );
        line[i++] = g_strdup_printf("    number:                    %d\n", device->partition_number ? 
                                                    device->partition_number : "" );
        line[i++] = g_strdup_printf("    type:                      %s\n", device->partition_type ?
                                                    device->partition_type : "" );
        line[i++] = g_strdup_printf("    flags:                     %s\n", device->partition_flags ? 
                                                    device->partition_flags : "" );
        line[i++] = g_strdup_printf("    offset:                    %s\n", device->partition_offset ? 
                                                    device->partition_offset : "" );
        line[i++] = g_strdup_printf("    alignment offset:          %s\n", device->partition_alignment_offset ? 
                                                    device->partition_alignment_offset : "" );
        line[i++] = g_strdup_printf("    size:                      %s\n", device->partition_size ? 
                                                    device->partition_size : "" );
        line[i++] = g_strdup_printf("    label:                     %s\n", device->partition_label ? 
                                                    device->partition_label : "" );
        line[i++] = g_strdup_printf("    uuid:                      %s\n", device->partition_uuid ? 
                                                    device->partition_uuid : "" );
    }
    if (device->device_is_optical_disc)
    {
        line[i++] = g_strdup_printf("  optical disc:\n");
        line[i++] = g_strdup_printf("    blank:                     %d\n", device->optical_disc_is_blank);
        line[i++] = g_strdup_printf("    appendable:                %d\n", device->optical_disc_is_appendable);
        line[i++] = g_strdup_printf("    closed:                    %d\n", device->optical_disc_is_closed);
        line[i++] = g_strdup_printf("    num tracks:                %s\n", device->optical_disc_num_tracks ?
                                                    device->optical_disc_num_tracks : "0" );
        line[i++] = g_strdup_printf("    num audio tracks:          %s\n", device->optical_disc_num_audio_tracks ?
                                                    device->optical_disc_num_audio_tracks : "0" );
        line[i++] = g_strdup_printf("    num sessions:              %s\n", device->optical_disc_num_sessions ?
                                                    device->optical_disc_num_sessions : "0" );
    }
    if (device->device_is_drive)
    {
        line[i++] = g_strdup_printf("  drive:\n");
        line[i++] = g_strdup_printf("    vendor:                    %s\n", device->drive_vendor ?
                                                    device->drive_vendor : "" );
        line[i++] = g_strdup_printf("    model:                     %s\n", device->drive_model ?
                                                    device->drive_model : "" );
        line[i++] = g_strdup_printf("    revision:                  %s\n", device->drive_revision ?
                                                    device->drive_revision : "" );
        line[i++] = g_strdup_printf("    serial:                    %s\n", device->drive_serial ?
                                                    device->drive_serial : "" );
        line[i++] = g_strdup_printf("    WWN:                       %s\n", device->drive_wwn ?
                                                    device->drive_wwn : "" );
        line[i++] = g_strdup_printf("    detachable:                %d\n", device->drive_can_detach);
        line[i++] = g_strdup_printf("    ejectable:                 %d\n", device->drive_is_media_ejectable);
        line[i++] = g_strdup_printf("    media:                     %s\n", device->drive_media ?
                                                    device->drive_media : "" );
        line[i++] = g_strdup_printf("      compat:                  %s\n", device->drive_media_compatibility ? 
                                                    device->drive_media_compatibility : "" );
        if ( device->drive_connection_interface == NULL ||
                                strlen (device->drive_connection_interface) == 0 )
            line[i++] = g_strdup_printf("    interface:                 (unknown)\n");
        else
            line[i++] = g_strdup_printf("    interface:                 %s\n", device->drive_connection_interface);
        if (device->drive_connection_speed == 0)
            line[i++] = g_strdup_printf("    if speed:                  (unknown)\n");
        else
            line[i++] = g_strdup_printf("    if speed:                  %" G_GINT64_FORMAT " bits/s\n", 
                                                    device->drive_connection_speed);
    }
    line[i] = NULL;
    gchar* output = g_strjoinv( NULL, line );
    i = 0;
    while ( line[i] )
        g_free( line[i++] );
    return output;
}

/* ************************************************************************
 * udev & mount monitors
 * ************************************************************************ */

gint cmp_devmounts( devmount_t *a, devmount_t *b )
{
    if ( !a && !b )
        return 0;
    if ( !a || !b )
        return 1;
    if ( a->major == b->major && a->minor == b->minor )
        return 0;
    return 1;
}

void parse_mounts( gboolean report )
{
    gchar *contents;
    gchar **lines;
    GError *error;
    guint n;
//printf("\n@@@@@@@@@@@@@ parse_mounts %s\n\n", report ? "TRUE" : "FALSE" );
    contents = NULL;
    lines = NULL;
    struct udev_device *udevice;
    dev_t dev;
    char* str;

    error = NULL;
    if (!g_file_get_contents ("/proc/self/mountinfo", &contents, NULL, &error))
    {
        g_warning ("Error reading /proc/self/mountinfo: %s", error->message);
        g_error_free (error);
        return;
    }

    // get all mount points for all devices
    GList* newmounts = NULL;
    GList* l;
    GList* changed = NULL;
    devmount_t *devmount;
    
    /* See Documentation/filesystems/proc.txt for the format of /proc/self/mountinfo
    *
    * Note that things like space are encoded as \020.
    * 
    * This file contains lines of the form:
    * 36 35 98:0 /mnt1 /mnt2 rw,noatime master:1 - ext3 /dev/root rw,errors=continue
    * (1)(2)(3)   (4)   (5)      (6)      (7)   (8) (9)   (10)         (11)
    * (1) mount ID:  unique identifier of the mount (may be reused after umount)
    * (2) parent ID:  ID of parent (or of self for the top of the mount tree)
    * (3) major:minor:  value of st_dev for files on filesystem
    * (4) root:  root of the mount within the filesystem
    * (5) mount point:  mount point relative to the process's root
    * (6) mount options:  per mount options
    * (7) optional fields:  zero or more fields of the form "tag[:value]"
    * (8) separator:  marks the end of the optional fields
    * (9) filesystem type:  name of filesystem of the form "type[.subtype]"
    * (10) mount source:  filesystem specific information or "none"
    * (11) super options:  per super block options
    * Parsers should ignore all unrecognised optional fields.
    */
    lines = g_strsplit (contents, "\n", 0);
    for ( n = 0; lines[n] != NULL; n++ )
    {
        guint mount_id;
        guint parent_id;
        guint major, minor;
        gchar encoded_root[PATH_MAX];
        gchar encoded_mount_point[PATH_MAX];
        gchar* mount_point;
        gchar* fstype;
      
        if ( strlen( lines[n] ) == 0 )
            continue;

        if ( sscanf( lines[n],
                  "%d %d %d:%d %s %s",
                  &mount_id,
                  &parent_id,
                  &major,
                  &minor,
                  encoded_root,
                  encoded_mount_point ) != 6 )
        {
            g_warning ("Error reading /proc/self/mountinfo: Error parsing line '%s'", lines[n]);
            continue;
        }

        /* ignore mounts where only a subtree of a filesystem is mounted */
        if ( g_strcmp0( encoded_root, "/" ) != 0 )
            continue;

        mount_point = g_strcompress( encoded_mount_point );
        if ( !mount_point || ( mount_point && mount_point[0] == '\0' ) )
        {
            g_free( mount_point );
            continue;
        }

        // fstype
        fstype = strstr( lines[n], " - " );
        if ( fstype )
        {
            fstype += 3;
            // modifies lines[n]
            if ( str = strchr( fstype, ' ' ) )
                str[0] = '\0';
        }
            
//printf("mount_point(%d:%d)=%s\n", major, minor, mount_point );
        devmount = NULL;
        for ( l = newmounts; l; l = l->next )
        {
            if ( ((devmount_t*)l->data)->major == major && 
                                        ((devmount_t*)l->data)->minor == minor )
            {
                devmount = (devmount_t*)l->data;
                break;
            }
        }
        if ( !devmount )
        {
//printf("     new devmount %s\n", mount_point);
            if ( report )
            {
                devmount = g_slice_new0( devmount_t );
                devmount->major = major;
                devmount->minor = minor;
                devmount->mount_points = NULL;
                devmount->fstype = g_strdup( fstype );
                devmount->mounts = NULL;
                newmounts = g_list_prepend( newmounts, devmount );
            }
            else
            {
                // initial load !report don't add non-block devices
                dev = makedev( major, minor );
                udevice = udev_device_new_from_devnum( udev, 'b', dev );
                if ( udevice )
                {
                    udev_device_unref( udevice );
                    devmount = g_slice_new0( devmount_t );
                    devmount->major = major;
                    devmount->minor = minor;
                    devmount->mount_points = NULL;
                    devmount->fstype = g_strdup( fstype );
                    devmount->mounts = NULL;
                    newmounts = g_list_prepend( newmounts, devmount );
                }
            }
        }

        if ( devmount && !g_list_find( devmount->mounts, mount_point ) )
        {
//printf("    prepended\n");
            devmount->mounts = g_list_prepend( devmount->mounts, mount_point );
        }
        else
            g_free (mount_point);      
    }
    g_free( contents );
    g_strfreev( lines );
//printf("\nLINES DONE\n\n");
    // translate each mount points list to string
    gchar *points, *old_points;
    GList* m;
    for ( l = newmounts; l; l = l->next )
    {
        devmount = (devmount_t*)l->data;
        // Sort the list to ensure that shortest mount paths appear first
        devmount->mounts = g_list_sort( devmount->mounts, (GCompareFunc) g_strcmp0 );
        m = devmount->mounts;
        points = g_strdup( (gchar*)m->data );
        while ( m = m->next )
        {
            old_points = points;
            points = g_strdup_printf( "%s, %s", old_points, (gchar*)m->data );
            g_free( old_points );
        }
        g_list_foreach( devmount->mounts, (GFunc)g_free, NULL );
        g_list_free( devmount->mounts );
        devmount->mounts = NULL;
        devmount->mount_points = points;
//printf( "translate %d:%d %s\n", devmount->major, devmount->minor, points );
    }
    
    // compare old and new lists
    GList* found;
    if ( report )
    {
        for ( l = newmounts; l; l = l->next )
        {
            devmount = (devmount_t*)l->data;
//printf("finding %d:%d\n", devmount->major, devmount->minor );
            found = g_list_find_custom( devmounts, (gconstpointer)devmount,
                                                    (GCompareFunc)cmp_devmounts );
            if ( found )
            {
//printf("    found\n");
                if ( !g_strcmp0( ((devmount_t*)found->data)->mount_points,
                                                        devmount->mount_points ) )
                {
//printf("    freed\n");
                    // no change to mount points, so remove from old list
                    devmount = (devmount_t*)found->data;
                    g_free( devmount->mount_points );
                    g_free( devmount->fstype );
                    devmounts = g_list_remove( devmounts, devmount );
                    g_slice_free( devmount_t, devmount );
                }
            }
            else
            {
                // new mount
//printf("    new mount %d:%d %s\n", devmount->major, devmount->minor, devmount->mount_points );
                devmount_t* devcopy = g_slice_new0( devmount_t );
                devcopy->major = devmount->major;
                devcopy->minor = devmount->minor;
                devcopy->mount_points = g_strdup( devmount->mount_points );
                devcopy->fstype = g_strdup( devmount->fstype );
                devcopy->mounts = NULL;
                changed = g_list_prepend( changed, devcopy );
            }
        }
    }
//printf( "\nREMAINING\n\n");
    // any remaining devices in old list have changed mount status
    for ( l = devmounts; l; l = l->next )
    {
        devmount = (devmount_t*)l->data;
//printf("remain %d:%d\n", devmount->major, devmount->minor );
        if ( report )
        {
            changed = g_list_prepend( changed, devmount );
        }
        else
        {
            g_free( devmount->mount_points );
            g_free( devmount->fstype );
            g_slice_free( devmount_t, devmount );
        }
    }
    g_list_free( devmounts );
    devmounts = newmounts;

    // report
    if ( report && changed )
    {
        VFSVolume* volume;
        char* devnode;
        for ( l = changed; l; l = l->next )
        {
            devnode = NULL;
            devmount = (devmount_t*)l->data;
            dev = makedev( devmount->major, devmount->minor );
            udevice = udev_device_new_from_devnum( udev, 'b', dev );
            if ( udevice )
                devnode = g_strdup( udev_device_get_devnode( udevice ) );
            if ( devnode )
            {
                printf( "mount changed: %s\n", devnode );
                if ( volume = vfs_volume_read_by_device( udevice ) )
                    vfs_volume_device_added( volume, TRUE );  //frees volume if needed
                g_free( devnode );
            }
            else
            {
                // not a block device
                if ( volume = vfs_volume_read_by_mount( devmount->mount_points ) )
                {
                    printf( "network mount changed: %s\n", devmount->mount_points );
                    vfs_volume_device_added( volume, FALSE ); //frees volume if needed
                }
                else
                    vfs_volume_nonblock_removed( devmount->mount_points );
            }
            udev_device_unref( udevice );
            g_free( devmount->mount_points );
            g_free( devmount->fstype );
            g_slice_free( devmount_t, devmount );
        }
        g_list_free( changed );
    }
//printf ( "END PARSE\n");
}

static void free_devmounts()
{
    GList* l;
    devmount_t *devmount;
    
    if ( !devmounts )
        return;
    for ( l = devmounts; l; l = l->next )
    {
        devmount = (devmount_t*)l->data;
        g_free( devmount->mount_points );
        g_free( devmount->fstype );
        g_slice_free( devmount_t, devmount );
    }
    g_list_free( devmounts );
    devmounts = NULL;
}

const char* get_devmount_fstype( int major, int minor )
{
    GList* l;
    
    for ( l = devmounts; l; l = l->next )
    {
        if ( ((devmount_t*)l->data)->major == major &&
                                        ((devmount_t*)l->data)->minor == minor )
            return ((devmount_t*)l->data)->fstype;
    }
    return NULL;
}

static gboolean cb_mount_monitor_watch( GIOChannel *channel, GIOCondition cond,
                                                            gpointer user_data )
{
    if ( cond & ~G_IO_ERR )
        return TRUE;

    //printf ("@@@ /proc/self/mountinfo changed\n");
    parse_mounts( TRUE );

    return TRUE;
}

static gboolean cb_udev_monitor_watch( GIOChannel *channel, GIOCondition cond,
                                                            gpointer user_data )
{

/*
printf("cb_monitor_watch %d\n", channel);
if ( cond & G_IO_IN )
    printf("    G_IO_IN\n");
if ( cond & G_IO_OUT )
    printf("    G_IO_OUT\n");
if ( cond & G_IO_PRI )
    printf("    G_IO_PRI\n");
if ( cond & G_IO_ERR )
    printf("    G_IO_ERR\n");
if ( cond & G_IO_HUP )
    printf("    G_IO_HUP\n");
if ( cond & G_IO_NVAL )
    printf("    G_IO_NVAL\n");

if ( !( cond & G_IO_NVAL ) )
{
    gint fd = g_io_channel_unix_get_fd( channel );
    printf("    fd=%d\n", fd);
    if ( fcntl(fd, F_GETFL) != -1 || errno != EBADF )
    {
        int flags = g_io_channel_get_flags( channel );
        if ( flags & G_IO_FLAG_IS_READABLE )
            printf( "    G_IO_FLAG_IS_READABLE\n");
    }
    else
        printf("    Invalid FD\n");
}
*/
    if ( ( cond & G_IO_NVAL ) ) 
    {
        g_warning( "udev g_io_channel_unref G_IO_NVAL" );
        g_io_channel_unref( channel );
        return FALSE;
    }
    else if ( !( cond & G_IO_IN ) )
    {
        if ( ( cond & G_IO_HUP ) )
        {
        g_warning( "udev g_io_channel_unref !G_IO_IN && G_IO_HUP" );
            g_io_channel_unref( channel );
            return FALSE;
        }
        else
            return TRUE;
    }
    else if ( !( fcntl( g_io_channel_unix_get_fd( channel ), F_GETFL ) != -1
                                                    || errno != EBADF ) )
    {
        // bad file descriptor
        g_warning( "udev g_io_channel_unref BAD_FD" );
        g_io_channel_unref( channel );
        return FALSE;
    }

    struct udev_device *udevice;
    const char *action;
    const char *acted = NULL;
    char* devnode;
    VFSVolume* volume;
    if ( udevice = udev_monitor_receive_device( umonitor ) )
    {
        action = udev_device_get_action( udevice );
        devnode = g_strdup( udev_device_get_devnode( udevice ) );
        if ( action )
        {
            // print action
            if ( !strcmp( action, "add" ) )
                acted = "added:   ";
            else if ( !strcmp( action, "remove" ) )
                acted = "removed: ";
            else if ( !strcmp( action, "change" ) )
                acted = "changed: ";
            else if ( !strcmp( action, "move" ) )
                acted = "moved:   ";
            if ( acted )
                printf( "udev %s%s\n", acted, devnode );

            // add/remove volume
            if ( !strcmp( action, "add" ) || !strcmp( action, "change" ) )
            {
                if ( volume = vfs_volume_read_by_device( udevice ) )
                    vfs_volume_device_added( volume, TRUE );  //frees volume if needed
            }
            else if ( !strcmp( action, "remove" ) )
            {
                char* devnode = g_strdup( udev_device_get_devnode( udevice ) );
                if ( devnode )
                {
                    vfs_volume_device_removed( devnode );
                    g_free( devnode );
                }
            }
            // what to do for move action?
        }
        g_free( devnode );
        udev_device_unref( udevice );
    }
    return TRUE;
}


/* ************************************************************************ */

















#if 0
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
#endif

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
                                                                        "%.0f%s" );
        vfs_file_size_to_string_format( total_size_str,
                                fs_stat.f_frsize * fs_stat.f_blocks, "%.0f%s" );
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
    if ( volume->device_type == DEVICE_TYPE_BLOCK )
    {
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
            {
                if ( g_str_has_prefix( volume->device_file, "/dev/loop" ) )
                    volume->icon = g_strdup_printf( "dev_icon_file" );
                else
                    volume->icon = g_strdup_printf( "dev_icon_internal_mounted" );
            }
            else
                volume->icon = g_strdup_printf( "dev_icon_internal_unmounted" );
        }
    }
    else if ( volume->device_type == DEVICE_TYPE_NETWORK )
        volume->icon = g_strdup_printf( "dev_icon_network" );
    
    // set disp_id using by-id
    if ( volume->device_type == DEVICE_TYPE_BLOCK )
    {
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
    }
    else
        disp_id = g_strdup( volume->udi );
    
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
            vfs_file_size_to_string_format( size_str, volume->size, "%.0f%s" );
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
            vfs_file_size_to_string_format( size_str, volume->size, "%.0f%s" );
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
    else if ( g_str_has_prefix( volume->device_file, "curlftpfs#" ) )
        disp_device = g_strdup( volume->device_file + 10 );
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

VFSVolume* vfs_volume_read_by_device( struct udev_device *udevice )
{   // uses udev to read device parameters into returned volume
    VFSVolume* volume = NULL;

    if ( !udevice )
        return NULL;
    device_t* device = device_alloc( udevice );
    if ( !device_get_info( device ) || !device->devnode
                                || !g_str_has_prefix( device->devnode, "/dev/" ) )
    {
        device_free( device );
        return NULL;
    }
    
    // translate device info to VFSVolume
    volume = g_slice_new0( VFSVolume );
    volume->device_type = DEVICE_TYPE_BLOCK;
    volume->device_file = g_strdup( device->devnode );
    volume->udi = g_strdup( device->device_by_id );
    volume->is_optical = device->device_is_optical_disc;
    volume->is_table = device->device_is_partition_table;
    volume->is_floppy = ( device->drive_media_compatibility
                    && !strcmp( device->drive_media_compatibility, "floppy" ) );
    volume->is_removable = !device->device_is_system_internal;
    volume->requires_eject = device->drive_is_media_ejectable;
    volume->is_mountable = device->device_is_media_available;
    volume->is_audiocd = ( device->device_is_optical_disc
                        && device->optical_disc_num_audio_tracks
                        && atoi( device->optical_disc_num_audio_tracks ) > 0 );
    volume->is_dvd = ( device->drive_media
                                && strstr( device->drive_media, "optical_dvd" ) );
    volume->is_blank = ( device->device_is_optical_disc 
                                && device->optical_disc_is_blank );
    volume->is_mounted = device->device_is_mounted;
    volume->is_user_visible = device->device_presentation_hide ? 
                                !atoi( device->device_presentation_hide ) : TRUE;
    volume->ever_mounted = FALSE;
    volume->open_main_window = NULL;
    volume->nopolicy = device->device_presentation_nopolicy ? 
                        atoi( device->device_presentation_nopolicy ) : FALSE;
    volume->mount_point = NULL;
    if ( device->mount_points && device->mount_points[0] != '\0' )
    {
        char* comma;
        if ( comma = strchr( device->mount_points, ',' ) )
        {
            comma[0] = '\0';
            volume->mount_point = g_strdup( device->mount_points );
            comma[0] = ',';
        }
        else
            volume->mount_point = g_strdup( device->mount_points );
    }
    volume->size = device->device_size;
    volume->label = g_strdup( device->id_label );
    volume->fs_type = g_strdup( device->id_type );
    volume->disp_name = NULL;
    volume->icon = NULL;
    volume->automount_time = 0;
    volume->inhibit_auto = FALSE;

    device_free( device );

    // adjustments
    volume->ever_mounted = volume->is_mounted;
    //if ( volume->is_blank )
    //    volume->is_mountable = FALSE;  //has_media is now used for showing
    if ( volume->is_dvd )
        volume->is_audiocd = FALSE;        

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
    return volume;
}

static gboolean path_is_mounted_mtab( const char* path, char** device_file )
{
    gchar *contents;
    gchar **lines;
    GError *error;
    guint n;
    gboolean ret = FALSE;
    char* str;
    char* file;
    char* point;
    gchar encoded_file[PATH_MAX];
    gchar encoded_point[PATH_MAX];

    if ( !path )
        return FALSE;

    contents = NULL;
    lines = NULL;
    error = NULL;
    if ( !g_file_get_contents( "/proc/mounts", &contents, NULL, NULL ) )
    {
        if ( !g_file_get_contents( "/etc/mtab", &contents, NULL, &error ) )
        {
            g_warning ("Error reading mtab: %s", error->message);
            g_error_free (error);
            return FALSE;
        }
    }
    lines = g_strsplit( contents, "\n", 0 );
    for ( n = 0; lines[n] != NULL; n++ )
    {
        if ( lines[n][0] == '\0' )
            continue;

        if ( sscanf( lines[n],
                  "%s %s ",
                  encoded_file,
                  encoded_point ) != 2 )
        {
            g_warning ("Error parsing mtab line '%s'", lines[n]);
            continue;
        }

        point = g_strcompress( encoded_point );
        if ( !g_strcmp0( point, path ) )
        {
            if ( device_file )
                *device_file = g_strcompress( encoded_file );
            ret = TRUE;
            break;
        }
        g_free( point );
    }
    g_free( contents );
    g_strfreev( lines );
    return ret;
}

int parse_network_url( const char* url, const char* fstype,
                                                        netmount_t** netmount )
{   // returns 0=not a network url  1=valid network url  2=invalid network url
    if ( !url || !netmount )
        return 0;

    int ret = 0;
    char* str;
    char* str2;
    netmount_t* nm = g_slice_new0( netmount_t );
    nm->fstype = NULL;
    nm->host = NULL;
    nm->ip = NULL;
    nm->port = NULL;
    nm->user = NULL;
    nm->pass = NULL;
    nm->path = NULL;

    if ( fstype && ( !strcmp( fstype, "nfs" ) || !strcmp( fstype, "smbfs" ) 
            || !strcmp( fstype, "cifs" ) || !strcmp( fstype, "sshfs" ) 
            || !strcmp( fstype, "nfs4" ) ) )
        ret = 2; //invalid as default response

    char* orig_url = strdup( url );
    char* xurl = orig_url;
    gboolean is_colon = FALSE;
    
    // determine url type
    if ( g_str_has_prefix( xurl, "smb:" ) || g_str_has_prefix( xurl, "smbfs:" ) 
                                               || g_str_has_prefix( xurl, "cifs:" ) 
                                               || g_str_has_prefix( xurl, "//" ) )
    {
        ret = 2;
        // mount [-t smbfs] //host[:<port>]/<path>
        if ( !g_str_has_prefix( xurl, "//" ) )
            is_colon = TRUE;
        if ( fstype && strcmp( fstype, "smbfs" ) && strcmp( fstype, "cifs" ) )
        {
            //wlog( "udevil: error: invalid type '%s' for SMB share - must be cifs or smbfs\n",
            //                                                    fstype, 2 );
            goto _net_free;
        }
        if ( !g_strcmp0( fstype, "smbfs" ) || g_str_has_prefix( xurl, "smbfs:" ) )
            nm->fstype = g_strdup( "smbfs" );
        else
            nm->fstype = g_strdup( "cifs" );
    }
    else if ( g_str_has_prefix( xurl, "nfs:" ) )
    {
        ret = 2;
        is_colon = TRUE;
        if ( fstype && strcmp( fstype, "nfs" ) && strcmp( fstype, "nfs4" ) )
        {
            //wlog( "udevil: error: invalid type '%s' for NFS share - must be nfs or nfs4\n",
            //                                                    fstype, 2 );
            goto _net_free;
        }
        nm->fstype = g_strdup( "nfs" );
    }
    else if ( g_str_has_prefix( xurl, "curlftpfs#" ) )
    {
        ret = 2;
        if ( g_str_has_prefix( xurl, "curlftpfs#ftp:" ) )
            is_colon = TRUE;
        if ( fstype && strcmp( fstype, "curlftpfs" ) )
        {
            //wlog( "udevil: error: invalid type '%s' for curlftpfs share - must be curlftpfs\n",
            //                                                    fstype, 2 );
            goto _net_free;
        }
        nm->fstype = g_strdup( "curlftpfs" );
    }
    else if ( g_str_has_prefix( xurl, "ftp:" ) )
    {
        ret = 2;
        is_colon = TRUE;
        if ( fstype && strcmp( fstype, "ftpfs" ) && strcmp( fstype, "curlftpfs" ) )
        {
            //wlog( "udevil: error: invalid type '%s' for FTP share - must be curlftpfs or ftpfs\n",
            //                                                    fstype, 2 );
            goto _net_free;
        }
        if ( fstype )
            nm->fstype = g_strdup( fstype );
        else
        {
            // detect curlftpfs or ftpfs
            if ( str = g_find_program_in_path( "curlftpfs" ) )
                nm->fstype = g_strdup( "curlftpfs" );
            else
                nm->fstype = g_strdup( "ftpfs" );
            g_free( str );
        }
    }
    else if ( g_str_has_prefix( xurl, "sshfs#" ) )
    {
        ret = 2;
        if ( g_str_has_prefix( xurl, "sshfs#ssh:" )
                            || g_str_has_prefix( xurl, "sshfs#sshfs:" ) 
                            || g_str_has_prefix( xurl, "sshfs#sftp:" ) )
            is_colon = TRUE;
        if ( fstype && strcmp( fstype, "sshfs" ) )
        {
            //wlog( "udevil: error: invalid type '%s' for sshfs share - must be sshfs\n",
            //                                                    fstype, 2 );
            goto _net_free;
        }
        nm->fstype = g_strdup( "sshfs" );
    }
    else if ( g_str_has_prefix( xurl, "ssh:" ) || g_str_has_prefix( xurl, "sshfs:" ) 
                                            || g_str_has_prefix( xurl, "sftp:" ) )
    {
        ret = 2;
        is_colon = TRUE;
        if ( fstype && strcmp( fstype, "sshfs" ) )
        {
            //wlog( "udevil: error: invalid type '%s' for sshfs share - must be sshfs\n",
            //                                                    fstype, 2 );
            goto _net_free;
        }
        nm->fstype = g_strdup( "sshfs" );
    }
    else if ( ( str = strstr( xurl, ":/" ) ) && xurl[0] != ':' && xurl[0] != '/' )
    {
        ret = 2;
        str[0] = '\0';
        if ( strchr( xurl, '@' ) || !g_strcmp0( fstype, "sshfs" ) )
            nm->fstype = g_strdup( "sshfs" );
        else
        {
            // mount [-t nfs] host:/path
            nm->fstype = g_strdup( "nfs" );
            if ( fstype && strcmp( fstype, "nfs" ) && strcmp( fstype, "nfs4" ) )
            {
                //wlog( "udevil: error: invalid type '%s' for NFS share - must be nfs or nfs4\n",
                //                                                    fstype, 2 );
                goto _net_free;
            }
        }
        str[0] = ':';
    }

    if ( ret != 2 )
        goto _net_free;
            
    // parse
    if ( is_colon && ( str = strchr( xurl, ':' ) ) )
    {
        xurl = str + 1;
    }

    while ( xurl[0] == '/' )
        xurl++;
    char* trim_url = g_strdup( xurl );

    // path
    if ( str = strchr( xurl, '/' ) )
    {
        nm->path = g_strdup( str );
        str[0] = '\0';
    }
    // user:pass
    if ( str = strchr( xurl, '@' ) )
    {
        str[0] = '\0';
        if ( str2 = strchr( xurl, ':' ) )
        {
            str2[0] = '\0';
            if ( str2[1] != '\0' )
                nm->pass = g_strdup( str2 + 1 );
        }
        if ( xurl[0] != '\0' )
            nm->user = g_strdup( xurl );
        xurl = str + 1;
    }
    // host:port
    if ( xurl[0] == '[' )
    {
        // ipv6 literal
        if ( str = strchr( xurl, ']' ) )
        {
            str[0] = '\0';
            if ( xurl[1] != '\0' )
                nm->host = g_strdup( xurl + 1 );
            if ( str[1] == ':' && str[2] != '\0' )
                nm->port = g_strdup( str + 1 );
        }
    }
    else if ( xurl[0] != '\0' )
    {
        if ( str = strchr( xurl, ':' ) )
        {
            str[0] = '\0';
            if ( str[1] != '\0' )
                nm->port = g_strdup( str + 1 );
        }
        nm->host = g_strdup( xurl );
    }

    // url
    if ( nm->host )
    {
        if ( !strcmp( nm->fstype, "cifs" ) || !strcmp( nm->fstype, "smbfs" ) )
            nm->url = g_strdup_printf( "//%s%s", nm->host, nm->path ? nm->path : "/" );
        else if ( !strcmp( nm->fstype, "nfs" ) )
            nm->url = g_strdup_printf( "%s:%s", nm->host, nm->path ? nm->path : "/" );
        else if ( !g_strcmp0( nm->fstype, "curlftpfs" ) )
            nm->url = g_strdup_printf( "curlftpfs#ftp://%s%s%s%s%s%s%s%s",
                            nm->user ? nm->user : "",
                            nm->pass ? ":" : "",
                            nm->pass ? nm->pass : "",
                            nm->user || nm->pass ? "@" : "",
                            nm->host,
                            nm->port ? ":" : "",
                            nm->port ? nm->port : "",
                            nm->path ? nm->path : "/" );
        else if ( !g_strcmp0( nm->fstype, "ftpfs" ) )
            nm->url = g_strdup( "none" );
        else if ( !g_strcmp0( nm->fstype, "sshfs" ) )
            nm->url = g_strdup_printf( "sshfs#%s%s%s%s%s:%s%s",
                            nm->user ? nm->user : "",
                            nm->pass ? ":" : "",
                            nm->pass ? nm->pass : "",
                            nm->user || nm->pass ? "@" : "",
                            nm->host,
                            nm->port ? nm->port : "",
                            nm->path ? nm->path : "/" );
        else
            nm->url = g_strdup( trim_url );
    }
    g_free( trim_url );
    g_free( orig_url );

    if ( !nm->host )
    {
        //wlog( "udevil: error: '%s' is not a recognized network url\n", url, 2 );
        goto _net_free;
    }
    
    // check user pass port
    if ( ( nm->user && strchr( nm->user, ' ' ) )
            || ( nm->pass && strchr( nm->pass, ' ' ) )
            || ( nm->port && strchr( nm->port, ' ' ) ) )
    {
        //wlog( "udevil: error: invalid network url\n", fstype, 2 );
        goto _net_free;
    }

/*
    // lookup ip
    if ( !( nm->ip = get_ip( nm->host ) ) || ( nm->ip && nm->ip[0] == '\0' ) )
    {
        wlog( "udevil: error: lookup host '%s' failed\n", nm->host, 2 );
        goto _net_free;
    }
*/
    // valid
    *netmount = nm;
    return 1;

_net_free:
    g_free( nm->url );
    g_free( nm->fstype );
    g_free( nm->host );
    g_free( nm->ip );
    g_free( nm->port );
    g_free( nm->user );
    g_free( nm->pass );
    g_free( nm->path );
    g_slice_free( netmount_t, nm );
    return ret;
}

VFSVolume* vfs_volume_read_by_mount( const char* mount_points )
{   // read a non-block device
    VFSVolume* volume;
    char* str;
    int i;
    struct stat64 statbuf;
    netmount_t *netmount = NULL;

    if ( !mount_points )
        return NULL;

    // get single mount point
    char* point = g_strdup( mount_points );
    if ( str = strchr( point, ',' ) )
        str[0] = '\0';
    g_strstrip( point );
    if ( !( point && point[0] == '/' ) )
        return NULL;

    // get device name
    char* name = NULL;
    if ( !( path_is_mounted_mtab( point, &name ) && name && name[0] != '\0' ) )
        return NULL;

    i = parse_network_url( name, NULL, &netmount );
    if ( i != 1 )
        return NULL;

    // network URL
    volume = g_slice_new0( VFSVolume );
    volume->device_type = DEVICE_TYPE_NETWORK;
    volume->udi = netmount->url;
    volume->label = netmount->host;
    volume->fs_type = netmount->fstype;
    volume->size = 0;
    volume->device_file = name;
    volume->is_mounted = TRUE;
    volume->ever_mounted = TRUE;
    volume->open_main_window = NULL;
    volume->mount_point = point;
    volume->disp_name = NULL;
    volume->icon = NULL;
    volume->automount_time = 0;
    volume->inhibit_auto = FALSE;

    // free unused netmount
    g_free( netmount->ip );
    g_free( netmount->port );
    g_free( netmount->user );
    g_free( netmount->pass );
    g_free( netmount->path );
    g_slice_free( netmount_t, netmount );

    vfs_volume_set_info( volume );
    return volume;
}


#if 0
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
#endif

gboolean vfs_volume_is_automount( VFSVolume* vol )
{   // determine if volume should be automounted or auto-unmounted
    int i, j;
    char* test;
    char* value;
    
    if ( !vol->is_mountable || vol->is_blank || vol->device_type != DEVICE_TYPE_BLOCK )
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

char* vfs_volume_device_info( const char* device_file )
{
    struct stat statbuf;    // skip stat64
    struct udev_device *udevice;
    
    if ( !udev )
        return g_strdup_printf( _("( udev was unavailable at startup )") );
        
    if ( stat( device_file, &statbuf ) != 0 )
    {
        g_printerr ( "Cannot stat device file %s: %m\n", device_file );
        return g_strdup_printf( _("( cannot stat device file )") );
    }
    if (statbuf.st_rdev == 0)
    {
        printf( "Device file %s is not a block device\n", device_file );
        return g_strdup_printf( _("( not a block device )") );
    }
        
    udevice = udev_device_new_from_devnum( udev, 'b', statbuf.st_rdev );
    if ( udevice == NULL )
    {
        printf( "No udev device for device %s (devnum 0x%08x)\n",
                                            device_file, (gint)statbuf.st_rdev );
        return g_strdup_printf( _("( no udev device )") );
    }
    
    device_t *device = device_alloc( udevice );
    if ( !device_get_info( device ) )
        return g_strdup_printf( "" );
    
    char* info = device_show_info( device );
    device_free( device );
    udev_device_unref( udevice );
    return info;
}

char* vfs_volume_device_mount_cmd( const char* device_file, const char* options )
{
    char* command = NULL;
    char* s1;
    const char* cmd = xset_get_s( "dev_mount_cmd" );
    if ( !cmd || ( cmd && cmd[0] == '\0' ) )
    {
        // discovery
        if ( s1 = g_find_program_in_path( "udevil" ) )
        {
            // udevil
            if ( options && options[0] != '\0' )
                command = g_strdup_printf( "%s mount %s -o '%s'",
                                            s1, device_file, options );
            else
                command = g_strdup_printf( "%s mount %s",
                                            s1, device_file );
        }
        else if ( s1 = g_find_program_in_path( "pmount" ) )
        {
            // pmount
            command = g_strdup_printf( "%s %s", s1, device_file );
        }
        else if ( s1 = g_find_program_in_path( "udisksctl" ) )
        {
            // udisks2
            if ( options && options[0] != '\0' )
                command = g_strdup_printf( "%s mount -b %s -o '%s'",
                                            s1, device_file, options );
            else
                command = g_strdup_printf( "%s mount -b %s",
                                            s1, device_file );
        }
        else if ( s1 = g_find_program_in_path( "udisks" ) )
        {
            // udisks1
            if ( options && options[0] != '\0' )
                command = g_strdup_printf( "%s --mount %s --mount-options '%s'",
                                        s1, device_file, options );
            else
                command = g_strdup_printf( "%s --mount %s",
                                        s1, device_file );
        }
        g_free( s1 );
    }
    else
    {
        // user specified
        s1 = replace_string( cmd, "%v", device_file, FALSE );
        command = replace_string( s1, "%o", options, TRUE );
        g_free( s1 );
    }
    return command;
}

char* vfs_volume_device_unmount_cmd( const char* device_file )
{
    char* command = NULL;
    char* s1;
    const char* cmd = xset_get_s( "dev_unmount_cmd" );
    if ( !cmd || ( cmd && cmd[0] == '\0' ) )
    {
        // discovery
        if ( s1 = g_find_program_in_path( "udevil" ) )
        {
            // udevil
            command = g_strdup_printf( "%s umount '%s'", s1, device_file );
        }
        else if ( s1 = g_find_program_in_path( "pumount" ) )
        {
            // pmount
            command = g_strdup_printf( "%s %s", s1, device_file );
        }
        else if ( s1 = g_find_program_in_path( "udisksctl" ) )
        {
            // udisks2
            command = g_strdup_printf( "%s unmount -b %s", s1, device_file );
        }
        else if ( s1 = g_find_program_in_path( "udisks" ) )
        {
            // udisks1
            command = g_strdup_printf( "%s --unmount %s", s1, device_file );
        }
        g_free( s1 );
    }
    else
    {
        // user specified
        command = replace_string( cmd, "%v", device_file, FALSE );
    }
    return command;
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
    const char* fstype = vfs_volume_get_fstype( vol );
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
    command = vfs_volume_device_mount_cmd( vol->device_file, options );
    g_free( options );
    return command;
}

void vfs_volume_exec( VFSVolume* vol, char* command )
{
//printf( "vfs_volume_exec %s %s\n", vol->device_file, command );
    if ( !command || command[0] == '\0' || vol->device_type != DEVICE_TYPE_BLOCK )
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

    printf( _("\nAutoexec: %s\n"), s1 );
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
                                        !vfs_volume_is_automount( vol ) ||
                                        vol->device_type != DEVICE_TYPE_BLOCK )
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
                        printf( _("\nAuto Open Tab for %s in %s\n"), vol->device_file,
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

    char* line = vfs_volume_device_unmount_cmd( vol->device_file );
    if ( line )
    {
        printf( _("\nAuto-Unmount: %s\n"), line );
        g_spawn_command_line_async( line, NULL );
        g_free( line );
    }
    else
        printf( _("\nAuto-Unmount: error: no unmount program available\n") );
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
    if ( line )
    {
        printf( _("\nAutomount: %s\n"), line );
        g_spawn_command_line_async( line, NULL );
        g_free( line );
    }
    else
        printf( _("\nAutomount: error: no mount program available\n") );
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
                {
                    vfs_volume_exec( volume, xset_get_s( "dev_exec_unmount" ) );
                    vfs_volume_special_unmounted( volume->device_file );
                }
                else if ( !was_audiocd && volume->is_audiocd )
                    vfs_volume_autoexec( volume );
                
                //media inserted ?
                if ( !was_mountable && volume->is_mountable )
                    vfs_volume_exec( volume, xset_get_s( "dev_exec_insert" ) );
                
                // media ejected ?
                if ( was_mountable && !volume->is_mountable && volume->is_mounted &&
                            ( volume->is_optical || volume->is_removable ) )
                    unmount_if_mounted( volume->device_file );
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

static gboolean vfs_volume_nonblock_removed( const char* mount_points )
{
    GList* l;
    VFSVolume* volume;
    char* str;
    
    if ( !mount_points )
        return FALSE;
    char* point = g_strdup( mount_points );
    if ( str = strchr( point, ',' ) )
        str[0] = '\0';
    g_strstrip( point );
    if ( !( point && point[0] == '/' ) )
        return FALSE;

    for ( l = volumes; l; l = l->next )
    {
        if ( ((VFSVolume*)l->data)->device_type != DEVICE_TYPE_BLOCK &&
                    !g_strcmp0( ((VFSVolume*)l->data)->mount_point, point ) )
        {
            // remove volume
            printf( "network mount removed: %s\n", point );
            volume = (VFSVolume*)l->data;
            vfs_volume_special_unmounted( volume->device_file );
            volumes = g_list_remove( volumes, volume );
            call_callbacks( volume, VFS_VOLUME_REMOVED );
            vfs_free_volume_members( volume );
            g_slice_free( VFSVolume, volume );
            return TRUE;
        }
    }

    return FALSE;
}

static void vfs_volume_device_removed( char* device_file )
{
    GList* l;
    VFSVolume* volume;
    
    if ( !device_file )
        return;

    for ( l = volumes; l; l = l->next )
    {
        if ( ((VFSVolume*)l->data)->device_type == DEVICE_TYPE_BLOCK &&
                !g_strcmp0( ((VFSVolume*)l->data)->device_file, device_file ) )
        {
            // remove volume
            //printf("remove volume %s\n", device_file );
            volume = (VFSVolume*)l->data;
            vfs_volume_exec( volume, xset_get_s( "dev_exec_remove" ) );
            if ( volume->is_mounted && volume->is_removable )
                unmount_if_mounted( volume->device_file );
            volumes = g_list_remove( volumes, volume );
            call_callbacks( volume, VFS_VOLUME_REMOVED );
            vfs_free_volume_members( volume );
            g_slice_free( VFSVolume, volume );
            return;
        }
    }
}

void unmount_if_mounted( const char* device_file )
{
    if ( !device_file )
        return;
    char* str = vfs_volume_device_unmount_cmd( device_file );
    if ( !str )
        return;
    char* mtab = "/etc/mtab";
    if ( !g_file_test( mtab, G_FILE_TEST_EXISTS ) )
        mtab = "/proc/mounts";
    char* line = g_strdup_printf( "bash -c \"grep -qs '^%s ' %s &>/dev/null && %s &>/dev/null\"",
                                                        device_file, mtab, str );
    g_free( str );
    printf( _("Unmount-If-Mounted: %s\n"), line );
    g_spawn_command_line_async( line, NULL );
    g_free( line );
}

void vfs_volume_special_unmount_all()
{
    GList* l;
    char* line;
    
    for ( l = special_mounts; l; l = l->next )
    {
        if ( l->data )
        {
            line = g_strdup_printf( "udevil umount '%s'", (char*)l->data );
            printf( _("\nAuto-Unmount: %s\n"), line );
            g_spawn_command_line_async( line, NULL );
            g_free( line );
            g_free( l->data );
        }
    }
    g_list_free( special_mounts );
    special_mounts = NULL;
}

void vfs_volume_special_unmounted( const char* device_file )
{
    GList* l;
    
    if ( !device_file )
        return;

    for ( l = special_mounts; l; l = l->next )
    {
        if ( !g_strcmp0( (char*)l->data, device_file ) )
        {
//printf("special_mounts --- %s\n", (char*)l->data );        
            g_free( l->data );
            special_mounts = g_list_remove( special_mounts, l->data );
            return;
        }
    }
}

void vfs_volume_special_mounted( const char* device_file )
{
    GList* l;
    const char* mfile = NULL;
    
    if ( !device_file )
        return;
//printf("vfs_volume_special_mounted %s\n", device_file );
    // is device_file an ISO mount point?  get device file
    if ( !g_str_has_prefix( device_file, "/dev/" ) )
    {
        for ( l = volumes; l; l = l->next )
        {
            if ( ((VFSVolume*)l->data)->device_type == DEVICE_TYPE_BLOCK &&
                    ((VFSVolume*)l->data)->is_mounted &&
                    g_str_has_prefix( ((VFSVolume*)l->data)->device_file, "/dev/loop" ) &&
                    !g_strcmp0( ((VFSVolume*)l->data)->mount_point, device_file ) )
            {
                mfile = ((VFSVolume*)l->data)->device_file;
                break;
            }
        }
    }

    if ( !mfile )
        mfile = device_file;

    for ( l = special_mounts; l; l = l->next )
    {
        if ( !g_strcmp0( (char*)l->data, mfile ) )
            return;
    }
//printf("special_mounts +++ %s\n", mfile );
    special_mounts = g_list_prepend( special_mounts, g_strdup( mfile ) );
}

gboolean on_cancel_inhibit_timer( gpointer user_data )
{
    global_inhibit_auto = FALSE;
    return FALSE;
}

gboolean vfs_volume_init()
{
    struct udev_device *udevice;
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *dev_list_entry;
    VFSVolume* volume;

    // create udev
    udev = udev_new();
    if ( !udev )
    {
        printf( "spacefm: unable to initialize udev\n" );
        return TRUE;
    }

    // read all block mount points
    parse_mounts( FALSE );

    // enumerate devices
    enumerate = udev_enumerate_new( udev );
    if ( enumerate )
    {
        udev_enumerate_add_match_subsystem( enumerate, "block" );
        udev_enumerate_scan_devices( enumerate );
        devices = udev_enumerate_get_list_entry( enumerate );

        udev_list_entry_foreach( dev_list_entry, devices )
        {
            const char *syspath = udev_list_entry_get_name( dev_list_entry );
            udevice = udev_device_new_from_syspath( udev, syspath );
            if ( udevice )
            {
                if ( volume = vfs_volume_read_by_device( udevice ) )
                    vfs_volume_device_added( volume, FALSE ); // frees volume if needed
                udev_device_unref( udevice );
            }
        }
        udev_enumerate_unref(enumerate);
    }

    // enumerate non-block
    parse_mounts( TRUE );

    // start udev monitor
    umonitor = udev_monitor_new_from_netlink( udev, "udev" );
    if ( !umonitor )
    {
        printf( "spacefm: cannot create udev monitor\n" );
        goto finish_;
    }
    if ( udev_monitor_enable_receiving( umonitor ) )
    {
        printf( "spacefm: cannot enable udev monitor receiving\n");
        goto finish_;
    }
    if ( udev_monitor_filter_add_match_subsystem_devtype( umonitor, "block", NULL ) )
    {
        printf( "spacefm: cannot set udev filter\n");
        goto finish_;
    }    

    gint ufd = udev_monitor_get_fd( umonitor );
    if ( ufd == 0 )
    {
        printf( "spacefm: cannot get udev monitor socket file descriptor\n");
        goto finish_;
    }    
    global_inhibit_auto = TRUE; // don't autoexec during startup

    uchannel = g_io_channel_unix_new( ufd );
    g_io_channel_set_flags( uchannel, G_IO_FLAG_NONBLOCK, NULL );
    g_io_channel_set_close_on_unref( uchannel, TRUE );
    g_io_add_watch( uchannel, G_IO_IN | G_IO_HUP, // | G_IO_NVAL | G_IO_ERR,
                                            (GIOFunc)cb_udev_monitor_watch, NULL );

    // start mount monitor
    GError *error = NULL;
    mchannel = g_io_channel_new_file ( "/proc/self/mountinfo", "r", &error );
    if ( mchannel != NULL )
    {
        g_io_channel_set_close_on_unref( mchannel, TRUE );
        g_io_add_watch ( mchannel, G_IO_ERR, (GIOFunc)cb_mount_monitor_watch, NULL );
    }
    else
    {
        free_devmounts();
        printf( "spacefm: error monitoring /proc/self/mountinfo: %s\n", error->message );
        g_error_free (error);
    }

    // do startup automounts
    GList* l;
    for ( l = volumes; l; l = l->next )
        vfs_volume_automount( (VFSVolume*)l->data );

    // start resume autoexec timer
    g_timeout_add_seconds( 3, ( GSourceFunc ) on_cancel_inhibit_timer, NULL );
    
    return TRUE;
finish_:
    if ( umonitor )
    {
        udev_monitor_unref( umonitor );
        umonitor = NULL;
    }
    if ( udev )
    {
        udev_unref( udev );
        udev = NULL;
    }
    return TRUE;
}

gboolean vfs_volume_finalize()
{
    // stop mount monitor
    if ( mchannel )
    {
        g_io_channel_unref( mchannel );
        mchannel = NULL;
    }
    free_devmounts();
    
    // stop udev monitor
    if ( uchannel )
    {
        g_io_channel_unref( uchannel );
        uchannel = NULL;
    }
    if ( umonitor )
    {
        udev_monitor_unref( umonitor );
        umonitor = NULL;
    }
    if ( udev )
    {
        udev_unref( udev );
        udev = NULL;
    }
    
    // free callbacks
    if ( callbacks )
        g_array_free( callbacks, TRUE );

    // free volumes / unmount all ?
    GList* l;
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

    // unmount networks and files mounted during this session
    vfs_volume_special_unmount_all();

    return TRUE;
}

/*
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
*/

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

gboolean vfs_volume_dir_avoid_changes( const char* dir )
{
    // determines if file change detection should be disabled for this
    // dir (eg nfs stat calls block when a write is in progress so file
    // change detection is unwanted)
    // return FALSE to detect changes in this dir, TRUE to avoid change detection
    if ( !udev || !dir )
        return FALSE;

    // canonicalize path
    char buf[ PATH_MAX + 1 ];
    char* canon = realpath( dir, buf );
    if ( !canon )
        return FALSE;
    
    // get devnum
    struct stat stat_buf;   // skip stat64
    if ( stat( canon, &stat_buf ) == -1 )
        return FALSE;
    //printf("    stat_buf.st_dev = %d:%d\n", major(stat_buf.st_dev), minor( stat_buf.st_dev) );
    struct udev_device* udevice = udev_device_new_from_devnum( udev, 'b',
                                                            stat_buf.st_dev );
    if ( !udevice )
    {
        //printf("!udevice %s\n", get_devmount_fstype( major( stat_buf.st_dev ),
        //                                    minor( stat_buf.st_dev ) ));
        // tmpfs is not a block device but we want to detect changes
        const char* fstype = get_devmount_fstype( major( stat_buf.st_dev ),
                                            minor( stat_buf.st_dev ) );
        return !( !g_strcmp0( fstype, "tmpfs" ) || !g_strcmp0( fstype, "ramfs" )
               || !g_strcmp0( fstype, "aufs" )  || !g_strcmp0( fstype, "devtmpfs" )
               || !g_strcmp0( fstype, "overlayfs" ) );
    }
    
    const char* devnode = udev_device_get_devnode( udevice );
    gboolean ret = ( devnode == NULL ); // TRUE if not a block device
    udev_device_unref( udevice );
    return ret;
}

