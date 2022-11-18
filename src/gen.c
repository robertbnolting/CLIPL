#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "parse.h"
#include "error.h"

static char *P_REGS[] = {"rdi", "rsi", "rdx", "rcx"};
static char *S_REGS[] = {"r8", "r9", "r10", "r11", "r12", "r13"};

static int vregs_idx;

static int s_regs_count;

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

static void optimize();

#define emit(...)		emitf("\t"  __VA_ARGS__)
#define emit_noindent(...)	emitf(__VA_ARGS__)

char *outputbuf;
size_t outputbuf_sz;

void set_output_file(FILE *fp)
{
	outputfp = fp;
	outputbuf = NULL;
	outputbuf_sz = 0;
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

	char tmpbuf[128];

	va_start(args, fmt);
	vsprintf(&tmpbuf[0], buf, args);
	va_end(args);
	strcat(&tmpbuf[0], "\n");

	outputbuf_sz += strlen(tmpbuf);
	outputbuf = realloc(outputbuf, outputbuf_sz);
	strcat(outputbuf, &tmpbuf[0]);
}

void gen(Node **funcs, size_t n_funcs)
{
	entrypoint_defined = 0;
	s_regs_count = 0;
	for (int i = 0; i < n_funcs; i++) {
		emit_func_prologue(funcs[i]);
	}

	if (!entrypoint_defined) {
		c_error("No entrypoint was specified. Use keyword 'entry' in front of function to mark it as the entrypoint.", -1);
	}

	//optimize();

	fprintf(outputfp, outputbuf);
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
	emit_noindent("\n\tmov rax, %d", code);
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
			push(P_REGS[ireg++]);
			stack_offset += 8; 	// adjust depending on type
		} else {
			printf("Not implemented.\n");
		}
		n->lvar_valproppair->loff = stack_offset;
	}
}

static void emit_func_prologue(Node *func)
{
	emit_noindent("section .text");
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
	vregs_idx = 0;

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
			if (n->lvar_valproppair->loff == 0) {
				stack_offset += 8;	// adjust depending on type
				n->lvar_valproppair->loff = stack_offset;
			}
			emit("mov [rbp+%d], v%d", n->lvar_valproppair->loff, vregs_idx++);
			break;
	}
}

static void emit_assign(Node *n)
{
	emit_expr(n->right);	// gets saved in rax
	emit_store(n->left); 	// rax gets stored at stack offset of left
}

static char *getArrayElems(Node *expr, int member_type)
{
	char *ret = NULL;
	size_t len = 0;

	if (expr->array_dims > 1) {
		char *acc = NULL;
		size_t acc_len = 0;
		for (int i = 0; i < expr->array_size; i++) {
			char *sub = getArrayElems(expr->array_elems[i], member_type);
			acc_len += strlen(sub);
			acc = realloc(acc, acc_len+1);
			strcat(acc, sub);
		}

		len += strlen(acc);
		ret = realloc(ret, len);
		strcat(ret, acc);
	} else {
		for (int i = 0; i < expr->array_size; i++) {
			switch (member_type)
			{
			case TYPE_INT:
			{
				char num[32];
				int n = sprintf(num, "%d, ", expr->array_elems[i]->ival);
				len += n;
				ret = realloc(ret, len+1);
				strcat(ret, num);
			}
			break;
			case TYPE_STRING:
			{
				char *str = malloc(strlen(expr->array_elems[i]->sval)) + 5;
				int n = sprintf(str, "%s, ", expr->array_elems[i]->sval);

				len += n;
				ret = realloc(ret, len);
				strcat(ret, str);

			}
			break;
			}
		}
	}

	return ret;
}

static void emit_literal(Node *expr)
{
	switch (expr->type)
	{
		case AST_INT:
			emit("mov v%d, %u", vregs_idx, expr->ival);
			break;
		case AST_BOOL:
			emit("mov v%d, %u", vregs_idx, expr->bval);
			break;
		case AST_STRING:
			if (!expr->slabel) {
				expr->slabel = make_label();
				emit_noindent("\nsection .data");
				emit("%s db %s", expr->slabel, expr->sval);
				emit_noindent("\nsection .text");
			}
			emit("mov v%d, %s", vregs_idx++, expr->slabel);
			emit("mov v%d, %ld", vregs_idx, expr->slen);
			break;
		case AST_ARRAY:
			if (!expr->alabel) {
				expr->alabel = make_label();
				emit_noindent("\nsection .data");
				emit("%s db %s", expr->alabel, getArrayElems(expr, expr->array_member_type));
				emit_noindent("\nsection .text");
			}
			emit("mov v%d, %s", vregs_idx, expr->alabel);
			break;
		default:
			printf("Internal error.");
	}
}

