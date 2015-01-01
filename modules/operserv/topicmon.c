/*
 * Copyright (c) 2014-2015 Chat Lounge IRC Network Development
 *
 * Author: Ben (Ben @ irc.chatlounge.net)
 *
 * Description:
 *
 *	This module will check for topic changes in any channel,
 * and post them to the services log channel.
 *
 */
 
#include "atheme.h"
#include "channels.h"
#include "common.h"
#include "servers.h"
#include "users.h"
 
DECLARE_MODULE_V1
(
	"operserv/topicmon", true, _modinit, _moddeinit,
	PACKAGE_STRING,
	"Chat Lounge IRC Network Development <http://www.chatlounge.net>"
);
 
static void watch_topic_changes(channel_t *channel);

void _modinit(module_t *m)
{
	hook_add_event("channel_topic");
	hook_add_channel_topic(watch_topic_changes);
};

void _moddeinit(module_unload_intent_t intent)
{
	hook_del_channel_topic(watch_topic_changes);
};

static void watch_topic_changes(channel_t *channel)
{
	//mowgli_node_t *n;
	//user_t *u = data->u;
	//channel_t *c = data->c;
	//server_t *s = data->s;

	//if (u == NULL)
	//	return;
	
	//if (!(data->s->flags & SF_EOB))
	//	return;
	
	slog(LG_INFO, "TOPICMON: \2%s\2 has changed the topic on \2%s\2 to:",
		channel->topic_setter, channel->name);
	slog(LG_INFO, "TOPICMON: %s",
		channel->topic);
};
