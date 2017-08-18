/*
 * atheme-services: A collection of minimalist IRC services
 * ctcp-common.c: Handling of CTCP commands.
 *
 * Copyright (c) 2005-2007 Atheme Project (http://www.atheme.org)
 * Copyright (c) 2017 ChatLounge IRC Network Development Team (http://www.chatlounge.net)
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "atheme.h"
#include "datastream.h"
#include "privs.h"
#include "pmodule.h"

mowgli_patricia_t *ctcptree;

static void ctcp_ping_handler(sourceinfo_t *si, char *cmd, char *args)
{
	char *s;

	s = strtok(args, "\001");
	if (s != NULL)
		strip(s);

	notice(si->service->nick, si->su->nick, "\001PING %.100s\001", s != NULL ? s : "pong!");
}

static void ctcp_version_handler(sourceinfo_t *si, char *cmd, char *args)
{
	char ver[BUFSIZE];

	get_version_string(ver, sizeof(ver));
	notice(si->service->nick, si->su->nick, "\001VERSION %s\001", ver);
}

static void ctcp_clientinfo_handler(sourceinfo_t *si, char *cmd, char *args)
{
	notice(si->service->nick, si->su->nick, "\001CLIENTINFO PING VERSION CLIENTINFO\001");
}

/* easter egg (so is the next one, who cares) */
static void ctcp_machinegod_handler(sourceinfo_t *si, char *cmd, char *args)
{
	notice(si->service->nick, si->su->nick, "\001MACHINEGOD http://www.findagrave.com/cgi-bin/fg.cgi?page=gr&GRid=10369601\001");
}

/* update this as necessary to track notable events */
static void ctcp_about_handler(sourceinfo_t *si, char *cmd, char *args)
{
	char reply[BUFSIZE];
	/*
	 * October 18th, 2014: The date that Ben officially decided to start the ChatIRCd and ChatServices projects.
	 *     The ChatLounge IRC Network Development Team does not recommend using UnrealIRCd for anything.
	 */
	if (config_options.helpchan && config_options.helpurl)
		snprintf(reply, sizeof reply, "\001ABOUT \002ChatServices\002 is developed by Ben, the lead developer of the ChatLounge IRC Network Development Team.  For software support, please join #ChatServices at \037irc.chatlounge.net\037.  If you're a normal user who needs assistance, please join your official network help channel instead at \002%s\002 or consult: \002%s\002",
			config_options.helpchan, config_options.helpurl);
	else if (config_options.helpchan && !config_options.helpurl)
		snprintf(reply, sizeof reply, "\001ABOUT \002ChatServices\002 is developed by Ben, the lead developer of the ChatLounge IRC Network Development Team.  For software support, please join #ChatServices at \037irc.chatlounge.net\037.  If you're a normal user who needs assistance, please join your official network help channel instead at: \002%s\002",
			config_options.helpchan);
	else if (!config_options.helpchan && config_options.helpurl)
		snprintf(reply, sizeof reply, "\001ABOUT \002ChatServices\002 is developed by Ben, the lead developer of the ChatLounge IRC Network Development Team.  For software support, please join #ChatServices at \037irc.chatlounge.net\037.  If you're a normal user who needs assistance, please join your official network help channel instead or consult: \002%s\002",
			config_options.helpurl);
	else
		snprintf(reply, sizeof reply, "\001ABOUT \002ChatServices\002 is developed by Ben, the lead developer of the ChatLounge IRC Network Development Team.  For software support, please join #ChatServices at \037irc.chatlounge.net\037.  If you're a normal user who needs assistance, please join your official network help channel instead.");

	notice(si->service->nick, si->su->nick, reply);
}

void common_ctcp_init(void)
{
	ctcptree = mowgli_patricia_create(noopcanon);

	mowgli_patricia_add(ctcptree, "\001PING", ctcp_ping_handler);
	mowgli_patricia_add(ctcptree, "\001VERSION\001", ctcp_version_handler);
	mowgli_patricia_add(ctcptree, "\001CLIENTINFO\001", ctcp_clientinfo_handler);
	mowgli_patricia_add(ctcptree, "\001MACHINEGOD\001", ctcp_machinegod_handler);
	mowgli_patricia_add(ctcptree, "\001ABOUT\001", ctcp_about_handler);
}

unsigned int handle_ctcp_common(sourceinfo_t *si, char *cmd, char *args)
{
	void (*handler)(sourceinfo_t *si, char *, char *);

	if ((handler = mowgli_patricia_retrieve(ctcptree, cmd)) != NULL)
	{
		handler(si, cmd, args);
		return 1;
	}

	return 0;
}

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
