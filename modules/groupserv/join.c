/*
 * Copyright (c) 2005 Atheme Development Group
 * Copyright (c) 2017 ChatLounge IRC Network Development Team
 *
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains routines to handle the GroupServ JOIN command.
 *
 */

#include "atheme.h"
#include "groupserv.h"

DECLARE_MODULE_V1
(
	"groupserv/join", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	VENDOR_STRING
);

static void gs_cmd_join(sourceinfo_t *si, int parc, char *parv[]);

command_t gs_join = { "JOIN", N_("Join a open group."), AC_AUTHENTICATED, 2, gs_cmd_join, { .path = "groupserv/join" } };

static void gs_cmd_join(sourceinfo_t *si, int parc, char *parv[])
{
	mygroup_t *mg;
	groupacs_t *ga;
	metadata_t *md, *md2;
	unsigned int flags = 0;
	bool invited = false;
	char description[256];

	if (!parv[0])
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "JOIN");
		command_fail(si, fault_needmoreparams, _("Syntax: JOIN <!groupname>"));
		return;
	}

	if (!(mg = mygroup_find(parv[0])))
	{
		command_fail(si, fault_alreadyexists, _("Group \2%s\2 does not exist."), parv[0]);
		return;
	}

	if ((md2 = metadata_find(si->smu, "private:groupinvite")))
	{
		if (!strcasecmp(md2->value, entity(mg)->name))
			invited = true;
		else
			invited = false;
	}

	if (!(mg->flags & MG_OPEN) && !invited)
	{
		command_fail(si, fault_noprivs, _("Group \2%s\2 is not open to anyone joining."), entity(mg)->name);
		return;
	}

	if (groupacs_sourceinfo_has_flag(mg, si, GA_BAN))
	{
		command_fail(si, fault_noprivs, _("You are not authorized to execute this command."));
		return;
	}

	if (groupacs_sourceinfo_has_flag(mg, si, 0))
	{
		command_fail(si, fault_nochange, _("You are already a member of group: \2%s\2."), entity(mg)->name);
		return;
	}

	if (MOWGLI_LIST_LENGTH(&mg->acs) > gs_config->maxgroupacs && (!(mg->flags & MG_ACSNOLIMIT)) && !invited)
        {
                command_fail(si, fault_toomany, _("Group %s access list is full."), entity(mg)->name);
                return;
        }

	if ((md = metadata_find(mg, "joinflags")))
		flags = atoi(md->value);
	else
		flags = gs_flags_parser(gs_config->join_flags == 0 ? 0 : gs_config->join_flags, 0, flags);

	if (gs_config->join_flags == 0 && flags == 0)
	{
		snprintf(description, sizeof description, "\2%s\2 joined.",
			entity(si->smu)->name);
		command_success_nodata(si, _("You are now a member of: \2%s\2"), entity(mg)->name);
	}
	else
	{
		snprintf(description, sizeof description, "\2%s\2 joined with the flags: %s",
			entity(si->smu)->name, gflags_tostr(ga_flags , flags));
		command_success_nodata(si, _("You are now a member of \2%s\2 with the flags: %s"),
			entity(mg)->name, gflags_tostr(ga_flags, flags));
	}

	ga = groupacs_add(mg, entity(si->smu), flags);

	if (invited)
		metadata_delete(si->smu, "private:groupinvite");

	notify_group_misc_change(si, mg, description);
}

void _modinit(module_t *m)
{
	use_groupserv_main_symbols(m);

	service_named_bind_command("groupserv", &gs_join);
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("groupserv", &gs_join);
}
