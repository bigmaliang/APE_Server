#ifndef __LIBAPE_FKQ_H__
#define __LIBAPE_FKQ_H__

#define FKQ_PIP_NAME "FangkequnPipe"
#define RAW_FKQDATA	"FKQDATA"

typedef struct {
    unsigned long alive_group;
	unsigned long num_login;
} st_fkq;

enum {
	ST_CLOSE = 0,
	ST_OPEN
} fkq_stat;

#define ADD_SUBUSER_HOSTUIN(sub, uin)							\
	do {														\
		if (get_property(sub->properties, "hostuin") == NULL) {	\
			add_property(&sub->properties, "hostuin", uin,		\
						 EXTEND_STR, EXTEND_ISPRIVATE);			\
		}														\
	} while (0)
#define GET_SUBUSER_HOSTUIN(sub)                                    \
    (get_property(sub->properties, "hostuin") != NULL?              \
     (char*)get_property(sub->properties, "hostuin")->val: NULL)

#define MAKE_FKQ_STAT(g_ape, p)										\
	do {															\
		if (get_property(g_ape->properties, "fkqstatic") == NULL) {	\
			add_property(&g_ape->properties, "fkqstatic", p,		\
						 EXTEND_POINTER, EXTEND_ISPRIVATE);			\
		}															\
	} while (0)
#define GET_FKQ_STAT(g_ape)												\
	(get_property(g_ape->properties, "fkqstatic") != NULL ?				\
	 (st_fkq*)get_property(g_ape->properties, "fkqstatic")->val: NULL)

#define ADD_FKQ_HOSTUIN(chan, uin)									\
	do {															\
		if (get_property(chan->properties, "hostuin") == NULL) {	\
			add_property(&chan->properties, "hostuin", uin,			\
						 EXTEND_STR, EXTEND_ISPRIVATE);				\
		}															\
	} while (0)
#define GET_FKQ_HOSTUIN(chan)										\
    (get_property(chan->properties, "hostuin") != NULL?				\
     (char*)get_property(chan->properties, "hostuin")->val: NULL)

#endif
