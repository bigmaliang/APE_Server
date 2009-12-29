#ifndef __LIBAPE_FKQ_H__
#define __LIBAPE_FKQ_H__

#define FKQ_PIP_NAME "FangkequnPipe"
#define RAW_FKQDATA	"FKQDATA"

typedef struct {
    unsigned long alive_group;
} st_fkq;

#define SET_SUBUSER_HOSTUIN(sub, uin)                                   \
    (add_property(&sub->properties, "hostuin", uin, EXTEND_STR, EXTEND_ISPRIVATE))

#define GET_SUBUSER_HOSTUIN(sub)                                    \
    (get_property(sub->properties, "hostuin") != NULL?              \
     (char*)get_property(sub->properties, "hostuin")->val: NULL)

#define SET_CHANNEL_PRIVATE(chan)                                       \
    (add_property(&chan->properties, "private", "1", EXTEND_STR, EXTEND_ISPRIVATE))

#define GET_CHANNEL_PRIVATE(chan)                                   \
    (get_property(chan->properties, "private",) != NULL?            \
     (char*)get_property(chan->properties, "private")->val: NULL)

#endif
