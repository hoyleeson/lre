#include <stdio.h>

#include "interpreter.h"


static int lrc_arg_register(struct keyword_stub *parent, struct lrc_stub_arg *arg)
{
	struct keyword_stub *kstub;

	kstub = arg_keyword_install(arg->keyword, arg->description, arg->handler, parent);
	if(!kstub) {
		loge("Failed to register lrc arg '%s'.", arg->keyword);
		return -EINVAL;
	}

	return 0;
}

static int lrc_expr_register(struct keyword_stub *parent, struct lrc_stub_expr *expr)
{
	struct keyword_stub *kstub;

	kstub = expr_keyword_install(expr->keyword, expr->description, expr->handler, parent);
	if(!kstub) {
		loge("Failed to register lrc expr '%s'.", expr->keyword);
		return -EINVAL;
	}

	return 0;
}

static int lrc_var_register(struct keyword_stub *parent, struct lrc_stub_var *var)
{
	struct keyword_stub *kstub;

	kstub = var_keyword_install(var->keyword, var->description, var->handler, parent);
	if(!kstub) {
		loge("Failed to register lrc var '%s'.", var->keyword);
		return -EINVAL;
	}

	return 0;
}

#define CHECK_ERR(ret, tag) 	do { if(ret) goto tag; } while(0)
static int lrc_call_register(struct keyword_stub *parent, struct lrc_stub_call *call)
{
	int i;
	int ret;
	struct keyword_stub *kstub;

	kstub = call_keyword_install(call->keyword, call->description, call->handler, parent);
	if(!kstub) {
		loge("Failed to register lrc call '%s'.", call->keyword);
		return -EINVAL;
	}

	for(i=0; i<call->argcount; i++) {
		ret = lrc_arg_register(kstub, &call->args[i]);
		CHECK_ERR(ret, err);
	}
	for(i=0; i<call->varcount; i++) {
		ret = lrc_var_register(kstub, &call->vars[i]);
		CHECK_ERR(ret, err);
	}

	return 0;
err:
	return -EINVAL;
}

static int lrc_func_register(struct keyword_stub *parent, struct lrc_stub_func *func)
{
	int i;
	int ret;
	struct keyword_stub *kstub;

	kstub = func_keyword_install(func->keyword, func->description, func->handler, parent);
	if(!kstub) {
		loge("Failed to register lrc func '%s'.", func->keyword);
		return -EINVAL;
	}

	for(i=0; i<func->argcount; i++) {
		ret = lrc_arg_register(kstub, &func->args[i]);
		CHECK_ERR(ret, err);
	}
	for(i=0; i<func->exprcount; i++) {
		ret = lrc_expr_register(kstub, &func->exprs[i]);
		CHECK_ERR(ret, err);
	}
	for(i=0; i<func->varcount; i++) {
		ret = lrc_var_register(kstub, &func->vars[i]);
		CHECK_ERR(ret, err);
	}
	for(i=0; i<func->funccount; i++) {
		ret = lrc_func_register(kstub, &func->funcs[i]);
		CHECK_ERR(ret, err);
	}

	return 0;
err:
	return -EINVAL;
}

int lrc_module_register(struct lrc_module *module)
{
	int i;
	int ret;

	if(!module) {
		loge("lrc module mast be not NULL.");
		return -EINVAL;
	}

	for(i=0; i<module->funccount; i++) {
		ret = lrc_func_register(NULL, &module->funcs[i]);
		CHECK_ERR(ret, err);
	}

	for(i=0; i<module->callcount; i++) {
		ret = lrc_call_register(NULL, &module->calls[i]);
		CHECK_ERR(ret, err);
	}
	return 0;
err:
	loge("Failed to register lrc module '%s'.", module->name);
	return -EINVAL;
}


int lre_init(void)
{
	int ret;
	log_init(LOG_MODE_STDERR, LOG_VERBOSE);
	ret = interpreter_init();
	if(ret) {
		loge("Interpreter initialize failed.");
		return ret;
	}
	return 0;
}

int lre_execute(const char *code)
{
	return interpreter_execute(code);
}

void lre_release(void)
{
	log_release();
}

