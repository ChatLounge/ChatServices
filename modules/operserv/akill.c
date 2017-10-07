/*
 * Copyright (c) 2003-2004 E. Will et al.
 * Copyright (c) 2005-2006 Atheme Development Group
 * Copyright (c) 2016-2017 ChatLounge IRC Network Development Team
 *
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains functionality which implements
 * the OperServ AKILL command.
 *
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"operserv/akill", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net>"
);

static void os_akill_newuser(hook_user_nick_t *data);

static void os_cmd_akill(sourceinfo_t *si, int parc, char *parv[]);
static void os_cmd_akill_add(sourceinfo_t *si, int parc, char *parv[]);
static void os_cmd_akill_del(sourceinfo_t *si, int parc, char *parv[]);
static void os_cmd_akill_list(sourceinfo_t *si, int parc, char *parv[]);
static void os_cmd_akill_sync(sourceinfo_t *si, int parc, char *parv[]);


command_t os_akill = { "AKILL", N_("Manages network bans."), PRIV_AKILL, 3, os_cmd_akill, { .path = "oservice/akill" } };

command_t os_akill_add = { "ADD", N_("Adds a network ban"), AC_NONE, 2, os_cmd_akill_add, { .path = "" } };
command_t os_akill_del = { "DEL", N_("Deletes a network ban"), AC_NONE, 1, os_cmd_akill_del, { .path = "" } };
command_t os_akill_list = { "LIST", N_("Lists all network bans"), AC_NONE, 1, os_cmd_akill_list, { .path = "" } };
command_t os_akill_sync = { "SYNC", N_("Synchronises network bans to servers"), AC_NONE, 0, os_cmd_akill_sync, { .path = "" } };

mowgli_patricia_t *os_akill_cmds;

void _modinit(module_t *m)
{
        service_named_bind_command("operserv", &os_akill);

	os_akill_cmds = mowgli_patricia_create(strcasecanon);

	/* Add sub-commands */
	command_add(&os_akill_add, os_akill_cmds);
	command_add(&os_akill_del, os_akill_cmds);
	command_add(&os_akill_list, os_akill_cmds);
	command_add(&os_akill_sync, os_akill_cmds);

	hook_add_event("user_add");
	hook_add_user_add(os_akill_newuser);
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("operserv", &os_akill);

	/* Delete sub-commands */
	command_delete(&os_akill_add, os_akill_cmds);
	command_delete(&os_akill_del, os_akill_cmds);
	command_delete(&os_akill_list, os_akill_cmds);
	command_delete(&os_akill_sync, os_akill_cmds);

	hook_del_user_add(os_akill_newuser);

	mowgli_patricia_destroy(os_akill_cmds, NULL, NULL);
}

static void os_akill_newuser(hook_user_nick_t *data)
{
	user_t *u = data->u;
	kline_t *k;

	/* If the user has been killed, don't do anything. */
	if (!u)
		return;

	if (is_internal_client(u))
		return;
	k = kline_find_user(u);
	if (k != NULL)
	{
		/* Server didn't have that kline, send it again.
		 * To ensure kline exempt works on akills too, do
		 * not send a KILL. -- jilles */
		char reason[BUFSIZE];
		snprintf(reason, sizeof(reason), "[#%lu] %s", k->number, k->reason);
		if (! (u->flags & UF_KLINESENT)) {
			kline_sts("*", k->user, k->host, k->duration ? k->expires - CURRTIME : 0, reason);
			u->flags |= UF_KLINESENT;
		}
	}
}

static void os_cmd_akill(sourceinfo_t *si, int parc, char *parv[])
{
	/* Grab args */
	char *cmd = parv[0];
        command_t *c;

	/* Bad/missing arg */
	if (!cmd)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "AKILL");
		command_fail(si, fault_needmoreparams, _("Syntax: AKILL ADD|DEL|LIST"));
		return;
	}

	c = command_find(os_akill_cmds, cmd);
	if (c == NULL)
	{
		command_fail(si, fault_badparams, _("Invalid command. Use \2/%s%s help\2 for a command listing."), (ircd->uses_rcommand == false) ? "msg " : "", si->service->disp);
		return;
	}

	command_exec(si->service, si, c, parc - 1, parv + 1);
}

