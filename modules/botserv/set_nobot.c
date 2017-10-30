/*
 * Copyright (c) 2005 William Pitcock <nenolod -at- nenolod.net>
 * Copyright (c) 2007 Jilles Tjoelker
 * Copyright (c) 2017 ChatLounge IRC Network Development Team
 *
 * Rights to this code are as documented in doc/LICENSE.
 *
 * Prevent a bot from being assigned to a channel.
 *
 */

#include "atheme.h"
#include "uplink.h"

DECLARE_MODULE_V1
(
	"botserv/set_nobot", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	VENDOR_STRING
);

void (*notify_channel_set_change)(sourceinfo_t *si, myuser_t *tmu, mychan_t *mc,
	const char *settingname, const char *setting) = NULL;

mowgli_patricia_t **bs_set_cmdtree;

static void bs_cmd_set_nobot(sourceinfo_t *si, int parc, char *parv[]);

command_t bs_set_nobot = { "NOBOT", N_("Prevent a bot from being assigned to a channel."), PRIV_CHAN_ADMIN, 2, bs_cmd_set_nobot, { .path = "botserv/set_nobot" } };

void _modinit(module_t *m)
{
	MODULE_TRY_REQUEST_SYMBOL(m, bs_set_cmdtree, "botserv/set_core", "bs_set_cmdtree");

	if (module_request("chanserv/main"))
		notify_channel_set_change = module_locate_symbol("chanserv/main", "notify_channel_set_change");

	command_add(&bs_set_nobot, *bs_set_cmdtree);
}

void _moddeinit(module_unload_intent_t intent)
{
	command_delete(&bs_set_nobot, *bs_set_cmdtree);
}

static void bs_cmd_set_nobot(sourceinfo_t *si, int parc, char *parv[])
{
	char *channel = parv[0];
	char *option = parv[1];
	mychan_t *mc;
	metadata_t *md;

	if (parc < 2 || !channel || !option)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "SET NOBOT");
		command_fail(si, fault_needmoreparams, _("Syntax: SET <#channel> NOBOT {ON|OFF}"));
		return;
	}

	mc = mychan_find(channel);
	if (!mc)
	{
		command_fail(si, fault_nosuch_target, _("Channel \2%s\2 is not registered."), channel);
		return;
	}

	if (!si->smu)
	{
		command_fail(si, fault_noprivs, _("You are not logged in."));
		return;
	}

	if (!has_priv(si, PRIV_CHAN_ADMIN))
	{
		command_fail(si, fault_noprivs, _("You are not authorized to perform this operation."));
		return;
	}

	if (!irccasecmp(option, "ON") || !irccasecmp(option, "1") || !irccasecmp(option, "TRUE"))
	{
		if (metadata_find(mc, "private:botserv:no-bot"))
		{
			command_fail(si, fault_nochange, _("The \2%s\2 flag is already set for channel: \2%s\2"), "NOBOT", mc->name);
			return;
		}

		metadata_add(mc, "private:botserv:no-bot", "ON");
		if ((md = metadata_find(mc, "private:botserv:bot-assigned")) != NULL)
		{
			if (mc->flags & MC_GUARD &&
					(!config_options.leave_chans ||
					 (mc->chan != NULL &&
					  MOWGLI_LIST_LENGTH(&mc->chan->members) > 1)))
				join(mc->name, chansvs.nick);
			part(mc->name, md->value);
			metadata_delete(mc, "private:botserv:bot-assigned");
			metadata_delete(mc, "private:botserv:bot-handle-fantasy");
		}

		if (!metadata_find(mc, "disable_fantasy"))
		{
			metadata_add(mc, "disable_fantasy", "ON");
			logcommand(si, CMDLOG_SET, "SET:FANTASY:OFF: \2%s\2", mc->name);
			verbose(mc, _("\2%s\2 disabled the FANTASY flag."), get_source_name(si));
			command_success_nodata(si, _("The \2%s\2 flag has been removed for channel \2%s\2."), "FANTASY", mc->name);
			notify_channel_set_change(si, si->smu, mc, "FANTASY", "OFF");
		}

		if (metadata_find(mc, "private:botserv:saycaller"))
		{
			metadata_delete(mc, "private:botserv:saycaller");
			logcommand(si, CMDLOG_SET, "SET:SAYCALLER:OFF: \2%s\2", mc->name);
			command_success_nodata(si, _("Say Caller is now \2OFF\2 on channel %s."), mc->name);
			verbose(mc, _("\2%s\2 disabled the SAYCALLER flag."), get_source_name(si));
			notify_channel_set_change(si, si->smu, mc, "SAYCALLER", "OFF");
		}

		logcommand(si, CMDLOG_SET, "SET:NOBOT:ON: \2%s\2", mc->name);
		verbose(mc, _("\2%s\2 enabled the NOBOT flag."), get_source_name(si));
		command_success_nodata(si, _("No Bot mode is now \2ON\2 on channel %s."), mc->name);
		notify_channel_set_change(si, si->smu, mc, "NOBOT", "ON");
	}
	else if(!irccasecmp(option, "OFF") || !irccasecmp(option, "0") || !irccasecmp(option, "FALSE"))
	{
		if (!metadata_find(mc, "private:botserv:no-bot"))
		{
			command_fail(si, fault_nochange, _("The \2%s\2 flag is not set for channel: \2%s\2"), "NOBOT", mc->name);
			return;
		}

		metadata_delete(mc, "private:botserv:no-bot");
		logcommand(si, CMDLOG_SET, "SET:NOBOT:OFF: \2%s\2", mc->name);
		verbose(mc, _("\2%s\2 disabled the NOBOT flag."), get_source_name(si));
		command_success_nodata(si, _("No Bot mode is now \2OFF\2 on channel %s."), mc->name);
		notify_channel_set_change(si, si->smu, mc, "NOBOT", "OFF");
	}
	else
	{
		command_fail(si, fault_badparams, STR_INVALID_PARAMS, "SET NOBOT");
		command_fail(si, fault_badparams, _("Syntax: SET <#channel> NOBOT {ON|OFF}"));
	}
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
