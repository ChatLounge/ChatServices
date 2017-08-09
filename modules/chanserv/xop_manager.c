/*
 * Copyright (c) 2015-2017 ChatLounge IRC Network Development Team
 * Copyright (c) 2005-2007 Atheme Development Group
 * Rights to this code are as documented in doc/LICENSE.
 *
 * This file contains code for the CService XOP and MANAGER functions.
 * Most useful if many users are migrating from DALnet.
 *
 * Based off of chanserv/xop.c
 *
 */

#include "atheme.h"
#include "template.h"
#include "chanserv.h"

DECLARE_MODULE_V1
(
	"chanserv/xop_manager", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net>"
);

void (*notify_channel_acl_change)(sourceinfo_t *si, myuser_t *tmu, mychan_t *mc,
	const char *flagstr, unsigned int flags) = NULL;
void (*notify_target_acl_change)(sourceinfo_t *si, myuser_t *tmu, mychan_t *mc,
	const char *flagstr, unsigned int flags) = NULL;

/* the individual command stuff, now that we've reworked, hardcode ;) --w00t */
static void cs_xop_do_add(sourceinfo_t *si, mychan_t *mc, myentity_t *mt, char *target, unsigned int level, const char *leveldesc, unsigned int restrictflags);
static void cs_xop_do_del(sourceinfo_t *si, mychan_t *mc, myentity_t *mt, char *target, unsigned int level, const char *leveldesc);
static void cs_xop_do_list(sourceinfo_t *si, mychan_t *mc, unsigned int level, const char *leveldesc, bool operoverride);

static void cs_cmd_manager(sourceinfo_t *si, int parc, char *parv[]);
static void cs_cmd_sop(sourceinfo_t *si, int parc, char *parv[]);
static void cs_cmd_aop(sourceinfo_t *si, int parc, char *parv[]);
static void cs_cmd_hop(sourceinfo_t *si, int parc, char *parv[]);
static void cs_cmd_vop(sourceinfo_t *si, int parc, char *parv[]);
static void cs_cmd_forcexop(sourceinfo_t *si, int parc, char *parv[]);

command_t cs_manager = { "MANAGER", N_("Manipulates a channel MANAGER list."),
						AC_NONE, 3, cs_cmd_manager, { .path = "cservice/xop"} };
command_t cs_sop = { "SOP", N_("Manipulates a channel SOP list."),
                        AC_NONE, 3, cs_cmd_sop, { .path = "cservice/xop" } };
command_t cs_aop = { "AOP", N_("Manipulates a channel AOP list."),
                        AC_NONE, 3, cs_cmd_aop, { .path = "cservice/xop" } };
command_t cs_hop = { "HOP", N_("Manipulates a channel HOP list."),
			AC_NONE, 3, cs_cmd_hop, { .path = "cservice/xop" } };
command_t cs_vop = { "VOP", N_("Manipulates a channel VOP list."),
                        AC_NONE, 3, cs_cmd_vop, { .path = "cservice/xop" } };
command_t cs_forcexop = { "FORCEXOP", N_("Forces access levels to xOP levels."),
                         AC_NONE, 1, cs_cmd_forcexop, { .path = "cservice/forcexop" } };

void _modinit(module_t *m)
{
	service_named_bind_command("chanserv", &cs_aop);
	service_named_bind_command("chanserv", &cs_sop);
	service_named_bind_command("chanserv", &cs_manager);
	if (ircd != NULL && ircd->uses_halfops)
		service_named_bind_command("chanserv", &cs_hop);
	service_named_bind_command("chanserv", &cs_vop);
	service_named_bind_command("chanserv", &cs_forcexop);

	if (module_request("chanserv/main"))
	{
		notify_channel_acl_change = module_locate_symbol("chanserv/main", "notify_channel_acl_change");
		notify_target_acl_change = module_locate_symbol("chanserv/main", "notify_target_acl_change");
	}
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("chanserv", &cs_aop);
	service_named_unbind_command("chanserv", &cs_sop);
	service_named_unbind_command("chanserv", &cs_manager);
	service_named_unbind_command("chanserv", &cs_hop);
	service_named_unbind_command("chanserv", &cs_vop);
	service_named_unbind_command("chanserv", &cs_forcexop);
}

