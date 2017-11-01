/*
 * Copyright (c) 2006 Jilles Tjoelker
 * Rights to this code are as documented in doc/LICENSE.
 *
 * Module to disable protect (+a) mode.
 * This will stop Atheme setting this mode by itself, but it can still
 * be used via OperServ MODE etc.
 *
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"protocol/mixin_noprotect", true, _modinit, _moddeinit,
	PACKAGE_STRING,
	VENDOR_STRING
);

bool oldflag;

void _modinit(module_t *m)
{

	if (ircd == NULL)
	{
		slog(LG_ERROR, "Module %s must be loaded after a protocol module.", m->name);
		m->mflags = MODTYPE_FAIL;
		return;
	}
	if (cnt.mychan > 0)
	{
		slog(LG_ERROR, "Module %s must be loaded from the configuration file, not via MODLOAD.", m->name);
		m->mflags = MODTYPE_FAIL;
		return;
	}
	oldflag = ircd->uses_protect;
	ircd->uses_protect = false;
	update_chanacs_flags();
}

void _moddeinit(module_unload_intent_t intent)
{

	ircd->uses_protect = oldflag;
	update_chanacs_flags();
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
