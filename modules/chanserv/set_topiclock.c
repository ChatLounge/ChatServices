/*
 * Copyright (c) 2003-2004 E. Will et al.
 * Copyright (c) 2006-2010 Atheme Development Group
 * Copyright (c) 2017 ChatLounge IRC Network Development Team
 *
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains routines to handle the CService SET TOPICLOCK command.
 *
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"chanserv/set_topiclock", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net>"
);

void (*notify_channel_set_change)(sourceinfo_t *si, myuser_t *tmu, mychan_t *mc,
	const char *settingname, const char *setting) = NULL;

static void cs_cmd_set_topiclock(sourceinfo_t *si, int parc, char *parv[]);

command_t cs_set_topiclock = { "TOPICLOCK", N_("Restricts who can change the topic."), AC_NONE, 2, cs_cmd_set_topiclock, { .path = "cservice/set_topiclock" } };

mowgli_patricia_t **cs_set_cmdtree;

void _modinit(module_t *m)
{
	MODULE_TRY_REQUEST_SYMBOL(m, cs_set_cmdtree, "chanserv/set_core", "cs_set_cmdtree");

	if (module_request("chanserv/main"))
		notify_channel_set_change = module_locate_symbol("chanserv/main", "notify_channel_set_change");

	command_add(&cs_set_topiclock, *cs_set_cmdtree);
}

void _moddeinit(module_unload_intent_t intent)
{
	command_delete(&cs_set_topiclock, *cs_set_cmdtree);
}

static void cs_cmd_set_topiclock(sourceinfo_t *si, int parc, char *parv[])
{
	mychan_t *mc;

	if (!(mc = mychan_find(parv[0])))
	{
		command_fail(si, fault_nosuch_target, _("Channel \2%s\2 is not registered."), parv[0]);
		return;
	}

	if (!parv[1])
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "SET TOPICLOCK");
		return;
	}

	if (!(chanacs_source_has_flag(mc, si, CA_SET) || chanacs_source_has_flag(mc, si, CA_FOUNDER)))
	{
		command_fail(si, fault_noprivs, _("You are not authorized to perform this command."));
		return;
	}

	if (!strcasecmp("ON", parv[1]) || !strcasecmp("1", parv[1]) || !strcasecmp("TRUE", parv[1]))
	{
		if (MC_TOPICLOCK & mc->flags)
		{
			command_fail(si, fault_nochange, _("The \2%s\2 flag is already set for channel: \2%s\2"), "TOPICLOCK", mc->name);
			return;
		}

		logcommand(si, CMDLOG_SET, "SET:TOPICLOCK:ON: \2%s\2", mc->name);
		verbose(mc, _("\2%s\2 enabled the TOPICLOCK flag."), get_source_name(si));

		mc->flags |= MC_KEEPTOPIC | MC_TOPICLOCK;
		topiclock_sts(mc->chan);

		command_success_nodata(si, _("The \2%s\2 flag has been set for channel: \2%s\2"), "TOPICLOCK", mc->name);

		notify_channel_set_change(si, si->smu, mc, "TOPICLOCK", "ON");

		return;
	}

	else if (!strcasecmp("OFF", parv[1]) || !strcasecmp("0", parv[1]) || !strcasecmp("FALSE", parv[1]))
	{
		if (!(MC_TOPICLOCK & mc->flags))
		{
			command_fail(si, fault_nochange, _("The \2%s\2 flag is not set for channel: \2%s\2"), "TOPICLOCK", mc->name);
			return;
		}

		logcommand(si, CMDLOG_SET, "SET:TOPICLOCK:OFF: \2%s\2", mc->name);
		verbose(mc, _("\2%s\2 disabled the TOPICLOCK flag."), get_source_name(si));

		mc->flags &= ~MC_TOPICLOCK;
		topiclock_sts(mc->chan);

		command_success_nodata(si, _("The \2%s\2 flag has been removed for channel: \2%s\2"), "TOPICLOCK", mc->name);

		notify_channel_set_change(si, si->smu, mc, "TOPICLOCK", "OFF");

		return;
	}
	else
	{
		command_fail(si, fault_badparams, STR_INVALID_PARAMS, "TOPICLOCK");
		return;
	}
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
