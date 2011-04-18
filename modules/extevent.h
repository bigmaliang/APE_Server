#ifndef __EXTEVENT_H__
#define __EXTEVENT_H__

#include "plugins.h"

#include "mevent.h"
/*
 * mevent_ape_ext.h is suplied by moon/deliver/v, not moon/event/plugin
 * (event though v isn't a moon/event/plugin/XXX backend, but they use
 *  the same protocol, an can handle each other's request)
 */
#include "mevent_ape_ext.h"

extern char *id_me, *id_v;
extern HASH *stbl;
extern HASH *utbl;
extern HASH *ctbl;

/*
 * init stbl, which refer to the other apeds(also named x) and v
 *   we name (x,v) snake for convenient
 */
void ext_e_init(char *evts, char *relation);


/*
 * notify v, an user onlined on me
 */
NEOERR* ext_e_useron(USERS *user, acetables *ape);

/*
 * notify v, an user offlined on me
 */
NEOERR* ext_e_useroff(char *uin);

/*
 * notify v OR x, you got a message
 */
NEOERR* ext_e_msgsnd(char *fuin, char *tuin, char *msg);

/*
 * notify x, you get a channel message
 */
NEOERR* ext_e_msgbrd(char *fuin, char *msg, char *cid, char *ctype);

/*
 * notify x, i havn't this channel
 */
NEOERR* ext_e_chan_miss(char *cid, char *ctype, char *id);

/*
 * notify others, i have this channel
 */
NEOERR* ext_e_chan_attend(char *chan);

void ext_event_static(acetables *g_ape, int lastcall);

#endif
