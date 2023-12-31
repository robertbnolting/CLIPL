#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "parse.h"
#include "gen.h"
#include "error.h"

#define MAX_REGISTER_COUNT 14

static char *Q_REGS[] = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi",
		       	 "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"};

static char *D_REGS[] = {"eax", "ebx", "ecx", "edx", "esi", "edi",
		       	 "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d"};

static char *W_REGS[] = {"ax", "bx", "cx", "dx", "si", "di",
		       	 "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w"};

static char *B_REGS[] = {"al", "bl", "cl", "dl", "sil", "dil",
		       	 "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b"};

static int vregs_idx;
static int vregs_count;

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
static void emit_store_offset();
static void emit_lvar();
static void emit_if();
static void emit_while();
static void emit_for();
static int emit_array_assign();
static int emit_offset_assign();
static void emit_int_arith_binop();
static size_t *emit_string_assign();
static size_t emit_string_arith_binop();
static void emit_func_call();
static void emit_syscall();

static int do_array_arithmetic();

static int **getArrayMembers();
static int *getArraySizes();

static InterferenceNode **lva();
static void sortByColor();
static void gen_nasm();

#define emit(...)		emitf("\t"  __VA_ARGS__)
#define emit_noindent(...)	emitf(__VA_ARGS__)

char *outputbuf;
size_t outputbuf_sz;

MnemNode **ins_array = NULL;
size_t ins_array_sz = 0;

void set_output_file(FILE *fp)
{
	outputfp = fp;
	outputbuf = NULL;
	outputbuf_sz = 0;
}

static char *makeLabel(int loop) {
	static int c = 0;
	char *fmt = malloc(10);
	if (loop) {
		sprintf(fmt, "Loop%d", c++);
	} else {
		sprintf(fmt, "L%d", c++);
	}
	return fmt;
}

static void clear(char *a)
{
	while (*a != 0) {
		*a = 0;
		a++;
	}
}

static void getStringLens(Node *n, size_t *pair)
{
	switch (n->type)
	{
		case AST_FUNCTION_CALL:
	 		getStringLens(global_functions[n->global_function_idx]->return_stmt->retval, pair);
			break;
		case AST_IDENT:
			pair[0] = n->lvar_valproppair->slen;
			pair[1] = n->lvar_valproppair->s_allocated;
			break;
		case AST_STRING:
			pair[0] = n->slen;
			pair[1] = n->s_allocated;
			break;
		case AST_ADD:
		{
			getStringLens(n->left, pair);
			int saved_pair[2];
			saved_pair[0] = pair[0];
			saved_pair[1] = pair[1];

			getStringLens(n->right, pair);
			pair[0] += saved_pair[0];
			pair[1] += saved_pair[1];
		}
			break;
		default:
			c_error("Not implemented.", -1);
			break;
	}
}

static int *getReturnArraySize(Node *n)
{
	switch (n->type)
	{
		case AST_IDENT:
			return n->lvar_valproppair->array_size;
		case AST_ARRAY:
			return getArraySizes(n, n->array_dims);
		case AST_FUNCTION_CALL:
			return getReturnArraySize(global_functions[n->global_function_idx]->return_stmt->retval);
		default:
			c_error("Not implemented.", -1);
			break;
	}
}

#define MATCHES(x) (!strcmp(mnem+off, x))
static int is_instruction_mnemonic(char *mnem)
{
	int off = 0;
	if (mnem[0] == '\t') {
		off = 1;
	}

	if (MATCHES("mov")) {
		return MOV;
	} else if (MATCHES("lea")) {
		return LEA;
	} else if (MATCHES("add")) {
		return ADD;
	} else if (MATCHES("sub")) {
		return SUB; 
	} else if (MATCHES("imul")) {
		return IMUL;
	} else if (MATCHES("div")) {
		return DIV;
	} else if (MATCHES("and")) {
		return AND;
	} else if (MATCHES("shr")) {
		return SHR;
	} else if (MATCHES("shl")) {
		return SHL;
	} else if (MATCHES("not")) {
		return NOT;
	} else if (MATCHES("or")) {
		return OR;
	} else if (MATCHES("neg")) {
		return NEG;
	} else if (MATCHES("cmp")) {
		return CMP;
	} else if (MATCHES("inc")) {
		return INC;
	} else if (MATCHES("dec")) {
		return DEC;
	} else if (MATCHES("je")) {
		return JE;
	} else if (MATCHES("jne")) {
		return JNE;
	} else if (MATCHES("jl")) {
		return JL;
	} else if (MATCHES("jle")) {
		return JLE;
	} else if (MATCHES("jg")) {
		return JG;
	} else if (MATCHES("jge")) {
		return JGE;
	} else if (MATCHES("jmp")) {
		return JMP;
	} else if (MATCHES("goto")) {
		return GOTO;
	} else if (MATCHES("call")) {
		return CALL;
	} else if (MATCHES("push")) {
		return PUSH;
	} else if (MATCHES("pop")) {
		return POP;
	} else if (MATCHES("ret")) {
		return RET;
	}

	return 0;
}

static int is_assignment_mnemonic(char *mnem)
{
	int off = 0;
	if (mnem[0] == '\t') {
		off = 1;
	}

	if (MATCHES("mov")) {
		return MOV;
	} else if (MATCHES("lea")) {
		return LEA;
	}

	return 0;
}
static int is_unary_mnemonic(char *mnem)
{
	int off = 0;
	if (mnem[0] == '\t') {
		off = 1;
	}

 	if (MATCHES("inc")) {
		return INC;
	} else if (MATCHES("dec")) {
		return DEC;
	} else if (MATCHES("div")) {
		return DIV;
	} else if (MATCHES("neg")) {
		return NEG;
	} else if (MATCHES("not")) {
		return NOT;
	} else if (MATCHES("je")) {
		return JE;
	} else if (MATCHES("jne")) {
		return JNE;
	} else if (MATCHES("jl")) {
		return JL;
	} else if (MATCHES("jle")) {
		return JLE;
	} else if (MATCHES("jg")) {
		return JG;
	} else if (MATCHES("jge")) {
		return JGE;
	} else if (MATCHES("jmp")) {
		return JMP;
	} else if (MATCHES("goto")) {
		return GOTO;
	} else if (MATCHES("call")) {
		return CALL;
	} else if (MATCHES("push")) {
		return PUSH;
	} else if (MATCHES("pop")) {
		return POP;
	}

	return 0;
}
#undef MATCHES

static int realRegToIdx(char *mnem, char *mode)
{
	for (int i = 0; i < MAX_REGISTER_COUNT; i++) {
		if (!strcmp(mnem, Q_REGS[i])) {
			*mode = 'q';
			return i;
		}
		if (!strcmp(mnem, D_REGS[i])) {
			*mode = 'd';
			return i;
		}
		if (!strcmp(mnem, W_REGS[i])) {
			*mode = 'w';
			return i;
		}
		if (!strcmp(mnem, B_REGS[i])) {
			*mode = 'b';
			return i;
		}
	}

	return -1;
}

int current_func;
MnemNode *makeMnemNode(char *mnem)
{
	static int in_loop = 0;

	if (mnem[0] == '\0') {
		return NULL;
	}

	MnemNode *r = malloc(sizeof(MnemNode));

	r->left = NULL;
	r->right = NULL;

	r->left_spec = NULL;
	r->right_spec = NULL;

	r->first_def = 0;

	r->vregs_used = NULL;
	r->n_vregs_used = 0;

	r->is_function_label = -1;

	if (mnem[0] == '\n') {
		r->type = NEWLINE;
		r->mnem = "\n";
		return r;
	}

	int off = 0;
	if (mnem[0] == '\t') {
		off = 1;
	}

	int type = is_instruction_mnemonic(mnem);
	char mode = 0;
	int idx = -1;
	int ret_belongs_to = -1;

	if (!type) {
		char *mnem_p = mnem;
		char buf[10] = {0};
		size_t buf_sz = 0;

		if (mnem_p[off] == '[') {
			type = BRACKET_EXPR;
			while (*(++mnem_p) != ']') {
				if (*mnem_p != 'v' && !(*mnem_p >= '0' && *mnem_p <= '9') && !(*mnem_p == 'd' || *mnem_p == 'w' || *mnem_p == 'b')) {
					if (buf_sz) {
						MnemNode *n = makeMnemNode(buf);
						r->vregs_used = realloc(r->vregs_used, sizeof(MnemNode*) * (r->n_vregs_used+1));
						r->vregs_used[r->n_vregs_used++] = n;
						clear(buf);
						buf_sz = 0;
					}
				} else {
					if (buf_sz) {
						if (buf[0] == 'v') {
							buf[buf_sz++] = *mnem_p;
						}
					} else if (*mnem_p == 'v') {
						buf[buf_sz++] = *mnem_p;
					}
				}
			}
			if (buf_sz) {
				MnemNode *n = makeMnemNode(buf);
				r->vregs_used = realloc(r->vregs_used, sizeof(MnemNode*) * (r->n_vregs_used+1));
				r->vregs_used[r->n_vregs_used++] = n;
				clear(buf);
				buf_sz = 0;
			}
		} else if (mnem_p[off] == 'v') {
			type = VIRTUAL_REG;
		} else if (mnem_p[strlen(mnem)-1] == ':') {
			type = LABEL;
			if (!strncmp(mnem_p, "Loop", 4))
				in_loop++;

			char *func_name = malloc(strlen(mnem));
			strcpy(func_name, mnem_p);

			func_name[strlen(func_name)-1] = '\0';

			int func_idx = find_function(&func_name[3]);
			if (func_idx >= 0) {
				current_func = func_idx;
				r->is_function_label = func_idx;
			}

			free(func_name);
		} else if (mnem_p[off] >= '0' && mnem_p[off] <= '9') {
			type = LITERAL;
		} else if (mnem_p[off] == '\'' || mnem_p[off] == '"') {
			type = LITERAL;
		} else if (!strcmp(mnem_p+1, "word")) {
			type = SPECIFIER;
		} else if (!strcmp(mnem_p, "byte")) {
			type = SPECIFIER;
		} else if (!strcmp(&mnem_p[off], "syscall")) {
			type = SYSCALL;
			r->in_loop = in_loop;
		} else if (!strncmp(mnem_p, "Loop", 4)) {
			in_loop--;
		} else {
			idx = realRegToIdx(&mnem_p[off], &mode);
			if (idx >= 0) {
				type = REAL_REG;
			}
		}
	}

	if (type == RET) {
		ret_belongs_to = current_func;
	}

	if (type == VIRTUAL_REG) {
		switch (mnem[1])
		{
			case 'd':
			case 'w':
			case 'b':
				idx = atoi(mnem+2);
				mode = mnem[1];
				break;
			default:
				idx = atoi(mnem+1);
				mode = 'q';
				break;
		}
	}

	r->ret_belongs_to = ret_belongs_to;
	r->idx = idx;
	r->mode = mode;

	if (type != LABEL) {
		r->mnem = malloc(strlen(mnem) + 1);
		strcpy(r->mnem, mnem);
	} else {
		r->mnem = malloc(strlen(mnem));
		mnem[strlen(mnem) - 1] = '\0';
		strcpy(r->mnem, mnem);
	}

	r->type = type;

	return r;
}

