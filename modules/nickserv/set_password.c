/*
 * Copyright (c) 2005 William Pitcock <nenolod -at- nenolod.net>
 * Copyright (c) 2007 Jilles Tjoelker
 * Copyright (c) 2016-2017 ChatLounge IRC Network Development
 *
 * Rights to this code are as documented in doc/LICENSE.
 *
 * Changes the password associated with your account.
 *
 */

#include "atheme.h"
#include "uplink.h"

DECLARE_MODULE_V1
(
	"nickserv/set_password", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net/>"
);

void (*add_history_entry_setting)(myuser_t *smu, myuser_t *tmu, const char *settingname, const char *setting) = NULL;

mowgli_patricia_t **ns_set_cmdtree;

static void ns_cmd_set_password(sourceinfo_t *si, int parc, char *parv[]);

command_t ns_set_password = { "PASSWORD", N_("Changes the password associated with your account."), AC_NONE, 1, ns_cmd_set_password, { .path = "nickserv/set_password" } };

void _modinit(module_t *m)
{
	MODULE_TRY_REQUEST_SYMBOL(m, ns_set_cmdtree, "nickserv/set_core", "ns_set_cmdtree");

	if (module_request("nickserv/main"))
		add_history_entry_setting = module_locate_symbol("nickserv/main", "add_history_entry_setting");

	command_add(&ns_set_password, *ns_set_cmdtree);
}

void _moddeinit(module_unload_intent_t intent)
{
	command_delete(&ns_set_password, *ns_set_cmdtree);
}

/* SET PASSWORD <password> */
static void ns_cmd_set_password(sourceinfo_t *si, int parc, char *parv[])
{
	char *password = parv[0];

	if (auth_module_loaded)
	{
		command_fail(si, fault_noprivs, _("You must change the password in the external system."));
		return;
	}

	if (!password)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "PASSWORD");
		return;
	}

	if (strlen(password) >= PASSLEN)
	{
		command_fail(si, fault_badparams, STR_INVALID_PARAMS, "PASSWORD");
		command_fail(si, fault_badparams, _("Registration passwords may not be longer than \2%d\2 characters."), PASSLEN - 1);
		return;
	}

	if (!strcasecmp(password, entity(si->smu)->name))
	{
		command_fail(si, fault_badparams, _("You cannot use your nickname as a password."));
		command_fail(si, fault_badparams, _("Syntax: SET PASSWORD <new password>"));
		return;
	}

	logcommand(si, CMDLOG_SET, "SET:PASSWORD");

	set_password(si->smu, password);

	if (si->smu->flags & MU_NOPASSWORD)
	{
		si->smu->flags &= ~MU_NOPASSWORD;
		command_success_nodata(si, _("The \2%s\2 flag has been removed for account \2%s\2."), "NOPASSWORD", entity(si->smu)->name);
		add_history_entry_setting(si->smu, si->smu, "NOPASSWORD", "OFF");
	}

	command_success_nodata(si, _("The password for \2%s\2 has been changed to: \2%s\2"), entity(si->smu)->name, password);

	add_history_entry_setting(si->smu, si->smu, "PASSWORD", "<Changed>");

	return;
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
