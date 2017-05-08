/*
 * Copyright (c) 2005 Atheme Development Group
 * Copyright (c) 2016-2017 ChatLounge IRC Network Development Team
 *
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains the main() routine.
 *
 */

#include "atheme.h"
#include "chanserv.h"
#include "template.h"
#include <limits.h>

#define CHANNEL_HISTORY_LIMIT 1000

DECLARE_MODULE_V1
(
	"chanserv/main", true, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net>"
);

void (*add_history_entry)(sourceinfo_t *si, mychan_t *mc, const char *desc) = NULL;
void (*add_history_entry_misc)(mychan_t *mc, const char *desc) = NULL;
bool *(*send_user_memo)(sourceinfo_t *si, myuser_t *target,
	const char *memotext, bool verbose, unsigned int status, bool senduseremail) = NULL;
bool *(*send_user_memo_misc)(myuser_t *target,
	const char *memotext, unsigned int status, bool senduseremail) = NULL;

static void cs_join(hook_channel_joinpart_t *hdata);
static void cs_part(hook_channel_joinpart_t *hdata);
static void cs_register(hook_channel_req_t *mc);
static void cs_succession(hook_channel_succession_req_t *data);
static void cs_newchan(channel_t *c);
static void cs_keeptopic_topicset(channel_t *c);
static void cs_topiccheck(hook_channel_topic_check_t *data);
static void cs_tschange(channel_t *c);
static void cs_leave_empty(void *unused);
static void on_shutdown(void *unused);

static mowgli_eventloop_timer_t *cs_leave_empty_timer = NULL;

static void join_registered(bool all)
{
	mychan_t *mc;
	mowgli_patricia_iteration_state_t state;

	MOWGLI_PATRICIA_FOREACH(mc, &state, mclist)
	{
		if (!(mc->flags & MC_GUARD))
			continue;
		if (metadata_find(mc, "private:botserv:bot-assigned") != NULL)
			continue;

		if (all)
		{
			join(mc->name, chansvs.nick);
			continue;
		}
		else if (mc->chan != NULL && mc->chan->members.count != 0)
		{
			join(mc->name, chansvs.nick);
			continue;
		}
	}
}

/* main services client routine */
static void chanserv(sourceinfo_t *si, int parc, char *parv[])
{
	mychan_t *mc = NULL;
	char orig[BUFSIZE];
	char newargs[BUFSIZE];
	char *cmd;
	char *args;

	/* this should never happen */
	if (parv[parc - 2][0] == '&')
	{
		slog(LG_ERROR, "services(): got parv with local channel: %s", parv[0]);
		return;
	}

	/* is this a fantasy command? */
	if (parv[parc - 2][0] == '#')
	{
		metadata_t *md;

		if (chansvs.fantasy == false)
		{
			/* *all* fantasy disabled */
			return;
		}

		mc = mychan_find(parv[parc - 2]);
		if (!mc)
		{
			/* unregistered, NFI how we got this message, but let's leave it alone! */
			return;
		}

		md = metadata_find(mc, "disable_fantasy");
		if (md)
		{
			/* fantasy disabled on this channel. don't message them, just bail. */
			return;
		}
	}

	/* make a copy of the original for debugging */
	mowgli_strlcpy(orig, parv[parc - 1], BUFSIZE);

	/* lets go through this to get the command */
	cmd = strtok(parv[parc - 1], " ");

	if (!cmd)
		return;
	if (*cmd == '\001')
	{
		handle_ctcp_common(si, cmd, strtok(NULL, ""));
		return;
	}

	/* take the command through the hash table */
	if (mc == NULL)
		command_exec_split(si->service, si, cmd, strtok(NULL, ""), si->service->commands);
	else
	{
		metadata_t *md = metadata_find(mc, "private:prefix");
		const char *prefix = (md ? md->value : chansvs.trigger);

		if (strlen(cmd) >= 2 && strchr(prefix, cmd[0]) && isalpha(*++cmd))
		{
			const char *realcmd = service_resolve_alias(si->service, NULL, cmd);

			/* XXX not really nice to look up the command twice
			 * -- jilles */
			if (command_find(si->service->commands, realcmd) == NULL)
				return;
			if (floodcheck(si->su, si->service->me))
				return;
			/* construct <channel> <args> */
			mowgli_strlcpy(newargs, parv[parc - 2], sizeof newargs);
			args = strtok(NULL, "");
			if (args != NULL)
			{
				mowgli_strlcat(newargs, " ", sizeof newargs);
				mowgli_strlcat(newargs, args, sizeof newargs);
			}
			/* let the command know it's called as fantasy cmd */
			si->c = mc->chan;
			/* fantasy commands are always verbose
			 * (a little ugly but this way we can !set verbose)
			 */
			mc->flags |= MC_FORCEVERBOSE;
			command_exec_split(si->service, si, realcmd, newargs, si->service->commands);
			mc->flags &= ~MC_FORCEVERBOSE;
		}
		else if (!ircncasecmp(cmd, chansvs.nick, strlen(chansvs.nick)) && !isalnum(cmd[strlen(chansvs.nick)]) && (cmd = strtok(NULL, "")) != NULL)
		{
			const char *realcmd;
			char *pptr;

			mowgli_strlcpy(newargs, parv[parc - 2], sizeof newargs);
			while (*cmd == ' ')
				cmd++;
			if ((pptr = strchr(cmd, ' ')) != NULL)
			{
				mowgli_strlcat(newargs, pptr, sizeof newargs);
				*pptr = '\0';
			}

			realcmd = service_resolve_alias(si->service, NULL, cmd);

			if (command_find(si->service->commands, realcmd) == NULL)
				return;
			if (floodcheck(si->su, si->service->me))
				return;

			si->c = mc->chan;

			/* fantasy commands are always verbose
			 * (a little ugly but this way we can !set verbose)
			 */
			mc->flags |= MC_FORCEVERBOSE;
			command_exec_split(si->service, si, realcmd, newargs, si->service->commands);
			mc->flags &= ~MC_FORCEVERBOSE;
		}
	}
}

