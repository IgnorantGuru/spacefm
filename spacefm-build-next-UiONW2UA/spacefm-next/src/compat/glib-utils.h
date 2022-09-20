/*
* C++ Interface: glib-mem
*
* Description: Compatibility macros for older versions of glib
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef _GLIB_UTILS_H_
#define _GLIB_UTILS_H_

#include <glib.h>

/* older versions of glib don't provde these API */

#if ! GLIB_CHECK_VERSION(2, 8, 0)
int g_mkdir_with_parents(const gchar *pathname, int mode);
#endif

#if ! GLIB_CHECK_VERSION(2, 16, 0)
int g_strcmp0(const char *str1, const char *str2);
#endif

#endif

