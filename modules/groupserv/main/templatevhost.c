/* templatevhost.c: Functions to handle vhost offers based
 * on group membership and matching templates.
 *
 */

#include "atheme.h"
#include "groupserv_common.h"
#include "groupserv_main.h"

const char *get_group_template_vhost(mygroup_t *mg, const char *name)
{
	metadata_t *md;
	char templatevhostmetadataname[128];
	char templatevhost[128];
	static char templatevhost2[128];
	int i = 0;

	if (name == NULL)
		return NULL;

	mowgli_strlcpy(templatevhost2, "", 128);

	snprintf(templatevhostmetadataname, sizeof templatevhostmetadataname,
		"private:templatevhost-%s", name);

	if (mg != NULL)
	{
		md = metadata_find(mg, templatevhostmetadataname);
		if (md != NULL)
		{
			snprintf(templatevhost, sizeof templatevhost, (char *)md->value);

			while (templatevhost[i] != '|' && templatevhost != '\0')
				i++;

			if (strlen(templatevhost) <= i)
				return NULL;

			snprintf(templatevhost2, sizeof templatevhost2, "%.*s", i, templatevhost);

			return templatevhost2;
		}
	}

	return NULL;
}

const char *get_group_template_vhost_by_flags(mygroup_t *mg, unsigned int level)
{
	return get_group_template_vhost(mg, get_group_template_name(mg, level));
}

time_t get_group_template_vhost_expiry(mygroup_t *mg, const char *name)
{
	metadata_t *md;
	char templatevhostmetadataname[128];
	char templatevhost[128];
	static char *templatevhost2;
	int i = 0;

	if (name == NULL || mg == NULL)
		return 0;

	snprintf(templatevhostmetadataname, sizeof templatevhostmetadataname,
		"private:templatevhost-%s", name);

	md = metadata_find(mg, templatevhostmetadataname);

	if (md == NULL)
		return 0;

	snprintf(templatevhost, sizeof templatevhost, (char *)md->value);

	while (templatevhost[i] != '|' && templatevhost != '\0')
		i++;

	if (strlen(templatevhost) <= i)
		return 0;

	templatevhost2 = strchr(templatevhost, '|');

	templatevhost2++;

	return atoi(templatevhost2);
}

time_t get_group_template_vhost_expiry_by_flags(mygroup_t *mg, unsigned int level)
{
	return get_group_template_vhost_expiry(mg, get_group_template_name(mg, level));
}