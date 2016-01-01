/*
 * Copyright (c) 2015-2016 ChatLounge IRC Network Development Team
 * Copyright (c) 2005 William Pitcock, et al.
 * Rights to this code are as documented in doc/LICENSE.
 *
 * This file contains code for the CService CLEAR EXCEPT function.
 *
 * Based off of chanserv/clear_bans.c .
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"chanserv/clear_invex", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net>"
);

static void cs_cmd_clear_invex(sourceinfo_t *si, int parc, char *parv[]);

command_t cs_clear_invex = { "INVEX", N_("Clears the invite exception list (+I) of a channel."),
	AC_NONE, 2, cs_cmd_clear_invex, { .path = "cservice/clear_invex" } };

mowgli_patricia_t **cs_clear_cmds;

void _modinit(module_t *m)
{
	MODULE_TRY_REQUEST_SYMBOL(m, cs_clear_cmds, "chanserv/clear", "cs_clear_cmds");

	command_add(&cs_clear_invex, *cs_clear_cmds);
}

void _moddeinit(module_unload_intent_t intent)
{
	command_delete(&cs_clear_invex, *cs_clear_cmds);
}

static void cs_cmd_clear_invex(sourceinfo_t *si, int parc, char *parv[])
{
	channel_t *c;
	mychan_t *mc = mychan_find(parv[0]);
	chanban_t *cb;
	mowgli_node_t *n, *tn;
	int hits;

	if (!mc)
	{
		command_fail(si, fault_nosuch_target, _("Channel \2%s\2 is not registered."), parv[0]);
		return;
	}

	if (!(c = channel_find(parv[0])))
	{
		command_fail(si, fault_nosuch_target, _("\2%s\2 is currently empty."), parv[0]);
		return;
	}

	if (!(chanacs_source_has_flag(mc, si, CA_RECOVER) || chanacs_source_has_flag(mc, si, CA_FOUNDER)))
	{
		command_fail(si, fault_noprivs, _("You are not authorized to perform this operation."));
		return;
	}

	if (metadata_find(mc, "private:close:closer"))
	{
		command_fail(si, fault_noprivs, _("\2%s\2 is closed."), parv[0]);
		return;
	}
	
	if (!strchr(ircd->ban_like_modes, 'I'))
	{
		command_fail(si, fault_unimplemented, _("Channel invite exceptions (+I) modes are not available."));
		return;
	}

	hits = 0;
	MOWGLI_ITER_FOREACH_SAFE(n, tn, c->bans.head)
	{
		cb = n->data;
		if (!strchr("I", cb->type))
			continue;
		modestack_mode_param(chansvs.nick, c, MTYPE_DEL, cb->type, cb->mask);
		chanban_delete(cb);
		hits++;
	}

	if (hits > 4)
		command_add_flood(si, FLOOD_MODERATE);

	logcommand(si, CMDLOG_DO, "CLEAR:INVEX: \2%s\2",
			mc->name);

	command_success_nodata(si, _("Cleared channel invite exceptions on \2%s\2 (%d removed)."),
			parv[0], hits);
}