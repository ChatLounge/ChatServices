/*
 * Copyright (c) 2005 William Pitcock <nenolod -at- nenolod.net>
 * Copyright (c) 2016 ChatLounge IRC Network Development Team
 *
 * Rights to this code are as documented in doc/LICENSE.
 *
 * Allows opers to offer vhosts to users
 *
 */

#include "atheme.h"
#include "hostserv.h"
#include "../groupserv/groupserv.h"

static bool *(*allow_vhost_change)(sourceinfo_t *si, myuser_t *target, bool shownotice) = NULL;

DECLARE_MODULE_V1
(
	"hostserv/offer", true, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net/>"
);

static void hs_cmd_offer(sourceinfo_t *si, int parc, char *parv[]);
static void hs_cmd_unoffer(sourceinfo_t *si, int parc, char *parv[]);
static void hs_cmd_offerlist(sourceinfo_t *si, int parc, char *parv[]);
static void hs_cmd_take(sourceinfo_t *si, int parc, char *parv[]);

static void write_hsofferdb(database_handle_t *db);
static void db_h_ho(database_handle_t *db, const char *type);

command_t hs_offer = { "OFFER", N_("Sets vhosts available for users to take."), PRIV_USER_VHOST, 2, hs_cmd_offer, { .path = "hostserv/offer" } };
command_t hs_unoffer = { "UNOFFER", N_("Removes a vhost from the list that users can take."), PRIV_USER_VHOST, 2, hs_cmd_unoffer, { .path = "hostserv/unoffer" } };
command_t hs_offerlist = { "OFFERLIST", N_("Lists all available vhosts."), AC_NONE, 1, hs_cmd_offerlist, { .path = "hostserv/offerlist" } };
command_t hs_take = { "TAKE", N_("Take an offered vhost for use."), AC_AUTHENTICATED, 2, hs_cmd_take, { .path = "hostserv/take" } };

struct hsoffered_ {
	char *vhost;
	time_t vhost_ts;
	stringref creator;
	myentity_t *group;
	mowgli_node_t node;
};

typedef struct hsoffered_ hsoffered_t;

mowgli_list_t hs_offeredlist;

void _modinit(module_t *m)
{
	if (!module_find_published("backend/opensex"))
	{
		slog(LG_INFO, "Module %s requires use of the OpenSEX database backend, refusing to load.", m->name);
		m->mflags = MODTYPE_FAIL;
		return;
	}

	hook_add_db_write(write_hsofferdb);
	db_register_type_handler("HO", db_h_ho);

	use_groupserv_main_symbols(m);

	service_named_bind_command("hostserv", &hs_offer);
	service_named_bind_command("hostserv", &hs_unoffer);
	service_named_bind_command("hostserv", &hs_offerlist);
	service_named_bind_command("hostserv", &hs_take);

	allow_vhost_change = module_locate_symbol("hostserv/main", "allow_vhost_change");
}

void _moddeinit(module_unload_intent_t intent)
{
	hook_del_db_write(write_hsofferdb);
	db_unregister_type_handler("HO");

 	service_named_unbind_command("hostserv", &hs_offer);
	service_named_unbind_command("hostserv", &hs_unoffer);
	service_named_unbind_command("hostserv", &hs_offerlist);
	service_named_unbind_command("hostserv", &hs_take);
}

static void write_hsofferdb(database_handle_t *db)
{
	mowgli_node_t *n;

	MOWGLI_ITER_FOREACH(n, hs_offeredlist.head)
	{
		hsoffered_t *l = n->data;

		db_start_row(db, "HO");

		if (l->group != NULL)
			db_write_word(db, l->group->name);

		db_write_word(db, l->vhost);
		db_write_time(db, l->vhost_ts);
		db_write_word(db, l->creator);
		db_commit_row(db);
	}

}

