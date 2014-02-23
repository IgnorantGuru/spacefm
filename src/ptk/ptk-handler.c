/*
 * SpaceFM ptk-handler.c
 * 
 * Copyright (C) 2013-2014 OmegaPhil <OmegaPhil+SpaceFM@gmail.com>
 * Copyright (C) 2014 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>
 * 
 * License: See COPYING file
 * 
*/

#include <glib/gi18n.h>
#include <string.h>
#include <fnmatch.h>

#include "ptk-handler.h"
#include "exo-tree-view.h"
#include "gtk2-compat.h"
#include "item-prop.h" // For get_text_view/load_text_view


// Archive handlers treeview model enum
enum {
    COL_XSET_NAME,
    COL_HANDLER_NAME
};

// Archive creation handlers combobox model enum
enum {
    // COL_XSET_NAME
    COL_HANDLER_EXTENSIONS = 1
};

const char* handler_prefix[] =
{
    "handarc_",
    "handfs_",
    "handnet_"
};

const char* handler_cust_prefix[] =
{
    "custarc_",
    "custfs_",
    "custnet_"
};

const char* handler_conf_xset[] =
{
    "arc_conf2",
    "dev_fs_cnf",
    "dev_net_cnf"
};

const char* dialog_titles[] =
{
    N_("Archive Handlers"),
    N_("Device Handlers"),
    N_("Protocol Handlers")
};

#define MOUNT_EXAMPLE "# Enter mount command or leave blank for auto:\n\n\n# # Examples: (remove # to enable a mount command)\n#\n# # udevil:\n#     udevil mount -o '%o' %v\n#\n# # pmount: (does not accept mount options)\n#     pmount %v\n#\n# # udisks v2:\n#     udisksctl mount -b %v -o '%o'\n#\n# # udisks v1: (enable all three lines!)\n#     fm_udisks=`udisks --mount %v --mount-options '%o' 2>&1`\n#     echo \"$fm_udisks\"\n#     [[ \"$fm_udisks\" = \"${fm_udisks/ount failed:/}\" ]]\n\n"

#define UNMOUNT_EXAMPLE "# Enter unmount command or leave blank for auto:\n\n\n# # Examples: (remove # to enable an unmount command)\n#\n# # udevil:\n#     udevil umount %v\n#\n# # pmount:\n#     pumount %v\n#\n# # udisks v2:\n#     udisksctl unmount -b %v\n#\n# # udisks v1: (enable all three lines!)\n#     fm_udisks=`udisks --unmount %v 2>&1`\n#     echo \"$fm_udisks\"\n#     [[ \"$fm_udisks\" = \"${fm_udisks/ount failed:/}\" ]]\n\n"

#define INFO_EXAMPLE "# Enter command to show device properties or leave blank for auto:\n\n"

typedef struct _Handler
{
    const char* xset_name;      //                   set->name
    const char* handler_name;   //                   set->menu_label
    const char* type;           // or whitelist      set->s
    const char* ext;            // or blacklist      set->x
    const char* compress_cmd;   // or mount          set->y
    const char* extract_cmd;    // or unmount        set->z
    const char* list_cmd;       // or info           set->context
                                // enabled           set->b
} Handler;

/* If you add a new handler, add it to (end of ) existing session file handler
 * list so existing users see the new handler. */
const Handler handlers_arc[]=
{
    /* In compress commands:
    *     %n: First selected filename to archive, or (with %O) a single filename
    *     %N: All selected filenames/directories to archive (standard)
    *     %o: Resulting single archive file
    *     %O: Resulting archive per source file/directory (use changes %n meaning)
    *
    * In extract commands:
    *     %x: Archive to extract
    *     %g: Extract To tarGet dir + optional subfolder
    *     %G: Extract To tarGet dir, never with subfolder
    *
    * In list commands:
    *     %x: Archive to list
    *
    * Plus standard substitution variables are accepted.
    */
    {
        "handarc_7z",
        "7-Zip",
        "application/x-7z-compressed",
        ".7z",
        "+\"$(which 7za || echo 7zr)\" a %o %N",
        "+\"$(which 7za || echo 7zr)\" x %x",
        "+\"$(which 7za || echo 7zr)\" l %x"
    },
    {
        "handarc_gz",
        "Gzip",
        "application/x-gzip application/x-gzpdf application/gzip",
        ".gz",
        "gzip -c %N > %O",
        "gzip -cd %x > %G",
        "+gunzip -l %x"
    },
    {
        "handarc_rar",
        "RAR",
        "application/x-rar",
        ".rar",
        "+rar a -r %o %N",
        "+unrar -o- x %x",
        "+unrar lt %x"
    },
    {
        "handarc_tar",
        "Tar",
        "application/x-tar",
        ".tar",
        "tar -cvf %o %N",
        "tar -xvf %x",
        "+tar -tvf %x"
    },
    {
        "handarc_tar_bz2",
        "Tar bzip2",
        "application/x-bzip-compressed-tar",
        ".tar.bz2",
        "tar -cvjf %o %N",
        "tar -xvjf %x",
        "+tar -tvf %x"
    },
    {
        "handarc_tar_gz",
        "Tar Gzip",
        "application/x-compressed-tar",
        ".tar.gz .tgz",
        "tar -cvzf %o %N",
        "tar -xvzf %x",
        "+tar -tvf %x"
    },
    {
        "handarc_tar_xz",
        "Tar xz",
        "application/x-xz-compressed-tar",
        ".tar.xz .txz",
        "tar -cvJf %o %N",
        "tar -xvJf %x",
        "+tar -tvf %x"
    },
    {
        "handarc_zip",
        "Zip",
        "application/x-zip application/zip",
        ".zip",
        "+zip -r %o %N",
        "+unzip %x",
        "+unzip -l %x"
    }
};

const Handler handlers_fs[]=
{
    /* In commands:
    *      %v  device
    *      %o  volume-specific mount options (use in mount command only)
    *      %a  mount point, or create auto mount point
    *  Plus standard substitution variables are accepted.
    * 
    *  Whitelist/Blacklist: (prefix list element with '+' if required)
    *      fstype (eg ext3)
    *      dev=DEVICE (/dev/sdd1)
    *      id=UDI
    *      label=VOLUME_LABEL (includes spaces as underscores)
    *      point=MOUNT_POINT
    *      audiocd=0 or 1
    *      optical=0 or 1
    *      removable=0 or 1
    *      mountable=0 or 1
    *      
    *      eg: +ext3 dev=/dev/sdb* id=ata-* label=Label_With_Spaces
    */
    {
        "handfs_fuseiso",
        "fuseiso",
        "file *fuseiso",
        "",
        "fuseiso %v %a",
        "fusermount -u %a",
        "grep \"%a\" ~/.mtab.fuseiso"
    },
    {
        "handfs_udiso",
        "udevil iso",
        "file iso9660",
        "",
        "uout=\"$(udevil mount %v)\"\nerr=$?; echo \"$uout\"\n[[ $err -ne 0 ]] && exit 1\npoint=\"${uout#Mounted }\"\n[[ \"$point\" = \"$uout\" ]] && exit 0\npoint=\"${point##* at }\"\n[[ -d \"$point\" ]] && spacefm \"$point\" &\n",
        "# Note: non-iso9660 types will fall through to Default unmount handler\nudevil umount %a\n",
        "mount | grep \"%a\""
    },
    {
        "handfs_def",
        "Default",
        "*",
        "",
        MOUNT_EXAMPLE,
        UNMOUNT_EXAMPLE,
        INFO_EXAMPLE
    }
};

const Handler handlers_net[]=
{
    /* In commands:
    *       %url%     $fm_url
    *       %proto%   $fm_url_proto
    *       %host%    $fm_url_host
    *       %port%    $fm_url_port
    *       %user%    $fm_url_user
    *       %pass%    $fm_url_pass
    *       %path%    $fm_url_path
    *       %a        mount point, or create auto mount point
    *                 $fm_mtab_fs   (mounted mtab fs type)
    *                 $fm_mtab_url  (mounted mtab url)
    *
    *  Whitelist/Blacklist: (prefix list element with '+' if required)
    *      protocol (eg ssh)
    *      url=URL (ssh://...)
    *      mtab_fs=TYPE    (mounted mtab fs type)
    *      mtab_url=URL    (mounted mtab url)
    *      host=HOSTNAME
    *      user=USERNAME
    *      point=MOUNT_POINT
    *      
    *      eg: +ssh url=ssh://*
    */
    {
        "handnet_ftp",
        "ftp",
        "ftp",
        "",
        "+options=\"nonempty\"\nif [ -n \"%user%\" ]; then\n    user=\",user=%user%\"\n    [[ -n \"%pass%\" ]] && user=\"$user:%pass%\"\nfi\n[[ -n \"%port%\" ]] && portcolon=:\necho \">>> curlftpfs -o $options$user ftp://%host%$portcolon%port%%path% %a\"\necho\ncurlftpfs -o $options$user ftp://%host%$portcolon%port%%path% \"%a\"\nerr=$?\nsleep 1  # required to prevent disconnect on too fast terminal close\n[[ $err -eq 0 ]]  # set error status\n",
        "fusermount -u \"%a\"",
        "mount | grep \"%a\""
    },
    {
        "handnet_ssh",
        "ssh",
        "ssh mtab_fs=fuse.sshfs",
        "",
        "+[[ -n \"$fm_url_user\" ]] && fm_url_user=\"${fm_url_user}@\"\n[[ -z \"$fm_url_port\" ]] && fm_url_port=22\necho \">>> sshfs -p $fm_url_port $fm_url_user$fm_url_host:$fm_url_path %a\"\necho\n# Run sshfs through nohup to prevent disconnect on terminal close\nsshtmp=\"$(mktemp --tmpdir spacefm-ssh-output-XXXXXXXX.tmp)\" || exit 1\nnohup sshfs -p $fm_url_port $fm_url_user$fm_url_host:$fm_url_path %a &> \"$sshtmp\"\nerr=$?\n[[ -e \"$sshtmp\" ]] && cat \"$sshtmp\" ; rm -f \"$sshtmp\"\n[[ $err -eq 0 ]]  # set error status\n\n# Alternate Method - if enabled, disable nohup line above and\n#                    uncheck Run In Terminal\n# # Run sshfs in a terminal without SpaceFM task.  sshfs disconnects when the\n# # terminal is closed\n# spacefm -s run-task cmd --terminal \"echo 'Connecting to $fm_url'; echo; sshfs -p $fm_url_port $fm_url_user$fm_url_host:$fm_url_path %a; if [ $? -ne 0 ]; then echo; echo '[ Finished ] Press Enter to close'; else echo; echo 'Press Enter to close (closing this window may unmount sshfs)'; fi; read\" & sleep 1\n",
        "fusermount -u %a",
        "mount | grep \"%a\""
    },
    {
        "handnet_udevil",
        "udevil",
        "ftp http https nfs smb ssh mtab_fs=fuse*",
        "",
        "udevil mount \"$fm_url\"",
        "udevil umount \"%a\"",
        "mount | grep \"%a\""
    }
};


