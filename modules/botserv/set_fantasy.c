/*
 * Copyright (c) 2005 William Pitcock <nenolod -at- nenolod.net>
 * Copyright (c) 2007 Jilles Tjoelker
 * Copyright (c) 2017 ChatLounge IRC Network Development Team
 *
 * Rights to this code are as documented in doc/LICENSE.
 *
 * Enable fantasy commands.
 *
 */

#include "atheme.h"
#include "uplink.h"

DECLARE_MODULE_V1
(
	"botserv/set_fantasy", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	VENDOR_STRING
);

mowgli_patricia_t **bs_set_cmdtree;

static void bs_set_fantasy_config_ready(void *unused);

static void bs_cmd_set_fantasy(sourceinfo_t *si, int parc, char *parv[]);

command_t bs_set_fantasy = { "FANTASY", N_("Enable fantasy commands."), AC_AUTHENTICATED, 2, bs_cmd_set_fantasy, { .path = "botserv/set_fantasy" } };

void _modinit(module_t *m)
{
	MODULE_TRY_REQUEST_SYMBOL(m, bs_set_cmdtree, "botserv/set_core", "bs_set_cmdtree");

	command_add(&bs_set_fantasy, *bs_set_cmdtree);

	hook_add_event("config_ready");
	hook_add_config_ready(bs_set_fantasy_config_ready);
}

void _moddeinit(module_unload_intent_t intent)
{
	command_delete(&bs_set_fantasy, *bs_set_cmdtree);

	hook_del_config_ready(bs_set_fantasy_config_ready);
}

static void bs_set_fantasy_config_ready(void *unused)
{
	if (chansvs.fantasy)
		bs_set_fantasy.access = NULL;
	else
		bs_set_fantasy.access = AC_DISABLED;
}

static void bs_cmd_set_fantasy(sourceinfo_t *si, int parc, char *parv[])
{
	char *channel = parv[0];
	char *option = parv[1];
	mychan_t *mc;
	metadata_t *md;

	if (parc < 2 || !channel || !option)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "SET FANTASY");
		command_fail(si, fault_needmoreparams, _("Syntax: SET <#channel> FANTASY {ON|OFF}"));
		return;
	}

	mc = mychan_find(channel);
	if (!mc)
	{
		command_fail(si, fault_nosuch_target, _("Channel \2%s\2 is not registered."), channel);
		return;
	}

	if (!chanacs_source_has_flag(mc, si, CA_SET))
	{
		command_fail(si, fault_noprivs, _("You are not authorized to perform this operation."));
		return;
	}

	if (!irccasecmp(option, "ON") || !irccasecmp(option, "1") || !irccasecmp(option, "TRUE"))
	{
		if ((md = metadata_find(mc, "private:botserv:bot-assigned")) != NULL)
			metadata_add(mc, "private:botserv:bot-handle-fantasy", md->value);

		logcommand(si, CMDLOG_SET, "SET:FANTASY:ON: \2%s\2", mc->name);

		command_success_nodata(si, _("Fantasy mode is now \2ON\2 on channel %s."), mc->name);
	}
	else if(!irccasecmp(option, "OFF") || !irccasecmp(option, "0") || !irccasecmp(option, "FALSE"))
	{
		metadata_delete(mc, "private:botserv:bot-handle-fantasy");
		logcommand(si, CMDLOG_SET, "SET:FANTASY:OFF: \2%s\2", mc->name);
		command_success_nodata(si, _("Fantasy mode is now \2OFF\2 on channel %s."), mc->name);
	}
	else
	{
		command_fail(si, fault_badparams, STR_INVALID_PARAMS, "SET FANTASY");
		command_fail(si, fault_badparams, _("Syntax: SET <#channel> FANTASY {ON|OFF}"));
	}
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