static void chanserv_config_ready(void *unused)
{
	chansvs.nick = chansvs.me->nick;
	chansvs.user = chansvs.me->user;
	chansvs.host = chansvs.me->host;
	chansvs.real = chansvs.me->real;

	service_set_chanmsg(chansvs.me, true);

	if (me.connected)
		join_registered(false); /* !config_options.leave_chans */
}

static int c_ci_vop(mowgli_config_file_entry_t *ce)
{
	if (ce->vardata == NULL)
	{
		conf_report_warning(ce, "no parameter for configuration option");
		return 0;
	}

	set_global_template_flags("VOP", flags_to_bitmask(ce->vardata, 0));

	return 0;
}

static int c_ci_hop(mowgli_config_file_entry_t *ce)
{
	if (ce->vardata == NULL)
	{
		conf_report_warning(ce, "no parameter for configuration option");
		return 0;
	}

	set_global_template_flags("HOP", flags_to_bitmask(ce->vardata, 0));

	return 0;
}

static int c_ci_aop(mowgli_config_file_entry_t *ce)
{
	if (ce->vardata == NULL)
	{
		conf_report_warning(ce, "no parameter for configuration option");
		return 0;
	}

	set_global_template_flags("AOP", flags_to_bitmask(ce->vardata, 0));

	return 0;
}

static int c_ci_sop(mowgli_config_file_entry_t *ce)
{
	if (ce->vardata == NULL)
	{
		conf_report_warning(ce, "no parameter for configuration option");
		return 0;
	}

	set_global_template_flags("SOP", flags_to_bitmask(ce->vardata, 0));

	return 0;
}

static int c_ci_templates(mowgli_config_file_entry_t *ce)
{
	mowgli_config_file_entry_t *flce;

	MOWGLI_ITER_FOREACH(flce, ce->entries)
	{
		if (flce->vardata == NULL)
		{
			conf_report_warning(ce, "no parameter for configuration option");
			return 0;
		}

		set_global_template_flags(flce->varname, flags_to_bitmask(flce->vardata, 0));
	}

	return 0;
}

/* Convert flags_requires_account into an unsigned integer representation,
 * ensure CA_FOUNDER is included, and CA_AKICK is NOT included.
 */

static int c_ci_flags_requires_account(mowgli_config_file_entry_t *ce)
{
	unsigned int flags;

	if (ce->vardata == NULL)
	{
		chansvs.flags_req_acct = CA_FOUNDER;
		return 0;
	}

	flags = flags_to_bitmask((char *)ce->vardata, CA_FOUNDER);

	flags &= ~CA_AKICK;

	chansvs.flags_req_acct |= flags;

	return 0;
}

void _modinit(module_t *m)
{
	chansvs.flags_req_acct = CA_FOUNDER;

	hook_add_event("config_ready");
	hook_add_config_ready(chanserv_config_ready);

	chansvs.me = service_add("chanserv", chanserv);

	hook_add_event("channel_join");
	hook_add_event("channel_part");
	hook_add_event("channel_register");
	hook_add_event("channel_succession");
	hook_add_event("channel_add");
	hook_add_event("channel_topic");
	hook_add_event("channel_can_change_topic");
	hook_add_event("channel_tschange");
	hook_add_event("user_identify");
	hook_add_event("shutdown");
	hook_add_channel_join(cs_join);
	hook_add_channel_part(cs_part);
	hook_add_channel_register(cs_register);
	hook_add_channel_succession(cs_succession);
	hook_add_channel_add(cs_newchan);
	hook_add_channel_topic(cs_keeptopic_topicset);
	hook_add_channel_can_change_topic(cs_topiccheck);
	hook_add_channel_tschange(cs_tschange);
	hook_add_shutdown(on_shutdown);

	cs_leave_empty_timer = mowgli_timer_add(base_eventloop, "cs_leave_empty", cs_leave_empty, NULL, 300);

	/* chanserv{} block */
	add_bool_conf_item("FANTASY", &chansvs.me->conf_table, 0, &chansvs.fantasy, false);
	add_conf_item("VOP", &chansvs.me->conf_table, c_ci_vop);
	add_conf_item("HOP", &chansvs.me->conf_table, c_ci_hop);
	add_conf_item("AOP", &chansvs.me->conf_table, c_ci_aop);
	add_conf_item("SOP", &chansvs.me->conf_table, c_ci_sop);
	add_conf_item("TEMPLATES", &chansvs.me->conf_table, c_ci_templates);
	add_bool_conf_item("CHANGETS", &chansvs.me->conf_table, 0, &chansvs.changets, false);
	add_bool_conf_item("NO_LEVELED_FLAGS", &chansvs.me->conf_table, 0, &chansvs.no_leveled_flags, false);
	add_uint_conf_item("MIN_NON_WILDCARD_CHARS_HOST_ACL", &chansvs.me->conf_table, 0, &chansvs.min_non_wildcard_chars_host_acl, 0, INT_MAX, 0);
	add_bool_conf_item("HIDE_XOP", &chansvs.me->conf_table, 0, &chansvs.hide_xop, false);
	add_dupstr_conf_item("TRIGGER", &chansvs.me->conf_table, 0, &chansvs.trigger, "!");
	add_duration_conf_item("EXPIRE", &chansvs.me->conf_table, 0, &chansvs.expiry, "d", 0);
	add_uint_conf_item("MAXCHANS", &chansvs.me->conf_table, 0, &chansvs.maxchans, 1, INT_MAX, 5);
	add_uint_conf_item("MAXCHANACS", &chansvs.me->conf_table, 0, &chansvs.maxchanacs, 0, INT_MAX, 0);
	add_uint_conf_item("MAXFOUNDERS", &chansvs.me->conf_table, 0, &chansvs.maxfounders, 1, (512 - 60) / (9 + 2), 4); /* fit on a line */
	add_dupstr_conf_item("FOUNDER_FLAGS", &chansvs.me->conf_table, 0, &chansvs.founder_flags, NULL);
	add_dupstr_conf_item("DEFTEMPLATES", &chansvs.me->conf_table, 0, &chansvs.deftemplates, NULL);
	add_duration_conf_item("AKICK_TIME", &chansvs.me->conf_table, 0, &chansvs.akick_time, "m", 0);
	add_bool_conf_item("USE_OWNER", &chansvs.me->conf_table, 0, &chansvs.use_owner, false);
	add_bool_conf_item("USE_ADMIN", &chansvs.me->conf_table, 0, &chansvs.use_admin, false);
	add_conf_item("FLAGS_REQUIRES_ACCOUNT", &chansvs.me->conf_table, c_ci_flags_requires_account);
}

