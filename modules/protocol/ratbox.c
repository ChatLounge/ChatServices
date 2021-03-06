/*
 * Copyright (c) 2003-2004 E. Will et al.
 * Copyright (c) 2005-2006 Atheme Development Group
 * Copyright (c) 2017-2018 ChatLounge IRC Network Development Team
 *
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains protocol support for ratbox-based ircd.
 *
 */

#include "atheme.h"
#include "uplink.h"
#include "pmodule.h"
#include "protocol/ratbox.h"

DECLARE_MODULE_V1("protocol/ratbox", true, _modinit, NULL, PACKAGE_STRING, VENDOR_STRING);

/* *INDENT-OFF* */

ircd_t Ratbox = {
	"Ratbox (1.0 or later)",        /* IRCd name */
	"$$",                           /* TLD Prefix, used by Global. */
	true,                           /* Whether or not we use IRCNet/TS6 UID */
	false,                          /* Whether or not we use RCOMMAND */
	false,                          /* Whether or not we support channel owners. */
	false,                          /* Whether or not we support channel protection. */
	false,                          /* Whether or not we support halfops. */
	false,                          /* Whether or not we use P10 */
	false,                          /* Whether or not we use vHosts. */
	0,                              /* Oper-only cmodes */
	0,                              /* Integer flag for owner channel flag. */
	0,                              /* Integer flag for protect channel flag. */
	0,                              /* Integer flag for halfops. */
	"+",                            /* Mode we set for owner. */
	"+",                            /* Mode we set for protect. */
	"+",                            /* Mode we set for halfops. */
	PROTOCOL_RATBOX,                /* Protocol type */
	0,                              /* Permanent cmodes */
	0,                              /* Oper-immune cmode */
	"beI",                          /* Ban-like cmodes */
	'e',                            /* Except mchar */
	'I',                            /* Invex mchar */
	IRCD_CIDR_BANS,                 /* Flags */
	false,                          /* Uses quiets */
	NULL,                           /* Mode for quiets, if supported. (e.g. "q" on ChatIRCd)  Otherwise, NULL. */
	"",                             /* Acting extban, if needed (e.g. "m:" on InspIRCd).  "" otherwise. */
	false                           /* True if the IRCd supports changing user modes via S2S. */
};

struct cmode_ ratbox_mode_list[] = {
  { 'i', CMODE_INVITE },
  { 'm', CMODE_MOD    },
  { 'n', CMODE_NOEXT  },
  { 'p', CMODE_PRIV   },
  { 's', CMODE_SEC    },
  { 't', CMODE_TOPIC  },
  { '\0', 0 }
};

struct extmode ratbox_ignore_mode_list[] = {
  { '\0', 0 }
};

struct cmode_ ratbox_status_mode_list[] = {
  { 'o', CSTATUS_OP    },
  { 'v', CSTATUS_VOICE },
  { '\0', 0 }
};

struct cmode_ ratbox_prefix_mode_list[] = {
  { '@', CSTATUS_OP    },
  { '+', CSTATUS_VOICE },
  { '\0', 0 }
};

struct cmode_ ratbox_user_mode_list[] = {
  { 'D', UF_DEAF     },
  { 'S', UF_SERVICE  },
  { 'a', UF_ADMIN    },
  { 'c', UF_LOCCONN  },
  { 'g', UF_ACCEPT   },
  { 'i', UF_INVIS    },
  { 'l', UF_LOCOPS   },
  { 'o', UF_IRCOP    },
  { 's', UF_SNOMASK  },
  { 'w', UF_WALLOPS  },
  { 'z', UF_OPERWALL },
  { '\0', 0 }
};

/* *INDENT-ON* */

void _modinit(module_t * m)
{
	MODULE_TRY_REQUEST_DEPENDENCY(m, "protocol/ts6-generic");

	mode_list = ratbox_mode_list;
	ignore_mode_list = ratbox_ignore_mode_list;
	status_mode_list = ratbox_status_mode_list;
	prefix_mode_list = ratbox_prefix_mode_list;
	user_mode_list = ratbox_user_mode_list;
	ignore_mode_list_size = ARRAY_SIZE(ratbox_ignore_mode_list);

	ircd = &Ratbox;

	m->mflags = MODTYPE_CORE;

	pmodule_loaded = true;
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
