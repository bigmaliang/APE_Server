#include "plugins.h"
#include "extevent.h"

static HTBL *etbl = NULL;

void ext_e_init(char *evts)
{
	mevent_t *evt;
	char *tkn[10];
	int nTok = 0;
	nTok = explode(' ', evts, tkn, 10);

	if (!etbl) etbl = hashtbl_init();
	
	while (nTok >= 0) {
		if (hashtbl_seek(etbl, tkn[nTok]) == NULL) {
			evt = mevent_init_plugin(tkn[nTok]);
			if (evt) {
				hashtbl_append(etbl, tkn[nTok], (void*)evt);
			}
		}
		nTok--;
	}
}