void _moddeinit(module_unload_intent_t intent)
{
	if (chansvs.me)
	{
		chansvs.nick = NULL;
		chansvs.user = NULL;
		chansvs.host = NULL;
		chansvs.real = NULL;
		service_delete(chansvs.me);
		chansvs.me = NULL;
	}

	hook_del_config_ready(chanserv_config_ready);
	hook_del_channel_join(cs_join);
	hook_del_channel_part(cs_part);
	hook_del_channel_register(cs_register);
	hook_del_channel_succession(cs_succession);
	hook_del_channel_add(cs_newchan);
	hook_del_channel_topic(cs_keeptopic_topicset);
	hook_del_channel_can_change_topic(cs_topiccheck);
	hook_del_channel_tschange(cs_tschange);
	hook_del_shutdown(on_shutdown);

	mowgli_timer_destroy(base_eventloop, cs_leave_empty_timer);
}

static void cs_join(hook_channel_joinpart_t *hdata)
{
	chanuser_t *cu = hdata->cu;
	user_t *u;
	channel_t *chan;
	mychan_t *mc;
	unsigned int flags;
	bool noop;
	bool secure;
	bool guard;
	metadata_t *md;
	chanacs_t *ca2;
	char akickreason[120] = "User is banned from this channel", *p;

	if (cu == NULL || is_internal_client(cu->user))
		return;
	u = cu->user;
	chan = cu->chan;

	/* first check if this is a registered channel at all */
	mc = mychan_find(chan->name);
	if (mc == NULL)
		return;

	flags = chanacs_user_flags(mc, u);
	noop = mc->flags & MC_NOOP || (u->myuser != NULL &&
			u->myuser->flags & MU_NOOP);
	/* attempt to deop people recreating channels, if the more
	 * sophisticated mechanism is disabled */
	secure = mc->flags & MC_SECURE || (!chansvs.changets &&
			chan->nummembers == 1 && chan->ts > CURRTIME - 300);
	/* chanserv or a botserv bot should join */
	guard = mc->flags & MC_GUARD ||
		metadata_find(mc, "private:botserv:bot-assigned") != NULL;

	if (chan->nummembers == 1 && mc->flags & MC_GUARD &&
		metadata_find(mc, "private:botserv:bot-assigned") == NULL)
		join(chan->name, chansvs.nick);

	/*
	 * CS SET RESTRICTED: if they don't have any access (excluding AKICK)
	 * or special privs to join restricted chans, boot them. -- w00t
	 */
	if ((mc->flags & MC_RESTRICTED) && !(flags & CA_ALLPRIVS) && !has_priv_user(u, PRIV_JOIN_STAFFONLY))
	{
		/* Stay on channel if this would empty it -- jilles */
		if (chan->nummembers <= (guard ? 2 : 1))
		{
			mc->flags |= MC_INHABIT;
			if (!guard)
				join(chan->name, chansvs.nick);
		}
		if (mc->mlock_on & CMODE_INVITE || chan->modes & CMODE_INVITE)
		{
			if (!(chan->modes & CMODE_INVITE))
				check_modes(mc, true);
			remove_banlike(chansvs.me->me, chan, ircd->invex_mchar, u);
			modestack_flush_channel(chan);
		}
		else
		{
			ban(chansvs.me->me, chan, u);
			remove_ban_exceptions(chansvs.me->me, chan, u);
		}
		try_kick(chansvs.me->me, chan, u, "You are not authorized to be on this channel");
		hdata->cu = NULL;
		return;
	}

	if (flags & CA_AKICK && !(flags & CA_EXEMPT))
	{
		/* Stay on channel if this would empty it -- jilles */
		if (chan->nummembers <= (guard ? 2 : 1))
		{
			mc->flags |= MC_INHABIT;
			if (!guard)
				join(chan->name, chansvs.nick);
		}
		/* use a user-given ban mask if possible -- jilles */
		ca2 = chanacs_find_host_by_user(mc, u, CA_AKICK);
		if (ca2 != NULL)
		{
			if (chanban_find(chan, ca2->host, 'b') == NULL)
			{
				chanban_add(chan, ca2->host, 'b');
				modestack_mode_param(chansvs.nick, chan, MTYPE_ADD, 'b', ca2->host);
				modestack_flush_channel(chan);
			}
		}
		else
		{
			/* XXX this could be done more efficiently */
			ca2 = chanacs_find(mc, entity(u->myuser), CA_AKICK);
			ban(chansvs.me->me, chan, u);
		}
		remove_ban_exceptions(chansvs.me->me, chan, u);
		if (ca2 != NULL)
		{
			md = metadata_find(ca2, "reason");
			if (md != NULL && *md->value != '|')
			{
				snprintf(akickreason, sizeof akickreason,
						"Banned: %s", md->value);
				p = strchr(akickreason, '|');
				if (p != NULL)
					*p = '\0';
				else
					p = akickreason + strlen(akickreason);
				/* strip trailing spaces, so as not to
				 * disclose the existence of an oper reason */
				p--;
				while (p > akickreason && *p == ' ')
					p--;
				p[1] = '\0';
			}
		}
		try_kick(chansvs.me->me, chan, u, akickreason);
		hdata->cu = NULL;
		return;
	}

	/* Kick out users who may be recreating channels mlocked +i.
	 * Users with +i flag are allowed to join, as are users matching
	 * an invite exception (the latter only works if the channel already
	 * exists because members are sent before invite exceptions).
	 * Because most networks do not use kick_on_split_riding or
	 * no_create_on_split, do not trust users coming back from splits;
	 * with the exception of users coming back after a services
	 * restart if the channel TS did not change.
	 * Unfortunately, this kicks users who have been invited by a channel
	 * operator, after a split.
	 */
	if (mc->mlock_on & CMODE_INVITE && !(flags & CA_INVITE) &&
			(!me.bursting || mc->flags & MC_RECREATED) &&
			(!(u->server->flags & SF_EOB) || (chan->nummembers <= 2 && (chan->nummembers <= 1 || chanuser_find(chan, chansvs.me->me)))) &&
			(!ircd->invex_mchar || !next_matching_ban(chan, u, ircd->invex_mchar, chan->bans.head)))
	{
		if (chan->nummembers <= (guard ? 2 : 1))
		{
			mc->flags |= MC_INHABIT;
			if (!guard)
				join(chan->name, chansvs.nick);
		}
		if (!(chan->modes & CMODE_INVITE))
			check_modes(mc, true);
		modestack_flush_channel(chan);
		try_kick(chansvs.me->me, chan, u, "Invite only channel");
		hdata->cu = NULL;
		return;
	}

	/* A second user joined and was not kicked; we do not need
	 * to stay on the channel artificially.
	 * If there is only one user, stay in the channel to avoid
	 * triggering autocycle-for-ops scripts and immediately
	 * destroying channels with kick on split riding.
	 */
	if (mc->flags & MC_INHABIT && chan->nummembers >= 3)
	{
		mc->flags &= ~MC_INHABIT;
		if (!(mc->flags & MC_GUARD) && !(chan->flags & CHAN_LOG) && chanuser_find(chan, chansvs.me->me))
			part(chan->name, chansvs.nick);
	}

	if (ircd->uses_owner)
	{
		if (flags & CA_USEOWNER)
		{
			if (flags & CA_AUTOOP && !(noop || cu->modes & ircd->owner_mode))
			{
				modestack_mode_param(chansvs.nick, chan, MTYPE_ADD, ircd->owner_mchar[1], CLIENT_NAME(u));
				cu->modes |= ircd->owner_mode;
			}
		}
		else if (secure && (cu->modes & ircd->owner_mode))
		{
			modestack_mode_param(chansvs.nick, chan, MTYPE_DEL, ircd->owner_mchar[1], CLIENT_NAME(u));
			cu->modes &= ~ircd->owner_mode;
		}
	}

	if (ircd->uses_protect)
	{
		if (flags & CA_USEPROTECT)
		{
			if (flags & CA_AUTOOP && !(noop || cu->modes & ircd->protect_mode || (ircd->uses_owner && cu->modes & ircd->owner_mode)))
			{
				modestack_mode_param(chansvs.nick, chan, MTYPE_ADD, ircd->protect_mchar[1], CLIENT_NAME(u));
				cu->modes |= ircd->protect_mode;
			}
		}
		else if (secure && (cu->modes & ircd->protect_mode))
		{
			modestack_mode_param(chansvs.nick, chan, MTYPE_DEL, ircd->protect_mchar[1], CLIENT_NAME(u));
			cu->modes &= ~ircd->protect_mode;
		}
	}

	if (flags & CA_AUTOOP)
	{
		if (!(noop || cu->modes & CSTATUS_OP))
		{
			modestack_mode_param(chansvs.nick, chan, MTYPE_ADD, 'o', CLIENT_NAME(u));
			cu->modes |= CSTATUS_OP;
		}
	}
	else if (secure && (cu->modes & CSTATUS_OP) && !(flags & CA_OP))
	{
		modestack_mode_param(chansvs.nick, chan, MTYPE_DEL, 'o', CLIENT_NAME(u));
		cu->modes &= ~CSTATUS_OP;
	}

	if (ircd->uses_halfops)
	{
		if (flags & CA_AUTOHALFOP)
		{
			if (!(noop || cu->modes & (CSTATUS_OP | ircd->halfops_mode)))
			{
				modestack_mode_param(chansvs.nick, chan, MTYPE_ADD, 'h', CLIENT_NAME(u));
				cu->modes |= ircd->halfops_mode;
			}
		}
		else if (secure && (cu->modes & ircd->halfops_mode) && !(flags & CA_HALFOP))
		{
			modestack_mode_param(chansvs.nick, chan, MTYPE_DEL, 'h', CLIENT_NAME(u));
			cu->modes &= ~ircd->halfops_mode;
		}
	}

	if (flags & CA_AUTOVOICE)
	{
		if (!(noop || cu->modes & (CSTATUS_OP | ircd->halfops_mode | CSTATUS_VOICE)))
		{
			modestack_mode_param(chansvs.nick, chan, MTYPE_ADD, 'v', CLIENT_NAME(u));
			cu->modes |= CSTATUS_VOICE;
		}
	}

	if (u->server->flags & SF_EOB && (md = metadata_find(mc, "private:entrymsg")))
	{
		if (metadata_find(mc, "private:botserv:bot-assigned") == NULL)
		{
			if (!u->myuser || !(u->myuser->flags & MU_NOGREET))
				notice(chansvs.nick, cu->user->nick, "[%s] %s", mc->name, md->value);
		}
	}

	if (u->server->flags & SF_EOB && (md = metadata_find(mc, "url")))
		numeric_sts(me.me, 328, cu->user, "%s :%s", mc->name, md->value);

	if (flags & CA_USEDUPDATE)
		mc->used = CURRTIME;
}

