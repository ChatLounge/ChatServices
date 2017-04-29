/*
 * Copyright (c) 2005 Atheme Development Group
 * Copyright (c) 2017 ChatLounge IRC Network Development Team
 *
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains routines to handle the GroupServ HELP command.
 *
 */

#include "atheme.h"
#include "groupserv.h"

DECLARE_MODULE_V1
(
	"groupserv/set_description", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net>"
);

static void gs_cmd_set_description(sourceinfo_t *si, int parc, char *parv[]);

command_t gs_set_description = { "DESCRIPTION", N_("Sets the group description."), AC_AUTHENTICATED, 2, gs_cmd_set_description, { .path = "groupserv/set_description" } };

static void gs_cmd_set_description(sourceinfo_t *si, int parc, char *parv[])
{
	mygroup_t *mg;
	char *desc = parv[1];

	if (!(mg = mygroup_find(parv[0])))
	{
		command_fail(si, fault_nosuch_target, _("Group \2%s\2 does not exist."), parv[0]);
		return;
	}

	if (!groupacs_sourceinfo_has_flag(mg, si, GA_SET))
	{
		command_fail(si, fault_noprivs, _("You are not authorized to execute this command."));
		return;
	}

	if (!desc || !strcasecmp("OFF", desc) || !strcasecmp("NONE", desc))
	{
		/* not in a namespace to allow more natural use of SET PROPERTY.
		 * they may be able to introduce spaces, though. c'est la vie.
		 */
		if (metadata_find(mg, "description"))
		{
			metadata_delete(mg, "description");
			logcommand(si, CMDLOG_SET, "SET:DESCRIPTION:NONE: \2%s\2", entity(mg)->name);
			command_success_nodata(si, _("The description for \2%s\2 has been cleared."), parv[0]);

			notify_group_set_change(si, si->smu, mg, "DESCRIPTION", "None");
			return;
		}

		command_fail(si, fault_nochange, _("A description for \2%s\2 was not set."), parv[0]);
		return;
	}

	/* we'll overwrite any existing metadata */
	metadata_add(mg, "description", desc);

	logcommand(si, CMDLOG_SET, "SET:DESCRIPTION: \2%s\2 \2%s\2", entity(mg)->name, desc);
	command_success_nodata(si, _("The description of \2%s\2 has been set to: \2%s\2"), parv[0], desc);

	notify_group_set_change(si, si->smu, mg, "DESCRIPTION", desc);
}

void _modinit(module_t *m)
{
	use_groupserv_main_symbols(m);
	use_groupserv_set_symbols(m);

	command_add(&gs_set_description, gs_set_cmdtree);
}

void _moddeinit(module_unload_intent_t intent)
{
	command_delete(&gs_set_description, gs_set_cmdtree);
}

