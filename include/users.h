/*
 * Copyright (C) 2005 William Pitcock, et al.
 * Copyright (C) 2017-2018 ChatLounge IRC Network Development Team
 *
 * Rights to this code are as documented in doc/LICENSE.
 *
 * Data structures for connected clients.
 *
 */

#ifndef USERS_H
#define USERS_H

struct user_
{
	object_t parent;

	stringref nick;
	stringref user;
	stringref host; /* Real host */
	stringref gecos;
	stringref chost; /* Cloaked host */
	stringref vhost; /* Visible host */
	stringref uid; /* Used for TS6, P10, IRCNet ircd. */
	stringref ip;

	mowgli_list_t channels;

	server_t *server;
	myuser_t *myuser;

	unsigned int offenses;
	unsigned int msgs; /* times FLOOD_MSGS_FACTOR */
	time_t lastmsg;

	unsigned long long int flags;

	time_t ts;

	mowgli_node_t snode; /* for server_t.userlist */

	char *certfp; /* client certificate fingerprint */
};

#define FLOOD_MSGS_FACTOR 256

#define UF_AWAY        0x0000000000000002ull
#define UF_INVIS       0x0000000000000004ull /* User has umode +i */
#define UF_IRCOP       0x0000000000000010ull /* User has umode +o */
#define UF_ADMIN       0x0000000000000020ull /* User has the admin umode. */
#define UF_SEENINFO    0x0000000000000080ull
#define UF_IMMUNE      0x0000000000000100ull /* user is immune from kickban, don't bother enforcing akicks */
#define UF_HIDEHOSTREQ 0x0000000000000200ull /* host hiding requested */
#define UF_SOPER_PASS  0x0000000000000400ull /* services oper pass entered */
#define UF_DOENFORCE   0x0000000000000800ull /* introduce enforcer when nick changes */
#define UF_ENFORCER    0x0000000000001000ull /* this is an enforcer client */
#define UF_WASENFORCED 0x0000000000002000ull /* this user was FNCed once already */
#define UF_DEAF        0x0000000000004000ull /* user does not receive channel msgs */
#define UF_SERVICE     0x0000000000008000ull /* user is a service (e.g. +S on ChatIRCd/Charybdis) */
#define UF_USEDCERT    0x0000000000010000ull /* user used a certificate fingerprint to login to services. */
#define UF_KLINESENT   0x0000000000020000ull /* have sent a kline for this user. */

/* More user mode types */

#define UF_FLOOD       0x0000000004000000ull /* If set, user is subject to fewer or no flood limits. */
#define UF_NOCTCP      0x0000000008000000ull /* User may not receive CTCP. */
#define UF_WEBTV       0x0000000010000000ull /* User is connected through WebTV. */
#define UF_VHOST       0x0000000020000000ull /* User is using a vhost. */
#define UF_DEAFPRIV    0x0000000040000000ull /* Like the deaf umode for channels but for PMs and private notices. */
#define UF_RESTRICTED  0x0000000080000000ull /* User is on a restricted connection. (e.g. +r on IRCd)*/
#define UF_STRIPMSG    0x0000000100000000ull /* Strip color and formatting codes in private messages sent to the user. */
#define UF_CENSOR      0x0000000200000000ull /* Censor messages sent to the user based on configured filters. */
#define UF_WHOISNOTIFY 0x0000000400000000ull /* If set, notify the user when someone performs a WHOIS on him. */
#define UF_HIDEOPER    0x0000000800000000ull /* Hide oper status in WHOIS. (e.g. +H on Hybrid) */
#define UF_HIDEIDLE    0x0000001000000000ull /* Hide idle time in WHOIS. (e.g. +q on Hybrid)*/
#define UF_NOINVITE    0x0000002000000000ull /* Block invites (e.g. +V on ElementalIRCd) */
#define UF_HIDECHANL   0x0000004000000000ull /* Hide channel list in WHOIS. (e.g. +p on Hybrid) */
#define UF_WEBCHAT     0x0000008000000000ull /* User is connected with web chat. */
#define UF_HIDESERVI   0x0000010000000000ull /* Hide server info in /WHOIS output. */
#define UF_HELPOP      0x0000020000000000ull /* User is marked as a helper. */
#define UF_LOCIRCOP    0x0000040000000000ull /* User is a local IRCop */
#define UF_COMMONCHAN  0x0000080000000000ull /* User may only receive private PRIVMSG from others with at least one channel in common. */
#define UF_LOCCONN     0x0000100000000000ull /* User may see local connect notices. */
#define UF_FARCONN     0x0000200000000000ull /* User may see far connect notices. */
#define UF_SHUN        0x0000400000000000ull /* User is shunned or similar. */
#define UF_SHUNVERB    0x0000800000000000ull /* User is shunned or similar (verbose). */
#define UF_IPCLOAK     0x0001000000000000ull /* IP-cloaking enabled. */
#define UF_SERVADMIN   0x0002000000000000ull /* User is designated as a services administrator. */
#define UF_SNOMASK     0x0004000000000000ull /* User has a snomask set. */
#define UF_SSL         0x0008000000000000ull /* User is connected with SSL. */
#define UF_OVERRIDE    0x0010000000000000ull /* Staff override mode (channel op or owner in all channels regardless of prefix/status) */
#define UF_REG         0x0020000000000000ull /* Is using a registered nick. */
#define UF_NOUNREGMSG  0x0040000000000000ull /* No PRIVMSGs targeted to this user from unregistered nicks. */
#define UF_NOUNIDMSG   0x0080000000000000ull /* No PRIVMSGs targeted to this user from sources not ID'd to services (account based). */
#define UF_NOFWD       0x0100000000000000ull /* User can't be forwarded (e.g. umode +Q on ChatIRCd/Charybdis). */
#define UF_BOT         0x0200000000000000ull /* User is a bot (e.g. typically +B). */
#define UF_NOSSLMSG    0x0400000000000000ull /* No PRIVMSGs targeted to this user from sources without SSL/TLS. */
#define UF_OPERWALL    0x0800000000000000ull /* Receive operwall messages. */
#define UF_WALLOPS     0x1000000000000000ull /* Receive wallops messages. */
#define UF_LOCOPS      0x2000000000000000ull /* Local wallops. */
#define UF_ACCEPT      0x4000000000000000ull /* Accept user mode (e.g. +g on ChatIRCd/Charybdis) */
#define UF_NETADMIN    0x8000000000000000ull /* User has the netadmin umode. */

#define CLIENT_NAME(user)	((user)->uid != NULL ? (user)->uid : (user)->nick)

typedef struct {
	user_t *u;		/* User in question. Write NULL here if you delete the user. */
	const char *oldnick;	/* Previous nick for nick changes. u->nick is the new nick. */
} hook_user_nick_t;

typedef struct {
	user_t *const u;
	const char *comment;
} hook_user_delete_t;

/* function.c */
E bool is_ircop(user_t *user);
E bool is_admin(user_t *user);
E bool is_internal_client(user_t *user);
E bool is_autokline_exempt(user_t *user);
E bool is_service(user_t *user);

/* users.c */
E mowgli_patricia_t *userlist;
E mowgli_patricia_t *uidlist;

E void init_users(void);

E user_t *user_add(const char *nick, const char *user, const char *host, const char *vhost, const char *ip, const char *uid, const char *gecos, server_t *server, time_t ts);
E void user_delete(user_t *u, const char *comment);
E user_t *user_find(const char *nick);
E user_t *user_find_named(const char *nick);
E void user_changeuid(user_t *u, const char *uid);
E bool user_changenick(user_t *u, const char *nick, time_t ts);
E void user_mode(user_t *user, const char *modes);
E void user_sethost(user_t *source, user_t *target, const char *host);
E const char *user_get_umodestr(user_t *u);
E bool user_is_channel_banned(user_t *u, char ban_type);
E void user_show_all_logins(myuser_t *mu, user_t *source, user_t *target);

/* uid.c */
E void init_uid(void);
E const char *uid_get(void);

#endif

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
