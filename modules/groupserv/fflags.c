/*
 * Copyright (c) 2005 Atheme Development Group
 * Copyright (c) 2016-2017 ChatLounge IRC Network Development Team
 *     (http://www.chatlounge.net/)
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains routines to handle the GroupServ FFLAGS command.
 *
 */

#include "atheme.h"
#include "groupserv.h"
#include "../hostserv/hostserv.h"

DECLARE_MODULE_V1
(
	"groupserv/fflags", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	VENDOR_STRING
);

static unsigned int (*get_hostsvs_req_time)(void) = NULL;
static bool *(*get_hostsvs_limit_first_req)(void) = NULL;

static void gs_cmd_fflags(sourceinfo_t *si, int parc, char *parv[]);

command_t gs_fflags = { "FFLAGS", N_("Forces a flag change on a user in a group."), PRIV_GROUP_ADMIN, 3, gs_cmd_fflags, { .path = "groupserv/fflags" } };

static void gs_cmd_fflags(sourceinfo_t *si, int parc, char *parv[])
{
	mowgli_node_t *n;
	mygroup_t *mg;
	myentity_t *mt;
	groupacs_t *ga;
	unsigned int flags = 0, oldflags = 0, addflags = 0, removeflags = 0;

	if (!parv[0] || !parv[1] || !parv[2])
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "FFLAGS");
		command_fail(si, fault_needmoreparams, _("Syntax: FFLAGS <!group> [user] [changes]"));
		return;
	}

	if ((mg = mygroup_find(parv[0])) == NULL)
	{
		command_fail(si, fault_nosuch_target, _("The group \2%s\2 does not exist."), parv[0]);
		return;
	}

	if (si->smu == NULL)
	{
		command_fail(si, fault_noprivs, _("You are not logged in."));
		return;
	}

	if ((mt = myentity_find_ext(parv[1])) == NULL)
	{
		command_fail(si, fault_nosuch_target, _("\2%s\2 is not a registered account."), parv[1]);
		return;
	}

	ga = groupacs_find(mg, mt, 0, false);
	if (ga != NULL)
		flags = ga->flags;

	oldflags = flags;

	if (strchr(parv[2], '+') || strchr(parv[2], '-') || strchr(parv[2], '='))
		flags = gs_flags_parser(parv[2], 1, flags);
	else
	{
		flags = get_group_template_flags(mg, parv[2]);
		if (flags == 0)
		{
			command_fail(si, fault_badparams, _("Invalid template name on the group: %s"), entity(mg)->name);
			return;
		}
	}

	if (!(flags & GA_FOUNDER) && groupacs_find(mg, mt, GA_FOUNDER, false))
	{
		if (mygroup_count_flag(mg, GA_FOUNDER) == 1)
		{
			command_fail(si, fault_noprivs, _("You may not remove the last founder."));
			return;
		}

		if (!groupacs_sourceinfo_has_flag(mg, si, GA_FOUNDER))
		{
			command_fail(si, fault_noprivs, _("You may not remove a founder's +F access."));
			return;
		}
	}

	if (ga != NULL && flags != 0)
	{
		if (ga->flags != flags)
			ga->flags = flags;
		else
		{
			command_fail(si, fault_nochange, _("Group \2%s\2 access for \2%s\2 unchanged."), entity(mg)->name, mt->name);
			return;
		}
	}
	else if (ga != NULL)
	{
		metadata_t *md;

		groupacs_delete(mg, mt);

		addflags = flags;
		removeflags = GA_ALL_ALL & ~addflags;
		addflags &= ~oldflags;
		removeflags &= oldflags & ~addflags;

		command_success_nodata(si, _("\2%s\2 has been removed from \2%s\2."), mt->name, entity(mg)->name);
		wallops("\2%s\2 is removing flags for \2%s\2 on \2%s\2", get_oper_name(si), mt->name, entity(mg)->name);
		logcommand(si, CMDLOG_ADMIN, "FFLAGS:REMOVE: \2%s\2 on \2%s\2", mt->name, entity(mg)->name);

		if (isuser(mt))
		{
			notify_target_acl_change(si, user(mt), mg,
				bitmask_to_gflags2(addflags, removeflags), ga->flags);
			notify_group_acl_change(si, user(mt), mg,
				bitmask_to_gflags2(addflags, removeflags), ga->flags);
		}

		if (isuser(mt) && module_locate_symbol("hostserv/main", "get_hostsvs_req_time") &&
			(get_group_template_vhost_by_flags(mg, oldflags)) != NULL &&
			(md = metadata_find(mt, "private:usercloak")) != NULL)
		{
			char templatevhost[128];

			myuser_t *mu = myuser_find_by_nick(parv[1]);

			mowgli_strlcpy(templatevhost, get_group_template_vhost_by_flags(mg, oldflags), BUFSIZE);

			if (strstr(templatevhost, "$account"))
				replace(templatevhost, BUFSIZE, "$account", entity(mt)->name);

			if (!strcasecmp(md->value, templatevhost))
			{
				hs_sethost_all(mu, NULL, get_source_name(si));
				// Send notice/memo to affected user.
				logcommand(si, CMDLOG_ADMIN, "VHOST:REMOVE: \2%s\2 by virtue of flags change on: \2%s\2", entity(mt)->name, entity(mg)->name);
				do_sethost_all(mu, NULL); // restore user vhost from user host
			}
		}

		return;
	}
	else if (flags != 0)
	{
		if (MOWGLI_LIST_LENGTH(&mg->acs) > gs_config->maxgroupacs && (!(mg->flags & MG_ACSNOLIMIT)))
		{
			command_fail(si, fault_toomany, _("Group %s access list is full."), entity(mg)->name);
			return;
		}
		ga = groupacs_add(mg, mt, flags);
	}
	else
	{
		command_fail(si, fault_nochange, _("Group \2%s\2 access for \2%s\2 unchanged."), entity(mg)->name, mt->name);
		return;
	}

	/* Remove the user's vhost setting, if the user's flags are changed or removed.
	 * However, change the user's vhost setting, *if* the user is being granted flags that
	 * match another TEMPLATEVHOST offer, *and* he previously held a group template vhost offer,
	 * and the vhost cooldown for the user has passed.
	 */

	get_hostsvs_req_time = module_locate_symbol("hostserv/main", "get_hostsvs_req_time");
	get_hostsvs_limit_first_req = module_locate_symbol("hostserv/main", "get_hostsvs_limit_first_req");

	if (get_hostsvs_req_time != NULL && get_hostsvs_limit_first_req != NULL)
	{
		if (isuser(mt) && (get_group_template_vhost_by_flags(mg, oldflags)) != NULL)
		{
			mowgli_node_t *o;
			bool matches = false;
			myuser_t *mu = myuser_find_by_nick(parv[1]);
			bool limit_first_req = get_hostsvs_limit_first_req();
			unsigned int request_time = get_hostsvs_req_time();

			MOWGLI_ITER_FOREACH(n, mu->nicks.head)
			{
				char templatevhost[128];
				metadata_t *md;

				if ((md = metadata_find(mt, "private:usercloak")) == NULL)
					break;

				mowgli_strlcpy(templatevhost, get_group_template_vhost_by_flags(mg, oldflags), BUFSIZE);

				if (strstr(templatevhost, "$account"))
						replace(templatevhost, BUFSIZE, "$account", entity(mt)->name);

				if (!strcasecmp(md->value, templatevhost))
				{
					if (!has_priv(si, PRIV_USER_VHOSTOVERRIDE) && !has_priv(si, PRIV_GROUP_ADMIN) && !has_priv(si, PRIV_ADMIN))
					{
						metadata_t *md_vhosttime = metadata_find(mu, "private:usercloak-timestamp");

						/* 86,400 seconds per day */
						if (limit_first_req && md_vhosttime == NULL && (CURRTIME - mu->registered > (request_time * 86400)))
						{
							hs_sethost_all(mu, NULL, get_source_name(si));
							// Send notice/memo to affected user.
							logcommand(si, CMDLOG_ADMIN, "VHOST:REMOVE: \2%s\2 by virtue of early flags change on: \2%s\2", entity(mt)->name, entity(mg)->name);
							do_sethost_all(mu, NULL); // restore user vhost from user host
							break;
						}

						time_t vhosttime = atoi(md_vhosttime->value);

						if (vhosttime + (request_time * 86400) > CURRTIME)
						{
							hs_sethost_all(mu, NULL, get_source_name(si));
							// Send notice/memo to affected user.
							logcommand(si, CMDLOG_ADMIN, "VHOST:REMOVE: \2%s\2 by virtue of early flags change on: \2%s\2", entity(mt)->name, entity(mg)->name);
							do_sethost_all(mu, NULL); // restore user vhost from user host
							break;
						}
					}

					// Check if the new flags have a vhost offer.

					if (get_group_template_vhost_by_flags(mg, flags))
					{
						char newtemplatevhost[128];
						mowgli_strlcpy(newtemplatevhost, get_group_template_vhost_by_flags(mg, flags), BUFSIZE);

						if (strstr(newtemplatevhost, "$account"))
							replace(newtemplatevhost, BUFSIZE, "$account", entity(mt)->name);

						hs_sethost_all(mu, newtemplatevhost, get_source_name(si));
						do_sethost_all(mu, newtemplatevhost);
						// Send notice/memo to affected user.
						logcommand(si, CMDLOG_ADMIN, "VHOST:CHANGE: from \2%s\2 to \2%s\2 on \2%s\2 by virtue of flags change on: \2%s\2",
							templatevhost,
							newtemplatevhost,
							entity(mt)->name,
							entity(mg)->name);

						matches = true;
						break;
					}
				}

				if (matches)
					break;
			}
		}
	}

	addflags = flags;
	removeflags = GA_ALL_ALL & ~addflags;
	addflags &= ~oldflags;
	removeflags &= oldflags & ~addflags;

	MOWGLI_ITER_FOREACH(n, entity(mg)->chanacs.head)
	{
		chanacs_t *ca = n->data;

		verbose(ca->mychan, "\2%s\2 now has flags \2%s\2 in the group \2%s\2 which communally has \2%s\2 on \2%s\2.",
			mt->name, gflags_tostr(ga_flags, ga->flags), entity(mg)->name,
			bitmask_to_flags(ca->level), ca->mychan->name);

		hook_call_channel_acl_change(&(hook_channel_acl_req_t){ .ca = ca });
	}

	if (get_group_template_name(mg, ga->flags))
	{
		wallops("\2%s\2 is modifying flags (\2%s\2) for \2%s\2 (TEMPLATE: \2%s\2) on \2%s\2", get_oper_name(si), gflags_tostr(ga_flags, ga->flags),
			mt->name, get_group_template_name(mg, ga->flags), entity(mg)->name);
		logcommand(si, CMDLOG_ADMIN, "FFLAGS: \2%s\2 now has flags \2%s\2 (TEMPLATE: \2%s\2) on \2%s\2", mt->name, gflags_tostr(ga_flags,  ga->flags),
			get_group_template_name(mg, ga->flags), entity(mg)->name);
		command_success_nodata(si, _("\2%s\2 now has flags \2%s\2 (TEMPLATE: \2%s\2) on: \2%s\2"),
			mt->name, gflags_tostr(ga_flags, ga->flags),
			get_group_template_name(mg, ga->flags), entity(mg)->name);
	}
	else
	{
		wallops("\2%s\2 is modifying flags (\2%s\2) for \2%s\2 on \2%s\2", get_oper_name(si), gflags_tostr(ga_flags, ga->flags), mt->name, entity(mg)->name);
		logcommand(si, CMDLOG_ADMIN, "FFLAGS: \2%s\2 now has flags \2%s\2 on \2%s\2", mt->name, gflags_tostr(ga_flags,  ga->flags), entity(mg)->name);
		command_success_nodata(si, _("\2%s\2 now has flags \2%s\2 on: \2%s\2"), mt->name, gflags_tostr(ga_flags, ga->flags), entity(mg)->name);
	}

	if (isuser(mt))
	{
		notify_target_acl_change(si, user(mt), mg,
			bitmask_to_gflags2(addflags, removeflags), ga->flags);
		notify_group_acl_change(si, user(mt), mg,
			bitmask_to_gflags2(addflags, removeflags), ga->flags);
	}
}

void _modinit(module_t *m)
{
	use_groupserv_main_symbols(m);

	service_named_bind_command("groupserv", &gs_fflags);
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("groupserv", &gs_fflags);
}