static void cs_xop(sourceinfo_t *si, int parc, char *parv[], const char *leveldesc)
{
	myentity_t *mt;
	mychan_t *mc;
	bool operoverride = false;
	unsigned int restrictflags;
	const char *chan = parv[0];
	const char *cmd = parv[1];
	char *uname = parv[2];
	unsigned int level;

	if (!cmd || !chan)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "xOP");
		command_fail(si, fault_needmoreparams, _("Syntax: MANAGER|SOP|AOP|HOP|VOP <#channel> ADD|DEL|LIST <nickname>"));
		return;
	}

	if ((strcasecmp("LIST", cmd)) && (!uname))
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "xOP");
		command_fail(si, fault_needmoreparams, _("Syntax: MANAGER|SOP|AOP|HOP|VOP <#channel> ADD|DEL|LIST <nickname>"));
		return;
	}

	/* make sure they're registered, logged in
	 * and the founder of the channel before
	 * we go any further.
	 */
	if (!si->smu)
	{
		/* if they're opers and just want to LIST, they don't have to log in */
		if (!(has_priv(si, PRIV_CHAN_AUSPEX) && !strcasecmp("LIST", cmd)))
		{
			command_fail(si, fault_noprivs, _("You are not logged in."));
			return;
		}
	}

	mc = mychan_find(chan);
	if (!mc)
	{
		command_fail(si, fault_nosuch_target, _("Channel \2%s\2 is not registered."), chan);
		return;
	}

	if (metadata_find(mc, "private:close:closer") && (!has_priv(si, PRIV_CHAN_AUSPEX) || strcasecmp("LIST", cmd)))
	{
		command_fail(si, fault_noprivs, _("\2%s\2 is closed."), chan);
		return;
	}

	level = get_template_flags(mc, leveldesc);
	if (level & CA_FOUNDER)
	{
		command_fail(si, fault_noprivs, _("\2%s\2 %s template has founder flag, not allowing xOP command."), chan, leveldesc);
		return;
	}

	/* ADD */
	if (!strcasecmp("ADD", cmd))
	{
		mt = myentity_find_ext(uname);

		restrictflags = chanacs_source_flags(mc, si);
		/* As in /cs flags, allow founder to do anything */
		if (restrictflags & CA_FOUNDER)
			restrictflags = ca_all;
		
		/* The following is a bit complicated, to allow for
		 * possible future denial of granting +f */
		if (!(restrictflags & CA_FLAGS))
		{
			command_fail(si, fault_noprivs, _("You are not authorized to perform this operation."));
			return;
		}
		restrictflags = allow_flags(mc, restrictflags);

		if (restrictflags & CA_FOUNDER)
			restrictflags = ca_all;

		if ((restrictflags & level) != level)
		{
			command_fail(si, fault_noprivs, _("You are not authorized to perform this operation."));
			return;
		}
		
		cs_xop_do_add(si, mc, mt, uname, level, leveldesc, restrictflags);
	}

	else if (!strcasecmp("DEL", cmd))
	{
		mt = myentity_find_ext(uname);

		restrictflags = chanacs_source_flags(mc, si);
		/* As in /cs flags, allow founder to do anything -- fix for #64: allow self removal. */
		if (entity(si->smu) == mt || restrictflags & CA_FOUNDER)
			restrictflags = ca_all;

		/* The following is a bit complicated, to allow for
		 * possible future denial of granting +f */
		if (!(restrictflags & CA_FLAGS))
		{
			command_fail(si, fault_noprivs, _("You are not authorized to perform this operation."));
			return;
		}
		restrictflags = allow_flags(mc, restrictflags);

		if (restrictflags & CA_FOUNDER)
			restrictflags = ca_all;		

		if ((restrictflags & level) != level)
		{
			command_fail(si, fault_noprivs, _("You are not authorized to perform this operation."));
			return;
		}

		cs_xop_do_del(si, mc, mt, uname, level, leveldesc);
	}

	else if (!strcasecmp("LIST", cmd))
	{
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
		cs_xop_do_list(si, mc, level, leveldesc, operoverride);
	}
}

static void cs_cmd_manager(sourceinfo_t *si, int parc, char *parv[])
{
	cs_xop(si, parc, parv, "MANAGER");
}

static void cs_cmd_sop(sourceinfo_t *si, int parc, char *parv[])
{
	cs_xop(si, parc, parv, "SOP");
}

static void cs_cmd_aop(sourceinfo_t *si, int parc, char *parv[])
{
	cs_xop(si, parc, parv, "AOP");
}

