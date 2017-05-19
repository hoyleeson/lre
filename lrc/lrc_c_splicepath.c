#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "../log.h"
#include "../utils.h"
#include "../lre.h"

struct lrc_splicepath {
	struct lrc_object base;
	char *basepath;
	char *path;
};

int splicepath_execute(lrc_obj_t *handle, struct lre_value *val)
{
	char path[PATH_MAX] = {0};
	int len = 0;
	struct lrc_splicepath *splicepath;
	char *outpath;

	splicepath = (struct lrc_splicepath *)handle;
	if(splicepath->basepath)
		len += snprintf(path + len, PATH_MAX - len, "%s/", splicepath->basepath);
	if(splicepath->path)
		len += snprintf(path + len, PATH_MAX - len, "%s", splicepath->path);

	if(strlen(path) == 0)
		return LRE_RET_ERROR;

	outpath = strdup(path);

	logd("splice path finish. path:%s", outpath);
	val->type = LRE_ARG_TYPE_STRING;
	val->valstring = outpath;
	return LRE_RET_OK;
}

static lrc_obj_t *func_splicepath_handler(void)
{
	struct lrc_splicepath *splicepath;

	splicepath = malloc(sizeof(*splicepath));
	if(!splicepath) {
		return (lrc_obj_t *)0;
	}
	splicepath->base.execcall = splicepath_execute;

	splicepath->basepath = NULL;
	splicepath->path = NULL;

	return (lrc_obj_t *)splicepath;
}

static int arg_basepath_handler(lrc_obj_t *handle, struct lre_value *lreval)
{
	struct lrc_splicepath *splicepath;

	splicepath = (struct lrc_splicepath *)handle;
	if(!lreval || lreval->type != LRE_ARG_TYPE_STRING || !lreval->valstring)
		return LRE_RET_ERROR;

	splicepath->basepath = strdup(lreval->valstring);
	logd("splicepath arg handler: basepath:%s", splicepath->basepath);
	return LRE_RET_OK;
}

static int arg_path_handler(lrc_obj_t *handle, struct lre_value *lreval)
{
	struct lrc_splicepath *splicepath;

	splicepath = (struct lrc_splicepath *)handle;
	if(!lreval || lreval->type != LRE_ARG_TYPE_STRING || !lreval->valstring)
		return LRE_RET_ERROR;

	splicepath->path = strdup(lreval->valstring);
	logd("splicepath arg handler: path:%s", splicepath->path);
	return LRE_RET_OK;
}

static struct lrc_stub_arg splicepath_args[] = {
	{
		.keyword  	 = "basepath",
		.description = "",
		.handler 	 = arg_basepath_handler,
	}, {
		.keyword  	 = "path",
		.description = "",
		.handler 	 = arg_path_handler,
	}
};

static struct lrc_stub_call lrc_calls[] = {
	{
		.keyword 	 = "splicepath",
		.description = "splice path by fragment.",
		.handler 	 = func_splicepath_handler,

		.args 	   = splicepath_args,
		.argcount  = ARRAY_SIZE(splicepath_args),
	}
};

struct lrc_module lrc_splicepath_mod = {
	.name = "lrc_call_splicepath",
	.calls = lrc_calls,
	.callcount = ARRAY_SIZE(lrc_calls),
};

int lrc_c_splicepath_init(void)
{
	int ret;

	ret = lrc_module_register(&lrc_splicepath_mod);
	if(ret)
		loge("Failed to register '%s' modules ", lrc_splicepath_mod.name);
	return ret;
}

