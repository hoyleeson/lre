#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../lre.h"

struct lrc_process {
	struct lrc_object base;
	char *path;
};

static lrc_obj_t *func_process_handler(void)
{
	struct lrc_process *process;

	process = malloc(sizeof(*process));
	if(!process) {
		return (lrc_obj_t *)0;
	}

	return (lrc_obj_t *)process;
}

static int arg_procname_handler(lrc_obj_t *handle, struct lre_value *lreval)
{
	struct lrc_process *process;

	process = (struct lrc_process *)handle;

	if(!lreval || lreval->type != LRE_ARG_TYPE_STRING || !lreval->valstring)
		return LRE_RET_ERROR;

	process->path = strdup(lreval->valstring);
	printf("procname:%s\n", process->path);
	return LRE_RET_OK;
}

static int arg_procdir_handler(lrc_obj_t *handle, struct lre_value *lreval)
{
	struct lrc_process *process;

	process = (struct lrc_process *)handle;

	if(!lreval || lreval->type != LRE_ARG_TYPE_STRING || !lreval->valstring)
		return LRE_RET_ERROR;

	//	fuzzypath(lreval->valstring);
	process->path = strdup(lreval->valstring);
	return LRE_RET_OK;
}

static int expr_running_handler(lrc_obj_t *handle, int opt, struct lre_value *lreval)
{
	struct lrc_process *process;

	process = (struct lrc_process *)handle;
	if(!process->path)
		return LRE_RET_ERROR;

	return LRE_RESULT_TRUE;
}

static int expr_user_handler(lrc_obj_t *handle, int opt, struct lre_value *lreval)
{
	struct lrc_process *process;

	process = (struct lrc_process *)handle;
	if(!process->path)
		return LRE_RET_ERROR;

	return LRE_RESULT_TRUE;
}

static int expr_cmdline_handler(lrc_obj_t *handle, int opt, struct lre_value *lreval)
{
	struct lrc_process *process;

	process = (struct lrc_process *)handle;
	if(!process->path)
		return LRE_RET_ERROR;

	if(!lreval || lreval->type != LRE_ARG_TYPE_INT)
		return LRE_RET_ERROR;

	return LRE_RESULT_TRUE;
}

static struct lrc_stub_arg process_args[] = {
	{
		.keyword  	 = "procname",
		.description = "Type: string. Specify process name. example: procname=\"redis\"",
		.handler 	 = arg_procname_handler,
	}, {
		.keyword  	 = "procdir",
		.description = "Type: string. Specify process binary directory. example: procdir=\"/usr/local/\"",
		.handler 	 = arg_procdir_handler,
	}
};

static struct lrc_stub_expr process_exprs[] = {
	{
		.keyword  	 = "running",
		.description = "Check process running or not. val: 1 or 0, 1: running, 0: not running, example: running==1",
		.handler 	 = expr_running_handler,
	}, {
		.keyword  	 = "user",
		.description = "Check user who starts the process. example: user==root",
		.handler 	 = expr_user_handler,
	}, {
		.keyword  	 = "cmdline",
		.description = "Check Process cmdline. example: cmdline==\"-i\"",
		.handler 	 = expr_cmdline_handler,
	}
};

static struct lrc_stub_func lrc_funcs[] = {
	{
		.keyword 	 = "process",
		.description = "Check process user,cmdline,running or not, etc.",
		.handler 	 = func_process_handler,

		.args 	   = process_args,
		.argcount  = ARRAY_SIZE(process_args),
		.exprs     = process_exprs,
		.exprcount = ARRAY_SIZE(process_exprs),
	}
};

struct lrc_module lrc_process_mod = {
	.name = "lrc_process",
	.funcs = lrc_funcs,
	.funccount = ARRAY_SIZE(lrc_funcs),
};


int lrc_process_init(void)
{
	int ret;

	ret = lrc_module_register(&lrc_process_mod);
	if(ret)
		printf("Failed to register '%s' modules \n", lrc_process_mod.name);
	return ret;
}

