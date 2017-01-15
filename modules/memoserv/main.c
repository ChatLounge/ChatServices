/*
 * Copyright (c) 2005 Atheme Development Group
 * Copyright (c) 2017 ChatLounge IRC Network Development Team
 *
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains the main() routine.
 *
 */

#include "atheme.h"
#include <limits.h>

DECLARE_MODULE_V1
(
	"memoserv/main", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net>"
);

static void on_user_identify(user_t *u);
static void on_user_away(user_t *u);

service_t *memosvs = NULL;
/*struct memoserv_conf *memosvs_conf;*/
unsigned int maxmemos;

void _modinit(module_t *m)
{
	hook_add_event("user_identify");
	hook_add_user_identify(on_user_identify);

	hook_add_event("user_away");
	hook_add_user_away(on_user_away);

	memosvs = service_add("memoserv", NULL);

	add_uint_conf_item("MAXMEMOS", &memosvs->conf_table, 0, &maxmemos, 1, INT_MAX, 30);
}

void _moddeinit(module_unload_intent_t intent)
{
        if (memosvs != NULL)
                service_delete(memosvs);
}

static void on_user_identify(user_t *u)
{
	myuser_t *mu = u->myuser;

	if (mu->memoct_new > 0)
	{
		notice(memosvs->me->nick, u->nick, ngettext(N_("You have %d new memo."),
						       N_("You have %d new memos."),
						       mu->memoct_new), mu->memoct_new);
		notice(memosvs->me->nick, u->nick, _("To read them, type /%s%s READ NEW"),
					ircd->uses_rcommand ? "" : "msg ", memosvs->disp);
	}
}

static void on_user_away(user_t *u)
{
	myuser_t *mu;
	mynick_t *mn;

	if (u->flags & UF_AWAY)
		return;
	mu = u->myuser;
	if (mu == NULL)
	{
		mn = mynick_find(u->nick);
		if (mn != NULL && myuser_access_verify(u, mn->owner))
			mu = mn->owner;
	}
	if (mu == NULL)
		return;
	if (mu->memoct_new > 0)
	{
		notice(memosvs->me->nick, u->nick, ngettext(N_("You have %d new memo."),
						       N_("You have %d new memos."),
						       mu->memoct_new), mu->memoct_new);
		notice(memosvs->me->nick, u->nick, _("To read them, type /%s%s READ NEW"),
					ircd->uses_rcommand ? "" : "msg ", memosvs->disp);
	}
}

/* send_user_memo:
 *
 *     Sends the target user a memo.  This function does no permissions
 * or rate limit checking, but will perform ignores checking.
 *
 * Inputs:
 *   sourceinfo_t *si - Source User
 *   myuser_t *target - Target User
 *   const char *memotext - Memo contents.
 *   bool verbose - Mention if the memos were successfully sent.
 *   unsigned int status - Memo type.  Use 0 (for regular) or MEMO_CHANNEL - See account.h .
 *   bool senduseremail - Send an e-mail if the user has that enabled.
 *       Normally you want to enable this.
 *
 * Output:
 *   Returns true if successful or if an ignore is triggered.  Otherwise, false.
 *
 * Side Effects:
 *   Sends a memo to the target user, if possible.
 */
bool send_user_memo(sourceinfo_t *si, myuser_t *target,
	const char *memotext, bool verbose, unsigned int status, bool senduseremail)
{
	mymemo_t *memo;
	mowgli_node_t *n, *o;
	user_t *tu;

	/* In theory this NULL check should always fail. */
	if (si->smu == NULL)
		return false;

	/* In case the function gets called with a NULL target or no memo text. */
	if (target == NULL)
		return false;

	if (memotext == NULL)
		return false;

	if (strlen(memotext) >= MEMOLEN)
		return false;

	if (*memotext == '\001')
	{
		if (verbose)
			command_fail(si, fault_badparams,
				_("Your memo contains invalid characters."));
		return false;
	}

	si->smu->memo_ratelimit_num++;
	si->smu->memo_ratelimit_time = CURRTIME;

	/* Does the recipient user permit memos? */
	if(target->flags & MU_NOMEMO)
	{
		if (verbose)
			command_fail(si, fault_noprivs,
				"\2%s\2 does not wish to receive memos.",
				entity(target)->name);
		return false;
	}

	if (target->memos.count >= maxmemos)
	{
		if (verbose)
			command_fail(si, fault_toomany, _("The inbox for \2%s\2 is full."),
				entity(target)->name);
		return false;
	}

	/* Ignores list check. */
	MOWGLI_ITER_FOREACH(n, target->memo_ignores.head)
	{
		mynick_t *mn;
		myuser_t *mu;

		if (nicksvs.no_nick_ownership)
			mu = myuser_find((const char *)n->data);
		else
		{
			mn = mynick_find((const char *)n->data);
			mu = mn != NULL ? mn->owner : NULL;
		}
		if (mu == si->smu)
		{
			logcommand(si, CMDLOG_SET,
				"failed SEND to \2%s\2 (on ignore list)",
				entity(target)->name);
			if (verbose)
				command_success_nodata(si,
					_("The memo has successfully been sent to: \2%s\2"),
					entity(target)->name);
			return true;
		}
	}

	logcommand(si, CMDLOG_SET, "SEND: to \2%s\2", entity(target)->name);

	/* Malloc and populate struct */
	memo = smalloc(sizeof(mymemo_t));
	memo->sent = CURRTIME;
	memo->status = status;
	mowgli_strlcpy(memo->sender, entity(si->smu)->name, NICKLEN);
	mowgli_strlcpy(memo->text, memotext, MEMOLEN);
	
	/* Create a linked list node and add to memos */
	n = mowgli_node_create();
	mowgli_node_add(memo, n, &target->memos);
	target->memoct_new++;

	/* Should we email this? */
	if (senduseremail && target->flags & MU_EMAILMEMOS)
		sendemail(si->su, target, EMAIL_MEMO, target->email, memo->text);

	/* Advise users they could possibly PM the target as he's online.
	 * This function only has the myuser_t so only show this if one of
	 * the logged in connections to the target has a nick matching the
	 * account name of the target.
	 */

	if (verbose && MOWGLI_LIST_LENGTH(&target->logins) > 0)
	{
		MOWGLI_ITER_FOREACH(o, target->logins.head)
		{
			tu = (user_t *)(o->data);

			if (!irccasecmp(tu->nick, entity(target)->name))
			{
				command_success_nodata(si,
					_("\2%s\2 is currently online, and you may talk directly, by sending a private message."),
					entity(target)->name);
				break;
			}
		}
	}

	/* Is the user online? If so, tell them about the new memo. */
	if (si->su == NULL || !irccasecmp(si->su->nick, entity(si->smu)->name))
		myuser_notice(memosvs->nick, target,
			"You have a new memo from %s (%zu).", entity(si->smu)->name,
			MOWGLI_LIST_LENGTH(&target->memos));
	else
		myuser_notice(memosvs->nick, target,
			"You have a new memo from %s (nick: %s) (%zu).",
			entity(si->smu)->name, si->su->nick,
			MOWGLI_LIST_LENGTH(&target->memos));
	myuser_notice(memosvs->nick, target, _("To read it, type: /%s%s READ %zu"),
		ircd->uses_rcommand ? "" : "msg ", memosvs->disp,
		MOWGLI_LIST_LENGTH(&target->memos));

	/* Tell user memo sent */
	command_success_nodata(si, _("The memo has been successfully sent to: \2%s\2"), entity(target)->name);

	return true;
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
