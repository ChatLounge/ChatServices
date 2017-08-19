/*
 * Copyright (c) 2005 Atheme Development Group
 * Copyright (c) 2017 ChatLounge IRC Network Development Team
 *
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains the main() routine.
 *
 */

#include "atheme.h"
#include "uplink.h"

DECLARE_MODULE_V1
(
	"saslserv/main", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net>"
);

void (*add_login_history_entry)(myuser_t *smu, myuser_t *tmu, const char *desc) = NULL;

mowgli_list_t sessions;
static mowgli_list_t sasl_mechanisms;
static char mechlist_string[400];
static bool hide_server_names;

sasl_session_t *find_session(const char *uid);
sasl_session_t *make_session(const char *uid, server_t *server);
void destroy_session(sasl_session_t *p);
static void sasl_logcommand(sasl_session_t *p, myuser_t *login, int level, const char *fmt, ...);
static void sasl_input(sasl_message_t *smsg);
static void sasl_packet(sasl_session_t *p, char *buf, int len);
static void sasl_write(char *target, char *data, int length);
static bool may_impersonate(myuser_t *source_mu, myuser_t *target_mu);
static myuser_t *login_user(sasl_session_t *p);
static void sasl_newuser(hook_user_nick_t *data);
static void sasl_server_eob(server_t *s);
static void delete_stale(void *vptr);
static void sasl_mech_register(sasl_mechanism_t *mech);
static void sasl_mech_unregister(sasl_mechanism_t *mech);
static void mechlist_build_string(char *ptr, size_t buflen);
static void mechlist_do_rebuild();
static void on_shutdown(void *unused);

sasl_mech_register_func_t sasl_mech_register_funcs = { &sasl_mech_register, &sasl_mech_unregister };

/* main services client routine */
static void saslserv(sourceinfo_t *si, int parc, char *parv[])
{
	char *cmd;
	char *text;
	char orig[BUFSIZE];

	/* this should never happen */
	if (parv[0][0] == '&')
	{
		slog(LG_ERROR, "services(): got parv with local channel: %s", parv[0]);
		return;
	}

	/* make a copy of the original for debugging */
	mowgli_strlcpy(orig, parv[parc - 1], BUFSIZE);

	/* lets go through this to get the command */
	cmd = strtok(parv[parc - 1], " ");
	text = strtok(NULL, "");

	if (!cmd)
		return;
	if (*orig == '\001')
	{
		handle_ctcp_common(si, cmd, text);
		return;
	}

	command_fail(si, fault_noprivs, "This service exists to identify "
			"connecting clients to the network. It has no "
			"public interface.");
}

service_t *saslsvs = NULL;
mowgli_eventloop_timer_t *delete_stale_timer = NULL;

static void sasl_mech_register(sasl_mechanism_t *mech)
{
	mowgli_node_t *node;

	slog(LG_DEBUG, "sasl_mech_register(): registering %s", mech->name);

	node = mowgli_node_create();
	mowgli_node_add(mech, node, &sasl_mechanisms);

	mechlist_do_rebuild();
}

static void sasl_mech_unregister(sasl_mechanism_t *mech)
{
	mowgli_node_t *n, *tn;
	sasl_session_t *session;

	slog(LG_DEBUG, "sasl_mech_unregister(): unregistering %s", mech->name);

	MOWGLI_ITER_FOREACH_SAFE(n, tn, sessions.head)
	{
		session = n->data;
		if (session->mechptr == mech)
		{
			slog(LG_DEBUG, "sasl_mech_unregister(): destroying session %s", session->uid);
			destroy_session(session);
		}
	}

	MOWGLI_ITER_FOREACH_SAFE(n, tn, sasl_mechanisms.head)
	{
		if (n->data == mech)
		{
			mowgli_node_delete(n, &sasl_mechanisms);
			mowgli_node_free(n);

			mechlist_do_rebuild();
			break;
		}
	}
}

void _modinit(module_t *m)
{
	hook_add_event("sasl_input");
	hook_add_sasl_input(sasl_input);
	hook_add_event("user_add");
	hook_add_user_add(sasl_newuser);
	hook_add_event("server_eob");
	hook_add_server_eob(sasl_server_eob);
	hook_add_event("sasl_may_impersonate");

	delete_stale_timer = mowgli_timer_add(base_eventloop, "sasl_delete_stale", delete_stale, NULL, 30);

	saslsvs = service_add("saslserv", saslserv);
	add_bool_conf_item("HIDE_SERVER_NAMES", &saslsvs->conf_table, 0, &hide_server_names, false);
	authservice_loaded++;

	hook_add_shutdown(on_shutdown);
}

