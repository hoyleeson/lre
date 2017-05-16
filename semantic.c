#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interpreter.h"

static struct keyword_stub *semantic_find_keystub(keystub_vec_t *kwvec, 
		const char *keyword)
{
	if(!kwvec)
		return NULL;

	return find_stub_by_keyword(kwvec, keyword);
}


static int semanitic_parse_childs(struct syntax_root *root, 
		keystub_vec_t *kwvec);
static int semanitic_parse_call(struct syntax_call *call, 
		keystub_vec_t *kwvec);

static int semanitic_parse_val(struct syntax_root *root,
		keystub_vec_t *kwvec)
{
	int ret;
	struct syntax_node *node;
	struct syntax_val *exprval;
	struct syntax_valblock *exprvb;
	struct syntax_call *exprvcall;
	struct keyword_stub *keystub;

	list_for_each_entry(node, &root->head, entry) {
		switch(node->type) {
			case SYNTAX_NODE_TYPE_VAL:	
				exprval = container_of(node, struct syntax_val, node);
				if(exprval->isvar) {
					keystub = semantic_find_keystub(kwvec, exprval->val);
					if(!keystub) {
						loge("Semantic: not found exprvar keystub '%s'", exprval->val);
						return -EINVAL;
					}
					exprval->valstub = keystub;
				}
				break;
			case SYNTAX_NODE_TYPE_VALBLOCK:	
				exprvb = container_of(node, struct syntax_valblock, node);
				ret = semanitic_parse_val(&exprvb->childs, kwvec);
				if(ret) {
					loge("Semantic: failed to parse expr val.");
					return ret;
				}
				break;
			case SYNTAX_NODE_TYPE_CALL:
				exprvcall = container_of(node, struct syntax_call, node);
				ret = semanitic_parse_call(exprvcall, kwvec);
				if(ret) {
					loge("Semantic: failed to parse expr val.");
					return ret;
				}
				break;
			default:
				break;
		}
	}

	return 0;
}

static int semanitic_parse_expr(struct syntax_expr *expr,
		keystub_vec_t *kwvec)
{
	int ret;
	struct keyword_stub *keystub;

	keystub = semantic_find_keystub(kwvec, expr->key);
	if(!keystub) {
		loge("Semantic: not found expr keystub '%s'", expr->key);
		return -EINVAL;
	}
	expr->keystub = keystub;
	ret = semanitic_parse_val(&expr->valchilds, kwvec);

	logd("Semantic: parse expr '%s' %s", 
			keystub->keyword, ret ? "failed" : "success");
	return ret;
}

static int semanitic_parse_func(struct syntax_func *func, 
		keystub_vec_t *kwvec)
{
	int ret;
	struct keyword_stub *funcstub;
	keystub_vec_t *subvec;

	funcstub = semantic_find_keystub(kwvec, func->keyword);
	if(!funcstub) {
		loge("Semantic: not found funcstub '%s'", func->keyword);
		return -EINVAL;
	}
	func->keystub = funcstub;
	subvec = funcstub->subvec;

	if(func->args.count > 0) {
		int i;
		struct keyword_stub *argstub;
		struct syntax_args *args = &func->args;

		for(i=0; i<args->count; i++) {
			argstub = semantic_find_keystub(subvec, args->args[i].key);
			if(!argstub) {
				loge("Semantic: not found argstub '%s'", args->args[i].key);
				return -EINVAL;
			}
			args->args[i].keystub = argstub;

			ret = semanitic_parse_val(&args->args[i].valchilds, kwvec);
			if(ret) {
				loge("Semantic: parse func args[%d] val failed.", i);
				return ret;
			}
		}
	}

	ret = semanitic_parse_childs(&func->body.childs, subvec);

	logd("Semantic: parse func '%s' %s", 
			funcstub->keyword, ret ? "failed" : "success");
	return ret;
}

static int semanitic_parse_call(struct syntax_call *call, 
		keystub_vec_t *kwvec)
{
	int ret = 0;
	struct keyword_stub *callstub;
	keystub_vec_t *subvec;

	callstub = semantic_find_keystub(kwvec, call->keyword);
	if(!callstub) {
		/* global method call */
		kwvec = get_root_keyvec();
		callstub = semantic_find_keystub(kwvec, call->keyword);
		if(!callstub) {
			loge("Semantic: not found keystub '%s'", call->keyword);
			return -EINVAL;
		}
	}
	call->keystub = callstub;
	subvec = callstub->subvec;

	if(call->args.count > 0) {
		int i;
		struct keyword_stub *argstub;
		struct syntax_args *args = &call->args;

		for(i=0; i<args->count; i++) {
			argstub = semantic_find_keystub(subvec, args->args[i].key);
			if(!argstub) {
				loge("Semantic: not found argstub '%s'", args->args[i].key);
				return -EINVAL;
			}
			args->args[i].keystub = argstub;

			ret = semanitic_parse_val(&args->args[i].valchilds, kwvec);
			if(ret) {
				loge("Semantic: parse call args[%d] '%s' val failed.",
					   	i, argstub->keyword);
				return ret;
			}
		}
	}

	logd("Semantic: parse call '%s' success", callstub->keyword);
	return 0;
}

static int semanitic_parse_block(struct syntax_block *block,
		keystub_vec_t *kwvec)
{
	return semanitic_parse_childs(&block->childs, kwvec);
}

static int semanitic_parse_content(struct syntax_content *content,
		keystub_vec_t *kwvec)
{
	return semanitic_parse_childs(&content->childs, kwvec);
}

static int semanitic_parse_childs(struct syntax_root *root, 
		keystub_vec_t *kwvec)
{
	int ret = 0;
	struct syntax_func *func;
	struct syntax_block *block;
	struct syntax_expr *expr;
	struct syntax_node *node;

	list_for_each_entry(node, &root->head, entry) {
		switch(node->type) {
			case SYNTAX_NODE_TYPE_FUNC:	
				func = container_of(node, struct syntax_func, node);
				ret = semanitic_parse_func(func, kwvec);
				if(ret) {
					loge("Semantic: failed to parse func.");
					return ret;
				}
				break;
			case SYNTAX_NODE_TYPE_EXPR:	
				expr = container_of(node, struct syntax_expr, node);
				ret = semanitic_parse_expr(expr, kwvec);
				if(ret) {
					loge("Semantic: failed to parse expr.");
					return ret;
				}
				break;
			case SYNTAX_NODE_TYPE_BLOCK:	
				block = container_of(node, struct syntax_block, node);
				ret = semanitic_parse_block(block, kwvec);
				if(ret) {
					loge("Semantic: failed to parse block.");
					return ret;
				}
				break;
			default:
				break;
		}
	}
	return ret;
}

static int semanitic_tree_parse(struct syntax_root *root, keystub_vec_t *kwvec)
{
	struct syntax_content *content;

	if(!root)
		return -EINVAL;

	content = container_of(root, struct syntax_content, childs);

	return semanitic_parse_content(content, kwvec);
}

int interp_semantic_analysis(struct interp_context *ctx)
{
	int ret;
	keystub_vec_t *kwvec;

	logd("Start analysis semantic.");
	kwvec = get_root_keyvec();
	ret = semanitic_tree_parse(ctx->root, kwvec);
	if(ret) {
		loge("Semanitic err: semantic analysis error.");
	}

	return ret;
}