static void os_cmd_akill_add(sourceinfo_t *si, int parc, char *parv[])
{
	user_t *u;
	char *target = parv[0];
	char *token = strtok(parv[1], " ");
	char star[] = "*";
	const char *kuser, *khost;
	char *treason, reason[BUFSIZE];
	long duration;
	char *s;
	kline_t *k, *kl, *kln;
	mowgli_node_t *n, *mkn;

	if (!target || !token)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "AKILL ADD");
		command_fail(si, fault_needmoreparams, _("Syntax: AKILL ADD <nick|hostmask> [!P|!T <minutes>] <reason>"));
		return;
	}

	if (!strcasecmp(token, "!P"))
	{
		duration = 0;
		treason = strtok(NULL, "");

		if (treason)
			mowgli_strlcpy(reason, treason, BUFSIZE);
		else
			mowgli_strlcpy(reason, "No reason given", BUFSIZE);
	}
	else if (!strcasecmp(token, "!T"))
	{
		s = strtok(NULL, " ");
		treason = strtok(NULL, "");
		if (treason)
			mowgli_strlcpy(reason, treason, BUFSIZE);
		else
			mowgli_strlcpy(reason, "No reason given", BUFSIZE);
		if (s)
		{
			duration = (atol(s) * 60);
			while (isdigit((unsigned char)*s))
				s++;
			if (*s == 'h' || *s == 'H')
				duration *= 60;
			else if (*s == 'd' || *s == 'D')
				duration *= 1440;
			else if (*s == 'w' || *s == 'W')
				duration *= 10080;
			else if (*s == '\0')
				;
			else
				duration = 0;
			if (duration == 0)
			{
				command_fail(si, fault_badparams, _("Invalid duration given."));
				command_fail(si, fault_badparams, _("Syntax: AKILL ADD <nick|hostmask> [!P|!T <minutes>] <reason>"));
				return;
			}
		}
		else {
			command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "AKILL ADD");
			command_fail(si, fault_needmoreparams, _("Syntax: AKILL ADD <nick|hostmask> [!P|!T <minutes>] <reason>"));
			return;
		}

	}
	else
	{
		duration = config_options.kline_time;
		mowgli_strlcpy(reason, token, BUFSIZE);
		treason = strtok(NULL, "");

		if (treason)
		{
			mowgli_strlcat(reason, " ", BUFSIZE);
			mowgli_strlcat(reason, treason, BUFSIZE);
		}
	}

	if (strchr(target,'!'))
	{
		command_fail(si, fault_badparams, _("Invalid character '%c' in user@host."), '!');
		return;
	}

	if (!(strchr(target, '@')))
	{
		if (!(u = user_find_named(target)))
		{
			command_fail(si, fault_nosuch_target, _("\2%s\2 is not on IRC."), target);
			return;
		}

		if (is_internal_client(u) || u == si->su)
			return;

		kuser = star;
		khost = u->host;
	}
	else
	{
		kuser = collapse(strtok(target, "@"));
		khost = collapse(strtok(NULL, ""));

		if (!kuser || !khost)
		{
			command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "AKILL ADD");
			command_fail(si, fault_needmoreparams, _("Syntax: AKILL ADD <user>@<host> [options] <reason>"));
			return;
		}

		if (strchr(khost,'@'))
		{
			command_fail(si, fault_badparams, _("Too many '%c' in user@host."), '@');
			command_fail(si, fault_badparams, _("Syntax: AKILL ADD <user>@<host> [options] <reason>"));
			return;
		}

		if (config_options.kline_non_wildcard_chars > 0 &&
			check_not_enough_non_wildcard_chars(target, (int) config_options.kline_non_wildcard_chars, 0) &&
			(strchr(kuser, '*') || strchr(kuser, '?')) &&
			!has_priv(si, PRIV_AKILL_ANYMASK))
		{
			command_fail(si, fault_badparams, _("Invalid user@host: \2%s@%s\2. At least %u non-wildcard characters are required."), kuser, khost, config_options.kline_non_wildcard_chars);
			return;
		}
	}

	if (!strcmp(kuser, "*"))
	{
		bool unsafe = false;
		char *p;

		if (!match(khost, "127.0.0.1") || !match_ips(khost, "127.0.0.1"))
			unsafe = true;
		else if (me.vhost != NULL && (!match(khost, me.vhost) || !match_ips(khost, me.vhost)))
			unsafe = true;
		else if ((p = strrchr(khost, '/')) != NULL && IsDigit(p[1]) && atoi(p + 1) < 4)
			unsafe = true;
		if (unsafe)
		{
			command_fail(si, fault_badparams, _("Invalid user@host: \2%s@%s\2. This mask is unsafe."), kuser, khost);
			logcommand(si, CMDLOG_ADMIN, "failed AKILL ADD \2%s@%s\2 (unsafe mask)", kuser, khost);
			return;
		}
	}

	if (kline_find(kuser, khost))
	{
		command_fail(si, fault_nochange, _("AKILL \2%s@%s\2 is already matched in the database."), kuser, khost);
		return;
	}

	/* Code for searching for more specific k-lines that will become irrelevant if a broader k-line is added.
	 * e.g. suppose the k-lines BadGuy@*.lamerz.net and *@kiddiez.lamerz.net are set.  If a k-line on
	 * *@*.lamerz.net is set, this will remove BadGuy@*.lamerz.net and *@kiddiez.lamerz.net as they are now
	 * redundant. - Ben
	 */
	bool showntitle = false;
	long unsigned int matchcount = 0;
	mowgli_list_t matchingklns = { NULL, NULL, 0 };

	if (!config_options.kline_do_not_remove_more_specific)
	{
		MOWGLI_ITER_FOREACH(n, klnlist.head)
		{
			kl = (kline_t *)n->data;

			if (!match(kuser, kl->user) && (!match(khost, kl->host) || !match_ips(khost, kl->host)))
			{
				/* If the k-line is set to expire later than the k-line being set, don't remove it. */
				if (duration && !kl->duration)
					continue;

				if (duration && (kl->expires > CURRTIME + duration))
					continue;

				if (!showntitle)
				{
					command_success_nodata(si, _("The following AKILLs are now redundant and removed:"));
					command_success_nodata(si, "==================================================");
					showntitle = true;
				}

				matchcount++;

				command_success_nodata(si, _("\2Removed:\2 %u: \2%s@%s\2 was set by \2%s\2 (%s)"), kl->number, kl->user, kl->host, kl->setby, kl->reason);
				verbose_wallops("\2%s\2 is \2removing\2 an \2AKILL\2 for \2%s@%s\2 -- reason: \2%s\2",
					get_oper_name(si), kl->user, kl->host, kl->reason);

				logcommand(si, CMDLOG_ADMIN, "AKILL:DEL: \2%s@%s\2", kl->user, kl->host);

				/* Add the k-line to the list of k-lines to remove, if any. */
				mowgli_node_add(kl, mowgli_node_create(), &matchingklns);
			}
		}
	}

	/* If applicable, actually remove the more specific k-lines and show the total k-lines removed. */
	if (showntitle)
	{
		MOWGLI_ITER_FOREACH(mkn, matchingklns.head)
		{
			kln = (kline_t *)mkn->data;

			kline_delete(kln);
		}

		command_success_nodata(si, _("A total of \2%u\2 matching, more specific AKILL%s been removed."), matchcount, matchcount == 1 ? " has" : "s have");
	}

	//mowgli_list_free(matchingklns);

	k = kline_add(kuser, khost, reason, duration, get_storage_oper_name(si));

	if (duration)
		command_success_nodata(si, _("Timed AKILL on \2%s@%s\2 was successfully added and will expire in %s."), k->user, k->host, timediff(duration));
	else
		command_success_nodata(si, _("AKILL on \2%s@%s\2 was successfully added."), k->user, k->host);

	verbose_wallops("\2%s\2 is \2adding\2 an \2AKILL\2 for \2%s@%s\2 -- reason: \2%s\2", get_oper_name(si), k->user, k->host,
		k->reason);

	if (duration)
		logcommand(si, CMDLOG_ADMIN, "AKILL:ADD: \2%s@%s\2 (reason: \2%s\2) (duration: \2%s\2)", k->user, k->host, k->reason, timediff(k->duration));
	else
		logcommand(si, CMDLOG_ADMIN, "AKILL:ADD: \2%s@%s\2 (reason: \2%s\2) (duration: \2Permanent\2)", k->user, k->host, k->reason);
}

