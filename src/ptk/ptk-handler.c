/*
 * SpaceFM ptk-handler.c
 * 
 * Copyright (C) 2015 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2013-2014 OmegaPhil <OmegaPhil+SpaceFM@gmail.com>
 * Copyright (C) 2014 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>
 * 
 * License: See COPYING file
 * 
*/

#include <glib/gi18n.h>
#include <string.h>
#include <stdlib.h>
#include <fnmatch.h>
#include <errno.h>
#include <gdk/gdkkeysyms.h>

#include "ptk-handler.h"
#include "exo-tree-view.h"
#include "gtk2-compat.h"


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

// xset name prefixes of default handlers
const char* handler_def_prefix[] =
{
    "hand_arc_+",
    "hand_fs_+",
    "hand_net_+",
    "hand_f_+"
};

// xset name prefixes of custom handlers
const char* handler_cust_prefix[] =
{
    "hand_arc_",
    "hand_fs_",
    "hand_net_",
    "hand_f_"
};

const char* handler_conf_xset[] =
{
    "arc_conf2",
    "dev_fs_cnf",
    "dev_net_cnf",
    "open_hand"
};

const char* dialog_titles[] =
{
    N_("Archive Handlers"),
    N_("Device Handlers"),
    N_("Protocol Handlers"),
    N_("File Handlers")
};

const char* modes[] = { "archive", "device", "protocol", "file" };
const char* cmds_arc[] = { "compress", "extract", "list" };
const char* cmds_mnt[] = { "mount", "unmount", "info" };

/* don't change this script header or it will break header detection on 
 * existing scripts! */
const char* script_header = "#!/bin/bash\n";

typedef struct
{
    GtkWidget* dlg;
    GtkWidget* parent;
    int mode;
    
    GtkWidget* view_handlers;
    GtkListStore* list;

    GtkWidget* chkbtn_handler_enabled;
    GtkWidget* entry_handler_name;
    GtkWidget* entry_handler_mime;
    GtkWidget* entry_handler_extension;
    GtkWidget* entry_handler_icon;
    GtkWidget* view_handler_compress;
    GtkWidget* view_handler_extract;
    GtkWidget* view_handler_list;
    GtkWidget* chkbtn_handler_compress_term;
    GtkWidget* chkbtn_handler_extract_term;
    GtkWidget* chkbtn_handler_list_term;
    GtkWidget* btn_remove;
    GtkWidget* btn_add;
    GtkWidget* btn_apply;
    GtkWidget* btn_up;
    GtkWidget* btn_down;
    GtkWidget* btn_ok;
    GtkWidget* btn_defaults;
    GtkWidget* btn_defaults0;
} HandlerData;

typedef struct _Handler
{
                                // enabled           set->b
    const char* xset_name;      //                   set->name
    const char* handler_name;   //                   set->menu_label
    const char* type;           // or whitelist      set->s
    const char* ext;            // or blacklist      set->x
    const char* compress_cmd;   // or mount          (script)
    gboolean compress_term;     //                   set->in_terminal
    const char* extract_cmd;    // or unmount        (script)
    gboolean extract_term;      //                   set->keep_terminal
    const char* list_cmd;       // or info           (script)
    gboolean list_term;         //                   set->scroll_lock
    
    /* save as custom item
    set->lock = FALSE;
    if handler equals default, don't save in session
    set->disable = TRUE; */
} Handler;

/* If you add a new handler, add it to (end of ) existing session file handler
 * list so existing users see the new handler. */
const Handler handlers_arc[]=
{
    /* In compress commands:
    *   %n: First selected filename/dir to archive
    *   %N: All selected filenames/dirs to archive, or (with %O) a single filename
    *   %o: Resulting single archive file
    *   %O: Resulting archive per source file/directory (use changes %N meaning)
    *
    * In extract commands:
    *   %x: Archive file to extract
    *   %g: Unique extraction target filename with optional subfolder
    *   %G: Unique extraction target filename, never with subfolder
    *
    * In list commands:
    *     %x: Archive to list
    *
    * Plus standard bash variables are accepted.
    */
    {
        "hand_arc_+7z",
        "7-Zip",
        "application/x-7z-compressed",
        "*.7z",
        "\"$(which 7za || echo 7zr)\" a %o %N",
        TRUE,
        "\"$(which 7za || echo 7zr)\" x %x",
        TRUE,
        "\"$(which 7za || echo 7zr)\" l %x",
        TRUE
    },
    {
        "hand_arc_+rar",
        "RAR",
        "application/x-rar",
        "*.rar *.RAR",
        "rar a -r %o %N",
        TRUE,
        "unrar -o- x %x",
        TRUE,
        "unrar lt %x",
        TRUE
    },
    {
        "hand_arc_+tar",
        "Tar",
        "application/x-tar",
        "*.tar",
        "tar -cvf %o %N",
        FALSE,
        "tar -xvf %x",
        FALSE,
        "tar -tvf %x",
        TRUE
    },
    {
        "hand_arc_+tar_bz2",
        "Tar bzip2",
        "application/x-bzip-compressed-tar",
        "*.tar.bz2",
        "tar -cvjf %o %N",
        FALSE,
        "tar -xvjf %x",
        FALSE,
        "tar -tvf %x",
        TRUE
    },
    {
        "hand_arc_+tar_gz",
        "Tar Gzip",
        "application/x-compressed-tar",
        "*.tar.gz *.tgz",
        "tar -cvzf %o %N",
        FALSE,
        "tar -xvzf %x",
        FALSE,
        "tar -tvf %x",
        TRUE
    },
    {
        "hand_arc_+tar_xz",
        "Tar xz",
        "application/x-xz-compressed-tar",
        "*.tar.xz *.txz",
        "tar -cvJf %o %N",
        FALSE,
        "tar -xvJf %x",
        FALSE,
        "tar -tvf %x",
        TRUE
    },
    {
        "hand_arc_+zip",
        "Zip",
        "application/x-zip application/zip",
        "*.zip *.ZIP",
        "zip -r %o %N",
        TRUE,
        "unzip %x",
        TRUE,
        "unzip -l %x",
        TRUE
    },
    {
        "hand_arc_+gz",
        "Gzip",
        "application/x-gzip application/x-gzpdf application/gzip",
        "*.gz",
        "gzip -c %N > %O",
        FALSE,
        "gzip -cd %x > %G",
        FALSE,
        "gunzip -l %x",
        FALSE
    },
    {
        "hand_arc_+xz",
        "XZ",
        "application/x-xz",
        "*.xz",
        "xz -cz %N > %O",
        FALSE,
        "xz -cd %x > %G",
        FALSE,
        "xz -tv %x",
        FALSE
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
        "hand_fs_+fuseiso",
        "fuseiso",
        "file *fuseiso",
        "",
        "fuseiso %v %a",
        FALSE,
        "fusermount -u %a",
        FALSE,
        "grep \"%a\" ~/.mtab.fuseiso",
        FALSE
    },
    {
        "hand_fs_+udiso",
        "udevil iso",
        "file iso9660",
        "",
        "uout=\"$(udevil mount %v)\"\nerr=$?; echo \"$uout\"\n[[ $err -ne 0 ]] && exit 1\npoint=\"${uout#Mounted }\"\n[[ \"$point\" = \"$uout\" ]] && exit 0\npoint=\"${point##* at }\"\n[[ -d \"$point\" ]] && spacefm \"$point\" &\n",
        FALSE,
        "# Note: non-iso9660 types will fall through to Default unmount handler\nudevil umount %a\n",
        FALSE,
        INFO_EXAMPLE,
        FALSE
    },
    {
        "hand_fs_+def",
        "Default",
        "*",
        "",
        MOUNT_EXAMPLE,
        FALSE,
        UNMOUNT_EXAMPLE,
        FALSE,
        INFO_EXAMPLE,
        FALSE
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
        "hand_net_+ftp",
        "ftp",
        "ftp",
        "",
        "options=\"nonempty\"\nif [ -n \"%user%\" ]; then\n    user=\",user=%user%\"\n    [[ -n \"%pass%\" ]] && user=\"$user:%pass%\"\nfi\n[[ -n \"%port%\" ]] && portcolon=:\necho \">>> curlftpfs -o $options$user ftp://%host%$portcolon%port%%path% %a\"\necho\ncurlftpfs -o $options$user ftp://%host%$portcolon%port%%path% \"%a\"\n[[ $? -eq 0 ]] && sleep 1 && ls \"%a\"  # set error status or wait until ready\n",
        TRUE,
        "fusermount -u \"%a\"",
        FALSE,
        INFO_EXAMPLE,
        FALSE
    },
    {
        "hand_net_+ssh",
        "ssh",
        "ssh mtab_fs=fuse.sshfs",
        "",
        "[[ -n \"$fm_url_user\" ]] && fm_url_user=\"${fm_url_user}@\"\n[[ -z \"$fm_url_port\" ]] && fm_url_port=22\necho \">>> sshfs -p $fm_url_port $fm_url_user$fm_url_host:$fm_url_path %a\"\necho\n# Run sshfs through nohup to prevent disconnect on terminal close\nsshtmp=\"$(mktemp --tmpdir spacefm-ssh-output-XXXXXXXX.tmp)\" || exit 1\nnohup sshfs -p $fm_url_port $fm_url_user$fm_url_host:$fm_url_path %a &> \"$sshtmp\"\nerr=$?\n[[ -e \"$sshtmp\" ]] && cat \"$sshtmp\" ; rm -f \"$sshtmp\"\n[[ $err -eq 0 ]]  # set error status\n\n# Alternate Method - if enabled, disable nohup line above and\n#                    uncheck Run In Terminal\n# # Run sshfs in a terminal without SpaceFM task.  sshfs disconnects when the\n# # terminal is closed\n# spacefm -s run-task cmd --terminal \"echo 'Connecting to $fm_url'; echo; sshfs -p $fm_url_port $fm_url_user$fm_url_host:$fm_url_path %a; if [ $? -ne 0 ]; then echo; echo '[ Finished ] Press Enter to close'; else echo; echo 'Press Enter to close (closing this window may unmount sshfs)'; fi; read\" & sleep 1\n",
        TRUE,
        "fusermount -u %a",
        FALSE,
        INFO_EXAMPLE,
        FALSE
    },
    {
        "hand_net_+udevil",
        "udevil",
        "ftp http https nfs smb ssh mtab_fs=fuse*",
        "",
        "udevil mount \"$fm_url\"",
        FALSE,
        "udevil umount \"%a\"",
        FALSE,
        INFO_EXAMPLE,
        FALSE
    }
};

const Handler handlers_file[]=
{
    /* %a custom mount point
     * Plus standard substitution variables are accepted. */
    {
        "hand_f_+iso",
        "Mount ISO",
        "application/x-iso9660-image application/x-iso-image application/x-cd-image",
        "*.img *.iso *.mdf *.nrg",
        "# is image file already mounted?\ncanon=\"$(readlink -f \"$fm_file\" 2>/dev/null)\"\nif [ -n \"$canon\" ]; then\n    canon_enc=\"${canon// /\\\\\\\\040}\" # encode spaces for mtab+grep\n    if grep -q \"^$canon_enc \" ~/.mtab.fuseiso 2>/dev/null; then\n        # file is mounted - get mount point\n        point=\"$(grep -m 1 \"^$canon_enc \" ~/.mtab.fuseiso \\\n                 | sed 's/.* \\(.*\\) fuseiso .*/\\1/' )\"\n    if [ -x \"$point\" ]; then\n            spacefm \"$point\" &\n            exit\n        fi\n    fi\nfi\n\n# mount & open\nfuseiso %f %a && spacefm %a &\n",
        FALSE,
        "",
        FALSE,
        "",
        FALSE
    }
};

