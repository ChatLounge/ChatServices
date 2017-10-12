/*
 * Copyright (c) 2009 Celestin, et al.
 *
 *
 * Rights to this code are as documented in doc/LICENSE.
 *
 * This file contains a BService INFO which can show
 * botserv settings on channel or bot.
 *
 */

#include "atheme.h"
#include "botserv.h"

DECLARE_MODULE_V1
(
	"botserv/info", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"Atheme Development Group <http://atheme.org/>"
);

static void bs_cmd_info(sourceinfo_t *si, int parc, char *parv[]);

command_t bs_info = { "INFO", N_("Allows you to see BotServ information about a channel or a bot."), AC_NONE, 1, bs_cmd_info, { .path = "botserv/info" } };

fn_botserv_bot_find *botserv_bot_find;
mowgli_list_t *bs_bots;

void _modinit(module_t *m)
{
	MODULE_TRY_REQUEST_SYMBOL(m, bs_bots, "botserv/main", "bs_bots");
	MODULE_TRY_REQUEST_SYMBOL(m, botserv_bot_find, "botserv/main", "botserv_bot_find");

	service_named_bind_command("botserv", &bs_info);
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("botserv", &bs_info);
}

/* ******************************************************************** */

static void bs_cmd_info(sourceinfo_t *si, int parc, char *parv[])
{
	char *dest = parv[0];
	mychan_t *mc = NULL;
	botserv_bot_t* bot = NULL;
	metadata_t *md;
	int additional = 0;
	unsigned int i, titlewidth;
	char buf[BUFSIZE], strfbuf[BUFSIZE], titleborder[BUFSIZE], *end;
	time_t registered;
	struct tm tm;
	mowgli_node_t *n;
	chanuser_t *cu;

	if (parc < 1 || parv[0] == NULL)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "INFO");
		command_fail(si, fault_needmoreparams, _("Syntax: INFO <#channel>"));
		command_fail(si, fault_needmoreparams, _("Syntax: INFO <botnick>"));
		return;
	}

	if (parv[0][0] == '#')
	{
		mc = mychan_find(dest);
	}
	else
	{
		bot = botserv_bot_find(dest);
	}

	if (bot != NULL)
	{
		/* "Information on bot " is 19 characters long.
		 */
		titlewidth = 19 + strlen(bot->nick);

		command_success_nodata(si, _("Information on bot \2%s\2"), bot->nick);

		i = 1;

		mowgli_strlcpy(titleborder, "-", sizeof titleborder);

		for (i; i < titlewidth; i++)
			mowgli_strlcat(titleborder, "-", sizeof titleborder);

		command_success_nodata(si, titleborder);

		command_success_nodata(si, _("Mask        : %s@%s"), bot->user, bot->host);
		command_success_nodata(si, _("Realname    : %s"), bot->real);
		registered = bot->registered;
		tm = *localtime(&registered);
		strftime(strfbuf, sizeof strfbuf, TIME_FORMAT, &tm);
		command_success_nodata(si, _("Created     : %s (%s ago)"), strfbuf, time_ago(registered));
		if (bot->private)
			command_success_nodata(si, _("Options     : PRIVATE"));
		else
			command_success_nodata(si, _("Options     : None"));
		command_success_nodata(si, _("Used on     : %zu channel%s"),
			MOWGLI_LIST_LENGTH(&bot->me->me->channels), MOWGLI_LIST_LENGTH(&bot->me->me->channels) == 1 ? "" : "s");
		if (has_priv(si, PRIV_CHAN_AUSPEX))
		{
			buf[0] = '\0';

			MOWGLI_ITER_FOREACH(n, bot->me->me->channels.head)
			{
				cu = (chanuser_t *)n->data;

				if (strlen(buf) > 79)
				{
					command_success_nodata(si, _("Channels    : %s"), buf);
					buf[0] = '\0';
				}

				if (buf[0])
					mowgli_strlcat(buf, " ", sizeof buf);
				mowgli_strlcat(buf, cu->chan->name, sizeof buf);
			}
			if (buf[0])
				command_success_nodata(si, _("Channels    : %s"), buf);
		}
	}
	else if (mc != NULL)
	{
		if (!(mc->flags & MC_PUBACL) && !chanacs_source_has_flag(mc, si, CA_ACLVIEW) && !has_priv(si, PRIV_CHAN_AUSPEX))
		{
			command_fail(si, fault_noprivs, _("You are not authorized to perform this operation."));
			return;
		}

		/* "Information on channel " is 23 characters long.
		 */
		titlewidth = 23 + strlen(mc->name);

		command_success_nodata(si, _("Information on channel \2%s\2"), mc->name);

		i = 1;

		mowgli_strlcpy(titleborder, "-", sizeof titleborder);

		for (i; i < titlewidth; i++)
			mowgli_strlcat(titleborder, "-", sizeof titleborder);

		command_success_nodata(si, titleborder);

		if ((md = metadata_find(mc, "private:botserv:bot-assigned")) != NULL)
			command_success_nodata(si, _("Bot nick    : %s"), md->value);
		else
			command_success_nodata(si, _("Bot nick    : <not assigned>"));
		end = buf;
		*end = '\0';
		if (metadata_find(mc, "private:botserv:bot-handle-fantasy"))
		{
			end += snprintf(end, sizeof(buf) - (end - buf), "%s%s", (additional) ? " " : "", "FANTASY");
			additional = 1;
		}
		if (metadata_find(mc, "private:botserv:no-bot"))
		{
			end += snprintf(end, sizeof(buf) - (end - buf), "%s%s", (additional) ? " " : "", "NOBOT");
			additional = 1;
		}
		if (metadata_find(mc, "private:botserv:saycaller"))
		{
			end += snprintf(end, sizeof(buf) - (end - buf), "%s%s", (additional) ? " " : "", "SAYCALLER");
			additional = 1;
		}
		command_success_nodata(si, _("Options     : %s"), (*buf) ? buf : "None");
	}
	else
	{
		command_fail(si, fault_nosuch_target, STR_INSUFFICIENT_PARAMS, "INFO");
		command_fail(si, fault_nosuch_target, _("Syntax: INFO <#channel>"));
		command_fail(si, fault_nosuch_target, _("Syntax: INFO <botnick>"));
	}
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */


