/*
 * Copyright (c) 2014-2017 ChatLounge IRC Network Development Team
 * Copyright (c) 2003-2004 E. Will et al.
 * Copyright (c) 2005-2007 Atheme Development Group
 *
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains protocol support for ChatIRCd, a charybdis fork.
 * Adapted from the Charybdis protocol module.
 */

#include "atheme.h"
#include "uplink.h"
#include "pmodule.h"
#include "protocol/chatircd.h"

DECLARE_MODULE_V1
(
	"protocol/chatircd", true, _modinit, NULL,
	PACKAGE_STRING,
	"ChatLounge IRC Network Development Team <http://www.chatlounge.net>"
);

/* *INDENT-OFF* */

ircd_t chatircd = {
	"ChatIRCd",                     /* IRCd name */
	"$$",                           /* TLD Prefix, used by Global. */
	true,                           /* Whether or not we use IRCNet/TS6 UID */
	false,                          /* Whether or not we use RCOMMAND */
	true,                           /* Whether or not we support channel owners. */
	true,                           /* Whether or not we support channel protection. */
	true,                           /* Whether or not we support halfops. */
	false,                          /* Whether or not we use P10 */
	false,                          /* Whether or not we use vHosts. */
	CMODE_EXLIMIT | CMODE_PERM | CMODE_NETADMINONLY | CMODE_ADMINONLY | CMODE_OPERONLY,	/* Oper-only cmodes */
	CSTATUS_OWNER,                  /* Integer flag for owner channel flag. */
	CSTATUS_PROTECT,                /* Integer flag for protect channel flag. */
	CSTATUS_HALFOP,                 /* Integer flag for halfops. */
	"+y",                           /* Mode we set for owner. */
	"+a",                           /* Mode we set for protect. */
	"+h",                           /* Mode we set for halfops. */
	PROTOCOL_CHARYBDIS,             /* Protocol type */
	CMODE_PERM,                     /* Permanent cmodes */
	0,                              /* Oper-immune cmode */
	"beIq",                         /* Ban-like cmodes */
	'e',                            /* Except mchar */
	'I',                            /* Invex mchar */
	IRCD_CIDR_BANS | IRCD_HOLDNICK, /* Flags */
	true,                           /* Uses quiets */
	"q",                            /* Mode for quiets, if supported. (e.g. "q" on ChatIRCd)  Otherwise, NULL. */
	""                              /* Acting extban, if needed (e.g. "m:" on InspIRCd).  "" otherwise. */
};

struct cmode_ chatircd_mode_list[] = {
  { 'i', CMODE_INVITE	},
  { 'm', CMODE_MOD	},
  { 'n', CMODE_NOEXT	},
  { 'p', CMODE_PRIV	},
  { 's', CMODE_SEC	},
  { 't', CMODE_TOPIC	},
  { 'r', CMODE_REGONLY	},
  { 'z', CMODE_OPMOD	},
  { 'g', CMODE_FINVITE	},
  { 'L', CMODE_EXLIMIT	},
  { 'P', CMODE_PERM	},
  { 'F', CMODE_FTARGET	},
  { 'Q', CMODE_DISFWD	},
  { 'T', CMODE_NONOTICE	},

  /* following modes are added as extensions */
  { 'N', CMODE_NETADMINONLY	},
  { 'S', CMODE_SSLONLY	},
  { 'O', CMODE_OPERONLY	},
  { 'A', CMODE_ADMINONLY},
  { 'c', CMODE_NOCOLOR	},
  { 'C', CMODE_NOCTCP	},

  { '\0', 0 }
};

static bool check_forward(const char *, channel_t *, mychan_t *, user_t *, myuser_t *);
static bool check_jointhrottle(const char *, channel_t *, mychan_t *, user_t *, myuser_t *);

struct extmode chatircd_ignore_mode_list[] = {
  { 'f', check_forward },
  { 'j', check_jointhrottle },
  { '\0', 0 }
};

struct cmode_ chatircd_status_mode_list[] = {
  { 'y', CSTATUS_OWNER },
  { 'a', CSTATUS_PROTECT },
  { 'o', CSTATUS_OP    },
  { 'h', CSTATUS_HALFOP },
  { 'v', CSTATUS_VOICE },
  { '\0', 0 }
};

struct cmode_ chatircd_prefix_mode_list[] = {
  { '~', CSTATUS_OWNER },
  { '&', CSTATUS_PROTECT },
  { '@', CSTATUS_OP    },
  { '%', CSTATUS_HALFOP },
  { '+', CSTATUS_VOICE },
  { '\0', 0 }
};