// Function prototypes
static void on_configure_handler_enabled_check( GtkToggleButton *togglebutton,
                                                HandlerData* hnd );
static void restore_defaults( HandlerData* hnd, gboolean all );
static gboolean validate_archive_handler( HandlerData* hnd );

gboolean ptk_handler_command_is_empty( const char* command )
{
    // test if command contains only comments and whitespace
    if ( !command )
        return TRUE;
    gchar** lines = g_strsplit( command, "\n", 0 );
    if ( !lines )
        return TRUE;

    int i;
    gboolean found = FALSE;
    for ( i = 0; lines[i]; i++ )
    {
        g_strstrip( lines[i] );
        if ( lines[i][0] != '\0' && lines[i][0] != '#' )
        {
            found = TRUE;
            break;
        }
    }
    g_strfreev( lines );
    return !found;
}

void ptk_handler_load_text_view( GtkTextView* view, const char* text )
{
    if ( view )
    {
        GtkTextBuffer* buf = gtk_text_view_get_buffer( GTK_TEXT_VIEW( view ) );
        gtk_text_buffer_set_text( buf, text ? text : "", -1 );
    }
}

char* ptk_handler_get_text_view( GtkTextView* view )
{
    if ( !view )
        return g_strdup( "" );
    GtkTextBuffer* buf = gtk_text_view_get_buffer( view );
    GtkTextIter iter, siter;
    gtk_text_buffer_get_start_iter( buf, &siter );
    gtk_text_buffer_get_end_iter( buf, &iter );
    char* text = gtk_text_buffer_get_text( buf, &siter, &iter, FALSE );
    if ( !text )
        return g_strdup( "" );
    return text;
}

char* ptk_handler_get_command( int mode, int cmd, XSet* handler_set )
{   /* if handler->disable, get const command, else get script path if exists */
    if ( !handler_set )
        return NULL;
    if ( handler_set->disable )
    {
        // is default handler - get command from const char
        const Handler* handler;
        int i, nelements;
        const char* command;
        char* str;
        
        if ( mode == HANDLER_MODE_ARC )
            nelements = G_N_ELEMENTS( handlers_arc );
        else if ( mode == HANDLER_MODE_FS )
            nelements = G_N_ELEMENTS( handlers_fs );
        else if  ( mode == HANDLER_MODE_NET )
            nelements = G_N_ELEMENTS( handlers_net );
        else if  ( mode == HANDLER_MODE_FILE )
            nelements = G_N_ELEMENTS( handlers_file );
        else
            return NULL;

        for ( i = 0; i < nelements; i++ )
        {
            if ( mode == HANDLER_MODE_ARC )
                handler = &handlers_arc[i];
            else if ( mode == HANDLER_MODE_FS )
                handler = &handlers_fs[i];
            else if ( mode == HANDLER_MODE_NET )
                handler = &handlers_net[i];
            else
                handler = &handlers_file[i];

            if ( !strcmp( handler->xset_name, handler_set->name ) )
            {
                // found default handler
                if ( cmd == HANDLER_COMPRESS )
                    command = handler->compress_cmd;
                else if ( cmd == HANDLER_EXTRACT )
                    command = handler->extract_cmd;
                else
                    command = handler->list_cmd;
                return g_strdup( command );
            }
        }
        return NULL;
    }
    // get default script path
    char* def_script = xset_custom_get_script( handler_set, FALSE );
    if ( !def_script )
    {
        g_warning( "ptk_handler_get_command unable to get script for custom %s",
                                                        handler_set->name );
        return NULL;
    }
    // name script
    char* str = g_strdup_printf( "/hand-%s-%s.sh", modes[mode],
                                mode == HANDLER_MODE_ARC ?
                                        cmds_arc[cmd] : cmds_mnt[cmd] );
    char* script = replace_string( def_script, "/exec.sh", str, FALSE );
    g_free( str );
    g_free( def_script );
    if ( g_file_test( script, G_FILE_TEST_EXISTS ) )
        return script;
    g_warning( "ptk_handler_get_command missing script for custom %s",
                                                        handler_set->name );
    g_free( script );
    return NULL;
}

char* ptk_handler_load_script( int mode, int cmd, XSet* handler_set,
                               GtkTextView* view, char** text )
{
    /* places command in textview buffer or char** */
    GtkTextBuffer* buf = NULL;
    GString* gstr = NULL;

    if ( text )
        *text = NULL;
    if ( ( !view && !text ) || !handler_set )
        return g_strdup( _("Error: unable to load command (internal error)") );
    if ( view )
    {
        buf = gtk_text_view_get_buffer( GTK_TEXT_VIEW( view ) );
        gtk_text_buffer_set_text( buf, "", -1 );
    }

    if ( handler_set->disable )
    {
        // is default handler - get contents from const char
        char* command = ptk_handler_get_command( mode, cmd, handler_set );
        if ( !command )
            return g_strdup( _("Error: unable to load command (internal error)") );
        if ( view )
            gtk_text_buffer_insert_at_cursor( buf, command, -1 );
        else
            *text = command ;
        return NULL; // success
    }
    
    // get default script path
    char* def_script = xset_custom_get_script( handler_set, FALSE );
    if ( !def_script )
    {
        g_warning( "get_handler_script unable to get script for custom %s",
                                                        handler_set->name );
        return g_strdup( _("Error: unable to save command (can't get script path?)") );
    }
    // name script
    char* script;
    char* str;
    str = g_strdup_printf( "/hand-%s-%s.sh", modes[mode],
                                mode == HANDLER_MODE_ARC ?
                                        cmds_arc[cmd] : cmds_mnt[cmd] );
    script = replace_string( def_script, "/exec.sh", str, FALSE );
    g_free( str );
    g_free( def_script );
    // load script
    //gboolean modified = FALSE;
    char line[ 4096 ];
    FILE* file = 0;
    gboolean start = TRUE;
    
    if ( !view )
        gstr = g_string_new( "" );

    if ( g_file_test( script, G_FILE_TEST_EXISTS ) )
    {
        if ( file = fopen( script, "r" ) )
        {
            // read file one line at a time to prevent splitting UTF-8 characters
            while ( fgets( line, sizeof( line ), file ) )
            {
                if ( !g_utf8_validate( line, -1, NULL ) )
                {
                    fclose( file );
                    if ( view )
                        gtk_text_buffer_set_text( buf, "", -1 );
                    else
                        g_string_erase( gstr, 0, -1 );
                    //modified = TRUE;
                    g_warning( _("file '%s' contents are not valid UTF-8"),
                                                            script );
                    break;
                }
                if ( start )
                {
                    if ( !strcmp( line, script_header ) || 
                                                    !strcmp( line, "\n" ) )
                        // skip script header and initial blank lines
                        continue;
                    start = FALSE;
                }
                // add line to buffer
                if ( view )
                    gtk_text_buffer_insert_at_cursor( buf, line, -1 );
                else
                    g_string_append( gstr, line );
            }
            if ( fclose( file ) != 0 )
                goto _read_error;
        }
        else
            goto _read_error;
    }
    g_free( script );
    
    if ( !view )
        *text = g_string_free( gstr, FALSE );
    
    //gtk_text_view_set_editable( GTK_TEXT_VIEW( view ), !file ||
    //                                        have_rw_access( script ) );
    //gtk_text_buffer_set_modified( buf, modified );
    //command_script_stat(  );
    return NULL;  // success

_read_error:
    if ( file )
        fclose( file );
    str = g_strdup_printf( "%s '%s':\n\n%s", _("Error reading file"), script,
                                                        g_strerror( errno ) );
    g_free( script ); 
    if ( !view )
        g_string_free( gstr, TRUE );
    return str;
}

char* ptk_handler_save_script( int mode, int cmd, XSet* handler_set,
                               GtkTextView* view, const char* command )
{
    /* writes command in textview buffer or const command to script */
    if ( !( handler_set && handler_set->disable == FALSE ) )
        return g_strdup( _("Error: unable to save command (internal error)") );
    // get default script path
    char* def_script = xset_custom_get_script( handler_set, FALSE );
    if ( !def_script )
    {
        g_warning( "save_handler_script unable to get script for custom %s",
                                                        handler_set->name );
        return g_strdup( _("Error: unable to save command (can't get script path?)") );
    }
    // create parent dir
    char* parent_dir = g_path_get_dirname( def_script );
    if ( !g_file_test( parent_dir, G_FILE_TEST_IS_DIR ) )
    {
        g_mkdir_with_parents( parent_dir, 0700 );
        chmod( parent_dir, 0700 );
    }
    g_free( parent_dir );
    // name script
    char* script;
    char* str;
    str = g_strdup_printf( "/hand-%s-%s.sh", modes[mode],
                                mode == HANDLER_MODE_ARC ?
                                        cmds_arc[cmd] : cmds_mnt[cmd] );
    script = replace_string( def_script, "/exec.sh", str, FALSE );
    g_free( str );
    g_free( def_script );
    // get text
    char* text;
    if ( view )
    {
        GtkTextBuffer* buf = gtk_text_view_get_buffer( GTK_TEXT_VIEW( view ) );
        GtkTextIter iter, siter;
        gtk_text_buffer_get_start_iter( buf, &siter );
        gtk_text_buffer_get_end_iter( buf, &iter );
        text = gtk_text_buffer_get_text( buf, &siter, &iter, FALSE );
    }
    else if ( command )
        text = (char*)command;
    else
        text = "";
    
//printf("WRITE %s\n", script );
    // write script
    FILE* file = 0;
    if ( file = fopen( script, "w" ) )
    {
        // add default script header   #!/bin/bash\n\n
        if ( fputs( script_header, file ) < 0 )
            goto _write_error;
        if ( fputs( "\n", file ) < 0 )
            goto _write_error;
        if ( text && fputs( text, file ) < 0 )
            goto _write_error;
        if ( !g_str_has_suffix( text, "\n" ) && fputs( "\n", file ) < 0 )
            goto _write_error;
        if ( fclose( file ) != 0 )
            goto _write_error;
        // This script isn't run directly so no need to make executable
        //if ( chmod( script, S_IRUSR | S_IWUSR | S_IXUSR ) != 0 )
        //    goto _write_error;
    }
    else
        goto _write_error;
    g_free( script );
    if ( view )
        g_free( text );
    return NULL;  // success
    
_write_error:
    if ( file )
        fclose( file );
    str = g_strdup_printf( "%s '%s':\n\n%s", _("Error writing to file"), script,
                                                        g_strerror( errno ) );
    g_free( script );    
    if ( view )
        g_free( text );
    return str;
}

