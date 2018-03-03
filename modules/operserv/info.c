/*
 * Copyright (c) 2010 Atheme Development Group, et al.
 * Copyright (c) 2016-2018 ChatLounge IRC Network Development Team
 *
 * Rights to this code are as documented in doc/LICENSE.
 *
 * This file contains code for OS INFO
 *
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"operserv/info", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	VENDOR_STRING
);

static void os_cmd_info(sourceinfo_t *si, int parc, char *parv[]);

command_t os_info = { "INFO", N_("Shows some useful information about the current settings of services."), PRIV_SERVER_AUSPEX, 1, os_cmd_info, { .path = "oservice/info" } };

void _modinit(module_t *m)
{
        service_named_bind_command("operserv", &os_info);
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("operserv", &os_info);
}

static void os_cmd_info(sourceinfo_t *si, int parc, char *parv[])
{
	mowgli_node_t *tn, *n2;

	logcommand(si, CMDLOG_GET, "INFO");

	command_success_nodata(si, _("===================================="));
	command_success_nodata(si, _("\02General Settings\02"));
	command_success_nodata(si, _("===================================="));
	command_success_nodata(si, _("How often services writes changes to the database: %d minute%s"),
		config_options.commit_interval / 60, config_options.commit_interval == 60 ? "" : "s");
	command_success_nodata(si, _("Default kline expiry time: %d day%s"),
		config_options.kline_time / 86400, config_options.kline_time == 86400 ? "" : "s");
	command_success_nodata(si, _("Minimum number of non-wildcard chars for klines: %u"), config_options.kline_non_wildcard_chars);
	command_success_nodata(si, _("Kline ident@host instead of *@host on automated bans: %s"),
		config_options.kline_with_ident ? "yes" : "no");
	command_success_nodata(si, _("Don't kline with ident@host if unverified (first char of ident is ~): %s"),
		config_options.kline_verified_ident ? "yes" : "no");
	command_success_nodata(si, _("Remove more specific klines if a wider kline is added: %s"),
		config_options.kline_do_not_remove_more_specific ? "no" : "yes");
	command_success_nodata(si, _("Will services be sending WALLOPS/GLOBOPS about various things: %s"), config_options.silent ? "no" : "yes");
	if (config_options.flood_msgs)
		command_success_nodata(si, _("How many messages before a flood is triggered: %d message%s"),
			config_options.flood_msgs, config_options.flood_msgs == 1 ? "" : "s");
	else
		command_success_nodata(si, _("How many messages before a flood is triggered: Disabled"));
	command_success_nodata(si, _("How long before the flood counter resets: %d second%s"),
		config_options.flood_time, config_options.flood_time == 1 ? "" : "s");
	command_success_nodata(si, _("Default maximum number of clones allowed: %d"), config_options.default_clone_allowed);
	if (config_options.ratelimit_uses)
		command_success_nodata(si, _("Number of commands used before ratelimiting starts: %d"),
			config_options.ratelimit_uses);
	else
		command_success_nodata(si, _("Number of commands used before ratelimiting starts: Disabled"));
	command_success_nodata(si, _("How long before ratelimiting counter resets, (if 0, ratelimiting is disabled): %d seconds"), config_options.ratelimit_period);
	command_success_nodata(si, _("SaslServ sends a QUIT right before shutdown or restart: %s"),
		config_options.send_sasl_quit ? "Yes" : "No");

	if (IS_TAINTED)
	{
		mowgli_node_t *n;

		command_success_nodata(si, _("Services is presently \2TAINTED\2, no support will be given for this configuration."));
		command_success_nodata(si, _("List of active taints:"));

		MOWGLI_ITER_FOREACH(n, taint_list.head)
		{
			taint_reason_t *tr = n->data;

			command_success_nodata(si, _("Taint Condition: %s"), tr->condition);
			command_success_nodata(si, _("Taint Location: %s:%d"), tr->file, tr->line);
			command_success_nodata(si, _("Taint Explanation: %s"), tr->buf);
		}
	}

	if (MOWGLI_LIST_LENGTH(&nicksvs.emailexempts) > 0)
	{
		command_success_nodata(si, _("Email address%s exempt from the maximum usernames check:"),
			MOWGLI_LIST_LENGTH(&nicksvs.emailexempts) == 1 ? "" : "es");
		command_success_nodata(si, _("------------------------------------------------------%s"),
			MOWGLI_LIST_LENGTH(&nicksvs.emailexempts) == 1 ? "" : "--");

		MOWGLI_ITER_FOREACH(tn, nicksvs.emailexempts.head)
		{
			command_success_nodata(si, _("  %s"), (char *)tn->data);
		}
	}

	if (MOWGLI_LIST_LENGTH(&config_options.exempts) > 0)
	{
		command_success_nodata(si, _("user@host mask%s that %s autokline exempt:"),
			MOWGLI_LIST_LENGTH(&config_options.exempts) == 1 ? "" : "s",
			MOWGLI_LIST_LENGTH(&config_options.exempts) == 1 ? "is" : "are");
		command_success_nodata(si, _("----------------------------------------"),
			MOWGLI_LIST_LENGTH(&config_options.exempts) == 1 ? "" : "-",
			MOWGLI_LIST_LENGTH(&config_options.exempts) == 1 ? "" : "-");

		MOWGLI_ITER_FOREACH(n2, config_options.exempts.head)
		{
			command_success_nodata(si, _("  %s"), (char *)n2->data);
		}
	}

	command_success_nodata(si, _("Number of services ignores: %d"), cnt.svsignore);
	command_success_nodata(si, _("Number of services operators: %d"), cnt.soper);
	command_success_nodata(si, _("===================================="));
	command_success_nodata(si, _("\02%s Settings\02"), nicksvs.nick);
	command_success_nodata(si, _("===================================="));
	command_success_nodata(si, _("Number of registered accounts: %d"), cnt.myuser);
	if (!nicksvs.no_nick_ownership)
		command_success_nodata(si, _("Number of registered nicks: %d"), cnt.mynick);
	command_success_nodata(si, _("No nick ownership enabled: %s"), nicksvs.no_nick_ownership ? "yes" : "no");
	command_success_nodata(si, _("Nickname expiration time: %d day%s"),
		nicksvs.expiry / 86400, nicksvs.expiry == 86400 ? "" : "s");
	command_success_nodata(si, _("Nickname enforce expiry time: %d day%s"),
		nicksvs.enforce_expiry / 86400, nicksvs.enforce_expiry == 86400 ? "" : "s");
	command_success_nodata(si, _("Default nickname enforce delay: %d second%s"),
		nicksvs.enforce_delay, nicksvs.enforce_delay == 1 ? "" : "s");
	command_success_nodata(si, _("Nickname enforce prefix: %s"), nicksvs.enforce_prefix);
	command_success_nodata(si, _("Use dynamic nickname enforcement (append digits to current nick): %s"), nicksvs.use_dynamic_enforce ? "Yes" : "No");
	if (nicksvs.use_dynamic_enforce)
		command_success_nodata(si, _("Maximum nick length: %u"), nicksvs.maxnicklength);
	command_success_nodata(si, _("Default user registration account flags: %s"),
		get_default_uflags());
	command_success_nodata(si, _("Maximum number of logins allowed per username: %d"), me.maxlogins);
	command_success_nodata(si, _("Maximum number of usernames that can be registered to one email address: %d"), me.maxusers);
	if (!nicksvs.no_nick_ownership)
		command_success_nodata(si, _("Maximum number of nicknames that one user can own: %d"), nicksvs.maxnicks);
	command_success_nodata(si, _("Minimum amount of time before account name changes are permitted: %u day%s"),
		nicksvs.acct_change_time, nicksvs.acct_change_time == 1 ? "" : "s");
	command_success_nodata(si, _("===================================="));
	command_success_nodata(si, _("\02%s Settings\02"), chansvs.nick);
	command_success_nodata(si, _("===================================="));
	command_success_nodata(si, _("Number of registered channels: %d"), cnt.mychan);
	command_success_nodata(si, _("ChanServ/BotServ joins channels with: %s"),
		chansvs.use_owner ? "owner" : (chansvs.use_admin ? "admin/protect" : "op"));
	command_success_nodata(si, _("Default channel founder ACL flags: %s %s"),
		chansvs.founder_flags ? chansvs.founder_flags : bitmask_to_flags(CA_ALLPRIVS & ca_all),
		chansvs.founder_flags ? "" : "(default)");
	command_success_nodata(si, _("Maximum number of channels that one user can own: %d"), chansvs.maxchans);
	command_success_nodata(si, _("Channel expiration time: %d day%s"),
		chansvs.expiry / 86400, chansvs.expiry == 86400 ? "" : "s");
	command_success_nodata(si, _("Default channel registration flags: %s"),
		get_default_cflags());
	command_success_nodata(si, _("Channel ACL flags that may only be set on accounts: %s"),
		bitmask_to_flags(chansvs.flags_req_acct));
	command_success_nodata(si, _("Leveled flags are enabled: %s"), chansvs.no_leveled_flags ? "no" : "yes");
	if (chansvs.min_non_wildcard_chars_host_acl)
		command_success_nodata(si, _("Minimum number of non-wildcard chars for hostmask-based channel ACLs: %u"), chansvs.min_non_wildcard_chars_host_acl);
	else
		command_success_nodata(si, _("Minimum number of non-wildcard chars for hostmask-based channel ACLs: Check Disabled"));
	if (chansvs.fantasy)
		command_success_nodata(si, _("Default channel fantasy trigger: %s"), chansvs.trigger);
	if (chansvs.maxchanacs)
		command_success_nodata(si, _("Maximum number of entries allowed in a channel access list: %d"), chansvs.maxchanacs);
	else
		command_success_nodata(si, _("Maximum number of entries allowed in a channel access list: Unlimited"));
	command_success_nodata(si, _("Maximum number of founders allowed per channel: %d"), chansvs.maxfounders);
	command_success_nodata(si, _("Users are permitted to self auto %s%sop/%svoice themselves without +f: %s"),
		ircd->uses_owner ? "owner/" : "", ircd->uses_protect ? "protect/" : "",
		ircd->uses_halfops ? "halfop/" : "", chansvs.permit_self_autoop ? "Yes" : "No");
	command_success_nodata(si, _("===================================="));

	hook_call_operserv_info(si);
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
