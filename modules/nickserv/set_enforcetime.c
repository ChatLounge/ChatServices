/*
 * Copyright (c) 2010 Atheme Development Group
 * Copyright (c) 2016-2017 ChatLounge IRC Network Development Team
 *
 * Rights to this code are as documented in doc/LICENSE.
 *
 * This file contains code for the NickServ SET ENFORCETIME function.
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"nickserv/set_enforcetime",false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net/>"
);

void (*add_history_entry_setting)(myuser_t *smu, myuser_t *tmu, const char *settingname, const char *setting) = NULL;

mowgli_patricia_t **ns_set_cmdtree;

static void ns_cmd_set_enforcetime(sourceinfo_t *si, int parc, char *parv[]);

command_t ns_set_enforcetime = { "ENFORCETIME", N_("Amount of time it takes before nickname protection occurs."), AC_NONE, 1, ns_cmd_set_enforcetime, { .path = "nickserv/set_enforcetime" } };

static void show_enforcetime(hook_user_req_t *hdata)
{
	metadata_t *md;
	
	if (!(hdata->mu == hdata->si->smu || has_priv(hdata->si, PRIV_USER_AUSPEX)))
		return;

	if ((md = metadata_find(hdata->mu, "private:enforcetime")) != NULL)
	{
		int enforcetime = atoi(md->value);
		command_success_nodata(hdata->si, "%s has an enforce grace period of %d second%s.",
			entity(hdata->mu)->name, enforcetime, enforcetime == 1 ? "" : "s");
	}
}

void _modinit(module_t *m)
{
	MODULE_TRY_REQUEST_DEPENDENCY(m, "nickserv/enforce");
	MODULE_TRY_REQUEST_SYMBOL(m, ns_set_cmdtree, "nickserv/set_core", "ns_set_cmdtree");

	if (module_request("nickserv/main"))
		add_history_entry_setting = module_locate_symbol("nickserv/main", "add_history_entry_setting");

	command_add(&ns_set_enforcetime, *ns_set_cmdtree);

	hook_add_event("user_info");
	hook_add_user_info(show_enforcetime);
}

void _moddeinit(module_unload_intent_t intent)
{
	command_delete(&ns_set_enforcetime, *ns_set_cmdtree);

	hook_del_user_info(show_enforcetime);
}

static void ns_cmd_set_enforcetime(sourceinfo_t *si, int parc, char *parv[])
{
	char *setting = parv[0];
	char enforcetimetext[5];

	if (!setting)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "ENFORCETIME");
		command_fail(si, fault_needmoreparams, _("Syntax: SET ENFORCETIME TIME|DEFAULT"));
		return;
	}

	int enforcetime = atoi(parv[0]);

	if (strcasecmp(setting, "DEFAULT") == 0)
	{
		if (metadata_find(si->smu, "private:doenforce"))
		{
			logcommand(si, CMDLOG_SET, "SET:ENFORCETIME:DEFAULT");
			metadata_delete(si->smu, "private:enforcetime");
			command_success_nodata(si, _("The \2%s\2 for account \2%s\2 has been reset to default, which is \2%d\2 seconds."), "ENFORCETIME", entity(si->smu)->name, nicksvs.enforce_delay);

			snprintf(enforcetimetext, sizeof enforcetimetext, "%d seconds", nicksvs.enforce_delay);

			add_history_entry_setting(si->smu, si->smu, "EMAILNOTIFY", "ON");
		}
		else
		{
			command_fail(si, fault_nochange, _("The \2%s\2 flag is not set for account \2%s\2."), "ENFORCE", entity(si->smu)->name);
		}
	}
	else if (enforcetime > 0 && enforcetime <= 180)
	{
		if (metadata_find(si->smu, "private:doenforce"))
		{
			logcommand(si, CMDLOG_SET, "SET:ENFORCETIME: %d", enforcetime);
			metadata_add(si->smu, "private:enforcetime", setting);
			command_success_nodata(si, _("The \2%s\2 for account \2%s\2 has been set to \2%d\2 second%s."), "ENFORCETIME", entity(si->smu)->name, enforcetime, enforcetime == 1 ? "" : "s");

			snprintf(enforcetimetext, sizeof enforcetimetext, "%d seconds", enforcetime);

			add_history_entry_setting(si->smu, si->smu, "EMAILNOTIFY", "ON");
		}
		else
		{
			command_fail(si, fault_nochange, _("The \2%s\2 flag is not set for account \2%s\2."), "ENFORCE", entity(si->smu)->name);
		}
	}
	else
	{
		command_fail(si, fault_badparams, STR_INVALID_PARAMS, "ENFORCETIME");
	}
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