// Function prototypes
static void on_configure_handler_enabled_check( GtkToggleButton *togglebutton,
                                                gpointer user_data );
static void restore_defaults( GtkWidget* dlg, gboolean all );
static gboolean validate_handler( GtkWidget* dlg, int mode );

gboolean ptk_handler_values_in_list( const char* list, GSList* values,
                                     char** msg )
{   /* test for the presence of values in list, using wildcards.
    *  list is space or comma separated, plus indicates required. */
    if ( !( list && list[0] ) || !values )
        return FALSE;
    
    if ( msg )
        *msg = NULL;

    // get elements of list
    gchar** elements = g_strsplit_set( list, " ,", 0 );
    if ( !elements )
        return FALSE;
    
    // test each element for match
    int i;
    GSList* l;
    char* element;
    char* ret_msg = NULL;
    char* str;
    gboolean required, match;
    gboolean ret = FALSE;
    for ( i = 0; elements[i]; i++ )
    {
        if ( !elements[i][0] )
            continue;
        if ( elements[i][0] == '+' )
        {
            // plus prefix indicates this element is required
            element = elements[i] + 1;
            required = TRUE;
        }
        else
        {
            element =  elements[i];
            required = FALSE;
        }
        if ( msg )
            match = FALSE;
        for ( l = values; l; l = l->next )
        {
            if ( fnmatch( element, (char*)l->data, 0 ) == 0 )
            {
                // match
                ret = match = TRUE;
            }
            else if ( required )
            {
                // no match of required
                g_strfreev( elements );
                g_free( ret_msg );
                return FALSE;
            }
        }
        if ( msg )
        {
            str = ret_msg;
            ret_msg = g_strdup_printf( "%s%s%s%s%s", ret_msg ? ret_msg : "",
                                        ret_msg ? " " : "",
                                        match ? "[" : "",
                                        elements[i],
                                        match ? "]" : "" );
            g_free( str );
        }
    }
    g_strfreev( elements );
    if ( ret && msg )
        *msg = ret_msg;
    else
        g_free( ret_msg );
    return ret;    
}

gboolean ptk_handler_equals_default( XSet* set )
{   // determine if set is duplicate of default handler
    const Handler* handler;
    int i, nelements, mode;
    
    if ( set->b != XSET_B_TRUE )
        // handler not enabled so not equal default
        return FALSE;
    
    if ( g_str_has_prefix( set->name, handler_prefix[HANDLER_MODE_ARC] ) )
    {
        mode = HANDLER_MODE_ARC;
        nelements = G_N_ELEMENTS( handlers_arc );
    }
    else if ( g_str_has_prefix( set->name, handler_prefix[HANDLER_MODE_FS] ) )
    {
        mode = HANDLER_MODE_FS;
        nelements = G_N_ELEMENTS( handlers_fs );
    }
    else if ( g_str_has_prefix( set->name, handler_prefix[HANDLER_MODE_NET] ) )
    {
        mode = HANDLER_MODE_NET;
        nelements = G_N_ELEMENTS( handlers_net );
    }
    else
        return FALSE;

    for ( i = 0; i < nelements; i++ )
    {
        if ( mode == HANDLER_MODE_ARC )
            handler = &handlers_arc[i];
        else if ( mode == HANDLER_MODE_FS )
            handler = &handlers_fs[i];
        else if ( mode == HANDLER_MODE_NET )
            handler = &handlers_net[i];

        if ( !strcmp( handler->xset_name, set->name ) )
        {
            // found default handler - test equal
            if ( !g_strcmp0( handler->handler_name, set->menu_label ) &&
                 !g_strcmp0( handler->type, set->s ) &&
                 !g_strcmp0( handler->ext, set->x ) &&
                 !g_strcmp0( handler->compress_cmd, set->y ) &&
                 !g_strcmp0( handler->extract_cmd, set->z ) &&
                 !g_strcmp0( handler->list_cmd, set->context ) )
                return TRUE;
            else
                return FALSE;
        }
    }
    return FALSE;
}

void ptk_handler_add_defaults( int mode, gboolean overwrite,
                                         gboolean add_missing )
{
    int i, nelements;
    char* list;
    char* str;
    XSet* set;
    XSet* set_conf;
    const Handler* handler;
    
    if ( mode == HANDLER_MODE_ARC )
        nelements = G_N_ELEMENTS( handlers_arc );
    else if ( mode == HANDLER_MODE_FS )
        nelements = G_N_ELEMENTS( handlers_fs );
    else if  ( mode == HANDLER_MODE_NET )
        nelements = G_N_ELEMENTS( handlers_net );
    else
        return;
    set_conf = xset_get( handler_conf_xset[mode] );
    list = g_strdup( set_conf->s );

    if ( !list )
    {
        // create default list - eg sets arc_conf2 ->s
        list = g_strdup( "" );
        overwrite = add_missing = TRUE;
    }
    /* disabled to allow loading of non-saved default handlers on start
    else if ( !overwrite && !add_missing )
    {
        g_free( list );
        return;
    }
    */
    
    for ( i = 0; i < nelements; i++ )
    {
        if ( mode == HANDLER_MODE_ARC )
            handler = &handlers_arc[i];
        else if ( mode == HANDLER_MODE_FS )
            handler = &handlers_fs[i];
        else if ( mode == HANDLER_MODE_NET )
            handler = &handlers_net[i];

        if ( add_missing && !strstr( list, handler->xset_name ) )
        {
            // add a missing default handler to the list
            str = list;
            list = g_strconcat( list, list[0] ? " " : "",
                                            handler->xset_name, NULL );
            g_free( str );
        }
        if ( add_missing || strstr( list, handler->xset_name ) )
        {
            set = xset_is( handler->xset_name );
            if ( !set || overwrite )
            {
                // create xset if missing
                if ( !set )
                    set = xset_get( handler->xset_name );
                // set handler values to defaults
                string_copy_free( &set->menu_label, handler->handler_name );
                string_copy_free( &set->s, handler->type );
                string_copy_free( &set->x, handler->ext );
                string_copy_free( &set->y, handler->compress_cmd );   // or mount
                string_copy_free( &set->z, handler->extract_cmd );    // or unmount
                string_copy_free( &set->context, handler->list_cmd ); // or info
                set->b = XSET_B_TRUE;
                // note: xset menu_labels are not saved unless !lock
                set->lock = FALSE;
                // handler equals default, so don't save in session
                set->disable = TRUE;
            }
        }
    }
    // update handler list
    g_free( set_conf->s );
    set_conf->s = list;
}

static XSet* add_new_handler( int mode )
{
    // creates a new xset for a custom handler type
    XSet* set;
    char* rand;
    char* name = NULL;

    // get a unique new xset name
    do
    {
        g_free( name );
        rand = randhex8();
        name = g_strconcat( handler_cust_prefix[mode], rand, NULL );
        g_free( rand );
    }
    while ( xset_is( name ) );

    // create and return the xset
    set = xset_get( name );
    g_free( name );
    return set;
}

// handler_xset_name optional if handler_xset passed
static void config_load_handler_settings( XSet* handler_xset,
                                          gchar* handler_xset_name,
                                          GtkWidget* dlg )
{
    // Fetching actual xset if only the name has been passed
    if ( !handler_xset )
        handler_xset = xset_get( handler_xset_name );

/*igcr this code is repeated in several places in this file.  Would be more
 * efficient to create a struct and just pass that (or set it as a single
 * object) - see ptk-file-misc.c MoveSet typedef */
    // Fetching widget references
    GtkWidget* chkbtn_handler_enabled = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "chkbtn_handler_enabled" );
    GtkWidget* entry_handler_name = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "entry_handler_name" );
    GtkWidget* entry_handler_mime = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "entry_handler_mime" );
    GtkWidget* entry_handler_extension = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "entry_handler_extension" );
    GtkWidget* view_handler_compress = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "view_handler_compress" );
    GtkWidget* view_handler_extract = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "view_handler_extract" );
    GtkWidget* view_handler_list = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "view_handler_list" );
    GtkWidget* chkbtn_handler_compress_term = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "chkbtn_handler_compress_term" );
    GtkWidget* chkbtn_handler_extract_term = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "chkbtn_handler_extract_term" );
    GtkWidget* chkbtn_handler_list_term = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "chkbtn_handler_list_term" );
    GtkWidget* btn_remove = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "btn_remove" );
    GtkWidget* btn_apply = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "btn_apply" );
    GtkWidget* btn_defaults0 = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "btn_defaults0" );
    int mode = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( dlg ),
                                            "dialog_mode" ) );

    /* At this point a handler exists, so making remove and apply buttons
     * sensitive as well as the enabled checkbutton */
    gtk_widget_set_sensitive( GTK_WIDGET( btn_remove ), TRUE );
    gtk_widget_set_sensitive( GTK_WIDGET( btn_apply ), TRUE );
    gtk_widget_set_sensitive( GTK_WIDGET( chkbtn_handler_enabled ), TRUE );
    gtk_widget_set_sensitive( GTK_WIDGET( btn_defaults0 ),
                g_str_has_prefix( handler_xset->name, handler_prefix[mode] ) );

    /* Configuring widgets with handler settings. Only name, MIME and
     * extension warrant a warning
     * Commands are prefixed with '+' when they are ran in a terminal */
    gboolean check_value = handler_xset->b != XSET_B_TRUE ? FALSE : TRUE;
    int start;
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_enabled ),
                                    check_value );
    gtk_entry_set_text( GTK_ENTRY( entry_handler_name ),
                                    handler_xset->menu_label ?
                                    handler_xset->menu_label : "" );
    gtk_entry_set_text( GTK_ENTRY( entry_handler_mime ),
                                    handler_xset->s ?
                                    handler_xset->s : "" );
    gtk_entry_set_text( GTK_ENTRY( entry_handler_extension ),
                                    handler_xset->x ?
                                    handler_xset->x : "" );
