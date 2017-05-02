/* groupserv.h - group services public interface
 *
 * Include this header for modules other than groupserv/main
 * that need to access group functionality.
 *
 * Copyright (C) 2010 Atheme Development Group
 * Copyright (c) 2016-2017 ChatLounge IRC Network Development Team
 *     (http://www.chatlounge.net/)
 */

#ifndef GROUPSERV_H
#define GROUPSERV_H

#include "main/groupserv_common.h"

mygroup_t * (*mygroup_add)(const char *name);
mygroup_t * (*mygroup_find)(const char *name);
mygroup_t * (*mygroup_rename)(mygroup_t *mg, const char *name);

unsigned int (*mygroup_count_flag)(mygroup_t *mg, unsigned int flag);
unsigned int (*myentity_count_group_flag)(myentity_t *mu, unsigned int flagset);

groupacs_t * (*groupacs_add)(mygroup_t *mg, myentity_t *mt, unsigned int flags);
groupacs_t * (*groupacs_find)(mygroup_t *mg, myentity_t *mt, unsigned int flags, bool allow_recurse);
void (*groupacs_delete)(mygroup_t *mg, myentity_t *mt);

bool (*groupacs_sourceinfo_has_flag)(mygroup_t *mg, sourceinfo_t *si, unsigned int flag);
unsigned int (*groupacs_sourceinfo_flags)(mygroup_t *mg, sourceinfo_t *si);
unsigned int (*gs_flags_parser)(char *flagstring, int allow_minus, unsigned int flags);
mowgli_list_t * (*myentity_get_membership_list)(myentity_t *mu);
const char * (*mygroup_founder_names)(mygroup_t *mg);
void (*remove_group_chanacs)(mygroup_t *mg);

const char * (*get_group_item)(const char *str, const char *name);
const char * (*get_group_template_name)(mygroup_t *mg, unsigned int level);
unsigned int (*get_group_template_flags)(mygroup_t *mg, const char *name);
unsigned int (*get_global_group_template_flags)(const char *name);

const char * (*get_group_template_vhost)(mygroup_t *mg, const char *name);
const char * (*get_group_template_vhost_by_flags)(mygroup_t *mg, unsigned int level);
time_t (*get_group_template_vhost_expiry)(mygroup_t *mg, const char *name);
time_t * (*get_group_template_vhost_expiry_by_flags)(mygroup_t *mg, unsigned int level);

void * (*gflags_make_bitmasks)(const char *string, unsigned int *addflags, unsigned int *removeflags);
unsigned int (*allow_gflags)(mygroup_t *mg, unsigned int theirflags);
char * (*bitmask_to_gflags)(unsigned int flags);
char * (*bitmask_to_gflags2)(unsigned int addflags, unsigned int removeflags);
bool (*groupacs_modify)(groupacs_t *ga, unsigned int *addflags, unsigned int *removeflags, unsigned int restrictflags);
bool (*groupacs_modify_simple)(groupacs_t *ca, unsigned int addflags, unsigned int removeflags);
void (*groupacs_close)(groupacs_t *ga);

void (*show_global_group_template_flags)(sourceinfo_t *si);

void (*notify_target_acl_change)(sourceinfo_t *si, myuser_t *tmu, mygroup_t *mg, const char *flagstr, unsigned int flags);
void (*notify_group_acl_change)(sourceinfo_t *si, myuser_t *tmu, mygroup_t *mg, const char *flagstr, unsigned int flags);
void (*notify_group_set_change)(sourceinfo_t *si, myuser_t *tmu, mygroup_t *mg, const char *settingname, const char *setting);
void (*notify_group_misc_change)(sourceinfo_t *si, mygroup_t *mg, const char *description);

struct gflags *ga_flags;

groupserv_config_t *gs_config;

mowgli_patricia_t *global_group_template_dict;

