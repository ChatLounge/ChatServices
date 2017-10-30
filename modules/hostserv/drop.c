/*
 *  Description:
 *
 *      Permits a user to disable his vhost setting permanently, without oper
 *  intervention.
 *
 *      Some code borrowed from vhost.c.
 *
 *  Rationale:
 *
 *      /HostServ OFF only disables the VHOST for the duration of IRC session
 *  and must be run at the beginning of every IRC session, prior to oper
 *  intervention.
 *
 *  Copyright (c) 2015-2017 - ChatLounge IRC Network Development Team
 */

#include "atheme.h"
#include "hostserv.h"

DECLARE_MODULE_V1
(
	"hostserv/drop", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	VENDOR_STRING
);

void (*add_history_entry)(myuser_t *smu, myuser_t *tmu, const char *desc) = NULL;

static void hs_cmd_drop(sourceinfo_t *si, int parc, char *parv[]);

command_t hs_drop = { "DROP", N_("Disables your account's vhost permanently."), AC_AUTHENTICATED, 1, hs_cmd_drop, { .path = "hostserv/drop" } };

void _modinit(module_t *m)
{
	service_named_bind_command("hostserv", &hs_drop);
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("hostserv", &hs_drop);
}

static void hs_cmd_drop(sourceinfo_t *si, int parc, char *parv[])
{
	char description[300];

	if (si->smu == NULL)
	{
		command_fail(si, fault_noprivs, _("You are not logged in."));
		return;
	}

	hs_sethost_all(si->smu, NULL, get_source_name(si));
	command_success_nodata(si, _("Deleted all vhosts for \2%s\2."), entity(si->smu)->name);
	logcommand(si, CMDLOG_ADMIN, "VHOST:REMOVE: \2%s\2", entity(si->smu)->name);
	do_sethost_all(si->smu, NULL);

	if ((add_history_entry = module_locate_symbol("nickserv/history", "add_history_entry")) != NULL)
	{
		snprintf(description, sizeof description, "Removed vhost.");
		add_history_entry(si->smu, si->smu, description);
	}
}
