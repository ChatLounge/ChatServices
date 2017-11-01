/*
 * Copyright (c) 2005 Atheme Development Group
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains routines to handle the GameServ HELP command.
 *
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"gameserv/help", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	VENDOR_STRING
);

static void gs_cmd_help(sourceinfo_t *si, int parc, char *parv[]);

command_t gs_help = { "HELP", N_("Displays contextual help information."), AC_NONE, 2, gs_cmd_help, { .path = "help" } };

void _modinit(module_t *m)
{
	service_named_bind_command("gameserv", &gs_help);
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("gameserv", &gs_help);
}

/* HELP <command> [params] */
void gs_cmd_help(sourceinfo_t *si, int parc, char *parv[])
{
	char *command = parv[0];

	if (!command)
	{
		command_success_nodata(si, _("***** \2%s Help\2 *****"), si->service->nick);
		command_success_nodata(si, _("\2%s\2 provides games and tools for playing games to the network."), si->service->nick);
		command_success_nodata(si, " ");
		command_success_nodata(si, _("For more information on a command, type:"));
		command_success_nodata(si, "\2/%s%s help <command>\2", (ircd->uses_rcommand == false) ? "msg " : "", si->service->disp);
		command_success_nodata(si, " ");

		command_help(si, si->service->commands);

		command_success_nodata(si, _("***** \2End of Help\2 *****"));
		return;
	}

	/* take the command through the hash table */
	help_display(si, si->service, command, si->service->commands);
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
