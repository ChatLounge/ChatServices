/*
 * Copyright (c) 2005 Atheme Development Group
 * Copyright (c) 2017 ChatLounge IRC Network Development Team
 *
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains routines to handle the GroupServ REGISTER command.
 *
 */

#include "atheme.h"
#include "groupserv.h"

DECLARE_MODULE_V1
(
	"groupserv/register", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net>"
);

static void gs_cmd_register(sourceinfo_t *si, int parc, char *parv[]);

command_t gs_register = { "REGISTER", N_("Registers a group."), AC_AUTHENTICATED, 2, gs_cmd_register, { .path = "groupserv/register" } };

static void gs_cmd_register(sourceinfo_t *si, int parc, char *parv[])
{
	mygroup_t *mg;
	char description[256];

	if (!parv[0] || *parv[0] != '!')
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "REGISTER");
		command_fail(si, fault_needmoreparams, _("To register a group: REGISTER <!groupname>"));
		return;
	}

	if (si->smu->flags & MU_WAITAUTH)
	{
		command_fail(si, fault_notverified, _("You need to verify your email address before you may register groups."));
		return;
	}

	if (mygroup_find(parv[0]))
	{
		command_fail(si, fault_alreadyexists, _("The group \2%s\2 already exists."), parv[0]);
		return;
	}

	if (strlen(parv[0]) >= NICKLEN)
	{
		command_fail(si, fault_badparams, _("The group name \2%s\2 is invalid."), parv[0]);
		return;
	}

	if (myentity_count_group_flag(entity(si->smu), GA_FOUNDER) > gs_config->maxgroups &&
	    !has_priv(si, PRIV_REG_NOLIMIT))
	{
		command_fail(si, fault_toomany, _("You have too many groups registered."));
		return;
	}

	if (metadata_find(si->smu, "private:restrict:setter"))
	{
		command_fail(si, fault_noprivs, _("You have been restricted from registering groups by network staff."));
		return;
	}

	mg = mygroup_add(parv[0]);
	groupacs_add(mg, entity(si->smu), GA_ALL | GA_FOUNDER);
	hook_call_group_register(mg);

	logcommand(si, CMDLOG_REGISTER, "REGISTER: \2%s\2", entity(mg)->name);
	command_success_nodata(si, _("The group \2%s\2 has been registered to: \2%s\2"), entity(mg)->name, entity(si->smu)->name);

	snprintf(description, sizeof description, "Registered group.");

	notify_group_misc_change(si, mg, description);
}


void _modinit(module_t *m)
{
	use_groupserv_main_symbols(m);

	service_named_bind_command("groupserv", &gs_register);
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("groupserv", &gs_register);
}

