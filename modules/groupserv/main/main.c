/* groupserv - group services
 * Copyright (c) 2010 Atheme Development Group
 * Copyright (c) 2016-2017 ChatLounge IRC Network Development Team
 *     (http://www.chatlounge.net/)
 */

#include "atheme.h"
#include "groupserv_main.h"

DECLARE_MODULE_V1
(
	"groupserv/main", MODULE_UNLOAD_CAPABILITY_RELOAD_ONLY, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net>"
);

service_t *groupsvs;

void (*add_history_entry)(sourceinfo_t *si, mygroup_t *mg, const char *desc) = NULL;
bool *(*send_user_memo)(sourceinfo_t *si, myuser_t *target,
	const char *memotext, bool verbose, unsigned int status, bool senduseremail) = NULL;

typedef struct {
	int version;

	mowgli_heap_t *mygroup_heap;
	mowgli_heap_t *groupacs_heap;
} groupserv_persist_record_t;

extern mowgli_heap_t *mygroup_heap, *groupacs_heap;

static int g_gi_templates(mowgli_config_file_entry_t *ge)
{
	clear_global_group_template_flags();

	mowgli_config_file_entry_t *flge;

	MOWGLI_ITER_FOREACH(flge, ge->entries)
	{
		if (flge->vardata == NULL)
		{
			conf_report_warning(ge, "no parameter for configuration option");
			return 0;
		}

		set_global_group_template_flags(flge->varname, gs_flags_parser(flge->vardata, 0, 0));
	}

	return 0;
}

/* notify_target_acl_change: Tells the target user about the new flags in a channel,
 *     both in the form of a notice (if possible) and a memo (if possible).
 *
 * Inputs: myuser_t *smu - Source user
 *         myuser_t *tmu - Target user
 *         mygroup_t *mg - Group where the change is taking place.
 *         const char *flagstr - Flag string used to perform the change.
 *         unsigned int flags - current flags on the target user.
 * Outputs: None
 * Side Effects: Send a notice and/or memo to the target user, if possible.
 */
void notify_target_acl_change(sourceinfo_t *si, myuser_t *tmu, mygroup_t *mg,
	const char *flagstr, unsigned int flags)
{
	char text[256];
	char text2[300];

	if (si->smu == NULL || tmu == NULL || mg == NULL || flagstr == NULL)
		return;

	if (!(tmu->flags & MU_NOTIFYACL))
		return;

	if (flags == 0)
		snprintf(text, sizeof text, "\2%s\2 has set \2%s\2 and removed you from: \2%s\2",
			entity(si->smu)->name, flagstr, entity(mg)->name);
	else if (get_group_template_name(mg, flags))
		snprintf(text, sizeof text, "\2%s\2 has set \2%s\2 on you in \2%s\2 where you now have the flags: \2%s\2 (TEMPLATE: \2%s\2)",
			entity(si->smu)->name, flagstr, entity(mg)->name, gflags_tostr(ga_flags, flags),
			get_group_template_name(mg, flags));
	else
		snprintf(text, sizeof text, "\2%s\2 has set \2%s\2 on you in \2%s\2 where you now have the flags: \2%s\2",
			entity(si->smu)->name, flagstr, entity(mg)->name, gflags_tostr(ga_flags, flags));

	if (MOWGLI_LIST_LENGTH(&tmu->logins) > 0)
		myuser_notice(groupsvs->nick, tmu, text);

	if (tmu->flags & MU_NOMEMO || !(tmu->flags & MU_NOTIFYMEMO))
		return;

	if ((send_user_memo = module_locate_symbol("memoserv/main", "send_user_memo")) == NULL)
		return;

	snprintf(text2, sizeof text2, "[automatic memo from \2%s\2] - %s", groupsvs->nick, text);

	send_user_memo(si, tmu, text2, false, MEMO_CHANNEL, tmu->flags & MU_EMAILNOTIFY);

	return;
}

/* notify_group_acl_change: Notifes the target group management about
 *     the new flags in a group in the form of a memo (if possible).
 *
 * Inputs: myuser_t *smu - Source user
 *         myuser_t *tmu - Target user
 *         mygroup_t *mg - Group where the change is taking place.
 *         const char *flagstr - Flag string used to perform the change.
 *         unsigned int flags - current flags on the target user.
 * Outputs: None
 * Side Effects: Send a notice and/or memo to the target user, if possible.
 */
