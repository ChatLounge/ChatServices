/*
 * Copyright (c) 2005 William Pitcock, et al.
 * Copyright (c) 2017 ChatLounge IRC Network Development Team
 *
 * Rights to this code are as documented in doc/LICENSE.
 *
 * This file contains code for the CService KICK functions.
 *
 * Renamed from clear_bans.c to clear_bans_any.c to not collide with the new module.
 *
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"chanserv/clear_bans", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net>"
);

void (*add_history_entry)(sourceinfo_t *si, mychan_t *mc, const char *desc) = NULL;

static void cs_cmd_clear_bans(sourceinfo_t *si, int parc, char *parv[]);

command_t cs_clear_bans = { "BANS", N_("Clears bans or other lists of a channel."),
	AC_NONE, 2, cs_cmd_clear_bans, { .path = "cservice/clear_bans" } };

mowgli_patricia_t **cs_clear_cmds;

void _modinit(module_t *m)
{
	MODULE_TRY_REQUEST_SYMBOL(m, cs_clear_cmds, "chanserv/clear", "cs_clear_cmds");

	command_add(&cs_clear_bans, *cs_clear_cmds);
}

void _moddeinit(module_unload_intent_t intent)
{
	command_delete(&cs_clear_bans, *cs_clear_cmds);
}

static void cs_cmd_clear_bans(sourceinfo_t *si, int parc, char *parv[])
{
	channel_t *c;
	mychan_t *mc = mychan_find(parv[0]);
	chanban_t *cb;
	mowgli_node_t *n, *tn;
	const char *item = parv[1], *p;
	int changes;

	if (item == NULL)
		item = "b";
	if (*item == '+' || *item == '-')
		item++;
	if (!strcmp(item, "*"))
		item = ircd->ban_like_modes;
	for (p = item; *p != '\0'; p++)
	{
		if (!strchr(ircd->ban_like_modes, *p))
		{
			command_fail(si, fault_badparams, _("Invalid mode; valid ones are %s."), ircd->ban_like_modes);
			return;
		}
	}
	if (*item == '\0')
	{
		command_fail(si, fault_badparams, _("Invalid mode; valid ones are %s."), ircd->ban_like_modes);
		return;
	}

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

	changes = 0;
	MOWGLI_ITER_FOREACH_SAFE(n, tn, c->bans.head)
	{
		cb = n->data;
		if (!strchr(item, cb->type))
			continue;
		modestack_mode_param(chansvs.nick, c, MTYPE_DEL, cb->type, cb->mask);
		chanban_delete(cb);
		changes++;
	}

	if (changes > 4)
		command_add_flood(si, FLOOD_MODERATE);

	logcommand(si, CMDLOG_DO, "CLEAR:BANS: \2%s\2 on \2%s\2",
			item, mc->name);

	command_success_nodata(si, _("Cleared %s modes on \2%s\2 (%d removed)."),
			item, mc->name, changes);

	if (changes > 0)
	{
		if (changes > 1)
			verbose(mc, "\2%s\2 removed all %d channel modes of type: %s",
				get_source_name(si), changes, item);
		else
			verbose(mc, "\2%s\2 removed 1 channel mode of type: %s",
				get_source_name(si), item);
	}

	if (module_locate_symbol("chanserv/history", "add_history_entry"))
	{
		add_history_entry = module_locate_symbol("chanserv/history", "add_history_entry");
	}

	if (add_history_entry != NULL)
	{
		char desc[350];

		snprintf(desc, sizeof desc, "Cleared all %s modes (removed %d).", item, changes);

		add_history_entry(si, mc, desc);
	}
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
