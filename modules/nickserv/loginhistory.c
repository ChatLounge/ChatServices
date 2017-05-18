/*
 * Copyright (c) 2017 ChatLounge IRC Network Development
 *
 *     LOGINHISTORY command for NickServ
 *
 *     This file contains the code for NickServ ID/Login history.
 * When loaded, it will show a list of previous login events and 
 * other NickServ account events (such as settings changes and 
 * ACL changes).
 *
 */

#include "atheme.h"

#define NICKSERV_LOGINHISTORY_LIMIT 25

DECLARE_MODULE_V1
(
	"nickserv/loginhistory", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net>"
);

static void ns_cmd_loginhistory(sourceinfo_t *si, int parc, char *parv[]);
static void ns_cmd_userloginhistory(sourceinfo_t *si, int parc, char *parv[]);

command_t ns_loginhistory = { "LOGINHISTORY", N_("View account login history."),
	AC_AUTHENTICATED, 2, ns_cmd_loginhistory, { .path = "nickserv/loginhistory" } };
command_t ns_userloginhistory = { "USERLOGINHISTORY", N_("View another user's account login history."),
	PRIV_USER_AUSPEX, 3, ns_cmd_userloginhistory, { .path = "nickserv/userloginhistory" } };

void _modinit(module_t *m)
{
	service_named_bind_command("nickserv", &ns_loginhistory);
	service_named_bind_command("nickserv", &ns_userloginhistory);
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("nickserv", &ns_loginhistory);
	service_named_unbind_command("nickserv", &ns_userloginhistory);
}

static void show_history(sourceinfo_t *si, myuser_t *mu, bool self)
{
	metadata_t *md;
	unsigned int i = 1;

	md = metadata_find(mu, "private:loginhistory-001");

	/* In theory this shouldn't trigger, but some NickServ accounts may predate this feature. */
	if (md == NULL)
	{
		if (self)
			command_fail(si, fault_nosuch_target, _("You do not have any login history yet."));
		else
			command_fail(si, fault_nosuch_target, _("\2%s\2 does not have any login history yet."), entity(mu)->name);
		return;
	}
	else
	{
		command_success_nodata(si, _("Account Login History for \2%s\2"), entity(mu)->name);
		command_success_nodata(si, _("===================================="));
	}

	for (i = 1; i < NICKSERV_LOGINHISTORY_LIMIT + 1; i++)
	{
		char mdname[25];
		char data[350];
		char author[32], timestamp[25], desc[300], *saveptr;
		char *token;

		snprintf(mdname, 25, "private:loginhistory-%03u", i);

		md = metadata_find(mu, mdname);

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
	command_success_nodata(si, _("End of Account Login History for \2%s\2"), entity(mu)->name);

	return;
}

static void ns_cmd_loginhistory(sourceinfo_t *si, int parc, char *parv[])
{
	if (si->smu == NULL)
	{
		command_fail(si, fault_noprivs, _("You are not logged in."));
		return;
	}

	show_history(si, si->smu, true);

	return;
}

static void ns_cmd_userloginhistory(sourceinfo_t *si, int parc, char *parv[])
{
	myuser_t *mu;

	if (si->smu == NULL)
	{
		command_fail(si, fault_noprivs, _("You are not logged in."));
		return;
	}

	if (!parv[0])
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "LOGINHISTORY");
		command_fail(si, fault_needmoreparams, _("Syntax: LOGINHISTORY <Account> [logins]"));
		return;
	}

	mu = myuser_find_ext(parv[0]);

	if (mu == NULL)
	{
		command_fail(si, fault_nosuch_target, _("\2%s\2 is not registered."), parv[0]);

		return;
	}

	show_history(si, mu, false);

	return;
}

/* add_login_history_entry:
 *
 *     Adds a login history entry to a NickServ account as metadata.
 *
 * Inputs:
 *   myuser_t *smu - Source of the change (normally also the target)
 *   myuser_t *tmu - The NickServ account where the entry is being added.
 *   const char *desc - A free form description of the event.
 *
 */

void add_login_history_entry(myuser_t *smu, myuser_t *tmu, const char *desc)
{
	metadata_t *md;
	char mdname2[25];
	char mdvalue[400];
	unsigned int i = 1;
	unsigned int last = 0; /* Store the number of the last (highest) history entry. */

	/* Look for the highest count of history entries.
	 * If necessary, overwrite the oldest one to make room for the newest one.
	 */
	for (i = 1; i < NICKSERV_LOGINHISTORY_LIMIT; i++)
	{
		char mdname[25];

		snprintf(mdname, 25, "private:loginhistory-%03d", i);

		md = metadata_find(tmu, mdname);

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
	if (last == NICKSERV_LOGINHISTORY_LIMIT - 1)
	{
		for (i = 2; i < NICKSERV_LOGINHISTORY_LIMIT + 1; i++)
		{
			char mdname[25], mdname3[25];

			snprintf(mdname, 25, "private:loginhistory-%03u", i);

			md = metadata_find(tmu, mdname);

			snprintf(mdname3, 25, "private:loginhistory-%03u", i - 1);

			if (md != NULL)
				metadata_add(tmu, mdname3, md->value);
		}
	}

		/* Actually add the entry. */
	snprintf(mdname2, 25, "private:loginhistory-%03u", last + 1);

	snprintf(mdvalue, 400, "%u %s %s", CURRTIME, smu == NULL ? "<none>" : entity(smu)->name, desc);

	metadata_add(tmu, mdname2, mdvalue);

	return;
};