static void cs_part(hook_channel_joinpart_t *hdata)
{
	chanuser_t *cu;
	mychan_t *mc;

	cu = hdata->cu;
	if (cu == NULL)
		return;
	mc = mychan_find(cu->chan->name);
	if (mc == NULL)
		return;
	if (metadata_find(mc, "private:botserv:bot-assigned") != NULL)
		return;

	if (CURRTIME - mc->used >= 3600)
		if (chanacs_user_flags(mc, cu->user) & CA_USEDUPDATE)
			mc->used = CURRTIME;

	/*
	 * When channel_part is fired, we haven't yet removed the
	 * user from the room. So, the channel will have two members
	 * if ChanServ is joining channels: the triggering user and
	 * itself.
	 *
	 * This if block was utter nonsense.  Refactor it into multiple
	 * branches for better clarity and debugging ability. --nenolod
	 */

	/* we're not parting if we've been told to never part. */
	if (!config_options.leave_chans)
		return;

	/* we're not parting if the channel has more than one person on it */
	if (cu->chan->nummembers > 2)
		return;

	/* internal clients parting a channel shouldn't cause chanserv to leave. */
	if (is_internal_client(cu->user))
		return;

	/* if we're enforcing an akick, we're MC_INHABIT.  do not part. */
	if (mc->flags & MC_INHABIT)
	{
		slog(LG_DEBUG, "cs_part(): not leaving channel %s due to MC_INHABIT flag", mc->name);
		return;
	}

	/* seems we've met all conditions to be parted from a channel. */
	part(cu->chan->name, chansvs.nick);
}

