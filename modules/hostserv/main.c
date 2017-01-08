/*
 * Copyright (c) 2005 Atheme Development Group
 * Copyright (c) 2016 ChatLounge IRC Network Development Team
 *
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains the main() routine.
 *
 */

#include "atheme.h"
#include "hostserv.h"

DECLARE_MODULE_V1
(
	"hostserv/main", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net/>"
);

static void on_user_identify(user_t *u);
service_t *hostsvs;
unsigned int hostsvs_req_time = 60;
bool hostsvs_limit_first_req = false;

static void osinfo_hook(sourceinfo_t *si)
{
	return_if_fail(si != NULL);

	command_success_nodata(si, _("\02%s Settings\02"), hostsvs->nick);
	command_success_nodata(si, _("===================================="));
	command_success_nodata(si, _("Are users limited in how many times they may change vhost settings: %s"), hostsvs_req_time ? "Yes" : "No");
	if (hostsvs_req_time)
	{
		command_success_nodata(si, _("Vhost change frequency restriction applies to first request: %s"), hostsvs_limit_first_req ? "Yes" : "No");
		command_success_nodata(si, _("How often users are permitted to change vhost settings: %u day%s"), hostsvs_req_time, hostsvs_req_time == 1 ? "" : "s");
		command_success_nodata(si, _("\02NOTE:\02 This also limits how often group management"));
		command_success_nodata(si, _("      can change the assigned vhost on each template."));
	}
	command_success_nodata(si, _("===================================="));
};

unsigned int get_hostsvs_req_time(void)
{
	return hostsvs_req_time;
};

bool get_hostsvs_limit_first_req(void)
{
	return hostsvs_limit_first_req;
};

/* allow_vhost_change:
 *
 *     Determines whether the source is permitted to change the
 * services-ased vhost for the target user.
 *
 * Inputs: si - User executing the command.  target - target user
 * Side Effects: None
 * Output: True if permitted, otherwise false.
 */
bool allow_vhost_change(sourceinfo_t *si, myuser_t *target, bool shownotice)
{
	metadata_t *md_vhosttime;
	time_t vhosttime;
	unsigned int request_time = get_hostsvs_req_time();
	bool limit_first_req = get_hostsvs_limit_first_req();

	if (!has_priv(si, PRIV_USER_VHOSTOVERRIDE) && !has_priv(si, PRIV_ADMIN))
	{
		md_vhosttime = metadata_find(target, "private:usercloak-timestamp");

		/* 86,400 seconds per day */
		if (limit_first_req && md_vhosttime == NULL && (CURRTIME - target->registered > (request_time * 86400)))
		{
			if (shownotice)
				command_fail(si, fault_noprivs, _("Users may only get new vhosts every %u days.  %s remaining for: %s"),
					request_time, timediff(target->registered + request_time * 86400 - CURRTIME), entity(target)->name);
			return false;
		}

		vhosttime = atoi(md_vhosttime->value);

		if (vhosttime + (request_time * 86400) > CURRTIME)
		{
			if (shownotice)
				command_fail(si, fault_noprivs, _("Users may only get new vhosts every %u days.  %s remaining for: %s"),
					request_time, timediff(vhosttime + request_time * 86400 - CURRTIME), entity(target)->name);
			return false;
		}
	}

	return true;
};

void _modinit(module_t *m)
{
	hook_add_event("user_identify");
	hook_add_user_identify(on_user_identify);
	hook_add_event("operserv_info");
	hook_add_operserv_info(osinfo_hook);

	hostsvs = service_add("hostserv", NULL);

	/* Minimum number of days between when users may reuse the HOSTSERV TAKE command to get another vhost. */
	add_uint_conf_item("REQUEST_TIME", &hostsvs->conf_table, 0, &hostsvs_req_time, 0, INT_MAX, 60);
	add_bool_conf_item("LIMIT_FIRST_REQUEST", &hostsvs->conf_table, 0, &hostsvs_limit_first_req, false);
}

void _moddeinit(module_unload_intent_t intent)
{
	if (hostsvs != NULL)
		service_delete(hostsvs);

	hook_del_user_identify(on_user_identify);
	hook_del_operserv_info(osinfo_hook);

	del_conf_item("REQUEST_TIME", &hostsvs->conf_table);
	del_conf_item("LIMIT_FIRST_REQUEST", &hostsvs->conf_table);
}

static void on_user_identify(user_t *u)
{
	myuser_t *mu = u->myuser;
	metadata_t *md;
	char buf[NICKLEN + 20];

	snprintf(buf, sizeof buf, "private:usercloak:%s", u->nick);
	md = metadata_find(mu, buf);
	if (md == NULL)
		md = metadata_find(mu, "private:usercloak");
	if (md == NULL)
		return;

	do_sethost(u, md->value);
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