static void emitf(char *fmt, ...) {
	// track vregs first definitions
	static int *vregs = NULL;
	static size_t vregs_sz = 0;

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

	char mnem[50] = {0};
	size_t mnem_len = 0;
	char c;

	MnemNode *ins = NULL;
	int prev_ins = 0;

	int counter = 0;
	// pseudo-asm parser
	for (;;) {
		c = tmpbuf[counter++];
		if (c == ' ' || c == '\0' || c == '\n') {
			if (c == '\n') {
				ins_array = realloc(ins_array, (ins_array_sz+1) * sizeof(MnemNode *));
				ins_array[ins_array_sz++] = makeMnemNode("\n");
				break;
			}
			if (is_instruction_mnemonic(mnem)) {
				ins = makeMnemNode(&mnem[0]);
				prev_ins = 1;
				clear(mnem);
				mnem_len = 0;
			} else if (ins != NULL && prev_ins) {
				MnemNode *next;
				if (ins->left == NULL) {
					next = makeMnemNode(mnem);
					if (next->type != SPECIFIER) {
						ins->left = next;
					} else {
						ins->left_spec = next;
					}

					if (ins->type == CALL) {
						for (int i = 0; i < global_function_count; i++) {
							if (!strcmp(global_functions[i]->flabel, &ins->left->mnem[3])) {
								ins->call_to = i;
								break;
							}
						}
					}

					if (is_unary_mnemonic(ins->mnem) && ins->left) {
						ins_array = realloc(ins_array, (ins_array_sz+1) * sizeof(MnemNode *));
						ins_array[ins_array_sz++] = ins;
						ins = NULL;
						break;
					} else if (is_assignment_mnemonic(ins->mnem) && ins->left) {
						if (ins->left->type == VIRTUAL_REG) {
							int i;
							for (i = 0; i < vregs_sz; i++) {
								if (vregs[i] == ins->left->idx) {
									break;
								}
							}
							if (i == vregs_sz) {
								ins->left->first_def = 1;
								vregs = realloc(vregs, sizeof(int) * (vregs_sz+1));
								vregs[vregs_sz++] = ins->left->idx;
							}
						}
					}
				} else if (ins->right == NULL) {
					next = makeMnemNode(mnem);
					if (next->type != SPECIFIER) {
						ins->right = next;
					} else {
						ins->right_spec = next;
					}

					if (ins->right) {
						ins_array = realloc(ins_array, (ins_array_sz+1) * sizeof(MnemNode *));
						ins_array[ins_array_sz++] = ins; 
						ins = NULL;
						break;
					}
				}

				clear(mnem);
				mnem_len = 0;
			} else {
				if (c == '\0') {
					MnemNode *other = makeMnemNode(&mnem[0]);
					if (other) {
						ins_array = realloc(ins_array, (ins_array_sz+1) * sizeof(MnemNode *));
						ins_array[ins_array_sz++] = other;
						prev_ins = 0;
						clear(mnem);
						mnem_len = 0;
						break;
					}
				} else {
					mnem[mnem_len++] = c;
				}
			}
		} else {
			mnem[mnem_len++] = c;
		}
	}
}

void gen(Node **funcs, size_t n_funcs)
{
	vregs_idx = MAX_REGISTER_COUNT; // 0 - MAX_REGISTER_COUNT for real regs

	entrypoint_defined = 0;

	for (int i = 0; i < n_funcs; i++) {
		emit_func_prologue(funcs[i]);
	}

	if (!entrypoint_defined) {
		c_error("No entrypoint was specified. Use keyword 'entry' in front of function to mark it as the entrypoint.", -1);
	}

	if (!ps_out) {
		gen_nasm();
	} else if (live_out) {
		d_warning("Collision between -dps and -dlive. Liveness information won't be shown.");
	}

	for (int i = 0; i < ins_array_sz; i++) {
		char tmpbuf[128] = {0};
		if (ins_array[i]->type >= MOV && ins_array[i]->type <= POP) {
			strcpy(tmpbuf, ins_array[i]->mnem);
			strcat(tmpbuf, " ");
			if (ins_array[i]->left_spec) {
				strcat(tmpbuf, ins_array[i]->left_spec->mnem);
				strcat(tmpbuf, " ");
			}
			strcat(tmpbuf, ins_array[i]->left->mnem);
			if (ins_array[i]->type < INC) {
				strcat(tmpbuf, ", ");
				if (ins_array[i]->right_spec) {
					strcat(tmpbuf, ins_array[i]->right_spec->mnem);
					strcat(tmpbuf, " ");
				}

				strcat(tmpbuf, ins_array[i]->right->mnem);
			}
			strcat(tmpbuf, "\n");
		} else if (ins_array[i]->type == LABEL) {
			strcpy(tmpbuf, ins_array[i]->mnem);
			strcat(tmpbuf, ":\n");
		} else if (ins_array[i]->type == NEWLINE) {
			strcpy(tmpbuf, "\n");
		} else {
			strcpy(tmpbuf, ins_array[i]->mnem);
			strcat(tmpbuf, "\n");
		}

		outputbuf_sz += strlen(tmpbuf);
		outputbuf = realloc(outputbuf, outputbuf_sz+1);
		if (i == 0) {
			strcpy(outputbuf, &tmpbuf[0]);
		} else {
			strcat(outputbuf, &tmpbuf[0]);
		}
		outputbuf[outputbuf_sz] = '\0';
	}

	fprintf(outputfp, outputbuf);
	fclose(outputfp);
}

static void push(char *reg)
{
	emit("push %s", reg);
}

static void pop(char *reg)
{
	emit("pop %s", reg);
}

static void emit_syscall(Node **args, size_t n_args)
{
	if (n_args < 2) {
		c_error("Invalid syscall expression. Correct usage is: syscall(NR, arg0, ...)", -1);
	} else if (n_args > 6) {
		c_error("Too many arguments for syscall.", -1);
	}

	char *regs[] = {"rdi", "rsi", "rdx", "r10", "r8", "r9"};

	int func_returns[n_args-1];

	for (int i = 0; i < n_args; i++) {
		if (args[i]->type == AST_FUNCTION_CALL) {
			emit_expr(args[i]);
			if (global_functions[args[i]->global_function_idx]->return_type == TYPE_STRING) {
				func_returns[i] = vregs_idx-2;
			} else {
				func_returns[i] = vregs_idx++;
			}
		}
	}

	for (int i = 0; i < n_args; i++) {
		if (i == 0) {
			emit_expr(args[i]);
			emit("mov rax v%d", vregs_idx++);
		} else {
			if (args[i]->type == AST_FUNCTION_CALL) {
				emit("mov %s v%d", regs[i-1], func_returns[i]);
			} else {
				emit_expr(args[i]);
				if (args[i]->type == AST_IDENT) {
					if (args[i]->lvar_valproppair->type == AST_STRING) {
						emit("mov %s v%d", regs[i-1], vregs_idx-2);
					} else {
						emit("mov %s v%d", regs[i-1], vregs_idx++);
					}
				} else if (args[i]->type == AST_STRING) {
					emit("mov %s v%d", regs[i-1], vregs_idx-2);
				} else {
					emit("mov %s v%d", regs[i-1], vregs_idx++);
				}
			}
		}
	}

	emit("syscall");
	emit("\n");

	emit("mov v%d rax", vregs_idx);
}

static void emit_func_prologue(Node *func)
{
	if (func->is_fn_entrypoint) {
		entrypoint_defined = 1;
		emit_noindent("section .text");
		emit_noindent("global _start");
		emit_noindent("_start:");
	} else if (!func->is_called) {
		return;
	} else {
		emit_noindent("section .text");
	}

	emit_noindent("global fn_%s", func->flabel);
	emit_noindent("fn_%s:", func->flabel);
	push("rbp");
	emit("mov rbp rsp");
	emit("\n");

	int end_prologue = ins_array_sz;

	stack_offset = 0;
	int param_offset = 2;	// to account for pushed return address/pushed rbp

	for (int i = func->n_params-1; i >= 0; i--) {
		switch (func->fnparams[i]->lvar_valproppair->type)
		{
		case TYPE_STRING:
			stack_offset += 16;

			emit("mov v%d [rbp+%d]", vregs_idx, 8 * param_offset++);
			emit("mov v%d [v%d]", vregs_idx, vregs_idx);
			emit("mov [rsp+%d] vd%d", stack_offset, vregs_idx++);
			emit("mov v%d [rbp+%d]", vregs_idx, 8 * param_offset++);
			emit("mov [rsp+%d] v%d", stack_offset-8, vregs_idx++);

			func->fnparams[i]->lvar_valproppair->loff = stack_offset;

			break;
		case TYPE_INT:
		case TYPE_BOOL:
			stack_offset += 8;

			emit("mov vd%d [rbp+%d]", vregs_idx, 8 * param_offset++);
			emit("mov [rsp+%d] vd%d", stack_offset, vregs_idx++);

			func->fnparams[i]->lvar_valproppair->loff = stack_offset;

			break;
		case TYPE_ARRAY:
		{
			stack_offset += 8;

			emit("mov v%d [rbp+%d]", vregs_idx, 8 * param_offset++);
			emit("mov [rsp+%d] v%d", stack_offset, vregs_idx++);

			func->fnparams[i]->lvar_valproppair->loff = stack_offset;
		}
			break;
		default:
			c_error("Not implemented.", -1);
		}
	}

	emit("\n");
	emit_block(func->fnbody, func->n_stmts);

	ins_array = realloc(ins_array, sizeof(MnemNode*) * (ins_array_sz+1));
	memmove(&ins_array[end_prologue+1], &ins_array[end_prologue], sizeof(MnemNode*) * (ins_array_sz-end_prologue));

	ins_array_sz++;

	stack_offset += 8;

	char *numVarsStr = malloc(10);
	sprintf(numVarsStr, "%d", stack_offset);

	MnemNode *sub = makeMnemNode("\tsub");
	sub->left = makeMnemNode("rsp");
	sub->right = makeMnemNode(numVarsStr);

	ins_array[end_prologue] = sub;

	if (func->return_type == TYPE_VOID) {
		emit("mov rax 0");
	}

	emit("\n");
	emit_noindent("ret_%d:", current_func);
	emit("add rsp %d", stack_offset);

	func->end_body = ins_array_sz;
	func->start_body = end_prologue - 1;

	emit("pop rbp");
	emit("\n");

	if (func->is_fn_entrypoint) {
		emit("mov rax 60");
		emit("mov rdi 0");
		emit("syscall");
	} else {
		emit("ret");
	}
}

