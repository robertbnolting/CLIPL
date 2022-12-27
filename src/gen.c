#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "parse.h"
#include "gen.h"
#include "error.h"

#define PSEUDO_ASM_OUT 1

#define MAX_REGISTER_COUNT 13

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

static FILE *outputfp;
static int entrypoint_defined;

static void emit_func_prologue();
static void emit_block();
static void emit_expr();
static void emit_literal();
static void emit_declaration();
static void emit_assign();
static void emit_store();
static void emit_lvar();
static void emit_array_assign();
static void emit_offset_assign();

static size_t do_array_arithmetic();

static int **getArrayMembers();
static int get_idx_offset();

static InterferenceNode **lva();
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
	outputbuf = malloc(0);
	outputbuf_sz = 0;
}

static char *_label() {
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
	} else if (MATCHES("add")) {
		return ADD;
	} else if (MATCHES("cmp")) {
		return CMP;
	} else if (MATCHES("inc")) {
		return INC;
	} else if (MATCHES("je")) {
		return JE;
	} else if (MATCHES("jne")) {
		return JNE;
	} else if (MATCHES("jz")) {
		return JZ;
	} else if (MATCHES("jnz")) {
		return JNZ;
	} else if (MATCHES("goto")) {
		return GOTO;
	} else if (MATCHES("push")) {
		return PUSH;
	} else if (MATCHES("pop")) {
		return POP;
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
	} else if (MATCHES("je")) {
		return JE;
	} else if (MATCHES("jne")) {
		return JNE;
	} else if (MATCHES("jz")) {
		return JZ;
	} else if (MATCHES("jnz")) {
		return JNZ;
	} else if (MATCHES("goto")) {
		return GOTO;
	} else if (MATCHES("push")) {
		return PUSH;
	} else if (MATCHES("pop")) {
		return POP;
	}

	return 0;
}
#undef MATCHES

MnemNode *makeMnemNode(char *mnem)
{
	if (mnem[0] == '\0') {
		return NULL;
	}

	MnemNode *r = malloc(sizeof(MnemNode));

	int off = 0;
	if (mnem[0] == '\t') {
		off = 1;
	}

	int type = is_instruction_mnemonic(mnem);

	if (!type) {
		char *mnem_p = mnem;
		char buf[10] = {0};
		size_t buf_sz = 0;

		if (mnem_p[off] == '[') {
			type = BRACKET_EXPR;
			while (*(++mnem_p) != ']') {
				if (*mnem_p != 'v' && !(*mnem_p >= '0' && *mnem_p <= '9')) {
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
		}
	}

	int idx = -1;
	char mode = 0;
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
	r->left = NULL;
	r->right = NULL;

	r->left_spec = NULL;
	r->right_spec = NULL;

	r->first_def = 0;

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
	strcat(&tmpbuf[0], "\n");

	char mnem[50] = {0};
	size_t mnem_len = 0;
	char c;

	MnemNode *ins = NULL;
	int prev_ins = 0;

	int counter = 0;
	// pseudo-asm parser
	while (c = tmpbuf[counter++]) {
		if (c == ' ' || c == '\n') {
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
						ins_array[ins_array_sz++] = ins; ins = NULL;
					}
				}

				clear(mnem);
				mnem_len = 0;
			} else {
				if (c == '\n') {
					MnemNode *other = makeMnemNode(&mnem[0]);
					if (other) {
						ins_array = realloc(ins_array, (ins_array_sz+1) * sizeof(MnemNode *));
						ins_array[ins_array_sz++] = other;
						prev_ins = 0;
						clear(mnem);
						mnem_len = 0;
					}
				} else {
					mnem[mnem_len++] = c;
				}
			}
		} else {// if (c != ',' || (c == ',' && (mnem[mnem_len-1] >= '0' && mnem[mnem_len-1] <= '9'))) {
			mnem[mnem_len++] = c;
		}
	}

#if 0
	outputbuf_sz += strlen(tmpbuf);
	outputbuf = realloc(outputbuf, outputbuf_sz+1);
	strcat(outputbuf, &tmpbuf[0]);
