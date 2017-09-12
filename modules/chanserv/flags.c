/*
 * Copyright (c) 2005-2007 William Pitcock, et al.
 * Copyright (c) 2016-2017 ChatLounge IRC Network Development Team
 *
 * Rights to this code are as documented in doc/LICENSE.
 *
 * This file contains code for the CService FLAGS functions.
 *
 */

#include "atheme.h"
#include "template.h"

DECLARE_MODULE_V1
(
	"chanserv/flags", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net>"
);

void (*notify_channel_acl_change)(sourceinfo_t *si, myuser_t *tmu, mychan_t *mc,
	const char *flagstr, unsigned int flags) = NULL;
void (*notify_target_acl_change)(sourceinfo_t *si, myuser_t *tmu, mychan_t *mc,
	const char *flagstr, unsigned int flags) = NULL;

static void cs_cmd_flags(sourceinfo_t *si, int parc, char *parv[]);

command_t cs_flags = { "FLAGS", N_("Manipulates specific permissions on a channel."),
                        AC_NONE, 3, cs_cmd_flags, { .path = "cservice/flags" } };

void _modinit(module_t *m)
{
	service_named_bind_command("chanserv", &cs_flags);

	if (module_request("chanserv/main"))
	{
		notify_channel_acl_change = module_locate_symbol("chanserv/main", "notify_channel_acl_change");
		notify_target_acl_change = module_locate_symbol("chanserv/main", "notify_target_acl_change");
	}
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("chanserv", &cs_flags);
}

static void do_list(sourceinfo_t *si, mychan_t *mc, unsigned int flags)
{
	chanacs_t *ca;
	mowgli_node_t *m, *n;
	unsigned int entrywidth = 5, nickhostwidth = 13, flagswidth = 5; /* "Nickname/Host" is 13 chars long, "Flags" is 5. */
	bool operoverride = false;
	unsigned int i = 0;
	char fmtstring[BUFSIZE], entryspacing[BUFSIZE], entryborder[BUFSIZE], nickhostspacing[BUFSIZE], nickhostborder[BUFSIZE], flagsborder[BUFSIZE];

	if (!(mc->flags & MC_PUBACL) && !chanacs_source_has_flag(mc, si, CA_ACLVIEW))
	{
		if (has_priv(si, PRIV_CHAN_AUSPEX))
			operoverride = true;
		else
		{
			command_fail(si, fault_noprivs, _("You are not authorized to perform this operation."));
			return;
		}
	}

	/* Set entrywidth, flagswidth, and nickhostwidth to the length of the
	 * longest entries.
	 * - Ben
	 */
	MOWGLI_ITER_FOREACH(m, mc->chanacs.head)
	{
		ca = m->data;

		if (strlen(ca->entity ? ca->entity->name : ca->host) > nickhostwidth)
			nickhostwidth = strlen(ca->entity ? ca->entity->name : ca->host);

		if (strlen(bitmask_to_flags(ca->level)) > flagswidth)
			flagswidth = strlen(bitmask_to_flags(ca->level));

		i++;
	}

	while (i != 0)
	{
		i =  i/10;
		if (i > 5)
			entrywidth++;
	}

	mowgli_strlcpy(entryspacing, " ", BUFSIZE);
	mowgli_strlcpy(entryborder, "-", BUFSIZE);
	mowgli_strlcpy(nickhostspacing, " ", BUFSIZE);
	mowgli_strlcpy(nickhostborder, "-", BUFSIZE);
	mowgli_strlcpy(flagsborder, "-", BUFSIZE);

	i = 1;

	for (i; i < entrywidth; i++)
	{
		mowgli_strlcat(entryborder, "-", BUFSIZE);
		if (i > 4)
			mowgli_strlcat(entryspacing, " ", BUFSIZE);
	}

	i = 1;

	for (i; i < nickhostwidth; i++)
	{
		mowgli_strlcat(nickhostborder, "-", BUFSIZE);
		if (i > 12)
			mowgli_strlcat(nickhostspacing, " ", BUFSIZE);
	}

	i = 1;

	for (i; i < flagswidth; i++)
	{
		mowgli_strlcat(flagsborder, "-", BUFSIZE);
	}

	command_success_nodata(si, _("FLAGS list for: \2%s\2"), mc->name);

	command_success_nodata(si, _("Entry%sNickname/Host%sFlags"), entryspacing, nickhostspacing);
	command_success_nodata(si, "%s %s %s", entryborder, nickhostborder, flagsborder);

	i = 1;

	/* Make dynamic format string. */
	snprintf(fmtstring, BUFSIZE, "%%%ud %%-%us %%-%us (%%s) (%%s) [modified %%s ago, on %%s]",
		entrywidth, nickhostwidth, flagswidth);

	MOWGLI_ITER_FOREACH(n, mc->chanacs.head)
	{
		const char *template, *mod_ago;
		struct tm tm;
		char mod_date[64];

		ca = n->data;

		if (flags && !(ca->level & flags))
			continue;

		template = get_template_name(mc, ca->level);
		mod_ago = ca->tmodified ? time_ago(ca->tmodified) : "?";

		tm = *localtime(&ca->tmodified);
		strftime(mod_date, sizeof mod_date, TIME_FORMAT, &tm);

		command_success_nodata(si, _(fmtstring),
			i, ca->entity ? ca->entity->name : ca->host, bitmask_to_flags(ca->level),
			template == NULL ? "<Custom>" : template, mc->name, mod_ago, mod_date);
		i++;
	}

	command_success_nodata(si, "%s %s %s", entryborder, nickhostborder, flagsborder);
	command_success_nodata(si, _("End of \2%s\2 FLAGS listing.  Total of %u %s."),
		mc->name, i - 1, i - 1 == 1 ? "entry" : "entries");

	if (operoverride)
		logcommand(si, CMDLOG_ADMIN, "FLAGS: \2%s\2 (oper override)", mc->name);
	else
		logcommand(si, CMDLOG_GET, "FLAGS: \2%s\2", mc->name);
}