void _moddeinit(module_unload_intent_t intent)
{
	mowgli_node_t *n, *tn;

	hook_del_sasl_input(sasl_input);
	hook_del_user_add(sasl_newuser);
	hook_del_server_eob(sasl_server_eob);

	mowgli_timer_destroy(base_eventloop, delete_stale_timer);

	del_conf_item("HIDE_SERVER_NAMES", &saslsvs->conf_table);

	if (saslsvs != NULL)
		service_delete(saslsvs);

	authservice_loaded--;

	hook_del_shutdown(on_shutdown);

	if (sessions.head != NULL)
		slog(LG_DEBUG, "saslserv/main: shutting down with a non-empty session list, a mech did not unregister itself!");

	MOWGLI_ITER_FOREACH_SAFE(n, tn, sessions.head)
	{
		destroy_session(n->data);
	}
}

/*
 * Begin SASL-specific code
 */

/* find an existing session by uid */
sasl_session_t *find_session(const char *uid)
{
	sasl_session_t *p;
	mowgli_node_t *n;

	if (uid == NULL)
		return NULL;

	MOWGLI_ITER_FOREACH(n, sessions.head)
	{
		p = n->data;
		if(p->uid != NULL && !strcmp(p->uid, uid))
			return p;
	}

	return NULL;
}

/* create a new session if it does not already exist */
sasl_session_t *make_session(const char *uid, server_t *server)
{
	sasl_session_t *p = find_session(uid);
	mowgli_node_t *n;

	if(p)
		return p;

	p = malloc(sizeof(sasl_session_t));
	memset(p, 0, sizeof(sasl_session_t));
	p->uid = strdup(uid);

	p->server = server;
	n = mowgli_node_create();
	mowgli_node_add(p, n, &sessions);

	return p;
}

/* free a session and all its contents */
void destroy_session(sasl_session_t *p)
{
	mowgli_node_t *n, *tn;
	myuser_t *mu;

	if (p->flags & ASASL_NEED_LOG && p->username != NULL)
	{
		mu = myuser_find_by_nick(p->username);
		if (mu != NULL && !(ircd->flags & IRCD_SASL_USE_PUID))
			sasl_logcommand(p, mu, CMDLOG_LOGIN, "LOGIN (session timed out)");
	}

	MOWGLI_ITER_FOREACH_SAFE(n, tn, sessions.head)
	{
		if(n->data == p)
		{
			mowgli_node_delete(n, &sessions);
			mowgli_node_free(n);
		}
	}

	free(p->uid);
	free(p->buf);
	p->buf = p->p = NULL;
	if(p->mechptr)
		p->mechptr->mech_finish(p); /* Free up any mechanism data */
	p->mechptr = NULL; /* We're not freeing the mechanism, just "dereferencing" it */
	free(p->username);
	free(p->certfp);
	free(p->authzid);

	free(p);
}

/* interpret an AUTHENTICATE message */
static void sasl_input(sasl_message_t *smsg)
{
	sasl_session_t *p = make_session(smsg->uid, smsg->server);
	int len = strlen(smsg->buf);
	char *tmpbuf;
	int tmplen;

	/* Abort packets, or maybe some other kind of (D)one */
	if(smsg->mode == 'D')
	{
		destroy_session(p);
		return;
	}

	if(smsg->mode != 'S' && smsg->mode != 'C')
		return;

	if(smsg->mode == 'S' && smsg->ext != NULL &&
			!strcmp(smsg->buf, "EXTERNAL"))
	{
		free(p->certfp);
		p->certfp = sstrdup(smsg->ext);
	}

	if(p->buf == NULL)
	{
		p->buf = (char *)malloc(len + 1);
		p->p = p->buf;
		p->len = len;
	}
	else
	{
		if(p->len + len + 1 > 8192) /* This is a little much... */
		{
			sasl_sts(p->uid, 'D', "F");
			destroy_session(p);
			return;
		}

		p->buf = (char *)realloc(p->buf, p->len + len + 1);
		p->p = p->buf + p->len;
		p->len += len;
	}

	memcpy(p->p, smsg->buf, len);

	/* Messages not exactly 400 bytes are the end of a packet. */
	if(len < 400)
	{
		p->buf[p->len] = '\0';
		tmpbuf = p->buf;
		tmplen = p->len;
		p->buf = p->p = NULL;
		p->len = 0;
		sasl_packet(p, tmpbuf, tmplen);
		free(tmpbuf);
	}
}

