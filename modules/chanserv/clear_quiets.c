/*
 * Copyright (c) 2015-2017 ChatLounge IRC Network Development Team
 * Copyright (c) 2005 William Pitcock, et al.
 *
 * Rights to this code are as documented in doc/LICENSE.
 *
 * This file contains code for the CService CLEAR EXCEPT function.
 *
 * Based off of chanserv/clear_bans.c .
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"chanserv/clear_quiets", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	VENDOR_STRING
);

void (*add_history_entry)(sourceinfo_t *si, mychan_t *mc, const char *desc) = NULL;

static void cs_cmd_clear_quiets(sourceinfo_t *si, int parc, char *parv[]);

command_t cs_clear_quiets = { "QUIETS", N_("Clears the quiet list (+q) of a channel."),
	AC_NONE, 2, cs_cmd_clear_quiets, { .path = "cservice/clear_quiets" } };

mowgli_patricia_t **cs_clear_cmds;

void _modinit(module_t *m)
{
	MODULE_CONFLICT(m, "chanserv/clear_bans_any");

	MODULE_TRY_REQUEST_SYMBOL(m, cs_clear_cmds, "chanserv/clear", "cs_clear_cmds");

	command_add(&cs_clear_quiets, *cs_clear_cmds);
}

void _moddeinit(module_unload_intent_t intent)
{
	command_delete(&cs_clear_quiets, *cs_clear_cmds);
}

static void cs_cmd_clear_quiets(sourceinfo_t *si, int parc, char *parv[])
{
	channel_t *c;
	mychan_t *mc = mychan_find(parv[0]);
	chanban_t *cb;
	mowgli_node_t *n, *tn;
	int changes;

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

	if (!strchr(ircd->ban_like_modes, 'q'))
	{
		command_fail(si, fault_unimplemented, _("Channel quiet (+q) modes are not available."));
		return;
	}

	changes = 0;
	MOWGLI_ITER_FOREACH_SAFE(n, tn, c->bans.head)
	{
		cb = n->data;
		if (!strchr("q", cb->type))
			continue;
		modestack_mode_param(chansvs.nick, c, MTYPE_DEL, cb->type, cb->mask);
		chanban_delete(cb);
		changes++;
	}

	if (changes > 4)
		command_add_flood(si, FLOOD_MODERATE);

	logcommand(si, CMDLOG_DO, "CLEAR:QUIETS: \2%s\2",
			mc->name);

	command_success_nodata(si, _("Cleared channel quiets on \2%s\2 (%d removed)."),
			mc->name, changes);

	if (changes > 0)
	{
		if (changes > 1)
			verbose(mc, "\2%s\2 removed all %d channel quiets.",
				get_source_name(si), changes);
		else
			verbose(mc, "\2%s\2 removed 1 channel quiet.",
				get_source_name(si));
	}

	if (module_locate_symbol("chanserv/history", "add_history_entry"))
	{
		add_history_entry = module_locate_symbol("chanserv/history", "add_history_entry");
	}

	if (add_history_entry != NULL)
	{
		char desc[350];

		snprintf(desc, sizeof desc, "Cleared all quiets (removed %d).", changes);

		add_history_entry(si, mc, desc);
	}
}
