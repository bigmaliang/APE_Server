static unsigned int hook_connect(callbackp *callbacki)
{
        char *u, *s;

        JNEED_STR(callbacki->param, "uin", u, RETURN_BAD_PARAMS);
        JNEED_STR(callbacki->param, "samkey", s, RETURN_BAD_PARAMS);
        hn_unescape((unsigned char*)u, strlen(u), '%');

        if (mvt_has_login(u, s))
                return (RETURN_CONTINUE);

        /*
         * g_ape->nConnected-- will turn into negative
         * so, add them like in adduser()
         */
        callbacki->g_ape->nConnected++;
        callbacki->call_user->istmp = 0;

        hn_senderr(callbacki, 112, "ERR_LOGIN");
        return (RETURN_NULL);
}
