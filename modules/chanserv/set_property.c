/*
 * Copyright (c) 2003-2004 E. Will et al.
 * Copyright (c) 2006-2010 Atheme Development Group
 * Copyright (c) 2017 ChatLounge IRC Network Development Team
 *
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains routines to handle the CService SET PROPERTY command.
 *
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"chanserv/set_property", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net>"
);

void (*notify_channel_set_change)(sourceinfo_t *si, myuser_t *tmu, mychan_t *mc,
	const char *settingname, const char *setting) = NULL;

static void cs_cmd_set_property(sourceinfo_t *si, int parc, char *parv[]);

command_t cs_set_property = { "PROPERTY", N_("Manipulates channel metadata."), AC_NONE, 2, cs_cmd_set_property, { .path = "cservice/set_property" } };

mowgli_patricia_t **cs_set_cmdtree;

void _modinit(module_t *m)
{
	MODULE_TRY_REQUEST_SYMBOL(m, cs_set_cmdtree, "chanserv/set_core", "cs_set_cmdtree");

	if (module_request("chanserv/main"))
		notify_channel_set_change = module_locate_symbol("chanserv/main", "notify_channel_set_change");

	command_add(&cs_set_property, *cs_set_cmdtree);
}

void _moddeinit(module_unload_intent_t intent)
{
	command_delete(&cs_set_property, *cs_set_cmdtree);
}

static void cs_cmd_set_property(sourceinfo_t *si, int parc, char *parv[])
{
	mychan_t *mc;
	char *property = strtok(parv[1], " ");
	char *value = strtok(NULL, "");
	unsigned int count;
	mowgli_patricia_iteration_state_t state;
	metadata_t *md;

	if (!property)
	{
		command_fail(si, fault_needmoreparams, _("Syntax: SET <#channel> PROPERTY <property> [value]"));
		return;
	}

	/* do we really need to allow this? -- jilles */
	if (strchr(property, ':') && !has_priv(si, PRIV_METADATA))
	{
		command_fail(si, fault_badparams, _("Invalid property name."));
		return;
	}

	if (!(mc = mychan_find(parv[0])))
	{
		command_fail(si, fault_nosuch_target, _("Channel \2%s\2 is not registered."), parv[0]);
		return;
	}

	if (!is_founder(mc, entity(si->smu)))
	{
		command_fail(si, fault_noprivs, _("You are not authorized to perform this command."));
		return;
	}

	if (strchr(property, ':'))
		logcommand(si, CMDLOG_SET, "SET:PROPERTY: \2%s\2: \2%s\2/\2%s\2", mc->name, property, value);

	if (!value)
	{
		md = metadata_find(mc, property);

		if (!md)
		{
			command_fail(si, fault_nochange, _("Metadata entry \2%s\2 was not set on channel: \25s\2"), property, mc->name);
			return;
		}

		logcommand(si, CMDLOG_SET, "SET:PROPERTY: \2%s\2 \2%s\2 (deleted) (Former value: \2%s\2)", mc->name, property, md->value);
		verbose(mc, _("\2%s\2 deleted the metadata entry \2%s\2 that had the value: \2%s\2"), get_source_name(si), property, md->value);
		command_success_nodata(si, _("Metadata entry \2%s\2 that had the value \2%s\2 has been deleted."), property, md->value);

		char metadatadesc[100];
		char metadatavalue[200];

		snprintf(metadatadesc, sizeof metadatadesc, "metadata entry %s", property);
		snprintf(metadatavalue, sizeof metadatavalue, "<Deleted> (Former value: \2%s\2)", md->value);

		notify_channel_set_change(si, si->smu, mc, metadatadesc, metadatavalue);

		metadata_delete(mc, property);

		return;
	}

	count = 0;
	if (object(mc)->metadata)
	{
		MOWGLI_PATRICIA_FOREACH(md, &state, object(mc)->metadata)
		{
			if (strncmp(md->name, "private:", 8))
				count++;
		}
	}
	if (count >= me.mdlimit)
	{
		command_fail(si, fault_toomany, _("Cannot add \2%s\2 to \2%s\2 metadata table, it is full."),
						property, mc->name);
		return;
	}

	if (strlen(property) > 32 || strlen(value) > 300 || has_ctrl_chars(property))
	{
		command_fail(si, fault_badparams, _("Parameters are too long.  Aborting."));
		return;
	}

	metadata_add(mc, property, value);
	logcommand(si, CMDLOG_SET, "SET:PROPERTY: \2%s\2 on \2%s\2 to \2%s\2", property, mc->name, value);
	verbose(mc, _("\2%s\2 added the metadata entry \2%s\2 with the value: \2%s\2"), get_source_name(si), property, value);
	command_success_nodata(si, _("Metadata entry \2%s\2 set on channel: \2%s\2 (Value: %s)"), property, mc->name, value);

	char metadatadesc[100];

	snprintf(metadatadesc, sizeof metadatadesc, "metadata entry %s", property);

	notify_channel_set_change(si, si->smu, mc, metadatadesc, value);
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
