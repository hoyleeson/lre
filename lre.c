#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interpreter.h"
#include "lrc/builtin_lrc.h"

static LIST_HEAD(lre_modulelists);

struct lre_module {
	struct lrc_module *module;
	struct list_head entry;
};

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

static int lrc_arg_register(struct keyword_stub *parent, struct lrc_stub_arg *arg)
{
	struct keyword_stub *kstub;

	kstub = arg_keyword_install(arg->keyword, arg, arg->handler, parent);
	if(!kstub) {
		loge("Failed to register lrc arg '%s'.", arg->keyword);
		return -EINVAL;
	}

	return 0;
}

static int lrc_expr_register(struct keyword_stub *parent, struct lrc_stub_expr *expr)
{
	struct keyword_stub *kstub;

	kstub = expr_keyword_install(expr->keyword, expr, expr->handler, parent);
	if(!kstub) {
		loge("Failed to register lrc expr '%s'.", expr->keyword);
		return -EINVAL;
	}

	return 0;
}

static int lrc_var_register(struct keyword_stub *parent, struct lrc_stub_var *var)
{
	struct keyword_stub *kstub;

	kstub = var_keyword_install(var->keyword, var, var->handler, parent);
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

	if(!call->exec) {
		loge("Lrc call->exec callback mast be defined");	
		return -EINVAL;
	}
	kstub = call_keyword_install(call->keyword, call, call->handler, parent);
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

	kstub = func_keyword_install(func->keyword, func, func->handler, parent);
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

static void lrc_func_unregister(struct lrc_stub_func *func)
{
	struct keyword_stub *kstub;
	keystub_vec_t *vec;

	vec = get_root_keyvec();

	kstub = find_stub_by_keyword(vec, func->keyword);
	if(!kstub)
		return;

	func_keyword_uninstall(kstub);
}

static void lrc_call_unregister(struct lrc_stub_call *call)
{
	struct keyword_stub *kstub;
	keystub_vec_t *vec;

	vec = get_root_keyvec();

	kstub = find_stub_by_keyword(vec, call->keyword);
	if(!kstub)
		return;

	call_keyword_uninstall(kstub);
}



#define EXEC_DETAIL_GAP 		(4)
#define EXEC_DETAIL_UNIT_MAX 	(64)
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

	len = strlen(str) + 1;
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

	lre_module_create(module);
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


int lre_init(void)
{
	int ret;
	log_init(LOG_MODE_CALLBACK, LOG_VERBOSE);

	lre_conf_init();
	ret = interpreter_init();
	if(ret) {
		loge("Interpreter initialize failed.");
		return ret;
	}

	lrc_builtin_init();
	lre_macro_init();
	//interpreter_dump();
	return 0;
}

int lre_initX(const char *path, void (*logcb)(int, const char *))
{
	log_set_callback(logcb);
	lre_set_conf_path(path);
	return lre_init();
}

struct lre_result *lre_execute(const char *code)
{
	int ret;
	struct lre_result *res;	

	res = xzalloc(sizeof(*res));
	ret = interpreter_execute(code, res);
	if(ret) {
		loge("Error: Failed to interpret code:\n\t%s.", code);	
	}
	
	return res;
}

void lre_result_destroy(struct lre_result *res)
{
	if(res->details)
		free(res->details);
	free(res);
}

void lre_release(void)
{
	lrc_builtin_release();

	lre_macro_release();
	lre_conf_release();
	log_release();
}