#endif
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
		} else {
			strcpy(tmpbuf, ins_array[i]->mnem);
			strcat(tmpbuf, "\n");
		}

		outputbuf_sz += strlen(tmpbuf) + 1;
		outputbuf = realloc(outputbuf, outputbuf_sz);
		strcat(outputbuf, &tmpbuf[0]);
	}

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
	emit_noindent("\n\n\tmov rax %d", code);
	emit("mov rdi 0");

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
	emit("mov rbp rsp");
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
		int off;
		case AST_DECLARATION:
			switch (n->lvar_valproppair->type)
			{
				case TYPE_INT:
					stack_offset += 4;
					n->lvar_valproppair->loff = stack_offset;
					emit("mov [rbp-%d] vd%d", stack_offset, vregs_idx++);
					break;
				case TYPE_STRING:
					stack_offset += 10;
					n->lvar_valproppair->loff = stack_offset;
					emit("mov [rbp-%d] vw%d", stack_offset, vregs_idx-1);
					emit("mov [rbp-%d] v%d", stack_offset+2, vregs_idx++);
					break;
				default:
					break;
			}
			break;
		case AST_IDENT:
			off = n->lvar_valproppair->loff;
			switch (n->lvar_valproppair->type)
			{
				case TYPE_INT:
					emit("mov [rbp-%d] v%d", off, vregs_idx++);
					break;
				case TYPE_STRING:
					emit("mov [rbp-%d] vw%d", off, vregs_idx-1);
					emit("mov [rbp-%d] v%d", off+2, vregs_idx++);
					break;
				default:
					break;
			}
			break;
		case AST_IDX_ARRAY:
			off = get_idx_offset(n);
			emit("mov [rbp+%d] v%d", off, vregs_idx++);
			break;
		default:
			printf("Not implemented.\n");
			break;
	}
}

static void emit_assign(Node *n)
{
	if (n->left->lvar_valproppair->type != TYPE_ARRAY) {
		emit_expr(n->right);
		emit_store(n->left);
	} else {
		emit_array_assign(n->left, n->right);
	}
}

static void emit_array_assign(Node *var, Node *array)
{
#define pair (var->lvar_valproppair)
	emit_expr(var);

	if (array->type == AST_ARRAY || array->type == AST_IDENT || array->type == AST_IDX_ARRAY) {
		emit_offset_assign(pair->array_dims, pair->array_size, &pair->array_len, &pair->array_elems, pair->loff, array);
	} else {
		do_array_arithmetic(array, var);
	}
#undef pair
}

static void emit_offset_assign(int array_dims, int *array_size, size_t *array_len, Node ***array_elems, int loff, Node *array)
{
	if (array->type == AST_IDX_ARRAY) {
		ValPropPair *ref_array = array->lvar_valproppair->ref_array;
		Node *indexed_array;
		for (int i = 0; i < array->ndim_index; i++) {
			indexed_array = ref_array->array_elems[array->index_values[i]->ival];
		}
		// TODO change function
		//emit_array_assign(var, indexed_array);
	} else {
		int total_size = 1;
		for (int i = 0; i < array_dims; i++) {
			total_size *= array_size[i];
		}

		size_t member_sz = 0;
		int iter = 0;
		int **members = getArrayMembers(array, &member_sz, total_size, array_size[array_dims-1], array_dims, &iter, total_size * 4);

		if (member_sz > total_size) {
			c_error("Invalid array assignment.", -1);
		}

		int counter = 1;
		int acc = 1;

		for (int j = 1; j < array_dims; j++) {
			acc *= array_size[j];
		}

		*array_len = 0;

		for (int i = 0; i < member_sz; i++) {
			emit("mov dword [rbp-%d] %d", members[i][1]+(loff-members[0][1]), members[i][0]);

			if (counter < acc) {
				counter++;
			} else {
				(*array_len)++;
				counter = 1;
			}
		}

		*array_elems = array->array_elems;
	}
}

