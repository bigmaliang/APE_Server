#ifndef _HNPUB_H
#define _HNPUB_H

#include "main.h"
#include "cmd.h"
#include "raw.h"
#include "extend.h"
#include "channel.h"

#define ANC_DFT_TITLE	"title"
#define ANC_DFT_TARGET	"_blank"
typedef struct _anc {
	char *name;
	char *href;
	char *title;
	char *target;
} anchor;

typedef struct {
	int friend;
	int fkq;
} chatNum;

void hn_senderr(callbackp *callbacki, char *code, char *msg);
void hn_senddata(callbackp *callbacki, char *code, char *msg);
int hn_isvaliduin(char *uin);
int hn_str_cmp(void *a, void *b);

anchor* anchor_new(const char *name, const char *href,
				   const char *title, const char *target);
void anchor_free(void *a);
int anchor_cmp(void *a, void *b);

chatNum* chatnum_new(int fkq, int friend);
void chatnum_free(void *p);
int hn_chatnum_friend_cmp(void *a, void *b);
int hn_chatnum_fkq_cmp(void *a, void *b);

#define SFREE(p)								\
	do {										\
		if (p) {								\
			free(p);							\
		}										\
	} while (0)

#define MAKE_USER_TBL(g_ape)											\
	do {																\
		if (get_property(g_ape->properties, "userlist") == NULL) {		\
			add_property(&g_ape->properties, "userlist", hashtbl_init(), NULL, \
						 EXTEND_HTBL, EXTEND_ISPRIVATE);				\
		}																\
	} while (0)
#define GET_USER_TBL(g_ape)												\
	(get_property(g_ape->properties, "userlist") != NULL ?				\
	 (HTBL*)get_property(g_ape->properties, "userlist")->val: NULL)

#define SET_USER_FOR_APE(ape, uin, user)								\
	do {																\
		hashtbl_append(get_property(ape->properties, "userlist")->val,	\
					   uin, user);										\
	} while (0)
#define GET_USER_FROM_APE(ape, uin)										\
	(get_property(ape->properties, "userlist") != NULL ?				\
	 hashtbl_seek(get_property(ape->properties, "userlist")->val, uin): NULL)

#define MAKE_ONLINE_TBL(g_ape)											\
	do {																\
		if (get_property(g_ape->properties, "onlineuser") == NULL) {	\
			add_property(&g_ape->properties, "onlineuser", hashtbl_init(), NULL, \
						 EXTEND_HTBL, EXTEND_ISPRIVATE);				\
		}																\
	} while (0)
#define GET_ONLINE_TBL(ape)											\
	(get_property(ape->properties, "onlineuser") != NULL ?			\
	 (HTBL*)get_property(ape->properties, "onlineuser")->val: NULL)

#define SET_USER_FOR_ONLINE(ape, uin, user)								\
	do {																\
		hashtbl_append(get_property(ape->properties, "onlineuser")->val, \
					   uin, user);										\
	} while (0)
#define GET_USER_FROM_ONLINE(ape, uin)									\
	(get_property(ape->properties, "onlineuser") != NULL ?				\
	 hashtbl_seek(get_property(ape->properties, "onlineuser")->val, uin): NULL)

#define ADD_UIN_FOR_USER(user, uin)								\
	do {														\
		if (get_property(user->properties, "uin") == NULL) {	\
			add_property(&user->properties, "uin", uin,	NULL,	\
						 EXTEND_STR, EXTEND_ISPUBLIC);			\
		}														\
	} while (0)
#define GET_UIN_FROM_USER(user)									\
	(get_property(user->properties, "uin") != NULL ?			\
	 (char*)get_property(user->properties, "uin")->val: NULL)
#define ADD_NICK_FOR_USER(user, nick)							\
	do {														\
		if (get_property(user->properties, "nick") == NULL) {	\
			add_property(&user->properties, "nick", nick, NULL,	\
						 EXTEND_STR, EXTEND_ISPUBLIC);			\
		}														\
	} while (0)
#define GET_NICK_FROM_USER(user)								\
	(get_property(user->properties, "nick") != NULL ?			\
	 (char*)get_property(user->properties, "nick")->val: NULL)

#define MAKE_USER_FRIEND_TBL(user)										\
	do {																\
		if (get_property(user->properties, "friend") == NULL) {			\
			add_property(&user->properties, "friend", hashtbl_init(), NULL, \
						 EXTEND_HTBL, EXTEND_ISPRIVATE);				\
		}																\
	} while (0)
#define GET_USER_FRIEND_TBL(user)									\
	(get_property(user->properties, "friend") != NULL ?				\
	 (HTBL*)get_property(user->properties, "friend")->val: NULL)

#define MAKE_CHATNUM_TBL(g_ape)											\
	do {																\
		if (get_property(g_ape->properties, "chatnum") == NULL) {		\
			add_property(&g_ape->properties, "chatnum", hashtbl_init(), \
						 queue_destroy, EXTEND_HTBL, EXTEND_ISPRIVATE);	\
		}																\
	} while (0)
#define GET_CHATNUM_TBL(ape)										\
	(get_property(ape->properties, "chatnum") != NULL ?				\
	 (HTBL*)get_property(ape->properties, "chatnum")->val: NULL)

#endif
