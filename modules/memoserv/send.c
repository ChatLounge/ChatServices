/*
 * Copyright (c) 2005 Atheme Development Group
 * Rights to this code are as documented in doc/LICENSE.
 *
 * This file contains code for the Memoserv SEND function
 *
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"memoserv/send", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"Atheme Development Group <http://www.atheme.org>"
);

static void ms_cmd_send(sourceinfo_t *si, int parc, char *parv[]);
static unsigned int *maxmemos;

static bool *(*send_user_memo)(sourceinfo_t *si, myuser_t *target,
	const char *memotext, bool verbose, unsigned int status) = NULL;

command_t ms_send = { "SEND", N_("Sends a memo to a user."),
                        AC_AUTHENTICATED, 2, ms_cmd_send, { .path = "memoserv/send" } };

void _modinit(module_t *m)
{
	service_named_bind_command("memoserv", &ms_send);
	MODULE_TRY_REQUEST_SYMBOL(m, maxmemos, "memoserv/main", "maxmemos");

	if (module_request("memoserv/main"))
		send_user_memo = module_locate_symbol("memoserv/main", "send_user_memo");
	else
	{
		m->mflags = MODTYPE_FAIL;
		return;
	}
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("memoserv", &ms_send);
}

static void ms_cmd_send(sourceinfo_t *si, int parc, char *parv[])
{
	/* misc structs etc */
	user_t *tu;
	myuser_t *tmu;
	mowgli_node_t *n;
	mymemo_t *memo;
	command_t *cmd;
	service_t *memoserv;

	/* Grab args */
	char *target = parv[0];
	char *m = parv[1];

	/* Arg validation */
	if (!target || !m)
	{
		command_fail(si, fault_needmoreparams,
			STR_INSUFFICIENT_PARAMS, "SEND");

		command_fail(si, fault_needmoreparams,
			"Syntax: SEND <user> <memo>");

		return;
	}

	if (si->smu->flags & MU_WAITAUTH)
	{
		command_fail(si, fault_notverified, _("You need to verify your email address before you may send memos."));
		return;
	}

	/* rate limit it -- jilles */
	if (CURRTIME - si->smu->memo_ratelimit_time > MEMO_MAX_TIME)
		si->smu->memo_ratelimit_num = 0;
	if (si->smu->memo_ratelimit_num > MEMO_MAX_NUM && !has_priv(si, PRIV_FLOOD))
	{
		command_fail(si, fault_toomany, _("You have used this command too many times; please wait a while and try again."));
		return;
	}

	/* Check for memo text length -- includes/common.h */
	if (strlen(m) >= MEMOLEN)
	{
		command_fail(si, fault_badparams,
			"Please make sure your memo is less than %d characters", MEMOLEN);

		return;
	}

	/* Check to make sure the memo doesn't contain hostile CTCP responses.
	 * realistically, we'll probably want to check the _entire_ message for this... --nenolod
	 */
	if (*m == '\001')
	{
		command_fail(si, fault_badparams, _("Your memo contains invalid characters."));
		return;
	}

	memoserv = service_find("memoserv");
	if (memoserv == NULL)
		memoserv = si->service;

	if (*target != '#' && *target != '!')
	{
		/* See if target is valid */
		if (!(tmu = myuser_find_ext(target)))
		{
			command_fail(si, fault_nosuch_target,
				"\2%s\2 is not registered.", target);
			return;
		}

		si->smu->memo_ratelimit_num++;
		si->smu->memo_ratelimit_time = CURRTIME;

		/* Does the user allow memos? --pfish */
		if (tmu->flags & MU_NOMEMO)
		{
			command_fail(si, fault_noprivs,
				"\2%s\2 does not wish to receive memos.", target);
			return;
		}

		/* Check to make sure target inbox not full */
		if (tmu->memos.count >= *maxmemos)
		{
			command_fail(si, fault_toomany, _("%s's inbox is full"), target);
			logcommand(si, CMDLOG_SET, "failed SEND to \2%s\2 (target inbox full)", entity(tmu)->name);
			return;
		}

		if (send_user_memo(si, tmu, m, true, 0))
			logcommand(si, CMDLOG_SET, "SEND: to \2%s\2", entity(tmu)->name);
	}
	else if (*target == '#')
	{
		cmd = command_find(memoserv->commands, "SENDOPS");
		if (cmd != NULL)
			command_exec(memoserv, si, cmd, parc, parv);
		else
			command_fail(si, fault_nosuch_target, _("Channel memos are administratively disabled."));
	}
	else
	{
		cmd = command_find(memoserv->commands, "SENDGROUP");
		if (cmd != NULL)
			command_exec(memoserv, si, cmd, parc, parv);
		else
			command_fail(si, fault_nosuch_target, _("Group memos are administratively disabled."));
	}

	return;
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