static void emit_block(Node **block, size_t sz)
{
	for (int i = 0; i < sz; i++) {
		emit_expr(block[i]);
	}
}

static void emit_store_offset(int offset, int type)
{
	switch (type)
	{
		case TYPE_INT:
			emit("mov [rsp+%d] vd%d", offset, vregs_idx++);
			break;
		case TYPE_STRING:
			emit("mov [rsp+%d] v%d", offset-8, vregs_idx-2);
			emit("mov v%d [v%d]", vregs_idx-1, vregs_idx-1);
			emit("mov [rsp+%d] v%d", offset, vregs_idx-1);
			emit("\n");
			break;
		default:
			printf("Type not implemented.\n");
	}
}

static void emit_store(Node *n)
{
	switch (n->type)
	{
		int off;
		case AST_DECLARATION:
			switch (n->lvar_valproppair->type)
			{
				case TYPE_INT:
				default:
					stack_offset += 8;
					n->lvar_valproppair->loff = stack_offset;

					emit_store_offset(stack_offset, TYPE_INT);
					break;
				case TYPE_STRING:
					stack_offset += 16;
					n->lvar_valproppair->loff = stack_offset;

					emit_store_offset(stack_offset, TYPE_STRING);
					break;
			}
			break;
		case AST_IDENT:
			off = n->lvar_valproppair->loff;
			switch (n->lvar_valproppair->type)
			{
				case TYPE_INT:
				default:
					emit_store_offset(off, TYPE_INT);
					break;
				case TYPE_STRING:
					emit_store_offset(off, TYPE_STRING);
					break;
			}
			break;
		case AST_IDX_ARRAY:
		{
			ValPropPair *ref_array = n->lvar_valproppair->ref_array;
			int to_store = vregs_idx++;

			int offset_reg = vregs_idx++;
			emit("mov vd%d 0", offset_reg);
			for (int i = 0; i < n->ndim_index; i++) {
				int sizeacc = 1;
				for (int j = i+1; j < ref_array->array_dims; j++) {
					sizeacc *= ref_array->array_size[j];
				}

				emit_expr(n->index_values[i]);

				emit("lea vd%d [vd%d*%d]", vregs_idx, vregs_idx, sizeacc);
				emit("add vd%d vd%d", offset_reg, vregs_idx);
			}

			emit("lea vd%d [vd%d*8]", offset_reg, offset_reg);
			emit("sub rsp v%d", offset_reg);
			emit("mov [rsp+%d] v%d", ref_array->loff, to_store);
			emit("add rsp v%d", offset_reg);

			vregs_idx++;
		}
			break;
		default:
			c_error("Not implemented.", -1);
			break;
	}
}

static void emit_assign(Node *n)
{
	if (n->left->lvar_valproppair->type == TYPE_ARRAY) {
		emit_expr(n->left);
		emit_array_assign(n->left, n->right);
	} else if (n->left->lvar_valproppair->type == TYPE_STRING) {
		emit_string_assign(n->left, n->right);
	} else {
		emit_expr(n->right);
		emit_store(n->left);
	}
}

static int emit_array_assign(Node *var, Node *array)
{
#define pair (var->lvar_valproppair)
	//emit_expr(var);

	if (array->type == AST_ARRAY || array->type == AST_IDENT || array->type == AST_IDX_ARRAY || array->type == AST_FUNCTION_CALL) {
		if (var->type == AST_IDX_ARRAY) {
			return emit_offset_assign(pair->ref_array->array_dims, pair->ref_array->array_size, &pair->ref_array->array_len, &pair->ref_array->array_elems, pair->loff, array);
		} else {
			return emit_offset_assign(pair->array_dims, pair->array_size, &pair->array_len, &pair->array_elems, pair->loff, array);
		}
	} else {
		return do_array_arithmetic(array, var);
	}
#undef pair
}

static int emit_offset_assign(int array_dims, int *array_size, size_t *array_len, Node ***array_elems, int loff, Node *array)
{
	if (array->type == AST_IDX_ARRAY) {
		ValPropPair *ref_array = array->lvar_valproppair->ref_array;
		Node *indexed_array = ref_array->array_elems[array->index_values[0]->ival];

		for (int i = 1; i < array->ndim_index; i++) {
			indexed_array = indexed_array->array_elems[array->index_values[i]->ival];
		}

		return emit_offset_assign(array_dims, array_size, array_len, array_elems, loff, indexed_array);
	} else if (array->type == AST_FUNCTION_CALL) {
		emit_func_call(array);
		int arr_reg = vregs_idx++;
		for (int i = 0; i < array_size[0]+1; i++) {
			emit("mov v%d [v%d-%d]", vregs_idx, arr_reg, i*8);
			emit("mov qword [rsp+%d] v%d", loff-(i*8), vregs_idx++);
		}
	} else {
		int total_size = 1;
		for (int i = 0; i < array_dims; i++) {
			total_size *= array_size[i];
		}

		size_t member_sz = 0;
		int iter = 0;

		int **members = getArrayMembers(array, &member_sz, total_size, array_size[array_dims-1], &iter, total_size * 8);

		if (member_sz > total_size) {
			c_error("Invalid array assignment: Not enough space in array.", -1);
		}

		int counter = 1;
		int acc = 1;

		for (int j = 1; j < array_dims; j++) {
			acc *= array_size[j];
		}

		for (int i = 0; i < member_sz; i++) {
			emit("mov qword [rsp+%d] %d", members[i][1]+(loff-members[0][1]), members[i][0]);

			if (counter < acc) {
				counter++;
			} else {
				(*array_len)++;
				counter = 1;
			}
		}

		emit("mov qword [rsp+%d] %d", loff-(total_size*8), *array_len);

		int toplevel_len = member_sz / acc;

		*array_elems = realloc(*array_elems, sizeof(Node *) * *array_len);
		if (array->type == AST_ARRAY) {
			if (array_dims != array->array_dims) {
				memcpy(&((*array_elems)[*array_len - toplevel_len]), &array, sizeof(Node *) * toplevel_len);
			} else {
				memcpy(&((*array_elems)[*array_len - toplevel_len]), array->array_elems, sizeof(Node *) * toplevel_len);
			}
		} else if (array->type == AST_IDENT) {
#define pair (array->lvar_valproppair)
			if (pair->type == AST_ARRAY) {
				memcpy(&((*array_elems)[*array_len - toplevel_len]), pair->array_elems, sizeof(Node *) * toplevel_len);
			} else if (pair->type == AST_INT) {
				(*array_elems)[*array_len - toplevel_len] = makeNode(&(Node){AST_INT, .ival=pair->ival});
			} else {
				c_error("Not implemented.", -1);
			}
#undef pair
		} else {
			memcpy(&((*array_elems)[*array_len - 1]), &array, sizeof(Node *));
		}

		return member_sz;
	}
}

static int **getArrayMembers(Node *array, size_t *n_members, int total_size, int last_size, int *n_iter, size_t offset)
{
	int **members = NULL;

	size_t array_size;
	Node **array_elems;
	int array_dims;

	if (array->type == AST_ARRAY) {
		array_size = array->array_size;
		array_elems = array->array_elems;
		array_dims = array->array_dims;
	} else if (array->type == AST_IDENT) {
#define pair (array->lvar_valproppair)
		if (pair->type == AST_ARRAY) {
			array_size = pair->array_len;
			array_elems = pair->array_elems;
			array_dims = pair->array_dims;
		} else if (pair->type == AST_INT) {
			array_size = 1;
			array_elems = malloc(sizeof(Node *));
			array_elems[0] = makeNode(&(Node){AST_INT, .ival=pair->ival});
			array_dims = 1;
		} else {
			c_error("Not implemented.", -1);
		}
#undef pair
	} else if (array->type == AST_INT) {
		array_size = 1;
		array_elems = malloc(sizeof(Node*));
		array_elems[0] = array;
	} else {
		c_error("Not implemented.", -1);
	}

	if (array_dims == 1) {
		(*n_iter)++;
	}

	for (int i = 0; i < array_size; i++) {
		if (array_dims > 1) {
				size_t ret_sz = 0;
				int **ret = getArrayMembers(array_elems[i], &ret_sz, total_size, last_size, n_iter, 8 * (total_size - ((*n_iter) * last_size)));

				if (ret) {
					members = realloc(members, (*n_members+ret_sz) * sizeof(int*));
					memcpy(&members[*n_members], ret, ret_sz * sizeof(int*));
					*n_members += ret_sz;
					free(ret);
				}
		} else {
			switch (array_elems[i]->type) 
			{
				case AST_INT:
				{
					members = realloc(members, (*n_members+1) * sizeof(int*));
					members[*n_members] = malloc(sizeof(int[2]));
					int pair[2] = {array_elems[i]->ival, offset};
					memcpy(members[(*n_members)++], &pair[0], 2 * sizeof(int));
					offset -= 8;
					break;
				}
				case AST_IDENT:
				{
					members = realloc(members, (*n_members+1) * sizeof(int*));
					members[*n_members] = malloc(sizeof(int[2]));
					int pair[2] = {array_elems[i]->lvar_valproppair->ival, offset};
					memcpy(members[(*n_members)++], &pair[0], 2 * sizeof(int));
					offset -= 8;
					break;
				}
				case AST_FUNCTION_CALL:
				{
					members = realloc(members, (*n_members+1) * sizeof(int*));
					members[*n_members] = malloc(sizeof(int[2]));
					int pair[2] = {global_functions[array_elems[i]->global_function_idx]->return_stmt->retval->ival, offset};
					memcpy(members[(*n_members)++], &pair[0], 2 * sizeof(int));
					offset -= 8;
					break;
				}
				default:
					c_error("Not implemented.", -1);
			}
		}
	}

	return members;
}

