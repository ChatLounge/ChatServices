/*
 *	Description:
 *
 *		Permits a user to disable his vhost setting permanently, without oper
 *	intervention.
 *
 *		Some code borrowed from vhost.c.
 *
 *  Rationale:
 *
 *		/HostServ OFF only disables the VHOST for the duration of IRC session
 *	and must be run at the beginning of every IRC session, prior to oper
 *	intervention.
 *
 *	Copyright (c) 2015 - Chat Lounge IRC Network Development Team
 */
 
#include "atheme.h"
#include "hostserv.h"

DECLARE_MODULE_V1
(
	"hostserv/disable", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"Chat Lounge IRC Network Development Team <http://www.chatlounge.net>"
);

static void hs_cmd_disable(sourceinfo_t *si, int parc, char *parv[]);

command_t hs_disable = { "DISABLE", N_("Disables your account's vhost permanently."), AC_AUTHENTICATED, 1, hs_cmd_disable, { .path = "hostserv/disable" } };

void _modinit(module_t *m)
{
	service_named_bind_command("hostserv", &hs_disable);
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("hostserv", &hs_disable);
}

static void hs_cmd_disable(sourceinfo_t *si, int parc, char *parv[])
{
	myuser_t *mu = myuser_find_by_nick(si->su->nick);
	
	hs_sethost_all(mu, NULL, get_source_name(si));
	command_success_nodata(si, _("Deleted all vhosts for \2%s\2."), entity(mu)->name);
	logcommand(si, CMDLOG_ADMIN, "VHOST:REMOVE: \2%s\2", entity(mu)->name);
	do_sethost_all(mu, NULL);
}