/*igtodo code review g_strdup leaks below this line */
    if (!handler_xset->y)
    {
        load_text_view( GTK_TEXT_VIEW( view_handler_compress ), "" );
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_compress_term ),
                                        FALSE);
    }
    else
    {
        if ( handler_xset->y[0] == '+' )
        {
            load_text_view( GTK_TEXT_VIEW( view_handler_compress ),
                            handler_xset->y + 1 );
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_compress_term ),
                                            TRUE);
        }
        else
        {
            load_text_view( GTK_TEXT_VIEW( view_handler_compress ),
                            handler_xset->y );
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_compress_term ),
                                            FALSE);
        }
    }
    if (!handler_xset->z)
    {
        load_text_view( GTK_TEXT_VIEW( view_handler_extract ), "" );
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_extract_term ),
                                        FALSE);
    }
    else
    {
        if ( handler_xset->z[0] == '+' )
        {
            load_text_view( GTK_TEXT_VIEW( view_handler_extract ),
                            handler_xset->z + 1 );
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_extract_term ),
                                            TRUE);
        }
        else
        {
            load_text_view( GTK_TEXT_VIEW( view_handler_extract ),
                            handler_xset->z );
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_extract_term ),
                                            FALSE);
        }
    }
    if (!handler_xset->context)
    {
        load_text_view( GTK_TEXT_VIEW( view_handler_list ), "" );
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_list_term ),
                                        FALSE);
    }
    else
    {
        if ( handler_xset->context[0] == '+' )
        {
            load_text_view( GTK_TEXT_VIEW( view_handler_list ),
                            handler_xset->context + 1 );
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_list_term ),
                                            TRUE);
        }
        else
        {
            load_text_view( GTK_TEXT_VIEW( view_handler_list ),
                                           handler_xset->context );
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_list_term ),
                                            FALSE);
        }
    }
}

static void config_unload_handler_settings( GtkWidget* dlg )
{
    // Fetching widget references
    GtkWidget* chkbtn_handler_enabled = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "chkbtn_handler_enabled" );
    GtkWidget* entry_handler_name = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "entry_handler_name" );
    GtkWidget* entry_handler_mime = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "entry_handler_mime" );
    GtkWidget* entry_handler_extension = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "entry_handler_extension" );
    GtkWidget* view_handler_compress = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "view_handler_compress" );
    GtkWidget* view_handler_extract = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "view_handler_extract" );
    GtkWidget* view_handler_list = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "view_handler_list" );
    GtkWidget* chkbtn_handler_compress_term = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "chkbtn_handler_compress_term" );
    GtkWidget* chkbtn_handler_extract_term = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "chkbtn_handler_extract_term" );
    GtkWidget* chkbtn_handler_list_term = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "chkbtn_handler_list_term" );
    GtkWidget* btn_remove = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "btn_remove" );
    GtkWidget* btn_apply = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "btn_apply" );
    GtkWidget* btn_defaults0 = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "btn_defaults0" );

    // Disabling main change buttons
    gtk_widget_set_sensitive( GTK_WIDGET( btn_remove ), FALSE );
    gtk_widget_set_sensitive( GTK_WIDGET( btn_apply ), FALSE );
    gtk_widget_set_sensitive( GTK_WIDGET( btn_defaults0 ), FALSE );

    // Unchecking handler
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_enabled ),
                                  FALSE);

    // Resetting all widgets
    gtk_entry_set_text( GTK_ENTRY( entry_handler_name ), g_strdup( "" ) );
    gtk_entry_set_text( GTK_ENTRY( entry_handler_mime ), g_strdup( "" ) );
    gtk_entry_set_text( GTK_ENTRY( entry_handler_extension ), g_strdup( "" ) );
    load_text_view( GTK_TEXT_VIEW( view_handler_compress ), "" );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_compress_term ),
                                  FALSE);
    load_text_view( GTK_TEXT_VIEW( view_handler_extract ), "" );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_extract_term ),
                                  FALSE);
    load_text_view( GTK_TEXT_VIEW( view_handler_list ), "" );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( chkbtn_handler_list_term ),
                                  FALSE);
}

static void populate_archive_handlers( GtkListStore* list, GtkWidget* dlg )
{
    /* Fetching available archive handlers (literally gets member s from
     * the xset) - user-defined order has already been set */
    int mode = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( dlg ),
                                            "dialog_mode" ) );
    char* archive_handlers_s = xset_get_s( handler_conf_xset[mode] );

    // Making sure archive handlers are available
    if ( !archive_handlers_s )
        return;

/*igcr copying all these strings is inefficient, just need to parse */
    gchar** archive_handlers = g_strsplit( archive_handlers_s, " ", -1 );

    // Debug code
    //g_message("archive_handlers_s: %s", archive_handlers_s);

    // Looping for handlers (NULL-terminated list)
    GtkTreeIter iter;
    int i;
    for (i = 0; archive_handlers[i] != NULL; ++i)
    {
        // Ignore invalid handler xset names
        if ( g_str_has_prefix( archive_handlers[i], handler_prefix[mode] ) ||
             g_str_has_prefix( archive_handlers[i], handler_cust_prefix[mode] ) )
        {
            // Obtaining appending iterator for treeview model
            gtk_list_store_append( GTK_LIST_STORE( list ), &iter );

            // Fetching handler
            XSet* handler_xset = xset_get( archive_handlers[i] );

            // Adding handler to model
/*igcr memory leak - don't copy these strings, just pass them */
            gchar* handler_name = g_strdup( handler_xset->menu_label );
            gchar* xset_name = g_strdup( archive_handlers[i] );
            gtk_list_store_set( GTK_LIST_STORE( list ), &iter,
                                COL_XSET_NAME, xset_name,
                                COL_HANDLER_NAME, handler_name,
                                -1 );
        }
    }

    // Clearing up archive_handlers
    g_strfreev( archive_handlers );

    // Fetching selection from treeview
    GtkWidget *view_handlers = (GtkWidget*)g_object_get_data(
                                    G_OBJECT( dlg ), "view_handlers" );
    GtkTreeSelection* selection;
    selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( view_handlers ) );

    /* Loading first archive handler if there is one and no selection is
     * present */
    if ( i > 0 &&
        !gtk_tree_selection_get_selected( GTK_TREE_SELECTION( selection ),
         NULL, NULL ) )
    {
        GtkTreePath* new_path = gtk_tree_path_new_first();
        gtk_tree_selection_select_path( GTK_TREE_SELECTION( selection ),
                                new_path );
        gtk_tree_path_free( new_path );
        new_path = NULL;
    }
}

