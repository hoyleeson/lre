#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lre_internal.h"


#define VAL_UNINITIALIZED 	(0xff)

#define EXEC_STACK_GAP  	(64)
#define EXEC_OUT_UNIT_MAX 	(256)

void exec_output_cb(lrc_obj_t *handle, const char *str)
{
	int len;
	int etc = 0;
	struct interp *interp;
	struct interp_context *ctx;

	ctx = (struct interp_context *)handle->context;
	interp = ctx->interp;

	if(ctx->detailen + EXEC_OUT_UNIT_MAX >= ctx->detailcap) {
		if(interp_expands_outbuf(interp)) {
			loge("Execute: Expand output buf failed.");
			return;
		}
		ctx->details = interp->outbuf;
		ctx->detailcap = interp->obufsize;
	}

	len = strlen(str) + 1;
	if(len > EXEC_OUT_UNIT_MAX) {
		logw("lre push exec detail more than %d", EXEC_OUT_UNIT_MAX);
		len = EXEC_OUT_UNIT_MAX - 4;
		etc = 1;
	}

	ctx->detailen += snprintf(ctx->details + ctx->detailen, len, "%s", str);
	ctx->detailen += sprintf(ctx->details + ctx->detailen, "%s ", etc ? "...": "");
}


static inline int exec_ctx_push_handle(struct interp_context *ctx,
		lrc_obj_t *handle)
{
	if(ctx->stackidx + EXEC_STACK_GAP >= ctx->stackdepth)
		return -EINVAL;

	ctx->stack[ctx->stackidx++] = handle;
	handle->context = ctx;
	handle->output = exec_output_cb;
	return ctx->stackidx - 1;
}

static inline lrc_obj_t *exec_ctx_pop_handle(struct interp_context *ctx)
{
	return ctx->stack[--ctx->stackidx];
}

static inline lrc_obj_t *exec_ctx_get_handle(struct interp_context *ctx)
{
	return ctx->stack[ctx->stackidx - 1];
}


static int peek_val_type(const char *str)
{
	if(is_digit(str))
		return LRE_ARG_TYPE_INT;

	if(is_double_digit(str))
		return LRE_ARG_TYPE_DOUBLE;

	return LRE_ARG_TYPE_STRING;
}

