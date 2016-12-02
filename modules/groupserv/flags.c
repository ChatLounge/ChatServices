/*
 * Copyright (c) 2005 Atheme Development Group
 * Copyright (c) 2016 ChatLounge IRC Network Development Team
 *     (http://www.chatlounge.net/)
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains routines to handle the GroupServ HELP command.
 *
 */

#include "atheme.h"
#include "groupserv.h"

DECLARE_MODULE_V1
(
	"groupserv/flags", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net/>"
);

static void gs_cmd_flags(sourceinfo_t *si, int parc, char *parv[]);

command_t gs_flags = { "FLAGS", N_("Sets flags on a user in a group."), AC_AUTHENTICATED, 3, gs_cmd_flags, { .path = "groupserv/flags" } };

static void gs_cmd_flags(sourceinfo_t *si, int parc, char *parv[])
{
	mowgli_node_t *n;
	mygroup_t *mg;
	myentity_t *mt;
	groupacs_t *ga;
	unsigned int flags = 0, oldflags = 0;
	unsigned int dir = 0;
	char *c;
	bool operoverride = false;

	if (!parv[0])
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "FLAGS");
		command_fail(si, fault_needmoreparams, _("Syntax: FLAGS <!group> [user] [changes]"));
		return;
	}

	if ((mg = mygroup_find(parv[0])) == NULL)
	{
		command_fail(si, fault_nosuch_target, _("The group \2%s\2 does not exist."), parv[0]);
		return;
	}

	if (!groupacs_sourceinfo_has_flag(mg, si, (parv[2] != NULL ? GA_FLAGS : GA_ACLVIEW)))
	{
		if (has_priv(si, PRIV_GROUP_AUSPEX))
			operoverride = true;
		else
		{
			command_fail(si, fault_noprivs, _("You are not authorized to perform this operation."));
			return;
		}
	}

	if (!parv[1])
	{
		int i = 1;

		command_success_nodata(si, _("Entry Account                Flags"));
		command_success_nodata(si, "----- ---------------------- -----");

		MOWGLI_ITER_FOREACH(n, mg->acs.head)
		{
			ga = n->data;

			command_success_nodata(si, "%-5d %-22s %s (%s)", i, ga->mt->name,
						gflags_tostr(ga_flags, ga->flags),
						get_group_template_name(mg, ga->flags) == NULL ? "<Custom>" : get_group_template_name(mg, ga->flags));

			i++;
		}

		command_success_nodata(si, "----- ---------------------- -----");
		command_success_nodata(si, _("End of \2%s\2 FLAGS listing."), parv[0]);

		if (operoverride)
			logcommand(si, CMDLOG_ADMIN, "FLAGS: \2%s\2 (oper override)", parv[0]);
		else
			logcommand(si, CMDLOG_GET, "FLAGS: \2%s\2", parv[0]);

		return;
	}

	/* simple check since it's already checked above */
	if (operoverride)
	{
		command_fail(si, fault_noprivs, _("You are not authorized to perform this operation."));
		return;
	}

	if ((mt = myentity_find_ext(parv[1])) == NULL)
	{
		command_fail(si, fault_nosuch_target, _("\2%s\2 is not a registered account."), parv[1]);
		return;
	}

	if (!parv[2])
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "FLAGS");
		command_fail(si, fault_needmoreparams, _("Syntax: FLAGS <!group> <user> <changes>"));
		return;
	}

	if (isuser(mt) && (MU_NEVERGROUP & user(mt)->flags) && (groupacs_find(mg, mt, 0, true) == NULL))
	{
		command_fail(si, fault_noprivs, _("\2%s\2 does not wish to have flags in any groups."), parv[1]);
		return;
	}

	ga = groupacs_find(mg, mt, 0, false);
	if (ga != NULL)
		flags = ga->flags;

	oldflags = flags;
	if (strchr(parv[2], '+') || strchr(parv[2], '-') || strchr(parv[2], '=') || strchr(parv[2], '!'))
	{
		flags = gs_flags_parser(parv[2], 1, flags);
		if (flags == 0)
		{
			command_fail(si, fault_badparams, _("Invalid flags specified."));
			return;
		}
	}
	else
	{
		flags = get_group_template_flags(mg, parv[2]);
		if (flags == 0)
		{
			command_fail(si, fault_badparams, _("Invalid template name."));
			return;
		}
	}

	/* check for MU_NEVEROP and forbid committing the change if it's enabled */
	if (!(oldflags & GA_CHANACS) && (flags & GA_CHANACS))
	{
		if (isuser(mt) && user(mt)->flags & MU_NEVEROP)
		{
			command_fail(si, fault_noprivs, _("\2%s\2 does not wish to be added to channel access lists (NEVEROP set)."), mt->name);
			return;
		}
	}

	if ((flags & GA_FOUNDER) && !(oldflags & GA_FOUNDER))
	{
		if (!(groupacs_sourceinfo_flags(mg, si) & GA_FOUNDER))
		{
			flags &= ~GA_FOUNDER;
			goto no_founder;
		}

		flags |= GA_FLAGS;
	}
	else if ((oldflags & GA_FOUNDER) && !(flags & GA_FOUNDER) && !(groupacs_sourceinfo_flags(mg, si) & GA_FOUNDER))
	{
		command_fail(si, fault_noprivs, _("You are not authorized to perform this operation."));
		return;
	}

	if (!gs_config->no_leveled_flags)
	{
		if (((oldflags & GA_FLAGS) || (flags & GA_FLAGS)) && !((groupacs_sourceinfo_flags(mg, si) & GA_SET) || (groupacs_sourceinfo_flags(mg, si) & GA_FOUNDER))) {
			command_fail(si, fault_noprivs, _("You are not authorized to perform this operation."));
			return;
		}
		
		if (((oldflags & GA_SET) || (flags & GA_SET)) && !(groupacs_sourceinfo_flags(mg, si) & GA_FOUNDER)) {
			command_fail(si, fault_noprivs, _("You are not authorized to perform this operation."));
			return;
		}
	}
	
	if (!(flags & GA_FOUNDER) && groupacs_find(mg, mt, GA_FOUNDER, false))
	{
		if (mygroup_count_flag(mg, GA_FOUNDER) == 1)
		{
			command_fail(si, fault_noprivs, _("You may not remove the last founder."));
			return;
		}

		if (!groupacs_sourceinfo_has_flag(mg, si, GA_FOUNDER))
		{
			command_fail(si, fault_noprivs, _("You may not remove a founder's +F access."));
			return;
		}
	}