static user_t *get_changets_user(mychan_t *mc)
{
	metadata_t *md;

	return_val_if_fail(mc != NULL, chansvs.me->me);

	md = metadata_find(mc, "private:botserv:bot-assigned");
	if (md != NULL)
	{
		user_t *u = user_find(md->value);

		return_val_if_fail(is_internal_client(u), chansvs.me->me);

		return u;
	}

	return chansvs.me->me;
}

static void cs_register(hook_channel_req_t *hdata)
{
	mychan_t *mc;

	mc = hdata->mc;
	if (mc->chan)
	{
		if (mc->flags & MC_GUARD)
			join(mc->name, chansvs.nick);
		if (metadata_find(mc, "private:botserv:bot-assigned") != NULL)
			return;

		mlock_sts(mc->chan);
		topiclock_sts(mc->chan);
		check_modes(mc, true);
	}
}

static void cs_succession(hook_channel_succession_req_t *data)
{
	chanacs_change_simple(data->mc, entity(data->mu), NULL, custom_founder_check(), 0, NULL);
}

/* Called on every set of a topic, after updating our internal structures */
static void cs_keeptopic_topicset(channel_t *c)
{
	mychan_t *mc;
	metadata_t *md;

	mc = mychan_find(c->name);

	if (mc == NULL)
		return;

	md = metadata_find(mc, "private:topic:text");
	if (md != NULL)
	{
		if (c->topic != NULL && !strcmp(md->value, c->topic))
			return;
		metadata_delete(mc, "private:topic:text");
	}

	if (metadata_find(mc, "private:topic:setter"))
		metadata_delete(mc, "private:topic:setter");

	if (metadata_find(mc, "private:topic:ts"))
		metadata_delete(mc, "private:topic:ts");

	if (c->topic && c->topic_setter)
	{
		slog(LG_DEBUG, "KeepTopic: topic set for %s by %s: %s", c->name,
			c->topic_setter, c->topic);
		metadata_add(mc, "private:topic:setter",
			c->topic_setter);
		metadata_add(mc, "private:topic:text",
			c->topic);
		metadata_add(mc, "private:topic:ts",
			number_to_string(c->topicts));
	}
	else
		slog(LG_DEBUG, "KeepTopic: topic cleared for %s", c->name);
}

/* Called when a topic change is received from the network, before altering
 * our internal structures */
static void cs_topiccheck(hook_channel_topic_check_t *data)
{
	mychan_t *mc;
	unsigned int accessfl = 0;

	mc = mychan_find(data->c->name);
	if (mc == NULL)
		return;

	if ((mc->flags & (MC_KEEPTOPIC | MC_TOPICLOCK)) == (MC_KEEPTOPIC | MC_TOPICLOCK))
	{
		if (data->u == NULL || !((accessfl = chanacs_user_flags(mc, data->u)) & CA_TOPIC))
		{
			/* topic burst or unauthorized user, revert it */
			data->approved = 1;
			slog(LG_DEBUG, "cs_topiccheck(): reverting topic change on channel %s by %s",
					data->c->name,
					data->u != NULL ? data->u->nick : "<server>");

			if (data->u != NULL && !(mc->mlock_off & CMODE_TOPIC))
			{
				/* they do not have access to be opped either,
				 * deop them and set +t */
				/* note: channel_mode() takes nicks, not UIDs
				 * when used with a non-NULL source */
				if (ircd->uses_halfops && !(accessfl & (CA_OP | CA_AUTOOP | CA_HALFOP | CA_AUTOHALFOP)))
					channel_mode_va(chansvs.me->me, data->c,
							3, "+t-oh", data->u->nick, data->u->nick);
				else if (!ircd->uses_halfops && !(accessfl & (CA_OP | CA_AUTOOP)))
					channel_mode_va(chansvs.me->me, data->c,
							2, "+t-o", data->u->nick);
			}
		}
	}
}

