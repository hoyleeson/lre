#ifndef _LRE_CONF_H
#define _LRE_CONF_H

#define DEFAULT_LRE_CONF_PATH 		"."

const char *lre_get_conf_path(void);
void lre_set_conf_path(const char *path);

int lre_conf_init(void);
void lre_conf_release(void);

#endif

