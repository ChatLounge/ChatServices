/*
 * Copyright (c) 2006-2010 Atheme Development Group
 * Copyright (c) 2017-2018 ChatLounge IRC Network Development Team
 *
 * Rights to this code are as documented in doc/LICENSE.
 *
 * Changes and shows nickname CertFP authentication lists.
 *
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"nickserv/cert", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	VENDOR_STRING
);

void (*add_history_entry)(myuser_t *smu, myuser_t *tmu, const char *desc) = NULL;
void (*add_login_history_entry)(myuser_t *smu, myuser_t *tmu, const char *desc) = NULL;

static void ns_cmd_cert(sourceinfo_t *si, int parc, char *parv[]);

command_t ns_cert = { "CERT", N_("Changes and shows your nickname CertFP authentication list."), AC_NONE, 2, ns_cmd_cert, { .path = "nickserv/cert" } };

void _modinit(module_t *m)
{
	service_named_bind_command("nickserv", &ns_cert);

	hook_add_event("user_can_login");
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("nickserv", &ns_cert);
}

static void ns_cmd_cert(sourceinfo_t *si, int parc, char *parv[])
{
	myuser_t *mu;
	mowgli_node_t *n;
	char *mcfp, description[300];
	mycertfp_t *cert;
	user_t *cu;
	service_t *ns;
	hook_user_login_check_t req;

	if (parc < 1)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "CERT");
		command_fail(si, fault_needmoreparams, _("Syntax: CERT ADD|DEL|IDENTIFY|LIST [fingerprint]"));
		return;
	}

	if (!strcasecmp(parv[0], "LIST"))
	{
		if (parc < 2)
		{
			mu = si->smu;
			if (mu == NULL)
			{
				command_fail(si, fault_noprivs, _("You are not logged in."));
				return;
			}
		}
		else
		{
			if (!has_priv(si, PRIV_USER_AUSPEX))
			{
				command_fail(si, fault_noprivs, _("You are not authorized to use the target argument."));
				return;
			}

			if (!(mu = myuser_find_ext(parv[1])))
			{
				command_fail(si, fault_badparams, _("\2%s\2 is not registered."), parv[1]);
				return;
			}
		}

		if (MOWGLI_LIST_LENGTH(&mu->cert_fingerprints) > 0)
		{
			if (mu != si->smu)
				logcommand(si, CMDLOG_ADMIN, "CERT:LIST: \2%s\2", entity(mu)->name);
			else
				logcommand(si, CMDLOG_GET, "CERT:LIST");

			command_success_nodata(si, _("Fingerprint list for \2%s\2:"), entity(mu)->name);

			MOWGLI_ITER_FOREACH(n, mu->cert_fingerprints.head)
			{
				mcfp = ((mycertfp_t*)n->data)->certfp;
				command_success_nodata(si, "- %s", mcfp);
			}

			command_success_nodata(si, _("End of \2%s\2 fingerprint list."), entity(mu)->name);
		}
		else
			command_fail(si, fault_nosuch_key, "The fingerprint list for \2%s\2 is empty.", entity(mu)->name);
	}
	else if (!strcasecmp(parv[0], "ADD"))
	{
		mu = si->smu;
		if (parc < 2)
		{
			mcfp = si->su != NULL ? si->su->certfp : NULL;

			if (mcfp == NULL)
			{
				command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "CERT ADD");
				command_fail(si, fault_needmoreparams, _("Syntax: CERT ADD <fingerprint>"));
				return;
			}
		}
		else
		{
			mcfp = parv[1];
		}

		if (mu == NULL)
		{
			command_fail(si, fault_noprivs, _("You are not logged in."));
			return;
		}

		cert = mycertfp_find(mcfp);
		if (cert == NULL)
			;
		else if (cert->mu == mu)
		{
			command_fail(si, fault_nochange, _("Fingerprint \2%s\2 is already on your fingerprint list."), mcfp);
			return;
		}
		else
		{
			command_fail(si, fault_nochange, _("Fingerprint \2%s\2 is already on another user's fingerprint list."), mcfp);
			return;
		}
		if (mycertfp_add(mu, mcfp))
		{
			command_success_nodata(si, _("Added fingerprint \2%s\2 to your fingerprint list."), mcfp);
			logcommand(si, CMDLOG_SET, "CERT:ADD: \2%s\2", mcfp);

			if ((add_history_entry = module_locate_symbol("nickserv/history", "add_history_entry")) != NULL)
			{
				snprintf(description, sizeof description, "Fingerprint added: \2%s\2", mcfp);
				add_history_entry(si->smu, si->smu, description);
			}
		}
		else
			command_fail(si, fault_toomany, _("Your fingerprint list is full."));
	}
	else if (!strcasecmp(parv[0], "DEL"))
	{
		if (parc < 2)
		{
			command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "CERT DEL");
			command_fail(si, fault_needmoreparams, _("Syntax: CERT DEL <fingerprint>"));
			return;
		}

		mu = si->smu;

		if (mu == NULL)
		{
			command_fail(si, fault_noprivs, _("You are not logged in."));
			return;
		}

		cert = mycertfp_find(parv[1]);

		if (cert == NULL || cert->mu != mu)
		{
			command_fail(si, fault_nochange, _("Fingerprint \2%s\2 is not on your fingerprint list."), parv[1]);
			return;
		}

		if (mu->flags & MU_NOPASSWORD && MOWGLI_LIST_LENGTH(&mu->cert_fingerprints) < 2)
		{
			command_fail(si, fault_noprivs, _("The \2NOPASSWORD\2 option has been enabled so removing the last certificate fingerprint is not permitted since it will cause account lockout."));
			return;
		}

		if (si->su->flags & UF_USEDCERT && mu->flags & MU_NOPASSWORD && !strcmp(si->su->certfp, parv[1]))
		{
			command_fail(si, fault_noprivs, _("The \2NOPASSWORD\2 option has been enabled and you have attempted to remove the certificate fingerprint you identified with.  Please disable \2NOPASSWORD\2 or identify from another connection."));
			return;
		}

		command_success_nodata(si, _("Deleted fingerprint \2%s\2 from your fingerprint list."), parv[1]);
		logcommand(si, CMDLOG_SET, "CERT:DEL: \2%s\2", parv[1]);
		mycertfp_delete(cert);

		if ((add_history_entry = module_locate_symbol("nickserv/history", "add_history_entry")) != NULL)
		{
			snprintf(description, sizeof description, "Fingerprint removed: \2%s\2", parv[1]);
			add_history_entry(si->smu, si->smu, description);
		}
	}
	else if (!strcasecmp(parv[0], "IDENTIFY") || !strcasecmp(parv[0], "ID"))
	{
		cu = si->su;
		if (cu->certfp == NULL) // No fingerprint
		{
			command_fail(si, fault_nochange, _("You don't have a fingerprint."));
		}

		cert = mycertfp_find(cu->certfp);
		if (cert == NULL) // Fingerprint not in DB
		{
			command_fail(si, fault_nochange, _("Your current fingerprint doesn't match any accounts."));
			return;
		}

		ns = service_find("nickserv");
		if (ns == NULL)
		{
			return;
		}

		mu = cert->mu;

		req.si = si;
		req.mu = mu;
		req.allowed = true;
		hook_call_user_can_login(&req);
		if (!req.allowed)
		{
			logcommand(si, CMDLOG_LOGIN, "failed CERTFP login to \2%s\2 (denied by hook)", entity(mu)->name);
			return;
		}

		if (metadata_find(mu, "private:freeze:freezer"))
		{
			command_fail(si, fault_authfail, _("You cannot login as \2%s\2 because the account has been frozen."), entity(mu)->name);
			logcommand(si, CMDLOG_LOGIN, "failed LOGIN to %s (frozen) via CERTFP (%s)", entity(mu)->name, cu->certfp);
			return;
		}

		if (cu->myuser != NULL) // Already logged in.
		{
			command_fail(si, fault_nochange, _("You are already logged in as: \2%s\2"), entity(cu->myuser)->name);
			return;
		}

		if ((mu->flags & MU_STRICTACCESS) && !myuser_access_verify(si->su, mu))
		{
			command_fail(si, fault_authfail, _("You may not log in from this connection as STRICTACCESS has been enabled on this account."));

			if ((add_login_history_entry = module_locate_symbol("nickserv/loginhistory", "add_login_history_entry")) != NULL)
			{
				snprintf(description, sizeof description, "Failed login: CERT from %s (%s@%s) [%s] (one or more client certificates have been compromised.)", si->su->nick, si->su->user, si->su->host, si->su->ip);
				add_login_history_entry(mu, mu, description);
			}

			return;
		}

		if (MOWGLI_LIST_LENGTH(&mu->logins) >= me.maxlogins)
		{
			command_fail(si, fault_toomany, _("There are already \2%zu\2 sessions logged in to \2%s\2 (maximum allowed: %u)."), MOWGLI_LIST_LENGTH(&mu->logins), entity(mu)->name, me.maxlogins);
			return;
		}

		cu->flags |= UF_USEDCERT;

		myuser_login(ns, cu, mu, true, "CERT IDENTIFY");
		logcommand_user(ns, cu, CMDLOG_LOGIN, "LOGIN via CERT IDENTIFY (%s)", cu->certfp);
		notice(ns->nick, cu->nick, nicksvs.no_nick_ownership ? _("You are now logged in as \2%s\2.") : _("You are now identified for \2%s\2."), entity(mu)->name);
	}
	else
	{
		command_fail(si, fault_needmoreparams, STR_INVALID_PARAMS, "CERT");
		command_fail(si, fault_needmoreparams, _("Syntax: CERT ADD|DEL|IDENTIFY|LIST [fingerprint]"));
		return;
	}
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