/* Called on creation of a channel */
static void cs_newchan(channel_t *c)
{
	mychan_t *mc;
	chanuser_t *cu;
	metadata_t *md;
	char *setter;
	char *text;
	time_t channelts = 0;
	time_t topicts;
	char str[21];

	if (!(mc = mychan_find(c->name)))
		return;

	/* set channel_t.mychan */
	c->mychan = mc;

	/* schedule a mode lock check when we know the current modes
	 * -- jilles */
	mc->flags |= MC_MLOCK_CHECK;

	md = metadata_find(mc, "private:channelts");
	if (md != NULL)
		channelts = atol(md->value);
	if (channelts == 0)
		channelts = mc->registered;

	if (c->ts > channelts && channelts > 0)
		mc->flags |= MC_RECREATED;
	else
		mc->flags &= ~MC_RECREATED;

	if (chansvs.changets && c->ts > channelts && channelts > 0)
	{
		user_t *u;

		u = get_changets_user(mc);

		/* Stop the splitrider -- jilles */
		c->ts = channelts;
		clear_simple_modes(c);
		c->modes |= CMODE_NOEXT | CMODE_TOPIC;
		check_modes(mc, false);
		/* No ops to clear */
		chan_lowerts(c, u);
		cu = chanuser_add(c, CLIENT_NAME(u));
		cu->modes |= CSTATUS_OP;
		/* make sure it parts again sometime (empty SJOIN etc) */
		mc->flags |= MC_INHABIT;
	}
	else if (c->ts != channelts)
	{
		snprintf(str, sizeof str, "%lu", (unsigned long)c->ts);
		metadata_add(mc, "private:channelts", str);
	}
	else if (!(MC_TOPICLOCK & mc->flags) && MOWGLI_LIST_LENGTH(&c->members) == 0)
	{
		mlock_sts(c);
		/* Same channel, let's assume ircd has kept topic.
		 * However, if topiclock is enabled, we must change it back
		 * regardless.
		 * Also, if there is someone in this channel already, it is
		 * being created by a service and we must restore.
		 * -- jilles */
		return;
	}

	mlock_sts(c);
	topiclock_sts(c);

	if (!(MC_KEEPTOPIC & mc->flags))
		return;

	md = metadata_find(mc, "private:topic:setter");
	if (md == NULL)
		return;
	setter = md->value;

	md = metadata_find(mc, "private:topic:text");
	if (md == NULL)
		return;
	text = md->value;

	md = metadata_find(mc, "private:topic:ts");
	if (md == NULL)
		return;
	topicts = atol(md->value);

	handle_topic(c, setter, topicts, text);
	topic_sts(c, chansvs.me->me, setter, topicts, 0, text);
}

static void cs_tschange(channel_t *c)
{
	mychan_t *mc;
	char str[21];

	if (!(mc = mychan_find(c->name)))
		return;

	/* store new TS */
	snprintf(str, sizeof str, "%lu", (unsigned long)c->ts);
	metadata_add(mc, "private:channelts", str);

	/* schedule a mode lock check when we know the new modes
	 * -- jilles */
	mc->flags |= MC_MLOCK_CHECK;

	/* reset the mlock if needed */
	mlock_sts(c);
	topiclock_sts(c);
}

static void on_shutdown(void *unused)
{
	if (chansvs.me != NULL && chansvs.me->me != NULL)
		quit_sts(chansvs.me->me, "shutting down");
}

static void cs_leave_empty(void *unused)
{
	mychan_t *mc;
	mowgli_patricia_iteration_state_t state;

	(void)unused;
	MOWGLI_PATRICIA_FOREACH(mc, &state, mclist)
	{
		if (!(mc->flags & MC_INHABIT))
			continue;
		/* If there is only one user, stay indefinitely. */
		if (mc->chan != NULL && mc->chan->nummembers == 2)
			continue;
		mc->flags &= ~MC_INHABIT;
		if (mc->chan != NULL &&
				!(mc->chan->flags & CHAN_LOG) &&
				(!(mc->flags & MC_GUARD) ||
				 (config_options.leave_chans && mc->chan->nummembers == 1) ||
				 metadata_find(mc, "private:close:closer")) &&
				chanuser_find(mc->chan, chansvs.me->me))
		{
			slog(LG_DEBUG, "cs_leave_empty(): leaving %s", mc->chan->name);
			part(mc->chan->name, chansvs.nick);
		}
	}
}

/* notify_target_acl_change: Tells the target user about the new flags in a
 *     channel, both in the form of a notice (if possible) and a memo (if
 *     possible).
 *
 * Inputs: myuser_t *smu - Source user
 *         myuser_t *tmu - Target user
 *         mychan_t *mc - Channel where the change is taking place.
 *         const char *flagstr - Flag string used to perform the change.
 *         unsigned int flags - current flags on the target user.
 * Outputs: None
 * Side Effects: Send a notice and/or memo to the target user, if possible.
 */
void notify_target_acl_change(sourceinfo_t *si, myuser_t *tmu, mychan_t *mc,
	const char *flagstr, unsigned int flags)
{
	char text[256], text2[300];

	if (si->smu == NULL || tmu == NULL || mc == NULL || flagstr == NULL)
		return;

	if (!(tmu->flags & MU_NOTIFYACL))
		return;

	if (flags == 0)
		snprintf(text, sizeof text, "\2%s\2 has set \2%s\2 and removed you from: \2%s\2",
			entity(si->smu)->name, flagstr, mc->name);
	else if (get_template_name(mc, flags))
		snprintf(text, sizeof text, "\2%s\2 has set \2%s\2 on you in \2%s\2 where you now have the flags: \2%s\2 (TEMPLATE: \2%s\2)",
			entity(si->smu)->name, flagstr, mc->name, bitmask_to_flags(flags),
			get_template_name(mc, flags));
	else
		snprintf(text, sizeof text, "\2%s\2 has set \2%s\2 on you in \2%s\2 where you now have the flags: \2%s\2",
			entity(si->smu)->name, flagstr, mc->name, bitmask_to_flags(flags));

	if (MOWGLI_LIST_LENGTH(&tmu->logins) > 0)
		myuser_notice(chansvs.nick, tmu, text);

	if (tmu->flags & MU_NOMEMO || !(tmu->flags & MU_NOTIFYMEMO))
		return;

	if ((send_user_memo = module_locate_symbol("memoserv/main", "send_user_memo")) == NULL)
		return;

	snprintf(text2, sizeof text2, "[automatic memo from \2%s\2] - %s", chansvs.nick, text);

	send_user_memo(si, tmu, text2, false, MEMO_CHANNEL, tmu->flags & MU_EMAILNOTIFY);

	return;
}

