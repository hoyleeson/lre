#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lre_internal.h"
#include "lrc/builtin_lrc.h"

static LIST_HEAD(lre_modulelists);

struct lre_module {
	struct lrc_module *module;
	struct list_head entry;
};


void lre_value_dup2_string(struct lre_value *val, const char *str)
{
	val->type = LRE_ARG_TYPE_STRING;
	val->ptr = val->valstring = strdup(str);
	assert_ptr(val->ptr);
	val->flags = LRE_VALUE_F_NEEDFREE;
}

void lre_value_set_string(struct lre_value *val, char *str)
{
	val->type = LRE_ARG_TYPE_STRING;
	val->valstring = str;
}

void lre_value_set_int(struct lre_value *val, int ival)
{
	val->type = LRE_ARG_TYPE_INT;
	val->valint = ival;
}

void lre_value_set_double(struct lre_value *val, double dval)
{
	val->type = LRE_ARG_TYPE_DOUBLE;
	val->valdouble = dval;
}

const char *lre_value_get_string(struct lre_value *val)
{
	return val->valstring;
}

int lre_value_get_int(struct lre_value *val)
{
	return val->valint;
}

double lre_value_get_double(struct lre_value *val)
{
	return val->valdouble;
}

int lre_value_is_int(struct lre_value *val)
{
	return (val->type == LRE_ARG_TYPE_INT);
}

int lre_value_is_double(struct lre_value *val)
{
	return (val->type == LRE_ARG_TYPE_DOUBLE);
}

int lre_value_is_string(struct lre_value *val)
{
	return (val->type == LRE_ARG_TYPE_STRING);
}


void lre_value_init(struct lre_value *val)
{
	val->type = LRE_ARG_TYPE_UNKNOWN;
	val->ptr = NULL;
	val->flags = 0;
}

void lre_value_release(struct lre_value *val)
{
	if(val->flags == LRE_VALUE_F_NEEDFREE)
		free(val->ptr);
}

/****************************************************************/

static void lre_module_create(struct lrc_module *module)
{
	struct lre_module *lrm;

	lrm = xzalloc(sizeof(*lrm));
	lrm->module = module;
	list_add_tail(&lrm->entry, &lre_modulelists);
}

static void lre_module_delete(struct lrc_module *module)
{
	struct lre_module *lrm, *tmp;

	list_for_each_entry_safe(lrm, tmp, &lre_modulelists, entry) {
		if(lrm->module == module) {
			list_del(&lrm->entry);
		}
	}
}


static int lrc_call_register(struct keyword_stub *parent, struct lrc_stub_call *call)
{
	int i;
	struct keyword_stub *kstub;
	struct keyword_stub *substub;
	struct lrc_stub_arg *arg;
	struct lrc_stub_var *var;

	logi("Register lrc call '%s'.", call->keyword);
	if(!call->exec) {
		loge("Lrc call->exec callback must be defined");	
		return -EINVAL;
	}
	kstub = keyword_install(KEYSTUB_TYPE_CALL, call->keyword, call, parent);
	if(!kstub) {
		loge("Failed to register lrc call '%s'.", call->keyword);
		return -EINVAL;
	}

	for(i=0; i<call->argcount; i++) {
		arg = &call->args[i];
		substub = keyword_install(KEYSTUB_TYPE_ARG, arg->keyword, arg, kstub);
		if(!substub) {
			loge("Failed to register lrc arg '%s'.", arg->keyword);
			return -EINVAL;
		}
	}
	for(i=0; i<call->varcount; i++) {
		var = &call->vars[i];
		substub = keyword_install(KEYSTUB_TYPE_VAR, var->keyword, var, kstub);
		if(!substub) {
			loge("Failed to register lrc var '%s'.", var->keyword);
			return -EINVAL;
		}
	}

	return 0;
}

