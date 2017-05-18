#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interpreter.h"


#define VAL_UNINITIALIZED 	(0xff)

#define EXEC_STACK_DEPTH 	(4096)	
#define EXEC_STACK_GAP  	(64)


struct exec_context {
	lrc_obj_t *stack[EXEC_STACK_DEPTH];	
	int index;
	struct lre_exec_detail detail;
};


static inline int exec_ctx_push_handle(struct exec_context *ctx, 
		lrc_obj_t *handle)
{
	if(ctx->index + EXEC_STACK_GAP >= EXEC_STACK_DEPTH)
		return -EINVAL;

	ctx->stack[ctx->index++] = handle;
	handle->detail = &ctx->detail;
	return ctx->index - 1;
}

static inline lrc_obj_t *exec_ctx_pop_handle(struct exec_context *ctx)
{
	return ctx->stack[--ctx->index];
}

static inline lrc_obj_t *exec_ctx_get_handle(struct exec_context *ctx)
{
	return ctx->stack[ctx->index - 1];
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

static int execute_syntax_childs(struct syntax_root *root, struct exec_context *ctx);
static int execute_syntax_call(struct syntax_call *call, 
		struct exec_context *ctx, struct lre_value *arg);

static int execute_syntax_val(struct syntax_root *root, 
		struct exec_context *ctx, struct lre_value *arg)
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
		tmparg.type = LRE_ARG_TYPE_UNKNOWN;

		switch(node->type) {
			case SYNTAX_NODE_TYPE_VAL:	
				exprval = container_of(node, struct syntax_val, node);
				if(exprval->isvar) {
					struct keyword_stub *valstub;
					valstub = exprval->valstub;
					valstub->var_handler(handle, &tmparg);
					if(ret != LRE_RET_OK) {
						loge("Execute err: failed to call '%s' var_handler.", 
								valstub->keyword);
						goto out;	
					}
					break;
				}

				val2lre_value(exprval->val, &tmparg);
				break;
			case SYNTAX_NODE_TYPE_VALBLOCK:	
				exprvb = container_of(node, struct syntax_valblock, node);
				ret = execute_syntax_val(&exprvb->childs, ctx, &tmparg);
				if(ret)
					goto out;
				break;
			case SYNTAX_NODE_TYPE_CALL:
				exprvcall = container_of(node, struct syntax_call, node);
				ret = execute_syntax_call(exprvcall, ctx, &tmparg);
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
			if(arg->valint == RET_DEAD_VAL)
				goto out;
		} else {
			arg->valint = lre_calc_int(arg->valint, tmparg.valint, opt);
			if(arg->valint == RET_DEAD_VAL)
				goto out;
		}

		opt = node->opt;
	}

	return 0;
out:
	return -EINVAL;
}

static int execute_syntax_expr(struct syntax_expr *expr, struct exec_context *ctx)
{
	int ret;
	struct keyword_stub *keystub;
	struct lre_value arg;
	lrc_obj_t *handle;

	handle = exec_ctx_get_handle(ctx);

	keystub = expr->keystub;

	logv("Execute expr '%s'.", keystub->keyword);
	arg.type = LRE_ARG_TYPE_UNKNOWN;
	ret = execute_syntax_val(&expr->valchilds, ctx, &arg);
	if(ret) {
		loge("Execute err: expr '%s' to lre_value err.", keystub->keyword);
		return RET_DEAD_VAL;
	}

	ret = keystub->expr_handler(handle, expr->opt, &arg);
	if(!vaild_lre_results(ret)) {
		loge("Execute err: '%s' expr handler exec err.", keystub->keyword);
		return RET_DEAD_VAL;
	}
	logi("Execute expr '%s' result: %d.", keystub->keyword, ret);

	return ret;
}

static int execute_syntax_func(struct syntax_func *func, 
		struct exec_context *ctx)
{
	int ret;
	struct keyword_stub *funcstub;
	lrc_obj_t *handle;

	funcstub = func->keystub;

	logd("Execute func '%s'.", funcstub->keyword);
	handle = funcstub->func_handler();
	if(!handle) {
		loge("Execute err: failed to call '%s' func_handler.", funcstub->keyword);
		goto out;
	}

	exec_ctx_push_handle(ctx, handle);

	if(func->args.count > 0) {
		int i;
		struct syntax_args *args = &func->args;
		struct lre_value arg;
		struct keyword_stub *argstub;

		for(i=0; i<args->count; i++) {
			argstub = args->args[i].keystub;

			arg.type = LRE_ARG_TYPE_UNKNOWN;
			ret = execute_syntax_val(&args->args[i].valchilds, ctx, &arg);
			if(ret) {
				loge("Execute err: func arg '%s' to lre_value err.", argstub->keyword);
				return RET_DEAD_VAL;
			}

			ret = argstub->arg_handler(handle, &arg);
			if(ret != LRE_RET_OK) {
				loge("Execute err: failed to call '%s' arg_handler.(%d)",
					   	argstub->keyword, ret);
				goto out;
			}
		}
	}