static void emit_idx_array(Node *n)
{
	ValPropPair *ref_array = n->lvar_valproppair;
	int array_offset = 0;
	for (int i = 0; i < n->ndim_index; i++) {
		int sizeacc = 1;
		for (int j = i+1; j < ref_array->array_dims; j++) {
			sizeacc *= ref_array->array_size[j];
		}
		array_offset += n->index_values[i]->ival * sizeacc;
	}

	if (ref_array->array_dims == n->ndim_index) {
		emit("mov rax, [%s+%d]", ref_array->array_elems->alabel, array_offset);
	} else {
		emit("mov rax, %s+%d", ref_array->array_elems->alabel, array_offset);
	}
}

static void emit_load(int offset, char *base)
{
	emit("mov v%d, [%s+%d]", vregs_idx, base, offset);
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

static void emit_int_arith_binop(Node *expr)
{
	char *op = NULL;
	switch (expr->type)
	{
		case AST_ADD: op = "add"; break;
		case AST_SUB: op = "sub"; break;
	}

	emit_expr(expr->left);
	emit("mov v%d, v%d", vregs_idx+1, vregs_idx);
	vregs_idx += 2;
	emit_expr(expr->right);
	emit("mov v%d, v%d", vregs_idx+1, vregs_idx);

	emit("%s v%d, v%d", op, vregs_idx-1, vregs_idx+1);
	vregs_idx -= 1;
}

static void emit_string_arith_binop(Node *expr)
{
	emit_expr(expr->left);
	push("rax");
	emit_expr(expr->right);
	emit("mov rcx, rax");
	emit("pop rax");

	char *label = make_label();
	emit("mov rsi, 0");
	emit_noindent("%s:", label);
	emit("mov dil, [rcx+rsi]");
	emit("mov rdx, rax");
	emit("add rdx, v%d", vregs_idx);
	emit("add rdx, rsi");
	emit("mov [rdx], dil");
	emit("inc rsi");
	emit("cmp rsi, v%d", vregs_idx+1);
	emit("jne %s", label);

	emit("add v%d, v%d", vregs_idx, vregs_idx+1);
	vregs_idx++;
}

static void emit_comp_binop(Node *expr)
{
	emit_expr(expr->left);
	push("rax");
	emit_expr(expr->right);
	emit("mov rcx, rax");
	pop("rax");

	emit("cmp rax, rcx");
}

static void emit_binop(Node *expr)
{
	switch (expr->type)
	{
	case AST_ADD:
	case AST_SUB:
		if (expr->result_type == TYPE_INT) {
			emit_int_arith_binop(expr);
		}
		if (expr->result_type == TYPE_STRING) {
			emit_string_arith_binop(expr);
		}
		break;
	case AST_GT:
	case AST_LT:
	case AST_EQ:
	case AST_NE:
	case AST_GE:
	case AST_LE:
		emit_comp_binop(expr);
		break;
	default:
		printf("Not implemented.\n");
		exit(1);
	}
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
		case AST_IDX_ARRAY:
			emit_idx_array(expr);
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

static int *def(int *live, size_t live_sz, char *var)
{
	int idx = atoi(var);

	for (int i = 0; i < live_sz; i++) {
		if (live[i] == idx) {
			memmove(&live[i], &live[i+1], sizeof(int*) * (live_sz - i));
			break;
		}
	}

	return live;
}

static int *ref(int *live, size_t live_sz, char *var)
{
	int idx = atoi(var);
	
	live = realloc(live, live_sz+1);
	live[live_sz] = idx;
}

static void clear(char *a)
{
	while (*a != 0) {
		*a = 0;
		a++;
	}
}

static void lva()
{
	int *live = NULL;
	size_t live_sz = 0;

	char mnem[16] = {0};
	char c;
	int mnem_len = 0;
	for (int i = outputbuf_sz-1; i >= 0; i--) {
		c = outputbuf[i];
		if (c == ',') {
			if (mnem[0] == 'v') {
				live = def(live, live_sz, &mnem[1]);
				live_sz--;
			}
			clear(&mnem[0]);
			mnem_len = 0;
		} else if (c == '\n') {
			if (mnem[0] == 'v') {
				live = ref(live, live_sz, &mnem[1]);
				live_sz++;
			}
			clear(&mnem[0]);
			mnem_len = 0;
		} else if (c == ' ') {
			clear(&mnem[0]);
			mnem_len = 0;
		} else {
			mnem[mnem_len++] = c;
		}
	}
}

static void optimize()
{
	lva();
}
