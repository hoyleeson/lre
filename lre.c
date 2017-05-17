#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define EXEC_DETAIL_GAP 		(4)
#define EXEC_DETAIL_UNIT_MAX 	(32)
#define EXEC_DETAIL_LEN_MAX 	(100 * EXEC_DETAIL_UNIT_MAX)

void lre_exec_detail_init(struct lre_exec_detail *detail, int cap)
{
	detail->detail = xzalloc(cap + EXEC_DETAIL_GAP);
	detail->cap = cap;
	detail->len = 0;
}

void lre_exec_detail_release(struct lre_exec_detail *detail)
{
	free(detail->detail);
	detail->cap = detail->len = 0;
}

static void lre_exec_detail_expand(struct lre_exec_detail *detail)
{
	char *ptr;
	int newcap;

	newcap = detail->cap * 2;
	ptr = realloc(detail->detail, newcap + EXEC_DETAIL_GAP);
	if(!ptr) {
		fatal("Expand exec detail memory failed.");
	}
	detail->cap = newcap;
	detail->detail = ptr;
}

int lre_push_exec_detail(struct lrc_object *obj, const char *str)
{
	int len;
	int etc = 0;
	struct lre_exec_detail *detail = obj->detail;

	if(detail->len + EXEC_DETAIL_UNIT_MAX >= EXEC_DETAIL_LEN_MAX)
		return -ENOMEM;

	len = strlen(str);
	if(len > EXEC_DETAIL_UNIT_MAX) {
		logw("lre push exec detail more than %d", EXEC_DETAIL_UNIT_MAX);
		len = EXEC_DETAIL_UNIT_MAX - 4;
		etc = 1;
	}

	if(detail->cap - detail->len < len)
		lre_exec_detail_expand(detail);

	detail->len += snprintf(detail->detail + detail->len, len, "%s", str);
	detail->len += sprintf(detail->detail + detail->len, "%s, ", etc ? "...": "");
	return 0;
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

int lre_execute(const char *code, struct lre_result *res)
{
	return interpreter_execute(code, res);
}

void lre_release(void)
{
	log_release();
}