static int do_array_arithmetic(Node *expr, Node *var)
{
	int ret;
	switch (expr->type)
	{
#define pair (var->lvar_valproppair)
	case AST_ADD:
		if (expr->right->type == AST_ARRAY) {
			int assign_off = var->lvar_valproppair->loff;
			size_t n = do_array_arithmetic(expr->left, var);

			assign_off -= n * 8;


			int total_size = 1;
			for (int i = 0; i < pair->array_dims; i++) {
				total_size *= pair->array_size[i];
			}

			ret = n + emit_offset_assign(pair->array_dims, pair->array_size, &pair->array_len, &pair->array_elems, assign_off, expr->right);
			if (ret > total_size) {
				c_error("Invalid array assignment: Not enough space in array.", -1);
			}
		} else {
			emit("\n");
			emit("mov vd%d [rsp+%d]", vregs_idx, pair->loff - (pair->array_size[0] * 8));
			emit("lea vd%d [vd%d*8]", vregs_idx, vregs_idx);

			int offset_reg = vregs_idx++;

			if (expr->right->type == AST_INT) {
				emit("mov v%d %d", vregs_idx, expr->right->ival);
			} else {
				emit_expr(expr->right);
			}

			emit("\n");

			emit("sub rsp v%d", offset_reg);

			emit_store_offset(pair->loff, TYPE_INT);

			emit("add rsp v%d", offset_reg);


			pair->array_elems[pair->array_len++] = expr->right;

			emit("inc qword [rsp+%d]", pair->loff - (pair->array_size[0] * 8));

			ret = pair->array_len;
		}

		return ret;

#undef pair
	case AST_ARRAY:
		emit_expr(var);
		return emit_array_assign(var, expr);
	default:
		printf("Not implemented.\n");
		return 0;
	}
}

static int *getArraySizes(Node *array, int dims)
{
	int *sizes = malloc(dims * sizeof(int));

	if (dims == 0) {
		return NULL;
	} else {
		sizes[0] = array->array_size; }
	
	int *next_sizes = getArraySizes(array->array_elems[0], dims-1);

	if (next_sizes) {
		memcpy(&sizes[1], next_sizes, (dims-1) * sizeof(int));
	}

	return sizes;
}

static void emit_literal(Node *expr)
{
	switch (expr->type)
	{
		case AST_INT:
			emit("mov vd%d %u", vregs_idx, expr->ival);
			break;
		case AST_BOOL:
			emit("mov vd%d %u", vregs_idx, expr->bval);
			break;
		case AST_STRING:
			if (!expr->slabel) {
				expr->slabel = makeLabel(0);
				emit("\n");
				emit_noindent("section .data");
				emit("%s db %s, 0", expr->slabel, expr->sval);
				emit("\n");
				emit_noindent("section .text");
			}
			emit("mov v%d %s", vregs_idx++, expr->slabel);
			stack_offset += 8;
			emit("mov qword [rsp+%d] %d", stack_offset, expr->slen);
			emit("lea v%d [rsp+%d]", vregs_idx++, stack_offset);
			break;
		case AST_ARRAY:
		{
			Node *var = malloc(sizeof(Node));
			int *array_size = getArraySizes(expr, expr->array_dims);

			int total_size = 1;
			for (int i = 0; i < expr->array_dims; i++) {
				total_size *= array_size[i];
			}

			var->type = AST_IDENT;
			var->lvar_valproppair = makeValPropPair(&(ValPropPair)
				{"", 1, TYPE_ARRAY, .array_type=expr->array_member_type, .array_dims=expr->array_dims, .array_size=array_size});

			stack_offset += (total_size+1) * 8;
			var->lvar_valproppair->loff = stack_offset;

			emit_array_assign(var, expr);

			emit("lea v%d [rsp+%d]", vregs_idx, stack_offset);
		}
			break;
		default:
			printf("Internal error.");
	}
}

static void emit_array_arg(Node *n)
{
	switch (n->type)
	{
	case AST_ARRAY:
	{
		Node *var = malloc(sizeof(Node));
		int *array_size = getArraySizes(n, n->array_dims);

		int total_size = 1;
		for (int i = 0; i < n->array_dims; i++) {
			total_size *= array_size[i];
		}

		var->type = AST_IDENT;
		var->lvar_valproppair = makeValPropPair(&(ValPropPair)
			{"", 1, TYPE_ARRAY, .array_type=n->array_member_type, .array_dims=n->array_dims, .array_size=array_size});

		stack_offset += (total_size+1) * 8;
		var->lvar_valproppair->loff = stack_offset;

		emit_array_assign(var, n);

		emit("lea v%d [rsp+%d]", vregs_idx, stack_offset);
	}
		break;
	case AST_IDENT:
		emit("lea v%d [rsp+%d]", vregs_idx, n->lvar_valproppair->loff);
		break;
	case AST_FUNCTION_CALL:
		emit_func_call(n);
		break;
	default:
		c_error("Not implemented.", -1);
	}
}

static void emit_idx_array(Node *n)
{
	ValPropPair *ref_array = n->lvar_valproppair->ref_array;

	int offset_reg = vregs_idx++;
	emit("mov vd%d 0", offset_reg);
	for (int i = 0; i < n->ndim_index; i++) {
		int sizeacc = 1;
		for (int j = i+1; j < ref_array->array_dims; j++) {
			sizeacc *= ref_array->array_size[j];
		}

		emit_expr(n->index_values[i]);

		emit("lea vd%d [vd%d*%d]", vregs_idx, vregs_idx, sizeacc);
		emit("add vd%d vd%d", offset_reg, vregs_idx);
	}

	emit("lea vd%d [vd%d*8]", offset_reg, offset_reg);
	emit("sub rsp v%d", offset_reg);
	emit("mov vd%d [rsp+%d]", vregs_idx, ref_array->loff);
	emit("add rsp v%d", offset_reg);
}

static void emit_load(int offset, char *base, int type)
{
	switch (type)
	{
		case TYPE_STRING:
			emit("mov v%d [%s+%d]", vregs_idx++, base, offset-8);
			emit("lea v%d [%s+%d]", vregs_idx++, base, offset);
			break;
		case TYPE_INT:
		default:
			emit("mov vd%d [%s+%d]", vregs_idx, base, offset);
			break;
	}
}

static void emit_lvar(Node *n)
{
	switch (n->lvar_valproppair->type)
	{
		case AST_ARRAY:
			emit("lea v%d [rsp+%d]", vregs_idx, n->lvar_valproppair->loff);
			break;
		case AST_INT:
		case AST_STRING:
			emit_load(n->lvar_valproppair->loff, "rsp", n->lvar_valproppair->type);
			break;
		default:
			c_error("Not implemented.", -1);
	}
}

static void emit_declaration(Node *n)
{
	switch (n->lvar_valproppair->type)
	{
		case TYPE_STRING:
			stack_offset += 16;
			n->lvar_valproppair->loff = stack_offset;

			break;
		case TYPE_ARRAY:
		{
			if (!n->lvar_valproppair->loff) {
				int acc = 1;
				int i;
				for (i = 0; i < n->v_array_dimensions; i++) {
					if (n->varray_size[i] < 0) {
						break;
					}
					acc *= n->varray_size[i];
				}
				if (i != n->v_array_dimensions) {
					c_error("Not implemented.", -1);
				} else {
					stack_offset += 8 * (acc+1);	// +1 for array_len
					n->lvar_valproppair->loff = stack_offset;
				}
			}
		}
			break;
		case TYPE_INT:
		default:
			stack_offset += 8;
			n->lvar_valproppair->loff = stack_offset;

			break;
	}
}

static void emit_int_arith_binop(Node *expr)
{
	int left_idx;
	int right_idx;

	switch (expr->type)
	{
		case AST_ADD:
		case AST_ADD_ASSIGN:
			emit_expr(expr->left);
			left_idx = vregs_idx++;
			emit_expr(expr->right);
			right_idx = vregs_idx;

			emit("add vd%d vd%d", right_idx, left_idx);
			break;
		case AST_SUB:
		case AST_SUB_ASSIGN:
			emit_expr(expr->left);
			left_idx = vregs_idx++;
			emit_expr(expr->right);
			right_idx = vregs_idx;

			emit("sub vd%d vd%d", left_idx, right_idx);
			emit("mov vd%d vd%d", right_idx, left_idx);
			break;
		case AST_MUL:
		case AST_MUL_ASSIGN:
			emit_expr(expr->left);
			left_idx = vregs_idx++;
			emit_expr(expr->right);
			right_idx = vregs_idx;

			emit("imul vd%d vd%d", right_idx, left_idx);
			break;
		case AST_DIV:
		case AST_DIV_ASSIGN:
			emit_expr(expr->left);
			left_idx = vregs_idx++;
			emit_expr(expr->right);
			right_idx = vregs_idx;

			emit("mov rdx 0");
			emit("mov rax v%d", left_idx);

			emit("div v%d", right_idx);

			emit("mov v%d rax", left_idx);
			emit("mov vd%d vd%d", right_idx, left_idx);
			break;
		case AST_MOD:
		case AST_MOD_ASSIGN:
			emit_expr(expr->left);
			left_idx = vregs_idx++;
			emit_expr(expr->right);
			right_idx = vregs_idx;

			emit("mov rdx 0");
			emit("mov rax v%d", left_idx);

			emit("div v%d", right_idx);

			emit("mov v%d rdx", left_idx);
			emit("mov vd%d vd%d", right_idx, left_idx);
			break;
	}
}

static size_t *emit_string_assign(Node *var, Node *string)
{
	if (string->type == AST_STRING || (string->type == AST_IDENT && string->lvar_valproppair->type == AST_STRING)
	    || string->type == AST_IDX_ARRAY 
	    || (string->type == AST_FUNCTION_CALL && global_functions[string->global_function_idx]->return_type == AST_STRING)) {
		emit_expr(string);

		size_t *pair = malloc(sizeof(size_t) * 2);
		getStringLens(string, pair);

		if (var) {
			emit_store(var);

			var->lvar_valproppair->slen = pair[0];
			var->lvar_valproppair->s_allocated = pair[1];
		}

		return pair;
	} else {
		size_t len = emit_string_arith_binop(string);

		if (var) {
			emit_store(var);
			var->lvar_valproppair->slen = len;
		}
		size_t *pair = malloc(sizeof(size_t) * 2);
		pair[0] = len;
		pair[1] = 0;
		return pair;
	}
}