/* find a mechanism by name */
static sasl_mechanism_t *find_mechanism(char *name)
{
	mowgli_node_t *n;
	sasl_mechanism_t *mptr;

	MOWGLI_ITER_FOREACH(n, sasl_mechanisms.head)
	{
		mptr = n->data;
		if(!strcmp(mptr->name, name))
			return mptr;
	}

	slog(LG_DEBUG, "find_mechanism(): cannot find mechanism `%s'!", name);

	return NULL;
}

static void sasl_server_eob(server_t *s)
{
	/* new server online, push mechlist to make sure it's using the current one */
	sasl_mechlist_sts(mechlist_string);
}

static void mechlist_do_rebuild()
{
	mechlist_build_string(mechlist_string, sizeof(mechlist_string));

	/* push mechanism list to the network */
	if (me.connected)
		sasl_mechlist_sts(mechlist_string);
}

static void mechlist_build_string(char *ptr, size_t buflen)
{
	int l = 0;
	mowgli_node_t *n;

	MOWGLI_ITER_FOREACH(n, sasl_mechanisms.head)
	{
		sasl_mechanism_t *mptr = n->data;
		if(l + strlen(mptr->name) > buflen)
			break;
		strcpy(ptr, mptr->name);
		ptr += strlen(mptr->name);
		*ptr++ = ',';
		l += strlen(mptr->name) + 1;
	}

	if(l)
		ptr--;
	*ptr = '\0';
}

/* given an entire sasl message, advance session by passing data to mechanism
 * and feeding returned data back to client.
 */
static void sasl_packet(sasl_session_t *p, char *buf, int len)
{
	int rc;
	size_t tlen = 0;
	char *cloak, *out = NULL;
	char temp[BUFSIZE];
	char mech[61];
	size_t out_len = 0;
	metadata_t *md;

	/* First piece of data in a session is the name of
	 * the SASL mechanism that will be used.
	 */
	if(!p->mechptr)
	{
		if(len > 60)
		{
			sasl_sts(p->uid, 'D', "F");
			destroy_session(p);
			return;
		}

		memcpy(mech, buf, len);
		mech[len] = '\0';

		if(!(p->mechptr = find_mechanism(mech)))
		{
			sasl_sts(p->uid, 'M', mechlist_string);

			sasl_sts(p->uid, 'D', "F");
			destroy_session(p);
			return;
		}

		rc = p->mechptr->mech_start(p, &out, &out_len);
	}else{
		if(len == 1 && *buf == '+')
			rc = p->mechptr->mech_step(p, (char []) { '\0' }, 0,
					&out, &out_len);
		else if ((tlen = base64_decode(buf, temp, BUFSIZE)) &&
				tlen != (size_t)-1)
			rc = p->mechptr->mech_step(p, temp, tlen, &out, &out_len);
		else
			rc = ASASL_FAIL;
	}

	/* Some progress has been made, reset timeout. */
	p->flags &= ~ASASL_MARKED_FOR_DELETION;

	if(rc == ASASL_DONE)
	{
		myuser_t *mu = login_user(p);
		if(mu)
		{
			if ((md = metadata_find(mu, "private:usercloak")))
				cloak = md->value;
			else
				cloak = "*";

			if (!(mu->flags & MU_WAITAUTH))
				svslogin_sts(p->uid, "*", "*", cloak, mu);
			sasl_sts(p->uid, 'D', "S");
			/* Will destroy session on introduction of user to net. */
		}
		else
		{
			sasl_sts(p->uid, 'D', "F");
			destroy_session(p);
		}
		return;
	}
	else if(rc == ASASL_MORE)
	{
		if(out_len)
		{
			if(base64_encode(out, out_len, temp, BUFSIZE))
			{
				sasl_write(p->uid, temp, strlen(temp));
				free(out);
				return;
			}
		}
		else
		{
			sasl_sts(p->uid, 'C', "+");
			free(out);
			return;
		}
	}

	/* If we reach this, they failed SASL auth, so if they were trying
	 * to identify as a specific user, bad_password them.
	 */
	if (p->username)
	{
		myuser_t *mu = myuser_find_by_nick(p->username);
		if (mu)
		{
			char description[BUFSIZE];

			if (p->server && !hide_server_names)
				snprintf(description, BUFSIZE, "Unknown user on %s (via SASL)", p->server->name);
			else
				snprintf(description, BUFSIZE, "Unknown user (via SASL)");

			struct sourceinfo_vtable sasl_vtable = {
				.description = description
			};

			sourceinfo_t *si = sourceinfo_create();
			si->service = saslsvs;
			si->sourcedesc = p->uid;
			si->connection = curr_uplink->conn;
			si->v = &sasl_vtable;
			si->force_language = language_find("en");

			bad_password(si, mu);

			object_unref(si);
		}
	}

	free(out);
	sasl_sts(p->uid, 'D', "F");
	destroy_session(p);
}