/* Check if a user is attempting to change his own flags.
 *
 * Inputs:
 *   char *target - character array.
 *   myuser_t *mu - User to check against.
 * Returns:
 *   true if yes, false if not.
 * Side Effects:
 *   None
 */
static bool check_self_match(const char *target, myuser_t *mu)
{
	mowgli_node_t *n;

	if (mu == NULL)
		return false;

	MOWGLI_ITER_FOREACH(n, mu->nicks.head)
	{
		if (!irccasecmp(target, ((mynick_t *)(n->data))->nick))
			return true;
	}

	return false;
}

/* FLAGS <channel> [user] [flags] */
static void cs_cmd_flags(sourceinfo_t *si, int parc, char *parv[])
{
	chanacs_t *ca;
	mowgli_node_t *n;
	char *channel = parv[0];
	char *target = sstrdup(parv[1]);
	char *flagstr = parv[2];
	const char *oldtemplate = NULL;
	const char *newtemplate = NULL;
	const char *str1;
	unsigned int addflags, removeflags, restrictflags;
	unsigned int oldlevel, newlevel;
	bool selfautomode = false;
	hook_channel_acl_req_t req;
	mychan_t *mc;

	if (parc < 1)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "FLAGS");
		command_fail(si, fault_needmoreparams, _("Syntax: FLAGS <channel> [target] [flags]"));
		return;
	}

	mc = mychan_find(channel);
	if (!mc)
	{
		command_fail(si, fault_nosuch_target, _("Channel \2%s\2 is not registered."), channel);
		return;
	}

	if (metadata_find(mc, "private:close:closer") && (target || !has_priv(si, PRIV_CHAN_AUSPEX)))
	{
		command_fail(si, fault_noprivs, _("\2%s\2 is closed."), channel);
		return;
	}

	if (!target || (target && target[0] == '+' && flagstr == NULL))
	{
		unsigned int flags = (target != NULL) ? flags_to_bitmask(target, 0) : 0;

		do_list(si, mc, flags);
		return;
	}

	/*
	 * There used to be Anope compatibility here, adding support for
	 * "/msg ChanServ flags #channel modify/clear/list", 
	 * but this has caused issues described in the following bug report:
	 * https://github.com/shalture/shalture/issues/16
	 * ...so it was removed again. ~ToBeFree
	 */

	myentity_t *mt;

	if (!si->smu)
	{
		command_fail(si, fault_noprivs, _("You are not logged in."));
		return;
	}

	if (!flagstr)
	{
		if (!(mc->flags & MC_PUBACL) && !chanacs_source_has_flag(mc, si, CA_ACLVIEW))
		{
			command_fail(si, fault_noprivs, _("You are not authorized to execute this command."));
			return;
		}
		if (validhostmask(target))
			ca = chanacs_find_host_literal(mc, target, 0);
		else
		{
			if (!(mt = myentity_find_ext(target)))
			{
				command_fail(si, fault_nosuch_target, _("\2%s\2 is not registered."), target);
				return;
			}
			free(target);
			target = sstrdup(mt->name);
			ca = chanacs_find_literal(mc, mt, 0);
		}
		if (ca != NULL)
		{
			str1 = bitmask_to_flags2(ca->level, 0);
			command_success_string(si, str1, _("Flags for \2%s\2 in \2%s\2 are \2%s\2."),
					target, channel,
					str1);
		}
		else
			command_success_string(si, "", _("No flags for \2%s\2 in \2%s\2."),
					target, channel);
		logcommand(si, CMDLOG_GET, "FLAGS: \2%s\2 on \2%s\2", mc->name, target);
		return;
	}

	/* founder may always set flags -- jilles */
	restrictflags = chanacs_source_flags(mc, si);
	if (restrictflags & CA_FOUNDER)
		restrictflags = ca_all;
	else
	{
		if (!(restrictflags & CA_FLAGS))
		{
			if (chansvs.permit_self_autoop && check_self_match(target, si->smu))
			{
				/* Allow self autoop if +o, self autohalfop if +h,
				 * and self autovoice if +v.  Admin and Owner aren't
				 * here because they are controlled with +O like Op. */
				if (si->smu != NULL &&
					!(restrictflags & CA_AKICK && !(restrictflags & CA_EXEMPT) && !(restrictflags & CA_REMOVE)) && /* Not on AKICK, or is but with an exemption. */
					//!irccasecmp(target, entity(si->smu)->name) &&
					restrictflags & CA_OP) // &&
					//(strcmp(flagstr, "+O") || strcmp(flagstr, "-O")))
				{
					selfautomode = true;
					goto selfautomode;
				}

				if (ircd->uses_halfops && si->smu != NULL &&
					!(restrictflags & CA_AKICK && !(restrictflags & CA_EXEMPT) && !(restrictflags & CA_REMOVE)) && /* Not on AKICK, or is but with an exemption. */
					//!irccasecmp(target, entity(si->smu)->name) &&
					restrictflags & CA_HALFOP) // &&
					//(strcmp(flagstr, "+H") || strcmp(flagstr, "-H")))
				{
					selfautomode = true;
					goto selfautomode;
				}

				if (si->smu != NULL &&
					!(restrictflags & CA_AKICK && !(restrictflags & CA_EXEMPT) && !(restrictflags & CA_REMOVE)) && /* Not on AKICK, or is but with an exemption. */
					//!irccasecmp(target, entity(si->smu)->name) &&
					restrictflags & CA_VOICE) // &&
					//(strcmp(flagstr, "+V") || strcmp(flagstr, "-V")))
				{
					selfautomode = true;
					goto selfautomode;
				}
			}

			/* allow a user to remove their own access
			 * even without +f */
			if (restrictflags & CA_AKICK ||
					si->smu == NULL ||
					!check_self_match(target, si->smu) ||
					strcmp(flagstr, "-*"))
			{
				command_fail(si, fault_noprivs, _("You are not authorized to execute this command."));
				return;
			}
		}
		if (!check_self_match(target, si->smu))
			restrictflags = allow_flags(mc, restrictflags);
		else
			restrictflags |= allow_flags(mc, restrictflags);
	}