static size_t emit_string_arith_binop(Node *expr)
{
	size_t *string1_pair = emit_string_assign(NULL, expr->left);
	size_t string1_len = string1_pair[0];
	int string1 = vregs_idx-2;
	int string1_end = vregs_idx-1;

	size_t *string2_pair = emit_string_assign(NULL, expr->right);
	size_t string2_len = string2_pair[0];
	int string2 = vregs_idx-2;
	int string2_end = vregs_idx-1;

	char *buf1 = makeLabel(0);
	char *buf2 = makeLabel(0);

	char *copyBuf2 = makeLabel(0);
	char *copyBuf1 = makeLabel(0);
	char *loop1 = makeLabel(0);
	char *loop2 = makeLabel(0);
	char *cont_label1 = makeLabel(0);
	char *cont_label2 = makeLabel(0);

	int idx_reg = vregs_idx++;
	int idx2_reg = vregs_idx++;
	int c_reg = vregs_idx++;
	int buf1_reg = vregs_idx++;
	int buf2_reg = vregs_idx++;

	emit_noindent("section .bss");
	emit("%s resb %d", buf1, 100/*new_len+1*/);
	emit("%s resb %d", buf2, 100/*new_len+1*/);
	emit_noindent("section .text");

	emit("mov v%d %s", buf2_reg, buf2);
	emit("mov v%d 0", idx_reg);

	emit("cmp qword [v%d] 0", string2_end);
	emit("je %s", cont_label1);

	emit_noindent("%s:", copyBuf2);
	emit("mov vb%d [v%d+v%d]", c_reg, string2, idx_reg);
	emit("mov [v%d+v%d] vb%d", buf2_reg, idx_reg, c_reg);
	emit("\n");
	emit("inc v%d", idx_reg);
	emit("cmp v%d [v%d]", idx_reg, string2_end);
	emit("jl %s", copyBuf2);
	emit("\n");

	emit_noindent("%s:", cont_label1);

	emit("mov v%d %s", buf1_reg, buf1);
	emit("mov v%d 0", idx_reg);

	emit("cmp qword [v%d] 0", string1_end);
	emit("je %s", cont_label2);
	emit_noindent("%s:", loop1);
	emit("mov vb%d [v%d+v%d]", c_reg, string1, idx_reg);
	emit("mov [v%d+v%d] vb%d", buf1_reg, idx_reg, c_reg);
	emit("\n");
	emit("inc v%d", idx_reg);
	emit("inc qword [v%d]", string2_end);
	emit("cmp v%d [v%d]", idx_reg, string1_end);
	emit("jl %s", loop1);
	emit("\n");

	emit_noindent("%s:", cont_label2);

	emit("mov v%d %s", buf2_reg, buf2);
	emit("mov v%d 0", idx2_reg);

	emit_noindent("%s:", loop2);
	emit("mov vb%d [v%d+v%d]", c_reg, buf2_reg, idx2_reg);
	emit("mov [v%d+v%d] vb%d", buf1_reg, idx_reg, c_reg);
	emit("\n");
	emit("inc v%d", idx_reg);
	emit("inc v%d", idx2_reg);
	emit("cmp v%d [v%d]", idx_reg, string2_end);
	emit("jl %s", loop2);

	emit("mov v%d 0", idx_reg);

	emit_noindent("%s:", copyBuf1);
	emit("mov vb%d [v%d+v%d]", c_reg, buf1_reg, idx_reg);
	emit("mov [v%d+v%d] vb%d", buf2_reg, idx_reg, c_reg);
	emit("\n");
	emit("inc v%d", idx_reg);
	emit("cmp v%d [v%d]", idx_reg, string2_end);
	emit("jl %s", copyBuf1);

	emit("mov v%d %s", vregs_idx++, buf2);
	emit("mov v%d v%d", vregs_idx++, string2_end);
}

static void emit_comp_binop(Node *expr)
{
	char *false_label = makeLabel(0);
	char *cont_label = makeLabel(0);

	if (expr->type == AST_BOOL) {
		if (expr->bval) {
			emit("mov vd%d 1", vregs_idx);
		} else {
			emit("mov vd%d 0", vregs_idx);
		}
	} else {
		if (expr->type == AST_IDENT) {
			emit_expr(expr);
			emit("cmp vd%d 1", vregs_idx++);
			emit("jne %s", false_label);
		} else {
			emit_expr(expr->left);
			int l_idx = vregs_idx;
			vregs_idx++;
			emit_expr(expr->right);

			emit("cmp vd%d vd%d", l_idx, vregs_idx++);

			switch (expr->type)
			{
			case AST_EQ:
				emit("jne %s", false_label);
				break;
			case AST_NE:
				emit("je %s", false_label);
				break;
			case AST_LT:
				emit("jge %s", false_label);
				break;
			case AST_LE:
				emit("jg %s", false_label);
				break;
			case AST_GT:
				emit("jle %s", false_label);
				break;
			case AST_GE:
				emit("jl %s", false_label);
				break;
			}
		}

		emit("mov vd%d 1", vregs_idx);
		emit("jmp %s", cont_label);
		emit_noindent("%s:", false_label);
		emit("mov vd%d 0", vregs_idx);
		emit_noindent("%s:", cont_label);
	}
}