no_founder:
	if (ga != NULL && flags != 0)
	{
		if (ga->flags != flags)
			ga->flags = flags;
		else
		{
			command_fail(si, fault_nochange, _("Group \2%s\2 access for \2%s\2 unchanged."), entity(mg)->name, mt->name);
			return;
		}
	}
	else if (ga != NULL)
	{
		groupacs_delete(mg, mt);
		command_success_nodata(si, _("\2%s\2 has been removed from \2%s\2."), mt->name, entity(mg)->name);
		logcommand(si, CMDLOG_SET, "FLAGS:REMOVE: \2%s\2 on \2%s\2", mt->name, entity(mg)->name);
		return;
	}
	else if (flags != 0)
	{
		if (MOWGLI_LIST_LENGTH(&mg->acs) > gs_config->maxgroupacs && (!(mg->flags & MG_ACSNOLIMIT)))
		{
			command_fail(si, fault_toomany, _("Group %s access list is full."), entity(mg)->name);
			return;
		}
		ga = groupacs_add(mg, mt, flags);
	}
	else
	{
		command_fail(si, fault_nochange, _("Group \2%s\2 access for \2%s\2 unchanged."), entity(mg)->name, mt->name);
		return;
	}

	MOWGLI_ITER_FOREACH(n, entity(mg)->chanacs.head)
	{
		chanacs_t *ca = n->data;

		verbose(ca->mychan, "\2%s\2 now has flags \2%s\2 in the group \2%s\2 which communally has \2%s\2 on \2%s\2.",
			mt->name, gflags_tostr(ga_flags, ga->flags), entity(mg)->name,
			bitmask_to_flags(ca->level), ca->mychan->name);

		hook_call_channel_acl_change(&(hook_channel_acl_req_t){ .ca = ca });
	}

	command_success_nodata(si, _("\2%s\2 now has flags \2%s\2 on \2%s\2."), mt->name, gflags_tostr(ga_flags, ga->flags), entity(mg)->name);

	/* XXX */
	logcommand(si, CMDLOG_SET, "FLAGS: \2%s\2 now has flags \2%s\2 on \2%s\2", mt->name, gflags_tostr(ga_flags,  ga->flags), entity(mg)->name);
}

void _modinit(module_t *m)
{
	use_groupserv_main_symbols(m);

	service_named_bind_command("groupserv", &gs_flags);
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("groupserv", &gs_flags);
}