static void on_configure_button_press( GtkButton* widget, GtkWidget* dlg )
{
    int i;

    // Fetching widgets and handler details
    GtkButton* btn_add = (GtkButton*)g_object_get_data( G_OBJECT( dlg ),
                                            "btn_add" );
    GtkButton* btn_apply = (GtkButton*)g_object_get_data( G_OBJECT( dlg ),
                                            "btn_apply" );
    GtkButton* btn_remove = (GtkButton*)g_object_get_data( G_OBJECT( dlg ),
                                            "btn_remove" );
    GtkTreeView* view_handlers = (GtkTreeView*)g_object_get_data( G_OBJECT( dlg ),
                                            "view_handlers" );
    GtkWidget* chkbtn_handler_enabled = (GtkWidget*)g_object_get_data(
                                            G_OBJECT( dlg ),
                                            "chkbtn_handler_enabled" );
    GtkWidget* entry_handler_name = (GtkWidget*)g_object_get_data(
                                            G_OBJECT( dlg ),
                                            "entry_handler_name" );
    GtkWidget* entry_handler_mime = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "entry_handler_mime" );
    GtkWidget* entry_handler_extension = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "entry_handler_extension" );
    GtkWidget* view_handler_compress = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "view_handler_compress" );
    GtkWidget* view_handler_extract = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "view_handler_extract" );
    GtkWidget* view_handler_list = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "view_handler_list" );
    GtkWidget* chkbtn_handler_compress_term = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "chkbtn_handler_compress_term" );
    GtkWidget* chkbtn_handler_extract_term = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "chkbtn_handler_extract_term" );
    GtkWidget* chkbtn_handler_list_term = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                            "chkbtn_handler_list_term" );
    int mode = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( dlg ),
                                            "dialog_mode" ) );

    const gchar* handler_name = gtk_entry_get_text( GTK_ENTRY ( entry_handler_name ) );
    const gchar* handler_mime = gtk_entry_get_text( GTK_ENTRY ( entry_handler_mime ) );
    const gchar* handler_extension = gtk_entry_get_text(
                        GTK_ENTRY ( entry_handler_extension ) );
    const gboolean handler_compress_term = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON ( chkbtn_handler_compress_term ) );
    const gboolean handler_extract_term = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON ( chkbtn_handler_extract_term ) );
    const gboolean handler_list_term = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON ( chkbtn_handler_list_term ) );
    gchar* handler_compress, *handler_extract, *handler_list;

    /* Commands are prefixed with '+' when they are to be ran in a
     * terminal */
    if (handler_compress_term)
    {
        handler_compress = g_strconcat( "+",
            get_text_view( GTK_TEXT_VIEW ( view_handler_compress ) ),
            NULL );
    }
    else handler_compress = get_text_view( GTK_TEXT_VIEW ( view_handler_compress ) );

    if (handler_extract_term)
    {
        handler_extract = g_strconcat( "+",
            get_text_view( GTK_TEXT_VIEW ( view_handler_extract ) ),
            NULL );
    }
    else handler_extract = get_text_view( GTK_TEXT_VIEW ( view_handler_extract ) );

    if (handler_list_term)
    {
        handler_list = g_strconcat( "+",
            get_text_view( GTK_TEXT_VIEW ( view_handler_list ) ),
            NULL );
    }
    else handler_list = get_text_view( GTK_TEXT_VIEW ( view_handler_list ) );

    // Fetching the model and iter from the selection
    GtkTreeIter it;
    GtkTreeModel* model;
    gchar* handler_name_from_model = NULL;  // Used to detect renames
    gchar* xset_name = NULL;
    XSet* handler_xset = NULL;

    // Fetching selection from treeview
    GtkTreeSelection* selection;
    selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( view_handlers ) );

    // Getting selection fails if there are no handlers
    if ( gtk_tree_selection_get_selected( GTK_TREE_SELECTION( selection ),
         &model, &it ) )
    {
        // Succeeded - handlers present
        // Fetching data from the model based on the iterator. Note that
        // this variable used for the G_STRING is defined on the stack,
        // so should be freed for me
/*igcr  memory leak - xset_name and handler_name_from_model must be freed
 * by you.  See https://developer.gnome.org/gtk3/stable/GtkTreeModel.html#gtk-tree-model-get */
        gtk_tree_model_get( model, &it,
                            COL_XSET_NAME, &xset_name,
                            COL_HANDLER_NAME, &handler_name_from_model,
                            -1 );

        // Fetching the xset now I have the xset name
        handler_xset = xset_is(xset_name);

        // Making sure it has been fetched
        if (!handler_xset)
        {
            g_warning("Unable to fetch the xset for the archive handler '%s'",
                                                            handler_name);
            goto _clean_exit;
        }
    }

    if ( widget == btn_add )
    {
        // Exiting if there is no handler to add
        if ( !( handler_name && handler_name[0] ) )
            goto _clean_exit;

        // Adding new handler as a copy of the current active handler
        XSet* new_handler_xset = add_new_handler( mode );
        new_handler_xset->b = gtk_toggle_button_get_active(
                                GTK_TOGGLE_BUTTON( chkbtn_handler_enabled )
                              ) ? XSET_B_TRUE : XSET_B_FALSE;

        xset_set_set( new_handler_xset, "label", handler_name );
        xset_set_set( new_handler_xset, "s", handler_mime );  // Mime Type(s)
        xset_set_set( new_handler_xset, "x", handler_extension );  // Extension(s)
        xset_set_set( new_handler_xset, "y", handler_compress );  // Compress command
        xset_set_set( new_handler_xset, "z", handler_extract );  // Extract command
        xset_set_set( new_handler_xset, "cxt", handler_list );  // List command

        // Fetching list store
/*igcr jfyi shouldn't need an object for this - can get list store from list */
        GtkListStore* list = (GtkListStore*)g_object_get_data( G_OBJECT( dlg ), "list" );

        // Obtaining appending iterator for treeview model
        GtkTreeIter iter;
        gtk_list_store_append( GTK_LIST_STORE( list ), &iter );

        // Adding handler to model
/*igcr you don't need to copy these two strings, just pass them */
        gchar* new_handler_name = g_strdup( handler_name );
        gchar* new_xset_name = g_strdup( new_handler_xset->name );
        gtk_list_store_set( GTK_LIST_STORE( list ), &iter,
                            COL_XSET_NAME, new_xset_name,
                            COL_HANDLER_NAME, new_handler_name,
                            -1 );

        // Updating available archive handlers list
        if (g_strcmp0( xset_get_s( handler_conf_xset[mode] ), "" ) <= 0)
        {
            // No handlers present - adding new handler
            xset_set( handler_conf_xset[mode], "s", new_xset_name );
        }
        else
        {
            // Adding new handler to handlers
            gchar* new_handlers_list = g_strdup_printf( "%s %s",
                                        xset_get_s( handler_conf_xset[mode] ),
                                        new_xset_name );
            xset_set( handler_conf_xset[mode], "s", new_handlers_list );

            // Clearing up
            g_free(new_handlers_list);
        }

        // Clearing up
        g_free(new_handler_name);
        g_free(new_xset_name);

        // Activating the new handler - the normal loading code
        // automatically kicks in
        GtkTreePath* new_handler_path = gtk_tree_model_get_path( GTK_TREE_MODEL( model ),
                                                                &iter );
        gtk_tree_view_set_cursor( GTK_TREE_VIEW( view_handlers ),
                                        new_handler_path, NULL, FALSE );
        gtk_tree_path_free( new_handler_path );

        // Making sure the remove and apply buttons are sensitive
        gtk_widget_set_sensitive( GTK_WIDGET( btn_remove ), TRUE );
        gtk_widget_set_sensitive( GTK_WIDGET( btn_apply ), TRUE );

        /* Validating - remember that IG wants the handler to be saved
         * even with invalid commands */
        validate_handler( dlg, mode );
    }
    else if ( widget == btn_apply )
    {
        // Exiting if apply has been pressed when no handlers are present
        if (xset_name == NULL) goto _clean_exit;

        /* Validating - remember that IG wants the handler to be saved
         * even with invalid commands */
        validate_handler( dlg, mode );

        // Determining current handler enabled state
        gboolean handler_enabled = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON( chkbtn_handler_enabled ) ) ?
            XSET_B_TRUE : XSET_B_FALSE;

        // Checking if the handler has been renamed
        if (g_strcmp0( handler_name_from_model, handler_name ) != 0)
        {
            // It has - updating model
            gtk_list_store_set( GTK_LIST_STORE( model ), &it,
                        COL_XSET_NAME, xset_name,
                        COL_HANDLER_NAME, handler_name,
                        -1 );
        }

        // Saving archive handler
        handler_xset->b = handler_enabled;
        xset_set_set( handler_xset, "label", handler_name );
        xset_set_set( handler_xset, "s", handler_mime );
        xset_set_set( handler_xset, "x", handler_extension );
        xset_set_set( handler_xset, "y", handler_compress );
        xset_set_set( handler_xset, "z", handler_extract );
        xset_set_set( handler_xset, "cxt", handler_list );
        // prevent saving of default handlers later in session
        handler_xset->disable = ptk_handler_equals_default( handler_xset );
    }
    else if ( widget == btn_remove )
    {
        // Exiting if remove has been pressed when no handlers are present
        if (xset_name == NULL) goto _clean_exit;

        // Updating available archive handlers list - fetching current
        // handlers
        const char* archive_handlers_s = xset_get_s( handler_conf_xset[mode] );
/*igcr also inefficient to copy all these strings  - although may be fast
 * enough for this function - could use strstr to find deleted handler */
        gchar** archive_handlers = archive_handlers_s ? 
                                   g_strsplit( archive_handlers_s, " ", -1 ) :
                                   NULL;
        gchar* new_archive_handlers_s = g_strdup( "" );
        gchar* new_archive_handlers_s_temp;

        // Looping for handlers (NULL-terminated list)
        if ( archive_handlers )
        {
            for (i = 0; archive_handlers[i] != NULL; ++i)
            {
                // Appending to new archive handlers list when it isnt the
                // deleted handler - remember that archive handlers are
                // referred to by their xset names, not handler names!!
                if (g_strcmp0( archive_handlers[i], xset_name ) != 0)
                {
                    // Debug code
                    //g_message("archive_handlers[i]: %s\nxset_name: %s",
                    //                        archive_handlers[i], xset_name);

                    new_archive_handlers_s_temp = new_archive_handlers_s;
                    if (g_strcmp0( new_archive_handlers_s, "" ) == 0)
                    {
                        new_archive_handlers_s = g_strdup( archive_handlers[i] );
                    }
                    else
                    {
                        new_archive_handlers_s = g_strdup_printf( "%s %s",
                                    new_archive_handlers_s, archive_handlers[i] );
                    }
                    g_free(new_archive_handlers_s_temp);
                }
            }
        }

        // Finally updating handlers
        xset_set( handler_conf_xset[mode], "s", new_archive_handlers_s );

        // Deleting xset
        xset_custom_delete( handler_xset, FALSE );
        handler_xset = NULL;

        // Removing handler from the list
        gtk_list_store_remove( GTK_LIST_STORE( model ), &it );

        if (g_strcmp0( new_archive_handlers_s, "" ) == 0)
        {
            /* Making remove and apply buttons insensitive if the last
             * handler has been removed */
            gtk_widget_set_sensitive( GTK_WIDGET( btn_remove ), FALSE );
            gtk_widget_set_sensitive( GTK_WIDGET( btn_apply ), FALSE );

            /* Now that all are removed, the user needs sensitive widgets
             * to be able to add a further handler */
            gtk_widget_set_sensitive( GTK_WIDGET( chkbtn_handler_enabled ),
                                      TRUE );
            gtk_widget_set_sensitive( GTK_WIDGET( entry_handler_name ),
                                      TRUE );
            gtk_widget_set_sensitive( GTK_WIDGET( entry_handler_mime ),
                                      TRUE );
            gtk_widget_set_sensitive( GTK_WIDGET( entry_handler_extension ),
                                      TRUE );
            gtk_widget_set_sensitive( GTK_WIDGET( view_handler_compress ),
                                      TRUE );
            gtk_widget_set_sensitive( GTK_WIDGET( view_handler_extract ),
                                      TRUE );
            gtk_widget_set_sensitive( GTK_WIDGET( view_handler_list ),
                                      TRUE );
            gtk_widget_set_sensitive( GTK_WIDGET( chkbtn_handler_compress_term ),
                                      TRUE );
            gtk_widget_set_sensitive( GTK_WIDGET( chkbtn_handler_extract_term ),
                                      TRUE );
            gtk_widget_set_sensitive( GTK_WIDGET( chkbtn_handler_list_term ),
                                      TRUE );
        }
        else
        {
            /* Still remaining handlers - selecting the first one,
             * otherwise nothing is now selected */
            GtkTreePath *new_path = gtk_tree_path_new_first();
            gtk_tree_selection_select_path( GTK_TREE_SELECTION( selection ),
                                    new_path );
            gtk_tree_path_free( new_path );
        }

        // Clearing up
        g_strfreev( archive_handlers );
        g_free( new_archive_handlers_s );
    }

_clean_exit:

    // Saving settings
    xset_autosave( FALSE, FALSE );

    // Freeing strings
    g_free( handler_compress );
    g_free( handler_extract );
    g_free( handler_list );
}

static void on_configure_changed( GtkTreeSelection* selection,
                                  GtkWidget* dlg )
{
    /* This event is triggered when the selected row is changed through
     * the keyboard, or no row is selected at all */

    // Fetching the model and iter from the selection
    GtkTreeIter it;
    GtkTreeModel* model;
    if ( !gtk_tree_selection_get_selected( selection, &model, &it ) )
    {
        // User has unselected all rows - removing loaded handler
        config_unload_handler_settings( dlg );
        return;
    }

    /* Fetching data from the model based on the iterator. Note that this
     * variable used for the G_STRING is defined on the stack, so should
     * be freed for me */
/*igcr memory leak - free these */
    gchar* handler_name;  // Not actually used...
    gchar* xset_name;
    gtk_tree_model_get( model, &it,
                        COL_XSET_NAME, &xset_name,
                        COL_HANDLER_NAME, &handler_name,
                        -1 );

    // Loading new archive handler values
    config_load_handler_settings( NULL, xset_name, dlg );

    /* Focussing archive handler name
     * Selects the text rather than just placing the cursor at the start
     * of the text... */
    /*GtkWidget* entry_handler_name = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                                "entry_handler_name" );
    gtk_widget_grab_focus( entry_handler_name );*/
}