static void op(Node *expr)
{
	switch (expr->type)
	{
	case AST_ADD:
	case AST_SUB:
	case AST_MUL:
	case AST_DIV:
	case AST_MOD:
		if (expr->result_type == TYPE_INT) {
			emit_int_arith_binop(expr);
		}
		if (expr->result_type == TYPE_STRING) {
			emit_string_arith_binop(expr);
		}
		break;
	case AST_ADD_ASSIGN:
	case AST_SUB_ASSIGN:
	case AST_MUL_ASSIGN:
	case AST_DIV_ASSIGN:
	case AST_MOD_ASSIGN:
		if (expr->result_type == TYPE_INT) {
			emit_int_arith_binop(expr);
		}
		if (expr->result_type == TYPE_STRING) {
			emit_string_arith_binop(expr);
		}

		emit_store(expr->left);
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

static void emit_func_call(Node *n)
{
	int idx = n->global_function_idx;

	int real_params = 0;

	if (idx < 0) {
		emit_syscall(n->callargs, n->n_args);
	} else {
#define func (global_functions[idx])
		int arg_type;
		for (int i = 0; i < n->n_args; i++) {
			switch (n->callargs[i]->type)
			{
			case AST_INT:
			case AST_STRING:
			case AST_ARRAY:
				arg_type = n->callargs[i]->type;
				break;
			case AST_IDENT:
				arg_type = n->callargs[i]->lvar_valproppair->type;
				break;
			case AST_ADD:
			case AST_SUB:
			case AST_MUL:
			case AST_DIV:
			case AST_MOD:
			case AST_GT:
			case AST_LT:
			case AST_EQ:
			case AST_NE:
			case AST_GE:
			case AST_LE:
				arg_type = n->callargs[i]->result_type;
				break;
			case AST_FUNCTION_CALL:
				if(global_functions[n->callargs[i]->global_function_idx]->ret_array_dims) {
					arg_type = AST_ARRAY;
				} else {
					arg_type = global_functions[n->callargs[i]->global_function_idx]->return_type;
				}
				break;
			}

			if (arg_type == AST_ARRAY) {
				emit_array_arg(n->callargs[i]);
			} else {
				emit_expr(n->callargs[i]);
			}

			switch (arg_type)
			{
			case AST_INT:
			case AST_ARRAY:
				emit("push v%d", vregs_idx++);
				real_params++;
				break;
			case AST_STRING:
			{
				emit("push v%d", vregs_idx-2);
				emit("push v%d", vregs_idx-1);

				size_t *pair = malloc(sizeof(size_t) * 2);
				getStringLens(n->callargs[i], pair);

				func->fnparams[i]->lvar_valproppair->slen = pair[0];
				func->fnparams[i]->lvar_valproppair->s_allocated = pair[1];

				real_params += 2;
			}
				break;
			default:
				c_error("Not implemented.", -1);
			}
		}

		emit("call fn_%s", func->flabel);

		func->called_to = realloc(func->called_to, sizeof(int)*(func->n_called_to+1));
		func->called_to[func->n_called_to++] = ins_array_sz;

		emit("add rsp %d", real_params*8);	// clean up the stack

		switch (func->return_type)
		{
			case TYPE_INT:
			case TYPE_ARRAY:
				emit("mov v%d rax", vregs_idx);
				break;
			case TYPE_STRING:
				stack_offset += 8;
				emit("mov v%d rax", vregs_idx++);
				emit("mov [rsp+%d] rbx", stack_offset, vregs_idx);
				emit("lea v%d [rsp+%d]", vregs_idx++, stack_offset);
				break;
			default:
				break;
		}
	}
#undef func
}

static void emit_if(Node *n)
{
	char *cont_label = makeLabel(0);
	char *else_label = makeLabel(0);

	emit_expr(n->if_cond);

	emit("cmp vd%d 1", vregs_idx++);
	emit("jne %s", else_label);

	emit_block(n->if_body, n->n_if_stmts);

	emit("jmp %s", cont_label);
	emit_noindent("%s:", else_label);

	emit_block(n->else_body, n->n_else_stmts);

	emit_noindent("%s:", cont_label);
}

static void emit_while(Node *n)
{
	char *cond_label = makeLabel(0);
	char *body_label = makeLabel(1);

	emit("jmp %s", cond_label);

	emit_noindent("%s:", body_label);
	emit_block(n->while_body, n->n_while_stmts);

	emit_noindent("%s:", cond_label);
	emit_comp_binop(n->while_cond);
	emit("cmp vd%d 1", vregs_idx++);
	emit("je %s", body_label);
}

static void emit_for(Node *n)
{
#define for_enum (n->for_enum)
#define for_it (n->for_iterator)

	int enum_off;

	int *sizes;

	if (for_enum->type == AST_IDENT || for_enum->type == AST_STRING) {
		if (for_enum->type == AST_IDENT) {
			if (for_enum->lvar_valproppair->type != TYPE_STRING) {
				enum_off = for_enum->lvar_valproppair->loff;
				sizes = for_enum->lvar_valproppair->array_size;

				goto cont_nostring;
			}
		}

		char *loop_label = makeLabel(1);
		int acc = vregs_idx++;

		emit_expr(for_enum);
		int string = vregs_idx-2;
		int len = vregs_idx-1;

		emit("mov v%d [v%d]", len, len);

		emit_declaration(for_it);

		emit("mov vd%d 0", acc);
		emit_noindent("%s:", loop_label);

		emit("mov vb%d [v%d+v%d]", vregs_idx, string, acc);

		stack_offset += 10;
		emit("mov [rsp+%d] vb%d", stack_offset, vregs_idx);
		emit("mov byte [rsp+%d] 0", stack_offset+1);
		emit("lea v%d [rsp+%d]", vregs_idx++, stack_offset);
		emit("mov qword [rsp+%d] 1", stack_offset+2);
		emit("lea v%d [rsp+%d]", vregs_idx++, stack_offset+2);

		emit_store_offset(for_it->lvar_valproppair->loff, for_it->vtype);

		emit_block(n->for_body, n->n_for_stmts);

		emit("inc vd%d", acc);
		emit("cmp vd%d vd%d", acc, len);
		emit("jl %s", loop_label);

		return;
	} else if (for_enum->type == AST_ARRAY) {
		sizes = getArraySizes(for_enum, for_enum->array_dims);

		int acc = 1;
		for (int i = 0; i < for_enum->array_dims; i++) {
			acc *= sizes[i];
		}

		stack_offset += 8 * (acc+1);

		enum_off = stack_offset;

		size_t array_len = 0;
		emit_offset_assign(for_enum->array_dims, sizes, &array_len, &for_enum->array_elems, stack_offset, for_enum);
	} else if (for_enum->type == AST_FUNCTION_CALL) {
		emit_func_call(for_enum);
		stack_offset += 8;
		emit("mov [rsp+%d] v%d", stack_offset, vregs_idx++);
		enum_off = stack_offset;
		sizes = getReturnArraySize(for_enum);
	}
cont_nostring:

	char *loop_label = makeLabel(1);
	int acc = vregs_idx++;
	int len = vregs_idx++;
	int idx = vregs_idx++;
	int array_address = vregs_idx++;

	emit_declaration(for_it);

	emit("mov vd%d 0", acc);
	emit("mov vd%d 0", idx);
	emit("mov vd%d %d", len, sizes[0]);

	if (for_enum->lvar_valproppair) {
		if (for_enum->lvar_valproppair->is_array_reference) {
			emit("mov v%d [rsp+%d]", array_address, enum_off);
		} else {
			emit("lea v%d [rsp+%d]", array_address, enum_off);
		}
	} else if (for_enum->type == AST_FUNCTION_CALL) {
		emit("mov v%d [rsp+%d]", array_address, enum_off);
	} else {
		emit("lea v%d [rsp+%d]", array_address, enum_off);
	}

	emit_noindent("%s:", loop_label);

	int sizeacc = 1;
	for (int i = 0; i < for_it->v_array_dimensions; i++) {
		sizeacc *= for_it->varray_size[i];
	}

	emit("lea vd%d [vd%d*8]", idx, acc);
	emit("imul vd%d %d", idx, sizeacc);

	if (!for_it->v_array_dimensions) {
		emit("sub v%d v%d", array_address, idx);
		emit("mov vd%d [v%d]", vregs_idx, array_address);
		emit("add v%d v%d", array_address, idx);
		emit_store_offset(for_it->lvar_valproppair->loff, for_it->vtype);
	} else {
		int it_sizes = 1;
		for (int i = 0; i < for_it->v_array_dimensions; i++) {
			it_sizes *= for_it->varray_size[i];
		}

		for (int i = 0; i < it_sizes; i++) {
			emit("sub rsp v%d", idx);
			emit("mov vd%d [rsp+%d]", vregs_idx, enum_off-(i*8));
			emit("add rsp v%d", idx);
			emit("mov qword [rsp+%d] vd%d", for_it->lvar_valproppair->loff-(i*8), vregs_idx);
		}
	}

	emit_block(n->for_body, n->n_for_stmts);

	emit("inc vd%d", acc);
	emit("cmp vd%d vd%d", acc, len);
	emit("jl %s", loop_label);

#undef for_enum
#undef for_it
}

static void emit_ret(Node *n)
{
	if (n->retval) {
		emit_expr(n->retval);

		switch (n->rettype)
		{
			case TYPE_INT:
				emit("mov rax v%d", vregs_idx++);
				break;
			case TYPE_STRING:
				emit("mov rax v%d", vregs_idx-2);
				emit("mov rbx [v%d]", vregs_idx-1);
				break;
			case TYPE_ARRAY:
				emit("mov rax v%d", vregs_idx++);
				break;
		}
	} else {
		emit("mov rax 0");
	}

	emit("jmp ret_%d", current_func);
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
			emit_ret(expr);
			break;
		case AST_DECLARATION:
			emit_declaration(expr);
			break;
		case AST_ASSIGN:
			emit_assign(expr);
			break;
		case AST_IF_STMT:
			emit_if(expr);
			break;
		case AST_WHILE_STMT:
			emit_while(expr);
			break;
		case AST_FOR_STMT:
			emit_for(expr);
			break;
		case AST_FUNCTION_CALL:
			emit_func_call(expr);
			break;
		default:
			op(expr);
			break;
	}
}

static int *addToLiveRange(int idx, int *live, size_t *live_sz)
{
	int j;
	for (j = 0; j < *live_sz; j++) {
		if (live[j] == idx) {
			break;
		}
	}
	if (j == *live_sz) {
		live = realloc(live, sizeof(int) * (*live_sz+1));
		live[(*live_sz)++] = idx;
	}

	return live;
}

static int *liverange_subtract(int *live1, int *live2, size_t *size1, size_t size2)
{
	for (int i = 0; i < size2; i++) {
		for (int j = 0; j < *size1; j++) {
			if (live2[i] == live1[j]) {
				if (*size1 > 0) {
					memmove(&live1[j], &live1[j+1], sizeof(int) * (*size1 - (j+1)));
					(*size1)--;
				}
			}
		}
	}

	return live1;
}

static int *liverange_union(int *live1, int *live2, size_t size1, size_t *size2)
{
	if (size1 == 0) {
		return live2;
	}

	for (int i = 0; i < size1; i++) {
		int j;
		for (j = 0; j < *size2; j++) {
			if (live1[i] == live2[j]) {
				break;
			}
		}
		if (j == *size2) {
			live2 = realloc(live2, ((*size2)+1) * sizeof(int));
			live2[*size2] = live1[i];
			(*size2)++;
		}
	}

	return live2;
}

size_t live_range_sz;
size_t used_vregs_n;

static InterferenceNode **lva()
{
	live_range_sz = ins_array_sz;

	int **live_range = malloc(sizeof(int*) * live_range_sz);
	size_t *live_sz_array = malloc(sizeof(size_t) * live_range_sz);

	int *used_vregs = calloc(vregs_count-MAX_REGISTER_COUNT, sizeof(int));
	used_vregs_n = 0;

	int **preserved_regs = calloc(global_function_count, sizeof(int *));
	size_t *preserved_sz = calloc(global_function_count, sizeof(size_t));

	int *syscall_list = NULL;
	size_t syscall_list_sz = 0;

	int *prev_live_del;
	size_t prev_live_del_sz = 0;

	MnemNode *n;

	InterferenceNode **interference_graph = calloc(vregs_count, sizeof(InterferenceNode));

	int current_func;
	for (int p = 0; p < 2; p++) {
		for (int i = live_range_sz-1; i >= 0; i--) {
			int *live = NULL;
			size_t live_sz = 0;

			int *live_del = NULL;
			size_t live_del_sz = 0;

			n = ins_array[i];
			if (p == 0) {
				if (n->type >= MOV && n->type <= LEA) {
					if (n->right->type == VIRTUAL_REG || n->right->type == REAL_REG) {
						live = addToLiveRange(n->right->idx, live, &live_sz);
						if (!used_vregs[n->right->idx-MAX_REGISTER_COUNT] && n->right->type == VIRTUAL_REG) {
							used_vregs[n->right->idx - MAX_REGISTER_COUNT] = 1;
							used_vregs_n++;
						}
					} else if (n->right->type == BRACKET_EXPR) {
						int saved = live_sz;
						for (int i = 0; i < n->right->n_vregs_used; i++) {
							live = addToLiveRange(n->right->vregs_used[i]->idx, live, &live_sz);
							if (saved != live_sz && !used_vregs[n->right->vregs_used[i]->idx-MAX_REGISTER_COUNT]
							&& n->right->vregs_used[i]->type == VIRTUAL_REG) {

								used_vregs[n->right->vregs_used[i]->idx - MAX_REGISTER_COUNT] = 1;
								used_vregs_n++;
							}
						}
					}

					if (n->left->type == VIRTUAL_REG || n->left->type == REAL_REG) {
						if (n->left->first_def) {
							live_del = addToLiveRange(n->left->idx, live_del, &live_del_sz);
						} else if (!n->left->first_def && n->left->type == REAL_REG) {
							live_del = addToLiveRange(n->left->idx, live_del, &live_del_sz);
						}
					} else if (n->left->type == BRACKET_EXPR) {
						for (int i = 0; i < n->left->n_vregs_used; i++) {
							if (n->left->vregs_used[i]->first_def) {
								live_del = addToLiveRange(n->left->vregs_used[i]->idx, live_del, &live_del_sz);
							}
						}
					}
				} else if (n->type > LEA && n->type <= POP) { // instruction but not mov/lea
					if (n->type <= CMP) {	// is binary operation
						if (n->right->type == VIRTUAL_REG || n->right->type == REAL_REG) {
							live = addToLiveRange(n->right->idx, live, &live_sz);
							if (!used_vregs[n->right->idx-MAX_REGISTER_COUNT] && n->right->type == VIRTUAL_REG) {
								used_vregs[n->right->idx-MAX_REGISTER_COUNT] = 1;
								used_vregs_n++;
							}
						} else if (n->right->type == BRACKET_EXPR) {
							int saved = live_sz;
							for (int i = 0; i < n->right->n_vregs_used; i++) {
								live = addToLiveRange(n->right->vregs_used[i]->idx, live, &live_sz);
								if (saved != live_sz && !used_vregs[n->right->vregs_used[i]->idx-MAX_REGISTER_COUNT]
								&& n->right->vregs_used[i]->type == VIRTUAL_REG) {

									used_vregs[n->right->vregs_used[i]->idx - MAX_REGISTER_COUNT] = 1;
									used_vregs_n++;
								}
							}
						}
					}
					if (n->left->type == VIRTUAL_REG || n->left->type == REAL_REG) {
						live = addToLiveRange(n->left->idx, live, &live_sz);
						if (!used_vregs[n->left->idx-MAX_REGISTER_COUNT] && n->left->type == VIRTUAL_REG) {
							used_vregs[n->left->idx-MAX_REGISTER_COUNT] = 1;
							used_vregs_n++;
						}
					} else if (n->left->type == BRACKET_EXPR) {
						int saved = live_sz;
						for (int i = 0; i < n->left->n_vregs_used; i++) {
							live = addToLiveRange(n->left->vregs_used[i]->idx, live, &live_sz);

							if (saved != live_sz && !used_vregs[n->left->vregs_used[i]->idx-MAX_REGISTER_COUNT]
							&& n->left->vregs_used[i]->type == VIRTUAL_REG) {

								used_vregs[n->left->vregs_used[i]->idx - MAX_REGISTER_COUNT] = 1;
								used_vregs_n++;
							}

						}
					}
					if (n->type == DIV) {
						live = addToLiveRange(0, live, &live_sz);
						live = addToLiveRange(3, live, &live_sz);
					}
					if (n->type == CALL) {
						live_del = addToLiveRange(0, live_del, &live_del_sz);
					}
				} else if (n->type == SYSCALL) {
					live = addToLiveRange(0, live, &live_sz); // rax
					live = addToLiveRange(5, live, &live_sz); // rdi
					live = addToLiveRange(4, live, &live_sz); // rsi
					live = addToLiveRange(3, live, &live_sz); // rdx
					live = addToLiveRange(8, live, &live_sz); // r10
					live = addToLiveRange(6, live, &live_sz); // r8
					live = addToLiveRange(7, live, &live_sz); // r9
					live = addToLiveRange(2, live, &live_sz); // rcx
					live = addToLiveRange(9, live, &live_sz); // r11

					syscall_list = realloc(syscall_list, sizeof(int) * (syscall_list_sz + 1));
					syscall_list[syscall_list_sz++] = i;
				} else if (n->type == RET) {
					live = addToLiveRange(0, live, &live_sz);
				}

				if (i == live_range_sz-1) {
					live_range[i] = malloc(sizeof(int) * live_sz);
					live_range[i] = memcpy(live_range[i], live, live_sz * sizeof(int));

					live_sz_array[i] = live_sz;
				} else {
					live = liverange_union(live_range[i+1], live, live_sz_array[i+1], &live_sz);
					live_range[i] = malloc(sizeof(int) * live_sz);
					live_range[i] = memcpy(live_range[i], live, live_sz * sizeof(int));

					if (prev_live_del_sz) {
						live_range[i] = liverange_subtract(live_range[i], prev_live_del, &live_sz, prev_live_del_sz);

						live_sz_array[i] = live_sz;

						free(prev_live_del);
						prev_live_del_sz = 0;
					} else {
						live_sz_array[i] = live_sz;
					}

					if (live_del_sz) {
						prev_live_del = malloc(live_del_sz * sizeof(int));
						memcpy(prev_live_del, live_del, live_del_sz * sizeof(int));
						prev_live_del_sz = live_del_sz;
					}
				}
			} else { 						// second pass
				if (n->type >= JE && n->type <= GOTO) {		// control-flow change instruction
					char *label = n->left->mnem;
					int *live_at_label = NULL;
					size_t live_at_label_sz = 0;
					int label_idx;
					for (int j = 0; j < live_range_sz; j++) {
						if (ins_array[j]->type == LABEL) {
							if (!strcmp(ins_array[j]->mnem, label)) {
								if (j < i) {
									live_at_label = live_range[j];
									live_at_label_sz = live_sz_array[j];
									label_idx = j;
								}
								break;
							}
						}
					}
					if (live_at_label != NULL) {
						for (int j = i; j >= label_idx; j--) {
							live_range[j] = liverange_union(live_at_label, live_range[j], live_at_label_sz, &live_sz_array[j]);
						}
					}
				} else if (n->type == CALL) {			// inter-procedural preserving of registers
					for (int l = 0; l < live_sz_array[i]; l++) {
						if (live_range[i][l] >= MAX_REGISTER_COUNT) {
							preserved_regs[n->call_to] = realloc(preserved_regs[n->call_to], sizeof(int) * (preserved_sz[n->call_to] + 1));
							preserved_regs[n->call_to][preserved_sz[n->call_to]++] = live_range[i][l];
							//preserved_regs[current_func] = realloc(preserved_regs[current_func], sizeof(int) * (preserved_sz[current_func] + 1));
							//preserved_regs[current_func][preserved_sz[current_func]++] = live_range[i][l];
						}
					}
				} else if (n->type == RET) {
					current_func = n->ret_belongs_to;
				}
			}

			free(live);
		}
	}

#define func (global_functions[i])
	for (int i = 0; i < global_function_count; i++) {
		if (preserved_sz[i] > 0) {
			ins_array = realloc(ins_array, (ins_array_sz + (2*preserved_sz[i])) * sizeof(MnemNode *));
			live_range = realloc(live_range, (live_range_sz + (2*preserved_sz[i])) * sizeof(MnemNode *));
			live_sz_array = realloc(live_sz_array, (live_range_sz + (2*preserved_sz[i])) * sizeof(MnemNode *));

			// make space for inserting push instructions for saving regs at start of function body
			memmove(&ins_array[func->start_body + preserved_sz[i]], &ins_array[func->start_body], sizeof(MnemNode *) * (ins_array_sz - func->start_body));
			memmove(&live_range[func->start_body + preserved_sz[i]], &live_range[func->start_body], sizeof(MnemNode *) * (live_range_sz - func->start_body));
			memmove(&live_sz_array[func->start_body + preserved_sz[i]], &live_sz_array[func->start_body], sizeof(MnemNode *) * (live_range_sz - func->start_body));
			ins_array_sz += preserved_sz[i];
			live_range_sz += preserved_sz[i];

			func->end_body += preserved_sz[i];

			// make space for inserting pop instructions for retrieving regs at end of function body
			memmove(&ins_array[func->end_body + preserved_sz[i]], &ins_array[func->end_body], sizeof(MnemNode *) * (ins_array_sz - func->end_body));
			memmove(&live_range[func->end_body + preserved_sz[i]], &live_range[func->end_body], sizeof(MnemNode *) * (live_range_sz - func->end_body));
			memmove(&live_sz_array[func->end_body + preserved_sz[i]], &live_sz_array[func->end_body], sizeof(MnemNode *) * (live_range_sz - func->end_body));
			ins_array_sz += preserved_sz[i];
			live_range_sz += preserved_sz[i];

			for (int j = 0; j < syscall_list_sz; j++) {
				if (syscall_list[j] > func->end_body) {
					syscall_list[j] += 2 * preserved_sz[i];
				} else if (syscall_list[j] > func->start_body) {
					syscall_list[j] += preserved_sz[i];
				}
			}

			for (int j = i+1; j < global_function_count; j++) {
				global_functions[j]->start_body += 2 * preserved_sz[i];
				global_functions[j]->end_body += 2 * preserved_sz[i];
			}

			MnemNode *n;
			for (int j = 0; j < preserved_sz[i]; j++) {
				char *reg = malloc(10);
				sprintf(reg, "v%d", preserved_regs[i][j]);

				n = makeMnemNode("\tpush");
				n->left = makeMnemNode(reg);

				ins_array[func->start_body+j] = n;

				n = makeMnemNode("\tpop");
				n->left = makeMnemNode(reg);
				ins_array[func->end_body + (-1 * (j - preserved_sz[i] + 1))] = n;

				free(reg);
			}
		}
	}
#undef func

	for (int i = 0; i < syscall_list_sz; i++) {
		int unused_arg_regs[9];
		size_t unused_arg_regs_sz = 0;

#define idx 		(syscall_list[i+1]+1) // last instruction before the next syscall
#define is_sc_arg(x)	( (x==0) || (x==5) || (x==4) || (x==3) || (x==8) || (x==6) || (x==7) || (x==2) || (x==9) )
		if (i == syscall_list_sz-1) {
			for (int j = 0; j < live_sz_array[0]; j++) {
				if (is_sc_arg(live_range[0][j])) {
					unused_arg_regs[unused_arg_regs_sz++] = live_range[0][j];
				}
			}
		} else {
			for (int j = 0; j < live_sz_array[idx]; j++) {
				if (is_sc_arg(live_range[idx][j])) {
					unused_arg_regs[unused_arg_regs_sz++] = live_range[idx][j];
				}
			}
		}
#undef is_sc_arg
#undef idx

		if (ins_array[syscall_list[i]]->in_loop) {
			ins_array = realloc(ins_array, (ins_array_sz + 4) * sizeof(MnemNode *));

			memmove(&ins_array[syscall_list[i] + 2], &ins_array[syscall_list[i]], sizeof(MnemNode *) * (ins_array_sz - syscall_list[i]));
			ins_array_sz += 2;

			MnemNode *rcx_push = makeMnemNode("\tpush");
			rcx_push->left = makeMnemNode("rcx");
			MnemNode *r11_push = makeMnemNode("\tpush");
			r11_push->left = makeMnemNode("r11");

			ins_array[syscall_list[i]] = rcx_push;
			ins_array[syscall_list[i]+1] = r11_push;

			for (int j = i; j < syscall_list_sz; j++) {
				syscall_list[j] += 2;
			}

			memmove(&ins_array[syscall_list[i] + 3], &ins_array[syscall_list[i] + 1], sizeof(MnemNode *) * (ins_array_sz - (syscall_list[i]+1)));
			ins_array_sz += 2;

			MnemNode *rcx_pop = makeMnemNode("\tpop");
			rcx_pop->left = makeMnemNode("rcx");
			MnemNode *r11_pop = makeMnemNode("\tpop");
			r11_pop->left = makeMnemNode("r11");


			ins_array[syscall_list[i]+1] = r11_pop;
			ins_array[syscall_list[i]+2] = rcx_pop;
		}

		for (int j = syscall_list[i]; j >= 0; j--) {
			if (i != syscall_list_sz-1 && j == syscall_list[i+1])
				break;

			live_range[j] = liverange_subtract(live_range[j], &unused_arg_regs[0], &live_sz_array[j], unused_arg_regs_sz);
		}
	}

	// create InterferenceNode for every live variable
	for (int i = 0; i < live_range_sz; i++) {
		int *live = live_range[i];
		size_t live_sz = live_sz_array[i];

		for (int j = 0; j < live_sz; j++) {
			int k;
			for (k = 0; k < vregs_count; k++) {
				if (interference_graph[k]) {
					if (interference_graph[k]->idx == live[j]) {
						break;
					}
				}
			}
			if (k == vregs_count) {
				InterferenceNode *n = malloc(sizeof(InterferenceNode));
				n->idx = live[j];
				n->neighbors = malloc(0);
				n->neighbor_count = 0;
				n->color = -1;
				n->saturation = 0;
				interference_graph[live[j]] = n;
			}
		}

#define InterNode (interference_graph[live[j]])
		for (int j = 0; j < live_sz; j++) {
			for (int k = 0; k < live_sz; k++) {
				if (live[j] != live[k]) {
					int l;
					for (l = 0; l < InterNode->neighbor_count; l++) {
						if (InterNode->neighbors[l] == interference_graph[live[k]]) {
							break;
						}
					}
					if (l == InterNode->neighbor_count) {
						InterNode->neighbors = realloc(InterNode->neighbors, (InterNode->neighbor_count+1) * sizeof(InterferenceNode*));
						InterNode->neighbors[InterNode->neighbor_count] = interference_graph[live[k]];
						InterNode->neighbor_count++;
					}
				}
			}
		}
#undef InterNode

		if (live_out) {
			printf("LIVE at %d: ", i);
			for (int j = 0; j < live_sz; j++) {
				if (live[j] >= MAX_REGISTER_COUNT) {
					printf("%d, ", live[j]);
				} else {
					printf("%s, ", Q_REGS[live[j]]);
				}
			}
			printf("\n---\n");
		}
	}

	if (live_out) {
		for (int i = 0; i < vregs_count; i++) {
			if (i >= MAX_REGISTER_COUNT) {
				printf("Neighbors of v%d:\n", i);
			} else {
				printf("Neighbors of %s:\n", Q_REGS[i]);
			}
			if (interference_graph[i]) {
				for (int j = 0; j < interference_graph[i]->neighbor_count; j++) {
					if (interference_graph[i]->neighbors[j]->idx >= MAX_REGISTER_COUNT) {
						printf("\tv%d\n", interference_graph[i]->neighbors[j]->idx);
					} else {
						printf("\t%s\n", Q_REGS[interference_graph[i]->neighbors[j]->idx]);
					}
				}
			} else {
				printf("v%d is unused.\n", i);
			}
		}
	}

	return interference_graph;
}

// Implementation of DSatur graph coloring algorithm
static void color(InterferenceNode **g)
{
	// pre-color real registers
	for (int i = 0; i < MAX_REGISTER_COUNT; i++) {
		if (g[i]) {
			g[i]->color = i;
		}
	}

	int colored_nodes = 0;
	while (colored_nodes < used_vregs_n) {
		InterferenceNode *highest_sat = NULL;
		// calculate saturation of each node
		for (int i = MAX_REGISTER_COUNT; i < vregs_count; i++) {
			if (g[i]) {
				if (g[i]->color < 0) {
					for (int j = 0; j < g[i]->neighbor_count; j++) {
						if (g[i]->neighbors[j]->color >= 0) {
							g[i]->saturation++;
						}
					}
					if (!highest_sat) {
						highest_sat = g[i];
					} else {
						if (g[i]->saturation > highest_sat->saturation) {
							highest_sat = g[i];
						} else if (g[i]->saturation == highest_sat->saturation) {
							int uncolored_neighbors_current = 0;
							for (int j = 0; j < g[i]->neighbor_count; j++) {
								if (g[i]->neighbors[j]->color < 0)
									uncolored_neighbors_current++;
							}
							int uncolored_neighbors_highest = 0;
							for (int j = 0; j < highest_sat->neighbor_count; j++) {
								if (highest_sat->neighbors[j]->color < 0)
									uncolored_neighbors_highest++;
							}

							if (uncolored_neighbors_current > uncolored_neighbors_highest) {
								highest_sat = g[i];
							}
						}
					}
				}
			}
		
		}

		if (highest_sat->neighbor_count) {
			sortByColor(highest_sat->neighbors, highest_sat->neighbor_count);
		}

		int lowest_color = -1;
		for (int i = 0; i < highest_sat->neighbor_count; i++) {
			if (highest_sat->neighbors[i]->color == lowest_color+1) {
				lowest_color = highest_sat->neighbors[i]->color;
			}
		}
		highest_sat->color = lowest_color+1;
		colored_nodes++;
	}
	if (live_out) {
		for (int i = 0; i < vregs_count; i++) {
			if (g[i]) {
				printf("v%d: %s\n", g[i]->idx, Q_REGS[g[i]->color]);
			}
		}
	}
}

static char *assign_color(char *mnem, InterferenceNode **g)
{
	if (mnem[0] != 'v')
		return NULL;

	char *ret = NULL;
	int idx;
	char mode;
	switch (mnem[1])
	{
		case 'd':
		case 'w':
		case 'b':
			idx = atoi(mnem+2);
			mode = mnem[1];
			break;
		default:
			mode = 'q';
			idx = atoi(mnem+1);
			break;
	}
	char *reg;
	for (int j = 0; j < vregs_count; j++) {
		if (g[j]) {
			if (g[j]->idx == idx) {
				if (g[j]->color < MAX_REGISTER_COUNT) {
					switch (mode)
					{
						case 'q':
							reg = Q_REGS[g[j]->color]; break;
						case 'd':
							reg = D_REGS[g[j]->color]; break;
						case 'w':
							reg = W_REGS[g[j]->color]; break;
						case 'b':
							reg = B_REGS[g[j]->color]; break;
					}
					ret = malloc(strlen(reg) + 1);
					strcpy(ret, reg);
					break;
				} else {
					c_error("Spilling not implemented.\n", -1);
				}
			}
		}
	}

	return ret;
}

static char *substitute_vreg(char *str, char *reg)
{
	size_t len = strlen(str);
	char *str_p = str;
	while (*str_p++) {
		if (*str_p == 'v') {
			char *start = str_p;
			int loop = 1;
			while (loop) {
				str_p++;
				if (!(*str_p >= '0' && *str_p <= '9')) {
					switch (*str_p)
					{
						case 'd':
						case 'w':
						case 'b':
							if (loop > 1) {
								loop = 0;
							}
							loop++;
							break;
						default:
							if (loop == 1) {
								loop = 0;
								break;
							}
							int offset = strlen(reg) - (str_p - start);
							char *ret = calloc(strlen(str) + offset, 1);
							strncpy(ret, str, start-str);
							strcat(ret, reg);
							strncat(ret, str_p, str + len - str_p);
							//memmove(str_p + offset, str_p, str + len - str_p);
							//memcpy(start, reg, strlen(reg));
							//str[strlen(str) + offset] = '\0';
							free(str);
							return ret;
					}
				} else {
					loop++;
				}
			}
		}
	}
	return str;
}

static void assign_registers(InterferenceNode **g)
{
	for (int i = 0; i < live_range_sz; i++) {
		MnemNode *n = ins_array[i];
		if (MOV <= n->type && RET >= n->type) {
			if (n->left->type == VIRTUAL_REG) {
				char *reg = assign_color(n->left->mnem, g);
				if (reg) {
					n->left->mnem = realloc(n->left->mnem, strlen(reg)+1);
					strcpy(n->left->mnem, reg);
				} else {	// instructions operating on unused vregs are discarded
					memmove(&ins_array[i], &ins_array[i+1], sizeof(MnemNode *) * (ins_array_sz - i));
					ins_array_sz--;
					live_range_sz--;
					i--;
				}
			} else if (n->left->type == BRACKET_EXPR) {
				for (int i = 0; i < n->left->n_vregs_used; i++) {
					char *reg = assign_color(n->left->vregs_used[i]->mnem, g);
					n->left->mnem = substitute_vreg(n->left->mnem, reg);
				}
			}

			if (!is_unary_mnemonic(n->mnem)){
				if (n->right->type == VIRTUAL_REG) {
					char *reg = assign_color(n->right->mnem, g);
					if (reg) {
						n->right->mnem = realloc(n->right->mnem, strlen(reg)+1);
						strcpy(n->right->mnem, reg);
					}
				} else if (n->right->type == BRACKET_EXPR) {
					for (int i = 0; i < n->right->n_vregs_used; i++) {
						char *reg = assign_color(n->right->vregs_used[i]->mnem, g);
						n->right->mnem = substitute_vreg(n->right->mnem, reg);
					}
				}
			}
		}
	}
}


// Modified mergesort from https://github.com/geohot/mergesorts/blob/master/mergesort.c
static void sortByColor(InterferenceNode **arr, size_t len)
{
	if (len == 1) { return; }
	if (len == 2) {
		if (arr[0]->color > arr[1]->color) {
			InterferenceNode *t = arr[1];
			arr[1] = arr[0];
			arr[0] = t;
		}
	}

	size_t p = len/2;
	InterferenceNode **arr1 = arr;
	InterferenceNode **arr2 = arr+p;

	sortByColor(arr1, p);
	sortByColor(arr2, len-p);

	InterferenceNode **t = malloc(sizeof(InterferenceNode *) * len);
	InterferenceNode **rt = t;
	while (1) {
		if (arr1 < arr+p && arr2 < arr+len) {
			if (arr1[0]->color <= arr2[0]->color) {
				*t = *arr1;
				arr1++;
			} else {
				*t = *arr2;
				arr2++;
			}
		} else if(arr1 < arr+p) {
			*t = *arr1;
			arr1++;
		} else if(arr2 < arr+len) {
			*t = *arr2;
			arr2++;
		} else {
			break;
		}
		t++;
	}

	memcpy(arr, rt, sizeof(InterferenceNode *) * len);
	free(rt);
}

static void gen_nasm()
{
	vregs_count = vregs_idx;

	InterferenceNode **graph = lva();
	color(graph);

	assign_registers(graph);
}
