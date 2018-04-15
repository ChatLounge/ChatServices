/*
 * ChatServices: A collection of IRC services.
 * phandler.c: Generic protocol handling routines.
 *
 * Copyright (c) 2005-2007 Atheme Project (https://atheme.github.io)
 * Copyright (c) 2018 ChatLounge IRC Network Development Tean (https://www.chatlounge.net)
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "atheme.h"
#include "uplink.h"

unsigned int(*server_login) (void) = generic_server_login;
void (*introduce_nick) (user_t *u) = generic_introduce_nick;
void (*wallops_sts) (const char *text) = generic_wallops_sts;
void (*join_sts) (channel_t *c, user_t *u, bool isnew, char *modes) = generic_join_sts;
void (*chan_lowerts) (channel_t *c, user_t *u) = generic_chan_lowerts;
void (*kick) (user_t *source, channel_t *c, user_t *u, const char *reason) = generic_kick;
void (*msg) (const char *from, const char *target, const char *fmt, ...) = generic_msg;
void (*msg_global_sts) (user_t *from, const char *mask, const char *text) = generic_msg_global_sts;
void (*notice_user_sts) (user_t *from, user_t *target, const char *text) = generic_notice_user_sts;
void (*notice_global_sts) (user_t *from, const char *mask, const char *text) = generic_notice_global_sts;
void (*notice_channel_sts) (user_t *from, channel_t *target, const char *text) = generic_notice_channel_sts;
void (*wallchops) (user_t *source, channel_t *target, const char *message) = generic_wallchops;
void (*numeric_sts) (server_t *from, int numeric, user_t *target, const char *fmt, ...) = generic_numeric_sts;
void (*kill_id_sts) (user_t *killer, const char *id, const char *reason) = generic_kill_id_sts;
void (*part_sts) (channel_t *c, user_t *u) = generic_part_sts;
void (*kline_sts) (const char *server, const char *user, const char *host, long duration, const char *reason) = generic_kline_sts;
void (*unkline_sts) (const char *server, const char *user, const char *host) = generic_unkline_sts;
void (*xline_sts) (const char *server, const char *realname, long duration, const char *reason) = generic_xline_sts;
void (*unxline_sts) (const char *server, const char *realname) = generic_unxline_sts;
void (*qline_sts) (const char *server, const char *mask, long duration, const char *reason) = generic_qline_sts;
void (*unqline_sts) (const char *server, const char *mask) = generic_unqline_sts;
void (*topic_sts) (channel_t *c, user_t *source, const char *setter, time_t ts, time_t prevts, const char *topic) = generic_topic_sts;
void (*mode_sts) (char *sender, channel_t *target, char *modes) = generic_mode_sts;
void (*umode_sts) (char *sender, user_t *target, char *modes) = generic_umode_sts;
void (*ping_sts) (void) = generic_ping_sts;
void (*quit_sts) (user_t *u, const char *reason) = generic_quit_sts;
void (*ircd_on_login) (user_t *u, myuser_t *account, const char *wantedhost) = generic_on_login;
bool (*ircd_on_logout) (user_t *u, const char *account) = generic_on_logout;
void (*jupe) (const char *server, const char *reason) = generic_jupe;
void (*sethost_sts) (user_t *source, user_t *target, const char *host) = generic_sethost_sts;
void (*fnc_sts) (user_t *source, user_t *u, const char *newnick, int type) = generic_fnc_sts;
void (*holdnick_sts)(user_t *source, int duration, const char *nick, myuser_t *account) = generic_holdnick_sts;
void (*invite_sts) (user_t *source, user_t *target, channel_t *channel) = generic_invite_sts;
void (*svslogin_sts) (char *target, char *nick, char *user, char *host, myuser_t *account) = generic_svslogin_sts;
void (*sasl_sts) (char *target, char mode, char *data) = generic_sasl_sts;
void (*sasl_mechlist_sts) (const char *mechlist) = generic_sasl_mechlist_sts;
mowgli_node_t *(*next_matching_ban)(channel_t *c, user_t *u, int type, mowgli_node_t *first) = generic_next_matching_ban;
mowgli_node_t *(*next_matching_host_chanacs)(mychan_t *mc, user_t *u, mowgli_node_t *first) = generic_next_matching_host_chanacs;
bool (*is_valid_nick)(const char *nick) = generic_is_valid_nick;
bool (*is_valid_host)(const char *host) = generic_is_valid_host;
bool (*is_valid_username)(const char *username) = generic_is_valid_username;
void (*mlock_sts)(channel_t *c) = generic_mlock_sts;
void (*topiclock_sts)(channel_t *c) = generic_topiclock_sts;
void (*quarantine_sts)(user_t *source, user_t *victim, long duration, const char *reason) = generic_quarantine_sts;
bool (*is_extban)(const char *mask) = generic_is_extban;
void (*dline_sts)(const char *server, const char *host, long duration, const char *reason) = generic_dline_sts;
void (*undline_sts)(const char *server, const char *host) = generic_undline_sts;

unsigned int generic_server_login(void)
{
	/* Nothing to do here. */
	return 0;
}

