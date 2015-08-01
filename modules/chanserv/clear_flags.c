/*
 * Copyright (c) 2010 Atheme Development Group
 * Rights to this code are as documented in doc/LICENSE.
 *
 * This file contains code for the CService CLEAR FLAGS functions.
 *
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"chanserv/clear_flags", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"Atheme Development Group <http://www.atheme.org>"
);

static void cs_cmd_clear_flags(sourceinfo_t *si, int parc, char *parv[]);

command_t cs_clear_flags = { "FLAGS", "Clears all channel flags.", AC_NONE, 2, cs_cmd_clear_flags, { .path = "cservice/clear_flags" } };

mowgli_patricia_t **cs_clear_cmds;

void _modinit(module_t *m)
{
	MODULE_TRY_REQUEST_SYMBOL(m, cs_clear_cmds, "chanserv/clear", "cs_clear_cmds");

        command_add(&cs_clear_flags, *cs_clear_cmds);
}

void _moddeinit(module_unload_intent_t intent)
{
	command_delete(&cs_clear_flags, *cs_clear_cmds);
}

static void cs_cmd_clear_flags(sourceinfo_t *si, int parc, char *parv[])
{
	mychan_t *mc;
	mowgli_node_t *n, *tn;
	chanacs_t *ca;
	char *name = parv[0];
	int changes = 0;
	
	char *key = parv[1];
	char fullcmd[512];
	char key0[80], key1[80];

	if (!name)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "CLEAR FLAGS");
		command_fail(si, fault_needmoreparams, "Syntax: CLEAR <#channel> FLAGS");
		return;
	}

	if (!(mc = mychan_find(name)))
	{
		command_fail(si, fault_nosuch_target, "\2%s\2 is not registered.", name);
		return;
	}

	if (metadata_find(mc, "private:close:closer"))
	{
		command_fail(si, fault_noprivs, "\2%s\2 is closed.", name);
		return;
	}

	if (!mc->chan)
	{
		command_fail(si, fault_nosuch_target, "\2%s\2 does not exist.", name);
		return;
	}

	if (!chanacs_source_has_flag(mc, si, CA_FOUNDER))
	{
		command_fail(si, fault_noprivs, "You are not authorized to perform this operation.");
		return;
	}

	if (si->su != NULL)
	{
		if (!key)
		{
			create_challenge(si, mc->name, 0, key0);
			snprintf(fullcmd, sizeof fullcmd, "/%s%s CLEAR %s FLAGS %s",
					(ircd->uses_rcommand == false) ? "msg " : "",
					chansvs.me->disp, mc->name, key0);
			command_success_nodata(si, _("To avoid accidental use of this command, this operation has to be confirmed. Please confirm by replying with \2%s\2"),
					fullcmd);
			return;
		}
		/* accept current and previous key */
		create_challenge(si, mc->name, 0, key0);
		create_challenge(si, mc->name, 1, key1);
		if (strcmp(key, key0) && strcmp(key, key1))
		{
			command_fail(si, fault_badparams, _("Invalid key for %s."), "CLEAR FLAGS");
			return;
		}
	}

	MOWGLI_ITER_FOREACH_SAFE(n, tn, mc->chanacs.head)
	{
		ca = n->data;

		if (ca->level & CA_FOUNDER)
			continue;

		changes++;
		object_unref(ca);
	}

	logcommand(si, CMDLOG_DO, "CLEAR:FLAGS: \2%s\2", mc->name);
	command_success_nodata(si, _("Cleared flags in \2%s\2."), name);
	if (changes > 0)
		verbose(mc, "\2%s\2 removed all %d non-founder access entries.",
				get_source_name(si), changes);
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