static void cs_cmd_hop(sourceinfo_t *si, int parc, char *parv[])
{
	/* Don't reject the command. This helps the rare case where
	 * a network switches to a non-halfop ircd: users can still
	 * remove pre-transition HOP entries.
	 */
	if (!ircd->uses_halfops && si->su != NULL)
		notice(chansvs.nick, si->su->nick, "Warning: Your IRC server does not support halfops.");

	cs_xop(si, parc, parv, "HOP");
}

static void cs_cmd_vop(sourceinfo_t *si, int parc, char *parv[])
{
	cs_xop(si, parc, parv, "VOP");
}

static void cs_xop_do_add(sourceinfo_t *si, mychan_t *mc, myentity_t *mt, char *target, unsigned int level, const char *leveldesc, unsigned int restrictflags)
{
	chanacs_t *ca;
	unsigned int addflags = level, removeflags = ~level;
	unsigned int oldflags;
	const char *oldtemplate;
	bool isnew;
	hook_channel_acl_req_t req;

	if (!mt)
	{
		/* we might be adding a hostmask */
		if (!validhostmask(target))
		{
			command_fail(si, fault_badparams, _("\2%s\2 is neither a registered account nor a hostmask."), target);
			return;
		}
		
		if (validhostmask(target) && chansvs.min_non_wildcard_chars_host_acl > 0 &&
			check_not_enough_non_wildcard_chars(target,
				chansvs.min_non_wildcard_chars_host_acl, 1) &&
			!has_priv(si, PRIV_AKILL_ANYMASK))
		{
			command_fail(si, fault_badparams, _("Invalid nick!user@host: \2%s\2  At least %u non-wildcard characters are required."), target, chansvs.min_non_wildcard_chars_host_acl);
			return;
		}
		
		if (validhostmask(target) && (addflags & chansvs.flags_req_acct))
		{
			command_fail(si, fault_badparams, _("You may not add any of the following flags to hostmasks: %s"),
				bitmask_to_flags(chansvs.flags_req_acct));
			return;
		}

		target = collapse(target);
		ca = chanacs_open(mc, NULL, target, true, entity(si->smu));
		if (ca->level == level)
		{
			command_fail(si, fault_nochange, _("\2%s\2 is already on the %s list for: \2%s\2"), target, leveldesc, mc->name);
			return;
		}
		isnew = ca->level == 0;

		if (isnew && chanacs_is_table_full(ca))
		{
			command_fail(si, fault_toomany, _("Channel %s access list is full."), mc->name);
			chanacs_close(ca);
			return;
		}

		req.ca = ca;
		req.oldlevel = ca->level;

		oldflags = ca->level;

		if (!chanacs_modify(ca, &addflags, &removeflags, restrictflags))
		{
			command_fail(si, fault_noprivs, _("You are not authorized to modify the access entry for \2%s\2 on: \2%s\2"), target, mc->name);
			chanacs_close(ca);
			return;
		}

		req.newlevel = ca->level;

		hook_call_channel_acl_change(&req);

		chanacs_close(ca);

		if (!isnew)
		{
			oldtemplate = get_template_name(mc, oldflags);

			logcommand(si, CMDLOG_SET, "ADD: \2%s\2 \2%s\2 on \2%s\2 (changed access from \2%s\2)",
				mc->name, leveldesc, target, oldtemplate == NULL ? "<Custom>" : oldtemplate);
			command_success_nodata(si, _("\2%s\2's access on \2%s\2 has been changed from \2%s\2 to \2%s\2."),
				target, mc->name, oldtemplate == NULL ? "<Custom>" : oldtemplate, leveldesc);
			verbose(mc, "\2%s\2 changed \2%s\2's access from \2%s\2 to \2%s\2.",
				get_source_name(si), target, oldtemplate == NULL ? "<Custom>" : oldtemplate, leveldesc);
		}
		else
		{
			logcommand(si, CMDLOG_SET, "ADD: \2%s\2 \2%s\2 on \2%s\2", mc->name, leveldesc, target);
			command_success_nodata(si, _("\2%s\2 has been added to the %s list for \2%s\2."), target, leveldesc, mc->name);
			verbose(mc, "\2%s\2 added \2%s\2 to the %s list.", get_source_name(si), target, leveldesc);
		}

		return;
	}

	ca = chanacs_open(mc, mt, NULL, true, entity(si->smu));

	if (ca->level & CA_FOUNDER)
	{
		command_fail(si, fault_noprivs, _("\2%s\2 is the founder for \2%s\2 and may not be added to the %s list."), mt->name, mc->name, leveldesc);
		return;
	}

	if (ca->level == level)
	{
		command_fail(si, fault_nochange, _("\2%s\2 is already on the %s list for \2%s\2."), mt->name, leveldesc, mc->name);
		return;
	}

	/* NEVEROP logic moved here
	 * Allow changing access level, but not adding
	 * -- jilles */
	if (isuser(mt) && MU_NEVEROP & user(mt)->flags && (ca->level == 0 || ca->level == CA_AKICK))
	{
		command_fail(si, fault_noprivs, _("\2%s\2 does not wish to be added to channel access lists (NEVEROP set)."), mt->name);
		chanacs_close(ca);
		return;
	}

	/* just assume there's just one entry for that user -- jilles */

	isnew = ca->level == 0;
	if (isnew && chanacs_is_table_full(ca))
	{
		command_fail(si, fault_toomany, _("Channel %s access list is full."), mc->name);
		chanacs_close(ca);
		return;
	}

	req.ca = ca;
	req.oldlevel = ca->level;
	oldflags = ca->level;

	if (!chanacs_modify(ca, &addflags, &removeflags, restrictflags))
	{
		command_fail(si, fault_noprivs, _("You are not authorized to modify the access entry for \2%s\2 on \2%s\2."), mt->name, mc->name);
		chanacs_close(ca);
		return;
	}

	req.newlevel = ca->level;

	hook_call_channel_acl_change(&req);
	chanacs_close(ca);

	if (!isnew)
	{
		oldtemplate = get_template_name(mc, oldflags);

		logcommand(si, CMDLOG_SET, "ADD: \2%s\2 \2%s\2 on \2%s\2 (changed access from \2%s\2)",
			mc->name, leveldesc, mt->name, oldtemplate == NULL ? "<Custom>" : oldtemplate);
		command_success_nodata(si, _("\2%s\2's access on \2%s\2 has been changed from \2%s\2 to \2%s\2."),
			mt->name, mc->name, oldtemplate == NULL ? "<Custom>" : oldtemplate, leveldesc);
		verbose(mc, "\2%s\2 changed \2%s\2's access from \2%s\2 to \2%s\2.",
			get_source_name(si), mt->name, oldtemplate == NULL ? "<Custom>" : oldtemplate, leveldesc);
	}
	else
	{
		/* they have no access, add */
		logcommand(si, CMDLOG_SET, "ADD: \2%s\2 \2%s\2 on \2%s\2", mc->name, leveldesc, mt->name);
		command_success_nodata(si, _("\2%s\2 has been added to the %s list for \2%s\2."), mt->name, leveldesc, mc->name);
		verbose(mc, "\2%s\2 added \2%s\2 to the %s list.", get_source_name(si), mt->name, leveldesc);
	}

	if (isuser(mt))
	{
		myuser_t *tmu = myuser_find(mt->name);
		unsigned int addflags = get_template_flags(mc, leveldesc);
		unsigned int removeflags = ca_all & ~addflags;
		addflags &= ~oldflags;
		removeflags &= oldflags & ~addflags;

		char flagstr[54]; // 26 characters, * 2 for upper and lower case, then add a potential plus and minus sign. -> 54
		mowgli_strlcpy(flagstr, bitmask_to_flags2(addflags, removeflags), 54);
		notify_channel_acl_change(si, tmu, mc, flagstr, get_template_flags(mc, leveldesc));
		notify_target_acl_change(si, tmu, mc, flagstr, get_template_flags(mc, leveldesc));
	}
}

