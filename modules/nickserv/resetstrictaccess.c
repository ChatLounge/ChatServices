/* Copyright (c) 2017 ChatLounge IRC Network Development Team
 *
 *     This file contains the functions to recover, and send an e-mail
 * to a user who has the STRICTACCESS setting enabled, to disable it in
 * the event the user is locked out of his account as a result of losing
 * access to all connections defined in the NickServ ACCESS list.
 *
 *     Based loosely off of sendpass and sendpass_user.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"nickserv/resetstrictaccess", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	VENDOR_STRING
);

enum specialoperation
{
	op_none,
	op_force,
	op_clear
};

static void ns_cmd_resetstrictaccess(sourceinfo_t *si, int parc, char *parv[]);

command_t ns_resetstrictaccess = { "RESETSTRICTACCESS",
	N_("Send an e-mail to recover access if locked out from STRICTACCESS."),
	AC_NONE, 2, ns_cmd_resetstrictaccess, { .path = "nickserv/resetstrictaccess" } };

static void ns_cmd_resetstrictaccess(sourceinfo_t *si, int parc, char *parv[])
{
	myuser_t *mu;
	char *name = parv[0];
	char *key;
	enum specialoperation op = op_none;
	bool ismarked = false;
	metadata_t *md;

	if (!name)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "SENDPASS");
		command_fail(si, fault_needmoreparams, _("Syntax: SENDPASS <account>"));
		return;
	}

	if (!(mu = myuser_find_by_nick(name)))
	{
		command_fail(si, fault_nosuch_target, _("\2%s\2 is not registered."), name);
		return;
	}

	/* If here, check if the key matches, if there even is a key. */
	if (parc > 1)
	{
		if ((md = metadata_find(mu, "private:resetstrictaccess:key")) == NULL)
		{
			command_fail(si, fault_nochange, _("\2%s\2 did not have a key set."), entity(mu)->name);
			return;
		}

		if (has_priv(si, PRIV_USER_ADMIN) && !strcasecmp(parv[1], "CLEAR"))
		{
			metadata_delete(mu, "private:resetstrictaccess:key");
			metadata_delete(mu, "private:resetstrictaccess:timestamp");
			metadata_delete(mu, "private:resetstrictaccess:sender");

			command_success_nodata(si, _("The key for \2%s\2 for disabling STRICTACCESS has been cleared."),
				entity(mu)->name);

			logcommand(si, CMDLOG_ADMIN, _("RESETSTRICTACCESS:CLEAR: \2%s\2"),
				entity(mu)->name);

			return;
		}

		if (!strcmp(parv[1], (char *)md->value))
		{
			if (mu->flags & MU_STRICTACCESS)
				mu->flags &= ~MU_STRICTACCESS;

			metadata_delete(mu, "private:resetstrictaccess:key");
			metadata_delete(mu, "private:resetstrictaccess:timestamp");
			metadata_delete(mu, "private:resetstrictaccess:sender");

			command_success_nodata(si, _("The STRICTACCESS option on the account \2%s\2 has been successfully disabled."),
				entity(mu)->name);

			logcommand(si, CMDLOG_SET, "SET:STRICTACCESS:OFF (recovery for \2%s\2)", entity(mu)->name);

			return;
		}
	}

	if (mu->flags & MU_WAITAUTH)
	{
		command_fail(si, fault_badparams, _("\2%s\2 is not verified."), entity(mu)->name);
		return;
	}

	/* See if STRICTACCESS has even been set for the account.  No point in the e-mail otherwise. */
	if (!(mu->flags & MU_STRICTACCESS))
	{
		command_fail(si, fault_nochange, _("\2%s\2 does not have STRICTACCESS enabled so the reset key was not sent."),
			entity(mu)->name);
		return;
	}

	if (metadata_find(mu, "private:mark:setter"))
	{
		ismarked = true;
		/* don't want to disclose this, so just go ahead... */
	}

	if (MOWGLI_LIST_LENGTH(&mu->logins) > 0)
	{
		command_fail(si, fault_noprivs, _("This operation cannot be performed on %s as someone is logged in to it."), entity(mu)->name);
		return;
	}

	if (metadata_find(mu, "private:freeze:freezer"))
	{
		command_success_nodata(si, _("%s has been frozen by the %s administration."), entity(mu)->name, me.netname);
		return;
	}

	/* Check to see there hasn't already been a key request within the last 24 hours. */
	if (metadata_find(mu, "private:resetstrictaccess:key")
		&& (md = metadata_find(mu, "private:resetstrictaccess:timestamp")) != NULL
		&& atoi(md->value) + 86400 > CURRTIME)
	{
		command_fail(si, fault_alreadyexists, _("A key was already sent for: \2%s\2   Please wait %s until trying again."),
			entity(mu)->name,
			timediff(86400 - (CURRTIME - atoi(md->value))));

		return;
	}

	if (metadata_find(mu, "private:resetstrictaccess:key"))
	{
		command_fail(si, fault_alreadyexists, _("\2%s\2 already has a key for disabling STRICTACCESS outstanding."), entity(mu)->name);
		if (has_priv(si, PRIV_USER_SENDPASS))
			command_fail(si, fault_alreadyexists, _("Use RESETSTRICTACCESS %s CLEAR to clear it so that a new one can be sent."), entity(mu)->name);
		return;
	}

	key = random_string(128);

	if (sendemail(si->su != NULL ? si->su : si->service->me, mu, EMAIL_RESETSTRICTACCESS, mu->email, key))
	{
		metadata_add(mu, "private:resetstrictaccess:key", key);
		logcommand(si, CMDLOG_ADMIN, "RESETSTRICTACCESS: \2%s\2 (change key)", entity(mu)->name);
		command_success_nodata(si, _("The key for disabling STRICTACCESS for \2%s\2 has been sent to the corresponding email address."), entity(mu)->name);
		if (ismarked)
			wallops("%s sent the key for disabling STRICTACCESS for the \2MARKED\2 account %s.", get_oper_name(si), entity(mu)->name);

		metadata_add(mu, "private:resetstrictaccess:sender", get_oper_name(si));
		metadata_add(mu, "private:resetstrictaccess:timestamp", number_to_string(time(NULL)));
	}
	else
		command_fail(si, fault_emailfail, _("Email send failed."));

	free(key);
};

void _modinit(module_t *m)
{
	MODULE_TRY_REQUEST_DEPENDENCY(m, "nickserv/set_strictaccess");

	service_named_bind_command("nickserv", &ns_resetstrictaccess);
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("nickserv", &ns_resetstrictaccess);
}
