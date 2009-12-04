#ifndef __LIBAPE_PUSH_H__
#define __LIBAPE_PUSH_H__

typedef struct {
    unsigned long msg_notice;
    unsigned long msg_feed;
    unsigned long msg_limited;
} st_push;

#define SET_USER_FOR_APE(ape, uin, user)								\
	do {																\
		hashtbl_append(get_property(ape->properties, "userlist")->val,	\
					   uin, user);										\
	} while (0)

#define GET_USER_FROM_APE(ape, uin)										\
	(get_property(ape->properties, "userlist") != NULL ?				\
	 hashtbl_seek(get_property(ape->properties, "userlist")->val, uin): NULL)

#define SET_UIN_FOR_USER(user, uin)					\
	do {											\
		add_property(&user->properties, "uin", uin,	\
					 EXTEND_STR, EXTEND_ISPUBLIC);	\
	} while (0)

#define GET_UIN_FROM_USER(user)									\
	(get_property(user->properties, "uin") != NULL ?			\
	 (char*)get_property(user->properties, "uin")->val: NULL)

#define GET_USER_FRIEND_TBL(user)									\
	(get_property(user->properties, "friend") != NULL ?				\
	 (HTBL*)get_property(user->properties, "friend")->val: NULL)

#define GET_USER_LIST													\
	(get_property(callbacki->g_ape->properties, "userlist") != NULL ?	\
	 (HTBL*)get_property(callbacki->g_ape->properties, "userlist")->val: NULL)

#endif
