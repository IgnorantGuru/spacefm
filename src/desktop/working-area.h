#ifndef WORKING_AREA_H
#define WORKING_AREA_H

#include <gdk/gdk.h>
void get_working_area(GdkScreen* screen, GdkRectangle *rect);
void print_xdisplay_size( GdkDisplay* gdisplay, int screen_num );

#endif

