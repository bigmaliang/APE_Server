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
	MEVENT_TRIGGER(NULL, evt, aname, REQ_CMD_APP_GETSECY, FLAGS_SYNC);

	hdf_get_copy(evt->hdfrcv, "aname", &res, aname);

	return res;
}

HDF* lcs_app_info(acetables *g_ape, char *aname)
{
	HTBL *etbl = GET_EVENT_TBL(g_ape);
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "aic");
	if (!evt) return NULL;
	
	hdf_set_value(evt->hdfsnd, "aname", aname);
	MEVENT_TRIGGER(NULL, evt, aname, REQ_CMD_APPINFO, FLAGS_SYNC);

	HDF *hdf;
	hdf_init(&hdf);
	if (evt->hdfrcv) {
		hdf_copy(hdf, NULL, evt->hdfrcv);
	}

	return hdf;
}

char* lcs_get_admin(acetables *g_ape, char *uname, char *aname)
{
	HTBL *etbl = GET_EVENT_TBL(g_ape);
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "dyn");
	if (!evt) return 0;

	hdf_set_value(evt->hdfsnd, "uname", uname);
	hdf_set_value(evt->hdfsnd, "aname", aname);
	MEVENT_TRIGGER(NULL, evt, uname, REQ_CMD_GETADMIN, FLAGS_SYNC);

	char *oname = hdf_get_value(evt->hdfrcv, "oname", NULL);

	if (oname) oname = strdup(oname);
	return oname;
}

void lcs_remember_user(callbackp *callbacki, char *uname, char *aname)
{
	HTBL *etbl = GET_EVENT_TBL(callbacki->g_ape);
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "aic");
	mevent_t *evtp = (mevent_t*)hashtbl_seek(etbl, "place");
	if (!evt || !evtp) return;

	hdf_set_value(evtp->hdfsnd, "ip", callbacki->ip);
	MEVENT_TRIGGER_VOID(evtp, (char*)callbacki->ip, REQ_CMD_PLACEGET, FLAGS_SYNC);
	char *city = hdf_get_value(evtp->hdfrcv, "0.c", "Mars");

	hdf_set_value(evt->hdfsnd, "uname", uname);
	hdf_set_value(evt->hdfsnd, "aname", aname);
	hdf_set_value(evt->hdfsnd, "ip", callbacki->ip);
	hdf_set_value(evt->hdfsnd, "addr", city);
	MEVENT_TRIGGER_VOID(evt, uname, REQ_CMD_APPUSERIN, FLAGS_NONE);
}

void lcs_add_track(acetables *g_ape, char *aname, char *uname, char *oname,
				   char *ip, char *url, char *title, char *refer, int type)
{
	HTBL *etbl = GET_EVENT_TBL(g_ape);
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "dyn");
	if (!evt) return;

	hdf_set_value(evt->hdfsnd, "aname", aname);
	hdf_set_value(evt->hdfsnd, "uname", uname);
	hdf_set_value(evt->hdfsnd, "oname", oname);
	hdf_set_value(evt->hdfsnd, "ip", ip);
	hdf_set_value(evt->hdfsnd, "url", url);
	hdf_set_value(evt->hdfsnd, "title", title);
	if (refer) hdf_set_value(evt->hdfsnd, "refer", refer);
	hdf_set_int_value(evt->hdfsnd, "type", type);
	
	MEVENT_TRIGGER_VOID(evt, uname, REQ_CMD_ADDTRACK, FLAGS_NONE);
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
	MEVENT_TRIGGER_VOID(evt, to, REQ_CMD_MSGSET, FLAGS_NONE);
}