static void db_h_ho(database_handle_t *db, const char *type)
{
	hsoffered_t *l;
	const char *buf;
	time_t vhost_ts;
	const char *creator;
	myentity_t *mt = NULL;

	buf = db_sread_word(db);
	if (*buf == '!')
	{
		mt = myentity_find(buf);
		buf = db_sread_word(db);
	}

	vhost_ts = db_sread_time(db);
	creator = db_sread_word(db);

	l = smalloc(sizeof(hsoffered_t));
	l->group = mt;
	l->vhost = sstrdup(buf);
	l->vhost_ts = vhost_ts;
	l->creator = strshare_get(creator);

	mowgli_node_add(l, &l->node, &hs_offeredlist);
}

static inline hsoffered_t *hs_offer_find(const char *host, myentity_t *mt)
{
	mowgli_node_t *n;
	hsoffered_t *l;

	MOWGLI_ITER_FOREACH(n, hs_offeredlist.head)
	{
		l = n->data;

		if (!irccasecmp(l->vhost, host) && (l->group == mt || mt == NULL))
			return l;
	}

	return NULL;
}

/* OFFER <host> */
static void hs_cmd_offer(sourceinfo_t *si, int parc, char *parv[])
{
	char *group = parv[0];
	char *host;
	hsoffered_t *l;
	myentity_t *mt = NULL;

	if (!group)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "OFFER");
		command_fail(si, fault_needmoreparams, _("Syntax: OFFER [!group] <vhost>"));
		return;
	}

	if (*group != '!')
	{
		host = group;
		group = NULL;
	}
	else
		host = parv[1];

	if (!host)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "OFFER");
		command_fail(si, fault_needmoreparams, _("Syntax: OFFER [!group] <vhost>"));
		return;
	}

	if (si->smu == NULL)
	{
		command_fail(si, fault_noprivs, _("You are not logged in."));
		return;
	}

	if (group != NULL)
	{
		mt = myentity_find(group);
		if (mt == NULL)
		{
			command_fail(si, fault_badparams, _("The requested group does not exist."));
			return;
		}
	}

	l = hs_offer_find(host, mt);
	if (l != NULL)
	{
		command_fail(si, fault_badparams, _("The requested offer already exists."));
		return;
	}

	l = smalloc(sizeof(hsoffered_t));

	l->group = mt;
	l->vhost = sstrdup(host);
	l->vhost_ts = CURRTIME;;
	l->creator = strshare_ref(entity(si->smu)->name);

	mowgli_node_add(l, &l->node, &hs_offeredlist);

	command_success_nodata(si, _("You have offered vhost \2%s\2."), host);
	logcommand(si, CMDLOG_ADMIN, "OFFER: \2%s\2", host);

	return;
}

/* UNOFFER <vhost> */
static void hs_cmd_unoffer(sourceinfo_t *si, int parc, char *parv[])
{
	char *host = parv[0];
	hsoffered_t *l;
	mowgli_node_t *n;

	if (!host)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "UNOFFER");
		command_fail(si, fault_needmoreparams, _("Syntax: UNOFFER <vhost>"));
		return;
	}

	l = hs_offer_find(host, NULL);
	if (l == NULL)
	{
		command_fail(si, fault_nosuch_target, _("vhost \2%s\2 not found in vhost offer database."), host);
		return;
	}

	logcommand(si, CMDLOG_ADMIN, "UNOFFER: \2%s\2", host);

	while (l != NULL)
	{
		mowgli_node_delete(&l->node, &hs_offeredlist);

		strshare_unref(l->creator);
		free(l->vhost);
		free(l);

		l = hs_offer_find(host, NULL);
	}

	command_success_nodata(si, _("You have unoffered vhost \2%s\2."), host);
}

static bool myuser_is_in_group(myuser_t *mu, myentity_t *mt)
{
	mygroup_t *mg;
	mowgli_node_t *n;

	return_val_if_fail(mt != NULL, false);
	return_val_if_fail((mg = group(mt)) != NULL, false);

	if (!mu) /* NULL users can't be in groups! :o */
		return false;

	MOWGLI_ITER_FOREACH(n, mg->acs.head)
	{
		groupacs_t *ga = n->data;

		if (ga->mt == entity(mu) && ga->flags & GA_VHOST)
			return true;
	}

	return false;
}