static int val2lre_value(char *val, struct lre_value *arg)
{
	int l;
	int type;

	type = peek_val_type(val);
	arg->type = type;
	switch(type) {
		case LRE_ARG_TYPE_STRING:
			if(val[0] != '"') {
				arg->valstring = val;
				break;
			}
			l = strlen(val);
			val[l - 1] = 0;
			val++;
			arg->valstring = val;
			break;
		case LRE_ARG_TYPE_INT:
			arg->valint = atoi(val);
			break;
		case LRE_ARG_TYPE_DOUBLE:
			arg->valdouble = atof(val);
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

static int execute_syntax_childs(struct interp_context *ctx,
		struct syntax_root *root);
static int execute_syntax_call(struct interp_context *ctx,
		struct syntax_call *call, struct lre_value *arg);

static int execute_syntax_val(struct interp_context *ctx, 
		struct syntax_root *root, struct lre_value *arg)
{
	int ret = 0;
	struct syntax_node *node;
	struct syntax_val *exprval;
	struct syntax_valblock *exprvb;
	struct syntax_call *exprvcall;
	struct lre_value tmparg;
	int opt = SYNTAX_SYM_RESERVED;
	lrc_obj_t *handle;

	handle = exec_ctx_get_handle(ctx);

	list_for_each_entry(node, &root->head, entry) {
		lre_value_init(&tmparg);

		switch(node->type) {
			case SYNTAX_NODE_TYPE_VAL:	
				exprval = container_of(node, struct syntax_val, node);
				if(exprval->isvar) {
					struct keyword_stub *vstub;
					struct lrc_stub_var *stubvar;

					vstub = exprval->valstub;
					stubvar = (struct lrc_stub_var *)vstub->data;
					stubvar->handler(handle, &tmparg);
					if(ret != LRE_RET_OK) {
						loge("Execute err: failed to call '%s' var handler.", 
								vstub->keyword);
						goto out;	
					}
					break;
				}

				val2lre_value(exprval->val, &tmparg);
				break;
			case SYNTAX_NODE_TYPE_VALBLOCK:	
				exprvb = container_of(node, struct syntax_valblock, node);
				ret = execute_syntax_val(ctx, &exprvb->childs, &tmparg);
				if(ret)
					goto out;
				break;
			case SYNTAX_NODE_TYPE_CALL:
				exprvcall = container_of(node, struct syntax_call, node);
				ret = execute_syntax_call(ctx, exprvcall, &tmparg);
				if(ret)
					goto out;

				break;
			default:
				goto out;
		}
		
		if(tmparg.type == LRE_ARG_TYPE_UNKNOWN) {
			loge("Execute err: unknown arg type.");
			goto out;
		} else if(tmparg.type == LRE_ARG_TYPE_STRING) {
			*arg = tmparg;
			return 0;
		}

		if(arg->type == LRE_ARG_TYPE_UNKNOWN) {
			*arg = tmparg;
		} else if(arg->type == LRE_ARG_TYPE_DOUBLE || 
				tmparg.type == LRE_ARG_TYPE_DOUBLE) {
			arg->type = LRE_ARG_TYPE_DOUBLE;
			arg->valdouble = lre_calc_dobule(arg->valdouble, tmparg.valdouble, opt);
			if(arg->valint == RET_DEAD_VAL) {
				loge("Execute err: double calc error. op: %d", opt);
				goto out;
			}
		} else {
			arg->valint = lre_calc_int(arg->valint, tmparg.valint, opt);
			if(arg->valint == RET_DEAD_VAL) {
				loge("Execute err: integer calc error. op: %d", opt);
				goto out;
			}
		}

		opt = node->opt;
	}

	return 0;
out:
	return -EINVAL;
}

static int execute_syntax_expr(struct interp_context *ctx, 
		struct syntax_expr *expr)
{
	int ret;
	struct keyword_stub *estub;
	struct lrc_stub_expr *stubexpr;
	struct lre_value arg;
	lrc_obj_t *handle;

	handle = exec_ctx_get_handle(ctx);

	estub = expr->keystub;
	stubexpr = (struct lrc_stub_expr *)estub->data;

	logv("Execute expr '%s'.", estub->keyword);
	lre_value_init(&arg);
	ret = execute_syntax_val(ctx, &expr->valchilds, &arg);
	if(ret) {
		loge("Execute err: expr '%s' to lre_value err.", estub->keyword);
		return RET_DEAD_VAL;
	}

	ret = stubexpr->handler(handle, expr->opt, &arg);
	if(!vaild_lre_results(ret)) {
		loge("Execute err: failed to call '%s' expr handler.", estub->keyword);
		return RET_DEAD_VAL;
	}
	lre_value_release(&arg);
	logd("Execute expr '%s' result: %d.", estub->keyword, ret);

	return ret;
}

static int execute_syntax_func(struct interp_context *ctx, 
		struct syntax_func *func)
{
	int ret;
	struct keyword_stub *fstub;
	struct lrc_stub_func *stubfunc;
	lrc_obj_t *handle;

	fstub = func->keystub;
	stubfunc = (struct lrc_stub_func *)fstub->data;

	logv("Execute func '%s'.", fstub->keyword);
	handle = stubfunc->constructor();
	if(!handle) {
		loge("Execute err: failed to call '%s' func constructor.", fstub->keyword);
		return RET_DEAD_VAL;
	}

	handle->type = LRC_OBJECT_FUNC;
	exec_ctx_push_handle(ctx, handle);

	if(func->args.count > 0) {
		int i;
		struct lre_value arg;
		struct keyword_stub *astub;
		struct lrc_stub_arg *stubarg;
		struct syntax_args *args = &func->args;

		for(i=0; i<args->count; i++) {
			astub = args->args[i].keystub;
			stubarg = (struct lrc_stub_arg *)astub->data;

			lre_value_init(&arg);
			ret = execute_syntax_val(ctx, &args->args[i].valchilds, &arg);
			if(ret) {
				loge("Execute err: func arg '%s' to lre_value error.", astub->keyword);
				ret = RET_DEAD_VAL;
				goto out;
			}

			ret = stubarg->handler(handle, &arg);
			if(ret != LRE_RET_OK) {
				loge("Execute err: failed to call '%s' arg handler.(%d)",
					   	astub->keyword, ret);
				ret = RET_DEAD_VAL;
				goto out;
			}
			lre_value_release(&arg);
		}
	}

	if(stubfunc->exec) {
		ret = stubfunc->exec(handle);
		if(ret) {
			loge("Execute err: failed to func '%s' inner func.", fstub->keyword);
			ret = RET_DEAD_VAL;
			goto out;
		}
	}

	ret = execute_syntax_childs(ctx, &func->body.childs);

out:
	if(stubfunc->destructor)
		stubfunc->destructor(handle);

	exec_ctx_pop_handle(ctx);
	logd("Execute func '%s' result: %d.", fstub->keyword, ret);
	return ret;
}

static int execute_syntax_call(struct interp_context *ctx,
		struct syntax_call *call, struct lre_value *arg)
{
	int ret;
	struct keyword_stub *cstub;
	struct lrc_stub_call *stubcall;
	lrc_obj_t *handle;

	cstub = call->keystub;

	logv("Execute call '%s'.", cstub->keyword);
	stubcall = (struct lrc_stub_call *)cstub->data;
	handle = stubcall->constructor();
	if(!handle) {
		loge("Execute err: failed to call '%s' call constructor.", cstub->keyword);
		return -EINVAL;
	}

	handle->type = LRC_OBJECT_CALL;
	exec_ctx_push_handle(ctx, handle);

	if(call->args.count > 0) {
		int i;
		struct lre_value arg;
		struct keyword_stub *astub;
		struct lrc_stub_arg *stubarg;
		struct syntax_args *args = &call->args;

		for(i=0; i<args->count; i++) {
			astub = args->args[i].keystub;
			stubarg = (struct lrc_stub_arg *)astub->data;

			lre_value_init(&arg);
			ret = execute_syntax_val(ctx, &args->args[i].valchilds, &arg);
			if(ret) {
				loge("Execute err: call arg '%s' to lre_vaule err.", astub->keyword);
				return ret;
			}

			ret = stubarg->handler(handle, &arg);
			if(ret != LRE_RET_OK) {
				loge("Execute err: failed to call '%s' arg handler.(%d)", 
						astub->keyword, ret);
				ret = -EINVAL;
				goto out;
			}
			lre_value_release(&arg);
		}
	}

	if(!stubcall->exec) {
		loge("Execute err: lrc call '%s' execfn is NULL.", cstub->keyword);
		ret = -EINVAL;
		goto out;
	}

	ret = stubcall->exec(handle, arg);
	if(ret) {
		loge("Execute err: failed to call '%s' inner func.", cstub->keyword);
		ret = -EINVAL;
		goto out;
	}

	ret = 0;
out:
	if(stubcall->destructor)
		stubcall->destructor(handle);

	exec_ctx_pop_handle(ctx);
	logd("Execute call '%s' result: %d.", cstub->keyword, ret);
	return ret;
}

static int execute_syntax_block(struct interp_context *ctx, 
		struct syntax_block *block)
{
	int ret;

	ret = execute_syntax_childs(ctx, &block->childs);

	if(block->negative)
		ret = !ret;
	return ret;
}

static int execute_syntax_content(struct interp_context *ctx, 
		struct syntax_content *content)
{
	return execute_syntax_childs(ctx, &content->childs);
}


static int execute_syntax_childs(struct interp_context *ctx, 
		struct syntax_root *root)
{
	int ret;
	struct syntax_func *func;
	struct syntax_block *block;
	struct syntax_expr *expr;
	struct syntax_node *node;
	int opt = SYNTAX_SYM_RESERVED;
	int res = VAL_UNINITIALIZED;

	list_for_each_entry(node, &root->head, entry) {
		switch(node->type) {
			case SYNTAX_NODE_TYPE_FUNC:	
				func = container_of(node, struct syntax_func, node);
				ret = execute_syntax_func(ctx, func);
				break;
			case SYNTAX_NODE_TYPE_EXPR:	
				expr = container_of(node, struct syntax_expr, node);
				ret = execute_syntax_expr(ctx, expr);
				break;
			case SYNTAX_NODE_TYPE_BLOCK:	
				block = container_of(node, struct syntax_block, node);
				ret = execute_syntax_block(ctx, block);
				break;
			default:
				ret = RET_DEAD_VAL;
				break;
		}
		if(!vaild_lre_results(ret)) {
			return RET_DEAD_VAL;
		}

		if(res == VAL_UNINITIALIZED)
			res = ret;
		else {
			switch(opt) {
				case SYNTAX_SYM_AND:
					res = res && ret;
					break;
				case SYNTAX_SYM_OR:
					res = res || ret;
					break;
				default:
					return RET_DEAD_VAL;
			}
		}

		if((node->opt == SYNTAX_SYM_AND && !res) ||
			   	(node->opt == SYNTAX_SYM_OR && res))
			return res;
		opt = node->opt;
	}
	return res;
}

static int execute_syntax_tree(struct interp_context *ctx,
		struct syntax_root *root)
{
	struct syntax_content *content;

	if(!root)
		return -EINVAL;

	content = container_of(root, struct syntax_content, childs);
	return execute_syntax_content(ctx, content);
}


int interp_execute(struct interp_context *ctx)
{
	int ret;
	struct interp *interp = ctx->interp;

	ctx->stack = interp->stacks;
	ctx->stackidx = 0;
	ctx->stackdepth = interp->stacksize;

	logd("Execute: Start exec rules.");
	ret = execute_syntax_tree(ctx, ctx->tree);
	if(ctx->stackidx > 0) {
		loge("Execute err: exec rules interrupted.(stack: %d, ret: %d)", 
				ctx->stackidx, ret);
	}

	return ret;
}

