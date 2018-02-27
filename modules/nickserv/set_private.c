/*
 * Copyright (c) 2006-2007 William Pitcock, et al.
 * Copyright (c) 2017-2018 ChatLounge IRC Network Development Team
 *
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains routines to handle the NickServ SET PRIVATE command.
 */

#include "atheme.h"
#include "list_common.h"
#include "list.h"

DECLARE_MODULE_V1
(
	"nickserv/set_private", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	VENDOR_STRING
);

void (*add_history_entry_setting)(myuser_t *smu, myuser_t *tmu, const char *settingname, const char *setting) = NULL;

mowgli_patricia_t **ns_set_cmdtree;

/* SET PRIVATE ON|OFF */
static void ns_cmd_set_private(sourceinfo_t *si, int parc, char *parv[])
{
	char *params = parv[0];

	if (!params)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "PRIVATE");
		return;
	}

	if (!strcasecmp("ON", params) || !strcasecmp("1", params) || !strcasecmp("TRUE", params))
	{
		if (MU_PRIVATE & si->smu->flags)
		{
			command_fail(si, fault_nochange, _("The \2%s\2 flag is already set for \2%s\2."), "PRIVATE", entity(si->smu)->name);
			return;
		}

		logcommand(si, CMDLOG_SET, "SET:PRIVATE:ON");

		si->smu->flags |= MU_PRIVATE;
		si->smu->flags |= MU_HIDEMAIL;

		command_success_nodata(si, _("The \2%s\2 flag has been set for \2%s\2."), "PRIVATE" ,entity(si->smu)->name);

		add_history_entry_setting(si->smu, si->smu, "PRIVATE", "ON");

		return;
	}
	else if (!strcasecmp("OFF", params) || !strcasecmp("0", params) || !strcasecmp("FALSE", params))
	{
		if (!(MU_PRIVATE & si->smu->flags))
		{
			command_fail(si, fault_nochange, _("The \2%s\2 flag is not set for \2%s\2."), "PRIVATE", entity(si->smu)->name);
			return;
		}

		logcommand(si, CMDLOG_SET, "SET:PRIVATE:OFF");

		si->smu->flags &= ~MU_PRIVATE;

		command_success_nodata(si, _("The \2%s\2 flag has been removed for \2%s\2."), "PRIVATE", entity(si->smu)->name);

		add_history_entry_setting(si->smu, si->smu, "PRIVATE", "OFF");

		return;
	}
	else
	{
		command_fail(si, fault_badparams, STR_INVALID_PARAMS, "PRIVATE");
		return;
	}
}

command_t ns_set_private = { "PRIVATE", N_("Hides information about you from other users."), AC_NONE, 1, ns_cmd_set_private, { .path = "nickserv/set_private" } };

static bool has_private(const mynick_t *mn, const void *arg)
{
	myuser_t *mu = mn->owner;

	return ( mu->flags & MU_PRIVATE ) == MU_PRIVATE;
}

static bool account_has_private(myuser_t *mu, const void *arg)
{
	return ( mu->flags & MU_PRIVATE ) == MU_PRIVATE;
}

void _modinit(module_t *m)
{
	MODULE_TRY_REQUEST_SYMBOL(m, ns_set_cmdtree, "nickserv/set_core", "ns_set_cmdtree");

	if (module_request("nickserv/main"))
		add_history_entry_setting = module_locate_symbol("nickserv/main", "add_history_entry_setting");

	command_add(&ns_set_private, *ns_set_cmdtree);

	use_nslist_main_symbols(m);

	static list_param_t private;
	private.opttype = OPT_BOOL;
	private.is_match = has_private;

	static list_param_account_t account_private;
	account_private.opttype = OPT_BOOL;
	account_private.is_match = account_has_private;

	list_register("private", &private);
	list_account_register("private", &account_private);

	use_account_private++;
}

void _moddeinit(module_unload_intent_t intent)
{
	command_delete(&ns_set_private, *ns_set_cmdtree);

	list_unregister("private");
	list_account_unregister("private");

	use_account_private--;
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