void notify_group_acl_change(sourceinfo_t *si, myuser_t *tmu, mygroup_t *mg,
	const char *flagstr, unsigned int flags)
{
	char text[256], text1[256], text2[300];
	groupacs_t *ga;
	mowgli_node_t *m;

	if (flags == 0)
	{
		snprintf(text, sizeof text, "\2%s\2 has set \2%s\2 and removed \2%s\2 from: \2%s\2",
			entity(si->smu)->name, flagstr, entity(tmu)->name, entity(mg)->name);
		snprintf(text1, sizeof text1, "\2%s\2 set \2%s\2, removing: \2%s\2",
			entity(si->smu)->name, flagstr, entity(tmu)->name, entity(mg)->name);
	}
	else if (get_group_template_name(mg, flags))
	{
		snprintf(text, sizeof text, "\2%s\2 has set \2%s\2 on \2%s\2 in \2%s\2 who now has the flags: \2%s\2 (TEMPLATE: \2%s\2)",
			entity(si->smu)->name, flagstr,
			entity(tmu)->name, entity(mg)->name,
			gflags_tostr(ga_flags, flags),
			get_group_template_name(mg, flags));
		snprintf(text1, sizeof text1, "\2%s\2 set \2%s\2 on \2%s\2 who now has flags: \2%s\2 (TEMPLATE: \2%s\2)",
			entity(si->smu)->name, flagstr,
			entity(tmu)->name, gflags_tostr(ga_flags, flags),
			get_group_template_name(mg, flags));
	}
	else
	{
		snprintf(text, sizeof text, "\2%s\2 has set \2%s\2 on \2%s\2 in \2%s\2 who now has the flags: \2%s\2",
			entity(si->smu)->name, flagstr,
			entity(tmu)->name, entity(mg)->name,
			gflags_tostr(ga_flags, flags));
		snprintf(text1, sizeof text1, "\2%s\2 has set \2%s\2 on \2%s\2 who now has the flags: \2%s\2",
			entity(si->smu)->name, flagstr,
			entity(tmu)->name, gflags_tostr(ga_flags, flags));
	}

	snprintf(text2, sizeof text2, "[automatic memo from \2%s\2] - %s", groupsvs->nick, text);

	add_history_entry = module_locate_symbol("groupserv/history", "add_history_entry");

	if (add_history_entry != NULL)
		add_history_entry(si, mg, text1);

	MOWGLI_ITER_FOREACH(m, mg->acs.head)
	{
		ga = m->data;

		/* Don't run this for the target, if the target has MU_NOTIFYACL enabled, as this
		 * should be covered under notify_target_acl_change.  Prevents double notices. */
		if (ga->mt == entity(tmu) && tmu->flags & MU_NOTIFYACL)
			continue;

		/* Skip if the entity isn't a NickServ account as this would not make sense.
		 * ToDo: Make it work for group memos?
		 */
		if (!isuser(ga->mt))
			continue;

		/* Check for +F and/or +s.  If not, skip.
		 * ToDo: Have this as default, have channel configuration setting? */
		if (!(ga->flags & GA_FOUNDER || ga->flags & GA_SET))
			continue;

		/* Send the notice to everyone but the source. */
		if (si->smu != user(ga->mt) && user(ga->mt)->flags & MU_NOTIFYACL)
			myuser_notice(groupsvs->nick, user(ga->mt), text);

		if (!(user(ga->mt)->flags & MU_NOMEMO) && user(ga->mt)->flags & MU_NOTIFYMEMO &&
			user(ga->mt)->flags & MU_NOTIFYACL &&
			(send_user_memo = module_locate_symbol("memoserv/main", "send_user_memo")) != NULL)
			send_user_memo(si, user(ga->mt), text2, false, MEMO_CHANNEL, user(ga->mt)->flags & MU_EMAILNOTIFY);
	}
}

/* notify_group_set_change: Notifes the target group management about
 *     the new settings in a group in the form of a memo (if possible).
 *     Additionally, if group/history is loaded, add another history entry.
 *     Additionally, send a private notice to every member of group management
 *     with +F and/or +s who is online and has NOTIFY_ACL enabled.
 *
 * Inputs: myuser_t *smu - Source user
 *         myuser_t *tmu - Target user
 *         mygroup_t *mg - Group where the change is taking place.
 *         const char *settingname - Name of the setting changed.
 *         const char *setting - Actual setting.. usually "ON" or "OFF" but some settings have other options.
 * Outputs: None
 * Side Effects: Send a notice and/or memo to the target user, if possible.
 */
