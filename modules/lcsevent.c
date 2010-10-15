#include "plugins.h"
#include "libape-lcs.h"
#include "lcsevent.h"

char* lcs_app_secy(acetables *g_ape, char *aname)
{
	char *res = NULL;
	
	HTBL *etbl = GET_EVENT_TBL(g_ape);
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "aic");
	if (!evt) return NULL;

	hdf_set_value(evt->hdfsnd, "aname", aname);
	AEVENT_TRIGGER(NULL, evt, aname, REQ_CMD_APP_GETSECY, FLAGS_SYNC);

	hdf_get_copy(evt->hdfrcv, "aname", &res, aname);

	return res;
}

HDF* lcs_app_info(acetables *g_ape, char *aname)
{
	HTBL *etbl = GET_EVENT_TBL(g_ape);
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "aic");
	if (!evt) return NULL;
	
	hdf_set_value(evt->hdfsnd, "aname", aname);
	AEVENT_TRIGGER(NULL, evt, aname, REQ_CMD_APPINFO, FLAGS_SYNC);

	HDF *hdf;
	hdf_init(&hdf);
	if (evt->hdfrcv) {
		hdf_copy(hdf, NULL, evt->hdfrcv);
	}

	return hdf;
}

int lcs_user_join_get(acetables *g_ape, char *uname, char *aname, char **oname)
{
	HTBL *etbl = GET_EVENT_TBL(g_ape);
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "dyn");
	if (!evt) return 0;

	hdf_set_value(evt->hdfsnd, "uname", uname);
	hdf_set_value(evt->hdfsnd, "aname", aname);
	AEVENT_TRIGGER(0, evt, uname, REQ_CMD_JOINGET, FLAGS_SYNC);

	HDF *node = hdf_get_obj(evt->hdfrcv, "0");
	char *s;
	while (node) {
		s = hdf_get_value(node, "oname", NULL);
		if (s && *s) {
			*oname = strdup(s);
			return hdf_get_int_value(node, "id", 0);
		}
		node = hdf_obj_next(node);
	}

	return 0;
}

unsigned int lcs_user_join_set(callbackp *callbacki, char *aname,
							   char *uname, char *oname,
							   char *url, char *title, char *ref, int retcode)
{
	HTBL *etbl = GET_EVENT_TBL(callbacki->g_ape);
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "dyn");
	if (!evt) return 0;
	
	if (!callbacki || !aname || !uname) return 0;

	hdf_set_value(evt->hdfsnd, "uname", uname);
	hdf_set_value(evt->hdfsnd, "aname", aname);
	hdf_set_value(evt->hdfsnd, "oname", oname);
	hdf_set_value(evt->hdfsnd, "ip", callbacki->ip);
	hdf_set_value(evt->hdfsnd, "refer", ref ? ref : "");
	hdf_set_value(evt->hdfsnd, "url", url);
	hdf_set_value(evt->hdfsnd, "title", title);
	hdf_set_int_value(evt->hdfsnd, "retcode", retcode);
	AEVENT_TRIGGER(0, evt, uname, REQ_CMD_JOINSET, FLAGS_SYNC);
	
	return hdf_get_int_value(evt->hdfrcv, "id", 0);
}

void lcs_user_remember_me(callbackp *callbacki, char *uname, char *aname)
{
	HTBL *etbl = GET_EVENT_TBL(callbacki->g_ape);
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "aic");
	if (!evt) return;

	hdf_set_value(evt->hdfsnd, "uname", uname);
	hdf_set_value(evt->hdfsnd, "aname", aname);
	hdf_set_value(evt->hdfsnd, "ip", callbacki->ip);
	AEVENT_TRIGGER_VOID(evt, uname, REQ_CMD_APPUSERIN, FLAGS_NONE);
}

void lcs_set_msg(acetables *g_ape, char *msg, char *from, char *to, int type)
{
	if (!g_ape || !msg || !from || !to) return;
	
	HTBL *etbl = GET_EVENT_TBL(g_ape);
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "msg");
	if (!evt) return;

	hdf_set_value(evt->hdfsnd, "from", from);
	hdf_set_value(evt->hdfsnd, "to", to);
	hdf_set_value(evt->hdfsnd, "raw", msg);
	hdf_set_int_value(evt->hdfsnd, "type", type);
	AEVENT_TRIGGER_VOID(evt, to, REQ_CMD_MSGSET, FLAGS_NONE);
}