static void os_cmd_akill_del(sourceinfo_t *si, int parc, char *parv[])
{
	char *target = parv[0];
	char *userbuf, *hostbuf;
	unsigned int number;
	char *s;
	kline_t *k;

	if (!target)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "AKILL DEL");
		command_fail(si, fault_needmoreparams, _("Syntax: AKILL DEL <hostmask>"));
		return;
	}

	if (strchr(target, ','))
	{
		unsigned int start = 0, end = 0, i;
		char t[16];

		s = strtok(target, ",");

		do
		{
			if (strchr(s, ':'))
			{
				for (i = 0; *s != ':'; s++, i++)
					t[i] = *s;

				t[++i] = '\0';
				start = atoi(t);

				s++;	/* skip past the : */

				for (i = 0; *s != '\0'; s++, i++)
					t[i] = *s;

				t[++i] = '\0';
				end = atoi(t);

				for (i = start; i <= end; i++)
				{
					if (!(k = kline_find_num(i)))
					{
						command_fail(si, fault_nosuch_target, _("No such AKILL with number \2%d\2."), i);
						continue;
					}

					command_success_nodata(si, _("AKILL on \2%s@%s\2 has been successfully removed."), k->user, k->host);
					verbose_wallops("\2%s\2 is \2removing\2 an \2AKILL\2 for \2%s@%s\2 -- reason: \2%s\2",
						get_oper_name(si), k->user, k->host, k->reason);

					logcommand(si, CMDLOG_ADMIN, "AKILL:DEL: \2%s@%s\2", k->user, k->host);
					kline_delete(k);
				}

				continue;
			}

			number = atoi(s);

			if (!(k = kline_find_num(number)))
			{
				command_fail(si, fault_nosuch_target, _("No such AKILL with number \2%d\2."), number);
				return;
			}

			command_success_nodata(si, _("AKILL on \2%s@%s\2 has been successfully removed."), k->user, k->host);
			verbose_wallops("\2%s\2 is \2removing\2 an \2AKILL\2 for \2%s@%s\2 -- reason: \2%s\2",
				get_oper_name(si), k->user, k->host, k->reason);

			logcommand(si, CMDLOG_ADMIN, "AKILL:DEL: \2%s@%s\2", k->user, k->host);
			kline_delete(k);
		} while ((s = strtok(NULL, ",")));

		return;
	}

	if (!strchr(target, '@'))
	{
		unsigned int start = 0, end = 0, i;
		char t[16];

		if (strchr(target, ':'))
		{
			for (i = 0; *target != ':'; target++, i++)
				t[i] = *target;

			t[++i] = '\0';
			start = atoi(t);

			target++;	/* skip past the : */

			for (i = 0; *target != '\0'; target++, i++)
				t[i] = *target;

			t[++i] = '\0';
			end = atoi(t);

			for (i = start; i <= end; i++)
			{
				if (!(k = kline_find_num(i)))
				{
					command_fail(si, fault_nosuch_target, _("No such AKILL with number \2%d\2."), i);
					continue;
				}

				command_success_nodata(si, _("AKILL on \2%s@%s\2 has been successfully removed."), k->user, k->host);
				verbose_wallops("\2%s\2 is \2removing\2 an \2AKILL\2 for \2%s@%s\2 -- reason: \2%s\2",
					get_oper_name(si), k->user, k->host, k->reason);

				logcommand(si, CMDLOG_ADMIN, "AKILL:DEL: \2%s@%s\2", k->user, k->host);
				kline_delete(k);
			}

			return;
		}

		number = atoi(target);

		if (!(k = kline_find_num(number)))
		{
			command_fail(si, fault_nosuch_target, _("No such AKILL with number \2%d\2."), number);
			return;
		}

		command_success_nodata(si, _("AKILL on \2%s@%s\2 has been successfully removed."), k->user, k->host);

		verbose_wallops("\2%s\2 is \2removing\2 an \2AKILL\2 for \2%s@%s\2 -- reason: \2%s\2",
			get_oper_name(si), k->user, k->host, k->reason);

		logcommand(si, CMDLOG_ADMIN, "AKILL:DEL: \2%s@%s\2", k->user, k->host);
		kline_delete(k);
		return;
	}

	userbuf = strtok(target, "@");
	hostbuf = strtok(NULL, "");

	if (!(k = kline_find(userbuf, hostbuf)))
	{
		command_fail(si, fault_nosuch_target, _("No such AKILL: \2%s@%s\2."), userbuf, hostbuf);
		return;
	}

	command_success_nodata(si, _("AKILL on \2%s@%s\2 has been successfully removed."), userbuf, hostbuf);

	verbose_wallops("\2%s\2 is \2removing\2 an \2AKILL\2 for \2%s@%s\2 -- reason: \2%s\2",
		get_oper_name(si), k->user, k->host, k->reason);

	logcommand(si, CMDLOG_ADMIN, "AKILL:DEL: \2%s@%s\2", k->user, k->host);
	kline_delete(k);
}

