/*
 * Copyright (c) 2005-2006 William Pitcock, et al.
 * Copyright (c) 2016-2017 ChatLounge IRC Network Development Team
 *
 * Rights to this code are as documented in doc/LICENSE.
 *
 * This file contains code for the CService FFLAGS functions.
 *
 */

#include "atheme.h"
#include "template.h"

DECLARE_MODULE_V1
(
	"chanserv/fflags", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	VENDOR_STRING
);

void (*notify_channel_acl_change)(sourceinfo_t *si, myuser_t *tmu, mychan_t *mc,
	const char *flagstr, unsigned int flags) = NULL;
void (*notify_target_acl_change)(sourceinfo_t *si, myuser_t *tmu, mychan_t *mc,
	const char *flagstr, unsigned int flags) = NULL;

static void cs_cmd_fflags(sourceinfo_t *si, int parc, char *parv[]);

command_t cs_fflags = { "FFLAGS", N_("Forces a flags change on a channel."),
                        PRIV_CHAN_ADMIN, 3, cs_cmd_fflags, { .path = "cservice/fflags" } };

void _modinit(module_t *m)
{
	service_named_bind_command("chanserv", &cs_fflags);

	if (module_request("chanserv/main"))
	{
		notify_channel_acl_change = module_locate_symbol("chanserv/main", "notify_channel_acl_change");
		notify_target_acl_change = module_locate_symbol("chanserv/main", "notify_target_acl_change");
	}
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("chanserv", &cs_fflags);
}

/* FFLAGS <channel> <user> <flags> */
static void cs_cmd_fflags(sourceinfo_t *si, int parc, char *parv[])
{
	const char *channel = parv[0];
	const char *target = parv[1];
	const char *flagstr = parv[2];
	mychan_t *mc;
	myentity_t *mt;
	unsigned int addflags, removeflags;
	unsigned int newlevel;
	chanacs_t *ca;
	hook_channel_acl_req_t req;

	if (parc < 3)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "FFLAGS");
		command_fail(si, fault_needmoreparams, _("Syntax: FFLAGS <channel> <target> <flags>"));
		return;
	}

	mc = mychan_find(channel);
	if (!mc)
	{
		command_fail(si, fault_nosuch_target, _("Channel \2%s\2 is not registered."), channel);
		return;
	}

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
				command_fail(si, fault_badparams, _("Usage: FFLAGS %s <target> <flags>"), mc->name);
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
		target = mt->name;

		ca = chanacs_open(mc, mt, NULL, true, entity(si->smu));

		/* XXX this should be more like flags.c */
		if (removeflags & CA_FLAGS)
			removeflags |= CA_FOUNDER, addflags &= ~CA_FOUNDER;
		else if (addflags & CA_FOUNDER)
		{
			if (!myentity_allow_foundership(mt))
			{
				command_fail(si, fault_toomany, _("\2%s\2 cannot take foundership of a channel."), mt->name);
				chanacs_close(ca);
				return;
			}
			addflags |= CA_FLAGS, removeflags &= ~CA_FLAGS;
		}

		if (is_founder(mc, mt) && removeflags & CA_FOUNDER && mychan_num_founders(mc) == 1)
		{
			command_fail(si, fault_noprivs, _("You may not remove the last founder."));
			return;
		}

		req.ca = ca;
		req.oldlevel = ca->level;

		if (!chanacs_modify(ca, &addflags, &removeflags, ca_all, si->smu))
		{
			/* this shouldn't happen */
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
		if (addflags & CA_FOUNDER)
		{
			command_fail(si, fault_badparams, _("You may not set founder status on a hostmask."));
			return;
		}

		ca = chanacs_open(mc, NULL, target, true, entity(si->smu));

		req.ca = ca;
		req.oldlevel = ca->level;

		if (!chanacs_modify(ca, &addflags, &removeflags, ca_all, si->smu))
		{
			/* this shouldn't happen */
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
	flagstr = bitmask_to_flags2(addflags, removeflags);
	wallops("\2%s\2 is forcing flags change \2%s\2 on \2%s\2 in: \2%s\2", get_oper_name(si), flagstr, target, mc->name);
	command_success_nodata(si, _("Flags \2%s\2 were set on \2%s\2 in: \2%s\2"), flagstr, target, channel);
	logcommand(si, CMDLOG_ADMIN, "FFLAGS: \2%s\2 \2%s\2 \2%s\2", mc->name, target, flagstr);
	verbose(mc, _("\2%s\2 forced flags change \2%s\2 on: \2%s\2"), get_source_name(si), flagstr, target);

	if (isuser(mt))
	{
		myuser_t *tmu = myuser_find(target);

		char flagstr2[54]; // 26 characters, * 2 for upper and lower case, then add a potential plus and minus sigh. -> 54
		mowgli_strlcpy(flagstr2, flagstr, 54);
		notify_target_acl_change(si, tmu, mc, flagstr2, newlevel);
		notify_channel_acl_change(si, tmu, mc, flagstr2, newlevel);
	}
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
