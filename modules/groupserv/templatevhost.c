/*
 * Copyright (c) 2016-2017 ChatLounge IRC Network Development Team
 *     (http://www.chatlounge.net/)
 *
 *     Provides the TEMPLATEVHOST command for GroupServ.  (Un)set
 * optional vhost offers for users based on group membership if the
 * membership matches an existing template for the group.
 *
 *
 */

#include "atheme.h"
#include "groupserv.h"
#include "template.h"
#include "../hostserv/hostserv.h"

DECLARE_MODULE_V1
(
	"groupserv/templatevhost", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	VENDOR_STRING
);

bool hostserv_loaded = false;

static bool *(*allow_vhost_change)(sourceinfo_t *si, myuser_t *target, bool shownotice) = NULL;
static unsigned int (*get_hostsvs_req_time)(void) = NULL;
static bool *(*get_hostsvs_limit_first_req)(void) = NULL;

static void gs_cmd_templatevhost(sourceinfo_t *si, int parc, char *parv[]);

command_t gs_templatevhost = { "TEMPLATEVHOST", N_("Manipulates available vhosts to group members based on template."),
		AC_NONE, 3, gs_cmd_templatevhost, { .path = "groupserv/templatevhost" } };

void _modinit(module_t *m)
{
	use_groupserv_main_symbols(m);

	if (module_request("hostserv/main"))
	{
		allow_vhost_change = module_locate_symbol("hostserv/main", "allow_vhost_change");
		get_hostsvs_req_time = module_locate_symbol("hostserv/main", "get_hostsvs_req_time");
		get_hostsvs_limit_first_req = module_locate_symbol("hostserv/main", "get_hostsvs_limit_first_req");
		hostserv_loaded = true;
	}
	else
		hostserv_loaded = false;

	service_named_bind_command("groupserv", &gs_templatevhost);
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("groupserv", &gs_templatevhost);
}

