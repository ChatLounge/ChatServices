/*
 * Copyright (c) 2005 William Pitcock, et al.
 * Rights to this code are as documented in doc/LICENSE.
 *
 * This file contains code for the nickserv DROP function.
 *
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"nickserv/drop", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	VENDOR_STRING
);

static void ns_cmd_drop(sourceinfo_t *si, int parc, char *parv[]);
static void ns_cmd_fdrop(sourceinfo_t *si, int parc, char *parv[]);

command_t ns_drop = { "DROP", N_("Drops an account registration."), AC_NONE, 3, ns_cmd_drop, { .path = "nickserv/drop" } };
command_t ns_fdrop = { "FDROP", N_("Forces dropping an account registration."), PRIV_USER_ADMIN, 1, ns_cmd_fdrop, { .path = "nickserv/fdrop" } };

void _modinit(module_t *m)
{
	service_named_bind_command("nickserv", &ns_drop);
	service_named_bind_command("nickserv", &ns_fdrop);
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("nickserv", &ns_drop);
	service_named_unbind_command("nickserv", &ns_fdrop);
}

static void ns_cmd_drop(sourceinfo_t *si, int parc, char *parv[])
{
	myuser_t *mu;
	mynick_t *mn;
	char *acc = parv[0];
	char *pass = parv[1];
	char *key = parv[2];
	char fullcmd[512];
	char key0[80], key1[80];

	if (!acc || !pass)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "DROP");
		command_fail(si, fault_needmoreparams, _("Syntax: DROP <account> <password>"));
		return;
	}

	if (!(mu = myuser_find(acc)))
	{
		if (!nicksvs.no_nick_ownership)
		{
			mn = mynick_find(acc);
			if (mn != NULL && command_find(si->service->commands, "UNGROUP"))
			{
				command_fail(si, fault_nosuch_target, _("\2%s\2 is a grouped nick, use %s to remove it."), acc, "UNGROUP");
				return;
			}
		}
		command_fail(si, fault_nosuch_target, _("\2%s\2 is not registered."), acc);
		return;
	}

	if (metadata_find(mu, "private:freeze:freezer"))
	{
		command_fail(si, fault_authfail, nicksvs.no_nick_ownership ? "You cannot login as \2%s\2 because the account has been frozen." : "You cannot identify to \2%s\2 because the nickname has been frozen.", entity(mu)->name);
		return;
	}

	if (!verify_password(mu, pass))
	{
		command_fail(si, fault_authfail, _("Authentication failed. Invalid password for \2%s\2."), entity(mu)->name);
		bad_password(si, mu);
		return;
	}

	if (!nicksvs.no_nick_ownership &&
			MOWGLI_LIST_LENGTH(&mu->nicks) > 1 &&
			command_find(si->service->commands, "UNGROUP"))
	{
		command_fail(si, fault_noprivs, _("Account \2%s\2 has %zu other nick(s) grouped to it, remove those first."),
				entity(mu)->name, MOWGLI_LIST_LENGTH(&mu->nicks) - 1);
		return;
	}

	if (is_soper(mu))
	{
		command_fail(si, fault_noprivs, _("The nickname \2%s\2 belongs to a services operator; it cannot be dropped."), acc);
		return;
	}

	if (mu->flags & MU_HOLD)
	{
		command_fail(si, fault_noprivs, _("The account \2%s\2 is held; it cannot be dropped."), acc);
		return;
	}

	if (!key)
	{
		create_challenge(si, entity(mu)->name, 0, key0);
		snprintf(fullcmd, sizeof fullcmd, "/%s%s DROP %s %s %s",
				(ircd->uses_rcommand == false) ? "msg " : "",
				nicksvs.me->disp, entity(mu)->name, pass, key0);
		command_success_nodata(si, _("This is a friendly reminder that you are about to \2destroy\2 the account \2%s\2."),
				entity(mu)->name);
		command_success_nodata(si, _("To avoid accidental use of this command, this operation has to be confirmed. Please confirm by replying with \2%s\2"),
				fullcmd);
		return;
	}

	create_challenge(si, entity(mu)->name, 0, key0);
	create_challenge(si, entity(mu)->name, 1, key1);
	if (strcmp(key, key0) && strcmp(key, key1))
	{
		command_fail(si, fault_badparams, _("Invalid key for %s."), "DROP");
		return;
	}

	command_add_flood(si, FLOOD_MODERATE);
	logcommand(si, CMDLOG_REGISTER, "DROP: \2%s\2", entity(mu)->name);
	hook_call_user_drop(mu);
	if (!nicksvs.no_nick_ownership)
		holdnick_sts(si->service->me, 0, entity(mu)->name, NULL);
	command_success_nodata(si, _("The account \2%s\2 has been dropped."), entity(mu)->name);
	object_dispose(mu);
}

static void ns_cmd_fdrop(sourceinfo_t *si, int parc, char *parv[])
{
	myuser_t *mu;
	mynick_t *mn;
	char *acc = parv[0];
	mowgli_node_t *n;

	if (!acc)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "FDROP");
		command_fail(si, fault_needmoreparams, _("Syntax: FDROP <account>"));
		return;
	}

	if (!(mu = myuser_find(acc)))
	{
		if (!nicksvs.no_nick_ownership)
		{
			mn = mynick_find(acc);
			if (mn != NULL && command_find(si->service->commands, "FUNGROUP"))
			{
				command_fail(si, fault_nosuch_target, _("\2%s\2 is a grouped nick, use %s to remove it."), acc, "FUNGROUP");
				return;
			}
		}
		command_fail(si, fault_nosuch_target, _("\2%s\2 is not registered."), acc);
		return;
	}

	if (is_soper(mu))
	{
		command_fail(si, fault_noprivs, _("The nickname \2%s\2 belongs to a services operator; it cannot be dropped."), acc);
		return;
	}

	if (mu->flags & MU_HOLD)
	{
		command_fail(si, fault_noprivs, _("The account \2%s\2 is held; it cannot be dropped."), acc);
		return;
	}

	wallops("%s dropped the account \2%s\2", get_oper_name(si), entity(mu)->name);
	logcommand(si, CMDLOG_ADMIN | LG_REGISTER, "FDROP: \2%s\2", entity(mu)->name);
	hook_call_user_drop(mu);
	if (!nicksvs.no_nick_ownership)
	{
		MOWGLI_ITER_FOREACH(n, mu->nicks.head)
		{
			mn = n->data;
			holdnick_sts(si->service->me, 0, mn->nick, NULL);
		}
	}
	command_success_nodata(si, _("The account \2%s\2 has been dropped."), entity(mu)->name);
	object_dispose(mu);
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
