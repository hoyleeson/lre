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
	struct keyword_stub *estub;
	struct lrc_stub_expr *stubexpr;
	struct lre_value arg;
	lrc_obj_t *handle;

	handle = exec_ctx_get_handle(ctx);

	estub = expr->keystub;
	stubexpr = (struct lrc_stub_expr *)estub->data;

	logv("Execute expr '%s'.", estub->keyword);
	arg.type = LRE_ARG_TYPE_UNKNOWN;
	ret = execute_syntax_val(&expr->valchilds, ctx, &arg);
	if(ret) {
		loge("Execute err: expr '%s' to lre_value err.", estub->keyword);
		return RET_DEAD_VAL;
	}

	ret = stubexpr->handler(handle, expr->opt, &arg);
	if(!vaild_lre_results(ret)) {
		loge("Execute err: failed to call '%s' expr handler.", estub->keyword);
		return RET_DEAD_VAL;
	}
	logi("Execute expr '%s' result: %d.", estub->keyword, ret);

	return ret;
}

static int execute_syntax_func(struct syntax_func *func, 
		struct exec_context *ctx)
{
	int ret;
	struct keyword_stub *fstub;
	struct lrc_stub_func *stubfunc;
	lrc_obj_t *handle;

	fstub = func->keystub;
	stubfunc = (struct lrc_stub_func *)fstub->data;

	logd("Execute func '%s'.", fstub->keyword);
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

			arg.type = LRE_ARG_TYPE_UNKNOWN;
			ret = execute_syntax_val(&args->args[i].valchilds, ctx, &arg);
			if(ret) {
				loge("Execute err: func arg '%s' to lre_value err.", astub->keyword);
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

	ret = execute_syntax_childs(&func->body.childs, ctx);

out:
	if(stubfunc->destructor)
		stubfunc->destructor(handle);

	exec_ctx_pop_handle(ctx);
	return ret;
}

static int execute_syntax_call(struct syntax_call *call, 
		struct exec_context *ctx, struct lre_value *arg)
{
	int ret;
	struct keyword_stub *cstub;
	struct lrc_stub_call *stubcall;
	lrc_obj_t *handle;

	cstub = call->keystub;

	logd("Execute call '%s'.", cstub->keyword);
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

			arg.type = LRE_ARG_TYPE_UNKNOWN;
			ret = execute_syntax_val(&args->args[i].valchilds, ctx, &arg);
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

	if(detail->len > 0) {
		ctx->details = strdup(detail->detail);
		assert_ptr(ctx->details);
	}

	lre_exec_detail_release(detail);
	return 0;
}

