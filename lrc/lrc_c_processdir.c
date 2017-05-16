#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../lre.h"

struct lrc_processdir {
	struct lrc_object base;
};

static lrc_obj_t *func_processdir_handler(void)
{
	struct lrc_processdir *processdir;

	processdir = malloc(sizeof(*processdir));
	if(!processdir) {
		return (lrc_obj_t *)0;
	}

	return (lrc_obj_t *)processdir;
}

static int arg_procname_handler(lrc_obj_t *handle, struct lre_value *lreval)
{
	struct lrc_processdir *processdir;

	processdir = (struct lrc_processdir *)handle;

	return LRE_RESULT_OK;
}

static int arg_procdir_handler(lrc_obj_t *handle, struct lre_value *lreval)
{
	struct lrc_processdir *processdir;

	processdir = (struct lrc_processdir *)handle;

	return LRE_RESULT_OK;
}

static struct lrc_stub_arg processdir_args[] = {
	{
		.keyword  	 = "procname",
		.description = "",
		.handler 	 = arg_procname_handler,
	}, {
		.keyword  	 = "procdir",
		.description = "",
		.handler 	 = arg_procdir_handler,
	}
};

static struct lrc_stub_call lrc_calls[] = {
	{
		.keyword 	 = "processdir",
		.description = "Get process directory.",
		.handler 	 = func_processdir_handler,

		.args 	   = processdir_args,
		.argcount  = ARRAY_SIZE(processdir_args),
	}
};

struct lrc_module lrc_processdir_mod = {
	.name = "lrc_call_processdir",
	.calls = lrc_calls,
	.callcount = ARRAY_SIZE(lrc_calls),
};

int lrc_c_processdir_init(void)
{
	int ret;

	ret = lrc_module_register(&lrc_processdir_mod);
	if(ret)
		printf("Failed to register '%s' modules \n", lrc_processdir_mod.name);
	return ret;
}

