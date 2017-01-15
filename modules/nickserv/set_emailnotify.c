/*
 * Copyright (c) 2017 ChatLounge IRC Network Development Team <http://www.chatlounge.net/>
 *
 * Rights to this code are as documented in doc/LICENSE.
 *
 * When enabled, e-mail memos that result from channel or group ACL or settings changes.
 *
 */

#include "atheme.h"
#include "uplink.h"
#include "list_common.h"
#include "list.h"

DECLARE_MODULE_V1
(
	"nickserv/set_noop", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net/>"
);

mowgli_patricia_t **ns_set_cmdtree;

static void ns_cmd_set_emailnotify(sourceinfo_t *si, int parc, char *parv[]);

command_t ns_set_emailnotify = { "EMAILNOTIFY", N_("E-mail memos that result from channel or group ACL or settings changes."), AC_NONE, 1, ns_cmd_set_emailnotify, { .path = "nickserv/set_emailnotify" } };

static bool has_emailnotify(const mynick_t *mn, const void *arg)
{
	myuser_t *mu = mn->owner;

	return ( mu->flags & MU_EMAILNOTIFY ) == MU_EMAILNOTIFY;
}

void _modinit(module_t *m)
{
	MODULE_TRY_REQUEST_SYMBOL(m, ns_set_cmdtree, "nickserv/set_core", "ns_set_cmdtree");

	command_add(&ns_set_emailnotify, *ns_set_cmdtree);

	use_nslist_main_symbols(m);

	static list_param_t emailnotify;
	emailnotify.opttype = OPT_BOOL;
	emailnotify.is_match = has_emailnotify;

	list_register("emailnotify", &emailnotify);
}



void _moddeinit(module_unload_intent_t intent)
{
	command_delete(&ns_set_emailnotify, *ns_set_cmdtree);

	list_unregister("emailnotify");
}

/* SET EMAILNOTIFY [ON|OFF] */
static void ns_cmd_set_emailnotify(sourceinfo_t *si, int parc, char *parv[])
{
	char *params = parv[0];

	if (!params)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "EMAILNOTIFY");
		return;
	}

	if (!strcasecmp("ON", params))
	{
		if (MU_EMAILNOTIFY & si->smu->flags)
		{
			command_fail(si, fault_nochange, _("The \2%s\2 flag is already set for account \2%s\2."), "EMAILNOTIFY", entity(si->smu)->name);
			return;
		}

		logcommand(si, CMDLOG_SET, "SET:EMAILNOTIFY:ON");

		si->smu->flags |= MU_EMAILNOTIFY;

		command_success_nodata(si, _("The \2%s\2 flag has been set for account \2%s\2."), "EMAILNOTIFY", entity(si->smu)->name);

		return;
	}
	else if (!strcasecmp("OFF", params))
	{
		if (!(MU_EMAILNOTIFY & si->smu->flags))
		{
			command_fail(si, fault_nochange, _("The \2%s\2 flag is not set for account \2%s\2."), "EMAILNOTIFY", entity(si->smu)->name);
			return;
		}

		logcommand(si, CMDLOG_SET, "SET:EMAILNOTIFY:OFF");

		si->smu->flags &= ~MU_EMAILNOTIFY;

		command_success_nodata(si, _("The \2%s\2 flag has been removed for account \2%s\2."), "EMAILNOTIFY", entity(si->smu)->name);

		return;
	}

	else
	{
		command_fail(si, fault_badparams, STR_INVALID_PARAMS, "EMAILNOTIFY");
		return;
	}
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
