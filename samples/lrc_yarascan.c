#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../lre.h"

struct lrc_yarascan {
	struct lrc_object base;
};

static lrc_obj_t *func_yarascan_handler(void)
{
	struct lrc_yarascan *yarascan;

	yarascan = malloc(sizeof(*yarascan));
	if(!yarascan) {
		return (lrc_obj_t *)0;
	}

	return (lrc_obj_t *)yarascan;
}

static int arg_rule_handler(lrc_obj_t *handle, struct lre_value *lreval)
{
	struct lrc_yarascan *yarascan;

	yarascan = (struct lrc_yarascan *)handle;

	return LRE_RET_OK;
}

static int arg_target_handler(lrc_obj_t *handle, struct lre_value *lreval)
{
	struct lrc_yarascan *yarascan;

	yarascan = (struct lrc_yarascan *)handle;

	return LRE_RET_OK;
}

static int expr_matched_handler(lrc_obj_t *handle, int opt, struct lre_value *lreval)
{
	struct lrc_yarascan *yarascan;

	return LRE_RESULT_TRUE;
}


static struct lrc_stub_arg yarascan_args[] = {
	{
		.keyword  	 = "rule",
		.description = "Type: string. Specify the path to a yarascan",
		.handler 	 = arg_rule_handler,
	}, {
		.keyword  	 = "target",
		.description = "Type: string. Specify the path to a yarascan",
		.handler 	 = arg_target_handler,
	}
};

static struct lrc_stub_expr yarascan_exprs[] = {
	{
		.keyword  	 = "matched",
		.description = "File exist or not. val: 1 or 0, 1: exist, 0: not exist, example: exist==1",
		.handler 	 = expr_matched_handler,
	}
};

static struct lrc_stub_func lrc_funcs[] = {
	{
		.keyword 	 = "yarascan",
		.description = "Check yarascan permissions,owner,is exist or not, etc.",
		.handler 	 = func_yarascan_handler,

		.args 	   = yarascan_args,
		.argcount  = ARRAY_SIZE(yarascan_args),
		.exprs     = yarascan_exprs,
		.exprcount = ARRAY_SIZE(yarascan_exprs),
	}
};

struct lrc_module lrc_yarascan_mod = {
	.name = "lrc_yarascan",
	.funcs = lrc_funcs,
	.funccount = ARRAY_SIZE(lrc_funcs),
};

int lrc_yarascan_init(void)
{
	int ret;

	ret = lrc_module_register(&lrc_yarascan_mod);
	if(ret)
		printf("Failed to register '%s' modules \n", lrc_yarascan_mod.name);

	return ret;
}

