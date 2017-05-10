/*
 * Copyright (c) 2005 William Pitcock <nenolod -at- nenolod.net>
 * Copyright (c) 2007 Jilles Tjoelker
 * Copyright (c) 2017 ChatLounge IRC Network Development Team
 *
 * Rights to this code are as documented in doc/LICENSE.
 *
 * Disables the ability to receive memos.
 *
 */

#include "atheme.h"
#include "uplink.h"
#include "list.h"

DECLARE_MODULE_V1
(
	"nickserv/set_nomemo", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net/>"
);

void (*add_history_entry_setting)(myuser_t *smu, myuser_t *tmu, const char *settingname, const char *setting) = NULL;

mowgli_patricia_t **ns_set_cmdtree;

static void ns_cmd_set_nomemo(sourceinfo_t *si, int parc, char *parv[]);

command_t ns_set_nomemo = { "NOMEMO", N_("Disables the ability to receive memos."), AC_NONE, 1, ns_cmd_set_nomemo, { .path = "nickserv/set_nomemo" } };

static bool has_nomemo(const mynick_t *mn, const void *arg)
{
	myuser_t *mu = mn->owner;

	return ( mu->flags & MU_NOMEMO ) == MU_NOMEMO;
}

void _modinit(module_t *m)
{
	MODULE_TRY_REQUEST_SYMBOL(m, ns_set_cmdtree, "nickserv/set_core", "ns_set_cmdtree");

	if (module_request("nickserv/main"))
		add_history_entry_setting = module_locate_symbol("nickserv/main", "add_history_entry_setting");

	command_add(&ns_set_nomemo, *ns_set_cmdtree);

	use_nslist_main_symbols(m);

	static list_param_t nomemo;
	nomemo.opttype = OPT_BOOL;
	nomemo.is_match = has_nomemo;

	list_register("nomemo", &nomemo);
}

void _moddeinit(module_unload_intent_t intent)
{
	command_delete(&ns_set_nomemo, *ns_set_cmdtree);

	list_unregister("nomemo");
}

/* SET NOMEMO [ON|OFF] */
static void ns_cmd_set_nomemo(sourceinfo_t *si, int parc, char *parv[])
{
	char *params = parv[0];

	if (!params)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "NOMEMO");
		return;
	}

	if (!strcasecmp("ON", params))
	{
		if (MU_NOMEMO & si->smu->flags)
		{
			command_fail(si, fault_nochange, _("The \2%s\2 flag is already set for account \2%s\2."), "NOMEMO", entity(si->smu)->name);
			return;
		}

		logcommand(si, CMDLOG_SET, "SET:NOMEMO:ON");
		si->smu->flags |= MU_NOMEMO;
		command_success_nodata(si, _("The \2%s\2 flag has been set for account \2%s\2."), "NOMEMO", entity(si->smu)->name);

		add_history_entry_setting(si->smu, si->smu, "NOMEMO", "ON");

		return;
	}
	else if (!strcasecmp("OFF", params))
	{
		if (!(MU_NOMEMO & si->smu->flags))
		{
			command_fail(si, fault_nochange, _("The \2%s\2 flag is not set for account \2%s\2."), "NOMEMO", entity(si->smu)->name);
			return;
		}

		logcommand(si, CMDLOG_SET, "SET:NOMEMO:OFF");
		si->smu->flags &= ~MU_NOMEMO;
		command_success_nodata(si, _("The \2%s\2 flag has been removed for account \2%s\2."), "NOMEMO", entity(si->smu)->name);

		add_history_entry_setting(si->smu, si->smu, "NOMEMO", "OFF");

		return;
	}
	else
	{
		command_fail(si, fault_badparams, STR_INVALID_PARAMS, "NOMEMO");
		return;
	}
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