static void on_configure_drag_end( GtkWidget* widget,
                                   GdkDragContext* drag_context,
                                   GtkWidget* dlg )
{
    GtkListStore* list = (GtkListStore*)g_object_get_data( G_OBJECT( dlg ),
                                                                    "list" );
    int mode = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( dlg ),
                                            "dialog_mode" ) );
    // Regenerating archive handlers list xset
    // Obtaining iterator pointing at first handler
    GtkTreeIter iter;
    if (!gtk_tree_model_get_iter_first( GTK_TREE_MODEL( list ), &iter ))
    {
        // Failed to get iterator - warning user and exiting
        g_warning("Drag'n'drop end event detected, but unable to get an"
        " iterator to the start of the model!");
        return;
    }

    // Looping for all handlers
    gchar* handler_name_unused;  // Not actually used...
    gchar* xset_name;
    gchar* archive_handlers = g_strdup( "" );
    gchar* archive_handlers_temp;
    do
    {
        // Fetching data from the model based on the iterator. Note that
        // this variable used for the G_STRING is defined on the stack,
        // so should be freed for me
/*igcr memory leak - free these */
        gtk_tree_model_get( GTK_TREE_MODEL( list ), &iter,
                            COL_XSET_NAME, &xset_name,
                            COL_HANDLER_NAME, &handler_name_unused,
                            -1 );

        archive_handlers_temp = archive_handlers;
        if (g_strcmp0( archive_handlers, "" ) == 0)
        {
            archive_handlers = g_strdup( xset_name );
        }
        else
        {
            archive_handlers = g_strdup_printf( "%s %s",
                archive_handlers, xset_name );
        }
        g_free(archive_handlers_temp);
    }
    while(gtk_tree_model_iter_next( GTK_TREE_MODEL( list ), &iter ));

    // Saving the new archive handlers list
    xset_set( handler_conf_xset[mode], "s", archive_handlers );
    g_free(archive_handlers);

    // Saving settings
    xset_autosave( FALSE, FALSE );

    // Ensuring first handler is selected (otherwise none are)
    GtkTreeSelection *selection = gtk_tree_view_get_selection(
                                        GTK_TREE_VIEW( widget ) );
    GtkTreePath *new_path = gtk_tree_path_new_first();
    gtk_tree_selection_select_path( GTK_TREE_SELECTION( selection ),
                            new_path );
    gtk_tree_path_free( new_path );
}

static void on_configure_handler_enabled_check( GtkToggleButton *togglebutton,
                                                gpointer user_data )
{
    /* When no handler is selected (i.e. the user selects outside of the
     * populated handlers list), the enabled checkbox might be checked
     * off - however the widgets must not be set insensitive when this
     * happens */
    GtkTreeView* view_handlers = (GtkTreeView*)g_object_get_data( G_OBJECT( user_data ),
                                            "view_handlers" );

    // Fetching selection from treeview
    GtkTreeSelection* selection;
    selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( view_handlers ) );
    
    // Exiting if no handler is selected
    GtkTreeIter it;
    GtkTreeModel* model;
    if ( !gtk_tree_selection_get_selected( selection, &model, &it ) )
        return;

    // Fetching current status
    gboolean enabled = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON ( togglebutton ) );

    // Getting at widgets
    GtkWidget* entry_handler_name = (GtkWidget*)g_object_get_data(
                                            G_OBJECT( user_data ),
                                            "entry_handler_name" );
    GtkWidget* entry_handler_mime = (GtkWidget*)g_object_get_data( G_OBJECT( user_data ),
                                            "entry_handler_mime" );
    GtkWidget* entry_handler_extension = (GtkWidget*)g_object_get_data( G_OBJECT( user_data ),
                                            "entry_handler_extension" );
    GtkWidget* view_handler_compress = (GtkWidget*)g_object_get_data( G_OBJECT( user_data ),
                                            "view_handler_compress" );
    GtkWidget* view_handler_extract = (GtkWidget*)g_object_get_data( G_OBJECT( user_data ),
                                            "view_handler_extract" );
    GtkWidget* view_handler_list = (GtkWidget*)g_object_get_data( G_OBJECT( user_data ),
                                            "view_handler_list" );
    GtkWidget* chkbtn_handler_compress_term = (GtkWidget*)g_object_get_data( G_OBJECT( user_data ),
                                            "chkbtn_handler_compress_term" );
    GtkWidget* chkbtn_handler_extract_term = (GtkWidget*)g_object_get_data( G_OBJECT( user_data ),
                                            "chkbtn_handler_extract_term" );
    GtkWidget* chkbtn_handler_list_term = (GtkWidget*)g_object_get_data( G_OBJECT( user_data ),
                                            "chkbtn_handler_list_term" );

    // Setting sensitive/insensitive various widgets as appropriate
    gtk_widget_set_sensitive( GTK_WIDGET( entry_handler_name ), enabled );
    gtk_widget_set_sensitive( GTK_WIDGET( entry_handler_mime ), enabled );
    gtk_widget_set_sensitive( GTK_WIDGET( entry_handler_extension ), enabled );
    gtk_widget_set_sensitive( GTK_WIDGET( view_handler_compress ), enabled );
    gtk_widget_set_sensitive( GTK_WIDGET( view_handler_extract ), enabled );
    gtk_widget_set_sensitive( GTK_WIDGET( view_handler_list ), enabled );
    gtk_widget_set_sensitive( GTK_WIDGET( chkbtn_handler_compress_term ), enabled );
    gtk_widget_set_sensitive( GTK_WIDGET( chkbtn_handler_extract_term ), enabled );
    gtk_widget_set_sensitive( GTK_WIDGET( chkbtn_handler_list_term ), enabled );
}

/*igcr some duplication here with on_configure_changed() - can you call
 * on_configure_changed(), or can a single event be used for selection
 * changed?  row-activated is when a row is activated by clicking
 * or double-clicking, or via keypress space/enter, not merely selected.  */
static void on_configure_row_activated( GtkTreeView* view,
                                        GtkTreePath* tree_path,
                                        GtkTreeViewColumn* col,
                                        GtkWidget* dlg )
{
    // This event is triggered when the selected row is changed by the
    // mouse

    // Fetching the model from the view
    GtkTreeModel* model = gtk_tree_view_get_model( GTK_TREE_VIEW( view ) );

    // Obtaining an iterator based on the view position
    GtkTreeIter it;
    if ( !gtk_tree_model_get_iter( model, &it, tree_path ) )
        return;

    // Fetching data from the model based on the iterator. Note that this
    // variable used for the G_STRING is defined on the stack, so should
    // be freed for me
/*igcr memory leaks - free these */
    gchar* handler_name;  // Not actually used...
    gchar* xset_name;
    gtk_tree_model_get( model, &it,
                        COL_XSET_NAME, &xset_name,
                        COL_HANDLER_NAME, &handler_name,
                        -1 );

    // Loading new archive handler values
    config_load_handler_settings( NULL, xset_name, dlg );

    // Focussing archive handler name
    // Selects the text rather than just placing the cursor at the start
    // of the text...
    /*GtkWidget* entry_handler_name = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                                "entry_handler_name" );
    gtk_widget_grab_focus( entry_handler_name );*/
}

static void restore_defaults( GtkWidget* dlg, gboolean all )
{
    int mode = GPOINTER_TO_INT( g_object_get_data( G_OBJECT( dlg ),
                                                "dialog_mode" ) );
    if ( all )
    {
        int response = xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_WARNING,
                            _("Restore Default Handlers"), NULL,
                            GTK_BUTTONS_YES_NO, _("Missing default handlers will be restored.\n\nAlso OVERWRITE ALL EXISTING default handlers?"),
                            NULL, NULL);
        if ( response != GTK_RESPONSE_YES && response != GTK_RESPONSE_NO )
            // dialog was closed with no button pressed - cancel
            return;
        ptk_handler_add_defaults( mode, response == GTK_RESPONSE_YES, TRUE );

        /* Reset archive handlers list (this also selects
         * the first handler and therefore populates the handler widgets) */
        GtkListStore* list = (GtkListStore*)g_object_get_data( G_OBJECT( dlg ), "list" );
        gtk_list_store_clear( GTK_LIST_STORE( list ) );
        populate_archive_handlers( GTK_LIST_STORE( list ), GTK_WIDGET( dlg ) );
    }
    else
    {
        // Fetching the model from the view
        GtkTreeView* view_handlers = (GtkTreeView*)g_object_get_data( G_OBJECT( dlg ),
                                                "view_handlers" );

        // Fetching selection from treeview
        GtkTreeSelection* selection;
        selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( view_handlers ) );
        
        // Exiting if no handler is selected
        GtkTreeIter it;
        GtkTreeModel* model;
        if ( !gtk_tree_selection_get_selected( selection, &model, &it ) )
            return;

        gchar* xset_name;
        gtk_tree_model_get( model, &it,
                            COL_XSET_NAME, &xset_name,
                            -1 );
        // a default handler is selected?
        if ( !( xset_name &&
                        g_str_has_prefix( xset_name, handler_prefix[mode] ) ) )
        {
            g_free( xset_name );
            return;
        }
        
        // get default handler
        int i, nelements;
        const Handler* handler = NULL;

        if ( mode == HANDLER_MODE_ARC )
            nelements = G_N_ELEMENTS( handlers_arc );
        else if ( mode == HANDLER_MODE_FS )
            nelements = G_N_ELEMENTS( handlers_fs );
        else if  ( mode == HANDLER_MODE_NET )
            nelements = G_N_ELEMENTS( handlers_net );
        else
            return;
            
        gboolean found_handler = FALSE;
        for ( i = 0; i < nelements; i++ )
        {
            if ( mode == HANDLER_MODE_ARC )
                handler = &handlers_arc[i];
            else if ( mode == HANDLER_MODE_FS )
                handler = &handlers_fs[i];
            else if ( mode == HANDLER_MODE_NET )
                handler = &handlers_net[i];
            if ( !g_strcmp0( handler->xset_name, xset_name ) )
            {
                found_handler = TRUE;
                break;
            }
        }
        g_free( xset_name );
        if ( !found_handler )
            return;
        
        // create fake xset
        XSet* set = g_slice_new( XSet );
        set->name = (char*)handler->xset_name;
        set->menu_label = (char*)handler->handler_name;
        set->s = (char*)handler->type;
        set->x = (char*)handler->ext;
        set->y = (char*)handler->compress_cmd;
        set->z = (char*)handler->extract_cmd;
        set->context = (char*)handler->list_cmd;
        set->b = XSET_B_TRUE;

        // show fake xset values
        config_load_handler_settings( set, NULL, dlg );

        g_slice_free( XSet, set );
    }
}

