/* list.h - /ns list public interface
 *
 * Include this header for modules other than nickserv/list
 * that need to access /ns list functionality.
 *
 * Copyright (C) 2010 Atheme Development Group
 * Copyright (c) 2018 ChatLounge IRC Network Development Team
 */

#ifndef NSLIST_H
#define NSLIST_H

#include "list_common.h"

void (*list_register)(const char *param_name, list_param_t *param);
void (*list_unregister)(const char *param_name);
void (*list_account_register)(const char *param_name, list_param_account_t *param);
void (*list_account_unregister)(const char *param_name);

static inline void use_nslist_main_symbols(module_t *m)
{
	MODULE_TRY_REQUEST_DEPENDENCY(m, "nickserv/list");
	MODULE_TRY_REQUEST_DEPENDENCY(m, "nickserv/listnicks");
	MODULE_TRY_REQUEST_SYMBOL(m, list_account_register, "nickserv/list", "list_account_register");
	MODULE_TRY_REQUEST_SYMBOL(m, list_register, "nickserv/listnicks", "list_register");
	MODULE_TRY_REQUEST_SYMBOL(m, list_account_unregister, "nickserv/list", "list_account_unregister");
	MODULE_TRY_REQUEST_SYMBOL(m, list_unregister, "nickserv/listnicks", "list_unregister");
}

#endif /* !NSLIST_H */