static inline void use_groupserv_main_symbols(module_t *m)
{
    MODULE_TRY_REQUEST_DEPENDENCY(m, "groupserv/main");
    MODULE_TRY_REQUEST_SYMBOL(m, mygroup_add, "groupserv/main", "mygroup_add");
    MODULE_TRY_REQUEST_SYMBOL(m, mygroup_find, "groupserv/main", "mygroup_find");
    MODULE_TRY_REQUEST_SYMBOL(m, mygroup_rename, "groupserv/main", "mygroup_rename");
    MODULE_TRY_REQUEST_SYMBOL(m, mygroup_count_flag, "groupserv/main", "mygroup_count_flag");
    MODULE_TRY_REQUEST_SYMBOL(m, myentity_count_group_flag, "groupserv/main", "myentity_count_group_flag");
    MODULE_TRY_REQUEST_SYMBOL(m, groupacs_add, "groupserv/main", "groupacs_add");
    MODULE_TRY_REQUEST_SYMBOL(m, groupacs_find, "groupserv/main", "groupacs_find");
    MODULE_TRY_REQUEST_SYMBOL(m, groupacs_delete, "groupserv/main", "groupacs_delete");
    MODULE_TRY_REQUEST_SYMBOL(m, groupacs_sourceinfo_has_flag, "groupserv/main", "groupacs_sourceinfo_has_flag");
    MODULE_TRY_REQUEST_SYMBOL(m, groupacs_sourceinfo_flags, "groupserv/main", "groupacs_sourceinfo_flags");

    MODULE_TRY_REQUEST_SYMBOL(m, get_group_item, "groupserv/main", "get_group_item");
    MODULE_TRY_REQUEST_SYMBOL(m, get_group_template_name , "groupserv/main", "get_group_template_name");
    MODULE_TRY_REQUEST_SYMBOL(m, get_group_template_flags, "groupserv/main", "get_group_template_flags");
    MODULE_TRY_REQUEST_SYMBOL(m, get_global_group_template_flags, "groupserv/main", "get_global_group_template_flags");

    MODULE_TRY_REQUEST_SYMBOL(m, get_group_template_vhost, "groupserv/main", "get_group_template_vhost");
    MODULE_TRY_REQUEST_SYMBOL(m, get_group_template_vhost_by_flags, "groupserv/main", "get_group_template_vhost_by_flags");
    MODULE_TRY_REQUEST_SYMBOL(m, get_group_template_vhost_expiry, "groupserv/main", "get_group_template_vhost_expiry");
	MODULE_TRY_REQUEST_SYMBOL(m, get_group_template_vhost_expiry_by_flags, "groupserv/main", "get_group_template_vhost_expiry_by_flags");

    MODULE_TRY_REQUEST_SYMBOL(m, gs_flags_parser, "groupserv/main", "gs_flags_parser");
    MODULE_TRY_REQUEST_SYMBOL(m, myentity_get_membership_list, "groupserv/main", "myentity_get_membership_list");
    MODULE_TRY_REQUEST_SYMBOL(m, mygroup_founder_names, "groupserv/main", "mygroup_founder_names");
    MODULE_TRY_REQUEST_SYMBOL(m, remove_group_chanacs, "groupserv/main", "remove_group_chanacs");
    MODULE_TRY_REQUEST_SYMBOL(m, gflags_make_bitmasks, "groupserv/main", "gflags_make_bitmasks");
    MODULE_TRY_REQUEST_SYMBOL(m, allow_gflags, "groupserv/main", "allow_gflags");
    MODULE_TRY_REQUEST_SYMBOL(m, bitmask_to_gflags, "groupserv/main", "bitmask_to_gflags");
    MODULE_TRY_REQUEST_SYMBOL(m, bitmask_to_gflags2, "groupserv/main", "bitmask_to_gflags2");
    MODULE_TRY_REQUEST_SYMBOL(m, groupacs_modify, "groupserv/main", "groupacs_modify");
    MODULE_TRY_REQUEST_SYMBOL(m, groupacs_modify_simple, "groupserv/main", "groupacs_modify_simple");
    MODULE_TRY_REQUEST_SYMBOL(m, groupacs_close, "groupserv/main", "groupacs_close");
    //MODULE_TRY_REQUEST_SYMBOL(m, show_global_group_template_flags, "groupserv/main", "show_global_group_template_flags");

    MODULE_TRY_REQUEST_SYMBOL(m, notify_target_acl_change, "groupserv/main", "notify_target_acl_change");
	MODULE_TRY_REQUEST_SYMBOL(m, notify_group_acl_change, "groupserv/main", "notify_group_acl_change");
	MODULE_TRY_REQUEST_SYMBOL(m, notify_group_set_change, "groupserv/main", "notify_group_set_change");
	MODULE_TRY_REQUEST_SYMBOL(m, notify_group_misc_change, "groupserv/main", "notify_group_misc_change");

    MODULE_TRY_REQUEST_SYMBOL(m, ga_flags, "groupserv/main", "ga_flags");
    MODULE_TRY_REQUEST_SYMBOL(m, gs_config, "groupserv/main", "gs_config");

    MODULE_TRY_REQUEST_SYMBOL(m, global_group_template_dict, "groupserv/main", "global_group_template_dict");
}

#ifndef IN_GROUPSERV_SET

mowgli_patricia_t *gs_set_cmdtree;

static inline void use_groupserv_set_symbols(module_t *m)
{
    MODULE_TRY_REQUEST_DEPENDENCY(m, "groupserv/set");

    mowgli_patricia_t **gs_set_cmdtree_tmp;
    MODULE_TRY_REQUEST_SYMBOL(m, gs_set_cmdtree_tmp, "groupserv/set", "gs_set_cmdtree");
    gs_set_cmdtree = *gs_set_cmdtree_tmp;
}

#endif

#endif
