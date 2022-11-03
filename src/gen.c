#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "parse.h"
#include "error.h"

static char *REGS[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

static int stack_offset;

static FILE *outputfp;
static int entrypoint_defined;

static void emit_func_prologue();
static void emit_block();
static void emit_expr();
static void emit_literal();
static void emit_declaration();
static void emit_assign();
static void emit_store();

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

// TODO: variable args for syscall parameters
static void emit_syscall(int code /*,...*/)
{
	emit("mov rax, %d", code);
	emit("mov rdi, 0");

	emit("syscall");
}

static void push_func_params(Node **params, size_t nparams)
{
	int ireg = 0;
	for (int i = 0; i < nparams; i++) {
		Node *n = params[i];
		// int, string, bool, array
		if (n->vtype == 1 || (n->vtype <= 5 && n->vtype >= 3)) {
			// TODO: what if regs are full
			push(REGS[ireg++]);
			stack_offset += 8; 	// adjust depending on type
		} else {
			printf("Not implemented.\n");
		}
		n->lvar_valproppair->loff = stack_offset;
	}
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
	stack_offset = 0;

	push_func_params(func->fnparams, func->n_params);
	
	emit_block(func->fnbody, func->n_stmts);

	if (func->is_fn_entrypoint) {
		emit_syscall(60);
	}
}

static void emit_block(Node **block, size_t sz)
{
	for (int i = 0; i < sz; i++) {
		emit_expr(block[i]);
	}
}

static void emit_store(Node *n)
{
	switch (n->type)
	{
		case AST_DECLARATION:
		case AST_IDENT:
			emit("mov [rbp+%d], rax", n->lvar_valproppair->loff);
			break;
	}
}

static void emit_assign(Node *n)
{
	emit_expr(n->right);	// gets saved in rax
	emit_store(n->left); 	// rax gets stored at stack offset of left
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

static void emit_load(int offset, char *base)
{
	emit("mov rax, [%s+%d]", base, offset);
}

static void emit_lvar(Node *n)
{
	emit_load(n->lvar_valproppair->loff, "rbp");
}

static void emit_declaration(Node *n)
{
	stack_offset += 8;	// adjust depending on type
	n->lvar_valproppair->loff = stack_offset;
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

static void emit_ret()
{
	emit("leave");
	emit("ret");
}

static void emit_expr(Node *expr)
{
	switch (expr->type)
	{
		case AST_IDENT:
			emit_lvar(expr);
			break;
		case AST_INT:
		case AST_STRING:
		case AST_FLOAT:
		case AST_BOOL:
		case AST_ARRAY:
			emit_literal(expr);
			break;
		case AST_RETURN_STMT:
			emit_expr(expr->retval);
			emit_ret();
			break;
		case AST_DECLARATION:
			emit_declaration(expr);
			break;
		case AST_ASSIGN:
			emit_assign(expr);
			break;
		default:
			emit_binop(expr);
			break;
	}
}