/* update_vhost_on_account_name_change:
 *
 *     This function checks if the user has an existing vhost that
 * uses the $account token.  If it does, update the vhost to show
 * the new account name.  Intended to be used by nickserv/set_accountname.
 *
 * Inputs: myuser_t *mu: User who's vhost is being changed.
 *         const char *newname: New account name.
 * Outputs: Returns the new vhost if a change occurred, NULL otherwise.
 * Side Effects: User's vhost may be updated, depending.
 */
const char *update_vhost_on_account_name_change(myuser_t *mu, const char *newname)
{
	groupacs_t *ga;
	hsoffered_t *l;
	mowgli_node_t *n;
	mowgli_list_t *ml;
	metadata_t *md;
	char vhoststring[128];
	static char newvhoststring[128];

	if (mu == NULL)
		return NULL;

	if (newname == NULL)
		return NULL;

	if ((md = metadata_find(mu, "private:usercloak")) == NULL)
		return NULL;

	MOWGLI_ITER_FOREACH(n, hs_offeredlist.head)
	{
		l = n->data;

		if (l->group != NULL && !myuser_is_in_group(mu, l->group))
			continue;

		if (!strstr(l->vhost, "$account"))
			continue;

		mowgli_strlcpy(vhoststring, l->vhost, sizeof vhoststring);
		replace(vhoststring, BUFSIZE, "$account", entity(mu)->name);

		if (!strcmp(vhoststring, md->value))
		{
			mowgli_strlcpy(newvhoststring, l->vhost, sizeof newvhoststring);
			replace(newvhoststring, BUFSIZE, "$account", newname);

			hs_sethost_all(mu, newvhoststring, newname);
			do_sethost_all(mu, newvhoststring);

			return newvhoststring;
		}
	}

	ml = myentity_get_membership_list(entity(mu));

	if (MOWGLI_LIST_LENGTH(ml) == 0)
		return false;

	MOWGLI_ITER_FOREACH(n, ml->head)
	{
		ga = n->data;

		if (ga->mt != entity(mu))
			continue;

		if ((ga->flags & GA_BAN))
			continue;

		if (!strstr(get_group_template_vhost_by_flags(ga->mg, ga->flags), "$account"))
			continue;

		mowgli_strlcpy(vhoststring, get_group_template_vhost_by_flags(ga->mg, ga->flags), sizeof vhoststring);
		replace(vhoststring, BUFSIZE, "$account", entity(mu)->name);

		if (!strcmp(vhoststring, md->value))
		{
			mowgli_strlcpy(newvhoststring, get_group_template_vhost_by_flags(ga->mg, ga->flags), sizeof newvhoststring);
			replace(newvhoststring, BUFSIZE, "$account", newname);

			hs_sethost_all(mu, newvhoststring, newname);
			do_sethost_all(mu, newvhoststring);

			return newvhoststring;
		}
	}

	return NULL;
}