void generic_introduce_nick(user_t *u)
{
	/* Nothing to do here. */
}

void generic_wallops_sts(const char *text)
{
	/* ugly, but some ircds offer no alternative -- jilles */
	user_t *u;
	mowgli_patricia_iteration_state_t state;
	char buf[BUFSIZE];

	snprintf(buf, sizeof buf, "*** Notice -- %s", text);
	MOWGLI_PATRICIA_FOREACH(u, &state, userlist)
	{
		if (!is_internal_client(u) && is_ircop(u))
			notice_user_sts(NULL, u, buf);
	}
}

void generic_join_sts(channel_t *c, user_t *u, bool isnew, char *modes)
{
	/* We can't do anything here. Bail. */
}

void generic_chan_lowerts(channel_t *c, user_t *u)
{
	slog(LG_ERROR, "chan_lowerts() called but not supported!");
	join_sts(c, u, true, channel_modes(c, true));
}

void generic_kick(user_t *source, channel_t *c, user_t *u, const char *reason)
{
	/* We can't do anything here. Bail. */
}

void generic_msg(const char *from, const char *target, const char *fmt, ...)
{
	va_list ap;
	char buf[BUFSIZE];

	va_start(ap, fmt);
	if (vsnprintf(buf, sizeof buf, fmt, ap) < 0)
	{
		va_end(ap);
		return;
	}
	va_end(ap);

	slog(LG_INFO, "Cannot send message to %s (%s): don't know how. Load a protocol module perhaps?", target, buf);
}

void generic_msg_global_sts(user_t *from, const char *mask, const char *text)
{
	slog(LG_INFO, "Cannot send global message to %s (%s): don't know how. Load a protocol module perhaps?", mask, text);
}

void generic_notice_user_sts(user_t *from, user_t *target, const char *text)
{
	slog(LG_INFO, "Cannot send notice to %s (%s): don't know how. Load a protocol module perhaps?", target->nick, text);
}

void generic_notice_global_sts(user_t *from, const char *mask, const char *text)
{
	slog(LG_INFO, "Cannot send global notice to %s (%s): don't know how. Load a protocol module perhaps?", mask, text);
}

void generic_notice_channel_sts(user_t *from, channel_t *target, const char *text)
{
	slog(LG_INFO, "Cannot send notice to %s (%s): don't know how. Load a protocol module perhaps?", target->name, text);
}

void generic_wallchops(user_t *sender, channel_t *channel, const char *message)
{
	/* ugly, but always works -- jilles */
	mowgli_node_t *n;
	chanuser_t *cu;

	MOWGLI_ITER_FOREACH(n, channel->members.head)
	{
		cu = (chanuser_t *)n->data;
		if (cu->user->server != me.me && cu->modes & CSTATUS_OP)
			notice(sender->nick, cu->user->nick, "[@%s] %s", channel->name, message);
	}
}

void generic_numeric_sts(server_t *from, int numeric, user_t *target, const char *fmt, ...)
{
	va_list va;
	char buf[BUFSIZE];

	va_start(va, fmt);
	if (vsnprintf(buf, sizeof buf, fmt, va) < 0)
	{
		va_end(va);
		return;
	}
	va_end(va);

	sts(":%s %d %s %s", SERVER_NAME(from), numeric, CLIENT_NAME(target), buf);
}

void generic_kill_id_sts(user_t *killer, const char *id, const char *reason)
{
	/* Can't do anything here, so bail. */
}

void generic_part_sts(channel_t *c, user_t *u)
{
	/* Can't do anything here, so bail. */
}

void generic_kline_sts(const char *server, const char *user, const char *host, long duration, const char *reason)
{
	/* Can't do anything here, so bail. */
}

void generic_unkline_sts(const char *server, const char *user, const char *host)
{
	/* Can't do anything here, so bail. */
}

void generic_xline_sts(const char *server, const char *realname, long duration, const char *reason)
{
	/* Can't do anything here, so bail. */
}

void generic_unxline_sts(const char *server, const char *realname)
{
	/* Can't do anything here, so bail. */
}

void generic_qline_sts(const char *server, const char *mask, long duration, const char *reason)
{
	/* Can't do anything here, so bail. */
}

void generic_unqline_sts(const char *server, const char *mask)
{
	/* Can't do anything here, so bail. */
}

void generic_dline_sts(const char *server, const char *host, long duration, const char *reason)
{
	/* can't do anything here. bail. */
}

void generic_undline_sts(const char *server, const char *host)
{
	/* can't do anything here. bail. */
}

void generic_topic_sts(channel_t *c, user_t *source, const char *setter, time_t ts, time_t prevts, const char *topic)
{
	/* Can't do anything here, so bail. */
}

void generic_mode_sts(char *sender, channel_t *target, char *modes)
{
	/* Can't do anything here, so bail. */
}

void generic_umode_sts(char *sender, user_t *target, char *modes)
{
	/* Can't do anything here, so bail. */
}

void generic_ping_sts(void)
{
	/* Can't do anything here, so bail. */
}

