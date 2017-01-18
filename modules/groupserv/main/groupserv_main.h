/* groupserv_main.h - group services main module header
 * Copyright (C) 2010 Atheme Development Group
 * Copyright (c) 2016 ChatLounge IRC Network Development Team
 *     (http://www.chatlounge.net/)
 */

#ifndef GROUPSERV_MAIN_H
#define GROUPSERV_MAIN_H

#include "atheme.h"
#include "groupserv_common.h"

E groupserv_config_t gs_config;

E void mygroups_init(void);
E void mygroups_deinit(void);
E mygroup_t *mygroup_add(const char *name);
E mygroup_t *mygroup_add_id(const char *id, const char *name);
E mygroup_t *mygroup_find(const char *name);

E groupacs_t *groupacs_add(mygroup_t *mg, myentity_t *mt, unsigned int flags);
E groupacs_t *groupacs_find(mygroup_t *mg, myentity_t *mt, unsigned int flags, bool allow_recurse);
E void groupacs_delete(mygroup_t *mg, myentity_t *mt);

E bool groupacs_sourceinfo_has_flag(mygroup_t *mg, sourceinfo_t *si, unsigned int flag);

E void gs_db_init(void);
E void gs_db_deinit(void);

E void gs_hooks_init(void);
E void gs_hooks_deinit(void);

E void mygroup_set_chanacs_validator(myentity_t *mt);
E unsigned int mygroup_count_flag(mygroup_t *mg, unsigned int flag);
E unsigned int gs_flags_parser(char *flagstring, bool allow_minus, unsigned int flags);
E void remove_group_chanacs(mygroup_t *mg);

E mowgli_list_t *myentity_get_membership_list(myentity_t *mt);
E unsigned int myentity_count_group_flag(myentity_t *mt, unsigned int flagset);

E const char *mygroup_founder_names(mygroup_t *mg);

E void gflags_make_bitmasks(const char *string, unsigned int *addflags, unsigned int *removeflags);
E unsigned int allow_gflags(mygroup_t *mg, unsigned int theirflags);
E char *bitmask_to_gflags(unsigned int flags);
E char *bitmask_to_gflags2(unsigned int addflags, unsigned int removeflags);
E bool groupacs_modify(groupacs_t *ga, unsigned int *addflags, unsigned int *removeflags, unsigned int restrictflags);
E bool groupacs_modify_simple(groupacs_t *ca, unsigned int addflags, unsigned int removeflags);
E void groupacs_close(groupacs_t *ga);

E const char *get_group_item(const char *str, const char *name);
E unsigned int get_group_template_flags(mygroup_t *mg, const char *name);
E unsigned int get_global_group_template_flags(const char *name);
E const char *get_group_template_name(mygroup_t *mg, unsigned int level);

E mowgli_patricia_t *global_group_template_dict;

E void notify_target_acl_change(sourceinfo_t *si, myuser_t *tmu, mygroup_t *mg, const char *flagstr, unsigned int flags);

/* services plumbing - sync with groupserv_common.h */
E service_t *groupsvs;
E mowgli_list_t gs_cmdtree;
E mowgli_list_t conf_gs_table;
E struct gflags ga_flags[];
E struct gflags mg_flags[];



#endif