static void cs_xop_do_del(sourceinfo_t *si, mychan_t *mc, myentity_t *mt, char *target, unsigned int level, const char *leveldesc)
{
	chanacs_t *ca;
	hook_channel_acl_req_t req;
	unsigned int oldflags;

	/* let's finally make this sane.. --w00t */
	if (!mt)
	{
		/* we might be deleting a hostmask */
		if (!validhostmask(target))
		{
			command_fail(si, fault_badparams, _("\2%s\2 is neither a nickname nor a hostmask."), target);
			return;
		}

		ca = chanacs_find_host_literal(mc, target, level);
		if (ca == NULL || ca->level != level)
		{
			command_fail(si, fault_nochange, _("\2%s\2 is not on the %s list for \2%s\2."), target, leveldesc, mc->name);
			return;
		}

		req.ca = ca;
		req.oldlevel = ca->level;

		ca->level = 0;

		req.newlevel = ca->level;

		hook_call_channel_acl_change(&req);
		object_unref(ca);

		verbose(mc, "\2%s\2 removed \2%s\2 from the %s list.", get_source_name(si), target, leveldesc);
		logcommand(si, CMDLOG_SET, "DEL: \2%s\2 \2%s\2 from \2%s\2", mc->name, leveldesc, target);
		command_success_nodata(si, _("\2%s\2 has been removed from the %s list for \2%s\2."), target, leveldesc, mc->name);
		return;
	}

	if (!(ca = chanacs_find_literal(mc, mt, level)) || ca->level != level)
	{
		command_fail(si, fault_nochange, _("\2%s\2 is not on the %s list for \2%s\2."), mt->name, leveldesc, mc->name);
		return;
	}

	req.ca = ca;
	req.oldlevel = ca->level;
	oldflags = ca->level;

	ca->level = 0;

	req.newlevel = ca->level;

	hook_call_channel_acl_change(&req);
	object_unref(ca);

	command_success_nodata(si, _("\2%s\2 has been removed from the %s list for \2%s\2."), mt->name, leveldesc, mc->name);
	logcommand(si, CMDLOG_SET, "DEL: \2%s\2 \2%s\2 from \2%s\2", mc->name, leveldesc, mt->name);
	verbose(mc, "\2%s\2 removed \2%s\2 from the %s list.", get_source_name(si), mt->name, leveldesc);

	if (isuser(mt))
	{
		myuser_t *tmu = myuser_find(mt->name);
		unsigned int addflags = 0;
		unsigned int removeflags = ca_all & ~addflags;
		addflags &= ~oldflags;
		removeflags &= oldflags & ~addflags;

		char flagstr[54]; // 26 characters, * 2 for upper and lower case, then add a potential plus and minus sign. -> 54
		mowgli_strlcpy(flagstr, bitmask_to_flags2(addflags, removeflags), 54);
		notify_channel_acl_change(si, tmu, mc, flagstr, 0);
		notify_target_acl_change(si, tmu, mc, flagstr, 0);
	}
}