static gboolean validate_handler( GtkWidget* dlg, int mode )
{
    if ( mode != HANDLER_MODE_ARC )
        // only archive handlers currently have validity checks
        return TRUE;
    
    // Fetching widgets and handler details
    GtkWidget* entry_handler_name = (GtkWidget*)g_object_get_data(
                                            G_OBJECT( dlg ),
                                                "entry_handler_name" );
    GtkWidget* entry_handler_mime = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                                "entry_handler_mime" );
    GtkWidget* entry_handler_extension = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                                "entry_handler_extension" );
    GtkWidget* view_handler_compress = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                                "view_handler_compress" );
    GtkWidget* view_handler_extract = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                                "view_handler_extract" );
    GtkWidget* view_handler_list = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                                "view_handler_list" );
    GtkWidget* chkbtn_handler_compress_term = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                                "chkbtn_handler_compress_term" );
    GtkWidget* chkbtn_handler_extract_term = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                                "chkbtn_handler_extract_term" );
    GtkWidget* chkbtn_handler_list_term = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                                "chkbtn_handler_list_term" );

    const gchar* handler_name = gtk_entry_get_text( GTK_ENTRY ( entry_handler_name ) );
    const gchar* handler_mime = gtk_entry_get_text( GTK_ENTRY ( entry_handler_mime ) );
    const gchar* handler_extension = gtk_entry_get_text( GTK_ENTRY ( entry_handler_extension ) );
    const gboolean handler_compress_term = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON ( chkbtn_handler_compress_term ) );
    const gboolean handler_extract_term = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON ( chkbtn_handler_extract_term ) );
    const gboolean handler_list_term = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON ( chkbtn_handler_list_term ) );
    gchar* handler_compress, *handler_extract, *handler_list;

    /* Commands are prefixed with '+' when they are to be ran in a
     * terminal */
    if (handler_compress_term)
    {
        handler_compress = g_strconcat( "+",
            get_text_view( GTK_TEXT_VIEW ( view_handler_compress ) ),
            NULL );
    }
    else handler_compress = get_text_view( GTK_TEXT_VIEW ( view_handler_compress ) );

    if (handler_extract_term)
    {
        handler_extract = g_strconcat( "+",
            get_text_view( GTK_TEXT_VIEW ( view_handler_extract ) ),
            NULL );
    }
    else handler_extract = get_text_view( GTK_TEXT_VIEW ( view_handler_extract ) );

    if (handler_list_term)
    {
        handler_list = g_strconcat( "+",
            get_text_view( GTK_TEXT_VIEW ( view_handler_list ) ),
            NULL );
    }
    else handler_list = get_text_view( GTK_TEXT_VIEW ( view_handler_list ) );

    /* Validating data. Note that data straight from widgets shouldnt
     * be modified or stored
     * Note that archive creation also allows for a command to be
     * saved */
    if (g_strcmp0( handler_name, "" ) <= 0)
    {
        /* Handler name not set - warning user and exiting. Note
         * that the created dialog does not have an icon set */
        xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_WARNING,
                            _(dialog_titles[mode]), NULL, FALSE,
                            _("Please enter a valid handler name "
                            "before saving."), NULL, NULL );
        gtk_widget_grab_focus( entry_handler_name );
        return FALSE;
    }

    // Empty MIME is allowed if extension is filled
    if (g_strcmp0( handler_mime, "" ) <= 0 &&
        g_strcmp0( handler_extension, "" ) <= 0)
    {
        /* Handler MIME not set - warning user and exiting. Note
         * that the created dialog does not have an icon set */
/*igcr memory leak - passing g_strdup_printf */
        xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_WARNING,
                            _(dialog_titles[mode]), NULL, FALSE,
                            g_strdup_printf(_("Please enter a valid "
                            "MIME content type OR extension for the "
                            "'%s' handler before saving."),
                            handler_name), NULL, NULL );
        gtk_widget_grab_focus( entry_handler_mime );
        return FALSE;
    }
    if (g_strstr_len( handler_mime, -1, " " ) &&
        g_strcmp0( handler_extension, "" ) <= 0)
    {
        /* Handler MIME contains a space - warning user and exiting.
         * Note that the created dialog does not have an icon set */
/*igcr memory leak - passing g_strdup_printf */
        xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_WARNING,
                            _(dialog_titles[mode]), NULL, FALSE,
                            g_strdup_printf(_("Please ensure the MIME"
                            " content type for the '%s' handler "
                            "contains no spaces before saving."),
                            handler_name), NULL, NULL );
        gtk_widget_grab_focus( entry_handler_mime );
        return FALSE;
    }

    /* Empty extension is allowed if MIME type has been given, but if
     * anything has been entered it must be valid */
    if (
        (
            g_strcmp0( handler_extension, "" ) <= 0 &&
            g_strcmp0( handler_mime, "" ) <= 0
        )
        ||
        (
            g_strcmp0( handler_extension, "" ) > 0 &&
            handler_extension && *handler_extension != '.'
        )
    )
    {
        /* Handler extension is either not set or does not start with
         * a full stop - warning user and exiting. Note that the created
         * dialog does not have an icon set */
/*igcr memory leak - passing g_strdup_printf */
        xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_WARNING,
                            _(dialog_titles[mode]), NULL, FALSE,
                            g_strdup_printf(_("Please enter a valid "
                            "file extension OR MIME content type for"
                            " the '%s' handler before saving."),
                            handler_name), NULL, NULL );
        gtk_widget_grab_focus( entry_handler_extension );
        return FALSE;
    }

    /* Other settings are commands to run in different situations -
     * since different handlers may or may not need different
     * commands, empty commands are allowed but if something is given,
     * relevant substitution characters should be in place */

    /* Compression handler validation - remember to maintain this code
     * in ptk_file_archiver_create too
     * Checking if a compression command has been entered */
    if (g_strcmp0( handler_compress, "" ) != 0 &&
        g_strcmp0( handler_compress, "+" ) != 0)
    {
        /* It has - making sure all substitution characters are in
         * place - not mandatory to only have one of the particular
         * type */
        if (
            (
                !g_strstr_len( handler_compress, -1, "%o" ) &&
                !g_strstr_len( handler_compress, -1, "%O" )
            )
            ||
            (
                !g_strstr_len( handler_compress, -1, "%n" ) &&
                !g_strstr_len( handler_compress, -1, "%N" )
            )
        )
        {
/*igcr memory leak - passing g_strdup_printf - also fits on small screen? */
            xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_WARNING,
                            _(dialog_titles[mode]), NULL, FALSE,
                            g_strdup_printf(_("The following "
                            "substitution variables should be in the"
                            " '%s' compression command:\n\n"
                            "One of the following:\n\n"
                            "%%%%n: First selected file/directory to"
                            " archive\n"
                            "%%%%N: All selected files/directories to"
                            " archive\n\n"
                            "and one of the following:\n\n"
                            "%%%%o: Resulting single archive\n"
                            "%%%%O: Resulting archive per source "
                            "file/directory (see %%%%n/%%%%N)"),
                            handler_name), NULL, NULL );
            gtk_widget_grab_focus( view_handler_compress );
            return FALSE;
        }
    }

    if (g_strcmp0( handler_extract, "" ) != 0 &&
        g_strcmp0( handler_extract, "+" ) != 0 &&
        (
            !g_strstr_len( handler_extract, -1, "%x" )
        ))
    {
        /* Not all substitution characters are in place - warning
         * user and exiting. Note that the created dialog does not
         * have an icon set
         * TODO: IG problem */
/*igcr memory leak - passing g_strdup_printf */
        xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_WARNING,
                            _(dialog_titles[mode]), NULL, FALSE,
                            g_strdup_printf(_("The following "
                            "placeholders should be in the '%s' extraction"
                            " command:\n\n%%%%x: "
                            "Archive to extract"),
                            handler_name), NULL, NULL );
        gtk_widget_grab_focus( view_handler_extract );
        return FALSE;
    }

    if (g_strcmp0( handler_list, "" ) != 0 &&
        g_strcmp0( handler_list, "+" ) != 0 &&
        (
            !g_strstr_len( handler_list, -1, "%x" )
        ))
    {
        /* Not all substitution characters are in place  - warning
         * user and exiting. Note that the created dialog does not
         * have an icon set
         * TODO: Confirm if IG still has this problem */
/*igcr memory leak - passing g_strdup_printf */
        xset_msg_dialog( GTK_WIDGET( dlg ), GTK_MESSAGE_WARNING,
                            _(dialog_titles[mode]), NULL, FALSE,
                            g_strdup_printf(_("The following "
                            "placeholders should be in the '%s' list"
                            " command:\n\n%%%%x: "
                            "Archive to list"),
                            handler_name), NULL, NULL );
        gtk_widget_grab_focus( view_handler_list );
        return FALSE;
    }

    // Validation passed
    return TRUE;
}