/* notify_channel_acl_change: Notifes the target channel management about
 *     the new flags in a channel in the form of a memo (if possible).
 *
 * Inputs: myuser_t *smu - Source user
 *         myuser_t *tmu - Target user
 *         mychan_t *mc - Channel where the change is taking place.
 *         const char *flagstr - Flag string used to perform the change.
 *         unsigned int flags - current flags on the target user.
 * Outputs: None
 * Side Effects: Send a notice and/or memo to the target user, if possible.
 */
void notify_channel_acl_change(sourceinfo_t *si, myuser_t *tmu, mychan_t *mc,
	const char *flagstr, unsigned int flags)
{
	char text[256], text1[256], text2[300];
	chanacs_t *ca;
	mowgli_node_t *m;

	if (flags == 0)
	{
		snprintf(text, sizeof text, "\2%s\2 has set \2%s\2 and removed \2%s\2 from: \2%s\2",
			entity(si->smu)->name, flagstr, entity(tmu)->name, mc->name);
		snprintf(text1, sizeof text1, "\2%s\2 set \2%s\2, removing: \2%s\2",
			entity(si->smu)->name, flagstr, entity(tmu)->name, mc->name);
	}
	else if (get_template_name(mc, flags))
	{
		snprintf(text, sizeof text, "\2%s\2 has set \2%s\2 on \2%s\2 in \2%s\2 who now has the flags: \2%s\2 (TEMPLATE: \2%s\2)",
			entity(si->smu)->name, flagstr,
			entity(tmu)->name, mc->name,
			bitmask_to_flags(flags),
			get_template_name(mc, flags));
		snprintf(text1, sizeof text1, "\2%s\2 set \2%s\2 on \2%s\2 who now has flags: \2%s\2 (TEMPLATE: \2%s\2)",
			entity(si->smu)->name, flagstr,
			entity(tmu)->name, bitmask_to_flags(flags),
			get_template_name(mc, flags));
	}
	else
	{
		snprintf(text, sizeof text, "\2%s\2 has set \2%s\2 on \2%s\2 in \2%s\2 who now has the flags: \2%s\2",
			entity(si->smu)->name, flagstr,
			entity(tmu)->name, mc->name,
			bitmask_to_flags(flags));
		snprintf(text1, sizeof text1, "\2%s\2 has set \2%s\2 on \2%s\2 who now has the flags: \2%s\2",
			entity(si->smu)->name, flagstr,
			entity(tmu)->name, bitmask_to_flags(flags));
	}

	snprintf(text2, sizeof text2, "[automatic memo from \2%s\2] - %s", chansvs.nick, text);

	add_history_entry = module_locate_symbol("chanserv/history", "add_history_entry");

	if (add_history_entry != NULL)
		add_history_entry(si, mc, text1);

	MOWGLI_ITER_FOREACH(m, mc->chanacs.head)
	{
		ca = m->data;

		/* Don't run this for the target, if the target has MU_NOTIFYACL enabled, as this
		 * should be covered under notify_target_acl_change.  Prevents double notices. */
		if (ca->entity == entity(tmu) && tmu->flags & MU_NOTIFYACL)
			continue;

		/* Skip if the entity isn't a NickServ account as this would not make sense.
		 * ToDo: Make it work for group memos?
		 */
		if (!isuser(ca->entity))
			continue;

		/* Check for +F, +R, and/or +s.  If not, skip.
		 * ToDo: Have this as default, have channel configuration setting? */
		if (!(ca->level & CA_FOUNDER || ca->level & CA_RECOVER || ca->level & CA_SET))
			continue;

		/* Send the notice to everyone but the source. */
		if (si->smu != user(ca->entity) && user(ca->entity)->flags & MU_NOTIFYACL)
			myuser_notice(chansvs.nick, user(ca->entity), text);

		if (!(user(ca->entity)->flags & MU_NOMEMO) && user(ca->entity)->flags & MU_NOTIFYMEMO && user(ca->entity)->flags & MU_NOTIFYACL &&
			(send_user_memo = module_locate_symbol("memoserv/main", "send_user_memo")) != NULL)
			send_user_memo(si, user(ca->entity), text2, false, MEMO_CHANNEL, user(ca->entity)->flags & MU_EMAILNOTIFY);
	}
}

/* notify_channel_set_change: Notifes the target channel management about
 *     the new settings in a channel in the form of a memo (if possible).
 *     Additionally, if chanserv/history is loaded, add another history entry.
 *     Additionally, send a private notice to every member of channel management
 *     with +F, +R, and/or +s who is online and has NOTIFY_ACL enabled.
 *
 * Inputs: myuser_t *smu - Source user
 *         myuser_t *tmu - Target user
 *         mychan_t *mc - Channel where the change is taking place.
 *         const char *settingname - Name of the setting changed.
 *         const char *setting - Actual setting.. usually "ON" or "OFF" but some settings have other options.
 * Outputs: None
 * Side Effects: Send a notice and/or memo to the target user, if possible.
 */
void notify_channel_set_change(sourceinfo_t *si, myuser_t *tmu, mychan_t *mc,
	const char *settingname, const char *setting)
{
	char text[256], text1[256], text2[300];
	chanacs_t *ca;
	mowgli_node_t *m;

	snprintf(text, sizeof text, "\2%s\2 has set \2%s\2 on channel \2%s\2 to: \2%s\2",
		entity(tmu)->name, settingname, mc->name, setting);

	snprintf(text1, sizeof text1, "\2%s\2 setting changed to: \2%s\2", settingname, setting);

	snprintf(text2, sizeof text2, "[automatic memo from \2%s\2] - %s", chansvs.nick, text);

	add_history_entry = module_locate_symbol("chanserv/history", "add_history_entry");

	if (add_history_entry != NULL)
		add_history_entry(si, mc, text1);

	MOWGLI_ITER_FOREACH(m, mc->chanacs.head)
	{
		ca = m->data;

		/* Skip if the entity isn't a NickServ account.  ToDo: Make it work for group memos? */
		if (!isuser(ca->entity))
			continue;

		/* Don't send a duplicate notification to the source user. */
		//if (si->smu == tmu)
			//continue;

		/* Check for +F, +R, and/or +s.  If not, skip.
		 * ToDo: Have this as default, have channel configuration setting? */
		if (!(ca->level & CA_FOUNDER || ca->level & CA_RECOVER || ca->level & CA_SET))
			continue;

		/* Send the notice to everyone but the source. */
		if (si->smu != user(ca->entity) && user(ca->entity)->flags & MU_NOTIFYACL)
			myuser_notice(chansvs.nick, user(ca->entity), text);

		if (!(user(ca->entity)->flags & MU_NOMEMO) && user(ca->entity)->flags & MU_NOTIFYMEMO && user(ca->entity)->flags & MU_NOTIFYACL &&
			(send_user_memo = module_locate_symbol("memoserv/main", "send_user_memo")) != NULL)
			send_user_memo(si, user(ca->entity), text2, false, MEMO_CHANNEL, user(ca->entity)->flags & MU_EMAILNOTIFY);
	}
}

