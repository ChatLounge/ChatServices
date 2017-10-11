/*
 * Copyright (c) 2016 Austin Ellis
 * Copyright (c) 2017 ChatLounge IRC Network Development Team
 *
 * Rights to this code are as documented in doc/LICENSE.
 *
 * Enable say/act caller ID.
 *
 */

#include "atheme.h"
#include "uplink.h"

DECLARE_MODULE_V1
(
	"botserv/set_saycaller", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <https://www.chatlounge.net>"
);

void (*notify_channel_set_change)(sourceinfo_t *si, myuser_t *tmu, mychan_t *mc,
	const char *settingname, const char *setting) = NULL;

mowgli_patricia_t **bs_set_cmdtree;

static void bs_cmd_set_saycaller(sourceinfo_t *si, int parc, char *parv[]);

command_t bs_set_saycaller = { "SAYCALLER", N_("Enable Caller-ID on BotServ actions or messages."), AC_AUTHENTICATED, 2, bs_cmd_set_saycaller, { .path = "botserv/set_saycaller" } };

void _modinit(module_t *m)
{
	MODULE_TRY_REQUEST_SYMBOL(m, bs_set_cmdtree, "botserv/set_core", "bs_set_cmdtree");

	if (module_request("chanserv/main"))
		notify_channel_set_change = module_locate_symbol("chanserv/main", "notify_channel_set_change");

	command_add(&bs_set_saycaller, *bs_set_cmdtree);
}

void _moddeinit(module_unload_intent_t intent)
{
	command_delete(&bs_set_saycaller, *bs_set_cmdtree);

}

static void bs_cmd_set_saycaller(sourceinfo_t *si, int parc, char *parv[])
{
	char *channel = parv[0];
	char *option = parv[1];
	mychan_t *mc;
	metadata_t *md;

	if (parc < 2 || !channel || !option)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "SET SAYCALLER");
		command_fail(si, fault_needmoreparams, _("Syntax: SET <#channel> SAYCALLER {ON|OFF}"));
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
	
	if (metadata_find(mc, "private:frozen:freezer"))
	{
		command_fail(si, fault_noprivs, _("\2%s\2 is frozen."), mc->name);
		return;
	}

	if (!irccasecmp(option, "ON") || !irccasecmp(option, "1") || !irccasecmp(option, "TRUE"))
	{
		if ((md = metadata_find(mc, "private:botserv:bot-assigned")) != NULL)
		{
			if (metadata_find(mc, "private:botserv:saycaller"))
			{
				command_fail(si, fault_nochange, _("The \2%s\2 flag is already set for channel: \2%s\2"), "SAYCALLER", mc->name);
				return;
			}
			else
				metadata_add(mc, "private:botserv:saycaller", "ON");
		}
		else
		{
			command_fail(si, fault_noprivs, _("The channel \2%s\2 does not have a bot assigned to it.  You may not enable this option until you assign a bot to the channel."));
			return;
		}

		logcommand(si, CMDLOG_SET, "SET:SAYCALLER:ON: \2%s\2", mc->name);
		verbose(mc, _("\2%s\2 enabled the SAYCALLER flag."), get_source_name(si));
		command_success_nodata(si, _("Say Caller is now \2ON\2 on channel %s."), mc->name);

		notify_channel_set_change(si, si->smu, mc, "SAYCALLER", "ON");
	}
	else if(!irccasecmp(option, "OFF") || !irccasecmp(option, "0") || !irccasecmp(option, "FALSE"))
	{
		if (!metadata_find(mc, "private:botserv:saycaller"))
		{
			command_fail(si, fault_nochange, _("The \2%s\2 flag is not set for channel: \2%s\2"), "SAYCALLER", mc->name);
			return;
		}

		metadata_delete(mc, "private:botserv:saycaller");
		logcommand(si, CMDLOG_SET, "SET:SAYCALLER:OFF: \2%s\2", mc->name);
		command_success_nodata(si, _("Say Caller is now \2OFF\2 on channel %s."), mc->name);
		verbose(mc, _("\2%s\2 disabled the SAYCALLER flag."), get_source_name(si));
		notify_channel_set_change(si, si->smu, mc, "SAYCALLER", "OFF");
	}
	else
	{
		command_fail(si, fault_badparams, STR_INVALID_PARAMS, "SET SAYCALLER");
		command_fail(si, fault_badparams, _("Syntax: SET <#channel> SAYCALLER {ON|OFF}"));
	}
}