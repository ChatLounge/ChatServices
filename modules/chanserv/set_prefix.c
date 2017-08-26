/*
 * Copyright (c) 2006-2010 Atheme Development Group
 * Copyright (c) 2017 ChatLounge IRC Network Development Team
 *
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains routines to handle the CService SET PREFIX command.
 *
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"chanserv/set_prefix", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net>"
);
void (*notify_channel_set_change)(sourceinfo_t *si, myuser_t *tmu, mychan_t *mc,
	const char *settingname, const char *setting) = NULL;

static void cs_set_prefix_config_ready(void *unused);
static void cs_cmd_set_prefix(sourceinfo_t *si, int parc, char *parv[]);

command_t cs_set_prefix = { "PREFIX", N_("Sets the channel PREFIX."), AC_NONE, 2, cs_cmd_set_prefix, { .path = "cservice/set_prefix" } };

mowgli_patricia_t **cs_set_cmdtree;

void _modinit(module_t *m)
{
	MODULE_TRY_REQUEST_SYMBOL(m, cs_set_cmdtree, "chanserv/set_core", "cs_set_cmdtree");

	if (module_request("chanserv/main"))
		notify_channel_set_change = module_locate_symbol("chanserv/main", "notify_channel_set_change");

	command_add(&cs_set_prefix, *cs_set_cmdtree);

	hook_add_event("config_ready");
	hook_add_config_ready(cs_set_prefix_config_ready);
}

void _moddeinit(module_unload_intent_t intent)
{
	command_delete(&cs_set_prefix, *cs_set_cmdtree);

	hook_del_config_ready(cs_set_prefix_config_ready);
}

static void cs_set_prefix_config_ready(void *unused)
{
	if (chansvs.fantasy)
		cs_set_prefix.access = NULL;
	else
		cs_set_prefix.access = AC_DISABLED;
}

static int goodprefix(const char *p)
{
	int i;
	int haschar = 0;
	int hasnonprint = 0;

	for (i = 0; p[i]; i++) {
		if (!isspace((unsigned char)p[i])) { haschar = 1; }
		if (!isprint((unsigned char)p[i])) { hasnonprint = 1; }
	}

	return haschar && !hasnonprint;
}


static void cs_cmd_set_prefix(sourceinfo_t *si, int parc, char *parv[])
{
	mychan_t *mc;
	char *prefix = parv[1];

	if (!(mc = mychan_find(parv[0])))
	{
		command_fail(si, fault_nosuch_target, _("Channel \2%s\2 is not registered."), parv[0]);
		return;
	}

	if (!(chanacs_source_has_flag(mc, si, CA_SET) || chanacs_source_has_flag(mc, si, CA_FOUNDER)))
	{
		command_fail(si, fault_noprivs, _("You are not authorized to execute this command."));
		return;
	}

	if (!prefix || !strcasecmp(prefix, "DEFAULT"))
	{
		metadata_delete(mc, "private:prefix");
		logcommand(si, CMDLOG_SET, "SET:PREFIX: \2%s\2 reset", mc->name);
		command_success_nodata(si, _("The fantasy prefix for channel \2%s\2 has been reset."), mc->name);

		notify_channel_set_change(si, si->smu, mc, "PREFIX", "Default");

		return;
	}

	if (!goodprefix(prefix))
	{
		command_fail(si, fault_badparams, _("Prefix '%s' is invalid. The prefix may "
		             "contain only printable characters, and must contain at least "
		             "one non-space character."), prefix);
		return;
	}

	metadata_add(mc, "private:prefix", prefix);
	logcommand(si, CMDLOG_SET, "SET:PREFIX: \2%s\2 \2%s\2", mc->name, prefix);
	command_success_nodata(si, _("The fantasy prefix for channel \2%s\2 has been set to: \2%s\2"),
                               parv[0], prefix);

	notify_channel_set_change(si, si->smu, mc, "PREFIX", prefix);

}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