static void os_cmd_akill_list(sourceinfo_t *si, int parc, char *parv[])
{
	char *param = parv[0];
	char *user = NULL, *host = NULL;
	unsigned long num = 0;
	bool full = false;
	mowgli_node_t *n;
	kline_t *k;

	if (param != NULL)
	{
		if (!strcasecmp(param, "FULL"))
			full = true;
		else if ((host = strchr(param, '@')) != NULL)
		{
			*host++ = '\0';
			user = param;
			full = true;
		}
		else if (strchr(param, '.') || strchr(param, ':'))
		{
			host = param;
			full = true;
		}
		else if (isdigit((unsigned char)param[0]) &&
				(num = strtoul(param, NULL, 10)) != 0)
			full = true;
		else
		{
			command_fail(si, fault_badparams, STR_INVALID_PARAMS, "AKILL LIST");
			return;
		}
	}

	if (user || host || num)
		command_success_nodata(si, _("AKILL list matching given criteria (with reasons):"));
	else if (full)
		command_success_nodata(si, _("AKILL list (with reasons):"));
	else
		command_success_nodata(si, _("AKILL list:"));

	MOWGLI_ITER_FOREACH(n, klnlist.head)
	{
		k = (kline_t *)n->data;

		if (num != 0 && k->number != num)
			continue;
		if (user != NULL && match(k->user, user))
			continue;
		if (host != NULL && match(k->host, host) && match_ips(k->host, host))
			continue;

		if (k->duration && full)
			command_success_nodata(si, _("%lu: %s@%s - by \2%s\2 - expires in \2%s\2 - (%s)"), k->number, k->user, k->host, k->setby, timediff(k->expires > CURRTIME ? k->expires - CURRTIME : 0), k->reason);
		else if (k->duration && !full)
			command_success_nodata(si, _("%lu: %s@%s - by \2%s\2 - expires in \2%s\2"), k->number, k->user, k->host, k->setby, timediff(k->expires > CURRTIME ? k->expires - CURRTIME : 0));
		else if (!k->duration && full)
			command_success_nodata(si, _("%lu: %s@%s - by \2%s\2 - \2permanent\2 - (%s)"), k->number, k->user, k->host, k->setby, k->reason);
		else
			command_success_nodata(si, _("%lu: %s@%s - by \2%s\2 - \2permanent\2"), k->number, k->user, k->host, k->setby);
	}

	if (user || host || num)
		command_success_nodata(si, _("End of AKILL list."));
	else
		command_success_nodata(si, _("Total of \2%zu\2 %s in AKILL list."), klnlist.count, (klnlist.count == 1) ? "entry" : "entries");
	if (user || host)
		logcommand(si, CMDLOG_GET, "AKILL:LIST: \2%s@%s\2", user ? user : "*", host ? host : "*");
	else if (num)
		logcommand(si, CMDLOG_GET, "AKILL:LIST: \2%lu\2", num);
	else
		logcommand(si, CMDLOG_GET, "AKILL:LIST: \2%s\2", full ? " FULL" : "");
}

static void os_cmd_akill_sync(sourceinfo_t *si, int parc, char *parv[])
{
	mowgli_node_t *n;
	kline_t *k;

	logcommand(si, CMDLOG_DO, "AKILL:SYNC");

	MOWGLI_ITER_FOREACH(n, klnlist.head)
	{
		k = (kline_t *)n->data;


		char reason[BUFSIZE];
		snprintf(reason, sizeof(reason), "[#%lu] %s", k->number, k->reason);

		if (k->duration == 0)
			kline_sts("*", k->user, k->host, 0, reason);
		else if (k->expires > CURRTIME)
			kline_sts("*", k->user, k->host, k->expires - CURRTIME, reason);
	}

	command_success_nodata(si, _("AKILL list synchronized to servers."));
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