static void gs_cmd_templatevhost(sourceinfo_t *si, int parc, char *parv[])
{
	bool operoverride = false;
	metadata_t *md, *md2, *md3, *md4;
	mygroup_t *mg;
	mowgli_node_t *n;
	groupacs_t *ga;
	char *group = parv[0];
	char *target = parv[1];
	char *vhost = parv[2];
	char *p, *q, *r;
	char *s;
	char newstr[400], ss[60];
	int i = 0, j = 0;
	char templatevhostmask[128];
	char templatevhostmetadataname[128];
	char *templatevhostold;
	char templatename[128];
	char description[300];
	bool limit_first_req = get_hostsvs_limit_first_req();
	unsigned int request_time = get_hostsvs_req_time();

	if (!group)
	{
		command_fail(si, fault_nosuch_target, _("You must specify a group name."));
		return;
	}

	mg = mygroup_find(group);
	if (!mg)
	{
		command_fail(si, fault_nosuch_target, _("Group \2%s\2 is not registered."), group);
		return;
	}

	if (target)
	{
		snprintf(templatevhostmetadataname, sizeof templatevhostmetadataname,
			"private:templatevhost-%s", target);
	}

	if (!target)
	{
		/* No template name specified, so list the current settings. */

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
			command_success_nodata(si, "%-20s %s", _("Name"), _("V-Host Offer"));
			command_success_nodata(si, "%-20s %s", "--------------------", "----------");

			p = md->value;
			while (p != NULL)
			{
				struct tm tm;
				char moddate[64];
				time_t modtime;

				while (*p == ' ')
					p++;
				q = strchr(p, '=');
				if (q == NULL)
					break;
				r = strchr(q, ' ');
				snprintf(templatevhostmetadataname, sizeof templatevhostmetadataname,
					"private:templatevhost-%-0.*s", (int)(q - p), p);

				md2 = metadata_find(mg, templatevhostmetadataname);

				if (md2 != NULL)
				{
					snprintf(templatevhostmask, sizeof templatevhostmask,
						"%s", (char *)md2->value);

					while(templatevhostmask[i] != '|')
						i++;
				}
				else
				{
					snprintf(templatevhostmask, sizeof templatevhostmask, "<None>");
					i = 6;
				}

				while (p[j] != '=')
					j++;

				//*s = '\0';

				snprintf(templatename, 128, "%.*s", j, p);

				modtime = get_group_template_vhost_expiry(mg, templatename);

				tm = *localtime(&modtime);
				strftime(moddate, sizeof moddate, TIME_FORMAT, &tm);

				//command_success_nodata(si, "%-20.*s %s", (int)(q - p), p, md2 == NULL ? "<None>" : (char *)md2->value);
				if (md2 == NULL)
				{
					command_success_nodata(si, "%-20.*s <None>", (int)(q - p), p);
				}
				else
				{
					command_success_nodata(si, "%-20.*s %.*s [modified %s ago on %s]", (int)(q - p), p, i, templatevhostmask,
						time_ago(get_group_template_vhost_expiry(mg, templatename)),
						moddate);

					MOWGLI_ITER_FOREACH(n, mg->acs.head)
					{
						ga = n->data;

						if (templatename == NULL)
							continue;

						if (get_group_template_name(mg, ga->flags) == NULL)
							continue;

						if (!strcasecmp(templatename, get_group_template_name(mg, ga->flags)))
						{
							md4 = metadata_find(ga->mt, "private:usercloak");

							if (md4 == NULL)
								continue;

							md3 = metadata_find(ga->mt, "private:usercloak-timestamp");

							char *host = (char *)get_group_template_vhost_by_flags(mg, ga->flags);

							if (strstr(host, "$account"))
								replace(host, BUFSIZE, "$account", entity(ga->mt)->name);

							if (strcasecmp(md4->value, host))
								continue;

							if (md3 == NULL)
								command_success_nodata(si, "- %s (%s)", ga->mt->name);
							else
								command_success_nodata(si, "- %-18s [set %s ago]", ga->mt->name,
									time_ago(atoi(md3->value)));
						}
					}

				}
				//command_success_nodata(si, "%-20.*s %s", (int)(q - p), p, md2 == NULL ? "<None>" : strtok((char *)md2->value, "|"));
				//command_success_nodata(si, "Time: %s", strtok(NULL, "|"));

				//md2->value = s;

				p = r;

				i = 0; j = 0;
			}

			command_success_nodata(si, "%-20s %s", "--------------------", "----------");
			command_success_nodata(si, _("End of \2%s\2 TEMPLATEVHOST listing."), entity(mg)->name);
		}
		else
			command_success_nodata(si, _("No template vhost offers have been set on group: \2%s\2"), entity(mg)->name);

		if (operoverride)
			logcommand(si, CMDLOG_ADMIN, "TEMPLATEVHOST: \2%s\2 (oper override)", entity(mg)->name);
		else
			logcommand(si, CMDLOG_GET, "TEMPLATEVHOST: \2%s\2", entity(mg)->name);
	}
	else if (!vhost) /* Template name but no vhost, try to delete the template vhost. */
	{
		char oldvhost[80], *saveptr;

		// Code to check for GA_SET or oper override.
		if (!groupacs_sourceinfo_has_flag(mg, si, GA_SET) && !groupacs_sourceinfo_has_flag(mg, si, GA_FOUNDER))
		{
			if (has_priv(si, PRIV_GROUP_ADMIN))
				operoverride = true;
			else
			{
				command_fail(si, fault_noprivs, _("You are not authorized to perform this operation."));
				return;
			}
		}

		md = metadata_find(mg, "private:templates");

		if (md == NULL)
		{
			command_fail(si, fault_nosuch_key, _("No templates have been set on: \2%s\2"), entity(mg)->name);
			return;
		}

		md = metadata_find(mg, templatevhostmetadataname);

		if (md == NULL)
		{
			command_success_nodata(si, _("No vhost offer has been set on template \2%s\2 in group: \2%s\2"),
				target, entity(mg)->name);
			return;
		}

		/* Remove any matching vhost settings for users; if so, remove them. */
		MOWGLI_ITER_FOREACH(n, mg->acs.head)
		{
			ga = n->data;
			md2 = metadata_find(ga->mt, "private:usercloak");

			if (md2 == NULL)
				continue;

			char usertemplatevhost[128];

			if (get_group_template_vhost_by_flags(mg, ga->flags) == NULL)
				continue;

			myuser_t *mu = myuser_find_by_nick(entity(ga->mt)->name);

			mowgli_strlcpy(usertemplatevhost, get_group_template_vhost_by_flags(mg, ga->flags), BUFSIZE);

			if (strstr(usertemplatevhost, "$account"))
				replace(usertemplatevhost, BUFSIZE, "$account", entity(ga->mt)->name);

			if (!strcasecmp(usertemplatevhost, (char *)md2->value))
			{
				hs_sethost_all(mu, NULL, get_source_name(si));
				// Send notice/memo to affected users.
				do_sethost_all(mu, NULL);
				logcommand(si, CMDLOG_ADMIN, "VHOST:REMOVE: \2%s\2 because the template vhost offer for \2%s\2 on \2%s\2 has been removed.",
					entity(ga->mt)->name, target, entity(mg)->name);
			}
		}

		mowgli_strlcpy(oldvhost, strtok_r((char *)md->value, "|", &saveptr), sizeof oldvhost);

		logcommand(si, CMDLOG_GET, "TEMPLATEVHOST: \2%s\2 (formerly \2%s\2) removed on \2%s\2 %s",
			target, oldvhost, entity(mg)->name, operoverride ? "(override)" : "");
		command_success_nodata(si, _("The vhost offer for the template \2%s\2 on the group \2%s\2 has been dropped (formerly: \2%s\2)."),
			target, entity(mg)->name, oldvhost);

		snprintf(description, sizeof description, "Vhost offer for template \2%s\2 removed, formerly: \2%s\2",
			target, oldvhost);
		notify_group_misc_change(si, mg, description);

		metadata_delete(mg, templatevhostmetadataname);
	}
	else /* Template name and vhost specified.  Set the template to the new vhost mask. */
	{
		char vhoststring[300];

		/* Check for GA_SET, GA_FOUNDER, or oper override. */
		if (!groupacs_sourceinfo_has_flag(mg, si, GA_SET) && !groupacs_sourceinfo_has_flag(mg, si, GA_FOUNDER))
		{
			if (has_priv(si, PRIV_ADMIN) || has_priv(si, PRIV_GROUP_ADMIN) || has_priv(si, PRIV_USER_VHOSTOVERRIDE))
				operoverride = true;
			else
			{
				command_fail(si, fault_noprivs, _("You are not authorized to perform this operation."));
				return;
			}
		}

		md = metadata_find(mg, "private:templates");

		if (md == NULL)
		{
			command_fail(si, fault_nosuch_key, _("No templates have been set on: \2%s\2."), entity(mg)->name);
			return;
		}

		/* Check if the template exists. */
		if (!get_group_item(md->value, target))
		{
			command_fail(si, fault_nosuch_target, _("The template %s does not exist so you may not set a vhost offer for it."), target);
			return;
		}

		/* Check if the template vhost even exists. */

		md = metadata_find(mg, templatevhostmetadataname);

		snprintf(vhoststring, BUFSIZE, "%s|%u", vhost, (unsigned int)CURRTIME);

		/* Update existing users to the new vhost setting, if the user had an old matching vhost. */
		MOWGLI_ITER_FOREACH(n, mg->acs.head)
		{
			ga = n->data;
			md2 = metadata_find(ga->mt, "private:usercloak");

			if (md2 == NULL)
				continue;

			char usertemplatevhost[128];

			if (get_group_template_vhost_by_flags(mg, ga->flags) == NULL)
				continue;

			myuser_t *mu = myuser_find_by_nick(entity(ga->mt)->name);

			mowgli_strlcpy(usertemplatevhost, get_group_template_vhost_by_flags(mg, ga->flags), BUFSIZE);

			if (strstr(usertemplatevhost, "$account"))
				replace(usertemplatevhost, BUFSIZE, "$account", entity(ga->mt)->name);

			if (!strcasecmp(usertemplatevhost, (char *)md2->value))
			{
				char newusertemplatevhost[128];

				metadata_t *md_vhosttime = metadata_find(mu, "private:usercloak-timestamp");

				/* 86,400 seconds per day */
				if (limit_first_req && md_vhosttime == NULL && (CURRTIME - mu->registered > (request_time * 86400)))
				{
					hs_sethost_all(mu, NULL, get_source_name(si));
					// Send notice/memo to affected user.
					logcommand(si, CMDLOG_ADMIN, "VHOST:REMOVE: \2%s\2 by virtue of early template vhost offer change on: \2%s\2", entity(ga->mt)->name, entity(mg)->name);
					do_sethost_all(mu, NULL); // restore user vhost from user host
					continue;
				}

				time_t vhosttime = atoi(md_vhosttime->value);

				if (vhosttime + (request_time * 86400) > CURRTIME)
				{
					hs_sethost_all(mu, NULL, get_source_name(si));
					// Send notice/memo to affected user.
					logcommand(si, CMDLOG_ADMIN, "VHOST:REMOVE: \2%s\2 by virtue of early template vhost offer change on: \2%s\2", entity(ga->mt)->name, entity(mg)->name);
					do_sethost_all(mu, NULL); // restore user vhost from user host
					continue;
				}

				mowgli_strlcpy(newusertemplatevhost, vhost, BUFSIZE);

				if (strstr(newusertemplatevhost, "$account"))
					replace(newusertemplatevhost, BUFSIZE, "$account", entity(ga->mt)->name);

				hs_sethost_all(mu, newusertemplatevhost, get_source_name(si));
				// Send notice/memo to affected users (one per cycle).
				do_sethost_all(mu, newusertemplatevhost);
				logcommand(si, CMDLOG_ADMIN, "VHOST:REMOVE: \2%s\2 because the template vhost offer for \2%s\2 on \2%s\2 has been removed.",
					entity(ga->mt)->name, target, entity(mg)->name);
			}
		}

		if (md == NULL)
		{
			metadata_add(mg, templatevhostmetadataname, vhoststring);
			logcommand(si, CMDLOG_GET, "TEMPLATEVHOST: \2%s\2 for \2%s\2 added on \2%s\2 %s", vhost, target, entity(mg)->name,
				operoverride ? "(override)" : "");
			command_success_nodata(si, _("Added the vhost offer %s for the template %s in group: \2%s\2"), vhost, target, entity(mg)->name);

			snprintf(description, sizeof description, "Vhost offer for template \2%s\2 added: \2%s\2",
				target, vhost);
		}
		else
		{
			metadata_delete(mg, templatevhostmetadataname);
			metadata_add(mg, templatevhostmetadataname, vhoststring);
			logcommand(si, CMDLOG_GET, "TEMPLATEVHOST: \2%s\2 changed to \2%s\2 on \2%s\2 %s", target, vhost, entity(mg)->name,
				operoverride ? "(override)" : "");
			command_success_nodata(si, _("Changed the vhost offer for template %s to %s in group: \2%s\2"), target, vhost, entity(mg)->name);

			snprintf(description, sizeof description, "Vhost offer for template \2%s\2 changed to: \2%s\2",
				target, vhost);
		}

		notify_group_misc_change(si, mg, description);
	}
}
