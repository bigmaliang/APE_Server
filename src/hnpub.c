#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "log.h"
#include "hnpub.h"
#include "utils.h"

void hn_senderr(callbackp *callbacki, char *code, char *msg)
{
    if (callbacki == NULL || code == NULL || msg == NULL)
        return;
    
    RAW *raw;
    json_item *ej = json_new_object();
    json_set_property_strZ(ej, "code", code);
    json_set_property_strZ(ej, "value", msg);
    raw = forge_raw(RAW_ERR, ej);
    send_raw_inline(callbacki->client, callbacki->transport, raw, callbacki->g_ape);
}

void hn_senddata(callbackp *callbacki, char *code, char *msg)
{
    if (callbacki == NULL || code == NULL || msg == NULL)
        return;
    
    RAW *raw;
    json_item *ej = json_new_object();
    json_set_property_strZ(ej, "code", code);
    json_set_property_strZ(ej, "value", msg);
    raw = forge_raw(RAW_DATA, ej);
    send_raw_inline(callbacki->client, callbacki->transport, raw, callbacki->g_ape);
}

int hn_isvaliduin(char *uin)
{
	if (uin == NULL)
		return 0;
        
	char *p = uin;
	while (*p != '\0') {
		if (!isdigit((int)*p))
			return 0;
		p++;
	}
	return 1;
}

int hn_str_cmp(void *a, void *b)
{
	char *sa, *sb;
	sa = (char*)a;
	sb = (char*)b;
	
	return strcmp(sa, sb);
}

/*
 * anchor
 */
anchor* anchor_new(const char *name, const char *href,
				   const char *title, const char *target)
{
	if (!name || !href || !title || !target) return NULL;

	anchor *anc = xmalloc(sizeof(anchor));
	anc->name = strdup(name);
	anc->href = strdup(href);
	anc->title = strdup(title);
	anc->target = strdup(target);

	return anc;
}

void anchor_free(void *a)
{
	if (!a) return;

	anchor *anc = (anchor*)a;
	SFREE(anc->name);
	SFREE(anc->href);
	SFREE(anc->title);
	SFREE(anc->target);
	SFREE(anc);
}

int anchor_cmp(void *a, void *b)
{
	anchor *anca, *ancb;
	anca = (anchor*)a;
	ancb = (anchor*)b;

	return strcmp(anca->href, ancb->href);
}

/*
 * chat number
 */
chatNum* chatnum_new(int fkq, int friend)
{
	chatNum *cnum = xmalloc(sizeof(*cnum));
	
	cnum->fkq = fkq;
	cnum->friend = friend;

	return cnum;
}

void chatnum_free(void *p)
{
	chatNum *cnum = (chatNum*)p;
	
	SFREE(cnum);
}

int hn_chatnum_fkq_cmp(void *a, void *b)
{
	HTBL_ITEM *sa, *sb;
	sa = (HTBL_ITEM*)a;
	sb = (HTBL_ITEM*)b;

	chatNum *ca, *cb;
	ca = (chatNum*)sa->addrs;
	cb = (chatNum*)sb->addrs;

	return cb->fkq - ca->fkq;
}

int hn_chatnum_friend_cmp(void *a, void *b)
{
	HTBL_ITEM *sa, *sb;
	sa = (HTBL_ITEM*)a;
	sb = (HTBL_ITEM*)b;

	chatNum *ca, *cb;
	ca = (chatNum*)sa->addrs;
	cb = (chatNum*)sb->addrs;

	return cb->friend - ca->friend;
}