void generic_quit_sts(user_t *u, const char *reason)
{
	/* Can't do anything here, so bail. */
}

void generic_on_login(user_t *u, myuser_t *account, const char *wantedhost)
{
	/* nothing to do here. */
}

bool generic_on_logout(user_t *u, const char *account)
{
	/* nothing to do here. */
	return false;
}

void generic_jupe(const char *server, const char *reason)
{
	/* nothing to do here. */
}

void generic_sethost_sts(user_t *source, user_t *target, const char *host)
{
	/* nothing to do here. */
}

void generic_fnc_sts(user_t *source, user_t *u, const char *newnick, int type)
{
	if (type == FNC_FORCE) /* XXX this does not work properly */
		kill_id_sts(source, CLIENT_NAME(u), "Nickname enforcement");
}

void generic_holdnick_sts(user_t *source, int duration, const char *nick, myuser_t *account)
{
	/* nothing to do here. */
}

void generic_invite_sts(user_t *source, user_t *target, channel_t *channel)
{
	/* nothing to do here. */
}

void generic_svslogin_sts(char *target, char *nick, char *user, char *host, myuser_t *account)
{
	/* nothing to do here. */
}

void generic_sasl_sts(char *target, char mode, char *data)
{
	/* nothing to do here. */
}

void generic_sasl_mechlist_sts(const char *mechlist)
{
	/* nothing to do here. */
}

mowgli_node_t *generic_next_matching_ban(channel_t *c, user_t *u, int type, mowgli_node_t *first)
{
	chanban_t *cb;
	mowgli_node_t *n;
	char hostbuf[NICKLEN+USERLEN+HOSTLEN];
	char cloakbuf[NICKLEN+USERLEN+HOSTLEN];
	char realbuf[NICKLEN+USERLEN+HOSTLEN];
	char ipbuf[NICKLEN+USERLEN+HOSTLEN];

	snprintf(hostbuf, sizeof hostbuf, "%s!%s@%s", u->nick, u->user, u->vhost);
	snprintf(cloakbuf, sizeof cloakbuf, "%s!%s@%s", u->nick, u->user, u->chost);
	snprintf(realbuf, sizeof realbuf, "%s!%s@%s", u->nick, u->user, u->host);
	/* will be nick!user@ if ip unknown, doesn't matter */
	snprintf(ipbuf, sizeof ipbuf, "%s!%s@%s", u->nick, u->user, u->ip);
	MOWGLI_ITER_FOREACH(n, first)
	{
		cb = n->data;

		if (cb->type == type &&
				(!match(cb->mask, hostbuf) || !match(cb->mask, cloakbuf) || !match(cb->mask, realbuf) || !match(cb->mask, ipbuf) || (ircd->flags & IRCD_CIDR_BANS && !match_cidr(cb->mask, ipbuf))))
			return n;
	}
	return NULL;
}

mowgli_node_t *generic_next_matching_host_chanacs(mychan_t *mc, user_t *u, mowgli_node_t *first)
{
	chanacs_t *ca;
	mowgli_node_t *n;
	char hostbuf[NICKLEN+USERLEN+HOSTLEN];
	char hostbuf2[NICKLEN+USERLEN+HOSTLEN];
	char ipbuf[NICKLEN+USERLEN+HOSTLEN];

	snprintf(hostbuf, sizeof hostbuf, "%s!%s@%s", u->nick, u->user, u->vhost);
	snprintf(hostbuf2, sizeof hostbuf2, "%s!%s@%s", u->nick, u->user, u->chost);
	/* will be nick!user@ if ip unknown, doesn't matter */
	snprintf(ipbuf, sizeof ipbuf, "%s!%s@%s", u->nick, u->user, u->ip);

	MOWGLI_ITER_FOREACH(n, first)
	{
		ca = n->data;

		if (ca->entity != NULL)
		       continue;
		if (!match(ca->host, hostbuf) || !match(ca->host, hostbuf2) || !match(ca->host, ipbuf) || (ircd->flags & IRCD_CIDR_BANS && !match_cidr(ca->host, ipbuf)))
			return n;
	}
	return NULL;
}

bool generic_is_valid_nick(const char *nick)
{
	const char *iter = nick;

	/* nicknames may not normally begin with a number, due to UID collision */
	if (IsDigit(*iter))
		return false;
	if (*iter == '-')
		return false;

	for (; *iter != '\0'; iter++)
	{
		if (!IsNickChar(*iter))
			return false;
	}

	return true;
}

bool generic_is_valid_username(const char *username)
{
	const char *iter = username;

	for (; *iter != '\0'; iter++)
	{
		if (!IsUserChar(*iter))
			return false;
	}

	return true;
}

bool generic_is_valid_host(const char *host)
{
	/* don't know what to do here */
	return true;
}

void generic_mlock_sts(channel_t *c)
{
	/* nothing to do here. */
}

void generic_topiclock_sts(channel_t *c)
{
	/* nothing to do here. */
}

void generic_quarantine_sts(user_t *source, user_t *victim, long duration, const char *reason)
{
	/* nothing to do here */
}

bool generic_is_extban(const char *mask)
{
	return false;
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