gboolean ptk_handler_values_in_list( const char* list, GSList* values,
                                     char** msg )
{   /* test for the presence of values in list, using wildcards.
    *  list is space-separated, plus sign (+) indicates required. */
    if ( !( list && list[0] ) || !values )
        return FALSE;
    
    if ( msg )
        *msg = NULL;

    // get elements of list
    gchar** elements = g_strsplit( list, " ", -1 );
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

static gboolean value_in_list( const char* list, const char* value )
{   // this function must be FAST - is run multiple times on menu popup
    gboolean supported = FALSE;
    char* ptr;
    char* delim;
    char ch;

    // value in space-separated list with wildcards?
    if ( value && ( ptr = (char*)list ) && ptr[0] )
    {
        while ( 1 )
        {
            while ( ptr[0] == ' ' )
                ptr++;
            if ( !ptr[0] )
                break;
            delim = ptr;
            while ( delim[0] != ' ' && delim[0] )
                delim++;
            ch = delim[0];
            delim[0] = '\0';    // set temporary end of string                    
            if ( fnmatch( ptr, value, 0 ) == 0 )
            {
                delim[0] = ch;      // restore
                return TRUE;
            }
            delim[0] = ch;      // restore
            if ( ch == '\0' )
                break;          // is end of string
            ptr = delim + 1;
        }
    }
    return FALSE;
}

GSList* ptk_handler_file_has_handlers( int mode, int cmd,
                                       const char* path,
                                       VFSMimeType* mime_type,
                                       gboolean test_cmd,
                                       gboolean multiple,
                                       gboolean enabled_only )
{   /* this function must be FAST - is run multiple times on menu popup
     * command must be non-empty if test_cmd */
    const char* type;
    char* delim;
    char* ptr;
    char* msg;
    XSet* handler_set;
    char* under_path;
    char* new_path = NULL;
    GSList* handlers = NULL;
    
    if ( !path && !mime_type )
        return NULL;
    // Fetching and validating MIME type if provided
    if ( mime_type )
        type = (char*)vfs_mime_type_get_type( mime_type );
    else
        type = NULL;
    
    // replace spaces in path with underscores for matching
    if ( path && strchr( path, ' ' ) )
        under_path = new_path = replace_string( path, " ", "_", FALSE );
    else
        under_path = (char*)path;
    
    // parsing handlers space-separated list
    if ( ptr = xset_get_s( handler_conf_xset[mode] ) )
    {
        while ( ptr[0] )
        {
            while ( ptr[0] == ' ' )
                ptr++;
            if ( !ptr[0] )
                break;
            if ( delim = strchr( ptr, ' ' ) )
                delim[0] = '\0';    // set temporary end of string

            // Fetching handler
            handler_set = xset_is( ptr );
            if ( delim )
                delim[0] = ' ';     // remove temporary end of string

            // handler supports type or path ?
            if ( handler_set && 
                    ( !enabled_only || handler_set->b == XSET_B_TRUE ) &&
                    ( value_in_list( handler_set->s, type ) ||
                      value_in_list( handler_set->x, under_path ) ) )
            {
                // test command
                if ( test_cmd )
                {
                    char* command;
                    char* err_msg = ptk_handler_load_script( mode, cmd,
                                            handler_set, NULL, &command );
                    if ( err_msg )
                    {
                        g_warning( "%s", err_msg );
                        g_free( err_msg );
                    }
                    else if ( !ptk_handler_command_is_empty( command ) )
                    {
                        handlers = g_slist_prepend( handlers, handler_set );
                        if ( !multiple )
                        {
                            g_free( command );
                            break;
                        }
                    }
                    g_free( command );
                }
                else
                {
                    handlers = g_slist_prepend( handlers, handler_set );
                    if ( !multiple )
                        break;
                }
            }
            if ( !delim )
                break;
            ptr = delim + 1;
        }
    }
    g_free( new_path );
    return g_slist_reverse( handlers );
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
    else if ( mode == HANDLER_MODE_NET )
        nelements = G_N_ELEMENTS( handlers_net );
    else if ( mode == HANDLER_MODE_FILE )
        nelements = G_N_ELEMENTS( handlers_file );
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
        else
            handler = &handlers_file[i];

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
                set->in_terminal = handler->compress_term ?
                                            XSET_B_TRUE : XSET_B_UNSET;
                if ( mode != HANDLER_MODE_FILE )
                {
                    set->keep_terminal = handler->extract_term ?
                                                XSET_B_TRUE : XSET_B_UNSET;
                    set->scroll_lock = handler->list_term ?
                                                XSET_B_TRUE : XSET_B_UNSET;
                }
                set->b = XSET_B_TRUE;
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

XSet* add_new_handler( int mode )
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
    set->lock = FALSE;
    return set;
}

static void config_load_handler_settings( XSet* handler_xset,
                                          gchar* handler_xset_name,
                                          const Handler* handler,
                                          HandlerData* hnd )
{   // handler_xset_name optional if handler_xset passed
    // Fetching actual xset if only the name has been passed
    if ( !handler_xset )
        if ( !( handler_xset = xset_is( handler_xset_name ) ) )
            return;

    /* At this point a handler exists, so making remove and apply buttons
     * sensitive as well as the enabled checkbutton */
    gtk_widget_set_sensitive( GTK_WIDGET( hnd->btn_remove ), TRUE );
    gtk_widget_set_sensitive( GTK_WIDGET( hnd->btn_apply ), TRUE );
    gtk_widget_set_sensitive( GTK_WIDGET( hnd->btn_up ), TRUE );
    gtk_widget_set_sensitive( GTK_WIDGET( hnd->btn_down ), TRUE );
    gtk_widget_set_sensitive( GTK_WIDGET( hnd->chkbtn_handler_enabled ), TRUE );
    gtk_widget_set_sensitive( GTK_WIDGET( hnd->btn_defaults0 ),
                    g_str_has_prefix( handler_xset->name,
                                      handler_def_prefix[hnd->mode] ) );

    /* Configuring widgets with handler settings. Only name, MIME and
     * extension warrant a warning
     * Commands are prefixed with '+' when they are ran in a terminal */
    int start;
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( hnd->chkbtn_handler_enabled ),
                                  handler_xset->b == XSET_B_TRUE );
    
    gtk_entry_set_text( GTK_ENTRY( hnd->entry_handler_name ),
                                    handler_xset->menu_label ?
                                    handler_xset->menu_label : "" );
    gtk_entry_set_text( GTK_ENTRY( hnd->entry_handler_mime ),
                                    handler_xset->s ?
                                    handler_xset->s : "" );
    gtk_entry_set_text( GTK_ENTRY( hnd->entry_handler_extension ),
                                    handler_xset->x ?
                                    handler_xset->x : "" );
    if ( hnd->entry_handler_icon )
        gtk_entry_set_text( GTK_ENTRY( hnd->entry_handler_icon ),
                                    handler_xset->icon ?
                                    handler_xset->icon : "" );
/*igtodo code review g_strdup leaks below this line */
    if ( handler )
    {
        // load commands from const handler
        ptk_handler_load_text_view( GTK_TEXT_VIEW( hnd->view_handler_compress ),
                                                    handler->compress_cmd );
        if ( hnd->mode != HANDLER_MODE_FILE )
        {
            ptk_handler_load_text_view( GTK_TEXT_VIEW( hnd->view_handler_extract ),
                                                        handler->extract_cmd );
            ptk_handler_load_text_view( GTK_TEXT_VIEW( hnd->view_handler_list ),
                                                        handler->list_cmd );
        }
    }
    else
    {
        char* err_msg = ptk_handler_load_script( hnd->mode, HANDLER_COMPRESS,
                            handler_xset,
                            GTK_TEXT_VIEW( hnd->view_handler_compress ), NULL );
        if ( hnd->mode != HANDLER_MODE_FILE )
        {
            if ( !err_msg )
                err_msg = ptk_handler_load_script( hnd->mode, HANDLER_EXTRACT,
                                handler_xset,
                                GTK_TEXT_VIEW( hnd->view_handler_extract ), NULL );
            if ( !err_msg )
                err_msg = ptk_handler_load_script( hnd->mode, HANDLER_LIST,
                                handler_xset,
                                GTK_TEXT_VIEW( hnd->view_handler_list ), NULL );
        }
        if ( err_msg )
        {
            xset_msg_dialog( GTK_WIDGET( hnd->dlg ), GTK_MESSAGE_ERROR,
                                _("Error Loading Handler"), NULL, 0, 
                                err_msg, NULL, NULL );
            g_free( err_msg );
        }
    }
    // Run In Terminal checkboxes
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( hnd->chkbtn_handler_compress_term ),
                                    handler_xset->in_terminal == XSET_B_TRUE );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( hnd->chkbtn_handler_extract_term ),
                                    handler_xset->keep_terminal == XSET_B_TRUE );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( hnd->chkbtn_handler_list_term ),
                                    handler_xset->scroll_lock == XSET_B_TRUE );
}

static void config_unload_handler_settings( HandlerData* hnd )
{
    // Disabling main change buttons
    gtk_widget_set_sensitive( GTK_WIDGET( hnd->btn_remove ), FALSE );
    gtk_widget_set_sensitive( GTK_WIDGET( hnd->btn_apply ), FALSE );
    gtk_widget_set_sensitive( GTK_WIDGET( hnd->btn_up ), FALSE );
    gtk_widget_set_sensitive( GTK_WIDGET( hnd->btn_down ), FALSE );
    gtk_widget_set_sensitive( GTK_WIDGET( hnd->btn_defaults0 ), FALSE );

    // Unchecking handler
    if ( hnd->mode != HANDLER_MODE_FILE )
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( hnd->chkbtn_handler_enabled ),
                                  FALSE);

    // Resetting all widgets
    gtk_entry_set_text( GTK_ENTRY( hnd->entry_handler_name ), g_strdup( "" ) );
    gtk_entry_set_text( GTK_ENTRY( hnd->entry_handler_mime ), g_strdup( "" ) );
    gtk_entry_set_text( GTK_ENTRY( hnd->entry_handler_extension ), g_strdup( "" ) );
    ptk_handler_load_text_view( GTK_TEXT_VIEW( hnd->view_handler_compress ), NULL );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( hnd->chkbtn_handler_compress_term ),
                                  FALSE);
    ptk_handler_load_text_view( GTK_TEXT_VIEW( hnd->view_handler_extract ), NULL );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( hnd->chkbtn_handler_extract_term ),
                                  FALSE);
    ptk_handler_load_text_view( GTK_TEXT_VIEW( hnd->view_handler_list ), NULL );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( hnd->chkbtn_handler_list_term ),
                                  FALSE);
}

static void populate_archive_handlers( HandlerData* hnd, XSet* def_handler_set )
{
    /* Fetching available archive handlers (literally gets member s from
     * the xset) - user-defined order has already been set */
    char* archive_handlers_s = xset_get_s( handler_conf_xset[hnd->mode] );

    // Making sure archive handlers are available
    if ( !archive_handlers_s )
        return;

    gchar** archive_handlers = g_strsplit( archive_handlers_s, " ", -1 );

    // Debug code
    //g_message("archive_handlers_s: %s", archive_handlers_s);

    // Looping for handlers (NULL-terminated list)
    GtkTreeIter iter;
    GtkTreeIter def_handler_iter;
    def_handler_iter.stamp = 0;
    int i;
    for (i = 0; archive_handlers[i] != NULL; ++i)
    {
        if ( g_str_has_prefix( archive_handlers[i],
                                            handler_cust_prefix[hnd->mode] ) )
        {
            // Fetching handler  - ignoring invalid handler xset names
            XSet* handler_xset = xset_is( archive_handlers[i] );
            if ( handler_xset )
            {
                // Obtaining appending iterator for treeview model
                gtk_list_store_append( GTK_LIST_STORE( hnd->list ), &iter );
                // Adding handler to model
                const char* disabled = hnd->mode == HANDLER_MODE_FILE ?
                                                    _("(optional)") :
                                                    _("(disabled)");
                char* dis_name = g_strdup_printf( "%s %s",
                                    handler_xset->menu_label,
                                    handler_xset->b == XSET_B_TRUE ? "" :
                                        disabled );
                gtk_list_store_set( GTK_LIST_STORE( hnd->list ), &iter,
                                    COL_XSET_NAME, archive_handlers[i],
                                    COL_HANDLER_NAME, dis_name,
                                    -1 );
                g_free( dis_name );
                if ( def_handler_set == handler_xset )
                    def_handler_iter = iter;
            }
        }
    }

    // Clearing up archive_handlers
    g_strfreev( archive_handlers );

    // Fetching selection from treeview
    GtkTreeSelection* selection;
    selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( hnd->view_handlers ) );

    /* Loading first or default archive handler if there is one and no selection is
     * present */
    if ( i > 0 &&
        !gtk_tree_selection_get_selected( GTK_TREE_SELECTION( selection ),
         NULL, NULL ) )
    {
        GtkTreePath* tree_path;
        if ( def_handler_set && def_handler_iter.stamp )
            tree_path = gtk_tree_model_get_path( GTK_TREE_MODEL( hnd->list ),
                                                 &def_handler_iter );
        else
            tree_path = gtk_tree_path_new_first();
        gtk_tree_selection_select_path( GTK_TREE_SELECTION( selection ),
                                tree_path );
        gtk_tree_path_free( tree_path );
    }
}

