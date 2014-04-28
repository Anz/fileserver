/*
* Copyright (C) 2014 Roger Knecht
* License: http://www.gnu.org/licenses/gpl.html GPL version 2 or higher
*/
#ifndef VTP
#define VTP

#include "vfs.h"

/*
 * Handles virtual transfer protocol operation on given file descriptor and vfs node.
 */
void vtp_handle(int fd, vfsn_t *root);

#endif
