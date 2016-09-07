/*
 * Copyright (c) 2005 William Pitcock, et al.
 * Copyright (c) 2016 ChatLounge IRC Network Development Team
 *
 * Rights to this code are as documented in doc/LICENSE.
 *
 *     Lists object properties via their metadata table.  Ripped from
 * chanserv/taxonomy and adapted to work with GroupServ.
 *
 */

#include "atheme.h"
#include "groupserv.h"

DECLARE_MODULE_V1
(
	"groupserv/taxonomy", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net>"
);

static void gs_cmd_taxonomy(sourceinfo_t *si, int parc, char *parv[]);

command_t gs_taxonomy = { "TAXONOMY", N_("Displays a groups's metadata."),
                        AC_NONE, 1, gs_cmd_taxonomy, { .path = "groupserv/taxonomy" } };

void _modinit(module_t *m)
{
	use_groupserv_main_symbols(m);

	service_named_bind_command("groupserv", &gs_taxonomy);
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("groupserv", &gs_taxonomy);
}

void gs_cmd_taxonomy(sourceinfo_t *si, int parc, char *parv[])
{
	char *target = parv[0];
	mygroup_t *mg;
	mowgli_patricia_iteration_state_t state;
	metadata_t *md;
	bool isoper;

	if (!target || *target != '!')
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "TAXONOMY");
		command_fail(si, fault_needmoreparams, _("Syntax: TAXONOMY <!group>"));
		return;
	}

	if ((mg = mygroup_find(target)) == NULL)
	{
		command_fail(si, fault_nosuch_target, _("\2%s\2 is not a registered group."), target);
		return;
	}

	isoper = has_priv(si, PRIV_GROUP_AUSPEX);

	if (!(mg->flags & MG_PUBLIC) &&
			!groupacs_sourceinfo_has_flag(mg, si, GA_ACLVIEW) && !isoper)
	{
		command_fail(si, fault_noprivs, _("Group \2%s\2 is private."),
			entity(mg)->name);
		return;
	}

	if (!(mg->flags & MG_PUBLIC) &&
			!groupacs_sourceinfo_has_flag(mg, si, GA_ACLVIEW) && !isoper)
		logcommand(si, CMDLOG_ADMIN, "TAXONOMY: \2%s\2 (oper)", entity(mg)->name);
	else
		logcommand(si, CMDLOG_GET, "TAXONOMY: \2%s\2", entity(mg)->name);
	command_success_nodata(si, _("Taxonomy for: \2%s\2"), entity(mg)->name);

	MOWGLI_PATRICIA_FOREACH(md, &state, object(mg)->metadata)
	{
		if (!strncmp(md->name, "private:", 8) && !isoper)
			continue;

		command_success_nodata(si, "%-32s: %s", md->name, md->value);
	}

	command_success_nodata(si, _("End of \2%s\2 taxonomy."), entity(mg)->name);
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