static int **getArrayMembers(Node *array, size_t *n_members, int total_size, int last_size, int dims, int *n_iter, size_t offset)
{
	int **members = NULL;

	size_t array_size;
	Node **array_elems;

	if (array->type == AST_ARRAY) {
		array_size = array->array_size;
		array_elems = array->array_elems;
	} else if (array->type == AST_IDENT) {
		array_size = array->lvar_valproppair->array_len;
		array_elems = array->lvar_valproppair->array_elems;
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

	if (dims == 1) {
		(*n_iter)++;
	}

	for (int i = 0; i < array_size; i++) {
		if (dims > 1) {
				size_t ret_sz = 0;
				int **ret = getArrayMembers(array_elems[i], &ret_sz, total_size, last_size, dims-1, n_iter, 4 * (total_size - ((*n_iter) * last_size)));

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

static size_t do_array_arithmetic(Node *expr, Node *var)
{
	switch (expr->type)
	{
	case AST_ADD:
		/*
		Node ***elems = NULL;
		size_t *len = NULL;
		int dims = 0;

		if (array->type == AST_ARRAY) {
			elems = &array->array_elems;
			len = &array->array_size;
			dims = array->array_dims;
		} else if (array->type == AST_IDENT) {
			ValPropPair *pair = array->lvar_valproppair;
			elems = &pair->array_elems;
			len = &pair->array_len;
			dims = pair->array_dims;
		} else {
			printf("Not implemented.\n");
		}

		int type;
		*/
		int assign_off = var->lvar_valproppair->loff;
		size_t n = do_array_arithmetic(expr->left, var);

		assign_off -= n * 4;

#define pair (var->lvar_valproppair)
		emit_offset_assign(pair->array_dims, pair->array_size, &pair->array_len, &pair->array_elems, assign_off, expr->right);

		return n + pair->array_len;

#undef pair
		/*
		switch (type)
		{
		case AST_ARRAY:
			if (dims > expr->right->array_dims) {
				*elems = realloc(*elems, ((*len)+1) * sizeof(Node*));
				(*elems)[*len] = expr->right;
				(*len)++;
			} else {
				for (int i = 0; i < expr->right->array_size; i++) {
					*elems = realloc(*elems, ((*len)+1) * sizeof(Node*));
					(*elems)[*len] = expr->right->array_elems[i];
					(*len)++;
				}
			}
			break;
		case AST_INT:
		case AST_STRING:
		case AST_FLOAT:
		case AST_BOOL:
			*elems = realloc(*elems, ((*len)+1) * sizeof(Node*));
			(*elems)[*len] = expr->right;
			(*len)++;
			break;
		default:
			printf("Not implemented.\n");
			break;
		}

		return array;
		*/
	case AST_ARRAY:
		emit_array_assign(var, expr);
		return expr->array_size;
	case AST_IDENT:
		emit_array_assign(var, expr);
		return expr->lvar_valproppair->array_len;
	default:
		printf("Not implemented.\n");
		return NULL;
	}
}



static Node *getArrayMemberByIndex(Node **array, int idx)
{
	if (!array[0]) {
		return NULL;
	}

	if (array[0]->type == AST_ARRAY) {
		Node *elem = getArrayMemberByIndex(array[0]->array_elems, idx);
		if (!elem) {
			return getArrayMemberByIndex(&array[1], idx-1);
		} else {
			return elem;
		}
	} else {
		if (idx == 0) {
			return array[0];
		} else {
			return getArrayMemberByIndex(&array[1], idx-1);
		}
	}
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
			emit("mov v%d %u", vregs_idx, expr->ival);
			break;
		case AST_BOOL:
			emit("mov v%d %u", vregs_idx, expr->bval);
			break;
		case AST_STRING:
			if (!expr->slabel) {
				expr->slabel = _label();
				emit_noindent("\nsection .data");
				emit("%s db %s", expr->slabel, expr->sval);
				emit_noindent("\nsection .text");
			}
			emit("mov v%d %ld", vregs_idx++, expr->slen);
			emit("mov v%d %s", vregs_idx, expr->slabel);
			break;
		case AST_ARRAY:
			break;
		default:
			printf("Internal error.");
	}
}

static int get_idx_offset(Node *n)
{
	ValPropPair *ref_array = n->lvar_valproppair->ref_array; // TODO: check in parse.c if {AST_IDX_ARRAY}->lvar_valproppair is necessary
	int array_offset = 0;
	for (int i = 0; i < n->ndim_index; i++) {
		int sizeacc = 1;
		for (int j = i+1; j < ref_array->array_dims; j++) {
			sizeacc *= ref_array->array_size[j];
		}
		array_offset += n->index_values[i]->ival * sizeacc;
	}

	return ref_array->loff - 4 * array_offset;
}

// TODO: Port all AST_ARRAY properties to registers
static void emit_idx_array(Node *n)
{
	n->lvar_valproppair->loff = get_idx_offset(n);

	if (!n->lvar_valproppair->array_dims) {
		emit("mov vd%d [rbp-%d]", vregs_idx, n->lvar_valproppair->loff);
	}
}

static void emit_load(int offset, char *base, int type)
{
	switch (type)
	{
		case TYPE_STRING:
			emit("mov v%d 0", vregs_idx);
			emit("mov vw%d [%s-%d]", vregs_idx++, base, offset);
			emit("mov v%d [%s-%d]", vregs_idx, base, offset+2);
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
			stack_offset += 10;
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

	emit("%s v%d v%d", op, vregs_idx, saved_idx);
}

static void emit_string_arith_binop(Node *expr)
{
	emit_expr(expr->left);
	int string1 = vregs_idx;
	int string1_len = vregs_idx-1;
	vregs_idx++;

	emit_expr(expr->right);
	int string2 = vregs_idx;
	int string2_len = vregs_idx-1;
	vregs_idx++;

	char *loop_label = _label();

	int acc = vregs_idx++;
	emit("mov v%d 0", acc);
	emit_noindent("%s:", loop_label);

	int single_char = vregs_idx++;
	int end_of_string1 = vregs_idx++;
	emit("mov vb%d [v%d+v%d]", single_char, string2, acc);
	emit("mov v%d v%d", end_of_string1, string1);
	emit("add v%d v%d", end_of_string1, string1_len);
	emit("add v%d v%d", end_of_string1, acc);
	emit("mov [v%d] vb%d", end_of_string1, single_char);
	emit("inc v%d", acc);
	emit("cmp v%d v%d", acc, string2_len);
	emit("jne %s", loop_label);
	emit("add v%d v%d", string1_len, string2_len);

	emit("mov v%d v%d", vregs_idx++, string1_len);
	emit("mov v%d v%d", vregs_idx, string1);

}

static void emit_comp_binop(Node *expr)
{
	emit_expr(expr->left);
	push("rax");
	emit_expr(expr->right);
	emit("mov rcx rax");
	pop("rax");

	emit("cmp rax rcx");
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
			op(expr);
			break;
	}
}

static int *addToLiveRange(MnemNode *n, int *live, size_t *live_sz)
{
	if (n->type == VIRTUAL_REG) {
		int idx = n->idx;
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
	size_t used_vregs_n = 0;

	MnemNode *n;

	InterferenceNode **interference_graph = calloc(vregs_count, sizeof(InterferenceNode));

	for (int p = 0; p < 2; p++) {
		for (int i = ins_array_sz-1; i >= 0; i--) {
			int *live = malloc(0);
			size_t live_sz = 0;

			int *live_del = malloc(0);
			size_t live_del_sz = 0;

			if ((i == ins_array_sz-1)  && (p == 1) && (used_vregs_n != vregs_count)) { 	// not all vregs used
				for (int j = 0; j < used_vregs_n; j++) {
					if (!used_vregs[j]) {
						live = realloc(live, sizeof(int) * live_sz+1);
						live[live_sz++] = j;
					}
				}
			}

			n = ins_array[i];
			if (n->type == MOV) {
				if (n->right->type == VIRTUAL_REG) {
					live = addToLiveRange(n->right, live, &live_sz);
					if (!used_vregs[n->right->idx]) {
						used_vregs[n->right->idx] = 1;
						used_vregs_n++;
					}
				} else if (n->right->type == BRACKET_EXPR) {
					int saved = live_sz;
					for (int i = 0; i < n->right->n_vregs_used; i++) {
						live = addToLiveRange(n->right->vregs_used[i], live, &live_sz);
						if (saved != live_sz) {
							used_vregs[n->right->vregs_used[i]->idx] = 1;
							used_vregs_n++;
							saved = live_sz;
						}
					}
				}

				if (n->left->type == VIRTUAL_REG) {
					if (n->left->first_def) {
						live_del = addToLiveRange(n->left, live_del, &live_del_sz);
					}
				} else if (n->left->type == BRACKET_EXPR) {
					for (int i = 0; i < n->left->n_vregs_used; i++) {
						if (n->left->vregs_used[i]->first_def) {
							live_del = addToLiveRange(n->left->vregs_used[i], live_del, &live_del_sz);
						}
					}
				}
			} else if (n->type > MOV && n->type <= POP) { // instruction but not mov
				if (n->type <= CMP) {	// is binary operation
					if (n->right->type == VIRTUAL_REG) {
						live = addToLiveRange(n->right, live, &live_sz);
						used_vregs[n->right->idx] = 1;
						used_vregs_n++;
					} else if (n->right->type == BRACKET_EXPR) {
						for (int i = 0; i < n->right->n_vregs_used; i++) {
							live = addToLiveRange(n->right->vregs_used[i], live, &live_sz);
						}
					}
				}
				if (n->left->type == VIRTUAL_REG) {
					live = addToLiveRange(n->left, live, &live_sz);
				} else if (n->left->type == BRACKET_EXPR) {
					for (int i = 0; i < n->left->n_vregs_used; i++) {
						live = addToLiveRange(n->left->vregs_used[i], live, &live_sz);
					}
				} else if (p == 1 && (n->type >= JE && n->type <= GOTO)) {   // control-flow change instructions
					char *label = n->left->mnem;
					int *live_at_label = NULL;
					size_t live_at_label_sz = 0;
					for (int j = 0; j < ins_array_sz; j++) {
						if (ins_array[j]->type == LABEL) {
							if (!strcmp(ins_array[j]->mnem, label)) {
								live_at_label = live_range[j];
								live_at_label_sz = live_sz_array[j];
								break;
							}
						}
					}
					if (live_at_label != NULL) {
						live = liverange_union(live, live_at_label, live_sz, &live_at_label_sz);
						live_sz = live_at_label_sz;
					} else {
						printf("Label %s not found.\n", label);
						c_error("", -1);
					}
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
	int colored_nodes = 0;
	while (colored_nodes < vregs_count) {
		InterferenceNode *highest_sat = NULL;
		// calculate saturation of each node
		for (int i = 0; i < vregs_count; i++) {
			if (g[i]->color < 0) {
				for (int j = 0; j < g[i]->neighbor_count; j++) {
					if (g[i]->neighbors[j]->color > 0) {
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

						/*
						if (g[i]->neighbor_count > highest_sat->neighbor_count) {
							highest_sat = g[i];
						} */
					}
				}
			}
		}

		int lowest_color = -1;
		for (int i = 0; i < highest_sat->neighbor_count; i++) {
			if (highest_sat->neighbors[i]->color > lowest_color) {
				lowest_color = highest_sat->neighbors[i]->color;
			}
		}
		highest_sat->color = lowest_color+1;
		colored_nodes++;
	}
#if 1
	for (int i = 0; i < vregs_count; i++) {
		printf("v%d: %d\n", g[i]->idx, g[i]->color);
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
				c_error("Not implemented.\n");
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

static void optimize()
{
	vregs_count = vregs_idx;

	InterferenceNode **graph = lva();
	color(graph);

	assign_registers(graph);
}