static void cs_xop_do_list(sourceinfo_t *si, mychan_t *mc, unsigned int level, const char *leveldesc, bool operoverride)
{
	chanacs_t *ca;
	mowgli_node_t *m;
	unsigned int i = 0;
	unsigned int entrywidth = 5, accounthostwidth = 12; /* "Account/Host is 12 characters long." */
	mowgli_node_t *n;
	char fmtstring[BUFSIZE], entryspacing[BUFSIZE], entryborder[BUFSIZE], accounthostspacing[BUFSIZE], accounthostborder[BUFSIZE];

	/* Set entrywidth and nickhostwidth to the length of the
	 * longest entries.
	 * - Ben
	 */

	MOWGLI_ITER_FOREACH(m, mc->chanacs.head)
	{
		ca = m->data;

		if (strlen(ca->entity ? ca->entity->name : ca->host) > accounthostwidth)
			accounthostwidth = strlen(ca->entity ? ca->entity->name : ca->host);

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
	mowgli_strlcpy(accounthostspacing, " ", BUFSIZE);
	mowgli_strlcpy(accounthostborder, "-", BUFSIZE);

	i = 1;

	for (i; i < entrywidth; i++)
	{
		mowgli_strlcat(entryborder, "-", BUFSIZE);
		if (i > 4)
			mowgli_strlcat(entryspacing, " ", BUFSIZE);
	}

	i = 1;

	for (i; i < accounthostwidth; i++)
	{
		mowgli_strlcat(accounthostborder, "-", BUFSIZE);
		if (i > 11)
			mowgli_strlcat(accounthostspacing, " ", BUFSIZE);
	}

	snprintf(fmtstring, BUFSIZE, "%%%uu %%-%us", 5, 20);

	command_success_nodata(si, _("%s list for: \2%s\2"), leveldesc ,mc->name);
	command_success_nodata(si, "Entry%sAccount/Host", entryspacing);
	command_success_nodata(si, "%s %s", entryborder, accounthostborder);

	MOWGLI_ITER_FOREACH(n, mc->chanacs.head)
	{
		ca = (chanacs_t *)n->data;
		if (ca->level == level)
		{
			if (ca->entity == NULL)
				//command_success_nodata(si, "%d: \2%s\2", ++i, ca->host);
				command_success_nodata(si, _(fmtstring), ++i, ca->host);
			else
				//command_success_nodata(si, _("%d: \2%s\2"), ++i, ca->entity->name);
				command_success_nodata(si, _(fmtstring), ++i, ca->entity->name);
		}
	}

	/* XXX */
	command_success_nodata(si, "%s %s", entryborder, accounthostborder);
	command_success_nodata(si, _("End of \2%s\2 %s listing.  Total of \2%d\2 %s."),
		mc->name, leveldesc, i, (i == 1) ? "entry" : "entries");

	if (operoverride)
		logcommand(si, CMDLOG_ADMIN, "LIST: \2%s\2 \2%s\2 (oper override)", mc->name, leveldesc);
	else
		logcommand(si, CMDLOG_GET, "LIST: \2%s\2 \2%s\2", mc->name, leveldesc);
}

static void cs_cmd_forcexop(sourceinfo_t *si, int parc, char *parv[])
{
	char *chan = parv[0];
	chanacs_t *ca;
	mychan_t *mc = mychan_find(chan);
	mowgli_node_t *n;
	int changes;
	unsigned int newlevel;
	const char *desc;
	unsigned int ca_manager, ca_sop, ca_aop, ca_hop, ca_vop;

	if (!chan)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "FORCEXOP");
		command_fail(si, fault_needmoreparams, _("Syntax: FORCEXOP <#channel>"));
		return;
	}

	if (!mc)
	{
		command_fail(si, fault_nosuch_target, _("Channel \2%s\2 is not registered."), chan);
		return;
	}

	if (metadata_find(mc, "private:close:closer"))
	{
		command_fail(si, fault_noprivs, _("\2%s\2 is closed."), chan);
		return;
	}

	if (!is_founder(mc, entity(si->smu)))
	{
		command_fail(si, fault_noprivs, _("You are not authorized to perform this operation."));
		return;
	}

	ca_manager = get_template_flags(mc, "MANAGER");
	ca_sop = get_template_flags(mc, "SOP");
	ca_aop = get_template_flags(mc, "AOP");
	ca_hop = get_template_flags(mc, "HOP");
	ca_vop = get_template_flags(mc, "VOP");

	changes = 0;
	MOWGLI_ITER_FOREACH(n, mc->chanacs.head)
	{
		ca = (chanacs_t *)n->data;

		if (ca->level & CA_AKICK)
			continue;
		if (ca->level & CA_FOUNDER)
			newlevel = custom_founder_check(), desc = "Founder";
		else if (!(~ca->level & ca_sop))
			newlevel = ca_sop, desc = "SOP";
		else if (ca->level == ca_aop)
			newlevel = ca_aop, desc = "AOP";
		else if (ca->level == ca_hop)
			newlevel = ca_hop, desc = "HOP";
		else if (ca->level == ca_vop)
			newlevel = ca_vop, desc = "VOP";
		else if (ca->level & (CA_SET | CA_RECOVER))
			newlevel = ca_manager, desc = "MANAGER";
		else if (ca->level & CA_FLAGS)
			newlevel = ca_sop, desc = "SOP";
		else if (ca->level & (CA_OP | CA_AUTOOP | CA_REMOVE))
			newlevel = ca_aop, desc = "AOP";
		else if (ca->level & (CA_HALFOP | CA_AUTOHALFOP | CA_TOPIC))
		{
			if (ca_hop == ca_vop)
				newlevel = ca_aop, desc = "AOP";
			else
				newlevel = ca_hop, desc = "HOP";
		}
		else /*if (ca->level & CA_AUTOVOICE)*/
			newlevel = ca_vop, desc = "VOP";
#if 0
		else
			newlevel = 0;
#endif
		if (newlevel == ca->level)
			continue;
		changes++;
		command_success_nodata(si, "%s: %s -> %s", ca->entity != NULL ? ca->entity->name : ca->host, bitmask_to_flags(ca->level), desc);
		chanacs_modify_simple(ca, newlevel, ~newlevel);
	}
	command_success_nodata(si, _("FORCEXOP \2%s\2 done (\2%d\2 changes)"), mc->name, changes);
	if (changes > 0)
		verbose(mc, "\2%s\2 reset access levels to xOP (\2%d\2 changes)", get_source_name(si), changes);
	logcommand(si, CMDLOG_SET, "FORCEXOP: \2%s\2 (\2%d\2 changes)", mc->name, changes);
}
