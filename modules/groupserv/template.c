/*
 * Copyright (c) 2016-2017 ChatLounge IRC Network Development Team
 *     (http://www.chatlounge.net/)
 *
 *     Provides the TEMPLATE command for GroupServ, similar to
 * the way templates for ChanServ flags work.
 *
 */

#include "atheme.h"
#include "groupserv.h"
#include "template.h"

DECLARE_MODULE_V1
(
	"groupserv/template", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net>"
);

static void gs_cmd_template(sourceinfo_t *si, int parc, char *parv[]);

command_t gs_template = { "TEMPLATE", N_("Manipulates predefined sets of flags."),
		AC_NONE, 3, gs_cmd_template, { .path = "groupserv/template" } };

void _modinit(module_t *m)
{
	use_groupserv_main_symbols(m);

	service_named_bind_command("groupserv", &gs_template);
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("groupserv", &gs_template);
}

static int display_template(const char *key, void *data, void *privdata)
{
	sourceinfo_t *si = privdata;
	default_group_template_t *def_t = (default_group_template_t *)data;

	command_success_nodata(si, "%-20s %s", key, gflags_tostr(ga_flags, def_t->flags));

	return 0;
}

static void list_generic_flags(sourceinfo_t *si)
{
	command_success_nodata(si, "%-20s %s", _("Name"), _("Flags"));
	command_success_nodata(si, "%-20s %s", "--------------------", "-----");

	mowgli_patricia_foreach(global_group_template_dict, display_template, si);

	command_success_nodata(si, "%-20s %s", "--------------------", "-----");
	command_success_nodata(si, _("End of network wide template list."));
}

