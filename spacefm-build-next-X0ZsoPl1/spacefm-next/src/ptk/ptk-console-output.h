#ifndef _PTK_CONSOLE_OUTPUT_H_
#define _PTK_CONSOLE_OUTPUT_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

int ptk_console_output_run( GtkWindow* parent_win,
                            const char* title,
                            const char* desc,
                            const char* working_dir, 
                            int argc, char* argv[] );

G_END_DECLS
#endif