static void on_configure_drag_end( GtkWidget* widget,
                                   GdkDragContext* drag_context,
                                   HandlerData* hnd )
{
    // Regenerating archive handlers list xset
    // Obtaining iterator pointing at first handler
    GtkTreeIter iter;
    if (!gtk_tree_model_get_iter_first( GTK_TREE_MODEL( hnd->list ), &iter ))
    {
        // Failed to get iterator - warning user and exiting
        g_warning("Drag'n'drop end event detected, but unable to get an"
        " iterator to the start of the model!");
        return;
    }

    // Looping for all handlers
    gchar* xset_name;
    gchar* archive_handlers = g_strdup( "" );
    gchar* archive_handlers_temp;
    do
    {
        gtk_tree_model_get( GTK_TREE_MODEL( hnd->list ), &iter,
                            COL_XSET_NAME, &xset_name,
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
        g_free( xset_name );
    }
    while(gtk_tree_model_iter_next( GTK_TREE_MODEL( hnd->list ), &iter ));

    // Saving the new archive handlers list
    xset_set( handler_conf_xset[hnd->mode], "s", archive_handlers );
    g_free(archive_handlers);

    // Saving settings
    xset_autosave( FALSE, FALSE );

#if 0
    /* Ensuring first handler is selected (otherwise none are)
     * It seems the last selected row is re-selected if this code is not used. */
    if ( widget )
    {
        GtkTreeSelection *selection = gtk_tree_view_get_selection(
                                            GTK_TREE_VIEW( widget ) );
        GtkTreePath *new_path = gtk_tree_path_new_first();
        gtk_tree_selection_select_path( GTK_TREE_SELECTION( selection ),
                                new_path );
        gtk_tree_path_free( new_path );
    }
#endif
}

static void on_configure_button_press( GtkButton* widget, HandlerData* hnd )
{
    int i;
    char* err_msg = NULL;
    
    const gchar* handler_name = gtk_entry_get_text(
                                    GTK_ENTRY ( hnd->entry_handler_name ) );
    const gchar* handler_mime = gtk_entry_get_text(
                                    GTK_ENTRY ( hnd->entry_handler_mime ) );
    const gchar* handler_extension = gtk_entry_get_text(
                        GTK_ENTRY ( hnd->entry_handler_extension ) );
    const gchar* handler_icon;
    if ( hnd->entry_handler_icon )
        handler_icon = gtk_entry_get_text(
                                    GTK_ENTRY ( hnd->entry_handler_icon ) );
    else
        handler_icon = NULL;
    
    const gboolean handler_compress_term = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON ( hnd->chkbtn_handler_compress_term ) );
    const gboolean handler_extract_term = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON ( hnd->chkbtn_handler_extract_term ) );
    const gboolean handler_list_term = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON ( hnd->chkbtn_handler_list_term ) );

    // Fetching the model and iter from the selection
    GtkTreeIter it, iter;
    GtkTreeModel* model;
    gchar* handler_name_from_model = NULL;  // Used to detect renames
    gchar* xset_name = NULL;
    XSet* handler_xset = NULL;

    // Fetching selection from treeview
    GtkTreeSelection* selection;
    selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( hnd->view_handlers ) );

    // Getting selection fails if there are no handlers
    if ( gtk_tree_selection_get_selected( GTK_TREE_SELECTION( selection ),
         &model, &it ) )
    {
        // Succeeded - handlers present
        // Fetching data from the model based on the iterator.
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

    if ( GTK_WIDGET( widget ) == GTK_WIDGET( hnd->btn_add ) )
    {
        // Exiting if there is no handler to add
        if ( !( handler_name && handler_name[0] ) )
            goto _clean_exit;

        // Adding new handler as a copy of the current active handler
        XSet* new_handler_xset = add_new_handler( hnd->mode );
        new_handler_xset->b = gtk_toggle_button_get_active(
                                GTK_TOGGLE_BUTTON( hnd->chkbtn_handler_enabled )
                              ) ? XSET_B_TRUE : XSET_B_FALSE;
        new_handler_xset->disable = FALSE;  // not default - save in session
        xset_set_set( new_handler_xset, "label", handler_name );
        xset_set_set( new_handler_xset, "s", handler_mime );  // Mime Type(s) or whitelist
        xset_set_set( new_handler_xset, "x", handler_extension );  // Extension(s) or blacklist
        new_handler_xset->in_terminal = handler_compress_term ?
                                                XSET_B_TRUE : XSET_B_UNSET;
        err_msg = ptk_handler_save_script( hnd->mode, HANDLER_COMPRESS,
                                new_handler_xset,
                                GTK_TEXT_VIEW( hnd->view_handler_compress ), NULL );
        if ( hnd->mode == HANDLER_MODE_FILE )
        {
            if ( handler_icon )
                 xset_set_set( new_handler_xset, "icn", handler_icon );
        }
        else
        {
            new_handler_xset->keep_terminal = handler_extract_term ?
                                                    XSET_B_TRUE : XSET_B_UNSET;
            new_handler_xset->scroll_lock = handler_list_term ?
                                                    XSET_B_TRUE : XSET_B_UNSET;
            if ( !err_msg )
                err_msg = ptk_handler_save_script( hnd->mode, HANDLER_EXTRACT,
                                    new_handler_xset,
                                    GTK_TEXT_VIEW( hnd->view_handler_extract ), NULL );
            if ( !err_msg )
                err_msg = ptk_handler_save_script( hnd->mode, HANDLER_LIST,
                                    new_handler_xset,
                                    GTK_TEXT_VIEW( hnd->view_handler_list ), NULL );
        }

        // Obtaining appending iterator for treeview model
        gtk_list_store_prepend( GTK_LIST_STORE( hnd->list ), &iter );

        // Adding handler to model
        const char* disabled = hnd->mode == HANDLER_MODE_FILE ?
                                            _("(optional)") :
                                            _("(disabled)");
        char* dis_name = g_strdup_printf( "%s %s",
                                    handler_name,
                                    new_handler_xset->b == XSET_B_TRUE ? "" :
                                        disabled );
        gtk_list_store_set( GTK_LIST_STORE( hnd->list ), &iter,
                            COL_XSET_NAME, new_handler_xset->name,
                            COL_HANDLER_NAME, dis_name,
                            -1 );
        g_free( dis_name );
        
        // Updating available archive handlers list
        if (g_strcmp0( xset_get_s( handler_conf_xset[hnd->mode] ), "" ) <= 0)
        {
            // No handlers present - adding new handler
            xset_set( handler_conf_xset[hnd->mode], "s", new_handler_xset->name );
        }
        else
        {
            // Adding new handler to handlers
            gchar* new_handlers_list = g_strdup_printf( "%s %s",
                                new_handler_xset->name,
                                xset_get_s( handler_conf_xset[hnd->mode] ) );
            xset_set( handler_conf_xset[hnd->mode], "s", new_handlers_list );

            // Clearing up
            g_free(new_handlers_list);
        }

        // Activating the new handler - the normal loading code
        // automatically kicks in
        GtkTreePath* new_handler_path = gtk_tree_model_get_path(
                                                    GTK_TREE_MODEL( model ),
                                                    &iter );
        gtk_tree_view_set_cursor( GTK_TREE_VIEW( hnd->view_handlers ),
                                        new_handler_path, NULL, FALSE );
        gtk_tree_path_free( new_handler_path );

        // Making sure the remove and apply buttons are sensitive
        gtk_widget_set_sensitive( hnd->btn_remove, TRUE );
        gtk_widget_set_sensitive( hnd->btn_apply, TRUE );

        /* Validating - remember that IG wants the handler to be saved
         * even with invalid commands */
        validate_archive_handler( hnd );
    }
    else if ( GTK_WIDGET( widget ) == GTK_WIDGET( hnd->btn_apply ) )
    {
        // Exiting if apply has been pressed when no handlers are present
        if (xset_name == NULL) goto _clean_exit;

        // Validating - to be saved even with invalid commands
        validate_archive_handler( hnd );

        // Determining current handler enabled state
        gboolean handler_enabled = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON( hnd->chkbtn_handler_enabled ) );

        // Checking if the handler has been renamed
        if (g_strcmp0( handler_name_from_model, handler_name ) != 0)
        {
            // It has - updating model
            const char* disabled = hnd->mode == HANDLER_MODE_FILE ?
                                                _("(optional)") :
                                                _("(disabled)");
            char* dis_name = g_strdup_printf( "%s %s",
                                    handler_name,
                                    handler_enabled ? "" : disabled );
            gtk_list_store_set( GTK_LIST_STORE( model ), &it,
                        COL_XSET_NAME, xset_name,
                        COL_HANDLER_NAME, dis_name,
                        -1 );
            g_free( dis_name );
        }

        // Saving archive handler
        handler_xset->b = handler_enabled ? XSET_B_TRUE : XSET_B_UNSET;
        handler_xset->disable = FALSE;  // not default - save in session
        xset_set_set( handler_xset, "label", handler_name );
        xset_set_set( handler_xset, "s", handler_mime );
        xset_set_set( handler_xset, "x", handler_extension );
        handler_xset->in_terminal = handler_compress_term ?
                                                XSET_B_TRUE : XSET_B_UNSET;
        err_msg = ptk_handler_save_script( hnd->mode, HANDLER_COMPRESS,
                            handler_xset,
                            GTK_TEXT_VIEW( hnd->view_handler_compress ), NULL );
        if ( hnd->mode == HANDLER_MODE_FILE )
        {
            if ( handler_icon )
                 xset_set_set( handler_xset, "icn", handler_icon );
        }
        else
        {
            handler_xset->keep_terminal = handler_extract_term ?
                                                    XSET_B_TRUE : XSET_B_UNSET;
            handler_xset->scroll_lock = handler_list_term ?
                                                    XSET_B_TRUE : XSET_B_UNSET;
            if ( !err_msg )
                err_msg = ptk_handler_save_script( hnd->mode, HANDLER_EXTRACT,
                            handler_xset,
                            GTK_TEXT_VIEW( hnd->view_handler_extract ), NULL );
            if ( !err_msg )
                err_msg = ptk_handler_save_script( hnd->mode, HANDLER_LIST,
                            handler_xset,
                            GTK_TEXT_VIEW( hnd->view_handler_list ), NULL );
        }
    }
    else if ( GTK_WIDGET( widget ) == GTK_WIDGET( hnd->btn_remove ) )
    {
        // Exiting if remove has been pressed when no handlers are present
        if (xset_name == NULL) goto _clean_exit;

        // Updating available archive handlers list - fetching current
        // handlers
        const char* archive_handlers_s =
                                    xset_get_s( handler_conf_xset[hnd->mode] );
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
        xset_set( handler_conf_xset[hnd->mode], "s", new_archive_handlers_s );

        // Deleting xset
        xset_custom_delete( handler_xset, FALSE );
        handler_xset = NULL;

        // Removing handler from the list
        gtk_list_store_remove( GTK_LIST_STORE( model ), &it );

        if (g_strcmp0( new_archive_handlers_s, "" ) == 0)
        {
            /* Making remove and apply buttons insensitive if the last
             * handler has been removed */
            gtk_widget_set_sensitive( GTK_WIDGET( hnd->btn_remove ), FALSE );
            gtk_widget_set_sensitive( GTK_WIDGET( hnd->btn_apply ), FALSE );

            /* Now that all are removed, the user needs sensitive widgets
             * to be able to add a further handler */
            gtk_widget_set_sensitive( hnd->chkbtn_handler_enabled,
                                      TRUE );
            gtk_widget_set_sensitive( hnd->entry_handler_name,
                                      TRUE );
            gtk_widget_set_sensitive( hnd->entry_handler_mime,
                                      TRUE );
            gtk_widget_set_sensitive( hnd->entry_handler_extension,
                                      TRUE );
            gtk_widget_set_sensitive( hnd->view_handler_compress,
                                      TRUE );
            gtk_widget_set_sensitive( hnd->view_handler_extract,
                                      TRUE );
            gtk_widget_set_sensitive( hnd->view_handler_list,
                                      TRUE );
            gtk_widget_set_sensitive( hnd->chkbtn_handler_compress_term,
                                      TRUE );
            gtk_widget_set_sensitive( hnd->chkbtn_handler_extract_term,
                                      TRUE );
            gtk_widget_set_sensitive( hnd->chkbtn_handler_list_term,
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
    else if ( GTK_WIDGET( widget ) == GTK_WIDGET( hnd->btn_up ) ||
                        GTK_WIDGET( widget ) == GTK_WIDGET( hnd->btn_down ) )
    {
        if ( !handler_xset )
            // no row selected
            goto _clean_exit;
            
        // Note: gtk_tree_model_iter_previous requires GTK3, so not using
        GtkTreeIter iter_prev;
        if ( !gtk_tree_model_get_iter_first( GTK_TREE_MODEL( model ), &iter ) )
            goto _clean_exit;
        iter_prev = iter;
        do
        {
            // find my it (stamp is NOT unique - compare whole struct)
            if ( iter.stamp == it.stamp && iter.user_data == it.user_data &&
                                           iter.user_data2 == it.user_data2 &&
                                           iter.user_data3 == it.user_data3 )
            {
                if ( GTK_WIDGET( widget ) == GTK_WIDGET( hnd->btn_up ) )
                    iter = iter_prev;
                else
                    if ( !gtk_tree_model_iter_next( GTK_TREE_MODEL( model ),
                                                                    &iter ) )
                        goto _clean_exit;  // was last row
                break;
            }
            iter_prev = iter;
        } while ( gtk_tree_model_iter_next( GTK_TREE_MODEL( model ), &iter ) );
        gtk_list_store_swap( GTK_LIST_STORE( model ), &it, &iter );
        // save the new list
        on_configure_drag_end( NULL, NULL, hnd );
    }
    
    // Saving settings
    xset_autosave( FALSE, FALSE );

    if ( err_msg )
    {
        xset_msg_dialog( GTK_WIDGET( hnd->dlg ), GTK_MESSAGE_ERROR,
                            _("Error Saving Handler"), NULL, 0, 
                            err_msg, NULL, NULL );
        g_free( err_msg );
    }

_clean_exit:
    g_free( xset_name );
    g_free( handler_name_from_model );
}

static void on_configure_changed( GtkTreeSelection* selection,
                                  HandlerData* hnd )
{
    /* This event is triggered when the selected row is changed through
     * the keyboard, or no row is selected at all */

    // Fetching the model and iter from the selection
    GtkTreeIter it;
    GtkTreeModel* model;
    if ( !gtk_tree_selection_get_selected( selection, &model, &it ) )
    {
        // User has unselected all rows - removing loaded handler
        config_unload_handler_settings( hnd );
        return;
    }

    // Fetching data from the model based on the iterator.
    gchar* xset_name;
    gtk_tree_model_get( model, &it,
                        COL_XSET_NAME, &xset_name,
                        -1 );

    // Loading new archive handler values
    config_load_handler_settings( NULL, xset_name, NULL, hnd );
    g_free( xset_name );
    
    /* Focussing archive handler name
     * Selects the text rather than just placing the cursor at the start
     * of the text... */
    /*GtkWidget* entry_handler_name = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                                "entry_handler_name" );
    gtk_widget_grab_focus( entry_handler_name );*/
}

static void on_configure_handler_enabled_check( GtkToggleButton *togglebutton,
                                                HandlerData* hnd )
{
    /* When no handler is selected (i.e. the user selects outside of the
     * populated handlers list), the enabled checkbox might be checked
     * off - however the widgets must not be set insensitive when this
     * happens */

    if ( hnd->mode == HANDLER_MODE_FILE )
        return;

    // Fetching selection from treeview
    GtkTreeSelection* selection;
    selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( hnd->view_handlers ) );
    
    // Exiting if no handler is selected
    GtkTreeIter it;
    GtkTreeModel* model;
    if ( !gtk_tree_selection_get_selected( selection, &model, &it ) )
        return;

    // Fetching current status
    gboolean enabled = gtk_toggle_button_get_active(
                                        GTK_TOGGLE_BUTTON ( togglebutton ) );

    // Setting sensitive/insensitive various widgets as appropriate
    gtk_widget_set_sensitive( hnd->entry_handler_name, enabled );
    gtk_widget_set_sensitive( hnd->entry_handler_mime, enabled );
    gtk_widget_set_sensitive( hnd->entry_handler_extension, enabled );
    gtk_widget_set_sensitive( hnd->view_handler_compress, enabled );
    gtk_widget_set_sensitive( hnd->view_handler_extract, enabled );
    gtk_widget_set_sensitive( hnd->view_handler_list, enabled );
    gtk_widget_set_sensitive( hnd->chkbtn_handler_compress_term, enabled );
    gtk_widget_set_sensitive( hnd->chkbtn_handler_extract_term, enabled );
    gtk_widget_set_sensitive( hnd->chkbtn_handler_list_term, enabled );
}

/*igcr some duplication here with on_configure_changed() - can you call
 * on_configure_changed(), or can a single event be used for selection
 * changed?  row-activated is when a row is activated by clicking
 * or double-clicking, or via keypress space/enter, not merely selected.  */
static void on_configure_row_activated( GtkTreeView* view,
                                        GtkTreePath* tree_path,
                                        GtkTreeViewColumn* col,
                                        HandlerData* hnd )
{
    // This event is triggered when the selected row is changed by the
    // mouse

    // Fetching the model from the view
    GtkTreeModel* model = gtk_tree_view_get_model( GTK_TREE_VIEW( view ) );

    // Obtaining an iterator based on the view position
    GtkTreeIter it;
    if ( !gtk_tree_model_get_iter( model, &it, tree_path ) )
        return;

    // Fetching data from the model based on the iterator.
    gchar* xset_name;
    gtk_tree_model_get( model, &it,
                        COL_XSET_NAME, &xset_name,
                        -1 );

    // Loading new archive handler values
    config_load_handler_settings( NULL, xset_name, NULL, hnd );
    g_free( xset_name );
    
    // Focussing archive handler name
    // Selects the text rather than just placing the cursor at the start
    // of the text...
    /*GtkWidget* entry_handler_name = (GtkWidget*)g_object_get_data( G_OBJECT( dlg ),
                                                "entry_handler_name" );
    gtk_widget_grab_focus( entry_handler_name );*/
}

static void restore_defaults( HandlerData* hnd, gboolean all )
{
    if ( all )
    {
        int response = xset_msg_dialog( GTK_WIDGET( hnd->dlg ), GTK_MESSAGE_WARNING,
                            _("Restore Default Handlers"), NULL,
                            GTK_BUTTONS_YES_NO, _("Missing default handlers will be restored.\n\nAlso OVERWRITE ALL EXISTING default handlers?"),
                            NULL, NULL);
        if ( response != GTK_RESPONSE_YES && response != GTK_RESPONSE_NO )
            // dialog was closed with no button pressed - cancel
            return;
        ptk_handler_add_defaults( hnd->mode, response == GTK_RESPONSE_YES, TRUE );

        /* Reset archive handlers list (this also selects
         * the first handler and therefore populates the handler widgets) */
        gtk_list_store_clear( GTK_LIST_STORE( hnd->list ) );
        populate_archive_handlers( hnd, NULL );
    }
    else
    {
        // Fetching selection from treeview
        GtkTreeSelection* selection;
        selection = gtk_tree_view_get_selection(
                                        GTK_TREE_VIEW( hnd->view_handlers ) );
        
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
        if ( !( xset_name && g_str_has_prefix( xset_name,
                                            handler_def_prefix[hnd->mode] ) ) )
        {
            g_free( xset_name );
            return;
        }
        
        // get default handler
        int i, nelements;
        const Handler* handler = NULL;

        if ( hnd->mode == HANDLER_MODE_ARC )
            nelements = G_N_ELEMENTS( handlers_arc );
        else if ( hnd->mode == HANDLER_MODE_FS )
            nelements = G_N_ELEMENTS( handlers_fs );
        else if ( hnd->mode == HANDLER_MODE_NET )
            nelements = G_N_ELEMENTS( handlers_net );
        else if ( hnd->mode == HANDLER_MODE_FILE )
            nelements = G_N_ELEMENTS( handlers_file );
        else
            return;
            
        gboolean found_handler = FALSE;
        for ( i = 0; i < nelements; i++ )
        {
            if ( hnd->mode == HANDLER_MODE_ARC )
                handler = &handlers_arc[i];
            else if ( hnd->mode == HANDLER_MODE_FS )
                handler = &handlers_fs[i];
            else if ( hnd->mode == HANDLER_MODE_NET )
                handler = &handlers_net[i];
            else
                handler = &handlers_file[i];
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
        set->in_terminal = handler->compress_term ? XSET_B_TRUE : XSET_B_UNSET;
        if ( hnd->mode != HANDLER_MODE_FILE )
        {
            set->keep_terminal = handler->extract_term ? XSET_B_TRUE : XSET_B_UNSET;
            set->scroll_lock = handler->list_term ? XSET_B_TRUE : XSET_B_UNSET;
        }
        set->b = XSET_B_TRUE;

        // show fake xset values
        config_load_handler_settings( set, NULL, handler, hnd );

        g_slice_free( XSet, set );
    }
}

static gboolean validate_archive_handler( HandlerData* hnd )
{
    if ( hnd->mode != HANDLER_MODE_ARC )
        // only archive handlers currently have validity checks
        return TRUE;
    
    const gchar* handler_name = gtk_entry_get_text(
                                    GTK_ENTRY ( hnd->entry_handler_name ) );
    const gchar* handler_mime = gtk_entry_get_text(
                                    GTK_ENTRY ( hnd->entry_handler_mime ) );
    const gchar* handler_extension = gtk_entry_get_text(
                                    GTK_ENTRY ( hnd->entry_handler_extension ) );
    const gboolean handler_compress_term = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON ( hnd->chkbtn_handler_compress_term ) );
    const gboolean handler_extract_term = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON ( hnd->chkbtn_handler_extract_term ) );
    const gboolean handler_list_term = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON ( hnd->chkbtn_handler_list_term ) );

    /* Validating data. Note that data straight from widgets shouldnt
     * be modified or stored
     * Note that archive creation also allows for a command to be
     * saved */
    if (g_strcmp0( handler_name, "" ) <= 0)
    {
        /* Handler name not set - warning user and exiting. Note
         * that the created dialog does not have an icon set */
        xset_msg_dialog( GTK_WIDGET( hnd->dlg ), GTK_MESSAGE_WARNING,
                            _(dialog_titles[hnd->mode]), NULL, FALSE,
                            _("Please enter a valid handler name."),
                            NULL, NULL );
        gtk_widget_grab_focus( hnd->entry_handler_name );
        return FALSE;
    }

    // MIME and Pathname can't both be empty
    if ( g_strcmp0( handler_mime, "" ) <= 0 &&
         g_strcmp0( handler_extension, "" ) <= 0 )
    {
        xset_msg_dialog( GTK_WIDGET( hnd->dlg ), GTK_MESSAGE_WARNING,
                            _(dialog_titles[hnd->mode]), NULL, FALSE,
                            _("Please enter a valid MIME Type or Pathname "
                            "pattern."),
                            NULL, NULL );
        gtk_widget_grab_focus( hnd->entry_handler_mime );
        return FALSE;
    }

    gchar* handler_compress, *handler_extract, *handler_list;

    handler_compress = ptk_handler_get_text_view(
                                GTK_TEXT_VIEW ( hnd->view_handler_compress ) );
    handler_extract = ptk_handler_get_text_view(
                                GTK_TEXT_VIEW ( hnd->view_handler_extract ) );
    handler_list = ptk_handler_get_text_view(
                                GTK_TEXT_VIEW ( hnd->view_handler_list ) );
    gboolean ret = TRUE;
    
    /* Other settings are commands to run in different situations -
     * since different handlers may or may not need different
     * commands, empty commands are allowed but if something is given,
     * relevant substitution characters should be in place */

    /* Compression handler validation - remember to maintain this code
     * in ptk_file_archiver_create too
     * Checking if a compression command has been entered */
    if (g_strcmp0( handler_compress, "" ) != 0 )
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
            xset_msg_dialog( GTK_WIDGET( hnd->dlg ), GTK_MESSAGE_WARNING,
                            _(dialog_titles[hnd->mode]), NULL, FALSE,
                            _("The following "
                            "substitution variables should probably be in the "
                            "compression command:\n\n"
                            "One of the following:\n\n"
                            "%%%%n: First selected file/directory to"
                            " archive\n"
                            "%%%%N: All selected files/directories to"
                            " archive\n\n"
                            "and one of the following:\n\n"
                            "%%%%o: Resulting single archive\n"
                            "%%%%O: Resulting archive per source "
                            "file/directory (see %%%%n/%%%%N)"), NULL, NULL );
            gtk_widget_grab_focus( hnd->view_handler_compress );
            ret = FALSE;
            goto _cleanup;
        }
    }

    if (g_strcmp0( handler_extract, "" ) != 0 &&
        (
            !g_strstr_len( handler_extract, -1, "%x" )
        ))
    {
        /* Not all substitution characters are in place - warning
         * user and exiting. Note that the created dialog does not
         * have an icon set */
        xset_msg_dialog( GTK_WIDGET( hnd->dlg ), GTK_MESSAGE_WARNING,
                            _(dialog_titles[hnd->mode]), NULL, FALSE,
                            _("The following "
                            "variables should be in the extraction "
                            "command:\n\n%%%%x: "
                            "Archive to extract"), NULL, NULL );
        gtk_widget_grab_focus( hnd->view_handler_extract );
        ret = FALSE;
        goto _cleanup;
    }

    if (g_strcmp0( handler_list, "" ) != 0 &&
        (
            !g_strstr_len( handler_list, -1, "%x" )
        ))
    {
        /* Not all substitution characters are in place  - warning
         * user and exiting. Note that the created dialog does not
         * have an icon set */
        xset_msg_dialog( GTK_WIDGET( hnd->dlg ), GTK_MESSAGE_WARNING,
                            _(dialog_titles[hnd->mode]), NULL, FALSE,
                            _("The following "
                            "variables should be in the list "
                            "command:\n\n%%%%x: "
                            "Archive to list"),
                            NULL, NULL );
        gtk_widget_grab_focus( hnd->view_handler_list );
        ret = FALSE;
        goto _cleanup;
    }

