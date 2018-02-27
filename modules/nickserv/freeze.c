/*
 * Copyright (c) 2005-2007 Patrick Fish, et al.
 * Copyright (c) 2017-2018 ChatLounge IRC Network Development Team
 *
 * Rights to this code are as documented in doc/LICENSE.
 *
 * Gives services the ability to freeze nicknames
 *
 */

#include "atheme.h"
#include "authcookie.h"
#include "list_common.h"
#include "list.h"

DECLARE_MODULE_V1
(
	"nickserv/freeze", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	VENDOR_STRING
);

void (*add_history_entry_setting)(myuser_t *smu, myuser_t *tmu, const char *settingname, const char *setting) = NULL;

static void ns_cmd_freeze(sourceinfo_t *si, int parc, char *parv[]);

/* FREEZE ON|OFF -- don't pollute the root with THAW */
command_t ns_freeze = { "FREEZE", N_("Freezes an account."), PRIV_USER_ADMIN, 3, ns_cmd_freeze, { .path = "nickserv/freeze" } };

static bool is_frozen(const mynick_t *mn, const void *arg)
{
	myuser_t *mu = mn->owner;

	return !!metadata_find(mu, "private:freeze:freezer");
}

static bool is_account_frozen(myuser_t *mu, const void *arg)
{
	return !!metadata_find(mu, "private:freeze:freezer");
}

static bool frozen_match(const mynick_t *mn, const void *arg)
{
	const char *frozenpattern = (const char*)arg;
	metadata_t *mdfrozen;

	myuser_t *mu = mn->owner;
	mdfrozen = metadata_find(mu, "private:freeze:reason");

	if (mdfrozen != NULL && !match(frozenpattern, mdfrozen->value))
		return true;

	return false;
}

static bool frozen_account_match(myuser_t *mu, const void *arg)
{
	const char *frozenpattern = (const char*)arg;
	metadata_t *mdfrozen;

	mdfrozen = metadata_find(mu, "private:freeze:reason");

	if (mdfrozen != NULL && !match(frozenpattern, mdfrozen->value))
		return true;

	return false;
}

void _modinit(module_t *m)
{
	service_named_bind_command("nickserv", &ns_freeze);

	use_nslist_main_symbols(m);

	static list_param_t frozen;
	frozen.opttype = OPT_BOOL;
	frozen.is_match = is_frozen;

	static list_param_account_t frozen_account;
	frozen_account.opttype = OPT_BOOL;
	frozen_account.is_match = is_account_frozen;

	static list_param_t frozen_reason;
	frozen_reason.opttype = OPT_STRING;
	frozen_reason.is_match = frozen_match;

	static list_param_account_t frozen_account_reason;
	frozen_account_reason.opttype = OPT_STRING;
	frozen_account_reason.is_match = frozen_account_match;

	list_register("frozen", &frozen);
	list_register("frozen-reason", &frozen_reason);
	list_account_register("frozen", &frozen_account);
	list_account_register("frozen-reason", &frozen_account_reason);

	if (module_request("nickserv/main"))
		add_history_entry_setting = module_locate_symbol("nickserv/main", "add_history_entry_setting");
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("nickserv", &ns_freeze);

	list_unregister("frozen");
	list_unregister("frozen-reason");
	list_account_unregister("frozen");
	list_account_unregister("frozen-reason");
}

static void ns_cmd_freeze(sourceinfo_t *si, int parc, char *parv[])
{
	myuser_t *mu;
	char *target = parv[0];
	char *action = parv[1];
	char *reason = parv[2];
	user_t *u;
	mowgli_node_t *n, *tn;

	if (!target || !action)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "FREEZE");
		command_fail(si, fault_needmoreparams, _("Usage: FREEZE <account> <ON|OFF> [reason]"));
		return;
	}

	mu = myuser_find_ext(target);

	if (!mu)
	{
		command_fail(si, fault_nosuch_target, _("\2%s\2 is not registered."), target);
		return;
	}

	if (!strcasecmp(action, "ON"))
	{
		if (!reason)
		{
			command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "FREEZE");
			command_fail(si, fault_needmoreparams, _("Usage: FREEZE <nickname> ON <reason>"));
			return;
		}

		if (is_soper(mu))
		{
			command_fail(si, fault_badparams, _("The account \2%s\2 belongs to a services operator; it cannot be frozen."), target);
			return;
		}

		if (metadata_find(mu, "private:freeze:freezer"))
		{
			command_fail(si, fault_badparams, _("\2%s\2 is already frozen."), target);
			return;
		}

		metadata_add(mu, "private:freeze:freezer", get_oper_name(si));
		metadata_add(mu, "private:freeze:reason", reason);
		metadata_add(mu, "private:freeze:timestamp", number_to_string(CURRTIME));
		/* log them out */
		MOWGLI_ITER_FOREACH_SAFE(n, tn, mu->logins.head)
		{
			u = (user_t *)n->data;
			if (!ircd_on_logout(u, entity(mu)->name))
			{
				u->myuser = NULL;
				mowgli_node_delete(n, &mu->logins);
				mowgli_node_free(n);
			}
		}
		mu->flags |= MU_NOBURSTLOGIN;
		authcookie_destroy_all(mu);

		wallops("%s froze the account: \2%s\2 (%s)", get_oper_name(si), target, reason);
		logcommand(si, CMDLOG_ADMIN, "FREEZE:ON: \2%s\2 (reason: \2%s\2)", target, reason);
		command_success_nodata(si, _("\2%s\2 is now frozen; reason: %s"), target, reason);

		add_history_entry_setting(si->smu, mu, "FROZEN", "ON");
	}
	else if (!strcasecmp(action, "OFF"))
	{
		if (!metadata_find(mu, "private:freeze:freezer"))
		{
			command_fail(si, fault_badparams, _("\2%s\2 is not frozen."), target);
			return;
		}

		metadata_delete(mu, "private:freeze:freezer");
		metadata_delete(mu, "private:freeze:reason");
		metadata_delete(mu, "private:freeze:timestamp");

		wallops("%s unfroze/thawed the account: \2%s\2", get_oper_name(si), target);
		logcommand(si, CMDLOG_ADMIN, "FREEZE:OFF: \2%s\2", target);
		command_success_nodata(si, _("\2%s\2 has been unfrozen."), target);

		add_history_entry_setting(si->smu, mu, "FROZEN", "OFF");
	}
	else
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "FREEZE");
		command_fail(si, fault_needmoreparams, _("Usage: FREEZE <account> <ON|OFF> [reason]"));
	}
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
