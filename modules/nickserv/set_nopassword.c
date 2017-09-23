/*
 * Copyright (c) 2005 William Pitcock <nenolod -at- nenolod.net>
 * Copyright (c) 2007 Jilles Tjoelker
 * Copyright (c) 2017 ChatLounge IRC Network Development Team
 *     <admin -at- chatlounge.net>
 *
 * Rights to this code are as documented in doc/LICENSE.
 *
 * Allows you to opt-out of channel change messages.
 *
 */

#include "atheme.h"
#include "uplink.h"
#include "list_common.h"
#include "list.h"

DECLARE_MODULE_V1
(
	"nickserv/set_nopassword", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net>"
);

void (*add_history_entry_setting)(myuser_t *smu, myuser_t *tmu, const char *settingname, const char *setting) = NULL;

mowgli_patricia_t **ns_set_cmdtree;

static void ns_cmd_set_nopassword(sourceinfo_t *si, int parc, char *parv[]);

command_t ns_set_nopassword = { "NOPASSWORD", N_("Allows you to disable any password-based authentication methods except for XMLRPC/JSONRPC."), AC_NONE, 1, ns_cmd_set_nopassword, { .path = "nickserv/set_nopassword" } };

static bool has_nopassword(const mynick_t *mn, const void *arg)
{
	myuser_t *mu = mn->owner;

	return ( mu->flags & MU_NOPASSWORD ) == MU_NOPASSWORD;
}

void _modinit(module_t *m)
{
	if (module_request("nickserv/main"))
		add_history_entry_setting = module_locate_symbol("nickserv/main", "add_history_entry_setting");

	MODULE_TRY_REQUEST_SYMBOL(m, ns_set_cmdtree, "nickserv/set_core", "ns_set_cmdtree");

	command_add(&ns_set_nopassword, *ns_set_cmdtree);

	use_nslist_main_symbols(m);

	static list_param_t nopassword;
	nopassword.opttype = OPT_BOOL;
	nopassword.is_match = has_nopassword;

	list_register("nopassword", &nopassword);
}

void _moddeinit(module_unload_intent_t intent)
{
	command_delete(&ns_set_nopassword, *ns_set_cmdtree);

	list_unregister("nopassword");
}

/* SET NOPASSWORD [ON|OFF] */
static void ns_cmd_set_nopassword(sourceinfo_t *si, int parc, char *parv[])
{
	char *setting = parv[0];
	mowgli_node_t *n;
	bool match = false;

	if (!setting)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "NOPASSWORD");
		return;
	}

	if (!strcasecmp("ON", setting) || !strcasecmp("1", setting) || !strcasecmp("TRUE", setting))
	{
		if (MU_NOPASSWORD & si->smu->flags)
		{
			command_fail(si, fault_nochange, _("The \2%s\2 flag is already set for account \2%s\2."), "NOPASSWORD", entity(si->smu)->name);
			return;
		}

		if (MOWGLI_LIST_LENGTH(&si->smu->cert_fingerprints) == 0)
		{
			command_fail(si, fault_noprivs, _("You may not enable the \2NOPASSWORD\2 flag because your certificate fingerprint list is empty.  Please see: \2/msg %s HELP CERT\2"), nicksvs.nick);
			return;
		}

		if (!(si->su->certfp))
		{
			command_fail(si, fault_noprivs, _("You may not enable the NOPASSWORD option from a connection that doesn't have a certificate fingerprint."));
			return;
		}

		/* Loop in lieu of checking for UF_USEDCERT because the user may
		 * have added the fingerprint recently and not necessarily used it
		 * to identify to the account.
		 */
		MOWGLI_ITER_FOREACH(n, si->smu->cert_fingerprints.head)
		{
				if (!strcmp(((mycertfp_t*)n->data)->certfp, si->su->certfp))
				{
					match = true;
					break;
				}
		}

		if (!match)
		{
			command_fail(si, fault_noprivs, _("You may not enable the NOPASSWORD option from a connection that doesn't have a matching certificate fingerprint."));
			return;
		}

		logcommand(si, CMDLOG_SET, "SET:NOPASSWORD:ON");

		si->smu->flags |= MU_NOPASSWORD;

		command_success_nodata(si, _("The \2%s\2 flag has been set for account \2%s\2."), "NOPASSWORD" ,entity(si->smu)->name);

		add_history_entry_setting(si->smu, si->smu, "NOPASSWORD", "ON");

		return;
	}
	else if (!strcasecmp("OFF", setting) || !strcasecmp("0", setting) || !strcasecmp("FALSE", setting))
	{
		if (!(MU_NOPASSWORD & si->smu->flags))
		{
			command_fail(si, fault_nochange, _("The \2%s\2 flag is not set for account \2%s\2."), "NOPASSWORD", entity(si->smu)->name);
			return;
		}

		if (!(si->su->certfp))
		{
			command_fail(si, fault_noprivs, _("You may not disable the NOPASSWORD option from a connection that doesn't have a certificate fingerprint."));
			return;
		}

		/* Loop in lieu of checking for UF_USEDCERT because the user may
		 * have added the fingerprint recently and not necessarily used it
		 * to identify to the account.
		 */

		MOWGLI_ITER_FOREACH(n, si->smu->cert_fingerprints.head)
		{
				if (!strcmp(((mycertfp_t*)n->data)->certfp, si->su->certfp))
				{
					match = true;
					break;
				}
		}

		if (!match)
		{
			command_fail(si, fault_noprivs, _("You may not disable the NOPASSWORD option from a connection that doesn't have a matching certificate fingerprint."));
			return;
		}

		logcommand(si, CMDLOG_SET, "SET:NOPASSWORD:OFF");

		si->smu->flags &= ~MU_NOPASSWORD;

		command_success_nodata(si, _("The \2%s\2 flag has been removed for account \2%s\2."), "NOPASSWORD", entity(si->smu)->name);

		add_history_entry_setting(si->smu, si->smu, "NOPASSWORD", "OFF");

		return;
	}
	else
	{
		command_fail(si, fault_badparams, STR_INVALID_PARAMS, "NOPASSWORD");
		return;
	}
}