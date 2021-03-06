/* Copyright (c) 2016-2018 ChatLounge IRC Network Development Team
 *     <http://www.chatlounge.net/>
 *
 *     This files contains the functions to provide the NickServ
 * SET STRICTACCESS command.  When enabled, only connections matching
 * an entry in the NickServ ACCESS LIST are permitted to identify
 * to the account.
 */

#include "atheme.h"
#include "list_common.h"
#include "list.h"

DECLARE_MODULE_V1
(
	"nickserv/set_strictaccess", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	VENDOR_STRING
);

void (*add_history_entry_setting)(myuser_t *smu, myuser_t *tmu, const char *settingname, const char *setting) = NULL;

mowgli_patricia_t **ns_set_cmdtree;

/* SET STRICTACCESS ON/OFF */
static void ns_cmd_set_strictaccess(sourceinfo_t *si, int parc, char *parv[])
{
	char *params = parv[0];

	if (!params)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "STRICTACCESS");
		return;
	}

	if (!strcasecmp("ON", params) || !strcasecmp("1", params) || !strcasecmp("TRUE", params))
	{
		if (MU_STRICTACCESS & si->smu->flags)
		{
			command_fail(si, fault_nochange, _("The \2%s\2 flag has already been set for: \2%s\2"),
				"STRICTACCESS", entity(si->smu)->name);
			return;
		}

		if (!myuser_access_verify(si->su, si->smu))
		{
			command_fail(si, fault_nochange, _("You may not enable this option from a connection that doesn't match an access list entry."));
			return;
		}

		logcommand(si, CMDLOG_SET, "SET:STRICTACCESS:ON");

		si->smu->flags |= MU_STRICTACCESS;

		command_success_nodata(si, _("The \2%s\2 flag has been set for: \2%s\2"),
			"STRICTACCESS", entity(si->smu)->name);

		add_history_entry_setting(si->smu, si->smu, "STRICTACCESS", "ON");

		return;
	}
	else if (!strcasecmp("OFF", params) || !strcasecmp("0", params) || !strcasecmp("FALSE", params))
	{
		if(!(MU_STRICTACCESS & si->smu->flags))
		{
			command_fail(si, fault_nochange, _("The \2%s\2 flag is not set for: \2%s\2"),
				"STRICTACCESS", entity(si->smu)->name);
			return;
		}

		logcommand(si, CMDLOG_SET, "SET:STRICTACCESS:OFF");

		si->smu->flags &= ~MU_STRICTACCESS;

		command_success_nodata(si, _("The \2%s\2 flag has been removed for: \2%s\2"),
			"STRICTACCESS", entity(si->smu)->name);

		add_history_entry_setting(si->smu, si->smu, "STRICTACCESS", "OFF");

		return;
	}
	else
	{
		command_fail(si, fault_badparams, STR_INVALID_PARAMS, "STRICTACCESS");
		return;
	}
};

command_t ns_set_strictaccess = { "STRICTACCESS", N_("Prevents identifying to this account from connections that don't match any NickServ ACCESS list entries, if enabled."), AC_NONE, 1, ns_cmd_set_strictaccess, { .path = "nickserv/set_strictaccess" } };

static bool has_myuser_strictaccess(const mynick_t *mn, const void *arg)
{
	myuser_t *mu = mn->owner;

	return (mu->flags & MU_STRICTACCESS) == MU_STRICTACCESS;
}

static bool account_has_myuser_strictaccess(myuser_t *mu, const void *arg)
{
	return (mu->flags & MU_STRICTACCESS) == MU_STRICTACCESS;
}

void _modinit(module_t *m)
{
	MODULE_TRY_REQUEST_SYMBOL(m, ns_set_cmdtree, "nickserv/set_core", "ns_set_cmdtree");

	if (module_request("nickserv/main"))
		add_history_entry_setting = module_locate_symbol("nickserv/main", "add_history_entry_setting");

	command_add(&ns_set_strictaccess, *ns_set_cmdtree);

	use_myuser_strictaccess++;

	use_nslist_main_symbols(m);

	static list_param_t myuser_strictaccess;
	myuser_strictaccess.opttype = OPT_BOOL;
	myuser_strictaccess.is_match = has_myuser_strictaccess;

	static list_param_account_t account_myuser_strictaccess;
	account_myuser_strictaccess.opttype = OPT_BOOL;
	account_myuser_strictaccess.is_match = account_has_myuser_strictaccess;

	list_register("strictaccess", &myuser_strictaccess);
	list_account_register("strictaccess", &account_myuser_strictaccess);
}

void _moddeinit(module_unload_intent_t intent)
{
	command_delete(&ns_set_strictaccess, *ns_set_cmdtree);

	use_myuser_strictaccess--;

	list_unregister("strictaccess");
	list_account_unregister("strictaccess");
}
