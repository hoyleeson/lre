#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../lre.h"

struct lrc_fuzzypath {
	struct lrc_object base;
};

int fuzzypath_execute(lrc_obj_t *lreobj, struct lre_value *val)
{
	return 0;
}

static lrc_obj_t *func_fuzzypath_handler(void)
{
	struct lrc_fuzzypath *fuzzypath;

	fuzzypath = malloc(sizeof(*fuzzypath));
	if(!fuzzypath) {
		return (lrc_obj_t *)0;
	}
	fuzzypath->base.execcall = fuzzypath_execute;

	return (lrc_obj_t *)fuzzypath;
}

static int arg_basepath_handler(lrc_obj_t *handle, struct lre_value *lreval)
{
	struct lrc_fuzzypath *fuzzypath;

	fuzzypath = (struct lrc_fuzzypath *)handle;

	return LRE_RET_OK;
}

static int arg_path_handler(lrc_obj_t *handle, struct lre_value *lreval)
{
	struct lrc_fuzzypath *fuzzypath;

	fuzzypath = (struct lrc_fuzzypath *)handle;

	return LRE_RET_OK;
}

static struct lrc_stub_arg fuzzypath_args[] = {
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
		.keyword 	 = "fuzzypath",
		.description = "Get path by fuzzypath.",
		.handler 	 = func_fuzzypath_handler,

		.args 	   = fuzzypath_args,
		.argcount  = ARRAY_SIZE(fuzzypath_args),
	}
};

struct lrc_module lrc_fuzzypath_mod = {
	.name = "lrc_call_fuzzypath",
	.calls = lrc_calls,
	.callcount = ARRAY_SIZE(lrc_calls),
};

int lrc_c_fuzzypath_init(void)
{
	int ret;

	ret = lrc_module_register(&lrc_fuzzypath_mod);
	if(ret)
		printf("Failed to register '%s' modules \n", lrc_fuzzypath_mod.name);
	return ret;
}

