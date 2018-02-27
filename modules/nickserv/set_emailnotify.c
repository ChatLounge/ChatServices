/*
 * Copyright (c) 2017-2018 ChatLounge IRC Network Development Team <http://www.chatlounge.net/>
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
	"nickserv/set_emailnotify", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	VENDOR_STRING
);

void (*add_history_entry_setting)(myuser_t *smu, myuser_t *tmu, const char *settingname, const char *setting) = NULL;

mowgli_patricia_t **ns_set_cmdtree;

static void ns_cmd_set_emailnotify(sourceinfo_t *si, int parc, char *parv[]);

command_t ns_set_emailnotify = { "EMAILNOTIFY", N_("E-mail memos that result from channel or group ACL or settings changes."), AC_NONE, 1, ns_cmd_set_emailnotify, { .path = "nickserv/set_emailnotify" } };

static bool has_emailnotify(const mynick_t *mn, const void *arg)
{
	myuser_t *mu = mn->owner;

	return ( mu->flags & MU_EMAILNOTIFY ) == MU_EMAILNOTIFY;
}

static bool account_has_emailnotify(myuser_t *mu, const void *arg)
{
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

	static list_param_account_t account_emailnotify;
	account_emailnotify.opttype = OPT_BOOL;
	account_emailnotify.is_match = account_has_emailnotify;

	list_register("emailnotify", &emailnotify);
	list_account_register("emailnotify", &account_emailnotify);

	if (module_request("nickserv/main"))
		add_history_entry_setting = module_locate_symbol("nickserv/main", "add_history_entry_setting");
}

void _moddeinit(module_unload_intent_t intent)
{
	command_delete(&ns_set_emailnotify, *ns_set_cmdtree);

	list_unregister("emailnotify");
	list_account_unregister("emailnotify");
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

	if (!strcasecmp("ON", params) || !strcasecmp("1", params) || !strcasecmp("TRUE", params))
	{
		if (MU_EMAILNOTIFY & si->smu->flags)
		{
			command_fail(si, fault_nochange, _("The \2%s\2 flag is already set for account \2%s\2."), "EMAILNOTIFY", entity(si->smu)->name);
			return;
		}

		logcommand(si, CMDLOG_SET, "SET:EMAILNOTIFY:ON");

		si->smu->flags |= MU_EMAILNOTIFY;

		command_success_nodata(si, _("The \2%s\2 flag has been set for account \2%s\2."), "EMAILNOTIFY", entity(si->smu)->name);

		add_history_entry_setting(si->smu, si->smu, "EMAILNOTIFY", "ON");

		return;
	}
	else if (!strcasecmp("OFF", params) || !strcasecmp("0", params) || !strcasecmp("FALSE", params))
	{
		if (!(MU_EMAILNOTIFY & si->smu->flags))
		{
			command_fail(si, fault_nochange, _("The \2%s\2 flag is not set for account \2%s\2."), "EMAILNOTIFY", entity(si->smu)->name);
			return;
		}

		logcommand(si, CMDLOG_SET, "SET:EMAILNOTIFY:OFF");

		si->smu->flags &= ~MU_EMAILNOTIFY;

		command_success_nodata(si, _("The \2%s\2 flag has been removed for account \2%s\2."), "EMAILNOTIFY", entity(si->smu)->name);

		add_history_entry_setting(si->smu, si->smu, "EMAILNOTIFY", "OFF");

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
