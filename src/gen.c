#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "parse.h"
#include "gen.h"
#include "error.h"

#define PSEUDO_ASM_OUT 0

#define MAX_REGISTER_COUNT 14

static char *Q_REGS[] = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi",
		       	 "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"};

static char *D_REGS[] = {"eax", "ebx", "ecx", "edx", "esi", "edi",
		       	 "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d"};

static char *W_REGS[] = {"ax", "bx", "cx", "dx", "si", "di",
		       	 "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w"};

static char *B_REGS[] = {"al", "bl", "cl", "dl", "sil", "dil",
		       	 "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b"};

static char *P_REGS[] = {"rdi", "rsi", "rdx", "rcx"};
static char *S_REGS[] = {"r8", "r9", "r10", "r11", "r12", "r13"};

static int vregs_idx;
static int vregs_count;

static int s_regs_count;

static int stack_offset;

Node **global_functions;

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
static size_t *emit_string_assign();
static size_t emit_string_arith_binop();
static void emit_syscall();

static int do_array_arithmetic();

static int **getArrayMembers();
static int *getArraySizes();

static InterferenceNode **lva();
static void sortByColor();
static void optimize();

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

static char *makeLabel() {
	static int c = 0;
	char *fmt = malloc(10);
	sprintf(fmt, "L%d", c++);
	return fmt;
}

static void clear(char *a)
{
	while (*a != 0) {
		*a = 0;
		a++;
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

MnemNode *makeMnemNode(char *mnem)
{
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
						r->vregs_used[r->n_vregs_used++] = n; 	// TODO: memcpy
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
				r->vregs_used[r->n_vregs_used++] = n; 	// TODO: memcpy
				clear(buf);
				buf_sz = 0;
			}
		} else if (mnem_p[off] == 'v') {
			type = VIRTUAL_REG;
		} else if (mnem_p[strlen(mnem)-1] == ':') {
			type = LABEL;
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
		} else {
			idx = realRegToIdx(&mnem_p[off], &mode);
			if (idx >= 0) {
				type = REAL_REG;
			}
		}
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
	global_functions = funcs;

	entrypoint_defined = 0;
	s_regs_count = 0;

	for (int i = 0; i < n_funcs; i++) {
		emit_func_prologue(funcs[i]);
	}

	if (!entrypoint_defined) {
		c_error("No entrypoint was specified. Use keyword 'entry' in front of function to mark it as the entrypoint.", -1);
	}

#if !PSEUDO_ASM_OUT
	optimize(); 	// TODO: rename
#endif

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
		c_error("Too many arguments for linux syscall.", -1);
	}

	char *regs[] = {"rdi", "rsi", "rdx", "r10", "r8", "r9"};

	for (int i = 0; i < n_args; i++) {
		if (i == 0) {
			emit_expr(args[i]);
			emit("mov rax v%d", vregs_idx++);
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

	emit("syscall");
	emit("\n");
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
	emit("mov rbp rsp");
	emit("\n");
	stack_offset = 0;
	vregs_idx = MAX_REGISTER_COUNT; // 0 - MAX_REGISTER_COUNT for real regs, maybe additional type REAL_REG to be recognized in lva()

	int end_prologue = ins_array_sz;

	push_func_params(func->fnparams, func->n_params);

	emit("\n");
	emit_block(func->fnbody, func->n_stmts);

	ins_array = realloc(ins_array, sizeof(MnemNode*) * (ins_array_sz+1));

	memmove(&ins_array[end_prologue+1], &ins_array[end_prologue], sizeof(MnemNode*) * (ins_array_sz-end_prologue));

	ins_array_sz++;

	char *numVarsStr = malloc(10);
	sprintf(numVarsStr, "%d", stack_offset);

	MnemNode *sub = makeMnemNode("\tsub");
	sub->left = makeMnemNode("rsp");
	sub->right = makeMnemNode(numVarsStr);

	ins_array[end_prologue] = sub;

	emit("\n");
	emit("add rsp %d", stack_offset);
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
			emit("mov [rbp-%d] vd%d", offset, vregs_idx++);
			break;
		case TYPE_STRING:
			emit("mov [rbp-%d] v%d", offset-4, vregs_idx-2);
			emit("mov vd%d [v%d]", vregs_idx-1, vregs_idx-1);
			emit("mov [rbp-%d] vd%d", offset, vregs_idx-1);
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
					stack_offset += 4;
					n->lvar_valproppair->loff = stack_offset;
					emit_store_offset(stack_offset, TYPE_INT);
					break;
				case TYPE_STRING:
					stack_offset += 12;
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
		default:
			printf("Not implemented.\n");
			break;
	}
}

