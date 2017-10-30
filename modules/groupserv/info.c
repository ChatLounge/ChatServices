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
	"groupserv/info", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	VENDOR_STRING
);

static void gs_cmd_info(sourceinfo_t *si, int parc, char *parv[]);

command_t gs_info = { "INFO", N_("Displays information about registered groups."), AC_NONE, 2, gs_cmd_info, { .path = "groupserv/info" } };

static void gs_cmd_info(sourceinfo_t *si, int parc, char *parv[])
{
	mygroup_t *mg;
	struct tm tm;
	char buf[BUFSIZE], strfbuf[BUFSIZE];
	metadata_t *md;
	char titleborder[BUFSIZE];
	unsigned int i = 0, titlewidth = 0;

	if (!parv[0])
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "INFO");
		command_fail(si, fault_needmoreparams, _("Syntax: INFO <!groupname>"));
		return;
	}

	if (!(mg = mygroup_find(parv[0])))
	{
		command_fail(si, fault_alreadyexists, _("Group \2%s\2 does not exist."), parv[0]);
		return;
	}

	tm = *localtime(&mg->regtime);
	strftime(strfbuf, sizeof strfbuf, TIME_FORMAT, &tm);

	mowgli_strlcpy(titleborder, "-", sizeof titleborder);

	/* "Information on " is 15 characters. */
	titlewidth = 15 + strlen(entity(mg)->name);

	command_success_nodata(si, _("Information on \2%s\2"), entity(mg)->name);

	i = 1;

	for (i; i < titlewidth; i++)
		mowgli_strlcat(titleborder, "-", sizeof titleborder);

	command_success_nodata(si, titleborder);

	command_success_nodata(si, _("Registered  : %s (%s ago)"), strfbuf, time_ago(mg->regtime));

	if (has_priv(si, PRIV_GROUP_AUSPEX))
		command_success_nodata(si, _("Entity ID   : %s"), entity(mg)->id);

	if (mg->flags & MG_PUBLIC || (si->smu != NULL && groupacs_sourceinfo_has_flag(mg, si, 0) && !groupacs_sourceinfo_has_flag(mg, si, GA_BAN)) || has_priv(si, PRIV_GROUP_AUSPEX))
		command_success_nodata(si, _("Founder     : %s"), mygroup_founder_names(mg));

	if ((md = metadata_find(mg, "description")))
		command_success_nodata(si, _("Description : %s"), md->value);
	if ((md = metadata_find(mg, "channel")))
		command_success_nodata(si, _("Channel     : %s"), md->value);
	if ((md = metadata_find(mg, "url")))
		command_success_nodata(si, _("URL         : %s"), md->value);
	if ((md = metadata_find(mg, "email")))
		command_success_nodata(si, _("Email       : %s"), md->value);

	*buf = '\0';

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

	if (*buf)
		command_success_nodata(si, _("Flags       : %s"), buf);

	if ((md = metadata_find(mg, "joinflags")) != NULL)
	{
		command_success_nodata(si, _("Join flags: %s"), bitmask_to_gflags(atoi(md->value)));
	}

	command_success_nodata(si, _("\2*** End of Info for the group %s ***\2"), entity(mg)->name);

	logcommand(si, CMDLOG_GET, "INFO: \2%s\2", parv[0]);
}


void _modinit(module_t *m)
{
	use_groupserv_main_symbols(m);

	service_named_bind_command("groupserv", &gs_info);
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("groupserv", &gs_info);
}