_cleanup:
    g_free( handler_compress );
    g_free( handler_extract );
    g_free( handler_list );

    // Validation passed
    return ret;
}

void on_textview_font_change( GtkMenuItem* item, HandlerData* hnd )
{
    char* fontname = xset_get_s( "context_dlg" );
    PangoFontDescription* font_desc = fontname ?
                        pango_font_description_from_string( fontname ) : NULL;
    gtk_widget_modify_font( GTK_WIDGET( hnd->view_handler_compress ), font_desc );
    gtk_widget_modify_font( GTK_WIDGET( hnd->view_handler_extract ), font_desc );
    gtk_widget_modify_font( GTK_WIDGET( hnd->view_handler_list ), font_desc );
    if ( font_desc )
        pango_font_description_free( font_desc );
}

void on_textview_popup( GtkTextView *input, GtkMenu *menu, HandlerData* hnd )
{
    // uses same xsets as item-prop.c:on_script_popup()
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    XSet* set = xset_get( "sep_ctxt" );
    set->menu_style = XSET_MENU_SEP;
    set->browser = NULL;
    set->desktop = NULL;
    xset_add_menuitem( NULL, NULL, GTK_WIDGET( menu ), accel_group, set );
    set = xset_set_cb( "context_dlg", on_textview_font_change, hnd );
    set->browser = NULL;
    set->desktop = NULL;
    xset_add_menuitem( NULL, NULL, GTK_WIDGET( menu ), accel_group, set );
    
    gtk_widget_show_all( GTK_WIDGET( menu ) );
}