static void emit_assign(Node *n)
{
	if (n->left->lvar_valproppair->type == TYPE_ARRAY) {
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
	emit_expr(var);

	if (array->type == AST_ARRAY || array->type == AST_IDENT || array->type == AST_IDX_ARRAY) {
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
	} else {
		int total_size = 1;
		for (int i = 0; i < array_dims; i++) {
			total_size *= array_size[i];
		}

		size_t member_sz = 0;
		int iter = 0;

		int **members = getArrayMembers(array, &member_sz, total_size, array_size[array_dims-1], &iter, total_size * 4);

		if (member_sz > total_size) {
			c_error("Invalid array assignment: Not enough space in array.", -1);
		}

		int counter = 1;
		int acc = 1;

		for (int j = 1; j < array_dims; j++) {
			acc *= array_size[j];
		}

		for (int i = 0; i < member_sz; i++) {
			emit("mov dword [rbp-%d] %d", members[i][1]+(loff-members[0][1]), members[i][0]);

			if (counter < acc) {
				counter++;
			} else {
				(*array_len)++;
				counter = 1;
			}
		}

		int toplevel_len = member_sz / acc;

		*array_elems = realloc(*array_elems, sizeof(Node *) * *array_len);
		if (array->type == AST_ARRAY) {
			if (array_dims != array->array_dims) {
				memcpy(&((*array_elems)[*array_len - toplevel_len]), &array, sizeof(Node *) * toplevel_len);
			} else {
				memcpy(&((*array_elems)[*array_len - toplevel_len]), array->array_elems, sizeof(Node *) * toplevel_len);
			}
		} else if (array->type == AST_IDENT) {
			memcpy(&((*array_elems)[*array_len - toplevel_len]), array->lvar_valproppair->array_elems, sizeof(Node *) * toplevel_len);
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
		array_size = array->lvar_valproppair->array_len;
		array_elems = array->lvar_valproppair->array_elems;
		array_dims = array->lvar_valproppair->array_dims;
	} else {
		switch (array->type)
		{
		case AST_INT:
			array_size = 1;
			array_elems = malloc(sizeof(Node*));
			array_elems[0] = array;
			break;
		default:
			printf("Not implemented.\n");
			break;
		}
	}

	if (array_dims == 1) {
		(*n_iter)++;
	}

	for (int i = 0; i < array_size; i++) {
		if (array_dims > 1) {
				size_t ret_sz = 0;
				int **ret = getArrayMembers(array_elems[i], &ret_sz, total_size, last_size, n_iter, 4 * (total_size - ((*n_iter) * last_size)));

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
				default:
					members = realloc(members, (*n_members+1) * sizeof(int*));
					members[*n_members] = malloc(sizeof(int[2]));
					int pair[2] = {array_elems[i]->ival, offset};
					memcpy(members[(*n_members)++], &pair[0], 2 * sizeof(int));
					offset -= 4;
					break;
			}
		}
	}

	return members;
}

static int do_array_arithmetic(Node *expr, Node *var)
{
	switch (expr->type)
	{
	case AST_ADD:
		int assign_off = var->lvar_valproppair->loff;
		size_t n = do_array_arithmetic(expr->left, var);

		assign_off -= n * 4;

#define pair (var->lvar_valproppair)

		int total_size = 1;
		for (int i = 0; i < pair->array_dims; i++) {
			total_size *= pair->array_size[i];
		}

		int ret = n + emit_offset_assign(pair->array_dims, pair->array_size, &pair->array_len, &pair->array_elems, assign_off, expr->right);
		if (ret > total_size) {
			c_error("Invalid array assignment: Not enough space in array.", -1);
		}

		return ret;

#undef pair
	case AST_ARRAY:
	case AST_IDENT:
	case AST_IDX_ARRAY:
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
		sizes[0] = array->array_size;
	}
	
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
				expr->slabel = makeLabel();
				emit("\n");
				emit_noindent("section .data");
				emit("%s db %s, 0", expr->slabel, expr->sval);
				emit("\n");
				emit_noindent("section .text");
			}
			emit("mov v%d %s", vregs_idx++, expr->slabel);
			stack_offset += 4;
			emit("mov dword [rbp-%d] %d", stack_offset, expr->slen);
			emit("lea v%d [rbp-%d]", vregs_idx++, stack_offset);
			break;
		case AST_ARRAY:
			break;
		default:
			printf("Internal error.");
	}
}