/* output an arbitrary amount of data to the SASL client */
static void sasl_write(char *target, char *data, int length)
{
	char out[401];
	int last = 400, rem = length;

	while(rem)
	{
		int nbytes = rem > 400 ? 400 : rem;
		memcpy(out, data, nbytes);
		out[nbytes] = '\0';
		sasl_sts(target, 'C', out);

		data += nbytes;
		rem -= nbytes;
		last = nbytes;
	}

	/* The end of a packet is indicated by a string not of length 400.
	 * If last piece is exactly 400 in size, send an empty string to
	 * finish the transaction.
	 * Also if there is no data at all.
	 */
	if(last == 400)
		sasl_sts(target, 'C', "+");
}

static void sasl_logcommand(sasl_session_t *p, myuser_t *mu, int level, const char *fmt, ...)
{
	va_list args;
	char lbuf[BUFSIZE];

	va_start(args, fmt);
	vsnprintf(lbuf, BUFSIZE, fmt, args);
	slog(level, "%s %s:%s %s", service_get_log_target(saslsvs), mu ? entity(mu)->name : "", p->uid, lbuf);
	va_end(args);
}

static bool may_impersonate(myuser_t *source_mu, myuser_t *target_mu)
{
	hook_sasl_may_impersonate_t req;
	char priv[512] = PRIV_IMPERSONATE_ANY;
	char *classname;

	/* Allow same (although this function won't get called in that case anyway) */
	if(source_mu == target_mu)
		return true;

	/* Check for wildcard priv */
	if(has_priv_myuser(source_mu, priv))
		return true;

	/* Check for target-operclass specific priv */
	classname = (target_mu->soper && target_mu->soper->classname)
			? target_mu->soper->classname : "user";

	snprintf(priv, sizeof(priv), PRIV_IMPERSONATE_CLASS_FMT, classname);

	if(has_priv_myuser(source_mu, priv))
		return true;

	/* Check for target-entity specific priv */
	snprintf(priv, sizeof(priv), PRIV_IMPERSONATE_ENTITY_FMT, entity(target_mu)->name);

	if(has_priv_myuser(source_mu, priv))
		return true;

	/* Allow modules to check too */
	req.source_mu = source_mu;
	req.target_mu = target_mu;
	req.allowed = false;

	hook_call_sasl_may_impersonate(&req);

	return req.allowed;
}