gboolean on_activate_link( GtkLabel* label, gchar* uri, HandlerData* hnd )
{
    // click apply to save handler
    on_configure_button_press( GTK_BUTTON( hnd->btn_apply ), hnd );
    // open in editor
    int action = atoi( uri );
    if ( action > HANDLER_LIST || action < 0 )
        return TRUE;

    // Fetching selection from treeview
    GtkTreeSelection* selection;
    selection = gtk_tree_view_get_selection(
                                        GTK_TREE_VIEW( hnd->view_handlers ) );
    
    // Exiting if no handler is selected
    GtkTreeIter it;
    GtkTreeModel* model;
    if ( !gtk_tree_selection_get_selected( selection, &model, &it ) )
        return TRUE;

    gchar* xset_name = NULL;
    gtk_tree_model_get( model, &it,
                        COL_XSET_NAME, &xset_name,
                        -1 );
    XSet* set = xset_is( xset_name );
    g_free( xset_name );
    if ( !( set && set->disable == FALSE && set->b == XSET_B_TRUE ) )
        return TRUE;
    char* script = ptk_handler_get_command( hnd->mode, action, set );
    if ( !script )
        return TRUE;
    xset_edit( hnd->dlg, script, FALSE, FALSE );
    g_free( script );
    return TRUE;
}

static gboolean on_textview_keypress ( GtkWidget *widget, GdkEventKey *event,
                                                HandlerData* hnd )
{   // also used on dlg keypress
    if ( ( event->keyval == GDK_KEY_Return || 
                                event->keyval == GDK_KEY_KP_Enter ) )
    {
        int keymod = ( event->state & ( GDK_SHIFT_MASK | GDK_CONTROL_MASK |
                 GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK ) );
        if ( keymod == GDK_MOD1_MASK )
        {
            // Alt+Enter == Open Handler Command In Editor
            if ( widget == hnd->view_handler_compress )
                keymod = 0;
            else if ( widget == hnd->view_handler_extract )
                keymod = 1;
            else if ( widget == hnd->view_handler_list )
                keymod = 2;
            else
                return FALSE;
            char* uri = g_strdup_printf( "%d", keymod );            
            on_activate_link( NULL, uri, hnd );
            g_free( uri );
            return TRUE;
        }
    }
    else if ( event->keyval == GDK_KEY_F1 )
    {
        // F1 show help
        int keymod = ( event->state & ( GDK_SHIFT_MASK | GDK_CONTROL_MASK |
                 GDK_MOD1_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK ) );
        if ( keymod == 0 )
        {
            const char* help;
            if ( hnd->mode == HANDLER_MODE_ARC )
                help = "#handlers-arc";
            else if ( hnd->mode == HANDLER_MODE_FS )
                help = "#handlers-dev";
            else if ( hnd->mode == HANDLER_MODE_NET )
                help = "#handlers-pro";
            else if ( hnd->mode == HANDLER_MODE_FILE )
                help = "#handlers-fil";
            else
                help = NULL;
            xset_show_help( hnd->dlg, NULL, help );
            return TRUE;
        }        
    }
    return FALSE;
}