struct cmode_ chatircd_user_mode_list[] = {
  { 'p', UF_IMMUNE   },
  { 'a', UF_ADMIN    },
  { 'i', UF_INVIS    },
  { 'o', UF_IRCOP    },
  { 'D', UF_DEAF     },
  { 'S', UF_SERVICE  },
  { '\0', 0 }
};

/* *INDENT-ON* */

/* ircd allows forwards to existing channels; the target channel must be
 * +F or the setter must have ops in it */
static bool check_forward(const char *value, channel_t *c, mychan_t *mc, user_t *u, myuser_t *mu)
{
	channel_t *target_c;
	mychan_t *target_mc;
	chanuser_t *target_cu;

	if (!VALID_GLOBAL_CHANNEL_PFX(value) || strlen(value) > 50)
		return false;
	if (u == NULL && mu == NULL)
		return true;
	target_c = channel_find(value);
	target_mc = MYCHAN_FROM(target_c);
	if (target_c == NULL && target_mc == NULL)
		return false;
	if (target_c != NULL && target_c->modes & CMODE_FTARGET)
		return true;
	if (target_mc != NULL && target_mc->mlock_on & CMODE_FTARGET)
		return true;
	if (u != NULL)
	{
		target_cu = chanuser_find(target_c, u);
		if (target_cu != NULL && target_cu->modes & CSTATUS_OP)
			return true;
		if (chanacs_user_flags(target_mc, u) & CA_SET)
			return true;
	}
	else if (mu != NULL)
		if (chanacs_entity_has_flag(target_mc, entity(mu), CA_SET))
			return true;
	return false;
}

static bool check_jointhrottle(const char *value, channel_t *c, mychan_t *mc, user_t *u, myuser_t *mu)
{
	const char *p, *arg2;

	p = value, arg2 = NULL;
	while (*p != '\0')
	{
		if (*p == ':')
		{
			if (arg2 != NULL)
				return false;
			arg2 = p + 1;
		}
		else if (!isdigit((unsigned char)*p))
			return false;
		p++;
	}
	if (arg2 == NULL)
		return false;
	if (p - arg2 > 10 || arg2 - value - 1 > 10 || !atoi(value) || !atoi(arg2))
		return false;
	return true;
}

/* this may be slow, but it is not used much */
/* returns true if it matches, false if not */
/* note that the host part matches differently from a regular ban */
static bool extgecos_match(const char *mask, user_t *u)
{
	char hostgbuf[NICKLEN+USERLEN+HOSTLEN+GECOSLEN];
	char realgbuf[NICKLEN+USERLEN+HOSTLEN+GECOSLEN];

	snprintf(hostgbuf, sizeof hostgbuf, "%s!%s@%s#%s", u->nick, u->user, u->vhost, u->gecos);
	snprintf(realgbuf, sizeof realgbuf, "%s!%s@%s#%s", u->nick, u->user, u->host, u->gecos);
	return !match(mask, hostgbuf) || !match(mask, realgbuf);
}

/* Check if the user is both *NOT* identified to services, and
 * matches the given hostmask.  Syntax: $u:n!u@h
 * e.g. +b $u:*!webchat@* would ban all webchat users who are not
 * identified to services.
 */
static bool unidentified_match(const char *mask, user_t *u)
{
	char hostbuf[NICKLEN+USERLEN+HOSTLEN];
	char realbuf[NICKLEN+USERLEN+HOSTLEN];

	/* Is identified, so just bail. */
	if (u->myuser != NULL)
		return false;

	snprintf(hostbuf, sizeof hostbuf, "%s!%s@%s", u->nick, u->user, u->vhost);
	snprintf(realbuf, sizeof realbuf, "%s!%s@%s", u->nick, u->user, u->host);

	/* If here, not identified to services so just check if the given hostmask matches. */
	if (!match(mask, hostbuf) || !match(mask, realbuf))
		return true;

	return false;
}

