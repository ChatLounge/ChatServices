/*
 * Copyright (c) 2017 ChatLounge IRC Network Development Team
 *
 *     HISTORY command for ChanServ
 *
 *     When loaded, provides a means to view the channel ACL and settings
 * change history.  Only users with flags +F, +R, +s on the channel, or
 * services operators with "chan:auspex" or "general:admin" (regardless of
 * flags held on the channel) may use this command.
 *
 */

#include "atheme.h"

#define CHANNEL_HISTORY_LIMIT 25

DECLARE_MODULE_V1
(
	"chanserv/history", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net>"
);

static void cs_cmd_history(sourceinfo_t *si, int parc, char *parv[]);

void add_history_entry(sourceinfo_t *si, mychan_t *mc, const char *desc);

command_t cs_history = { "HISTORY", N_("View the channel change history."),
	AC_NONE, 1, cs_cmd_history, { .path = "cservice/history" } };

void _modinit(module_t *m)
{
	service_named_bind_command("chanserv", &cs_history);
};

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("chanserv", &cs_history);
};

static void cs_cmd_history(sourceinfo_t *si, int parc, char *parv[])
{
	char *channel = parv[0];
	metadata_t *md;
	unsigned int i = 1;
	mychan_t *mc;

	if (parc < 1)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "HISTORY");
		command_fail(si, fault_needmoreparams, _("Syntax: HISTORY <channel>"));
		return;
	}

	mc = mychan_find(channel);
	if (!mc)
	{
		command_fail(si, fault_nosuch_target, _("Channel \2%s\2 is not registered."), channel);
		return;
	}

	if (metadata_find(mc, "private:close:closer") && !has_priv(si, PRIV_CHAN_AUSPEX) && !has_priv(si, PRIV_ADMIN))
	{
		command_fail(si, fault_noprivs, _("\2%s\2 is closed."), channel);
		return;
	}

	if (!chanacs_source_has_flag(mc, si, CA_FOUNDER) && !chanacs_source_has_flag(mc, si, CA_RECOVER) && !chanacs_source_has_flag(mc, si, CA_SET) &&
		!has_priv(si, PRIV_CHAN_AUSPEX) && !has_priv(si, PRIV_ADMIN))
	{
		command_fail(si, fault_noprivs, _("You are not authorized to perform this operation."));
		return;
	}

	md = metadata_find(mc, "private:history-001");

	/* In theory this shouldn't trigger, but some channels may predate this feature. */
	if (md == NULL)
	{
		command_fail(si, fault_nosuch_target, _("\2%s\2 does not have have history yet."), mc->name);
		return;
	}
	else
	{
		command_success_nodata(si, _("Channel Changes History for \2%s\2"), mc->name);
		command_success_nodata(si, _("===================================="));
	}

	for (i = 1; i < CHANNEL_HISTORY_LIMIT + 1; i++)
	{
		char mdname[20];
		char data[350];
		char author[32], timestamp[25], desc[300], *saveptr;
		char *token;

		snprintf(mdname, 20, "private:history-%03u", i);

		md = metadata_find(mc, mdname);

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
	command_success_nodata(si, _("End of Channel Changes History for \2%s\2"), mc->name);
};

void add_history_entry(sourceinfo_t *si, mychan_t *mc, const char *desc)
{
	metadata_t *md;
	char mdname2[20];
	char mdvalue[400];
	unsigned int i = 1;
	unsigned int last = 0; /* Store the number of the last (highest) history entry. */

	/* Look for the highest count of history entries.
	 * If necessary, overwrite the oldest one to make room for the newest one.
	 */
	for (i = 1; i < CHANNEL_HISTORY_LIMIT; i++)
	{
		char mdname[20];

		snprintf(mdname, 20, "private:history-%03d", i);

		md = metadata_find(mc, mdname);

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
	if (last == CHANNEL_HISTORY_LIMIT - 1)
	{
		for (i = 2; i < CHANNEL_HISTORY_LIMIT + 1; i++)
		{
			char mdname[20], mdname3[20];

			snprintf(mdname, 20, "private:history-%03u", i);

			md = metadata_find(mc, mdname);

			snprintf(mdname3, 20, "private:history-%03u", i - 1);

			if (md != NULL)
				metadata_add(mc, mdname3, md->value);
		}
	}

	/* Actually add the entry. */
	snprintf(mdname2, 20, "private:history-%03u", last + 1);

	snprintf(mdvalue, 400, "%u %s %s", CURRTIME, si->smu == NULL ? "<none>" : entity(si->smu)->name, desc);

	metadata_add(mc, mdname2, mdvalue);

	return;
};

/* add_history_entry_misc: Similar to add_history_entry but the source is ChanServ itself.
 */

void add_history_entry_misc(mychan_t *mc, const char *desc)
{
	metadata_t *md;
	char mdname2[20];
	char mdvalue[400];
	unsigned int i = 1;
	unsigned int last = 0; /* Store the number of the last (highest) history entry. */

	/* Look for the highest count of history entries.
	 * If necessary, overwrite the oldest one to make room for the newest one.
	 */
	for (i = 1; i < CHANNEL_HISTORY_LIMIT; i++)
	{
		char mdname[20];

		snprintf(mdname, 20, "private:history-%03d", i);

		md = metadata_find(mc, mdname);

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
	if (last == CHANNEL_HISTORY_LIMIT - 1)
	{
		for (i = 2; i < CHANNEL_HISTORY_LIMIT + 1; i++)
		{
			char mdname[20], mdname3[20];

			snprintf(mdname, 20, "private:history-%03u", i);

			md = metadata_find(mc, mdname);

			snprintf(mdname3, 20, "private:history-%03u", i - 1);

			if (md != NULL)
				metadata_add(mc, mdname3, md->value);
		}
	}

	/* Actually add the entry. */
	snprintf(mdname2, 20, "private:history-%03u", last + 1);

	snprintf(mdvalue, 400, "%u %s %s", CURRTIME, chansvs.nick, desc);

	metadata_add(mc, mdname2, mdvalue);

	return;
};