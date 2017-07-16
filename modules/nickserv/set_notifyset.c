/*
 * Copyright (c) 2017 ChatLounge IRC Network Development Team <http://www.chatlounge.net/>
 *
 * Rights to this code are as documented in doc/LICENSE.
 *
 *     When enabled, get notified when your channel or group settings change,
 * if you're a channel owner or a part of channel management.
 *
 */

#include "atheme.h"
#include "uplink.h"
#include "list_common.h"
#include "list.h"

DECLARE_MODULE_V1
(
	"nickserv/set_notifyset", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net/>"
);

void (*add_history_entry_setting)(myuser_t *smu, myuser_t *tmu, const char *settingname, const char *setting) = NULL;

mowgli_patricia_t **ns_set_cmdtree;

static void ns_cmd_set_notifyset(sourceinfo_t *si, int parc, char *parv[]);

command_t ns_set_notifyset = { "NOTIFYSET", N_("Get notified when your channel or group settings change."), AC_NONE, 1, ns_cmd_set_notifyset, { .path = "nickserv/set_notifyset" } };

static bool has_notifyset(const mynick_t *mn, const void *arg)
{
	myuser_t *mu = mn->owner;

	return ( mu->flags & MU_NOTIFYSET ) == MU_NOTIFYSET;
}

void _modinit(module_t *m)
{
	MODULE_TRY_REQUEST_SYMBOL(m, ns_set_cmdtree, "nickserv/set_core", "ns_set_cmdtree");

	if (module_request("nickserv/main"))
		add_history_entry_setting = module_locate_symbol("nickserv/main", "add_history_entry_setting");

	command_add(&ns_set_notifyset, *ns_set_cmdtree);

	use_nslist_main_symbols(m);

	static list_param_t notifyset;
	notifyset.opttype = OPT_BOOL;
	notifyset.is_match = has_notifyset;

	list_register("notifyset", &notifyset);
}



void _moddeinit(module_unload_intent_t intent)
{
	command_delete(&ns_set_notifyset, *ns_set_cmdtree);

	list_unregister("notifyset");
}

/* SET NOTIFYSET [ON|OFF] */
static void ns_cmd_set_notifyset(sourceinfo_t *si, int parc, char *parv[])
{
	char *params = parv[0];

	if (!params)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "NOTIFYSET");
		return;
	}

	if (!strcasecmp("ON", params) || !strcasecmp("1", params) || !strcasecmp("TRUE", params))
	{
		if (MU_NOTIFYSET & si->smu->flags)
		{
			command_fail(si, fault_nochange, _("The \2%s\2 flag is already set for account \2%s\2."), "NOTIFYSET", entity(si->smu)->name);
			return;
		}

		logcommand(si, CMDLOG_SET, "SET:NOTIFYSET:ON");

		si->smu->flags |= MU_NOTIFYSET;

		command_success_nodata(si, _("The \2%s\2 flag has been set for account \2%s\2."), "NOTIFYSET", entity(si->smu)->name);

		add_history_entry_setting(si->smu, si->smu, "NOTIFYSET", "ON");

		return;
	}
	else if (!strcasecmp("OFF", params) || !strcasecmp("0", params) || !strcasecmp("FALSE", params))
	{
		if (!(MU_NOTIFYSET & si->smu->flags))
		{
			command_fail(si, fault_nochange, _("The \2%s\2 flag is not set for account \2%s\2."), "NOTIFYSET", entity(si->smu)->name);
			return;
		}

		logcommand(si, CMDLOG_SET, "SET:NOTIFYSET:OFF");

		si->smu->flags &= ~MU_NOTIFYSET;

		command_success_nodata(si, _("The \2%s\2 flag has been removed for account \2%s\2."), "NOTIFYSET", entity(si->smu)->name);

		add_history_entry_setting(si->smu, si->smu, "NOTIFYSET", "OFF");

		return;
	}

	else
	{
		command_fail(si, fault_badparams, STR_INVALID_PARAMS, "NOTIFYSET");
		return;
	}
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