void ptk_handler_show_config( int mode, PtkFileBrowser* file_browser,
                              XSet* def_handler_set )
{
    HandlerData* hnd = g_slice_new0( HandlerData );
    hnd->mode = mode;
    
    /* Create handlers dialog
     * Extra NULL on the NULL-terminated list to placate an irrelevant
     * compilation warning */
    hnd->parent = file_browser ? gtk_widget_get_toplevel(
                                GTK_WIDGET( file_browser->main_window ) ) :
                                NULL;
    hnd->dlg = gtk_dialog_new_with_buttons( _(dialog_titles[mode]),
                    hnd->parent ? GTK_WINDOW( hnd->parent ) : NULL,
                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                    NULL, NULL );
    gtk_container_set_border_width( GTK_CONTAINER ( hnd->dlg ), 5 );
    g_signal_connect( G_OBJECT( hnd->dlg ), "key-press-event",
                                    G_CALLBACK( on_textview_keypress ), hnd );

    // Debug code
    //g_message( "Parent window title: %s", gtk_window_get_title( GTK_WINDOW( hnd->parent ) ) );

    // Forcing dialog icon
    xset_set_window_icon( GTK_WINDOW( hnd->dlg ) );

    // Setting saved dialog size
    int width = xset_get_int( handler_conf_xset[HANDLER_MODE_ARC], "x" );
    int height = xset_get_int( handler_conf_xset[HANDLER_MODE_ARC], "y" );
    if ( width && height )
        gtk_window_set_default_size( GTK_WINDOW( hnd->dlg ), width, height );

    // Adding the help button but preventing it from taking the focus on click
    gtk_button_set_focus_on_click(
                                    GTK_BUTTON(
                                        gtk_dialog_add_button(
                                            GTK_DIALOG( hnd->dlg ),
                                            GTK_STOCK_HELP,
                                            GTK_RESPONSE_HELP
                                        )
                                    ),
                                    FALSE );

    // Adding standard buttons and saving references in the dialog
    // 'Restore defaults' button has custom text but a stock image
    hnd->btn_defaults = gtk_dialog_add_button( GTK_DIALOG( hnd->dlg ),
                                                _("All De_faults"),
                                                GTK_RESPONSE_NONE );
    GtkWidget* btn_defaults_image = xset_get_image( "GTK_STOCK_REVERT_TO_SAVED",
                                                GTK_ICON_SIZE_BUTTON );
    gtk_button_set_image( GTK_BUTTON( hnd->btn_defaults ),
                                        GTK_WIDGET ( btn_defaults_image ) );
    hnd->btn_defaults0 = gtk_dialog_add_button( GTK_DIALOG( hnd->dlg ),
                                                _("Defa_ults"),
                                                GTK_RESPONSE_NO );
    GtkWidget* btn_defaults_image0 = xset_get_image( "GTK_STOCK_REVERT_TO_SAVED",
                                                GTK_ICON_SIZE_BUTTON );
    gtk_button_set_image( GTK_BUTTON( hnd->btn_defaults0 ),
                                        GTK_WIDGET ( btn_defaults_image0 ) );
    hnd->btn_ok = gtk_dialog_add_button( GTK_DIALOG( hnd->dlg ), GTK_STOCK_CLOSE,
                                                   GTK_RESPONSE_OK );

    // Generating left-hand side of dialog
    GtkWidget* lbl_handlers = gtk_label_new( NULL );
    char* str = g_strdup_printf("<b>%s</b>", _(dialog_titles[mode]) );
    gtk_label_set_markup( GTK_LABEL( lbl_handlers ), str );
    g_free( str );
    gtk_misc_set_alignment( GTK_MISC( lbl_handlers ), 0, 0 );

    // Generating the main manager list
    // Creating model - xset name then handler name
    hnd->list = gtk_list_store_new( 2, G_TYPE_STRING, G_TYPE_STRING );

    // Creating treeview - setting single-click mode (normally this
    // widget is used for file lists, where double-clicking is the norm
    // for doing an action)
    hnd->view_handlers = exo_tree_view_new();
    gtk_tree_view_set_model( GTK_TREE_VIEW( hnd->view_handlers ),
                                    GTK_TREE_MODEL( hnd->list ) );
/*igcr probably doesn't need to be single click, as you're not using row
 * activation, only selection changed? */
    exo_tree_view_set_single_click( ((ExoTreeView*)hnd->view_handlers), TRUE );
    gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( hnd->view_handlers ), FALSE );

    // Turning the treeview into a scrollable widget
    GtkWidget* view_scroll = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( view_scroll ),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER( view_scroll ), hnd->view_handlers );

    // Enabling item reordering (GTK-handled drag'n'drop)
    gtk_tree_view_set_reorderable( GTK_TREE_VIEW( hnd->view_handlers ), TRUE );

    // Connecting treeview callbacks
    g_signal_connect( G_OBJECT( hnd->view_handlers ), "drag-end",
                        G_CALLBACK( on_configure_drag_end ),
                        hnd );
    g_signal_connect( G_OBJECT( hnd->view_handlers ), "row-activated",
                        G_CALLBACK( on_configure_row_activated ),
                        hnd );
    g_signal_connect( G_OBJECT( gtk_tree_view_get_selection(
                                    GTK_TREE_VIEW( hnd->view_handlers ) ) ),
                        "changed",
                        G_CALLBACK( on_configure_changed ),
                        hnd );

    // Adding column to the treeview
    GtkTreeViewColumn* col = gtk_tree_view_column_new();

    // Change columns to optimal size whenever the model changes
    gtk_tree_view_column_set_sizing( col, GTK_TREE_VIEW_COLUMN_AUTOSIZE );
    
    GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start( col, renderer, TRUE );

    // Tie model data to the column
    gtk_tree_view_column_add_attribute( col, renderer,
                                         "text", COL_HANDLER_NAME);

    gtk_tree_view_append_column ( GTK_TREE_VIEW( hnd->view_handlers ), col );

    // Set column to take all available space - false by default
    gtk_tree_view_column_set_expand ( col, TRUE );

    // Treeview widgets
    hnd->btn_remove = gtk_button_new_with_mnemonic( _("_Remove") );
    gtk_button_set_image( GTK_BUTTON( hnd->btn_remove ),
                                        xset_get_image( "GTK_STOCK_REMOVE",
                                        GTK_ICON_SIZE_BUTTON ) );
    gtk_button_set_focus_on_click( GTK_BUTTON( hnd->btn_remove ), FALSE );
    gtk_widget_set_sensitive( hnd->btn_remove, FALSE );
    g_signal_connect( G_OBJECT( hnd->btn_remove ), "clicked",
                        G_CALLBACK( on_configure_button_press ), hnd );

    hnd->btn_add = gtk_button_new_with_mnemonic( _("A_dd") );
    gtk_button_set_image( GTK_BUTTON( hnd->btn_add ),
                                        xset_get_image( "GTK_STOCK_ADD",
                                        GTK_ICON_SIZE_BUTTON ) );
    gtk_button_set_focus_on_click( GTK_BUTTON( hnd->btn_add ), FALSE );
    g_signal_connect( G_OBJECT( hnd->btn_add ), "clicked",
                        G_CALLBACK( on_configure_button_press ), hnd );

    hnd->btn_apply = gtk_button_new_with_mnemonic( _("Appl_y") );
    gtk_button_set_image( GTK_BUTTON( hnd->btn_apply ),
                                        xset_get_image( "GTK_STOCK_APPLY",
                                        GTK_ICON_SIZE_BUTTON ) );
    gtk_button_set_focus_on_click( GTK_BUTTON( hnd->btn_apply ), FALSE );
    gtk_widget_set_sensitive( hnd->btn_apply, FALSE );
    g_signal_connect( G_OBJECT( hnd->btn_apply ), "clicked",
                        G_CALLBACK( on_configure_button_press ), hnd );

    hnd->btn_up = gtk_button_new_with_mnemonic( _("U_p") );
    gtk_button_set_image( GTK_BUTTON( hnd->btn_up ),
                                        xset_get_image( "GTK_STOCK_GO_UP",
                                        GTK_ICON_SIZE_BUTTON ) );
    gtk_button_set_focus_on_click( GTK_BUTTON( hnd->btn_up ), FALSE );
    gtk_widget_set_sensitive( hnd->btn_up, FALSE );
    g_signal_connect( G_OBJECT( hnd->btn_up ), "clicked",
                        G_CALLBACK( on_configure_button_press ), hnd );

    hnd->btn_down = gtk_button_new_with_mnemonic( _("Do_wn") );
    gtk_button_set_image( GTK_BUTTON( hnd->btn_down ),
                                        xset_get_image( "GTK_STOCK_GO_DOWN",
                                        GTK_ICON_SIZE_BUTTON ) );
    gtk_button_set_focus_on_click( GTK_BUTTON( hnd->btn_down ), FALSE );
    gtk_widget_set_sensitive( hnd->btn_down, FALSE );
    g_signal_connect( G_OBJECT( hnd->btn_down ), "clicked",
                        G_CALLBACK( on_configure_button_press ), hnd );

    // Generating right-hand side of dialog
    hnd->chkbtn_handler_enabled = gtk_check_button_new_with_mnemonic(
                                        mode == HANDLER_MODE_FILE ?
                                            _("Ena_ble as a default opener") :
                                            _("Ena_ble Handler") );
    g_signal_connect( G_OBJECT( hnd->chkbtn_handler_enabled ), "toggled",
                G_CALLBACK ( on_configure_handler_enabled_check ), hnd );
    GtkWidget* lbl_handler_name = gtk_label_new( NULL );
    gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_handler_name ),
                                        _("_Name:") ),
    gtk_misc_set_alignment( GTK_MISC( lbl_handler_name ), 0, 0.8 );
    GtkWidget* lbl_handler_mime = gtk_label_new( NULL );
    gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_handler_mime ),
                                        mode == HANDLER_MODE_ARC ||
                                        mode == HANDLER_MODE_FILE ?
                                        _("MIM_E Type:") :
                                        _("Whit_elist:") );
    gtk_misc_set_alignment( GTK_MISC( lbl_handler_mime ), 0, 0.8 );
    GtkWidget* lbl_handler_extension = gtk_label_new( NULL );
    gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_handler_extension ),
                                        mode == HANDLER_MODE_ARC ||
                                        mode == HANDLER_MODE_FILE ?
                                        _("P_athname:") :
                                        _("Bl_acklist:") );
    gtk_misc_set_alignment( GTK_MISC( lbl_handler_extension ), 0, 1.0 );
    GtkWidget* lbl_handler_icon;
    if ( mode == HANDLER_MODE_FILE )
    {
        lbl_handler_icon = gtk_label_new( NULL );
        gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_handler_icon ),
                                            _("_Icon:") );
        gtk_misc_set_alignment( GTK_MISC( lbl_handler_icon ), 0, 1.0 );
    }
    else
        lbl_handler_icon = NULL;

    GtkWidget* lbl_handler_compress = gtk_label_new( NULL );
    if ( mode == HANDLER_MODE_ARC )
        str = _("<b>Co_mpress:</b>");
    else if ( mode == HANDLER_MODE_FILE )
        str = _("<b>Open Co_mmand:</b>");
    else
        str = _("<b>_Mount:</b>");
    gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_handler_compress ), str );
    gtk_misc_set_alignment( GTK_MISC( lbl_handler_compress ), 0, 1.0 );
    GtkWidget* lbl_handler_extract = gtk_label_new( NULL );
    gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_handler_extract ),
                                        mode == HANDLER_MODE_ARC ?
                                        _("<b>Ex_tract:</b>") :
                                        _("<b>Unmoun_t:</b>") );
    gtk_misc_set_alignment( GTK_MISC( lbl_handler_extract ), 0, 1.0 );
    GtkWidget* lbl_handler_list = gtk_label_new( NULL );
    gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_handler_list ),
                                        mode == HANDLER_MODE_ARC ?
                                        _("<b>Li_st:</b>") :
                                        _("<b>Propertie_s:</b>") );
    gtk_misc_set_alignment( GTK_MISC( lbl_handler_list ), 0, 1.0 );
    hnd->entry_handler_name = gtk_entry_new();
    hnd->entry_handler_mime = gtk_entry_new();
    hnd->entry_handler_extension = gtk_entry_new();
    hnd->entry_handler_icon = mode == HANDLER_MODE_FILE ? gtk_entry_new() :
                                                                    NULL;
        
    /* Creating new textviews in scrolled windows */
    hnd->view_handler_compress = gtk_text_view_new();
    gtk_text_view_set_wrap_mode( GTK_TEXT_VIEW( hnd->view_handler_compress ),
                                 GTK_WRAP_WORD_CHAR );
    GtkWidget* view_handler_compress_scroll = gtk_scrolled_window_new( NULL,
                                                                NULL );
    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( view_handler_compress_scroll ),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER( view_handler_compress_scroll ),
                                      hnd->view_handler_compress );
    g_signal_connect( G_OBJECT( hnd->view_handler_compress ), "key-press-event",
                          G_CALLBACK( on_textview_keypress ), hnd );

    hnd->view_handler_extract = gtk_text_view_new();
    gtk_text_view_set_wrap_mode( GTK_TEXT_VIEW( hnd->view_handler_extract ),
                                 GTK_WRAP_WORD_CHAR );
    GtkWidget* view_handler_extract_scroll = gtk_scrolled_window_new( NULL,
                                                                NULL );
    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( view_handler_extract_scroll ),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER( view_handler_extract_scroll ),
                                      hnd->view_handler_extract );
    g_signal_connect( G_OBJECT( hnd->view_handler_extract ), "key-press-event",
                          G_CALLBACK( on_textview_keypress ), hnd );

    hnd->view_handler_list = gtk_text_view_new();
    gtk_text_view_set_wrap_mode( GTK_TEXT_VIEW( hnd->view_handler_list ),
                                 GTK_WRAP_WORD_CHAR );
    GtkWidget* view_handler_list_scroll = gtk_scrolled_window_new( NULL,
                                                                NULL );
    gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( view_handler_list_scroll ),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER( view_handler_list_scroll ),
                                      hnd->view_handler_list );
    g_signal_connect( G_OBJECT( hnd->view_handler_list ), "key-press-event",
                          G_CALLBACK( on_textview_keypress ), hnd );

    // set fonts
    on_textview_font_change( NULL, hnd );
    // set textview popup menu event handlers
    g_signal_connect_after( G_OBJECT( hnd->view_handler_compress ), "populate-popup",
                        G_CALLBACK( on_textview_popup ), hnd );
    g_signal_connect_after( G_OBJECT( hnd->view_handler_extract ), "populate-popup",
                        G_CALLBACK( on_textview_popup ), hnd );
    g_signal_connect_after( G_OBJECT( hnd->view_handler_list ), "populate-popup",
                        G_CALLBACK( on_textview_popup ), hnd );

    // Setting widgets to be activated associated with label mnemonics
    gtk_label_set_mnemonic_widget( GTK_LABEL( lbl_handler_name ),
                                   hnd->entry_handler_name );
    gtk_label_set_mnemonic_widget( GTK_LABEL( lbl_handler_mime ),
                                   hnd->entry_handler_mime );
    gtk_label_set_mnemonic_widget( GTK_LABEL( lbl_handler_extension ),
                                   hnd->entry_handler_extension );
    gtk_label_set_mnemonic_widget( GTK_LABEL( lbl_handler_compress ),
                                   hnd->view_handler_compress );
    gtk_label_set_mnemonic_widget( GTK_LABEL( lbl_handler_extract ),
                                   hnd->view_handler_extract );
    gtk_label_set_mnemonic_widget( GTK_LABEL( lbl_handler_list ),
                                   hnd->view_handler_list );

    hnd->chkbtn_handler_compress_term =
                gtk_check_button_new_with_label( _("Run In Terminal") );
    hnd->chkbtn_handler_extract_term =
                gtk_check_button_new_with_label( _("Run In Terminal") );
    hnd->chkbtn_handler_list_term =
                gtk_check_button_new_with_label( _("Run In Terminal") );

    GtkWidget* lbl_edit0 = gtk_label_new( NULL );
    str = g_strdup_printf( "<a href=\"%d\">%s</a>", 0, _("Edit") );
    gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_edit0 ), str );
    g_free( str );
    g_signal_connect( G_OBJECT( lbl_edit0 ), "activate-link",
                        G_CALLBACK( on_activate_link ), hnd );
    GtkWidget* lbl_edit1 = gtk_label_new( NULL );
    str = g_strdup_printf( "<a href=\"%d\">%s</a>", 1, _("Edit") );
    gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_edit1 ), str );
    g_free( str );
    g_signal_connect( G_OBJECT( lbl_edit1 ), "activate-link",
                        G_CALLBACK( on_activate_link ), hnd );
    GtkWidget* lbl_edit2 = gtk_label_new( NULL );
    str = g_strdup_printf( "<a href=\"%d\">%s</a>", 2, _("Edit") );
    gtk_label_set_markup_with_mnemonic( GTK_LABEL( lbl_edit2 ), str );
    g_free( str );
    g_signal_connect( G_OBJECT( lbl_edit2 ), "activate-link",
                        G_CALLBACK( on_activate_link ), hnd );
    
    /* Creating container boxes - at this point the dialog already comes
     * with one GtkVBox then inside that a GtkHButtonBox
     * For the right side of the dialog, standard GtkBox approach fails
     * to allow precise padding of labels to allow all entries to line up
     *  - so reimplementing with GtkTable. Would many GtkAlignments have
     * worked? */
    GtkWidget* hbox_main = gtk_hbox_new( FALSE, 4 );
    GtkWidget* vbox_handlers = gtk_vbox_new( FALSE, 4 );
    GtkWidget* hbox_view_buttons = gtk_hbox_new( FALSE, 4 );
    GtkWidget* hbox_move_buttons = gtk_hbox_new( FALSE, 4 );
    GtkWidget* vbox_settings = gtk_vbox_new( FALSE, 0 );
    GtkWidget* tbl_settings = gtk_table_new( 3, mode == HANDLER_MODE_FILE ?
                                                                4 : 3, FALSE );
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
                        hnd->chkbtn_handler_enabled, FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( vbox_settings ),
                       GTK_WIDGET( tbl_settings ), FALSE, FALSE, 4 );

    /* view_handlers isn't added but view_scroll is - view_handlers is
     * inside view_scroll. No padding added to get it to align with the
     * enabled widget on the right side */
    gtk_box_pack_start( GTK_BOX( vbox_handlers ),
                        GTK_WIDGET( view_scroll ), TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( vbox_handlers ),
                        GTK_WIDGET( hbox_view_buttons ), FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( vbox_handlers ),
                        GTK_WIDGET( hbox_move_buttons ), FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( hbox_view_buttons ),
                        GTK_WIDGET( hnd->btn_remove ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox_view_buttons ),
                        GTK_WIDGET( gtk_vseparator_new() ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox_view_buttons ),
                        GTK_WIDGET( hnd->btn_add ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox_view_buttons ),
                        GTK_WIDGET( hnd->btn_apply ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox_move_buttons ),
                        GTK_WIDGET( hnd->btn_up ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox_move_buttons ),
                        GTK_WIDGET( hnd->btn_down ), TRUE, TRUE, 4 );

    gtk_table_set_row_spacing( GTK_TABLE( tbl_settings ), 1, 5 );

    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( lbl_handler_name ), 0, 1, 0, 1,
                        GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( hnd->entry_handler_name ), 1, 4, 0, 1,
                        GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( lbl_handler_mime ), 0, 1, 1, 2,
                        GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( hnd->entry_handler_mime ), 1, 4, 1, 2,
                        GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( lbl_handler_extension ), 0, 1, 2, 3,
                        GTK_FILL, GTK_FILL, 0, 0 );
    gtk_table_attach( GTK_TABLE( tbl_settings ),
                        GTK_WIDGET( hnd->entry_handler_extension ), 1, 4, 2, 3,
                        GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0 );
    if ( mode == HANDLER_MODE_FILE )
    {
        gtk_table_attach( GTK_TABLE( tbl_settings ),
                            GTK_WIDGET( lbl_handler_icon ), 0, 1, 3, 4,
                            GTK_FILL, GTK_FILL, 0, 0 );
        gtk_table_attach( GTK_TABLE( tbl_settings ),
                            GTK_WIDGET( hnd->entry_handler_icon ), 1, 4, 3, 4,
                            GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0 );
    }

    // Make sure widgets do not separate too much vertically
    gtk_box_set_spacing( GTK_BOX( vbox_settings ), 1 );

    // pack_end widgets must not expand to be flush up against the side
    gtk_box_pack_start( GTK_BOX( vbox_settings ),
                        GTK_WIDGET( hbox_compress_header ), FALSE, FALSE,
                        4 );
    gtk_box_pack_start( GTK_BOX( hbox_compress_header ),
                        GTK_WIDGET( lbl_handler_compress ), TRUE, TRUE,
                        4 );
    gtk_box_pack_start( GTK_BOX( hbox_compress_header ),
                        hnd->chkbtn_handler_compress_term, FALSE, TRUE, 4 );
    gtk_box_pack_end( GTK_BOX( hbox_compress_header ),
                        GTK_WIDGET( lbl_edit0 ), FALSE,
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
    gtk_box_pack_start( GTK_BOX( hbox_extract_header ),
                        hnd->chkbtn_handler_extract_term, FALSE, TRUE, 4 );
    gtk_box_pack_end( GTK_BOX( hbox_extract_header ),
                        GTK_WIDGET( lbl_edit1 ), FALSE,
                        FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( vbox_settings ),
                        GTK_WIDGET( view_handler_extract_scroll ), TRUE,
                        TRUE, 4 );

    gtk_box_pack_start( GTK_BOX( vbox_settings ),
                        GTK_WIDGET( hbox_list_header ), FALSE, FALSE,
                        4 );
    gtk_box_pack_start( GTK_BOX( hbox_list_header ),
                        GTK_WIDGET( lbl_handler_list ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( hbox_list_header ),
                        hnd->chkbtn_handler_list_term, FALSE, TRUE, 4 );
    gtk_box_pack_end( GTK_BOX( hbox_list_header ),
                        GTK_WIDGET( lbl_edit2 ), FALSE,
                        FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( vbox_settings ),
                        GTK_WIDGET( view_handler_list_scroll ), TRUE,
                        TRUE, 4 );

    /* Packing boxes into dialog with padding to separate from dialog's
     * standard buttons at the bottom */
    gtk_box_pack_start(
                GTK_BOX(
                    gtk_dialog_get_content_area( GTK_DIALOG( hnd->dlg ) )
                ),
                GTK_WIDGET( hbox_main ), TRUE, TRUE, 4 );

    // Adding archive handlers to list
    populate_archive_handlers( hnd, def_handler_set );

    // Show All
    gtk_widget_show_all( GTK_WIDGET( hnd->dlg ) );
    if ( mode == HANDLER_MODE_FILE )
    {
        gtk_widget_hide( hbox_extract_header );
        gtk_widget_hide( hbox_list_header );
        gtk_widget_hide( view_handler_extract_scroll );
        gtk_widget_hide( view_handler_list_scroll );
    }
    
    /* Rendering dialog - while loop is used to deal with standard
     * buttons that should not cause the dialog to exit */
/*igcr need to handle dialog delete event? */
    int response;
    while ( response = gtk_dialog_run( GTK_DIALOG( hnd->dlg ) ) )
    {
        if ( response == GTK_RESPONSE_OK )
        {
            break;
        }
        else if ( response == GTK_RESPONSE_HELP )
        {
            const char* help;
            if ( mode == HANDLER_MODE_ARC )
                help = "#handlers-arc";
            else if ( mode == HANDLER_MODE_FS )
                help = "#handlers-dev";
            else if ( mode == HANDLER_MODE_NET )
                help = "#handlers-pro";
            else if ( mode == HANDLER_MODE_FILE )
                help = "#handlers-fil";
            else
                help = NULL;
            xset_show_help( hnd->dlg, NULL, help );
        }
        else if ( response == GTK_RESPONSE_NONE )
        {
            // Restore defaults requested
            restore_defaults( hnd, TRUE );
        }
        else if ( response == GTK_RESPONSE_NO )
        {
            // Restore defaults requested
            restore_defaults( hnd, FALSE );
        }
        else
            break;
    }

    // Fetching dialog dimensions
    GtkAllocation allocation;
    gtk_widget_get_allocation ( GTK_WIDGET( hnd->dlg ), &allocation );
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
    gtk_widget_destroy( hnd->dlg );
    g_slice_free( HandlerData, hnd );
}
