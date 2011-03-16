#ifndef __LIBAPE_EXT_H__
#define __LIBAPE_EXT_H__

typedef struct {
	unsigned long msg_total;
	unsigned long num_user;
} stExt;

#define TRACE_NOK(err)							\
	if (err != STATUS_OK) {						\
		STRING zstra;	string_init(&zstra);	\
		nerr_error_traceback(err, &zstra);		\
		alog_err("%s", zstra.buf);				\
		string_clear(&zstra);					\
		nerr_ignore(&err);						\
	}


/*
 * STATISTIC
 */
#define MAKE_EXT_STAT(g_ape, p)										\
	do {															\
		if (get_property(g_ape->properties, "extstatic") == NULL) {	\
			add_property(&g_ape->properties, "extstatic", p, free,	\
						 EXTEND_POINTER, EXTEND_ISPRIVATE);			\
		}															\
	} while (0)
#define GET_EXT_STAT(g_ape)												\
	(get_property(g_ape->properties, "extstatic") != NULL ?				\
	 (stExt*)get_property(g_ape->properties, "extstatic")->val: NULL)

#endif	/* __LIBAPE_EXT_H__ */
