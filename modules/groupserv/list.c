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
	"groupserv/list", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	VENDOR_STRING
);

static void gs_cmd_list(sourceinfo_t *si, int parc, char *parv[]);

command_t gs_list = { "LIST", N_("List registered groups."), PRIV_GROUP_AUSPEX, 1, gs_cmd_list, { .path = "groupserv/list" } };

/* Perhaps add criteria to groupser/list like there is now in chanserv/list and nickserv/list in the future */
static void gs_cmd_list(sourceinfo_t *si, int parc, char *parv[])
{
	myentity_t *mt;
	char *pattern = parv[0];
	unsigned int matches = 0;
	myentity_iteration_state_t state;
	char buf[BUFSIZE];

	if (!pattern)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "LIST");
		command_fail(si, fault_needmoreparams, _("Syntax: LIST <group pattern>"));
		return;
	}

	/* No need to say "Groups currently registered". You can't have a unregistered group. */
	command_success_nodata(si, _("Groups matching pattern \2%s\2:"), pattern);

	*buf = '\0';

	MYENTITY_FOREACH_T(mt, &state, ENT_GROUP)
	{
		mygroup_t *mg = group(mt);
		continue_if_fail(mt != NULL);
		continue_if_fail(mg != NULL);

		if (!match(pattern, entity(mg)->name))
		{
			if (mg->flags & MG_REGNOLIMIT)
				mowgli_strlcat(buf, "REGNOLIMIT", BUFSIZE);

			if (mg->flags & MG_ACSNOLIMIT)
			{
				if (*buf)
					mowgli_strlcat(buf, " ", BUFSIZE);

				mowgli_strlcat(buf, "ACSNOLIMIT", BUFSIZE);
			}

			if (mg->flags & MG_OPEN)
			{
				if (*buf)
					mowgli_strlcat(buf, " ", BUFSIZE);

				mowgli_strlcat(buf, "OPEN", BUFSIZE);
			}

			if (mg->flags & MG_PUBACL)
			{
				if (*buf)
					mowgli_strlcat(buf, " ", BUFSIZE);

				mowgli_strlcat(buf, "PUBACL", BUFSIZE);
			}

			if (mg->flags & MG_PUBLIC)
			{
				if (*buf)
					mowgli_strlcat(buf, " ", BUFSIZE);

				mowgli_strlcat(buf, "PUBLIC", BUFSIZE);
			}

			command_success_nodata(si, _("- %s (%s) (Flags: %s)"), entity(mg)->name, mygroup_founder_names(mg),
				*buf ? buf : "<None>");
			matches++;
		}
	}

	if (matches == 0)
		command_success_nodata(si, _("No groups matched pattern \2%s\2"), pattern);
	else
		command_success_nodata(si, ngettext(N_("\2%d\2 match for pattern \2%s\2"), N_("\2%d\2 matches for pattern \2%s\2"), matches), matches, pattern);

	logcommand(si, CMDLOG_ADMIN, "LIST: \2%s\2 (\2%d\2 match%s)", pattern, matches, matches == 1 ? "" : "es");
}

void _modinit(module_t *m)
{
	use_groupserv_main_symbols(m);

	service_named_bind_command("groupserv", &gs_list);
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("groupserv", &gs_list);
}