static void gs_cmd_template(sourceinfo_t *si, int parc, char *parv[])
{
	metadata_t *md;
	bool operoverride = false, changegroupacs = false;
	size_t l;
	char *group = parv[0];
	char *target = parv[1];
	mygroup_t *mg;
	unsigned int oldflags, newflags = 0, addflags, removeflags, restrictflags;
	char *p, *q, *r;
	char ss[40], newstr[400], description[300];
	bool found, denied;

	if (!group)
	{
		//list_generic_flags(si);
		//logcommand(si, CMDLOG_GET, "TEMPLATE");
		command_fail(si, fault_nosuch_target, _("You must specify a group name."));
		return;
	}

	mg = mygroup_find(group);
	if (!mg)
	{
		command_fail(si, fault_nosuch_target, _("Group \2%s\2 is not registered."), group);
	}

	if (!target)
	{
		if (!(mg->flags & MG_PUBACL) && !groupacs_sourceinfo_has_flag(mg, si, GA_ACLVIEW))
		{
			if (has_priv(si, PRIV_GROUP_AUSPEX))
				operoverride = true;
			else
			{
				command_fail(si, fault_noprivs, _("You are not authorized to perform this operation."));
				return;
			}
		}

		if (metadata_find(mg, "private:close:closer") && !has_priv(si, PRIV_GROUP_AUSPEX))
		{
			command_fail(si, fault_noprivs, _("\2%s\2 is closed."), group);
			return;
		}

		md = metadata_find(mg, "private:templates");

		if (md != NULL)
		{
			command_success_nodata(si, "%-20s %s", _("Name"), _("Flags"));
			command_success_nodata(si, "%-20s %s", "--------------------", "-----");

			p = md->value;
			while (p != NULL)
			{
				while (*p == ' ')
					p++;
				q = strchr(p, '=');
				if (q == NULL)
					break;
				r = strchr(q, ' ');
				command_success_nodata(si, "%-20.*s %.*s", (int)(q - p), p, r != NULL ? (int)(r - q - 1) : (int)strlen(q + 1), q + 1);
				p = r;
			}

			command_success_nodata(si, "%-20s %s", "--------------------", "-----");
			command_success_nodata(si, _("End of \2%s\2 TEMPLATE listing."), entity(mg)->name);
		}
		else
			command_success_nodata(si, _("No templates set on group: \2%s\2"), entity(mg)->name);

		if (operoverride)
			logcommand(si, CMDLOG_ADMIN, "TEMPLATE: \2%s\2 (oper override)", entity(mg)->name);
		else
			logcommand(si, CMDLOG_GET, "TEMPLATE: \2%s\2", entity(mg)->name);
	}
	else
	{
		char *flagstr = parv[2];

		if (!si->smu)
		{
			command_fail(si, fault_noprivs, _("You are not logged in."));
			return;
		}

		/* founder may always set flags -- jilles */
		restrictflags = groupacs_sourceinfo_flags(mg, si);
		if (restrictflags & GA_FOUNDER)
			restrictflags = GA_ALL_ALL;
		else
		{
			if (!(restrictflags & GA_FLAGS))
			{
				command_fail(si, fault_noprivs, _("You are not authorized to execute this command."));
				return;
			}
			restrictflags = allow_gflags(mg, restrictflags);
		}

		if (metadata_find(mg, "private:close:closer"))
		{
			command_fail(si, fault_noprivs, _("\2%s\2 is closed."), group);
			return;
		}

		if (!target || !flagstr)
		{
			command_fail(si, fault_needmoreparams, _("Usage: TEMPLATE %s [target flags]"), group);
			return;
		}

		if (*target == '+' || *target == '-' || *target == '=')
		{
			command_fail(si, fault_badparams, _("Invalid template name \2%s\2."), target);
			return;
		}
		l = strlen(target);

		if (*flagstr == '!' && (flagstr[1] == '+' || flagstr[1] == '-' || flagstr[1] == '='))
		{
			changegroupacs = true;
			flagstr++;
		}

		if (*flagstr == '+' || *flagstr == '-' || *flagstr == '=')
		{
			gflags_make_bitmasks(flagstr, &addflags, &removeflags);
			if (addflags == 0 && removeflags == 0)
			{
				command_fail(si, fault_badparams, _("No valid flags given."));
				return;
			}

			/* Prevent adding +b to group templates.  Rationale:
			 *
			 * +b serves no purpose outside of public groups anyone can join.
			 * However, adding a template with +b could be used for griefing
			 * other users who are using a group template-specific vhost offer,
			 * which would be visible in all channels.
			 *
			 * Maybe implement the restriction in TEMPLATEVHOST instead?
			 * - Ben
			 */
			if (addflags & GA_BAN)
			{
				command_fail(si, fault_badparams, _("You may not add +b to group templates."));
				return;
			}
		}
		else
		{
			/* allow copying templates as well */
			addflags = get_group_template_flags(mg, flagstr);
			if (addflags == 0)
			{
				command_fail(si, fault_nosuch_key, _("Invalid template name given."));
				return;
			}
			removeflags = GA_ALL_ALL & ~addflags;
		}

		/* if adding +F, also add +f */
		if (addflags & GA_FOUNDER)
			addflags |= GA_FLAGS, removeflags &= ~GA_FLAGS;
		/* if removing +f, also remove +F */
		else if (removeflags & GA_FLAGS)
			removeflags |= GA_FOUNDER, addflags &= ~GA_FOUNDER;

		found = denied = false;
		oldflags = 0;

		md = metadata_find(mg, "private:templates");
		if (md != NULL)
		{
			p = md->value;
			mowgli_strlcpy(newstr, p, sizeof newstr);
			while (p != NULL)
			{
				while (*p == ' ')
					p++;
				q = strchr(p, '=');
				if (q == NULL)
					break;
				r = strchr(q, ' ');
				if (r != NULL && r < q)
					break;
				mowgli_strlcpy(ss, q, sizeof ss);
				if (r != NULL && r - q < (int)(sizeof ss - 1))
				{
					ss[r - q] = '\0';
				}
				if ((size_t)(q - p) == l && !strncasecmp(target, p, l))
				{
					found = true;
					oldflags = gs_flags_parser(ss, 0, 0);
					oldflags &= GA_ALL_ALL;
					addflags &= ~oldflags;
					removeflags &= oldflags & ~addflags;
					/* no change? */
					if ((addflags | removeflags) == 0)
						break;
					/* attempting to add bad flag? */
					/* attempting to remove bad flag? */
					/* attempting to manipulate something with more privs? */
					if (~restrictflags & addflags ||
							~restrictflags & removeflags ||
							~restrictflags & oldflags)
					{
						denied = true;
						break;
					}
					newflags = (oldflags | addflags) & ~removeflags;
					if (newflags == 0)
					{
						if (p == md->value)
							/* removing first entry,
							 * zap the space after it */
							mowgli_strlcpy(newstr, r != NULL ? r + 1 : "", sizeof newstr);
						else
						{
							/* otherwise, zap the space before it */
							p--;
							mowgli_strlcpy(newstr + (p - md->value), r != NULL ? r : "", sizeof newstr - (p - md->value));
						}
					}
					else
						snprintf(newstr + (p - md->value), sizeof newstr - (p - md->value), "%s=%s%s", target, bitmask_to_gflags(newflags), r != NULL ? r : "");
					break;
				}
				p = r;
			}
		}
		if (!found)
		{
			removeflags = 0;
			newflags = addflags;

			if ((addflags | removeflags) == 0)
				;
			else if (~restrictflags & addflags ||
					~restrictflags & removeflags ||
					~restrictflags & oldflags)
				denied = true;
			else if (md != NULL)
				snprintf(newstr + strlen(newstr), sizeof newstr - strlen(newstr), " %s=%s", target, bitmask_to_gflags(newflags));
			else
				snprintf(newstr, sizeof newstr, "%s=%s", target, bitmask_to_gflags(newflags));
		}
		if (oldflags == 0 && has_ctrl_chars(target))
		{
			command_fail(si, fault_badparams, _("Invalid template name \2%s\2."), target);
			return;
		}
		if ((addflags | removeflags) == 0)
		{
			if (oldflags != 0)
				command_fail(si, fault_nochange, _("Template \2%s\2 on \2%s\2 unchanged."), target, entity(mg)->name);
			else
				command_fail(si, fault_nosuch_key, _("No such template \2%s\2 on \2%s\2."), target, entity(mg)->name);
			return;
		}
		if (denied)
		{
			command_fail(si, fault_noprivs, _("You are not allowed to set \2%s\2 on template \2%s\2 in \2%s\2."), bitmask_to_gflags2(addflags, removeflags), target, entity(mg)->name);
			return;
		}
		if (strlen(newstr) >= 300)
		{
			command_fail(si, fault_toomany, _("Sorry, too many templates on \2%s\2."), entity(mg)->name);
			return;
		}
		p = md != NULL ? md->value : NULL;
		while (p != NULL)
		{
			while (*p == ' ')
			p++;
			q = strchr(p, '=');
			if (q == NULL)
					break;
			r = strchr(q, ' ');
			snprintf(ss,sizeof ss,"%.*s",r != NULL ? (int)(r - q - 1) : (int)strlen(q + 1), q + 1);
			if (gs_flags_parser(ss, 0, 0) == newflags)
			{
				command_fail(si, fault_alreadyexists, _("The template \2%.*s\2 already has flags \2%.*s\2."), (int)(q - p), p, r != NULL ? (int)(r - q - 1) : (int)strlen(q + 1), q + 1);
				return;
			}

			p = r;
		}

		if (newstr[0] == '\0')
			metadata_delete(mg, "private:templates");
		else
			metadata_add(mg, "private:templates", newstr);

		if (oldflags == 0)
		{
			command_success_nodata(si, _("Added template \2%s\2 with flags \2%s\2 in \2%s\2."),
				target, bitmask_to_gflags(newflags), entity(mg)->name);
			snprintf(description, sizeof description, "Added template \2%s\2 (new flags: %s)",
				target, bitmask_to_gflags(newflags));
		}
		else if (newflags == 0)
		{
			command_success_nodata(si, _("Removed template \2%s\2 from \2%s\2."),
				target, entity(mg)->name);
			snprintf(description, sizeof description, "Removed template \2%s\2",
				target);
		}
		else
		{
			command_success_nodata(si, _("Changed template \2%s\2 to \2%s\2 in \2%s\2."),
				target, bitmask_to_gflags(newflags), entity(mg)->name);
			snprintf(description, sizeof description, "Changed template \2%s\2 (new flags: %s)",
				target, bitmask_to_gflags(newflags));
		}

		notify_group_misc_change(si, mg, description);

		flagstr = bitmask_to_gflags2(addflags, removeflags);
		if (changegroupacs)
		{
			mowgli_node_t *n, *tn;
			groupacs_t *ga;
			int changes = 0, founderskipped = 0;
			char flagstr2[128];

			MOWGLI_ITER_FOREACH_SAFE(n, tn, mg->acs.head)
			{
				ga = n->data;
				if (ga->flags != oldflags)
					continue;
				if ((addflags | removeflags) & GA_FOUNDER)
				{
					founderskipped++;
					continue;
				}
				changes++;
				groupacs_modify_simple(ga, newflags, ~newflags);

				myuser_t *tmu = myuser_find(ga->mt->name);

				notify_target_acl_change(si, tmu, mg, flagstr, newflags);
				notify_group_acl_change(si, tmu, mg, flagstr, newflags);

				groupacs_close(ga);
			}
			logcommand(si, CMDLOG_SET, "TEMPLATE: \2%s\2 \2%s\2 !\2%s\2 (\2%d\2 change%s)", entity(mg)->name, target, flagstr, changes, changes == 1 ? "" : "s");
			mowgli_strlcpy(flagstr2, flagstr, sizeof flagstr2);

			command_success_nodata(si, _("%d access entr%s updated accordingly."), changes, changes == 1 ? "y" : "ies");
			if (founderskipped)
				command_success_nodata(si, _("Not updating %d access entries involving founder status. Please do it manually."), founderskipped);
		}
		else
			logcommand(si, CMDLOG_SET, "TEMPLATE: \2%s\2 \2%s\2 \2%s\2", entity(mg)->name, target, flagstr);
	}
}