void ptk_handler_show_config( int mode, PtkFileBrowser* file_browser )
{
    /*
    Archives Types - 1 per xset as:
        set->name       xset name
        set->b          enabled  (XSET_UNSET|XSET_B_FALSE|XSET_B_TRUE)
        set->menu_label Display Name
        set->s          Mime Type(s)
        set->x          Extension(s)
        set->y          Compress Command
        set->z          Extract Command
        set->context    List Command

    Configure menu item is used to store some dialog data:
        get this set with:
            set = xset_get( "arc_conf2" );
        set->x          dialog width  (string)
        set->y          dialog height (string)
        set->s          space separated list of xset names (archive types)

    Example to add a new custom archive type:
        XSet* newset = add_new_arctype();
        newset->b = XSET_B_TRUE;                              // enable
        xset_set_set( newset, "label", "Windows CAB" );        // set archive Name - this internally sets menu_label
        xset_set_set( newset, "s", "application/winjunk" );    // set Mime Type(s)
        xset_set_set( newset, "x", ".cab" );                   // set Extension(s)
        xset_set_set( newset, "y", "createcab" );              // set Compress cmd
        xset_set_set( newset, "z", "excab" );                  // set Extract cmd
        xset_set_set( newset, "cxt", "listcab" );              // set List cmd - This really is cxt and not ctxt - xset_set_set bug thats already worked around

    Example to retrieve an xset for an archive type:
        XSet* set = xset_is( "hndarc_rar" );
        if ( !set )
            // there is no set named "hndarc_rar" (remove it from the list)
        else
        {
            const char* display_name = set->menu_label;
            const char* compress_cmd = set->y;
            gboolean enabled = xset_get_b_set( set );
            // etc
        }
    */

    // TODO: <IgnorantGuru> Also, you might have a look at how your config dialog behaves from the desktop menu.  Specifically, you may want to pass your function (DesktopWindow* desktop) in lieu of file_browser in that case.  So the prototype will be:
    // nm don't have that branch handy.  But you function can accept both file_browser and desktop and use whichever is non-NULL for the parent
    // If that doesn't make sense now, ask me later or I can hack it in.  That archive menu appears when right-clicking a desktop item

    /* Archive handlers dialog, attaching to top-level window (in GTK,
     * everything is a 'widget') - no buttons etc added as everything is
     * custom...
     * Extra NULL on the NULL-terminated list to placate an irrelevant
     * compilation warning */
/*igcr file_browser may be null if desktop use later accomodated */
    GtkWidget *top_level = file_browser ? gtk_widget_get_toplevel(
                                GTK_WIDGET( file_browser->main_window ) ) :
                                NULL;
    GtkWidget *dlg = gtk_dialog_new_with_buttons( _(dialog_titles[mode]),
                    top_level ? GTK_WINDOW( top_level ) : NULL,
                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                    NULL, NULL );
    gtk_container_set_border_width( GTK_CONTAINER ( dlg ), 5 );

    // Debug code
    //g_message( "Parent window title: %s", gtk_window_get_title( GTK_WINDOW( top_level ) ) );

    // Forcing dialog icon
    xset_set_window_icon( GTK_WINDOW( dlg ) );

    // Setting saved dialog size
    int width = xset_get_int( handler_conf_xset[HANDLER_MODE_ARC], "x" );
    int height = xset_get_int( handler_conf_xset[HANDLER_MODE_ARC], "y" );
    if ( width && height )
        gtk_window_set_default_size( GTK_WINDOW( dlg ), width, height );

    // Adding the help button but preventing it from taking the focus on click
    gtk_button_set_focus_on_click(
                                    GTK_BUTTON(
                                        gtk_dialog_add_button(
                                            GTK_DIALOG( dlg ),
                                            GTK_STOCK_HELP,
                                            GTK_RESPONSE_HELP
                                        )
                                    ),
                                    FALSE );

    // Adding standard buttons and saving references in the dialog
    // 'Restore defaults' button has custom text but a stock image
    GtkButton* btn_defaults = GTK_BUTTON( gtk_dialog_add_button( GTK_DIALOG( dlg ),
                                                _("All De_faults"),
                                                GTK_RESPONSE_NONE ) );
    GtkWidget* btn_defaults_image = xset_get_image( "GTK_STOCK_REVERT_TO_SAVED",
                                                GTK_ICON_SIZE_BUTTON );
    gtk_button_set_image( GTK_BUTTON( btn_defaults ), GTK_WIDGET ( btn_defaults_image ) );
    //g_object_set_data( G_OBJECT( dlg ), "btn_defaults", GTK_BUTTON( btn_defaults ) );
    GtkButton* btn_defaults0 = GTK_BUTTON( gtk_dialog_add_button( GTK_DIALOG( dlg ),
                                                _("Defa_ults"),
                                                GTK_RESPONSE_NO ) );
    GtkWidget* btn_defaults_image0 = xset_get_image( "GTK_STOCK_REVERT_TO_SAVED",
                                                GTK_ICON_SIZE_BUTTON );
    gtk_button_set_image( GTK_BUTTON( btn_defaults0 ), GTK_WIDGET ( btn_defaults_image0 ) );
    g_object_set_data( G_OBJECT( dlg ), "btn_defaults0", GTK_BUTTON( btn_defaults0 ) );
    g_object_set_data( G_OBJECT( dlg ), "btn_ok",
                        gtk_dialog_add_button( GTK_DIALOG( dlg ),
                                                GTK_STOCK_CLOSE,
                                                GTK_RESPONSE_OK ) );

    // Generating left-hand side of dialog
    GtkWidget* lbl_handlers = gtk_label_new( NULL );
    char* str = g_strdup_printf("<b>%s</b>", _(dialog_titles[mode]) );
    gtk_label_set_markup( GTK_LABEL( lbl_handlers ), str );
    g_free( str );
    gtk_misc_set_alignment( GTK_MISC( lbl_handlers ), 0, 0 );

    // Generating the main manager list
    // Creating model - xset name then handler name
    GtkListStore* list = gtk_list_store_new( 2, G_TYPE_STRING, G_TYPE_STRING );
    g_object_set_data( G_OBJECT( dlg ), "list", list );

    // Creating treeview - setting single-click mode (normally this
    // widget is used for file lists, where double-clicking is the norm
    // for doing an action)
    GtkWidget* view_handlers = exo_tree_view_new();
    gtk_tree_view_set_model( GTK_TREE_VIEW( view_handlers ), GTK_TREE_MODEL( list ) );
/*igcr probably doesn't need to be single click, as you're not using row
 * activation, only selection changed? */
    exo_tree_view_set_single_click( (ExoTreeView*)view_handlers, TRUE );
    gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( view_handlers ), FALSE );
    g_object_set_data( G_OBJECT( dlg ), "view_handlers", view_handlers );

    // Turning the treeview into a scrollable widget
    GtkWidget* view_scroll = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( view_scroll ),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER( view_scroll ), view_handlers );

    // Enabling item reordering (GTK-handled drag'n'drop)
    gtk_tree_view_set_reorderable( GTK_TREE_VIEW( view_handlers ), TRUE );

    // Connecting treeview callbacks
    g_signal_connect( G_OBJECT( view_handlers ), "drag-end",
                        G_CALLBACK( on_configure_drag_end ),
                        GTK_WIDGET( dlg ) );
    g_signal_connect( G_OBJECT( view_handlers ), "row-activated",
                        G_CALLBACK( on_configure_row_activated ),
                        GTK_WIDGET( dlg ) );
    g_signal_connect( G_OBJECT( gtk_tree_view_get_selection(
                                    GTK_TREE_VIEW( view_handlers ) ) ),
                        "changed",
                        G_CALLBACK( on_configure_changed ),
                        GTK_WIDGET( dlg ) );

    // Adding column to the treeview
    GtkTreeViewColumn* col = gtk_tree_view_column_new();

    // Change columns to optimal size whenever the model changes
    gtk_tree_view_column_set_sizing( col, GTK_TREE_VIEW_COLUMN_AUTOSIZE );
    
    GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start( col, renderer, TRUE );

    // Tie model data to the column
    gtk_tree_view_column_add_attribute( col, renderer,
                                         "text", COL_HANDLER_NAME);

    gtk_tree_view_append_column ( GTK_TREE_VIEW( view_handlers ), col );

    // Set column to take all available space - false by default
    gtk_tree_view_column_set_expand ( col, TRUE );

    // Treeview widgets
    GtkButton* btn_remove = GTK_BUTTON( gtk_button_new_with_mnemonic( _("_Remove") ) );
    gtk_button_set_image( btn_remove, xset_get_image( "GTK_STOCK_REMOVE",
                                                    GTK_ICON_SIZE_BUTTON ) );
    gtk_button_set_focus_on_click( btn_remove, FALSE );
    gtk_widget_set_sensitive( GTK_WIDGET( btn_remove ), FALSE );
    g_signal_connect( G_OBJECT( btn_remove ), "clicked",
                        G_CALLBACK( on_configure_button_press ), dlg );
    g_object_set_data( G_OBJECT( dlg ), "btn_remove", GTK_BUTTON( btn_remove ) );

    GtkButton* btn_add = GTK_BUTTON( gtk_button_new_with_mnemonic( _("_Add") ) );
    gtk_button_set_image( btn_add, xset_get_image( "GTK_STOCK_ADD",
                                                GTK_ICON_SIZE_BUTTON ) );
    gtk_button_set_focus_on_click( btn_add, FALSE );
    g_signal_connect( G_OBJECT( btn_add ), "clicked",
                        G_CALLBACK( on_configure_button_press ), dlg );
    g_object_set_data( G_OBJECT( dlg ), "btn_add", GTK_BUTTON( btn_add ) );

    GtkButton* btn_apply = GTK_BUTTON( gtk_button_new_with_mnemonic( _("A_pply") ) );
    gtk_button_set_image( btn_apply, xset_get_image( "GTK_STOCK_APPLY",
                                                GTK_ICON_SIZE_BUTTON ) );
    gtk_button_set_focus_on_click( btn_apply, FALSE );
    gtk_widget_set_sensitive( GTK_WIDGET( btn_apply ), FALSE );
    g_signal_connect( G_OBJECT( btn_apply ), "clicked",
                        G_CALLBACK( on_configure_button_press ), dlg );
    g_object_set_data( G_OBJECT( dlg ), "btn_apply", GTK_BUTTON( btn_apply ) );

    // Generating right-hand side of dialog
    GtkWidget* chkbtn_handler_enabled = gtk_check_button_new_with_label( _("Handler Enabled") );
    g_signal_connect( G_OBJECT( chkbtn_handler_enabled ), "toggled",
                G_CALLBACK ( on_configure_handler_enabled_check ), dlg );
    g_object_set_data( G_OBJECT( dlg ), "chkbtn_handler_enabled",
                        GTK_CHECK_BUTTON( chkbtn_handler_enabled ) );
    GtkWidget* lbl_handler_name = gtk_label_new( NULL );
    gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_handler_name ),
                                        _("_Name:") ),
    gtk_misc_set_alignment( GTK_MISC( lbl_handler_name ), 0, 0.5 );
    GtkWidget* lbl_handler_mime = gtk_label_new( NULL );
    gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_handler_mime ),
                                        mode == HANDLER_MODE_ARC ?
                                        _("MIME _Type:") :
                                        _("Whitelist:") );
    gtk_misc_set_alignment( GTK_MISC( lbl_handler_mime ), 0, 0.5 );
    GtkWidget* lbl_handler_extension = gtk_label_new( NULL );
    gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_handler_extension ),
                                        mode == HANDLER_MODE_ARC ?
                                        _("E_xtension:") :
                                        _("Blacklist:") );
    gtk_misc_set_alignment( GTK_MISC( lbl_handler_extension ), 0, 0.5 );
    GtkWidget* lbl_handler_compress = gtk_label_new( NULL );
    gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_handler_compress ),
                                        mode == HANDLER_MODE_ARC ?
                                        _("<b>Co_mpress:</b>") :
                                        _("<b>_Mount:</b>") );
    gtk_misc_set_alignment( GTK_MISC( lbl_handler_compress ), 0, 1.0 );
    GtkWidget* lbl_handler_extract = gtk_label_new( NULL );
    gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_handler_extract ),
                                        mode == HANDLER_MODE_ARC ?
                                        _("<b>Extrac_t:</b>") :
                                        _("<b>Unmoun_t:</b>") );
    gtk_misc_set_alignment( GTK_MISC( lbl_handler_extract ), 0, 1.0 );
    GtkWidget* lbl_handler_list = gtk_label_new( NULL );
    gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_handler_list ),
                                        mode == HANDLER_MODE_ARC ?
                                        _("<b>L_ist:</b>") :
                                        _("<b>Propert_ies:</b>") );
    gtk_misc_set_alignment( GTK_MISC( lbl_handler_list ), 0, 1.0 );
    GtkWidget* entry_handler_name = gtk_entry_new();
    g_object_set_data( G_OBJECT( dlg ), "entry_handler_name",
                        GTK_ENTRY( entry_handler_name ) );
    GtkWidget* entry_handler_mime = gtk_entry_new();
    g_object_set_data( G_OBJECT( dlg ), "entry_handler_mime",
                        GTK_ENTRY( entry_handler_mime ) );
    GtkWidget* entry_handler_extension = gtk_entry_new();
    g_object_set_data( G_OBJECT( dlg ), "entry_handler_extension",
                        GTK_ENTRY( entry_handler_extension ) );

    /* Creating new textviews in scrolled windows */
    GtkWidget* view_handler_compress = gtk_text_view_new();
    g_object_set_data( G_OBJECT( dlg ), "view_handler_compress",
                        GTK_TEXT_VIEW( view_handler_compress ) );
    gtk_text_view_set_wrap_mode( GTK_TEXT_VIEW( view_handler_compress ),
                                 GTK_WRAP_WORD_CHAR );
    GtkWidget* view_handler_compress_scroll = gtk_scrolled_window_new( NULL,
                                                                NULL );
    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( view_handler_compress_scroll ),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER( view_handler_compress_scroll ),
                                      view_handler_compress );
    GtkWidget* view_handler_extract = gtk_text_view_new();
    g_object_set_data( G_OBJECT( dlg ), "view_handler_extract",
                        GTK_TEXT_VIEW( view_handler_extract ) );
    gtk_text_view_set_wrap_mode( GTK_TEXT_VIEW( view_handler_extract ),
                                 GTK_WRAP_WORD_CHAR );
    GtkWidget* view_handler_extract_scroll = gtk_scrolled_window_new( NULL,
                                                                NULL );
    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( view_handler_extract_scroll ),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER( view_handler_extract_scroll ),
                                      view_handler_extract );
    GtkWidget* view_handler_list = gtk_text_view_new();
    g_object_set_data( G_OBJECT( dlg ), "view_handler_list",
                        GTK_TEXT_VIEW( view_handler_list ) );
    gtk_text_view_set_wrap_mode( GTK_TEXT_VIEW( view_handler_list ),
                                 GTK_WRAP_WORD_CHAR );
    GtkWidget* view_handler_list_scroll = gtk_scrolled_window_new( NULL,
                                                                NULL );
    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( view_handler_list_scroll ),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER( view_handler_list_scroll ),
                                      view_handler_list );

    g_object_set_data( G_OBJECT( dlg ), "dialog_mode",
                       GINT_TO_POINTER( mode ) );

    // Setting widgets to be activated associated with label mnemonics
    gtk_label_set_mnemonic_widget( GTK_LABEL( lbl_handler_name ),
                                   entry_handler_name );
    gtk_label_set_mnemonic_widget( GTK_LABEL( lbl_handler_mime ),
                                   entry_handler_mime );
    gtk_label_set_mnemonic_widget( GTK_LABEL( lbl_handler_extension ),
                                   entry_handler_extension );
    gtk_label_set_mnemonic_widget( GTK_LABEL( lbl_handler_compress ),
                                   view_handler_compress );
    gtk_label_set_mnemonic_widget( GTK_LABEL( lbl_handler_extract ),
                                   view_handler_extract );
    gtk_label_set_mnemonic_widget( GTK_LABEL( lbl_handler_list ),
                                   view_handler_list );

    GtkWidget* chkbtn_handler_compress_term =
                gtk_check_button_new_with_label( _("Run In Terminal") );
    g_object_set_data( G_OBJECT( dlg ), "chkbtn_handler_compress_term",
                        GTK_CHECK_BUTTON( chkbtn_handler_compress_term ) );
    GtkWidget* chkbtn_handler_extract_term =
                gtk_check_button_new_with_label( _("Run In Terminal") );
    g_object_set_data( G_OBJECT( dlg ), "chkbtn_handler_extract_term",
                        GTK_CHECK_BUTTON( chkbtn_handler_extract_term ) );
    GtkWidget* chkbtn_handler_list_term =
                gtk_check_button_new_with_label( _("Run In Terminal") );
    g_object_set_data( G_OBJECT( dlg ), "chkbtn_handler_list_term",
                        GTK_CHECK_BUTTON( chkbtn_handler_list_term ) );

    /* Creating container boxes - at this point the dialog already comes
     * with one GtkVBox then inside that a GtkHButtonBox
     * For the right side of the dialog, standard GtkBox approach fails
     * to allow precise padding of labels to allow all entries to line up
     *  - so reimplementing with GtkTable. Would many GtkAlignments have
     * worked? */
    GtkWidget* hbox_main = gtk_hbox_new( FALSE, 4 );
    GtkWidget* vbox_handlers = gtk_vbox_new( FALSE, 4 );
    GtkWidget* hbox_view_buttons = gtk_hbox_new( FALSE, 4 );
    GtkWidget* vbox_settings = gtk_vbox_new( FALSE, 4 );
    GtkWidget* tbl_settings = gtk_table_new( 5, 3 , FALSE );
    GtkWidget* hbox_compress_header = gtk_hbox_new( FALSE, 4 );
    GtkWidget* hbox_extract_header = gtk_hbox_new( FALSE, 4 );
    GtkWidget* hbox_list_header = gtk_hbox_new( FALSE, 4 );

    /* Packing widgets into boxes
     * Remember, start and end-ness is broken
     * vbox_handlers packing must not expand so that the right side can
     * take the space */
    gtk_box_pack_start( GTK_BOX( hbox_main ),
                        GTK_WIDGET( vbox_handlers ), FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( vbox_handlers ),
                        GTK_WIDGET( lbl_handlers ), FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox_main ),
                       GTK_WIDGET( vbox_settings ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( vbox_settings ),
                       GTK_WIDGET( tbl_settings ), FALSE, FALSE, 4 );

    /* view_handlers isn't added but view_scroll is - view_handlers is
     * inside view_scroll. No padding added to get it to align with the
     * enabled widget on the right side */
    gtk_box_pack_start( GTK_BOX( vbox_handlers ),
                        GTK_WIDGET( view_scroll ), TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( vbox_handlers ),
                        GTK_WIDGET( hbox_view_buttons ), FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox_view_buttons ),
                        GTK_WIDGET( btn_remove ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox_view_buttons ),
                        GTK_WIDGET( gtk_vseparator_new() ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox_view_buttons ),
                        GTK_WIDGET( btn_add ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox_view_buttons ),
                        GTK_WIDGET( btn_apply ), TRUE, TRUE, 4 );

    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( chkbtn_handler_enabled ), 0, 1, 1, 2,
                        GTK_FILL, GTK_FILL, 0, 0 );

    gtk_table_set_row_spacing( GTK_TABLE( tbl_settings ), 1, 5 );

    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( lbl_handler_name ), 0, 1, 2, 3,
                        GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( entry_handler_name ), 1, 4, 2, 3,
                        GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( lbl_handler_mime ), 0, 1, 3, 4,
                        GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( entry_handler_mime ), 1, 4, 3, 4,
                        GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( lbl_handler_extension ), 0, 1, 4, 5,
                        GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( entry_handler_extension ), 1, 4, 4, 5,
                        GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0 );

    // Make sure widgets do not separate too much vertically
    gtk_box_set_spacing( GTK_BOX( vbox_settings ), 0 );

    // pack_end widgets must not expand to be flush up against the side
    gtk_box_pack_start( GTK_BOX( vbox_settings ),
                        GTK_WIDGET( hbox_compress_header ), FALSE, FALSE,
                        4 );
    gtk_box_pack_start( GTK_BOX( hbox_compress_header ),
                        GTK_WIDGET( lbl_handler_compress ), TRUE, TRUE,
                        4 );
    gtk_box_pack_end( GTK_BOX( hbox_compress_header ),
                        GTK_WIDGET( chkbtn_handler_compress_term ), FALSE,
                        FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( vbox_settings ),
                        GTK_WIDGET( view_handler_compress_scroll ), TRUE,
                        TRUE, 4 );

    gtk_box_pack_start( GTK_BOX( vbox_settings ),
                        GTK_WIDGET( hbox_extract_header ), FALSE, FALSE,
                        4 );
    gtk_box_pack_start( GTK_BOX( hbox_extract_header ),
                        GTK_WIDGET( lbl_handler_extract ), TRUE, TRUE,
                        4 );
    gtk_box_pack_end( GTK_BOX( hbox_extract_header ),
                        GTK_WIDGET( chkbtn_handler_extract_term ), FALSE,
                        FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( vbox_settings ),
                        GTK_WIDGET( view_handler_extract_scroll ), TRUE,
                        TRUE, 4 );

    gtk_box_pack_start( GTK_BOX( vbox_settings ),
                        GTK_WIDGET( hbox_list_header ), FALSE, FALSE,
                        4 );
    gtk_box_pack_start( GTK_BOX( hbox_list_header ),
                        GTK_WIDGET( lbl_handler_list ), TRUE, TRUE, 4 );
    gtk_box_pack_end( GTK_BOX( hbox_list_header ),
                        GTK_WIDGET( chkbtn_handler_list_term ), FALSE,
                        FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( vbox_settings ),
                        GTK_WIDGET( view_handler_list_scroll ), TRUE,
                        TRUE, 4 );

    /* Packing boxes into dialog with padding to separate from dialog's
     * standard buttons at the bottom */
    gtk_box_pack_start(
                GTK_BOX(
                    gtk_dialog_get_content_area( GTK_DIALOG( dlg ) )
                ),
                GTK_WIDGET( hbox_main ), TRUE, TRUE, 4 );

    // Adding archive handlers to list
    populate_archive_handlers( GTK_LIST_STORE( list ), GTK_WIDGET( dlg ) );

    /* Rendering dialog - while loop is used to deal with standard
     * buttons that should not cause the dialog to exit */
    gtk_widget_show_all( GTK_WIDGET( dlg ) );
    int response;
    while ( response = gtk_dialog_run( GTK_DIALOG( dlg ) ) )
    {
        if ( response == GTK_RESPONSE_OK )
        {
            break;
        }
        else if ( response == GTK_RESPONSE_HELP )
        {
            // TODO: Sort out proper help
            xset_show_help( dlg, NULL, "#designmode-style-context" );
        }
        else if ( response == GTK_RESPONSE_NONE )
        {
            // Restore defaults requested
            restore_defaults( GTK_WIDGET( dlg ), TRUE );
        }
        else if ( response == GTK_RESPONSE_NO )
        {
            // Restore defaults requested
            restore_defaults( GTK_WIDGET( dlg ), FALSE );
        }
        else
            break;
    }

    // Fetching dialog dimensions
    GtkAllocation allocation;
    gtk_widget_get_allocation ( GTK_WIDGET( dlg ), &allocation );
    width = allocation.width;
    height = allocation.height;

    // Checking if they are valid
    if ( width && height )
    {
        // They are - saving
        char* str = g_strdup_printf( "%d", width );
        xset_set( handler_conf_xset[HANDLER_MODE_ARC], "x", str );
        g_free( str );
        str = g_strdup_printf( "%d", height );
        xset_set( handler_conf_xset[HANDLER_MODE_ARC], "y", str );
        g_free( str );
    }

    // Clearing up dialog
    gtk_widget_destroy( dlg );
}
