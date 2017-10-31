/*
 * atheme-services: A collection of minimalist IRC services
 * template.c: Functions to work with predefined flags collections
 *
 * Copyright (c) 2005-2010 Atheme Project (http://www.atheme.org)
 * Copyright (c) 2016-2017 ChatLounge IRC Network Development Team
 *     (http://www.chatlounge.net)
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
//#include "template.h"
//#include "groupserv.h"
#include "groupserv_common.h"
#include "groupserv_main.h"

mowgli_patricia_t *global_group_template_dict = NULL;

void fix_global_group_template_flags(void)
{
	default_group_template_t *def_t;
	mowgli_patricia_iteration_state_t state;

	MOWGLI_PATRICIA_FOREACH(def_t, &state, global_group_template_dict)
	{
		def_t->flags &= GA_ALL;
	}
}

/* Template iteration - Needed for get_group_template_name */
typedef struct {
	const char *res;
	unsigned int level;
} template_iter_t;

void set_global_group_template_flags(const char *name, unsigned int flags)
{
	default_group_template_t *def_t;

	if (global_group_template_dict == NULL)
		global_group_template_dict = mowgli_patricia_create(strcasecanon);

	def_t = mowgli_patricia_retrieve(global_group_template_dict, name);

	// Debug
	slog(LG_INFO, "set_global_group_template_flags: add %s = %s", name, def_t == NULL ? "null" : "not null");

	if (def_t != NULL)
	{
		def_t->flags = flags;
		return;
	}

	def_t = smalloc(sizeof(default_group_template_t));
	def_t->flags = flags;
	// Debug
	if(mowgli_patricia_add(global_group_template_dict, name, def_t))
		slog(LG_INFO, "set_global_group_template_flags(): add %s is TRUE", name);
	else
		slog(LG_INFO, "set_global_group_template_flags(): add %s is FALSE", name);

	// Debug
	slog(LG_INFO, "set_global_group_template_flags(): add %s = %u", name, def_t->flags);
}

unsigned int get_global_group_template_flags(const char *name)
{
	default_group_template_t *def_t;

	if (global_group_template_dict == NULL)
		global_group_template_dict = mowgli_patricia_create(strcasecanon);

	def_t = mowgli_patricia_retrieve(global_group_template_dict, name);
	if (def_t == NULL)
		return 0;

	return def_t->flags;
}

static void release_global_group_template_data(const char *key, void *data, void *privdata)
{
	slog(LG_DEBUG, "release_global_group_template_data(): delete %s", key);

	free(data);
}

void clear_global_group_template_flags(void)
{
	if (global_group_template_dict == NULL)
		return;

	mowgli_patricia_destroy(global_group_template_dict, release_global_group_template_data, NULL);
	global_group_template_dict = NULL;
}

/* name1=value1 name2=value2 name3=value3... */
const char *get_group_item(const char *str, const char *name)
{
	char *p;
	static char result[300];
	int l;

	l = strlen(name);
	for (;;)
	{
		p = strchr(str, '=');
		if (p == NULL)
			return NULL;
		if (p - str == l && !strncasecmp(str, name, p - str))
		{
			mowgli_strlcpy(result, p, sizeof result);
			p = strchr(result, ' ');
			if (p != NULL)
				*p = '\0';
			return result;
		}
		str = strchr(p, ' ');
		if (str == NULL)
			return NULL;
		while (*str == ' ')
			str++;
	}
}

unsigned int get_group_template_flags(mygroup_t *mg, const char *name)
{
	metadata_t *md;
	const char *d;

	if (mg != NULL)
	{
		md = metadata_find(mg, "private:templates");
		if (md != NULL)
		{
			d = get_group_item(md->value, name);
			if (d != NULL)
				return gs_flags_parser((char *)d, 1, 0);
		}
	}

	return 0;
	//return get_global_group_template_flags(name);
}

static int global_group_template_search(const char *key, void *data, void *privdata)
{
	template_iter_t *iter = privdata;
	default_group_template_t *def_t = data;

	if (def_t->flags == iter->level)
		iter->res = key;

	return 0;
}

const char *get_group_template_name(mygroup_t *mg, unsigned int level)
{
	metadata_t *md;
	const char *p, *q, *r;
	char *s;
	char ss[40];
	static char flagname[400];
	template_iter_t iter;

	md = metadata_find(mg, "private:templates");
	if (md != NULL)
	{
		p = md->value;
		while (p != NULL)
		{
			while (*p == ' ')
				p++;
			q = strchr(p, '=');
			if (q == NULL)
				break;
			r = strchr(q, ' ');
			if (r != NULL && r < q)
				break;
			mowgli_strlcpy(ss, q, sizeof ss);
			if (r != NULL && r - q < (int)(sizeof ss - 1))
			{
				ss[r - q] = '\0';
			}
			//if (level == flags_to_bitmask(ss, 0))
			if (level == gs_flags_parser(ss, 0, 0))
			{
				mowgli_strlcpy(flagname, p, sizeof flagname);
				s = strchr(flagname, '=');
				if (s != NULL)
					*s = '\0';
				return flagname;
			}
			p = r;
		}
	}

	iter.res = NULL;
	iter.level = level;
	mowgli_patricia_foreach(global_group_template_dict, global_group_template_search, &iter);

	return iter.res;
}
