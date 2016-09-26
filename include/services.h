/*
 * Copyright (C) 2005 William Pitcock, et al.
 * Copyright (C) 2016 ChatLounge IRC Network Development Team
 *
 * Rights to this code are as documented in doc/LICENSE.
 *
 * Data structures related to services psuedo-clients.
 *
 */

#ifndef SERVICES_H
#define SERVICES_H

typedef struct chansvs_ chansvs_t;
typedef struct nicksvs_ nicksvs_t;

/* The nick/user/host/real strings in these structs simply point
 * to their counterparts in the service_t, and will probably be removed
 * at some point.
 */
/* core services */
struct chansvs_
{
  char *nick;                   /* the IRC client's nickname  */
  char *user;                   /* the IRC client's username  */
  char *host;                   /* the IRC client's hostname  */
  char *real;                   /* the IRC client's realname  */

  bool fantasy;                 /* enable fantasy commands    */

  char *trigger;                /* trigger, e.g. !, ` or .    */

  bool changets;                /* use TS to better deop people */

  service_t *me;                /* our user_t struct          */

  unsigned int expiry;          /* expiry time                */

  unsigned int akick_time;      /* default akick duration */

  unsigned int maxchans;        /* max channels one can register */
  unsigned int maxchanacs;      /* max entries in chanacs list */
  unsigned int maxfounders;     /* max founders per channel    */
  
  unsigned int min_non_wildcard_chars_host_acl; /* If not 0, defines the
                                 * minimum number of non-wildcard characters
                                 * hostmask based channel ACL entries must have.
                                 *
                                 * This is to prevent things such as *@* +Oo
                                 */

  char *founder_flags;          /* default founder flags for new channels */

  char *deftemplates;           /* default templates          */

  bool hide_xop;                /* hide XOP templates      */
  
  bool no_leveled_flags;        /* Do some flags have levels?  If true, old Atheme
                                 * behavior.  If false, these limitations apply:
                                 *  - To add/remove +f requires +s and/or +F.
                                 *  - To add/remove +s, requires +F.
                                 *  - To add/remove +R, requires +F.
                                 *
                                 *  Default configuration setting is false.
                                 */
  bool use_owner;               /* Control whether or not ChanServ and BotServ will
                                 * use the owner cmode when joining a channel.
                                 * Only takes effect if the IRCd supports owner mode.
                                 */
  bool use_admin;               /* Same as use_owner, but for the admin cmode.
                                 */
  unsigned int flags_req_acct; /* Integer representation of flags_requires_account.
                                 */
};

/* authentication services */
struct nicksvs_
{
  bool  spam;
  bool  no_nick_ownership;
  bool  use_dynamic_enforce;

  char   *nick;
  char   *user;
  char   *host;
  char   *real;

  service_t *me;

  unsigned int maxnicks;        /* max nicknames one can group */
  unsigned int expiry;          /* expiry time                */
  unsigned int enforce_expiry;  /* expiry time                */
  unsigned int enforce_delay;   /* delay for nickname enforce */
  unsigned int maxnicklength;   /* Maximum nick length.  Only needed for dynamic
                                 * enforce prefix.
                                 */
  char         *enforce_prefix; /* prefix for enforcement */
  char         *cracklib_dict; /* cracklib dictionary path */
  mowgli_list_t emailexempts; /* emails exempt from maxusers checks */
};

/* help us keep consistent messages */
#define STR_INSUFFICIENT_PARAMS _("Insufficient parameters for \2%s\2.")
#define STR_INVALID_PARAMS _("Invalid parameters for \2%s\2.")
#define STR_NO_PRIVILEGE _("You do not have the %s privilege.")

/* for command_add_flood(),
 * note that floodcheck() already does one FLOOD_MSGS_FACTOR
 */
#define FLOOD_HEAVY (3 * FLOOD_MSGS_FACTOR)
#define FLOOD_MODERATE FLOOD_MSGS_FACTOR
#define FLOOD_LIGHT 0

/* atheme.c */
E chansvs_t chansvs;
E nicksvs_t nicksvs;

/* services.c */
E int authservice_loaded;
E int use_myuser_access;
E int use_svsignore;
E int use_privmsg;
E int use_account_private;
E int use_channel_private;
E int use_limitflags;
E int use_myuser_strictaccess;

E int ban(user_t *source, channel_t *chan, user_t *target);
E int remove_banlike(user_t *source, channel_t *chan, int type, user_t *target);
E int remove_ban_exceptions(user_t *source, channel_t *chan, user_t *target);

E void try_kick_real(user_t *source, channel_t *chan, user_t *target, const char *reason);
E void (*try_kick)(user_t *source, channel_t *chan, user_t *target, const char *reason);

E void kill_user(user_t *source, user_t *victim, const char *fmt, ...) PRINTFLIKE(3, 4);
E void introduce_enforcer(const char *nick);
E void join(const char *chan, const char *nick);
E void joinall(const char *name);
E void part(const char *chan, const char *nick);
E void partall(const char *name);
E void myuser_login(service_t *svs, user_t *u, myuser_t *mu, bool sendaccount);
E void verbose(mychan_t *mychan, const char *fmt, ...) PRINTFLIKE(2, 3);
E void (*notice)(const char *from, const char *target, const char *fmt, ...) PRINTFLIKE(3, 4);
E void change_notify(const char *from, user_t *to, const char *message, ...) PRINTFLIKE(3, 4);
E bool bad_password(sourceinfo_t *si, myuser_t *mu);

E sourceinfo_t *sourceinfo_create(void);
E void command_fail(sourceinfo_t *si, cmd_faultcode_t code, const char *fmt, ...) PRINTFLIKE(3, 4);
E void command_success_nodata(sourceinfo_t *si, const char *fmt, ...) PRINTFLIKE(2, 3);
E void command_success_string(sourceinfo_t *si, const char *result, const char *fmt, ...) PRINTFLIKE(3, 4);
E void command_success_table(sourceinfo_t *si, table_t *table);
E const char *get_source_name(sourceinfo_t *si);
E const char *get_source_mask(sourceinfo_t *si);
E const char *get_oper_name(sourceinfo_t *si);
E const char *get_storage_oper_name(sourceinfo_t *si);
E const char *get_source_security_label(sourceinfo_t *si);

E void wallops(const char *, ...) PRINTFLIKE(1, 2);
E void verbose_wallops(const char *, ...) PRINTFLIKE(1, 2);
E bool check_vhost_validity(sourceinfo_t *si, const char *host);

/* ptasks.c */
E void handle_topic(channel_t *, const char *, time_t, const char *);
E int floodcheck(user_t *, user_t *);
E void command_add_flood(sourceinfo_t *si, unsigned int amount);

/* ctcp-common.c */
E void common_ctcp_init(void);
E unsigned int handle_ctcp_common(sourceinfo_t *si, char *, char *);

/* qrcode.c */
E void command_success_qrcode(sourceinfo_t *si, const char *data);

/* OperServ main.c */
E bool remove_more_specific_akills;

#endif

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
