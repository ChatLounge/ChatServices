/*
 * Copyright (c) 2017 ChatLounge IRC Network Development Team
 *
 *     HISTORY command for GroupServ
 *
 *     When loaded, provides a means to view the group ACL and settings
 * change history.  Only users with flags +F or +s on the group, or
 * services operators with "group:auspex" or "general:admin" (regardless of
 * flags held on the group) may use this command.
 *
 */

#include "atheme.h"
#include "groupserv.h"

#define GROUP_HISTORY_LIMIT 25

DECLARE_MODULE_V1
(
	"groupserv/history", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net>"
);

static void gs_cmd_history(sourceinfo_t *si, int parc, char *parv[]);

void add_history_entry(sourceinfo_t *si, mygroup_t *mg, const char *desc);

command_t gs_history = { "HISTORY", N_("View the group change history."),
	AC_NONE, 2, gs_cmd_history, { .path = "groupserv/history" } };

void _modinit(module_t *m)
{
	use_groupserv_main_symbols(m);

	service_named_bind_command("groupserv", &gs_history);
};

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("groupserv", &gs_history);
};

static void gs_cmd_history(sourceinfo_t *si, int parc, char *parv[])
{
	metadata_t *md;
	unsigned int i = 1;
	mygroup_t *mg;

	if (parc < 1)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "HISTORY");
		command_fail(si, fault_needmoreparams, _("Syntax: HISTORY <!group>"));
		return;
	}

	//Debug
	//command_success_nodata(si, "Debug: parv[0]: %s", parv[0] ? parv[0] : "\2(null)\2");
	//return;

	if (!(mg = mygroup_find(parv[0])))
	{
		command_fail(si, fault_nosuch_target, _("Group \2%s\2 is not registered."), parv[0]);
		return;
	}

	if (metadata_find(mg, "private:close:closer") && !has_priv(si, PRIV_GROUP_AUSPEX) && !has_priv(si, PRIV_ADMIN))
	{
		command_fail(si, fault_noprivs, _("\2%s\2 is closed."), entity(mg)->name);
		return;
	}

	if (!groupacs_sourceinfo_has_flag(mg, si, GA_FOUNDER) &&
		!groupacs_sourceinfo_has_flag(mg, si, GA_SET) &&
		!has_priv(si, PRIV_GROUP_AUSPEX) && !has_priv(si, PRIV_ADMIN))
	{
		command_fail(si, fault_noprivs, _("You are not authorized to perform this operation."));
		return;
	}

	md = metadata_find(mg, "private:history-001");

	/* In theory this shouldn't trigger, but some groups may predate this feature. */
	if (md == NULL)
	{
		command_fail(si, fault_nosuch_target, _("\2%s\2 does not have have history yet."), entity(mg)->name);
		return;
	}
	else
	{
		command_success_nodata(si, _("Group Changes History for \2%s\2"), entity(mg)->name);
		command_success_nodata(si, _("===================================="));
	}

	for (i = 1; i < GROUP_HISTORY_LIMIT + 1; i++)
	{
		char mdname[20];
		char data[350];
		char author[32], timestamp[25], desc[300], *saveptr;
		char *token;

		snprintf(mdname, 20, "private:history-%03u", i);

		md = metadata_find(mg, mdname);

		if (md == NULL)
			break;

		mowgli_strlcpy(data, (char *)(md->value), sizeof data);
		mowgli_strlcpy(timestamp, strtok_r(data, " ", &saveptr), sizeof timestamp);
		mowgli_strlcpy(author, strtok_r(NULL, " ", &saveptr), sizeof author);
		mowgli_strlcpy(desc, strtok_r(NULL, " ", &saveptr), sizeof author);

		while ((token = strtok_r(NULL, " ", &saveptr)))
		{
			mowgli_strlcat(desc, " ", sizeof desc);
			mowgli_strlcat(desc, token, sizeof desc);
		}

		command_success_nodata(si, _("%3u. %-50s (%s ago by %s)"), i, desc, time_ago(atoi(timestamp)), author);
	}

	command_success_nodata(si, _("===================================="));
	command_success_nodata(si, _("End of Group Changes History for \2%s\2"), entity(mg)->name);
};

void add_history_entry(sourceinfo_t *si, mygroup_t *mg, const char *desc)
{
	metadata_t *md;
	char mdname2[20];
	char mdvalue[400];
	unsigned int i = 1;
	unsigned int last = 0; /* Store the number of the last (highest) history entry. */

	/* Look for the highest count of history entries.
	 * If necessary, overwrite the oldest one to make room for the newest one.
	 */
	for (i = 1; i < GROUP_HISTORY_LIMIT; i++)
	{
		char mdname[20];

		snprintf(mdname, 20, "private:history-%03d", i);

		md = metadata_find(mg, mdname);

		if (md == NULL)
		{
			/* If no history exists, neither will the first entry.  A "0th" entry is not used. */
			if (i == 1 && last == 0)
				break;

			break;
		}

		last++;
	}

	/* Too many entries, will need to overwrite the first one, and move every prior entry up by one. */
	if (last == GROUP_HISTORY_LIMIT - 1)
	{
		for (i = 2; i < GROUP_HISTORY_LIMIT + 1; i++)
		{
			char mdname[20], mdname3[20];

			snprintf(mdname, 20, "private:history-%03u", i);

			md = metadata_find(mg, mdname);

			snprintf(mdname3, 20, "private:history-%03u", i - 1);

			if (md != NULL)
				metadata_add(mg, mdname3, md->value);
		}
	}

		/* Actually add the entry. */
	snprintf(mdname2, 20, "private:history-%03u", last + 1);

	snprintf(mdvalue, 400, "%u %s %s", CURRTIME, si->smu == NULL ? "<none>" : entity(si->smu)->name, desc);

	metadata_add(mg, mdname2, mdvalue);

	return;
};