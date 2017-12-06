/*
 * Copyright (c) 2017 ChatLounge IRC Network Development Team
 *
 * Rights to this code are as documented in doc/LICENSE.
 *
 * This file contains code for the NickServ FGROUP command.
 *
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"nickserv/fgroup", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	VENDOR_STRING
);

void (*add_history_entry)(myuser_t *smu, myuser_t *tmu, const char *desc) = NULL;

static void ns_cmd_fgroup(sourceinfo_t *si, int parc, char *parv[]);

command_t ns_fgroup = { "FGROUP", N_("Forces adding of a nickname to an account."), PRIV_USER_ADMIN, 2, ns_cmd_fgroup, { .path = "nickserv/fgroup" } };

void _modinit(module_t *m)
{
	service_named_bind_command("nickserv", &ns_fgroup);
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("nickserv", &ns_fgroup);
}

static void add_nick_to_myuser(sourceinfo_t *si, myuser_t *mu, const char *nick)
{
	mynick_t *mn;
	hook_user_req_t hdata;
	char description[300];

	mn = mynick_find(nick);

	if (mn != NULL)
	{
		//if (mn->owner == mu)
			if (si->smu == mn->owner)
				command_fail(si, fault_nochange, _("Nick \2%s\2 is already registered to your account."), mn->nick);
			else
				command_fail(si, fault_alreadyexists, _("Nick \2%s\2 is already registered to: \2%s\2"), mn->nick, entity(mn->owner)->name);
		return;
	}

	if (IsDigit(nick[0]))
	{
		command_fail(si, fault_badparams, _("It is not possible to register nicks that begin with a number."));
		return;
	}

	logcommand(si, CMDLOG_REGISTER, "FGROUP: \2%s\2 to \2%s\2", nick, entity(mu)->name);
	mn = mynick_add(mu, nick);
	mn->registered = CURRTIME;
	mn->lastseen = CURRTIME;

	hdata.si = si;
	hdata.mu = si->smu;
	hdata.mn = mn;
	hook_call_nick_group(&hdata);

	if (si->smu == mu)
		command_success_nodata(si, _("Nick \2%s\2 is now registered to your account."), mn->nick);
	else
		command_success_nodata(si, _("Nick \2%s\2 is now registered to: \2%s\2"), mn->nick, entity(mu)->name);

	if ((add_history_entry = module_locate_symbol("nickserv/history", "add_history_entry")) != NULL)
	{
		snprintf(description, sizeof description, "Nick grouped: \2%s\2", mn->nick);
		add_history_entry(si->smu, mu, description);
	}
}

static void ns_cmd_fgroup(sourceinfo_t *si, int parc, char *parv[])
{
	if (si->su == NULL)
	{
		command_fail(si, fault_noprivs, _("\2%s\2 can only be executed via IRC."), "GROUP");
		return;
	}

	if (nicksvs.no_nick_ownership)
	{
		command_fail(si, fault_noprivs, _("Nickname ownership is disabled."));
		return;
	}

	if (parc < 1)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "FGROUP");
		command_fail(si, fault_needmoreparams, _("Syntax: FGROUP <nickname> [accountname]"));
		return;
	}

	/* Add nick to own account. */
	if (parc < 2)
	{
		add_nick_to_myuser(si, si->smu, parv[0]);
		return;
	}

	/* Add nick to another account. */
	if (parc < 3)
	{
		myuser_t *mu;
		mu = myuser_find_by_nick(parv[1]);

		if (mu == NULL)
		{
			command_fail(si, fault_nosuch_target, "The account \2%s\2 does not exist.", parv[1]);
			return;
		}

		add_nick_to_myuser(si, mu, parv[0]);

		return;
	}
}