static void emit_idx_array(Node *n)
{
	ValPropPair *ref_array = n->lvar_valproppair->ref_array; // TODO: check in parse.c if {AST_IDX_ARRAY}->lvar_valproppair is necessary

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

	emit("lea vd%d [vd%d*4]", offset_reg, offset_reg);
	emit("add rbp v%d", offset_reg);
	emit("mov vd%d [rbp-%d]", vregs_idx, ref_array->loff);
	emit("sub rbp v%d", offset_reg);
}

static void emit_load(int offset, char *base, int type)
{
	switch (type)
	{
		case TYPE_STRING:
			emit("mov v%d [%s-%d]", vregs_idx++, base, offset-4);
			emit("lea v%d [%s-%d]", vregs_idx++, base, offset);
			break;
		case TYPE_ARRAY:
			break;
		case TYPE_INT:
		default:
			emit("mov vd%d [%s-%d]", vregs_idx, base, offset);
			break;
	}
}

static void emit_lvar(Node *n)
{
	switch (n->type)
	{
		case AST_ARRAY:
			break;
		case AST_INT:
		case AST_STRING:
		default:
			emit_load(n->lvar_valproppair->loff, "rbp", n->lvar_valproppair->type);
			break;
	}
}

static void emit_declaration(Node *n)
{
	switch (n->lvar_valproppair->type)
	{
		case TYPE_STRING:
			stack_offset += 12;
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
				} else {
					stack_offset += 4 * acc;
					n->lvar_valproppair->loff = stack_offset;
				}
			}
		}
			break;
		case TYPE_INT:
		default:
			stack_offset += 4;
			n->lvar_valproppair->loff = stack_offset;
			break;
	}
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
	int saved_idx = vregs_idx;
	vregs_idx++;
	vregs_count++;
	emit_expr(expr->right);

	emit("%s vd%d vd%d", op, vregs_idx, saved_idx);
}