/* TAKE <vhost> */
static void hs_cmd_take(sourceinfo_t *si, int parc, char *parv[])
{
	char *host = parv[0];
	hsoffered_t *l;
	mowgli_node_t *n, *o;
	mowgli_list_t *ml;
	char templatevhost[128], templatevhost2[128];

	if (!host)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "TAKE");
		command_fail(si, fault_needmoreparams, _("Syntax: TAKE <vhost>"));
		return;
	}

	if (si->smu == NULL)
	{
		command_fail(si, fault_nochange, _("You can't take vhosts when not logged in"));
		return;
	}

	if (metadata_find(si->smu, "private:restrict:setter"))
	{
		command_fail(si, fault_noprivs, _("You have been restricted from taking vhosts by network staff"));
		return;
	}

	if (!allow_vhost_change(si, si->smu, true))
		return;

	MOWGLI_ITER_FOREACH(n, hs_offeredlist.head)
	{
		l = n->data;

		if (l->group != NULL && !myuser_is_in_group(si->smu, l->group))
			continue;

		if (!irccasecmp(l->vhost, host))
		{
			if (strstr(host, "$account"))
				replace(host, BUFSIZE, "$account", entity(si->smu)->name);

			if (!check_vhost_validity(si, host))
				return;

			logcommand(si, CMDLOG_GET, "TAKE: \2%s\2 for \2%s\2", host, entity(si->smu)->name);

			command_success_nodata(si, _("You have taken vhost \2%s\2."), host);
			hs_sethost_all(si->smu, host, get_source_name(si));
			do_sethost_all(si->smu, host);

			return;
		}
	}

	// Code for group template-based vhost offers.

	ml = myentity_get_membership_list(entity(si->smu));

	if (si->smu != NULL && MOWGLI_LIST_LENGTH(ml) != 0)
	{
		MOWGLI_ITER_FOREACH(o, ml->head)
		{
			groupacs_t *ga = o->data;

			if (ga->mt != entity(si->smu))
				continue;

			if ((ga->flags & GA_BAN))
				continue;

			if (!irccasecmp(get_group_template_vhost_by_flags(ga->mg, ga->flags), host))
			{
				if (strstr(host, "$account"))
				replace(host, BUFSIZE, "$account", entity(si->smu)->name);

				if (!check_vhost_validity(si, host))
					return;

				logcommand(si, CMDLOG_GET, "TAKE: \2%s\2 for \2%s\2 (template \2%s\2 from group: \2%s\2)", host, entity(si->smu)->name, get_group_template_vhost_by_flags(ga->mg, ga->flags), entity(ga->mg)->name);

				command_success_nodata(si, _("You have taken vhost \2%s\2."), host);
				hs_sethost_all(si->smu, host, get_source_name(si));
				do_sethost_all(si->smu, host);

				return;
			}
		}
	}

	command_success_nodata(si, _("Vhost \2%s\2 not found in your vhost offer list."), host);
}

/* OFFERLIST */
static void hs_cmd_offerlist(sourceinfo_t *si, int parc, char *parv[])
{
	hsoffered_t *l;
	mowgli_list_t *ml;
	mowgli_node_t *n, *o;
	char buf[BUFSIZE];
	struct tm tm;
	unsigned int count = 0;

	ml = myentity_get_membership_list(entity(si->smu));

	MOWGLI_ITER_FOREACH(n, hs_offeredlist.head)
	{
		l = n->data;

		if (l->group != NULL && !myuser_is_in_group(si->smu, l->group) && !has_priv(si, PRIV_GROUP_ADMIN))
			continue;

		tm = *localtime(&l->vhost_ts);

		strftime(buf, BUFSIZE, TIME_FORMAT, &tm);

		if(l->group != NULL)
			command_success_nodata(si, "vhost:\2%s\2, group:\2%s\2 creator:\2%s\2 (%s)",
						l->vhost, entity(l->group)->name, l->creator, buf);
		else
			command_success_nodata(si, "vhost:\2%s\2, creator:\2%s\2 (%s)",
						l->vhost, l->creator, buf);
	}

	if (si->smu != NULL && MOWGLI_LIST_LENGTH(ml) != 0)
	{
		//command_success_nodata(si, _("V-Hosts available from group template memberships:"));

		MOWGLI_ITER_FOREACH(o, ml->head)
		{
			groupacs_t *ga = o->data;

			if (ga->mt != entity(si->smu))
				continue;

			if ((ga->flags & GA_BAN))
				continue;

			if (get_group_template_vhost_by_flags(ga->mg, ga->flags) == NULL)
				continue;

			count++;

			//command_success_nodata(si, _("vhost:\2%s\2, group: \2%s\2"), get_group_template_vhost_by_flags(ga->mg, ga->flags), entity(ga->mg)->name);
		}
		
		if (count > 0)
		{
			command_success_nodata(si, _("V-Hosts available from group template memberships:"));
		}

		MOWGLI_ITER_FOREACH(o, ml->head)
		{
			groupacs_t *ga = o->data;

			if (ga->mt != entity(si->smu))
				continue;

			if ((ga->flags & GA_BAN))
				continue;

			if (get_group_template_vhost_by_flags(ga->mg, ga->flags) != NULL)
				command_success_nodata(si, _("vhost:\2%s\2, group: \2%s\2"), get_group_template_vhost_by_flags(ga->mg, ga->flags), entity(ga->mg)->name);
		}
	}

	command_success_nodata(si, "End of list.");
	logcommand(si, CMDLOG_GET, "OFFERLIST");
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