static int lrc_func_register(struct keyword_stub *parent, struct lrc_stub_func *func)
{
	int i;
	struct keyword_stub *kstub;
	struct keyword_stub *substub;
	struct lrc_stub_arg *arg;
	struct lrc_stub_expr *expr;
	struct lrc_stub_var *var;
	struct lrc_stub_func *subfunc;

	logi("Register lrc func '%s'.", func->keyword);
	kstub = keyword_install(KEYSTUB_TYPE_FUNC, func->keyword, func, parent);
	if(!kstub) {
		loge("Failed to register lrc func '%s'.", func->keyword);
		return -EINVAL;
	}

	for(i=0; i<func->argcount; i++) {
		arg = &func->args[i];
		substub = keyword_install(KEYSTUB_TYPE_ARG, arg->keyword, arg, kstub);
		if(!substub) {
			loge("Failed to register lrc arg '%s'.", arg->keyword);
			return -EINVAL;
		}
	}
	for(i=0; i<func->exprcount; i++) {
		expr = &func->exprs[i];
		substub = keyword_install(KEYSTUB_TYPE_EXPR, expr->keyword, expr, kstub);
		if(!substub) {
			loge("Failed to register lrc expr '%s'.", expr->keyword);
			return -EINVAL;
		}
	}
	for(i=0; i<func->varcount; i++) {
		var = &func->vars[i];
		substub = keyword_install(KEYSTUB_TYPE_VAR, var->keyword, var, kstub);
		if(!substub) {
			loge("Failed to register lrc var '%s'.", var->keyword);
			return -EINVAL;
		}
	}
	for(i=0; i<func->funccount; i++) {
		subfunc = &func->funcs[i];
		substub = keyword_install(KEYSTUB_TYPE_FUNC, subfunc->keyword, subfunc, kstub);
		if(!substub) {
			loge("Failed to register lrc subfunc '%s'.", subfunc->keyword);
			return -EINVAL;
		}
	}

	return 0;
}

static void lrc_func_unregister(struct lrc_stub_func *func)
{
	struct keyword_stub *kstub;
	keystub_vec_t *vec;

	vec = get_root_keyvec();

	kstub = find_stub_by_keyword(vec, func->keyword);
	if(!kstub)
		return;

	keyword_uninstall(kstub);
}

static void lrc_call_unregister(struct lrc_stub_call *call)
{
	struct keyword_stub *kstub;
	keystub_vec_t *vec;

	vec = get_root_keyvec();

	kstub = find_stub_by_keyword(vec, call->keyword);
	if(!kstub)
		return;

	keyword_uninstall(kstub);
}


#define CHECK_ERR(ret, tag)    do { if(ret) goto tag; } while(0)
int lrc_module_register(struct lrc_module *module)
{
	int i;
	int ret;

	if(!module) {
		loge("lrc module must be not NULL.");
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

	lre_module_create(module);
	logi("Register lrc module '%s' success.", module->name);
	return 0;
err:
	loge("Failed to register lrc module '%s'.", module->name);
	return -EINVAL;
}

void lrc_module_unregister(struct lrc_module *module)
{
	int i;

	if(!module) {
		loge("Failed to unregister module. module NULL.");
		return;
	}

	for(i=0; i<module->funccount; i++) {
		lrc_func_unregister(&module->funcs[i]);
	}

	for(i=0; i<module->callcount; i++) {
		lrc_call_unregister(&module->calls[i]);
	}

	lre_module_delete(module);
}

/****************************************************************/

struct lre_result *lre_execute(struct lre_context *ctx, const char *code)
{
	int ret;
	struct lre_result *res;
	struct interp *interp;

	res = &ctx->res;
	interp = ctx->interp;

	ret = interpreter_execute(interp, code);
	if(ret) {
		loge("Error: Failed to interpret code:\n\t%s.", code);	
	}
	res->result = interp_get_result(interp);
	res->errcode = interp_get_errcode(interp);
	res->details = interp_get_output(interp);

	return &ctx->res;
}

struct lre_context *lre_context_create(void)
{
	struct lre_context *ctx;

	ctx = xzalloc(sizeof(*ctx));

	ctx->interp = interpreter_create();
	return ctx;
}

void lre_context_destroy(struct lre_context *ctx)
{
	if(ctx->interp)
		free(ctx->interp);

	free(ctx);
}

int lre_init(void)
{
	int ret;

	log_init(LOG_MODE_CALLBACK, LOG_VERBOSE);

	ret = lre_conf_init();
	if(ret) {
		loge("Failed to lre config initialized.");
		return -EINVAL;
	}

	ret = lre_keyword_init();
	if(ret) {
		loge("Failed to lre keyword initialized.");
		return -EINVAL;
	}

	ret = lre_macro_init();
	if(ret) {
		loge("Failed to lre macro initialized.");
		return -EINVAL;
	}

	lrc_builtin_init();
	logi("Logic rule engine initialized.");
	return 0;
}

int lre_initX(const char *confpath, void (*logcb)(int, const char *))
{
	log_set_callback(logcb);
	lre_set_conf_path(confpath);
	return lre_init();
}

void lre_release(void)
{
	lrc_builtin_release();

	lre_macro_release();
	lre_keyword_release();
	lre_conf_release();

	log_release();
	logi("Logic rule engine released.");
}

void lre_help(void)
{
	printf("----------------------------------------------------\n");
	lre_keyword_dump();
	printf("----------------------------------------------------\n");
}
