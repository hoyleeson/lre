#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lre_internal.h"

static char *conf_path = NULL;

void lre_set_conf_path(const char *path)
{
    if (!path)
        return;
    if (conf_path)
        free(conf_path);

    conf_path = strdup(path);
    assert_ptr(conf_path);
    logi("lre conf path:%s\n", conf_path);
}

const char *lre_get_conf_path(void)
{
    return conf_path ? conf_path : DEFAULT_LRE_CONF_PATH;
}

int lre_conf_init(void)
{
    return 0;
}

void lre_conf_release(void)
{
    if (conf_path)
        free(conf_path);
}