/* notify_channel_successor_change: Notifies the target channel management about
 *     a channel succession event and send a memo to each member (if possible).
 *     Additionally, if chanserv/history is loaded, add another history entry.
 *     Additionally, send a private notice to every member of channel management
 *     with +F, +R, and/or +s who is online and has NOTIFY_ACL enabled.
 *
 * Inputs: myuser_t *smu - Source user
 *         myuser_t *tmu - Target user
 *         mychan_t *mc - Channel where the change is taking place.
 */

void notify_channel_successor_change(myuser_t *smu, myuser_t *tmu, mychan_t *mc)
{
	char text[256], text1[256], text2[300];
	chanacs_t *ca;
	mowgli_node_t *m;

	snprintf(text, sizeof text, "Succession - Old Founder: \2%s\2 New Founder: \2%s\2",
		entity(smu)->name, entity(tmu)->name);

	snprintf(text1, sizeof text1, "Channel succession of \2%s\2 - Old Founder: \2%s\2 New Founder: \2%s\2",
		mc->name, entity(smu)->name, entity(tmu)->name);

	snprintf(text2, sizeof text2, "[automatic memo from \2%s\2] - %s",
		chansvs.nick, text1);

	add_history_entry_misc = module_locate_symbol("chanserv/history", "add_history_entry_misc");

	if (add_history_entry_misc != NULL)
		add_history_entry_misc(mc, text1);

	MOWGLI_ITER_FOREACH(m, mc->chanacs.head)
	{
		ca = m->data;

		/* Skip if the entity isn't a NickServ account.  ToDo: Make it work for group memos? */
		if (!isuser(ca->entity))
			continue;

		/* Don't send any notifications to the source user. */
		if (smu == user(ca->entity))
			continue;

		/* Check for +F, +R, and/or +s.  If not, skip.
		 * ToDo: Have this as default, have channel configuration setting? */
		if (!(ca->level & CA_FOUNDER || ca->level & CA_RECOVER || ca->level & CA_SET))
			continue;

		/* Send the notice to everyone but the target
		 * (already handled in account.c:myuser_delete).
		 */
		if (tmu != user(ca->entity) && user(ca->entity)->flags & MU_NOTIFYACL)
			myuser_notice(chansvs.nick, user(ca->entity), text);

		/* Send a memo to everyone with NOTIFY_ACL and NOTIFY_MEMO enabled, including the target. */
		if (!(user(ca->entity)->flags & MU_NOMEMO) && user(ca->entity)->flags & MU_NOTIFYMEMO && user(ca->entity)->flags & MU_NOTIFYACL &&
			(send_user_memo_misc = module_locate_symbol("memoserv/main", "send_user_memo_misc")) != NULL)
			send_user_memo_misc(user(ca->entity), text2, MEMO_CHANNEL, user(ca->entity)->flags & MU_EMAILNOTIFY);
	}
}

/* notify_channel_misc_change: Notifies the target channel management about
 *     a miscellaneous event and send a memo to each member (if possible).
 *     Additionally, if chanserv/history is loaded, add another history entry.
 *     Additionally, send a private notice to every member of channel management
 *     with +F, +R, and/or +s who is online and has NOTIFY_ACL enabled.
 *
 * Inputs: sourceinfo_t *si - Source user
 *         mychan_t *mc - Channel where the change is taking place.
 *         const char *desc - Free form description of the event.
 */

void notify_channel_misc_change(sourceinfo_t *si, mychan_t *mc, const char *desc)
{
	char text[256], text2[300];
	chanacs_t *ca;
	mowgli_node_t *m;

	snprintf(text, sizeof text, "Channel Event on \2%s\2 - %s", mc->name, desc);

	snprintf(text2, sizeof text2, "[automatic memo from \2%s\2] - %s", chansvs.nick, text);

	add_history_entry = module_locate_symbol("chanserv/history", "add_history_entry");

	if (add_history_entry != NULL)
		add_history_entry(si, mc, desc);

	MOWGLI_ITER_FOREACH(m, mc->chanacs.head)
	{
		ca = m->data;

		/* Skip if the entity isn't a NickServ account.  ToDo: Make it work for group memos? */
		if (!isuser(ca->entity))
			continue;

		/* Check for +F, +R, and/or +s.  If not, skip.
		 * ToDo: Have this as default, have channel configuration setting? */
		if (!(ca->level & CA_FOUNDER || ca->level & CA_RECOVER || ca->level & CA_SET))
			continue;

		/* Send the notice to everyone but the source. */
		if (si->smu != user(ca->entity) && user(ca->entity)->flags & MU_NOTIFYACL)
			myuser_notice(chansvs.nick, user(ca->entity), text);

		if (!(user(ca->entity)->flags & MU_NOMEMO) && user(ca->entity)->flags & MU_NOTIFYMEMO && user(ca->entity)->flags & MU_NOTIFYACL &&
			(send_user_memo = module_locate_symbol("memoserv/main", "send_user_memo")) != NULL)
			send_user_memo(si, user(ca->entity), text2, false, MEMO_CHANNEL, user(ca->entity)->flags & MU_EMAILNOTIFY);
	}
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
