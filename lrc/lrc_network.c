#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../lre.h"

struct lrc_network {
	struct lrc_object base;
	int prototype;
};

static lrc_obj_t *func_network_handler(void)
{
	struct lrc_network *network;

	network = malloc(sizeof(*network));
	if(!network) {
		return (lrc_obj_t *)0;
	}

	return (lrc_obj_t *)network;
}

static int arg_prototype_handler(lrc_obj_t *handle, struct lre_value *lreval)
{
	struct lrc_network *network;

	network = (struct lrc_network *)handle;

	return LRE_RESULT_OK;
}

static int expr_port_handler(lrc_obj_t *handle, int opt, struct lre_value *lreval)
{
	struct lrc_network *network;

	network = (struct lrc_network *)handle;

	return LRE_RESULT_TRUE;
}

static int expr_prototype_handler(lrc_obj_t *handle, int opt, struct lre_value *lreval)
{
	struct lrc_network *network;

	network = (struct lrc_network *)handle;

	return LRE_RESULT_TRUE;
}

static int expr_tcpstate_handler(lrc_obj_t *handle, int opt, struct lre_value *lreval)
{
	struct lrc_network *network;

	network = (struct lrc_network *)handle;

	return LRE_RESULT_TRUE;
}

static struct lrc_stub_arg network_args[] = {
	{
		.keyword  	 = "prototype",
		.description = "",
		.handler 	 = arg_prototype_handler,
	}
};

static struct lrc_stub_expr network_exprs[] = {
	{
		.keyword  	 = "port",
		.description = "",
		.handler 	 = expr_port_handler,
	}, {
		.keyword  	 = "prototype",
		.description = "",
		.handler 	 = expr_prototype_handler,
	}, {
		.keyword  	 = "tcpstate",
		.description = "",
		.handler 	 = expr_tcpstate_handler,
	}
};

static struct lrc_stub_func lrc_funcs[] = {
	{
		.keyword 	 = "network",
		.description = "Check network connect state, listen port, etc.",
		.handler 	 = func_network_handler,

		.args 	   = network_args,
		.argcount  = ARRAY_SIZE(network_args),
		.exprs     = network_exprs,
		.exprcount = ARRAY_SIZE(network_exprs),
	}
};

struct lrc_module lrc_network_mod = {
	.name = "lrc_network",
	.funcs = lrc_funcs,
	.funccount = ARRAY_SIZE(lrc_funcs),
};

int lrc_network_init(void)
{
	int ret;

	ret = lrc_module_register(&lrc_network_mod);
	if(ret)
		printf("Failed to register '%s' modules \n", lrc_network_mod.name);
	return ret;
}

