#ifndef __LIBAPE_LCS_H__
#define __LIBAPE_LCS_H__

#define LCS_PIP_NAME	"LiveCSPipe_"
#define RAW_LCSDATA		"LCS_DATA"

enum {
    ST_ONLINE = 0,
    ST_MSG_TOTAL,
	ST_NUM_USER
} lcsStastic;

typedef struct {
	unsigned long msg_total;
	unsigned long num_user;
} stLcs;

typedef struct {
	Queue *users;
	Queue *admins;
	Queue *dirtyusers;
} appBar;

appBar* abar_new();
void abar_free(void *p);



/*
 * appBar table
 */
#define MAKE_ABAR_TBL(g_ape)											\
	do {																\
		if (get_property(g_ape->properties, "abar") == NULL) {			\
			add_property(&g_ape->properties, "abar", hashtbl_init(),	\
						 abar_free, EXTEND_HTBL, EXTEND_ISPRIVATE);		\
		}																\
	} while (0)
#define GET_ABAR_TBL(ape)										\
	(get_property(ape->properties, "abar") != NULL ?			\
	 (HTBL*)get_property(ape->properties, "abar")->val: NULL)




/*
 * STATISTIC
 */
#define MAKE_LCS_STAT(g_ape, p)										\
	do {															\
		if (get_property(g_ape->properties, "lcsstatic") == NULL) {	\
			add_property(&g_ape->properties, "lcsstatic", p, free,	\
						 EXTEND_POINTER, EXTEND_ISPRIVATE);			\
		}															\
	} while (0)
#define GET_LCS_STAT(g_ape)												\
	(get_property(g_ape->properties, "lcsstatic") != NULL ?				\
	 (stLcs*)get_property(g_ape->properties, "lcsstatic")->val: NULL)




/*
 * USER PROPERTIES
 */
#define SET_USER_ADMIN(user)										\
	do {															\
		if (get_property(user->properties, "isadmin") == NULL) {	\
			add_property(&user->properties, "isadmin", "1",	NULL,	\
						 EXTEND_STR, EXTEND_ISPUBLIC);				\
		}															\
	} while (0)
#define GET_USER_ADMIN(g_ape)										\
	(get_property(g_ape->properties, "isadmin") != NULL ?			\
	 (char*)get_property(g_ape->properties, "isadmin")->val: NULL)

#define SET_USER_FRESH(user)										\
	do {															\
		if (get_property(user->properties, "fresh") == NULL) {		\
			add_property(&user->properties, "fresh", "1",	NULL,	\
						 EXTEND_STR, EXTEND_ISPUBLIC);				\
		}															\
	} while (0)
#define GET_USER_FRESH(g_ape)										\
	(get_property(g_ape->properties, "fresh") != NULL ?				\
	 (char*)get_property(g_ape->properties, "fresh")->val: NULL)




/*
 * CHANNEL PROPERTIES
 */
#define ADD_ANAME_FOR_CHANNEL(chan, aname)							\
	do {															\
		if (get_property(chan->properties, "aname") == NULL) {		\
			add_property(&chan->properties, "aname", aname,	NULL,	\
						 EXTEND_STR, EXTEND_ISPUBLIC);				\
		}															\
	} while (0)
#define GET_ANAME_FROM_CHANNEL(chan)							\
	(get_property(chan->properties, "aname") != NULL ?			\
	 (char*)get_property(chan->properties, "aname")->val: NULL)

#define ADD_PNAME_FOR_CHANNEL(chan, pname)							\
	do {															\
		if (get_property(chan->properties, "pname") == NULL) {		\
			add_property(&chan->properties, "pname", pname,	NULL,	\
						 EXTEND_STR, EXTEND_ISPUBLIC);				\
		}															\
	} while (0)
#define GET_PNAME_FROM_CHANNEL(chan)							\
	(get_property(chan->properties, "pname") != NULL ?			\
	 (char*)get_property(chan->properties, "pname")->val: NULL)



#endif	/* __LIBAPE_LCS_H__ */