static size_t *emit_string_assign(Node *var, Node *string)
{
	if (string->type == AST_STRING || (string->type == AST_IDENT && string->lvar_valproppair->type == AST_STRING)
	    || string->type == AST_IDX_ARRAY) {
		emit_expr(string);
		if (var) {
			emit_store(var);
			if (string->type == AST_STRING) {
				var->lvar_valproppair->slen = string->slen;
				var->lvar_valproppair->s_allocated = string->s_allocated;
				size_t *pair = malloc(sizeof(size_t) * 2);
				pair[0] = string->slen;
				pair[1] = string->s_allocated;
				return pair;
			} else {
				var->lvar_valproppair->slen = string->lvar_valproppair->slen;
				var->lvar_valproppair->s_allocated = string->lvar_valproppair->s_allocated;
				size_t *pair = malloc(sizeof(size_t) * 2);
				pair[0] = string->lvar_valproppair->slen;
				pair[1] = string->lvar_valproppair->s_allocated;
				return pair;
			}
		} else {
			if (string->type == AST_STRING) {
				size_t *pair = malloc(sizeof(size_t) * 2);
				pair[0] = string->slen;
				pair[1] = string->s_allocated;
				return pair;
			} else {
				size_t *pair = malloc(sizeof(size_t) * 2);
				pair[0] = string->lvar_valproppair->slen;
				pair[1] = string->lvar_valproppair->s_allocated;
				return pair;
			}
		}
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

	size_t new_len;
	if (string1_pair[1] && string2_pair[1]) {
		new_len = string1_pair[1] + string2_pair[1];
	} else if (string1_pair[1] && !string2_pair[1]) {
		new_len = string1_pair[1] + string2_len;
	} else if (!string1_pair[1] && string2_pair[1]) {
		new_len = string1_len + string2_pair[1];
	} else {
		new_len = string1_len + string2_len;
	}

	char *new_string = makeLabel();

	emit_noindent("section .bss");
	emit("%s resb %d", new_string, new_len+1);
	emit_noindent("section .text");

	char *loop1_label = makeLabel();
	char *loop2_label = makeLabel();

	int acc = vregs_idx++;
	int single_char = vregs_idx++;
	int new_string_reg = vregs_idx++;

	emit("\n");
	emit("mov v%d 0", acc);
	emit_noindent("%s:", loop1_label);

	emit("mov vb%d [v%d+v%d]", single_char, string1, acc);
	emit("mov v%d %s", new_string_reg, new_string);
	emit("add v%d v%d", new_string_reg, acc);
	emit("mov [v%d] vb%d", new_string_reg, single_char);
	emit("inc v%d", acc);
	emit("cmp v%d %d", acc, string1_len);
	emit("jl %s", loop1_label);

	string2_len++;
	emit("\n");
	emit("mov v%d 0", acc);
	emit_noindent("%s:", loop2_label);

	emit("mov vb%d [v%d+v%d]", single_char, string2, acc);
	emit("mov v%d %s", new_string_reg, new_string);
	emit("add v%d v%d", new_string_reg, acc);

	emit("mov vd%d [v%d]", vregs_idx, string1_end);
	emit("add v%d v%d", new_string_reg, vregs_idx++);

	emit("mov [v%d] vb%d", new_string_reg, single_char);

	emit("\n");

	emit("inc v%d", acc);
	emit("cmp v%d %d", acc, string2_len);
	emit("jl %s", loop2_label);
	string2_len--;

	emit("mov v%d %s", vregs_idx++, new_string);
	emit("mov vd%d [v%d]", string2_end, string2_end);
	emit("add [v%d] v%d", string1_end, string2_end);

	emit("mov v%d v%d", vregs_idx++, string1_end);

	emit("\n");

	return new_len;
}

static void emit_comp_binop(Node *expr)
{
	char *false_label = makeLabel();
	char *cont_label = makeLabel();

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

static void emit_func_call(Node *n)
{
	int idx = n->global_function_idx;

	if (idx < 0) {
		emit_syscall(n->callargs, n->n_args);
	} else {
		emit("call %s", global_functions[idx]->flabel);
	}
}

static void emit_if(Node *n)
{
	char *cont_label = makeLabel();
	char *else_label = makeLabel();

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
	char *cond_label = makeLabel();
	char *body_label = makeLabel();

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
	int saved_stack = stack_offset;
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

		char *loop_label = makeLabel();
		int acc = vregs_idx++;

		emit_expr(for_enum);
		int string = vregs_idx-2;
		int len = vregs_idx-1;

		emit("mov v%d [v%d]", len, len);

		emit_declaration(for_it);

		emit("mov vd%d 0", acc);
		emit_noindent("%s:", loop_label);

		emit("mov vb%d [v%d+v%d]", vregs_idx, string, acc);

		stack_offset += 2;
		emit("mov [rbp-%d] vb%d", stack_offset, vregs_idx);
		emit("mov byte [rbp-%d] 0", stack_offset-1);
		emit("lea v%d [rbp-%d]", vregs_idx++, stack_offset);
		stack_offset += 4;
		emit("mov dword [rbp-%d] 1", stack_offset);
		emit("lea v%d [rbp-%d]", vregs_idx++, stack_offset);

		emit_store_offset(for_it->lvar_valproppair->loff, for_it->vtype);

		emit_block(n->for_body, n->n_for_stmts);

		emit("inc vd%d", acc);
		emit("cmp vd%d vd%d", acc, len);
		emit("jl %s", loop_label);

		stack_offset = saved_stack;
		return;
	} else if (for_enum->type == AST_ARRAY) {
		sizes = getArraySizes(for_enum, for_enum->array_dims);

		int acc = 1;
		for (int i = 0; i < for_enum->array_dims; i++) {
			acc *= sizes[i];
		}

		stack_offset += 4 * acc;

		enum_off = stack_offset;

		size_t array_len = 0;
		emit_offset_assign(for_enum->array_dims, sizes, &array_len, &for_enum->array_elems, stack_offset, for_enum);
	}
cont_nostring:

	char *loop_label = makeLabel();
	int acc = vregs_idx++;
	int len = vregs_idx++;
	int idx = vregs_idx++;

	emit_declaration(for_it);

	emit("mov vd%d 0", acc);
	emit("mov vd%d 0", idx);
	emit("mov vd%d %d", len, sizes[0]);
	emit_noindent("%s:", loop_label);

	int sizeacc = 1;
	for (int i = 0; i < for_it->v_array_dimensions; i++) {
		sizeacc *= for_it->varray_size[i];
	}

	emit("lea vd%d [vd%d*4]", idx, acc);
	emit("imul vd%d %d", idx, sizeacc);

	if (!for_it->v_array_dimensions) {
		emit("add rbp v%d", idx);
		emit("mov vd%d [rbp-%d]", vregs_idx, enum_off);
		emit("sub rbp v%d", idx);
		emit_store_offset(for_it->lvar_valproppair->loff, for_it->vtype);
	} else {
		int it_sizes = 1;
		for (int i = 0; i < for_it->v_array_dimensions; i++) {
			it_sizes *= for_it->varray_size[i];
		}

		for (int i = 0; i < it_sizes; i++) {
			emit("add rbp v%d", idx);
			emit("mov vd%d [rbp-%d]", vregs_idx, enum_off-(i*4));
			emit("sub rbp v%d", idx);
			emit("mov dword [rbp-%d] vd%d", for_it->lvar_valproppair->loff-(i*4), vregs_idx);
		}
	}

	emit_block(n->for_body, n->n_for_stmts);

	emit("inc vd%d", acc);
	emit("cmp vd%d vd%d", acc, len);
	emit("jl %s", loop_label);

	stack_offset = saved_stack;
#undef for_enum
#undef for_it
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

static int *liverange_subtract(int *live1, int *live2, size_t size1, size_t size2)
{
	for (int i = 0; i < size2; i++) {
		for (int j = 0; j < size1; j++) {
			if (live2[i] == live1[j]) {
				if (size1 > 1) {
					memmove(&live1[j], &live1[j+1], sizeof(int) * (size1 - (j+1)));
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

static InterferenceNode **lva()
{
	int *live_range[ins_array_sz];
	size_t live_sz_array[ins_array_sz];

	int *used_vregs = calloc(vregs_count, sizeof(int));
	size_t used_vregs_n = MAX_REGISTER_COUNT;

	int *syscall_list = NULL;
	size_t syscall_list_sz = 0;

	MnemNode *n;

	InterferenceNode **interference_graph = calloc(vregs_count, sizeof(InterferenceNode));

	for (int p = 0; p < 2; p++) {
		for (int i = ins_array_sz-1; i >= 0; i--) {
			int *live = NULL;
			size_t live_sz = 0;

			int *live_del = malloc(0);
			size_t live_del_sz = 0;

			if ((i == ins_array_sz-1)  && (p == 1) && (used_vregs_n != vregs_count)) { 	// not all vregs used
				for (int j = MAX_REGISTER_COUNT; j < vregs_count; j++) {
					if (!used_vregs[j]) {
						live = realloc(live, sizeof(int) * (live_sz+1));
						live[live_sz++] = j;
					}
				}
			}

			n = ins_array[i];
			if (n->type >= MOV && n->type <= LEA) {
				if (n->right->type == VIRTUAL_REG || n->right->type == REAL_REG) {
					live = addToLiveRange(n->right->idx, live, &live_sz);
					if (!used_vregs[n->right->idx]) {
						used_vregs[n->right->idx] = 1;
						used_vregs_n++;
					}
				} else if (n->right->type == BRACKET_EXPR) {
					int saved = live_sz;
					for (int i = 0; i < n->right->n_vregs_used; i++) {
						live = addToLiveRange(n->right->vregs_used[i]->idx, live, &live_sz);
						if (saved != live_sz) {
							used_vregs[n->right->vregs_used[i]->idx] = 1;
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
						used_vregs[n->right->idx] = 1;
						used_vregs_n++;
					} else if (n->right->type == BRACKET_EXPR) {
						int saved = live_sz;
						for (int i = 0; i < n->right->n_vregs_used; i++) {
							live = addToLiveRange(n->right->vregs_used[i]->idx, live, &live_sz);
							if (saved != live_sz) {
								used_vregs[n->right->vregs_used[i]->idx] = 1;
								used_vregs_n++;
							}
						}
					}
				}
				if (n->left->type == VIRTUAL_REG || n->left->type == REAL_REG) {
					live = addToLiveRange(n->left->idx, live, &live_sz);
					used_vregs[n->left->idx] = 1;
					used_vregs_n++;
				} else if (n->left->type == BRACKET_EXPR) {
					int saved = live_sz;
					for (int i = 0; i < n->left->n_vregs_used; i++) {
						live = addToLiveRange(n->left->vregs_used[i]->idx, live, &live_sz);
						if (saved != live_sz) {
							used_vregs[n->left->vregs_used[i]->idx] = 1;
							used_vregs_n++;
						}
					}
				} else if (p == 1 && (n->type >= JE && n->type <= GOTO)) {   // control-flow change instruction
					char *label = n->left->mnem;
					int *live_at_label = NULL;
					size_t live_at_label_sz = 0;
					for (int j = 0; j < ins_array_sz; j++) {
						if (ins_array[j]->type == LABEL) {
							if (!strcmp(ins_array[j]->mnem, label)) {
								if (j < i) {
									live_at_label = live_range[j];
									live_at_label_sz = live_sz_array[j];
								}
								break;
							}
						}
					}
					if (live_at_label != NULL) {
						live = liverange_union(live_at_label, live, live_at_label_sz, &live_sz);
					}
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

				if (p == 0) {
					syscall_list = realloc(syscall_list, sizeof(int) * (syscall_list_sz + 1));
					syscall_list[syscall_list_sz++] = i;
				}
			}

			if (i == ins_array_sz-1) {
				live_range[i] = malloc(sizeof(int) * live_sz);
				live_range[i] = memcpy(live_range[i], live, live_sz * sizeof(int));

				live_sz_array[i] = live_sz;
			} else {
				live = liverange_union(live_range[i+1], live, live_sz_array[i+1], &live_sz);
				live_range[i] = malloc(sizeof(int) * live_sz);
				live_range[i] = memcpy(live_range[i], live, live_sz * sizeof(int));
				live_range[i] = liverange_subtract(live_range[i], live_del, live_sz, live_del_sz);
				if (live_sz >= live_del_sz) {
					live_sz_array[i] = live_sz - live_del_sz;
				}
			}
			free(live);
		}
	}

	for (int i = 0; i < syscall_list_sz; i++) {
		int unused_arg_regs[7];
		size_t unused_arg_regs_sz = 0;

#define idx 		(syscall_list[i+1]+1) // last instruction before the next syscall
#define is_sc_arg(x)	( (x==0) || (x==5) || (x==4) || (x==3) || (x==8) || (x==6) || (x==7) )
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

		if (i == syscall_list_sz-1) {
			for (int j = syscall_list[i]; j >= 0; j--) {
				live_range[j] = liverange_subtract(live_range[j], &unused_arg_regs[0], live_sz_array[j], unused_arg_regs_sz);
				if (live_sz_array[j] >= unused_arg_regs_sz) {
					live_sz_array[j] -= unused_arg_regs_sz;
				}
			}
		} else {
			for (int j = syscall_list[i]; j > syscall_list[i+1]; j--) {
				live_range[j] = liverange_subtract(live_range[j], &unused_arg_regs[0], live_sz_array[j], unused_arg_regs_sz);
				if (live_sz_array[j] >= unused_arg_regs_sz) {
					live_sz_array[j] -= unused_arg_regs_sz;
				}
			}
		}
	}
	
	// create InterferenceNode for every live variable
	for (int i = 0; i < ins_array_sz; i++) {
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

#if 1
		printf("LIVE at %d: ", i);
		for (int j = 0; j < live_sz; j++) {
			printf("%d, ", live[j]);
		}
		printf("\n---\n");
#endif
	}

#if 1
	for (int i = 0; i < vregs_count; i++) {
		printf("Neighbors of v%d:\n", i);
		if (interference_graph[i]) {
			for (int j = 0; j < interference_graph[i]->neighbor_count; j++) {
					printf("\tv%d\n", interference_graph[i]->neighbors[j]->idx);
			}
		} else {
			printf("v%d is unused.\n", i);
		}
	}
#endif

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

	int colored_nodes = MAX_REGISTER_COUNT;
	while (colored_nodes < vregs_count) {
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
#if 1
	for (int i = 0; i < vregs_count; i++) {
		if (g[i]) {
			printf("v%d: %d\n", g[i]->idx, g[i]->color);
		}
	}
#endif
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
							char *ret = malloc(strlen(str) + offset);
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
	for (int i = 0; i < ins_array_sz; i++) {
		MnemNode *n = ins_array[i];
		if (is_instruction_mnemonic(n->mnem)) {
			if (n->left->type == VIRTUAL_REG) {
				char *reg = assign_color(n->left->mnem, g);
				if (reg) {
					n->left->mnem = realloc(n->left->mnem, strlen(reg)+1);
					strcpy(n->left->mnem, reg);
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


// Modified insertion sort from https://github.com/geohot/mergesorts/blob/master/mergesort.c
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

static void optimize()
{
	vregs_count = vregs_idx;

	InterferenceNode **graph = lva();
	color(graph);

	assign_registers(graph);
}
