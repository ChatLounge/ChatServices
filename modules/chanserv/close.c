/*
 * Copyright (c) 2005-2006 Atheme Development Group
 * Copyright (c) 2017 ChatLounge IRC Network Development Team
 *
 * Rights to this code are as documented in doc/LICENSE.
 *
 * Closing for channels.
 *
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"chanserv/close", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	VENDOR_STRING
);

void (*add_history_entry)(sourceinfo_t *si, mychan_t *mc, const char *desc) = NULL;

static void cs_cmd_close(sourceinfo_t *si, int parc, char *parv[]);

/* CLOSE ON|OFF -- don't pollute the root with REOPEN */
command_t cs_close = { "CLOSE", N_("Closes a channel."),
			PRIV_CHAN_ADMIN, 3, cs_cmd_close , { .path = "cservice/close" }};

static void close_check_join(hook_channel_joinpart_t *data);

void _modinit(module_t *m)
{
	service_named_bind_command("chanserv", &cs_close);
	hook_add_event("channel_join");
	hook_add_first_channel_join(close_check_join);
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("chanserv", &cs_close);
	hook_del_channel_join(close_check_join);
}

static void close_check_join(hook_channel_joinpart_t *data)
{
	mychan_t *mc;
	chanuser_t *cu = data->cu;

	if (cu == NULL || is_internal_client(cu->user))
		return;
	mc = mychan_find(cu->chan->name);
	if (mc == NULL)
		return;

	if (metadata_find(mc, "private:close:closer"))
	{
		/* don't join if we're already in there */
		if (!chanuser_find(cu->chan, user_find_named(chansvs.nick)))
			join(cu->chan->name, chansvs.nick);

		/* stay for a bit to stop rejoin floods */
		mc->flags |= MC_INHABIT;

		/* lock it down */
		channel_mode_va(chansvs.me->me, cu->chan, 3, "+isbl", "*!*@*", "1");

		/* clear the channel */
		kick(chansvs.me->me, cu->chan, cu->user, "This channel has been closed");
		data->cu = NULL;
	}
}

static void cs_cmd_close(sourceinfo_t *si, int parc, char *parv[])
{
	char *target = parv[0];
	char *action = parv[1];
	char *reason = parv[2];
	mychan_t *mc;
	channel_t *c;
	chanuser_t *cu;
	mowgli_node_t *n, *tn;

	if (!target || !action)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "CLOSE");
		command_fail(si, fault_needmoreparams, _("Usage: CLOSE <#channel> <ON|OFF> [reason]"));
		return;
	}

	if (!(mc = mychan_find(target)))
	{
		command_fail(si, fault_nosuch_target, _("Channel \2%s\2 is not registered."), target);
		return;
	}

	if (!strcasecmp(action, "ON"))
	{
		if (!reason)
		{
			command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "CLOSE");
			command_fail(si, fault_needmoreparams, _("Usage: CLOSE <#channel> ON <reason>"));
			return;
		}

		if (mc->flags & CHAN_LOG)
		{
			command_fail(si, fault_noprivs, _("\2%s\2 cannot be closed."), target);
			return;
		}

		if (metadata_find(mc, "private:close:closer"))
		{
			command_fail(si, fault_nochange, _("\2%s\2 is already closed."), target);
			return;
		}

		metadata_add(mc, "private:close:closer", get_oper_name(si));
		metadata_add(mc, "private:close:reason", reason);
		metadata_add(mc, "private:close:timestamp", number_to_string(CURRTIME));

		if ((c = channel_find(target)))
		{
			if (!chanuser_find(c, user_find_named(chansvs.nick)))
				join(target, chansvs.nick);

			/* stay for a bit to stop rejoin floods */
			mc->flags |= MC_INHABIT;

			/* lock it down */
			channel_mode_va(chansvs.me->me, c, 3, "+isbl", "*!*@*", "1");

			/* clear the channel */
			MOWGLI_ITER_FOREACH_SAFE(n, tn, c->members.head)
			{
				cu = (chanuser_t *)n->data;

				if (!is_internal_client(cu->user))
					kick(chansvs.me->me, c, cu->user, "This channel has been closed");
			}
		}

		wallops("%s closed the channel: \2%s\2 (%s)", get_oper_name(si), mc->name, reason);
		logcommand(si, CMDLOG_ADMIN, "CLOSE:ON: \2%s\2 (reason: \2%s\2)", mc->name, reason);
		command_success_nodata(si, _("\2%s\2 is now closed."), mc->name);

		if (module_locate_symbol("chanserv/history", "add_history_entry"))
		{
			add_history_entry = module_locate_symbol("chanserv/history", "add_history_entry");
		}

		if (add_history_entry != NULL)
		{
			char desc[350];

			snprintf(desc, sizeof desc, "Channel has been temporarily closed.");

			add_history_entry(si, mc, desc);
		}
	}
	else if (!strcasecmp(action, "OFF"))
	{
		if (!metadata_find(mc, "private:close:closer"))
		{
			command_fail(si, fault_nochange, _("\2%s\2 is not closed."), target);
			return;
		}

		metadata_delete(mc, "private:close:closer");
		metadata_delete(mc, "private:close:reason");
		metadata_delete(mc, "private:close:timestamp");
		mc->flags &= ~MC_INHABIT;
		c = channel_find(target);
		if (c != NULL)
			if (chanuser_find(c, user_find_named(chansvs.nick)))
				part(c->name, chansvs.nick);
		c = channel_find(target);
		if (c != NULL)
		{
			/* hmm, channel still exists, probably permanent? */
			channel_mode_va(chansvs.me->me, c, 2, "-isbl", "*!*@*");
			check_modes(mc, true);
		}

		wallops("%s reopened the channel: \2%s\2", get_oper_name(si), mc->name);
		logcommand(si, CMDLOG_ADMIN, "CLOSE:OFF: \2%s\2", mc->name);
		command_success_nodata(si, _("\2%s\2 has been reopened."), mc->name);

		if (module_locate_symbol("chanserv/history", "add_history_entry"))
		{
			add_history_entry = module_locate_symbol("chanserv/history", "add_history_entry");
		}

		if (add_history_entry != NULL)
		{
			char desc[350];

			snprintf(desc, sizeof desc, "Channel has been reopened.");

			add_history_entry(si, mc, desc);
		}
	}
	else
	{
		command_fail(si, fault_badparams, STR_INVALID_PARAMS, "CLOSE");
		command_fail(si, fault_badparams, _("Usage: CLOSE <#channel> <ON|OFF> [reason]"));
	}
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