void notify_group_set_change(sourceinfo_t *si, myuser_t *tmu, mygroup_t *mg,
	const char *settingname, const char *setting)
{
	char text[256], text1[256], text2[300];
	groupacs_t *ga;
	mowgli_node_t *m;

	snprintf(text, sizeof text, "\2%s\2 has set \2%s\2 on channel \2%s\2 to: \2%s\2",
		entity(tmu)->name, settingname, entity(mg)->name, setting);

	snprintf(text1, sizeof text1, "\2%s\2 setting changed to: \2%s\2", settingname, setting);

	snprintf(text2, sizeof text2, "[automatic memo from \2%s\2] - %s", groupsvs->nick, text);

	add_history_entry = module_locate_symbol("groupserv/history", "add_history_entry");

	if (add_history_entry != NULL)
		add_history_entry(si, mg, text1);

	MOWGLI_ITER_FOREACH(m, mg->acs.head)
	{
		ga = m->data;

		/* Skip if the entity isn't a NickServ account.  ToDo: Make it work for group memos? */
		if (!isuser(ga->mt))
			continue;

		/* Don't send a duplicate notification to the source user. */
		//if (si->smu == tmu)
			//continue;

		/* Check for +F, +R, and/or +s.  If not, skip.
		 * ToDo: Have this as default, have channel configuration setting? */
		if (!(ga->flags & GA_FOUNDER || ga->flags & GA_SET))
			continue;

		/* Send the notice to everyone but the source. */
		if (si->smu != user(ga->mt) && user(ga->mt)->flags & MU_NOTIFYACL)
			myuser_notice(groupsvs->nick, user(ga->mt), text);

		if (!(user(ga->mt)->flags & MU_NOMEMO) && user(ga->mt)->flags & MU_NOTIFYMEMO && user(ga->mt)->flags & MU_NOTIFYACL &&
			(send_user_memo = module_locate_symbol("memoserv/main", "send_user_memo")) != NULL)
			send_user_memo(si, user(ga->mt), text2, false, MEMO_CHANNEL, user(ga->mt)->flags & MU_EMAILNOTIFY);
	}
}

void _modinit(module_t *m)
{
	groupserv_persist_record_t *rec = mowgli_global_storage_get("atheme.groupserv.main.persist");

	if (rec == NULL)
		mygroups_init();
	else
	{
		myentity_iteration_state_t iter;
		myentity_t *grp;

		mygroup_heap = rec->mygroup_heap;
		groupacs_heap = rec->groupacs_heap;

		mowgli_global_storage_free("atheme.groupserv.main.persist");
		free(rec);

		MYENTITY_FOREACH_T(grp, &iter, ENT_GROUP)
		{
			continue_if_fail(isgroup(grp));

			mygroup_set_chanacs_validator(grp);
		}
	}

	groupsvs = service_add("groupserv", NULL);
	add_uint_conf_item("MAXGROUPS", &groupsvs->conf_table, 0, &gs_config.maxgroups, 0, 65535, 5);
	add_uint_conf_item("MAXGROUPACS", &groupsvs->conf_table, 0, &gs_config.maxgroupacs, 0, 65535, 0);
	add_bool_conf_item("ENABLE_OPEN_GROUPS", &groupsvs->conf_table, 0, &gs_config.enable_open_groups, false);
	add_bool_conf_item("NO_LEVELED_FLAGS", &groupsvs->conf_table, 0, &gs_config.no_leveled_flags, false);
	add_dupstr_conf_item("JOIN_FLAGS", &groupsvs->conf_table, 0, &gs_config.join_flags, "+");
	//add_conf_item("TEMPLATES", &groupsvs->conf_table, g_gi_templates);

	gs_db_init();
	gs_hooks_init();
}

void _moddeinit(module_unload_intent_t intent)
{
	gs_db_deinit();
	gs_hooks_deinit();
	del_conf_item("MAXGROUPS", &groupsvs->conf_table);
	del_conf_item("MAXGROUPACS", &groupsvs->conf_table);
	del_conf_item("ENABLE_OPEN_GROUPS", &groupsvs->conf_table);
	del_conf_item("NO_LEVELED_FLAGS", &groupsvs->conf_table);
	del_conf_item("JOIN_FLAGS", &groupsvs->conf_table);
	del_conf_item("TEMPLATES", &groupsvs->conf_table);

	if (groupsvs)
		service_delete(groupsvs);

	switch (intent)
	{
		case MODULE_UNLOAD_INTENT_RELOAD:
		{
			groupserv_persist_record_t *rec = smalloc(sizeof(groupserv_persist_record_t));

			rec->version = 1;
			rec->mygroup_heap = mygroup_heap;
			rec->groupacs_heap = groupacs_heap;

			mowgli_global_storage_put("atheme.groupserv.main.persist", rec);
			break;
		}

		case MODULE_UNLOAD_INTENT_PERM:
		default:
			mygroups_deinit();
			break;
	}
}