/* authenticated, now double check that their account is ok for login */
static myuser_t *login_user(sasl_session_t *p)
{
	myuser_t *source_mu, *target_mu;

	/* source_mu is the user whose credentials we verified ("authentication id") */
	/* target_mu is the user who will be ultimately logged in ("authorization id") */

	source_mu = myuser_find_by_nick(p->username);
	if(source_mu == NULL)
		return NULL;

	if(p->authzid && *p->authzid)
	{
		target_mu = myuser_find_by_nick(p->authzid);
		if(target_mu == NULL)
			return NULL;
	}
	else
	{
		target_mu = source_mu;
		if(p->authzid != NULL)
			free(p->authzid);
		p->authzid = sstrdup(p->username);
	}

	if ((target_mu->flags & MU_STRICTACCESS))
	{
		mowgli_node_t *n, *tn;
		unsigned int notonaccesslist = 0;

		MOWGLI_ITER_FOREACH_SAFE(n, tn, source_mu->logins.head)
		{
			user_t *u = (user_t *)n->data;
			
			if (!myuser_access_verify(u, target_mu))
			{
				notonaccesslist = 1;
				notice(saslsvs->nick, u->nick, _("You may not log in from this connection as STRICTACCESS has been enabled on this account."));
			}
		}

		if (notonaccesslist)
			return NULL;
	}

	if(metadata_find(source_mu, "private:freeze:freezer"))
	{
		sasl_logcommand(p, source_mu, CMDLOG_LOGIN, "failed LOGIN to \2%s\2 (frozen)", entity(source_mu)->name);
		return NULL;
	}

	if(target_mu != source_mu)
	{
		if(!may_impersonate(source_mu, target_mu))
		{
			sasl_logcommand(p, source_mu, CMDLOG_LOGIN, "denied IMPERSONATE by \2%s\2 to \2%s\2", entity(source_mu)->name, entity(target_mu)->name);
			return NULL;
		}

		sasl_logcommand(p, source_mu, CMDLOG_LOGIN, "allowed IMPERSONATE by \2%s\2 to \2%s\2", entity(source_mu)->name, entity(target_mu)->name);

		if(metadata_find(target_mu, "private:freeze:freezer"))
		{
			sasl_logcommand(p, target_mu, CMDLOG_LOGIN, "failed LOGIN to \2%s\2 (frozen)", entity(target_mu)->name);
			return NULL;
		}
	}

	if(MOWGLI_LIST_LENGTH(&target_mu->logins) >= me.maxlogins)
	{
		sasl_logcommand(p, target_mu, CMDLOG_LOGIN, "failed LOGIN to \2%s\2 (too many logins)", entity(target_mu)->name);
		return NULL;
	}

	/* Log it with the full n!u@h later */
	p->flags |= ASASL_NEED_LOG;

	/* We just did SASL authentication for a user.  With IRCds which do not
	 * have unique UIDs for users, we will likely be expecting the login
	 * data to be bursted.  As a result, we should give the core a heads'
	 * up that this is going to happen so that hooks will be properly
	 * fired...
	 */
	if(ircd->flags & IRCD_SASL_USE_PUID)
	{
		target_mu->flags &= ~MU_NOBURSTLOGIN;
		target_mu->flags |= MU_PENDINGLOGIN;
	}

	return target_mu;
}

/* clean up after a user who is finally on the net */
static void sasl_newuser(hook_user_nick_t *data)
{
	user_t *u = data->u;
	sasl_session_t *p;
	myuser_t *mu;
	char buf[BUFSIZE];
	char description[300];

	/* If the user has been killed, don't do anything. */
	if (!u)
		return;

	p = find_session(u->uid);

	/* Not concerned unless it's a SASL login. */
	if(p == NULL)
		return;

	/* We will log it ourselves, if needed */
	p->flags &= ~ASASL_NEED_LOG;

	/* Find the account */
	mu = p->authzid ? myuser_find_by_nick(p->authzid) : NULL;
	if (mu == NULL)
	{
		notice(saslsvs->nick, u->nick, "Account %s dropped, login cancelled",
		       p->authzid ? p->authzid : "??");
		destroy_session(p);
		/* We'll remove their ircd login in handle_burstlogin() */
		return;
	}

	destroy_session(p);

	myuser_login(saslsvs, u, mu, false);

	logcommand_user(saslsvs, u, CMDLOG_LOGIN, "LOGIN");

	notice(saslsvs->nick, u->nick, _("You are now logged in to: %s"), entity(mu)->name);

	user_show_all_logins(mu, saslsvs->me, u);

	if ((add_login_history_entry = module_locate_symbol("nickserv/loginhistory", "add_login_history_entry")) != NULL)
	{
		snprintf(description, sizeof description, "Successful login: SASL");
		add_login_history_entry(mu, mu, description);
	}
}

/* This function is run approximately once every 30 seconds.
 * It looks for flagged sessions, and deletes them, while
 * flagging all the others. This way stale sessions are deleted
 * after no more than 60 seconds.
 */
static void delete_stale(void *vptr)
{
	sasl_session_t *p;
	mowgli_node_t *n, *tn;

	MOWGLI_ITER_FOREACH_SAFE(n, tn, sessions.head)
	{
		p = n->data;
		if(p->flags & ASASL_MARKED_FOR_DELETION)
		{
			mowgli_node_delete(n, &sessions);
			destroy_session(p);
			mowgli_node_free(n);
		} else
			p->flags |= ASASL_MARKED_FOR_DELETION;
	}
}

static void on_shutdown(void *unused)
{
	if (config_options.send_sasl_quit && saslsvs->me != NULL)
		quit_sts(saslsvs->me, "shutting down");
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
