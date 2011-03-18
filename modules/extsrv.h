#ifndef __EXTSRV_H__
#define __EXTSRV_H__

#include "plugins.h"

/*
 * bind to ip:port 
 *   to process COMMAND from other apeds(also named x) and v
 *   we name (x,v) snake for convenient
 *
 * this can be process through HTTP protocol, but it's too heavy
 */
void ext_s_init(acetables *g_ape, char *ip, char *port, char *me);

#endif
