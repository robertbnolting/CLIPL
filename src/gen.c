#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "parse.h"
#include "error.h"

static FILE *outputfp;
static int entrypoint_defined;

static void emit_func_prologue();
static void emit_block();
static void emit_expr();

#define emit(...)		emitf("\t"  __VA_ARGS__)
#define emit_noindent(...)	emitf(__VA_ARGS__)

void set_output_file(FILE *fp)
{
	outputfp = fp;
}

static char *make_label() {
	static int c = 0;
	char *fmt = malloc(10);
	sprintf(fmt, "L%d", c++);
	return fmt;
}

static void emitf(char *fmt, ...) {
	// from https://github.com/rui314/8cc/blob/master/gen.c
	char buf[256];
	int i = 0;
	for (char *c = fmt; *c; c++) {
		buf[i++] = *c;
	}

	buf[i] = '\0';

	va_list args;

	va_start(args, fmt);
	vfprintf(outputfp, buf, args);
	va_end(args);

	fprintf(outputfp, "\n");
}

void gen(Node **funcs, size_t n_funcs)
{
	entrypoint_defined = 0;
	for (int i = 0; i < n_funcs; i++) {
		emit_func_prologue(funcs[i]);
		emit_block(funcs[i]->fnbody, funcs[i]->n_stmts);
	}

	if (!entrypoint_defined) {
		c_error("No entrypoint was specified. Use keyword 'entry' in front of function to mark it as the entrypoint.", -1);
	}
}

void push(char *reg)
{
	emit("push %s", reg);
}

void pop(char *reg)
{
	emit("pop %s", reg);
}

static void emit_func_prologue(Node *func)
{
	emit_noindent("\nsection .text");
	if (func->is_fn_entrypoint) {
		entrypoint_defined = 1;
		emit_noindent("global _start");
		emit_noindent("_start:");
	}
	emit_noindent("global %s", func->flabel);
	emit_noindent("%s:", func->flabel);
	push("rbp");
	emit("mov rbp, rsp");
}

static void emit_block(Node **block, size_t sz)
{
	for (int i = 0; i < sz; i++) {
		emit_expr(block[i]);
	}
}

static void emit_literal(Node *expr)
{
	switch (expr->type)
	{
		case AST_INT:
			emit("mov rax, %u", expr->ival);
			break;
		case AST_BOOL:
			emit("mov rax, %u", expr->bval);
			break;
		case AST_STRING:
			if (!expr->slabel) {
				expr->slabel = make_label();
				emit_noindent("section .data");
				emit("%s db %s", expr->slabel, expr->sval);
				emit_noindent("section .text");
			}
			break;
		default:
			printf("Internal error.");
	}
}

static void emit_binop(Node *expr)
{
	char *op = NULL;
	switch (expr->type)
	{
		case AST_ADD: op = "add"; break;
		case AST_SUB: op = "sub"; break;
	}

	emit_expr(expr->left);
	push("rax");
	emit_expr(expr->right);
	emit("mov rcx, rax");
	pop("rax");

	emit("%s rax, rcx", op);
}

static void emit_rep()
{
	emit("leave");
	emit("ret");
}

static void emit_expr(Node *expr)
{
	switch (expr->type)
	{
		case AST_INT:
		case AST_STRING:
		case AST_FLOAT:
		case AST_BOOL:
		case AST_ARRAY:
			emit_literal(expr);
			break;
		case AST_RETURN:
			emit_expr(expr->retval);
			emit_ret();
		default:
			emit_binop(expr);
			break;
	}
}
