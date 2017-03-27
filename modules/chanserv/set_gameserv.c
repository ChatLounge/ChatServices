/*
 * Copyright (c) 2003-2004 E. Will et al.
 * Copyright (c) 2006-2009 Atheme Development Group
 * Copyright (c) 2017 ChatLounge IRC Network Development Team
 *
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains routines to handle the CService SET GAMESERV command.
 *
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"chanserv/set_gameserv", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net>"
);

void (*add_history_entry)(sourceinfo_t *si, mychan_t *mc, const char *desc) = NULL;
void (*notify_channel_set_change)(sourceinfo_t *si, myuser_t *tmu, mychan_t *mc,
	const char *settingname, const char *setting) = NULL;

static void cs_cmd_set_gameserv(sourceinfo_t *si, int parc, char *parv[]);

command_t cs_set_gameserv = { "GAMESERV", N_("Allows or disallows gaming services."), AC_NONE, 2, cs_cmd_set_gameserv, { .path = "cservice/set_gameserv" } };

mowgli_patricia_t **cs_set_cmdtree;

void _modinit(module_t *m)
{
	MODULE_TRY_REQUEST_SYMBOL(m, cs_set_cmdtree, "chanserv/set_core", "cs_set_cmdtree");

	if (module_locate_symbol("chanserv/history", "add_history_entry"))
		add_history_entry = module_locate_symbol("chanserv/history", "add_history_entry");

	if (module_request("chanserv/main"))
		notify_channel_set_change = module_locate_symbol("chanserv/main", "notify_channel_set_change");

	command_add(&cs_set_gameserv, *cs_set_cmdtree);
}

void _moddeinit(module_unload_intent_t intent)
{
	command_delete(&cs_set_gameserv, *cs_set_cmdtree);
}

static void cs_cmd_set_gameserv(sourceinfo_t *si, int parc, char *parv[])
{
	mychan_t *mc;
	const char *val;
	metadata_t *md;

	if (!(mc = mychan_find(parv[0])))
	{
		command_fail(si, fault_nosuch_target, _("Channel \2%s\2 is not registered."), parv[0]);
		return;
	}

	if (!parv[1])
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "SET GAMESERV");
		return;
	}

	if (!(chanacs_source_has_flag(mc, si, CA_SET) || chanacs_source_has_flag(mc, si, CA_FOUNDER)))
	{
		command_fail(si, fault_noprivs, _("You are not authorized to perform this command."));
		return;
	}

	if (!strcasecmp("ALL", parv[1]))
		val = "ALL";
	else if (!strcasecmp("VOICE", parv[1]) || !strcasecmp("VOICES", parv[1]))
		val = "VOICE";
	else if (!strcasecmp("OP", parv[1]) || !strcasecmp("OPS", parv[1]))
		val = "OP";
	else if (!strcasecmp("OFF", parv[1]))
		val = NULL;
	else
	{
		command_fail(si, fault_badparams, STR_INVALID_PARAMS, "GAMESERV");
		return;
	}

	md = metadata_find(mc, "gameserv");

	if (val != NULL)
	{
		if (md != NULL && !strcasecmp(md->value, val))
		{
			command_fail(si, fault_nochange, _("\2%s\2 is already set to %s for \2%s\2."), "GAMESERV", val, mc->name);
			return;
		}
		logcommand(si, CMDLOG_SET, "SET:GAMESERV: \2%s\2 to \2%s\2", mc->name, val);

		metadata_add(mc, "gameserv", val);

		command_success_nodata(si, _("\2%s\2 has been set to \2%s\2 for \2%s\2."), "GAMESERV", val, mc->name);

		if (add_history_entry == NULL)
		{
			add_history_entry = module_locate_symbol("chanserv/history", "add_history_entry");
		}

		if (add_history_entry != NULL)
		{
			char desc[350];

			snprintf(desc, sizeof desc, "GAMESERV setting set to: %s",
				!strcasecmp("OFF", val) ? "Disabled" :
				!strcasecmp("OP", val) ? "Channel Operators Only" :
				!strcasecmp("VOICE", val) ? "Voiced Users and Channel Operators" :
				"Enabled");

			add_history_entry(si, mc, desc);
		}

		notify_channel_set_change(si, si->smu, mc, "GAMESERV",
			!strcasecmp("OFF", val) ? "Disabled" :
			!strcasecmp("OP", val) ? "Channel Operators Only" :
			!strcasecmp("VOICE", val) ? "Voiced Users and Channel Operators" :
			"Enabled");

		return;
	}
	else
	{
		if (md == NULL)
		{
			command_fail(si, fault_nochange, _("\2%s\2 was not set for \2%s\2."), "GAMESERV", mc->name);
			return;
		}

		logcommand(si, CMDLOG_SET, "SET:GAMESERV:OFF: \2%s\2", mc->name);

		metadata_delete(mc, "gameserv");

		command_success_nodata(si, _("\2%s\2 has been disabled for \2%s\2."), "GAMESERV", mc->name);

		if (add_history_entry == NULL)
		{
			add_history_entry = module_locate_symbol("chanserv/history", "add_history_entry");
		}

		if (add_history_entry != NULL)
		{
			char desc[350];

			snprintf(desc, sizeof desc, "GAMESERV setting set to: %s", "OFF");

			add_history_entry(si, mc, desc);
		}

		notify_channel_set_change(si, si->smu, mc, "GAMESERV", "Disabled");
	}
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