	/* FIXME: */
	if(handle->execfunc) {
		ret = handle->execfunc(handle);
		if(ret) {
			loge("Execute err: failed to func '%s' inner func.", funcstub->keyword);
			ret = -EINVAL;
			goto out;
		}
	}

	ret = execute_syntax_childs(&func->body.childs, ctx);

	exec_ctx_pop_handle(ctx);
	return ret;

out:
	return RET_DEAD_VAL;
}

static int execute_syntax_call(struct syntax_call *call, 
		struct exec_context *ctx, struct lre_value *arg)
{
	int ret;
	struct keyword_stub *callstub;
	lrc_obj_t *handle;

	callstub = call->keystub;

	logd("Execute call '%s'.", callstub->keyword);
	handle = callstub->call_handler();
	if(!handle) {
		loge("Execute err: failed to call '%s' call_handler.", callstub->keyword);
		return -EINVAL;
	}

	exec_ctx_push_handle(ctx, handle);

	if(call->args.count > 0) {
		int i;
		struct syntax_args *args = &call->args;
		struct lre_value arg;
		struct keyword_stub *argstub;

		for(i=0; i<args->count; i++) {
			argstub = args->args[i].keystub;

			arg.type = LRE_ARG_TYPE_UNKNOWN;
			ret = execute_syntax_val(&args->args[i].valchilds, ctx, &arg);
			if(ret) {
				loge("Execute err: call arg '%s' to lre_vaule err.", argstub->keyword);
				return ret;
			}

			ret = argstub->arg_handler(handle, &arg);
			if(ret != LRE_RET_OK) {
				loge("Execute err: failed to call '%s' arg_handler.(%d)", 
						argstub->keyword, ret);
				ret = -EINVAL;
				goto out;
			}
		}
	}

	if(!handle->execcall) {
		loge("Execute err: lrc call '%s' execfn is NULL.", callstub->keyword);
		ret = -EINVAL;
		goto out;
	}

	/* FIXME: */
	ret = handle->execcall(handle, arg);
	if(ret) {
		loge("Execute err: failed to call '%s' inner func.", callstub->keyword);
		ret = -EINVAL;
		goto out;
	}

	ret = 0;
out:
	exec_ctx_pop_handle(ctx);
	return ret;
}

static int execute_syntax_block(struct syntax_block *block,
	   	struct exec_context *ctx)
{
	int ret;

	ret = execute_syntax_childs(&block->childs, ctx);

	if(block->negative)
		ret = !ret;
	return ret;
}

static int execute_syntax_content(struct syntax_content *content,
	   	struct exec_context *ctx)
{
	return execute_syntax_childs(&content->childs, ctx);
}


static int execute_syntax_childs(struct syntax_root *root,
	   	struct exec_context *ctx)
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
				ret = execute_syntax_func(func, ctx);
				break;
			case SYNTAX_NODE_TYPE_EXPR:	
				expr = container_of(node, struct syntax_expr, node);
				ret = execute_syntax_expr(expr, ctx);
				break;
			case SYNTAX_NODE_TYPE_BLOCK:	
				block = container_of(node, struct syntax_block, node);
				ret = execute_syntax_block(block, ctx);
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

static int execute_syntax_tree(struct syntax_root *root, 
		struct exec_context *ctx)
{
	struct syntax_content *content;

	if(!root)
		return -EINVAL;

	content = container_of(root, struct syntax_content, childs);

	return execute_syntax_content(content, ctx);
}


int interp_execute(struct interp_context *ctx)
{
	int ret;
	struct exec_context exec_ctx;
	struct lre_exec_detail *detail;

	memset(&exec_ctx, 0, sizeof(exec_ctx));
	exec_ctx.index = 0;
	detail = &exec_ctx.detail;

	lre_exec_detail_init(detail, 256);

	logd("Start execute rules.");
	ret = execute_syntax_tree(ctx->root, &exec_ctx);
	if(vaild_lre_results(ret)) {
		ctx->results = ret;
		ctx->errcode = LRE_RET_OK;
	} else {
		loge("Execute error, unexpect result:%d", ret);
		ctx->results = LRE_RESULT_UNKNOWN;
		ctx->errcode = ret;
	}

	if(detail->len > 0)
		ctx->details = strdup(detail->detail);

	lre_exec_detail_release(detail);
	return 0;
}

