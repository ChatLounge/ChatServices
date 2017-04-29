/*
 * Copyright (c) 2016-2017 ChatLounge IRC Network Development Team
 *
 * Rights to this code are documented in doc/LICENSE.
 *
 *     This file contains routines to handle the GroupServ SET PUBACL
 * command.  If enabled, anyone may view the FLAGS, TEMPLATE, and
 * TEMPLATEVHOST lists for the group.  Otherwise, GA_ACLVIEW (+A) is
 * required.
 *
 */

#include "atheme.h"
#include "groupserv.h"

DECLARE_MODULE_V1
(
	"groupserv/set_pubacl", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net>"
);

static void gs_cmd_set_pubacl(sourceinfo_t *si, int parc, char *parv[]);

command_t gs_set_pubacl = { "PUBACL", N_("If enabled, anyone can view the FLAGS, TEMPLATE, and TEMPLATEVHOST lists."), AC_AUTHENTICATED, 2, gs_cmd_set_pubacl, { .path = "groupserv/set_pubacl" } };

static void gs_cmd_set_pubacl(sourceinfo_t *si, int parc, char *parv[])
{
	mygroup_t *mg;

	if (!parv[0] || !parv[1])
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "PUBACL");
		command_fail(si, fault_needmoreparams, _("Syntax: PUBACL <!group> <ON|OFF>"));
		return;
	}

	if ((mg = mygroup_find(parv[0])) == NULL)
	{
		command_fail(si, fault_nosuch_target, _("The group \2%s\2 does not exist."), parv[0]);
		return;
	}

	if (!groupacs_sourceinfo_has_flag(mg, si, GA_FOUNDER) &&
		!groupacs_sourceinfo_has_flag(mg, si, GA_SET))
	{
		command_fail(si, fault_noprivs, _("You are not authorized to execute this command."));
		return;
	}

	if (!strcasecmp(parv[1], "ON"))
	{
		if (mg->flags & MG_PUBACL)
		{
			command_fail(si, fault_nochange, _("\2%s\2 already has PUBACL enabled."), entity(mg)->name);
			return;
		}

		mg->flags |= MG_PUBACL;

		logcommand(si, CMDLOG_SET, "PUBACL:ON: \2%s\2", entity(mg)->name);
		command_success_nodata(si, _("PUBACL has been set on: \2%s\2"), entity(mg)->name);

		notify_group_set_change(si, si->smu, mg, "PUBACL", "ON");
	}
	else if (!strcasecmp(parv[1], "OFF"))
	{
		if (!(mg->flags & MG_PUBACL))
		{
			command_fail(si, fault_nochange, _("\2%s\2 already has PUBACL disabled."), entity(mg)->name);
			return;
		}

		mg->flags &= ~MG_PUBACL;

		logcommand(si, CMDLOG_SET, "PUBACL:OFF: \2%s\2", entity(mg)->name);
		command_success_nodata(si, _("PUBACL is no longer set on: \2%s\2"), entity(mg)->name);

		notify_group_set_change(si, si->smu, mg, "PUBACL", "OFF");
	}
	else
	{
		command_fail(si, fault_badparams, STR_INVALID_PARAMS, "PUBACL");
		command_fail(si, fault_badparams, _("Syntax: PUBACL <!group> <ON|OFF>"));
	}
}

void _modinit(module_t *m)
{
	use_groupserv_main_symbols(m);
	use_groupserv_set_symbols(m);

	command_add(&gs_set_pubacl, gs_set_cmdtree);
}

void _moddeinit(module_unload_intent_t intent)
{
	command_delete(&gs_set_pubacl, gs_set_cmdtree);
}