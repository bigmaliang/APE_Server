#include "plugins.h"
#include "libape-lcs.h"
#include "lcsevent.h"

static HTBL *etbl = NULL;

void lcs_event_init(char *evts)
{
	mevent_t *evt;
	char *tkn[10];
	int nTok = 0;
	nTok = explode(' ', evts, tkn, 10);

	if (!etbl) etbl = hashtbl_init();
	
	while (nTok >= 0) {
		if (hashtbl_seek(etbl, tkn[nTok]) == NULL) {
			evt = mevent_init_plugin(tkn[nTok]);
			if (evt) {
				hashtbl_append(etbl, tkn[nTok], (void*)evt);
			}
		}
		nTok--;
	}

	nerr_init();
	merr_init((MeventLog)ape_log);
}

char* lcs_app_secy(char *aname)
{
	char *res = NULL;
	
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "aic");
	if (!evt) return NULL;

	hdf_set_value(evt->hdfsnd, "aname", aname);
	MEVENT_TRIGGER_RET(NULL, evt, aname, REQ_CMD_APP_GETSECY, FLAGS_SYNC);

	hdf_get_copy(evt->hdfrcv, "aname", &res, aname);

	return res;
}

HDF* lcs_app_info(char *aname)
{
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "aic");
	if (!evt) return NULL;
	
	hdf_set_value(evt->hdfsnd, "aname", aname);
	MEVENT_TRIGGER_RET(NULL, evt, aname, REQ_CMD_APPINFO, FLAGS_SYNC);

	HDF *hdf;
	hdf_init(&hdf);
	if (evt->hdfrcv) {
		hdf_copy(hdf, NULL, evt->hdfrcv);
	}

	return hdf;
}

char* lcs_get_admin(char *uname, char *aname)
{
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "dyn");
	if (!evt) return NULL;

	hdf_set_value(evt->hdfsnd, "uname", uname);
	hdf_set_value(evt->hdfsnd, "aname", aname);
	MEVENT_TRIGGER_RET(NULL, evt, uname, REQ_CMD_GETADMIN, FLAGS_SYNC);

	char *oname = hdf_get_value(evt->hdfrcv, "oname", NULL);

	if (oname) oname = strdup(oname);
	return oname;
}

void lcs_remember_user(const char *ip, char *uname, char *aname)
{
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "aic");
	mevent_t *evtp = (mevent_t*)hashtbl_seek(etbl, "place");
	if (!evt || !evtp) return;

	hdf_set_value(evtp->hdfsnd, "ip", ip);
	MEVENT_TRIGGER_VOID(evtp, (char*)ip, REQ_CMD_PLACEGET, FLAGS_SYNC);
	char *city = hdf_get_value(evtp->hdfrcv, "0.c", "Mars");

	hdf_set_value(evt->hdfsnd, "uname", uname);
	hdf_set_value(evt->hdfsnd, "aname", aname);
	hdf_set_value(evt->hdfsnd, "ip", ip);
	hdf_set_value(evt->hdfsnd, "addr", city);
	MEVENT_TRIGGER_VOID(evt, uname, REQ_CMD_APPUSERIN, FLAGS_NONE);
}

void lcs_add_track(char *aname, char *uname, char *oname,
				   char *ip, char *url, char *title, char *refer, int type)
{
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

void lcs_set_msg(char *msg, char *from, char *to, int type)
{
	if (!msg || !from || !to) return;
	
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "msg");
	if (!evt) return;

	hdf_set_value(evt->hdfsnd, "from", from);
	hdf_set_value(evt->hdfsnd, "to", to);
	hdf_set_value(evt->hdfsnd, "raw", msg);
	hdf_set_int_value(evt->hdfsnd, "type", type);
	MEVENT_TRIGGER_VOID(evt, to, REQ_CMD_MSGSET, FLAGS_NONE);
}

void lcs_static(acetables *g_ape, int lastcall)
{
	mevent_t *evt = (mevent_t*)hashtbl_seek(etbl, "rawdb");
	if (!evt) return;
	
    stLcs *st = GET_LCS_STAT(g_ape);
    char sql[1024];

    memset(sql, 0x0, sizeof(sql));
    snprintf(sql, sizeof(sql), "INSERT INTO counter (type, count) "
             " VALUES (%d, %u);", ST_ONLINE, g_ape->nConnected);
    hdf_set_value(evt->hdfsnd, "sqls.1", sql);
	
    memset(sql, 0x0, sizeof(sql));
    snprintf(sql, sizeof(sql), "INSERT INTO counter (type, count) "
             " VALUES (%d, %lu);", ST_MSG_TOTAL, st->msg_total);
    hdf_set_value(evt->hdfsnd, "sqls.2", sql);
	st->msg_total = 0;
	
    memset(sql, 0x0, sizeof(sql));
    snprintf(sql, sizeof(sql), "INSERT INTO counter (type, count) "
             " VALUES (%d, %lu);", ST_NUM_USER, st->num_user);
    hdf_set_value(evt->hdfsnd, "sqls.3", sql);
	st->num_user = 0;
	hashtbl_empty(GET_ONLINE_TBL(g_ape), NULL);

	MEVENT_TRIGGER_VOID(evt, NULL, REQ_CMD_STAT, FLAGS_NONE);
}
