/*
 * Copyright (c) 2003-2004 E. Will et al.
 * Copyright (c) 2005-2008 Atheme Development Group
 * Copyright (c) 2008-2010 ShadowIRCd Development Group
 * Copyright (c) 2017-2018 ChatLounge IRC Network Development Team
 *
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains protocol support for shadowircd.
 *
 */

#include "atheme.h"
#include "uplink.h"
#include "pmodule.h"
#include "protocol/shadowircd.h"

DECLARE_MODULE_V1("protocol/shadowircd", true, _modinit, NULL, PACKAGE_STRING, VENDOR_STRING);

/* *INDENT-OFF* */

ircd_t ShadowIRCd = {
	"ShadowIRCd 6+",                /* IRCd name */
	"$$",                           /* TLD Prefix, used by Global. */
	true,                           /* Whether or not we use IRCNet/TS6 UID */
	false,                          /* Whether or not we use RCOMMAND */
	false,                          /* Whether or not we support channel owners. */
	true,                           /* Whether or not we support channel protection. */
	true,                           /* Whether or not we support halfops. */
	false,                          /* Whether or not we use P10 */
	false,                          /* Whether or not we use vHosts. */
	CMODE_EXLIMIT | CMODE_PERM | CMODE_IMMUNE | CMODE_ADMINONLY | CMODE_OPERONLY, /* Oper-only cmodes */
	0,                  /* Integer flag for owner channel flag. */
	CSTATUS_PROTECT,                /* Integer flag for protect channel flag. */
	CSTATUS_HALFOP,                 /* Integer flag for halfops. */
	"+",                            /* Mode we set for owner. */
	"+a",                           /* Mode we set for protect. */
	"+h",                           /* Mode we set for halfops. */
	PROTOCOL_SHADOWIRCD,            /* Protocol type */
	CMODE_PERM,                     /* Permanent cmodes */
	CMODE_IMMUNE,                   /* Oper-immune cmode */
	"beIq",                         /* Ban-like cmodes */
	'e',                            /* Except mchar */
	'I',                            /* Invex mchar */
	IRCD_CIDR_BANS | IRCD_HOLDNICK, /* Flags */
	true,                           /* Uses quiets */
	"q",                            /* Mode for quiets, if supported. (e.g. "q" on ChatIRCd)  Otherwise, NULL. */
	"",                             /* Acting extban, if needed (e.g. "m:" on InspIRCd).  "" otherwise. */
	false                           /* True if the IRCd supports changing user modes via S2S. */
};

struct cmode_ shadowircd_mode_list[] = {
  { 'i', CMODE_INVITE },
  { 'm', CMODE_MOD    },
  { 'n', CMODE_NOEXT  },
  { 'p', CMODE_PRIV   },
  { 's', CMODE_SEC    },
  { 't', CMODE_TOPIC  },
  { 'c', CMODE_NOCOLOR},
  { 'r', CMODE_REGONLY},
  { 'z', CMODE_OPMOD  },
  { 'g', CMODE_FINVITE},
  { 'L', CMODE_EXLIMIT},
  { 'P', CMODE_PERM   },
  { 'F', CMODE_FTARGET},
  { 'Q', CMODE_DISFWD },
  { 'M', CMODE_IMMUNE },
  { 'C', CMODE_NOCTCP },
  { 'A', CMODE_ADMINONLY },
  { 'O', CMODE_OPERONLY },
  { 'S', CMODE_SSLONLY },
  { 'D', CMODE_NOACTIONS },
  { 'T', CMODE_NONOTICE },
  { 'G', CMODE_NOCAPS },
  { 'E', CMODE_NOKICKS },
  { 'd', CMODE_NONICKS },
  { 'K', CMODE_NOREPEAT },
  { 'J', CMODE_KICKNOREJOIN },
  { '\0', 0 }
};

struct cmode_ shadowircd_status_mode_list[] = {
  { 'a', CSTATUS_PROTECT },
  { 'o', CSTATUS_OP    },
  { 'h', CSTATUS_HALFOP },
  { 'v', CSTATUS_VOICE },
  { '\0', 0 }
};

struct cmode_ shadowircd_prefix_mode_list[] = {
  { '!', CSTATUS_PROTECT },
  { '@', CSTATUS_OP    },
  { '%', CSTATUS_HALFOP },
  { '+', CSTATUS_VOICE },
  { '\0', 0 }
};

struct cmode_ shadowircd_user_mode_list[] = {
  { 'B', UF_BOT        },
  { 'C', UF_NOCTCP     },
  { 'D', UF_DEAF       },
  { 'G', UF_COMMONCHAN },
  { 'Q', UF_NOFWD      },
  { 'R', UF_NOUNREGMSG },
  { 'S', UF_SERVICE    },
  { 'V', UF_NOINVITE   },
  { 'Z', UF_SSL        },

  { 'a', UF_ADMIN      },
  { 'g', UF_ACCEPT     },
  { 'i', UF_INVIS      },
  { 'l', UF_LOCOPS     },
  { 'o', UF_IRCOP      },
  { 'p', UF_OVERRIDE   },
  { 's', UF_SNOMASK    },
  { 'w', UF_WALLOPS    },
  { 'x', UF_IPCLOAK    },

  { '\0', 0 }
};

/* *INDENT-ON* */

void _modinit(module_t * m)
{
	MODULE_TRY_REQUEST_DEPENDENCY(m, "protocol/charybdis");

	mode_list = shadowircd_mode_list;
	user_mode_list = shadowircd_user_mode_list;
	status_mode_list = shadowircd_status_mode_list;
	prefix_mode_list = shadowircd_prefix_mode_list;

	ircd = &ShadowIRCd;

	m->mflags = MODTYPE_CORE;

	pmodule_loaded = true;
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
