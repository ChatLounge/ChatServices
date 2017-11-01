/*
 * Copyright (c) 2005 William Pitcock, et al.
 * Copyright (c) 2016 ChatLounge IRC Network Development Team
 *
 * Rights to this code are as documented in doc/LICENSE.
 *
 * This file contains code for the CService BAN/UNBAN function.
 *
 * This module and chanserv/unban_self may not be loaded at the same time.
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"chanserv/ban", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	VENDOR_STRING
);

static void cs_cmd_ban(sourceinfo_t *si, int parc, char *parv[]);
static void cs_cmd_unban(sourceinfo_t *si, int parc, char *parv[]);

command_t cs_ban = { "BAN", N_("Sets a ban on a channel."),
                        AC_AUTHENTICATED, 2, cs_cmd_ban, { .path = "cservice/ban" } };
command_t cs_unban = { "UNBAN", N_("Removes a ban on a channel."),
			AC_AUTHENTICATED, 2, cs_cmd_unban, { .path = "cservice/unban" } };

void _modinit(module_t *m)
{
	MODULE_CONFLICT(m, "chanserv/unban_self");
	service_named_bind_command("chanserv", &cs_ban);
	service_named_bind_command("chanserv", &cs_unban);
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("chanserv", &cs_ban);
	service_named_unbind_command("chanserv", &cs_unban);
}

static void cs_cmd_ban(sourceinfo_t *si, int parc, char *parv[])
{
	char *channel = parv[0];
	char *target = parv[1];
	char *newtarget;
	channel_t *c = channel_find(channel);
	mychan_t *mc = mychan_find(channel);
	user_t *tu;

	if (!channel || !target)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "BAN");
		command_fail(si, fault_needmoreparams, _("Syntax: BAN <#channel> <nickname|hostmask>"));
		return;
	}

	if (!mc)
	{
		command_fail(si, fault_nosuch_target, _("Channel \2%s\2 is not registered."), channel);
		return;
	}

	if (!c)
	{
		command_fail(si, fault_nosuch_target, _("\2%s\2 is currently empty."), channel);
		return;
	}

	if (!(chanacs_source_has_flag(mc, si, CA_REMOVE) || chanacs_source_has_flag(mc, si, CA_FOUNDER)))
	{
		command_fail(si, fault_noprivs, _("You are not authorized to perform this operation."));
		return;
	}

	if (metadata_find(mc, "private:close:closer"))
	{
		command_fail(si, fault_noprivs, _("\2%s\2 is closed."), channel);
		return;
	}

	if ((tu = user_find_named(target)))
	{
		char hostbuf[BUFSIZE];

		hostbuf[0] = '\0';

		mowgli_strlcat(hostbuf, "*!*@", BUFSIZE);
		mowgli_strlcat(hostbuf, tu->vhost, BUFSIZE);

		modestack_mode_param(chansvs.nick, c, MTYPE_ADD, 'b', hostbuf);
		chanban_add(c, hostbuf, 'b');
		logcommand(si, CMDLOG_DO, "BAN: \2%s\2 on \2%s\2 (for user \2%s!%s@%s\2)", hostbuf, mc->name, tu->nick, tu->user, tu->vhost);
		if (si->su == NULL || !chanuser_find(mc->chan, si->su))
			command_success_nodata(si, _("Banned \2%s\2 on \2%s\2."), target, channel);
		return;
	}
	else if ((is_extban(target) && (newtarget = target)) || ((newtarget = pretty_mask(target)) && validhostmask(newtarget)))
	{
		modestack_mode_param(chansvs.nick, c, MTYPE_ADD, 'b', newtarget);
		chanban_add(c, newtarget, 'b');
		logcommand(si, CMDLOG_DO, "BAN: \2%s\2 on \2%s\2", newtarget, mc->name);
		if (si->su == NULL || !chanuser_find(mc->chan, si->su))
			command_success_nodata(si, _("Banned \2%s\2 on \2%s\2."), newtarget, channel);
		return;
	}
	else
	{
		command_fail(si, fault_badparams, _("Invalid nickname/hostmask provided: \2%s\2"), target);
		command_fail(si, fault_badparams, _("Syntax: BAN <#channel> <nickname|hostmask>"));
		return;
	}
}

static void cs_cmd_unban(sourceinfo_t *si, int parc, char *parv[])
{
        const char *channel = parv[0];
        const char *target = parv[1];
        channel_t *c = channel_find(channel);
	mychan_t *mc = mychan_find(channel);
	user_t *tu;
	chanban_t *cb;

	if (!channel)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "UNBAN");
		command_fail(si, fault_needmoreparams, _("Syntax: UNBAN <#channel> <nickname|hostmask>"));
		return;
	}

	if (!target)
	{
		if (si->su == NULL)
		{
			command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "UNBAN");
			command_fail(si, fault_needmoreparams, _("Syntax: UNBAN <#channel> <nickname|hostmask>"));
			return;
		}
		target = si->su->nick;
	}

	if (!mc)
	{
		command_fail(si, fault_nosuch_target, _("Channel \2%s\2 is not registered."), channel);
		return;
	}

	if (!c)
	{
		command_fail(si, fault_nosuch_target, _("\2%s\2 is currently empty."), channel);
		return;
	}

	if (!chanacs_source_has_flag(mc, si, CA_REMOVE) &&
			(si->su == NULL ||
			 !chanacs_source_has_flag(mc, si, CA_EXEMPT) ||
			 irccasecmp(target, si->su->nick)))
	{
		command_fail(si, fault_noprivs, _("You are not authorized to perform this operation."));
		return;
	}

	if ((tu = user_find_named(target)))
	{
		mowgli_node_t *n, *tn;
		char hostbuf2[BUFSIZE];
		int count = 0;

		snprintf(hostbuf2, BUFSIZE, "%s!%s@%s", tu->nick, tu->user, tu->vhost);
		for (n = next_matching_ban(c, tu, 'b', c->bans.head); n != NULL; n = next_matching_ban(c, tu, 'b', tn))
		{
			tn = n->next;
			cb = n->data;

			logcommand(si, CMDLOG_DO, "UNBAN: \2%s\2 on \2%s\2 (for user \2%s\2)", cb->mask, mc->name, hostbuf2);
			modestack_mode_param(chansvs.nick, c, MTYPE_DEL, cb->type, cb->mask);
			chanban_delete(cb);
			count++;
		}
		if (count > 0)
			command_success_nodata(si, _("Unbanned \2%s\2 on \2%s\2 (%d ban%s removed)."),
				target, channel, count, (count != 1 ? "s" : ""));
		else
			command_success_nodata(si, _("No bans found matching \2%s\2 on \2%s\2."), target, channel);
		return;
	}
	else if ((cb = chanban_find(c, target, 'b')) != NULL || validhostmask(target))
	{
		if (cb)
		{
			modestack_mode_param(chansvs.nick, c, MTYPE_DEL, 'b', target);
			chanban_delete(cb);
			logcommand(si, CMDLOG_DO, "UNBAN: \2%s\2 on \2%s\2", target, mc->name);
			if (si->su == NULL || !chanuser_find(mc->chan, si->su))
				command_success_nodata(si, _("Unbanned \2%s\2 on \2%s\2."), target, channel);
		}
		else
			command_fail(si, fault_nosuch_key, _("No such ban \2%s\2 on \2%s\2."), target, channel);

		return;
	}
        else
        {
		command_fail(si, fault_badparams, _("Invalid nickname/hostmask provided: \2%s\2"), target);
		command_fail(si, fault_badparams, _("Syntax: UNBAN <#channel> [nickname|hostmask]"));
		return;
        }
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