selfautomode:
	if (*flagstr == '+' || *flagstr == '-' || *flagstr == '=')
	{
		flags_make_bitmasks(flagstr, &addflags, &removeflags);
		if (addflags == 0 && removeflags == 0)
		{
			command_fail(si, fault_badparams, _("No valid flags given, use /%s%s HELP FLAGS for a list"), ircd->uses_rcommand ? "" : "msg ", chansvs.me->disp);
			return;
		}
	}
	else
	{
		addflags = get_template_flags(mc, flagstr);
		if (addflags == 0)
		{
			/* Hack -- jilles */
			if (*target == '+' || *target == '-' || *target == '=')
				command_fail(si, fault_badparams, _("Usage: FLAGS %s [target] [flags]"), mc->name);
			else
				command_fail(si, fault_badparams, _("Invalid template name given, use /%s%s TEMPLATE %s for a list"), ircd->uses_rcommand ? "" : "msg ", chansvs.me->disp, mc->name);
			return;
		}
		removeflags = ca_all & ~addflags;
	}

	if (!validhostmask(target))
	{
		if (!(mt = myentity_find_ext(target)))
		{
			command_fail(si, fault_nosuch_target, _("\2%s\2 is not registered."), target);
			return;
		}
		free(target);
		target = sstrdup(mt->name);

		ca = chanacs_open(mc, mt, NULL, true, entity(si->smu));

		if (ca->level & CA_FOUNDER && removeflags & CA_FLAGS && !(removeflags & CA_FOUNDER))
		{
			command_fail(si, fault_noprivs, _("You may not remove a founder's +f access."));
			return;
		}
		if (ca->level & CA_FOUNDER && removeflags & CA_FOUNDER && mychan_num_founders(mc) == 1)
		{
			command_fail(si, fault_noprivs, _("You may not remove the last founder."));
			return;
		}
		if (!(ca->level & CA_FOUNDER) && addflags & CA_FOUNDER)
		{
			if (mychan_num_founders(mc) >= chansvs.maxfounders)
			{
				/* One is not plural. */
				if (chansvs.maxfounders == 1) {
					command_fail(si, fault_noprivs, _("Only 1 founder allowed per channel."));
				}
				else {
					command_fail(si, fault_noprivs, _("Only %d founders allowed per channel."), chansvs.maxfounders);
				}
				chanacs_close(ca);
				return;
			}
			if (!myentity_can_register_channel(mt))
			{
				command_fail(si, fault_toomany, _("\2%s\2 has too many channels registered."), mt->name);
				chanacs_close(ca);
				return;
			}
		}
		if (addflags & CA_FOUNDER)
			addflags |= CA_FLAGS, removeflags &= ~CA_FLAGS;

		/* If NEVEROP is set, don't allow adding new entries
		 * except sole +b. Adding flags if the current level
		 * is +b counts as adding an entry.
		 * -- jilles */
		/* XXX: not all entities are users */
		if (isuser(mt) && (MU_NEVEROP & user(mt)->flags && addflags != CA_AKICK && addflags != 0 && (ca->level == 0 || ca->level == CA_AKICK)))
		{
			command_fail(si, fault_noprivs, _("\2%s\2 does not wish to be added to channel access lists (NEVEROP set)."), mt->name);
			chanacs_close(ca);
			return;
		}

		if (ca->level == 0 && chanacs_is_table_full(ca))
		{
			command_fail(si, fault_toomany, _("Channel %s access list is full."), mc->name);
			chanacs_close(ca);
			return;
		}

		req.ca = ca;
		req.oldlevel = ca->level;
		oldlevel = ca->level;

		if (selfautomode)
		{
			if (ca->level & CA_OP && (addflags & CA_AUTOOP || removeflags & CA_AUTOOP))
				restrictflags |= CA_AUTOOP;
			if (ca->level & CA_HALFOP && (addflags & CA_AUTOHALFOP || removeflags & CA_AUTOHALFOP))
				restrictflags |= CA_AUTOHALFOP;
			if (ca->level & CA_VOICE && (addflags & CA_AUTOVOICE || removeflags & CA_AUTOVOICE))
				restrictflags |= CA_AUTOVOICE;
		}

		if (!chanacs_modify(ca, &addflags, &removeflags, restrictflags))
		{
			command_fail(si, fault_noprivs, _("You are not allowed to set \2%s\2 on \2%s\2 in \2%s\2."), bitmask_to_flags2(addflags, removeflags), mt->name, mc->name);
			chanacs_close(ca);
			return;
		}

		req.newlevel = ca->level;
		newlevel = ca->level;

		hook_call_channel_acl_change(&req);
		chanacs_close(ca);
	}
	else
	{
		if (!target)
		{
			command_fail(si, fault_needmoreparams, _("Invalid hostmask."));
			return;
		}

		if (chansvs.min_non_wildcard_chars_host_acl > 0 &&
			check_not_enough_non_wildcard_chars(target, (int) chansvs.min_non_wildcard_chars_host_acl, 1) &&
			(strchr(target, '*') || strchr(target, '?')) &&
			!has_priv(si, PRIV_AKILL_ANYMASK))
		{
			command_fail(si, fault_badparams, _("Invalid nick!user@host: \2%s\2  At least %u non-wildcard characters are required."), target, chansvs.min_non_wildcard_chars_host_acl);
			return;
		}

		if (addflags & CA_FOUNDER)
		{
			command_fail(si, fault_badparams, _("You may not set founder status on a hostmask."));
			return;
		}

		if (addflags & chansvs.flags_req_acct)
		{
			command_fail(si, fault_badparams, _("You may not add any of the following flags to hostmasks: %s"),
				bitmask_to_flags(chansvs.flags_req_acct));
			return;
		}

		ca = chanacs_open(mc, NULL, target, true, entity(si->smu));
		if (ca->level == 0 && chanacs_is_table_full(ca))
		{
			command_fail(si, fault_toomany, _("Channel %s access list is full."), mc->name);
			chanacs_close(ca);
			return;
		}

		req.ca = ca;
		req.oldlevel = ca->level;
		oldlevel = ca->level;

		if (!chanacs_modify(ca, &addflags, &removeflags, restrictflags))
		{
					command_fail(si, fault_noprivs, _("You are not allowed to set \2%s\2 on \2%s\2 in \2%s\2."), bitmask_to_flags2(addflags, removeflags), target, mc->name);
			chanacs_close(ca);
			return;
		}

		req.newlevel = ca->level;
		newlevel = ca->level;

		hook_call_channel_acl_change(&req);
		chanacs_close(ca);
	}

	if ((addflags | removeflags) == 0)
	{
		command_fail(si, fault_nochange, _("Channel access to \2%s\2 for \2%s\2 unchanged."), channel, target);
		return;
	}

	if (oldlevel != 0)
		oldtemplate = get_template_name(mc, oldlevel);

	if (newlevel != 0)
		newtemplate = get_template_name(mc, newlevel);

	flagstr = bitmask_to_flags2(addflags, removeflags);
	command_success_nodata(si, _("Flags \2%s\2 (Old template: \2%s\2 New template: \2%s\2) were set on \2%s\2 in \2%s\2."),
		flagstr,
		oldtemplate == NULL ? "<No template>" : oldtemplate,
		newtemplate == NULL ? "<No template>" : newtemplate,
		target, channel);
	logcommand(si, CMDLOG_SET, "FLAGS: \2%s\2 \2%s\2 \2%s\2 (Old template: \2%s\2 New template: \2%s\2)",
		mc->name, target, flagstr,
		oldtemplate == NULL ? "<No template>" : oldtemplate,
		newtemplate == NULL ? "<No template>" : newtemplate);

	if (oldtemplate == NULL)
	{
		if (newtemplate == NULL)
			verbose(mc, "\2%s\2 set flags \2%s\2 on: \2%s\2", get_source_name(si), flagstr, target);
		else
			verbose(mc, "\2%s\2 set flags \2%s\2 (New template: \2%s\2) on: \2%s\2",
				get_source_name(si), flagstr, newtemplate, target);
	}
	else
	{
		if (newtemplate == NULL)
			verbose(mc, "\2%s\2 set flags \2%s\2 (Old template: \2%s\2) on: \2%s\2",
				get_source_name(si), flagstr, oldtemplate, target);
		else
			verbose(mc, "\2%s\2 set flags \2%s\2 (Old template: \2%s\2 New template: \2%s\2) on: \2%s\2",
				get_source_name(si), flagstr, oldtemplate, newtemplate, target);
	}

	if (isuser(mt))
	{
		myuser_t *tmu = myuser_find(target);

		char flagstr2[54]; // 26 characters, * 2 for upper and lower case, then add a potential plus and minus sigh. -> 54
		mowgli_strlcpy(flagstr2, flagstr, 54);
		notify_target_acl_change(si, tmu, mc, flagstr2, newlevel);
		notify_channel_acl_change(si, tmu, mc, flagstr2, newlevel);
	}

	free(target);
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
