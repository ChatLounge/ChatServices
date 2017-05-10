/*
 * Copyright (c) 2017 ChatLounge IRC Network Development Team <http://www.chatlounge.net/>
 *
 * Rights to this code are as documented in doc/LICENSE.
 *
 * When enabled, get notified when your channel or group access changes.
 *
 */

#include "atheme.h"
#include "uplink.h"
#include "list_common.h"
#include "list.h"

DECLARE_MODULE_V1
(
	"nickserv/set_notifyacl", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net/>"
);

void (*add_history_entry_setting)(myuser_t *smu, myuser_t *tmu, const char *settingname, const char *setting) = NULL;

mowgli_patricia_t **ns_set_cmdtree;

static void ns_cmd_set_notifyacl(sourceinfo_t *si, int parc, char *parv[]);

command_t ns_set_notifyacl = { "NOTIFYACL", N_("Get notified when your channel or group access changes."), AC_NONE, 1, ns_cmd_set_notifyacl, { .path = "nickserv/set_notifyacl" } };

static bool has_notifyacl(const mynick_t *mn, const void *arg)
{
	myuser_t *mu = mn->owner;

	return ( mu->flags & MU_NOTIFYACL ) == MU_NOTIFYACL;
}

void _modinit(module_t *m)
{
	MODULE_TRY_REQUEST_SYMBOL(m, ns_set_cmdtree, "nickserv/set_core", "ns_set_cmdtree");

	if (module_request("nickserv/main"))
		add_history_entry_setting = module_locate_symbol("nickserv/main", "add_history_entry_setting");

	command_add(&ns_set_notifyacl, *ns_set_cmdtree);

	use_nslist_main_symbols(m);

	static list_param_t notifyacl;
	notifyacl.opttype = OPT_BOOL;
	notifyacl.is_match = has_notifyacl;

	list_register("notifyacl", &notifyacl);
}



void _moddeinit(module_unload_intent_t intent)
{
	command_delete(&ns_set_notifyacl, *ns_set_cmdtree);

	list_unregister("notifyacl");
}

/* SET NOTIFYACL [ON|OFF] */
static void ns_cmd_set_notifyacl(sourceinfo_t *si, int parc, char *parv[])
{
	char *params = parv[0];

	if (!params)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "NOTIFYACL");
		return;
	}

	if (!strcasecmp("ON", params))
	{
		if (MU_NOTIFYACL & si->smu->flags)
		{
			command_fail(si, fault_nochange, _("The \2%s\2 flag is already set for account \2%s\2."), "NOTIFYACL", entity(si->smu)->name);
			return;
		}

		logcommand(si, CMDLOG_SET, "SET:NOTIFYACL:ON");

		si->smu->flags |= MU_NOTIFYACL;

		command_success_nodata(si, _("The \2%s\2 flag has been set for account \2%s\2."), "NOTIFYACL", entity(si->smu)->name);

		add_history_entry_setting(si->smu, si->smu, "NOTIFYACL", "ON");

		return;
	}
	else if (!strcasecmp("OFF", params))
	{
		if (!(MU_NOTIFYACL & si->smu->flags))
		{
			command_fail(si, fault_nochange, _("The \2%s\2 flag is not set for account \2%s\2."), "NOTIFYACL", entity(si->smu)->name);
			return;
		}

		logcommand(si, CMDLOG_SET, "SET:NOTIFYACL:OFF");

		si->smu->flags &= ~MU_NOTIFYACL;

		command_success_nodata(si, _("The \2%s\2 flag has been removed for account \2%s\2."), "NOTIFYACL", entity(si->smu)->name);

		add_history_entry_setting(si->smu, si->smu, "NOTIFYACL", "OFF");

		return;
	}

	else
	{
		command_fail(si, fault_badparams, STR_INVALID_PARAMS, "NOTIFYACL");
		return;
	}
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
