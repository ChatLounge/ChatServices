/*
 * Copyright (c) 2003-2004 E. Will et al.
 * Copyright (c) 2006-2010 Atheme Development Group
 * Copyright (c) 2017 ChatLounge IRC Network Development Team
 *
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains routines to handle the CService SET EMAIL command.
 *
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"chanserv/set_email", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	VENDOR_STRING
);

void (*notify_channel_set_change)(sourceinfo_t *si, myuser_t *tmu, mychan_t *mc,
	const char *settingname, const char *setting) = NULL;

static void cs_cmd_set_email(sourceinfo_t *si, int parc, char *parv[]);

command_t cs_set_email = { "EMAIL", N_("Sets the channel e-mail address."), AC_NONE, 2, cs_cmd_set_email, { .path = "cservice/set_email" } };

mowgli_patricia_t **cs_set_cmdtree;

void _modinit(module_t *m)
{
	MODULE_TRY_REQUEST_SYMBOL(m, cs_set_cmdtree, "chanserv/set_core", "cs_set_cmdtree");

	if (module_request("chanserv/main"))
		notify_channel_set_change = module_locate_symbol("chanserv/main", "notify_channel_set_change");

	command_add(&cs_set_email, *cs_set_cmdtree);
}

void _moddeinit(module_unload_intent_t intent)
{
	command_delete(&cs_set_email, *cs_set_cmdtree);
}

static void cs_cmd_set_email(sourceinfo_t *si, int parc, char *parv[])
{
	mychan_t *mc;
	char *mail = parv[1];

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

	if (!mail || !strcasecmp(mail, "NONE") || !strcasecmp(mail, "OFF"))
	{
		if (metadata_find(mc, "email"))
		{
			metadata_delete(mc, "email");
			command_success_nodata(si, _("The e-mail address for channel \2%s\2 was deleted."), mc->name);
			verbose(mc, _("\2%s\2 cleared the e-mail address for this channel."), get_source_name(si));
			logcommand(si, CMDLOG_SET, "SET:EMAIL:NONE: \2%s\2", mc->name);

			notify_channel_set_change(si, si->smu, mc, "E-mail", "None");

			return;
		}

		command_fail(si, fault_nochange, _("The e-mail address for channel \2%s\2 was not set."), mc->name);
		return;
	}

	if (!validemail(mail))
	{
		command_fail(si, fault_badparams, _("\2%s\2 is not a valid e-mail address."), mail);
		return;
	}

	/* we'll overwrite any existing metadata */
	metadata_add(mc, "email", mail);

	logcommand(si, CMDLOG_SET, "SET:EMAIL: \2%s\2 \2%s\2", mc->name, mail);
	verbose(mc, _("\2%s\2 set the e-mail address for the channel to: \2%s\2"), get_source_name(si), mail);
	command_success_nodata(si, _("The e-mail address for channel \2%s\2 has been set to: \2%s\2"), parv[0], mail);

	notify_channel_set_change(si, si->smu, mc, "E-mail", mail);
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