static mowgli_node_t *chatircd_next_matching_ban(channel_t *c, user_t *u, int type, mowgli_node_t *first)
{
	chanban_t *cb;
	mowgli_node_t *n;
	char hostbuf[NICKLEN+USERLEN+HOSTLEN];
	char realbuf[NICKLEN+USERLEN+HOSTLEN];
	char ipbuf[NICKLEN+USERLEN+HOSTLEN];
	char strippedmask[NICKLEN+USERLEN+HOSTLEN+CHANNELLEN+2];
	char *p;
	bool negate, matched;
	int exttype;
	channel_t *target_c;

	snprintf(hostbuf, sizeof hostbuf, "%s!%s@%s", u->nick, u->user, u->vhost);
	snprintf(realbuf, sizeof realbuf, "%s!%s@%s", u->nick, u->user, u->host);
	/* will be nick!user@ if ip unknown, doesn't matter */
	snprintf(ipbuf, sizeof ipbuf, "%s!%s@%s", u->nick, u->user, u->ip);

	MOWGLI_ITER_FOREACH(n, first)
	{
		cb = n->data;

		if (cb->type != type)
			continue;

		/*
		 * strip any banforwards from the mask. (SRV-73)
		 * charybdis itself doesn't support banforward but i don't feel like copying
		 * this stuff into ircd-seven and it is possible that charybdis may support them
		 * one day.
		 *   --nenolod
		 */
		mowgli_strlcpy(strippedmask, cb->mask, sizeof strippedmask);
		p = strrchr(strippedmask, '$');
		if (p != NULL && p != strippedmask)
			*p = 0;

		if ((!match(strippedmask, hostbuf) || !match(strippedmask, realbuf) || !match(strippedmask, ipbuf) || !match_cidr(strippedmask, ipbuf)))
			return n;
		if (strippedmask[0] == '$')
		{
			p = strippedmask + 1;
			negate = *p == '~';
			if (negate)
				p++;
			exttype = *p++;
			if (exttype == '\0')
				continue;
			/* check parameter */
			if (*p++ != ':')
				p = NULL;
			switch (exttype)
			{
				case 'a':
					matched = u->myuser != NULL && !(u->myuser->flags & MU_WAITAUTH) && (p == NULL || !match(p, entity(u->myuser)->name));
					break;
				case 'c':
					if (p == NULL)
						continue;
					target_c = channel_find(p);
					if (target_c == NULL || (target_c->modes & (CMODE_PRIV | CMODE_SEC)))
						continue;
					matched = chanuser_find(target_c, u) != NULL;
					break;
				case 'o':
					matched = is_ircop(u);
					break;
				case 'r':
					if (p == NULL)
						continue;
					matched = !match(p, u->gecos);
					break;
				case 'u':
					if (p == NULL)
						continue;
					matched = unidentified_match(p, u);
					break;
				case 'x':
					if (p == NULL)
						continue;
					matched = extgecos_match(p, u);
					break;
				default:
					continue;
			}
			if (negate ^ matched)
				return n;
		}
	}
	return NULL;
}

static bool chatircd_is_valid_host(const char *host)
{
	const char *p;

	for (p = host; *p != '\0'; p++)
		if (!((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'Z') ||
					(*p >= 'a' && *p <= 'z') || *p == '.'
					|| *p == ':' || *p == '-' || *p == '/'))
			return false;
	return true;
}

static void chatircd_notice_channel_sts(user_t *from, channel_t *target, const char *text)
{
	sts(":%s NOTICE %s :%s", from ? CLIENT_NAME(from) : ME, target->name, text);
}

static bool chatircd_is_extban(const char *mask)
{
	const char without_param[] = "oza";
	const char with_param[] = "ajcxru";
	const size_t mask_len = strlen(mask);
	unsigned int offset = 0;

	if ((mask_len < 2 || mask[0] != '$'))
		return NULL;

	if (strchr(mask, ' '))
		return false;

	if (mask_len > 2 && mask[1] == '~')
		offset = 1;

	/* e.g. $a and $~a */
	if ((mask_len == 2 + offset) && strchr(without_param, mask[1 + offset]))
		return true;
	/* e.g. $~a:Shutter and $~a:Shutter */
	else if ((mask_len >= 3 + offset) && mask[2 + offset] == ':' && strchr(with_param, mask[1 + offset]))
		return true;
	return false;
}

void _modinit(module_t * m)
{
	MODULE_TRY_REQUEST_DEPENDENCY(m, "protocol/ts6-generic");

	notice_channel_sts = &chatircd_notice_channel_sts;

	next_matching_ban = &chatircd_next_matching_ban;
	is_valid_host = &chatircd_is_valid_host;
	is_extban = &chatircd_is_extban;

	mode_list = chatircd_mode_list;
	ignore_mode_list = chatircd_ignore_mode_list;
	status_mode_list = chatircd_status_mode_list;
	prefix_mode_list = chatircd_prefix_mode_list;
	user_mode_list = chatircd_user_mode_list;
	ignore_mode_list_size = ARRAY_SIZE(chatircd_ignore_mode_list);

	ircd = &chatircd;

	m->mflags = MODTYPE_CORE;

	pmodule_loaded = true;
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
