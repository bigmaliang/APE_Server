#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "log.h"
#include "hnpub.h"

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
