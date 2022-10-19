#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <limits.h>

#include "parse.h"
#include "lex.h"
#include "error.h"

#define AST_OUTPUT 0
#define CFG_OUTPUT 0
#define SYM_OUTPUT 1

static int pos;

#define curr()	(&Token_stream[pos])
#define get()	(&Token_stream[pos++])
#define next()	(pos++)
#define unget()	(pos--)
#define peek()	(&Token_stream[pos+1])
#define prev()	(&Token_stream[pos-1])

static int next_token();
static void expect();

// AST generation
static Node *read_global_expr();
static Node *read_primary_expr();
static Node *read_secondary_expr();
static Node *read_expr();

static Node *read_stmt();
static Node *read_if_stmt();
static Node *read_while_stmt();
static Node *read_for_stmt();
static Node *read_return_stmt();

static Node *read_assignment_expr();
static Node *read_relational_expr();
static Node *read_enumerable_expr();
static Node *read_indexed_array();
static Node *read_field_access();
static Node *read_additive_expr();
static Node *read_multiplicative_expr();
static Node *read_declaration_expr();
static Node *read_ident();
static Node *read_int();
static Node *read_float();
static Node *read_string();
static Node *read_bool();
static Node *read_array_expr();
static Node *read_record_def();
static Node *read_fn_def();
static Node *read_fn_call();
static Node **read_fn_parameters();
static Node **read_fn_body();

// AST traversal
static void traverse();
static char *get_array_sizes();
static char *list_nodearray();
static void list_stmts();

// Error messaging
static const char *tokenclassToString();
static const char *datatypeToString();

// CFG generation
static Node *cfg_aux_node();
static Node *cfg_join_node();

static Node *thread_ast();

// CFG traversal
static Node *printCFG();
static void printNode();

// Symbolic interpretation
static void sym_interpret();
static void interpret_expr();
static void interpret_assignment_expr();
static void interpret_declaration_expr();

// Parser start
static Node **global_functions;
static size_t global_function_count;

void parser_init()
{
	pos = 0;

	Node **node_array = malloc(0);
	size_t array_len = 0;

	for (;;) {
		Node *n = read_global_expr();
		if (n != NULL) {
			node_array = realloc(node_array, sizeof(Node *) * (array_len + 1));
			node_array[array_len] = n;
			array_len++;
		} else {
			break;
		}
	}

	global_functions = node_array;
	global_function_count = array_len;

	Node *cfg_array = thread_ast();

#if CFG_OUTPUT
	printCFG(cfg_array);
	printf("\n");
#endif

#if AST_OUTPUT
	for (int i = 0; i < array_len; i++) {
		traverse(node_array[i]);
		free(node_array[i]);
		printf("\n\n");
	}
	free(node_array);
#endif

	sym_interpret(cfg_array);
}

static Node *printCFG(Node *start)
{	
	Node *last = start;
	while (last != NULL) {
		printNode(last);
		if (last->type == AST_IF_STMT) {
			if (last->false_successor) {
				printf("\tTHEN: ");
				printCFG(last->successor);
				printf("\tELSE: ");
				last = printCFG(last->false_successor)->successor;
			} else {
				printf("\tTHEN: ");
				last = printCFG(last->successor)->successor;
			}
		} else if (last->type == AST_WHILE_STMT) {
			printCFG(last->while_true_successor);
			printf(")\n");
			last = last->successor;
		} else if (last->type == AST_FOR_STMT) {
			printCFG(last->for_loop_successor);
			printf(")\n");
			last = last->successor;
		} else if (last->type == CFG_JOIN_NODE) {
			return last;
		} else {
			last = last->successor;
		}
	}

	return NULL;
}

static int get_type_specifier(Token_type *tok)
{
	char *str = tok->repr;

	if (!strcmp(str, "void")) {
		return TYPE_VOID;
	} else if (!strcmp(str, "int")) {
		return TYPE_INT;
	} else if (!strcmp(str, "float")) {
		return TYPE_FLOAT;
	} else if (!strcmp(str, "string")) {
		return TYPE_STRING;
	} else if (!strcmp(str, "record")) {
		return TYPE_RECORD;
	} else if (!strcmp(str, "bool")) {
		return TYPE_BOOL;
	} else {
		return 0;
	}
}

static int is_keyword(Token_type *tok)
{
	char *str = tok->repr;

	if (!strcmp(str, "if")) {
		return KEYWORD_IF;
	} else if (!strcmp(str, "while")) {
		return KEYWORD_WHILE;
	} else if (!strcmp(str, "for")) {
		return KEYWORD_FOR;
	} else if (!strcmp(str, "return")) {
		return KEYWORD_RETURN;
	} else {
		return 0;
	}
}

static int is_stmt_node(Node *n)
{
	switch (n->type)
	{
		case AST_IF_STMT:
		case AST_WHILE_STMT:
		case AST_FOR_STMT:
		case AST_RETURN_STMT:
			return 1;
		default:
			return 0;
	}
}

static const char *tokenclassToString(int tclass)
{
	switch (tclass)
	{
		case EoF:
			return "End of File";
		case IDENTIFIER:
			return "Identifier";
		case INT:
			return "Integer";
		case FLOAT:
			return "Float";
		case STRING:
			return "String";
		case BOOL:
			return "Boolean";
		case TYPE_SPECIFIER:
			return "Type spcecifier";
		case EQ:
			return "==";
		case NE:
			return "!=";
		case GE:
			return ">=";
		case LE:
			return "<=";
		case ADD_ASSIGN:
			return "+=";
		case SUB_ASSIGN:
			return "-=";
		case MUL_ASSIGN:
			return "*=";
		case DIV_ASSIGN:
			return "/=";
		case ARROW_OP:
			return "->";
		default:
			return "Unknown token";
	}
}

static const char *datatypeToString(int type)
{
	switch (type)
	{
		case TYPE_INT:
			return "int";
		case TYPE_STRING:
			return "string";
		case TYPE_FLOAT:
			return "float";
		case TYPE_BOOL:
			return "bool";
	}
}

static int numPlaces (int n) 
{
	if (n < 0) n = (n == INT_MIN) ? INT_MAX : -n;
	if (n < 10) return 1;
	if (n < 100) return 2;
	if (n < 1000) return 3;
	if (n < 10000) return 4;
	if (n < 100000) return 5;
	if (n < 1000000) return 6;
	if (n < 10000000) return 7;
	if (n < 100000000) return 8;
	if (n < 1000000000) return 9;
	return 10;
}

static void printNode(Node *n)
{
	if (n == NULL) {
		return;
	}
	switch (n->type)
	{
		case AST_IDENT:
			printf("(IDENT: %s) ", n->name);
			break;
		case AST_INT:
			printf("(INT: %d) ", n->ival);
			break;
		case AST_FLOAT:
			printf("(FLOAT: %f) ", n->fval);
			break;
		case AST_STRING:
			printf("(STRING: %s) ", n->sval);
			break;
		case AST_BOOL:
			n->bval ? printf("(BOOLEAN: true) ") : printf("(BOOLEAN: false) ");
			break;
		case AST_ARRAY:
			printf("(%ld-member ARRAY) ", n->array_size);
			break;
		case AST_FIELD_ACCESS:
			printf("(FIELD ACCESS: %s.%s) ", n->access_rlabel->name, n->access_field->name);
			break;
		case AST_DECLARATION:
			printf("(%d-DECLARATION: %s) ", n->vtype, n->vlabel);
			break;
		case AST_IDX_ARRAY:
			printf("(%s[] index %d times) ", n->ia_label, n->ndim_index);
			break;
		case AST_RETURN_STMT:
			printf("(RETURN)");
			break;
		case AST_FUNCTION_CALL:
			printf("(CALL %s)\n", n->call_label);
			break;

		case AST_ADD:
			printf("(+)\n");
			break;
		case AST_SUB:
			printf("(-)\n");
			break;
		case AST_MUL:
			printf("(*)\n");
			break;
		case AST_DIV:
			printf("(/)\n");
			break;
		case AST_ASSIGN:
			printf("(=)\n");
			break;
		case AST_ADD_ASSIGN:
			printf("(+=)\n");
			break;
		case AST_SUB_ASSIGN:
			printf("(-=)\n");
			break;
		case AST_MUL_ASSIGN:
			printf("(*=)\n");
			break;
		case AST_DIV_ASSIGN:
			printf("(/=)\n");
			break;
		case AST_GT:
			printf("(>)\n");
			break;
		case AST_LT:
			printf("(<)\n");
			break;
		case AST_EQ:
			printf("(==)\n");
			break;
		case AST_NE:
			printf("(!=)\n");
			break;
		case AST_GE:
			printf("(>=)\n");
			break;
		case AST_LE:
			printf("(<=)\n");
			break;
		case AST_IF_STMT:
			printf("(IF)\n");
			break;
		case AST_WHILE_STMT:
			printf("(WHILE TRUE:\n");
			break;
		case AST_FOR_STMT:
			printf("(FOR BODY:\n");
			break;
		case CFG_AUXILIARY_NODE:
			break;
		case CFG_JOIN_NODE:
			printf("(ENDIF)\n");
			break;
	}
}

static void traverse(Node *root)
{
	if (root == NULL) {
		return;
	}

	char *s;
	switch (root->type)
	{	
		case AST_IDENT:
			printf("(IDENT: %s) ", root->name);
			break;
		case AST_INT:
			printf("(INT: %d) ", root->ival);
			break;
		case AST_FLOAT:
			printf("(FLOAT: %f) ", root->fval);
			break;
		case AST_STRING:
			printf("(STRING: %s) ", root->sval);
			break;
		case AST_BOOL:
			root->bval ? printf("(BOOLEAN: true) ") : printf("(BOOLEAN: false) ");
			break;
		case AST_ARRAY:
			s = list_nodearray(root->array_size, root->array_elems);
			printf("(ARRAY: [%s]) ", s);
			if (s[0] != 0) {
				free(s);
			}
			break;
		case AST_IDX_ARRAY:
			printf("(INDEXED %d-D ARRAY: %s | INDEX: ", root->ndim_index, root->ia_label);
			for (int i = 0; i < root->ndim_index; i++) {
				printf("[");
				traverse(root->index_values[i]);
				printf("]");
			}
			printf(")");
			break;
		case AST_FIELD_ACCESS:
			printf("(FIELD ACCESS | RECORD: ");
			traverse(root->access_rlabel);
			printf("| FIELD: ");
			traverse(root->access_field);
			printf(")");
			break;
		case AST_ADD:
			traverse(root->left);
			printf("(ADD: +) ");
			traverse(root->right);
			break;
		case AST_SUB:
			traverse(root->left);
			printf("(SUB: -) ");
			traverse(root->right);
			break;
		case AST_MUL:
			traverse(root->left);
			printf("(MUL: *) ");
			traverse(root->right);
			break;
		case AST_DIV:
			traverse(root->left);
			printf("(DIV: /) ");
			traverse(root->right);
			break;
		case AST_ASSIGN:
			traverse(root->left);
			printf("(ASSIGN: =) ");
			traverse(root->right);
			break;
		case AST_ADD_ASSIGN:
			traverse(root->left);
			printf("(ADD ASSIGN: +=) ");
			traverse(root->right);
			break;
		case AST_SUB_ASSIGN:
			traverse(root->left);
			printf("(SUB ASSIGN: -=) ");
			traverse(root->right);
			break;
		case AST_MUL_ASSIGN:
			traverse(root->left);
			printf("(MUL ASSIGN: *=) ");
			traverse(root->right);
			break;
		case AST_DIV_ASSIGN:
			traverse(root->left);
			printf("(DIV ASSIGN: /=) ");
			traverse(root->right);
			break;
		case AST_GT:
			traverse(root->left);
			printf("(GREATER: >) ");
			traverse(root->right);
			break;
		case AST_LT:
			traverse(root->left);
			printf("(LESS <) ");
			traverse(root->right);
			break;
		case AST_EQ:
			traverse(root->left);
			printf("(EQUAL: ==) ");
			traverse(root->right);
			break;
		case AST_NE:
			traverse(root->left);
			printf("(NOT EQUAL: !=) ");
			traverse(root->right);
			break;
		case AST_GE:
			traverse(root->left);
			printf("(GREATER EQUAL: >=) ");
			traverse(root->right);
			break;
		case AST_LE:
			traverse(root->left);
			printf("(LESS EQUAL: <=) ");
			traverse(root->right);
			break;
		case AST_DECLARATION:
			s = get_array_sizes(root->v_array_dimensions, root->varray_size);
			if (root->vrlabel != NULL) {
				root->v_array_dimensions ? printf("(%s-RECORD %d-D ARRAY DECLARATION: %s | ARRAY SIZE: %s) ", root->vrlabel, root->v_array_dimensions, root->vlabel, s)
				: printf("(%s-RECORD DECLARATION: %s) ", root->vrlabel, root->vlabel);

			} else {
				root->v_array_dimensions ? printf("(%d-D ARRAY DECLARATION: %s | MEMBER TYPE: %d | ARRAY SIZE: %s) ", root->v_array_dimensions, root->vlabel, root->vtype, s)
				: printf("(PRIMITIVE DECLARATION: %s | TYPE: %d) ", root->vlabel, root->vtype);
			}
			free(s);

			break;
		case AST_RECORD_DEF:
			printf("(RECORD DEFINITION: %s | FIELDS: {\n", root->rlabel);
			list_stmts(root->n_rfields, root->rfields);
			printf("})");
			break;
		case AST_FUNCTION_DEF:
			s = list_nodearray(root->n_params, root->fnparams);
			root->ret_array_dims ? printf("(FUNCTION DEFINITION: %s | RETURNS %d-D array with member type %d | PARAMS: %s | BODY: {\n", root->flabel, root->ret_array_dims, root->return_type, s)
			: printf("(FUNCTION DEFINITION: %s | RETURNS: %d | PARAMS: %s | BODY: {\n", root->flabel, root->return_type, s);
			if (s[0] != 0) {
				free(s);
			}
			list_stmts(root->n_stmts, root->fnbody);
			printf("})");
			break;
		case AST_FUNCTION_CALL:
			printf("(FUNCTION CALL: %s | ARGS: ", root->call_label);
			list_stmts(root->n_args, root->callargs);
			printf(")");
			/*
			if (s[0] != 0) {
				free(s);
			} */
			break;
		case AST_IF_STMT:
			printf("(IF STATEMENT | CONDITION: ");
			traverse(root->if_cond);
			printf("| THEN: {\n");
			list_stmts(root->n_if_stmts, root->if_body);
			printf("}");
			printf(" ELSE: {\n");
			list_stmts(root->n_else_stmts, root->else_body);
			printf("})");
			break;
		case AST_WHILE_STMT:
			printf("(WHILE STATEMENT | CONDITION: ");
			traverse(root->while_cond);
			printf("| BODY: {\n");
			list_stmts(root->n_while_stmts, root->while_body);
			printf("})");
			break;
		case AST_FOR_STMT:
			root->for_iterator->v_array_dimensions ? printf("(FOR STATMENT | ITERATOR OF TYPE %d-D ARRAY WITH MEMBER TYPE %d: %s | ENUMERABLE: ", root->for_iterator->v_array_dimensions, root->for_iterator->vtype, root->for_iterator->vlabel)
				: printf("(FOR STATEMENT | ITERATOR OF TYPE %d: %s | ENUMERABLE: ", root->for_iterator->vtype, root->for_iterator->vlabel);
			traverse(root->for_enum);
			printf("| BODY: {\n");
			list_stmts(root->n_for_stmts, root->for_body);
			printf("})");
			break;
		case AST_RETURN_STMT:
			printf("(RETURN STATEMENT: ");
			traverse(root->retval);
			printf(") ");
			break;
	}
}

static char *get_array_sizes(int dims, int *sizes)
{
	char *ret = NULL;
	size_t ret_len = 0;
	char *num = NULL;
	for (int i = 0; i < dims; i++) {
		num = malloc(numPlaces(sizes[i]) + 1);
		sprintf(num, "%d", sizes[i]);
		if (dims > 1 && i < dims-1) {
			ret = realloc(ret, ret_len + strlen(num) + 2);
			strcpy(&ret[ret_len], num);
			strcat(ret, "x");
			ret_len += strlen(num) + 1;
		} else {
			ret = realloc(ret, ret_len + strlen(num) + 1);
			strcpy(&ret[ret_len], num);
			ret_len += strlen(num);
		}
		free(num);
	}

	return ret;
}

static void list_stmts(size_t n, Node **body)
{
	for (size_t i = 0; i < n; i++) {
		traverse(body[i]);
		printf("\n");
	}
}

static char *list_nodearray(size_t n, Node **buffer)
{
	if (n == 0) {
		return "";
	}

	char *ret = malloc(0);
	size_t ret_size = 0;

	char *s = NULL;

	for (size_t i = 0; i < n; i++) {
		switch (buffer[i]->type)
		{
			case AST_IDENT:
				ret = realloc(ret, ret_size + strlen(buffer[i]->name) + 3);
				strcpy(&ret[ret_size], buffer[i]->name);
				ret_size += strlen(buffer[i]->name);
				break;
			case AST_INT:
				s = malloc(numPlaces(buffer[i]->ival) + 1);
				//s = malloc(11);	// INT_MAX has 10 digits
				sprintf(s, "%d", buffer[i]->ival);
			        ret = realloc(ret, ret_size + strlen(s) + 1);
				strcpy(&ret[ret_size], s);
				ret_size += strlen(s);
				free(s);
				break;
			case AST_FLOAT:
				s = malloc(20);
				sprintf(s, "%f", buffer[i]->fval);
			        ret = realloc(ret, ret_size + strlen(s) + 3);
				strcpy(&ret[ret_size], s);
				ret_size += strlen(s);
				break;

			case AST_STRING:
#define sval buffer[i]->sval
				ret = realloc(ret, ret_size + strlen(sval) + 3);
				strcpy(&ret[ret_size], sval);
				ret_size += strlen(sval);
				break;
#undef sval
			case AST_ARRAY:
				s = (char *) malloc(2);
				strcpy(s, "[");
				char *elems = list_nodearray(buffer[i]->array_size, buffer[i]->array_elems);
				s = realloc(s, strlen(elems) + 3);
				strcat(s, elems);
				strcat(s, "]");

				ret = realloc(ret, ret_size + strlen(s) + 3);
				strcpy(&ret[ret_size], s);
				ret_size += strlen(s);

				free(s);

				break;
			case AST_DECLARATION:
				if (buffer[i]->vrlabel != NULL) {
					char *rec = malloc(strlen(buffer[i]->vrlabel) + 1);
					strcpy(rec, buffer[i]->vrlabel);
					s = malloc(23 + strlen(buffer[i]->vlabel) + strlen(rec) + 1);

					strcpy(s, "(");
					strcat(s, rec);
					strcat(s, "-RECORD DECLARATION: ");
					strcat(s, buffer[i]->vlabel);
					strcat(s, ")");
				} else {
					char *type_s = malloc(10 + 11);
					sprintf(type_s, " | TYPE: %d)", buffer[i]->vtype);
					s = malloc(15 + strlen(buffer[i]->vlabel) + strlen(type_s) + 1);

					strcpy(s, "(DECLARATION: ");
					strcat(s, buffer[i]->vlabel);
					strcat(s, type_s);
				}

				ret = realloc(ret, ret_size + strlen(s) + 2);
				strcpy(&ret[ret_size], s);
				ret_size += strlen(s);

				free(s);

				break;
			case AST_FUNCTION_CALL:
				char *label = malloc(strlen(buffer[i]->call_label));
				strcpy(label, buffer[i]->call_label);
				char *args = list_nodearray(buffer[i]->n_args, buffer[i]->callargs);
				s = malloc(29 + strlen(label) + strlen(args) + 1);

				strcpy(s, "\n\t(FUNCTION CALL: ");
				strcat(s+17, label);
				strcat(s+17+(strlen(label)-1), " | ARGS: ");
				strcat(s+28+(strlen(label)-1), args);
				strcat(s+28+strlen(label)+strlen(args), ")");

				ret = realloc(ret, ret_size + strlen(s) + 2);
				strcpy(&ret[ret_size], s);
				ret_size += strlen(s);

				break;
			default:
				printf("Printing error: Could not printf Node.\n");
		}
		//strcpy(&ret[ret_size], ", ");
		strcat(ret, ", ");
		ret_size += 2;
	}

	return ret;
}

static Node *makeNode(Node *tmp)
{
	Node *r = malloc(sizeof(Node));

	*r = *tmp;

	return r;
}

static Node *ast_inttype(int val)
{
	return makeNode(&(Node){AST_INT, .ival = val});
}

static Node *ast_floattype(float val)
{
	return makeNode(&(Node){AST_FLOAT, .fval = val});
}

static Node *ast_identtype(char *n)
{
	return makeNode(&(Node){AST_IDENT, .name = n});
}

static Node *ast_stringtype(char *val)
{
	return makeNode(&(Node){AST_STRING, .sval = val});
}

static Node *ast_booltype(int val)
{
	return makeNode(&(Node){AST_BOOL, .bval = val});
}

static Node *ast_arraytype(size_t sz, Node **arr)
{
	return makeNode(&(Node){AST_ARRAY, .array_size=sz, .array_elems=arr});
}

static Node *ast_indexed_array(char *label, Node **idxs, size_t idxs_sz)
{
	return makeNode(&(Node){AST_IDX_ARRAY, .ia_label=label, .index_values=idxs, .ndim_index=idxs_sz});
}

static Node *ast_field_access(Node *rlabel, Node *field)
{
	return makeNode(&(Node){AST_FIELD_ACCESS, .access_rlabel=rlabel, .access_field=field});
}

static Node *ast_binop(int op, Node *lhs, Node *rhs)
{
	switch (op)
	{
		case '+':
			return makeNode(&(Node){AST_ADD, .left=lhs, .right=rhs});
		case '-':
			return makeNode(&(Node){AST_SUB, .left=lhs, .right=rhs});
		case '*':
			return makeNode(&(Node){AST_MUL, .left=lhs, .right=rhs});
		case '/':
			return makeNode(&(Node){AST_DIV, .left=lhs, .right=rhs});
		case '=':
			return makeNode(&(Node){AST_ASSIGN, .left=lhs, .right=rhs});
		case '>':
			return makeNode(&(Node){AST_GT, .left=lhs, .right=rhs});
		case '<':
			return makeNode(&(Node){AST_LT, .left=lhs, .right=rhs});
		case ADD_ASSIGN:
			return makeNode(&(Node){AST_ADD_ASSIGN, .left=lhs, .right=rhs});
		case SUB_ASSIGN:
			return makeNode(&(Node){AST_SUB_ASSIGN, .left=lhs, .right=rhs});
		case MUL_ASSIGN:
			return makeNode(&(Node){AST_MUL_ASSIGN, .left=lhs, .right=rhs});
		case DIV_ASSIGN:
			return makeNode(&(Node){AST_DIV_ASSIGN, .left=lhs, .right=rhs});
		case EQ:
			return makeNode(&(Node){AST_EQ, .left=lhs, .right=rhs});
		case NE:
			return makeNode(&(Node){AST_NE, .left=lhs, .right=rhs});
		case GE:
			return makeNode(&(Node){AST_GE, .left=lhs, .right=rhs});
		case LE:
			return makeNode(&(Node){AST_LE, .left=lhs, .right=rhs});
		default:
			return NULL;
	}
}

static Node *ast_decl(char *label, int type, char *rlabel, int array_dims, int *array_size)
{
	return makeNode(&(Node){AST_DECLARATION, .vlabel=label, .vtype=type, .vrlabel=rlabel, .v_array_dimensions=array_dims, .varray_size=array_size});
}

static Node *ast_record_def(char *label, size_t n_fields, Node **fields)
{
	return makeNode(&(Node){AST_RECORD_DEF, .rlabel=label, .n_rfields=n_fields, .rfields=fields});
}
 
static Node *ast_funcdef(char *label, int ret_type, int array_dims, size_t params_n, size_t stmts_n, Node **params, Node **body)
{
	return makeNode(&(Node){AST_FUNCTION_DEF, .flabel=label, .return_type=ret_type, .ret_array_dims=array_dims, .n_params=params_n, .n_stmts=stmts_n, .fnparams=params, .fnbody=body});
}

static Node *ast_funccall(char *label, size_t nargs, Node **args)
{
	return makeNode(&(Node){AST_FUNCTION_CALL, .call_label=label, .n_args=nargs, .callargs=args});
}

static Node *ast_if_stmt(Node *cond, size_t n_if, size_t n_else, Node **ifbody, Node **elsebody)
{
	return makeNode(&(Node){AST_IF_STMT, .if_cond=cond, .n_if_stmts=n_if, .n_else_stmts=n_else, .if_body=ifbody, .else_body=elsebody});
}

static Node *ast_while_stmt(Node *cond, size_t n_while, Node **whilebody)
{
	return makeNode(&(Node){AST_WHILE_STMT, .while_cond=cond, .n_while_stmts=n_while, .while_body=whilebody});
}

static Node *ast_for_stmt(Node *it, Node *enumerable, size_t n_for, Node **forbody)
{
	return makeNode(&(Node){AST_FOR_STMT, .for_iterator=it, .for_enum=enumerable, .n_for_stmts=n_for, .for_body=forbody});
}

static Node *ast_ret_stmt(Node *ret_val)
{
	return makeNode(&(Node){AST_RETURN_STMT, .retval=ret_val});
}

static int next_token(int tclass)
{
	Token_type *tok = get();
	if (tok->class != tclass) {
		unget();
		return 0;
	}
	return 1;
}

static void expect(int tclass, const char *msg)
{
	Token_type *tok = get();
	if (tok->class != tclass) {
		if (msg[0] == 0) {
			char errmsg[128];
			if (tclass < 256) {
				if (tok->class < 256) {
					sprintf(errmsg, "%c expected but got %c.", tclass, tok->class);
					c_error(errmsg, tok->line);
				} else {
					sprintf(errmsg, "%c expected but got %s.", tclass, tokenclassToString(tok->class));
					c_error(errmsg, tok->line);
				}
			} else {
				if (tok->class < 256) {
					sprintf(errmsg, "%s expected but got %c.", tokenclassToString(tclass), tok->class);
					c_error(errmsg, tok->line);
				} else {
					sprintf(errmsg, "%s expected but got %s.", tokenclassToString(tclass), tokenclassToString(tok->class));
					c_error(errmsg, tok->line);
				}
			}
		} else {
			c_error(msg, tok->line);
		}

		exit(1);
	}
}

static Node *read_global_expr()
{
	Token_type *tok = get();
	if (!strcmp(tok->repr, "fn")) {
		return read_fn_def();
	} else if (!strcmp(tok->repr, "record")) {
		return read_record_def();
	} else {
		if (tok->class != EoF) {
			c_error("Unexpected global expression.", tok->line);
		}
		return NULL;
	}
}

static Node *read_record_def()
{
	Token_type *tok = get();

	if (tok->class == IDENTIFIER) {
		char *label = malloc(strlen(tok->repr) + 1);
		strcpy(label, tok->repr);

		expect('{', "");

		Node **fields = NULL;
		size_t n_fields = 0;

		for (;;) {
			Node *f = read_declaration_expr(0);
			if (f == NULL) {
				expect('}', "Invalid expression in record fields declaration.");
				expect(';', "");
				return ast_record_def(label, n_fields, fields);
			}

			fields = realloc(fields, sizeof(Node *) * (n_fields+1));
			fields[n_fields] = f;
			n_fields++;

			expect(';', "");
		}
	} else {
		return NULL;
	}
}

static Node *read_fn_def()
{
	Token_type *tok = get();
	if (tok->class == IDENTIFIER) {
		char *flabel = malloc(strlen(tok->repr) + 1);
		strcpy(flabel, tok->repr);

		expect('(', "");

		size_t params_n;
		Node **params = read_fn_parameters(&params_n);

		expect(ARROW_OP, "");

		expect(TYPE_SPECIFIER, "-> operator must be followed by valid type specifier.");

		int ret_type = get_type_specifier(curr());
		int array_dims = 0;

		for (;;) {
			tok = get();
			if (tok->class == '[') {
				expect(']', "Closing ']' expected.");
				array_dims++;
			} else {
				unget();
				break;
			}
		}

		expect('{', "");

		size_t stmts_n;
		Node **body = read_fn_body(&stmts_n);

		expect('}', "");

		return ast_funcdef(flabel, ret_type, array_dims, params_n, stmts_n, params, body);
	}

	return NULL;
}

static Node **read_fn_parameters(size_t *n)
{
	Node **params = malloc(0);
	size_t params_sz = 0;

	Token_type *tok;
	for (;;) {
		Node *param = read_declaration_expr(1);
		
		if (param == NULL) {
			expect(')', "Closing ')' or type specifier expected.");
			*n = 0;
			return NULL;
		}
		params = realloc(params, sizeof(Node *) * (params_sz+1));
		params[params_sz] = param;
		params_sz++;

		tok = get();
		if (tok->class != ',') {
			if (tok->class == ')') {
				break;
			}
			c_error("',' or ')' expected.", tok->line);
			break;
		}
	}
	if (params_sz == 0) {
		*n = 0;
		free(params);
		return NULL;
	} else {
		*n = params_sz;
		return params;
	}
}

static Node **read_fn_body(size_t *n)
{
	Node **body = malloc(0);
	size_t body_sz = 0;

	for (;;) {
		Node *n = read_secondary_expr();
		if (n == NULL) {
			break;
		}
		
		if (!is_stmt_node(n))
		{
			expect(';', "Missing ';'.");
		}

		body = realloc(body, sizeof(body) * (body_sz + 1));
		body[body_sz] = n;
		body_sz++;
	}

	*n = body_sz;
	return body;
}

static Node *read_primary_expr()
{
	Token_type *tok = get();

	if (tok->class == '(') {
		Node *r = read_expr();
		//next();
		expect(')', "Expected ')' at end of expression.");
		return r;
	}

	if (tok->class == '[') {
		Node *r = read_array_expr();
		return r;
	}

	switch (tok->class) {
		case INT: return read_int(tok);
		case FLOAT: return read_float(tok);
		case IDENTIFIER: return read_ident(tok, 0);
		case STRING: return read_string(tok);
		case BOOL: return read_bool(tok);
		default: 
			     unget();
			     return NULL;
	}
}

static Node *read_secondary_expr()
{
	Node *r = read_stmt();
	if (r == NULL) {
		r = read_declaration_expr(0);
	}
	if (r == NULL) {
		r = read_expr();
	}

	return r;
}

static Node *read_expr()
{
	Node *r = read_assignment_expr();

	if (r == NULL) {
		unget();
		r = read_fn_call();
	}

	return r;
}

static Node *read_stmt()
{
	Token_type *tok = get();

	switch (is_keyword(tok))
	{
		case KEYWORD_IF:
			return read_if_stmt();
		case KEYWORD_WHILE:
			return read_while_stmt();
		case KEYWORD_FOR:
			return read_for_stmt();
		case KEYWORD_RETURN:
			return read_return_stmt();
		default:
			unget();
			return NULL;
	}
}

static Node *read_if_stmt()
{
	expect('(', "'(' expected after keyword 'if'.");

	Node *cond = read_relational_expr();

	expect(')', "')' expected after keyword 'if'.");

	expect('{', "'{' expected after keyword 'if'."); 

	Node **if_body = malloc(0);
	size_t if_body_sz = 0;

	for (;;) {
		Node *n = read_secondary_expr();

		if (n == NULL) {
			break;
		}
		
		if_body = realloc(if_body, sizeof(Node *) * (if_body_sz + 1));
		if_body[if_body_sz] = n;
		if_body_sz++;

		if (!is_stmt_node(n)) {
			expect(';', "Missing ';'.");
		}
	}

	expect('}', "'}' expected after keyword 'if'.");

	Node **else_body = malloc(0);
	size_t else_body_sz = 0;

	Token_type *tok = get();
	if (!strcmp("else", tok->repr)) {
		expect('{', "'{' expected after keyword 'else'.");

		for (;;) {
			Node *n = read_secondary_expr();

			if (n == NULL) {
				break;
			}
			
			else_body = realloc(else_body, sizeof(Node *) * (else_body_sz + 1));
			else_body[else_body_sz] = n;
			else_body_sz++;

			if (!is_stmt_node(n)) {
				expect(';', "Missing ';'.");
			}
		}

		expect('}', "'}' expected after keyword 'else'.");
	} else {
		unget();
		else_body = NULL;
	}

	return ast_if_stmt(cond, if_body_sz, else_body_sz, if_body, else_body);
}

static Node *read_while_stmt()
{
	expect('(', "'(' expected after keyword 'while'.");

	Node *cond = read_relational_expr();

	expect(')', "')' expected after keyword 'while'.");

	expect('{', "'{' expected after keyword 'while'.");

	Node **while_body = malloc(0);
	size_t while_body_sz = 0;

	for (;;) {
		Node *n = read_secondary_expr();

		if (n == NULL) {
			break;
		}
		
		while_body = realloc(while_body, sizeof(Node *) * (while_body_sz + 1));
		while_body[while_body_sz] = n;
		while_body_sz++;

		if (!is_stmt_node(n)) {
			expect(';', "Missing ';'.");
		}
	}

	expect('}', "'}' expected after keyword 'while'.");

	return ast_while_stmt(cond, while_body_sz, while_body);
}

static Node *read_for_stmt()
{
	expect('(', "'(' expected after keyword 'for'.");

	Node *iterator = read_declaration_expr(1);

	expect(':', "':' operator expected in iterator definition.");
	
	Node *enumerable = read_enumerable_expr();
	if (enumerable == NULL) {
		c_error("Expected enumerable expression in 'for' statement.", curr()->line);
	}

	expect(')', "'(' expected after keyword 'for'.");

	expect('{', "'{' expected after keyword 'for'.");

	Node **for_body = malloc(0);
	size_t for_body_sz = 0;
	for (;;) {
		Node *n = read_secondary_expr();

		if (n == NULL) {
			break;
		}
		
		for_body = realloc(for_body, sizeof(Node *) * (for_body_sz + 1));
		for_body[for_body_sz] = n;
		for_body_sz++;

		if (!is_stmt_node(n)) {
			expect(';', "Missing ';'.");
		}
	}

	expect('}', "'}' expected after keyword 'for'.");

	return ast_for_stmt(iterator, enumerable, for_body_sz, for_body);
}

static Node *read_return_stmt()
{
	Node *n = read_expr();
	
	if (n == NULL) {
		if (curr()->class != ';') {
			c_error("Missing ';'.", prev()->line);
		}
		next();
	} else {
		expect(';', "Missing ';'.");
	}

	return ast_ret_stmt(n);
}

static Node *read_fn_call()
{
	Token_type *tok = get();
	if (tok->class == IDENTIFIER) {
		char *label = malloc(strlen(tok->repr) + 1);
		strcpy(label, tok->repr);

		if (next_token('(')) {
			Node **args = malloc(0);
			size_t args_sz = 0;
			for (;;) {
				Node *arg = read_expr();
				if (arg == NULL) {
					tok = get();
					break;
				}

				args = realloc(args, sizeof(Node *) * (args_sz+1));
				args[args_sz] = arg;
				args_sz++;

				tok = get();
				if (tok->class != ',') {
					break;
				}
			}
			if (tok->class != ')') {
				c_error("Closing ')' expected.", tok->line);
				free(label);
				free(args);
				return NULL;
			}

			return ast_funccall(label, args_sz, args);
		}
		free(label);
	}

	return NULL;
}

static Node *read_assignment_expr()
{
	Node *r = read_relational_expr();

	if (curr()->class == '=') {
		next();
		r = ast_binop('=', r, read_assignment_expr());
	} else if (curr()->class == ADD_ASSIGN) {
		next();
		r = ast_binop(ADD_ASSIGN, r, read_assignment_expr());
	} else if (curr()->class == SUB_ASSIGN) {
		next();
		r = ast_binop(SUB_ASSIGN, r, read_assignment_expr());
	} else if (curr()->class == MUL_ASSIGN) {
		next();
		r = ast_binop(MUL_ASSIGN, r, read_assignment_expr());
	} else if (curr()->class == DIV_ASSIGN) {
		next();
		r = ast_binop(DIV_ASSIGN, r, read_assignment_expr());
	}

	return r;
}

static Node *read_relational_expr()
{
	Node *r = read_additive_expr();

	for (;;) {
		switch (curr()->class)
		{
			case '>':
				next();
				r = ast_binop('>', r, read_relational_expr());
				break;
			case '<':
				next();
				r = ast_binop('<', r, read_relational_expr());
				break;
			case EQ:
				next();
				r = ast_binop(EQ, r, read_relational_expr());
				break;
			case NE:
				next();
				r = ast_binop(NE, r, read_relational_expr());
				break;
			case GE:
				next();
				r = ast_binop(GE, r, read_relational_expr());
				break;
			case LE:
				next();
				r = ast_binop(LE, r, read_relational_expr());
				break;
			default:
				return r;
		}
	}
}

static Node *read_declaration_expr(int no_assignment)
{
	Token_type *tok = get();
	int type;
	char *rlabel = NULL;
	int array_dims = 0;
	int *array_size = NULL;
	if (tok->class == TYPE_SPECIFIER) {
		type = get_type_specifier(tok);
		if (type == TYPE_RECORD) {
			tok = get();
			if (tok->class == IDENTIFIER) {
				rlabel = malloc(strlen(tok->repr) + 1);
				strcpy(rlabel, tok->repr);
			} else {
				c_error("Specifier 'record' must be followed by valid label.");
			}
		}
		tok = get();
		if (tok->class == IDENTIFIER) {
			char *label = malloc(strlen(tok->repr) + 1);
			strcpy(label, tok->repr);

			for (;;) {
				if (next_token('[')) {
					array_size = realloc(array_size, sizeof(int) * (array_dims+1));
					tok = get();
					if (tok->class == INT) {
						char *end;
						#define s (tok->repr)
						array_size[array_dims] = strncasecmp(s, "0b", 2) ? strtol(s, &end, 0) : strtol(s, &end, 2);
						#undef s
						expect(']', "");
					} else if (tok->class == ']') {
						array_size[array_dims] = 0;
					}
					array_dims += 1;
				} else {
					break;
				}
			}

			Node *lhs = ast_decl(label, type, rlabel, array_dims, array_size);

			tok = get();
			if (tok->class == '=') {
				if (!no_assignment) {
					return ast_binop('=', lhs, read_expr());
				} else {
					c_error("No variable assignment in function definitions or for-statements.", tok->line);
				}
			} else {
				unget();
				return lhs;
			}
		} else {
			c_error("Invalid declaration expression.", tok->line);
		}
	}

	unget();
	return NULL;
}

static Node *read_field_access()
{
	Node *r = read_primary_expr();

	for (;;) {
		if (curr()->class == '.' && r->type == AST_IDENT) {
			next();
			Token_type *tok = get();
			r = ast_field_access(r, read_ident(tok, 1));
		} else {
			return r;
		}
	}
}

static Node *read_multiplicative_expr()
{
	Node *r = read_field_access();

	for (;;) {
		if (curr()->class == '*') {
			next();
			r = ast_binop('*', r, read_primary_expr());
		} else if (curr()->class == '/') {
			next();
			r = ast_binop('/', r, read_primary_expr());
		} else {
			return r;
		}
	}
}

static Node *read_additive_expr()
{
	Node *r = read_multiplicative_expr();
	for (;;) {
		if (curr()->class == '+') {
			next();
			r = ast_binop('+', r, read_multiplicative_expr());
		} else if (curr()->class == '-') {
			next();
			r = ast_binop('-', r, read_multiplicative_expr());
		} else {
			return r;
		}
	}
}

static Node *read_indexed_array()
{
	Token_type *tok = get();

	char *label = malloc(strlen(tok->repr) + 1);
	strcpy(label, tok->repr);
	label[strlen(tok->repr)] = '\0';

	Node *index;
	Node **index_array = NULL;
	int index_array_sz = 0;
	for (;;) {
		if (!next_token('[')) {
			break;
		}

		index = read_expr();

		if (index == NULL || index->type == AST_STRING || index->type == AST_ARRAY
		   || index->type == AST_FLOAT) 
		{
			c_error("Invalid array index.", tok->line);
		}

		expect(']', "");

		index_array = realloc(index_array, sizeof(Node *) * (index_array_sz+1));
		index_array[index_array_sz] = index;
		index_array_sz++;
	}

	return ast_indexed_array(label, index_array, index_array_sz);
}

static Node *read_enumerable_expr()
{
	Node *r;
	Token_type *tok = get();
	if (tok->class == '[') {
		r = read_array_expr();
	} else {
		r = read_ident(tok, 0);
	}

	return r;
}

static Node *read_array_expr()
{
	Node **array = malloc(0);
	size_t array_sz = 0;
	Token_type *tok;
	for (;;) {
		Node *e = read_expr();
		if (e == NULL) {
			break;
		}

		array = realloc(array, sizeof(Node *) * (array_sz + 1));
		array[array_sz] = e;
		array_sz++;

		tok = get();
		if (tok->class != ',') {
			unget();
			break;
		}
	}

	expect(']', "Unexpected token in array expression.");

	return ast_arraytype(array_sz, array);
}

static Node *read_int(Token_type *tok)
{
	char *s = tok->repr;
	char *end;
	long v = strncasecmp(s, "0b", 2) ? strtol(s, &end, 0) : strtol(s, &end, 2);

	return ast_inttype(v);
}

static Node *read_float(Token_type *tok)
{
	char *s = tok->repr;
	char *end;
	float v = strtof(s, &end);

	return ast_floattype(v);
}

static Node *read_string(Token_type *tok)
{
	char *s = malloc(strlen(tok->repr) + 1);
	strcpy(s, tok->repr);
	s[strlen(tok->repr)] = '\0';

	return ast_stringtype(s);
}

static Node *read_bool(Token_type *tok)
{
	if (!strcmp(tok->repr, "true")) {
		return ast_booltype(1);
	} else {
		return ast_booltype(0);
	}
}

static Node *read_ident(Token_type *tok, int no_brackets)
{
	char *s;
	if (!no_brackets) {
		if (curr()->class == '(') {
			unget();
			return read_fn_call();
		} 
	}

	if (curr()->class == '[') {
		unget();
		return read_indexed_array();
	}

	s = malloc(strlen(tok->repr) + 1);
	strcpy(s, tok->repr);

	return ast_identtype(s);
}

static Node *find_function(char *name)
{
	for (int i = 0; i < global_function_count; i++) {
		if(!strcmp(global_functions[i]->flabel, name)) {
			return global_functions[i];
		}
	}

	return NULL;
}

static Node *cfg_aux_node()
{
	return makeNode(&(Node){CFG_AUXILIARY_NODE});
}

static Node *cfg_join_node()
{
	return makeNode(&(Node){CFG_JOIN_NODE});
}

static Node *last_node;
static void thread_expression();

static Node *thread_ast()
{
	Node tmp;

	last_node = &tmp;

	for (int i = 0; i < global_function_count; i++) {
		if (!strcmp(global_functions[i]->flabel, "main")) {
			thread_expression(global_functions[i]);
			last_node->successor = NULL;
			return tmp.successor;
		}
	}
	
	c_error("No main function defined.");
	return 0;
}

static void thread_block(Node **block, size_t block_size)
{
	for (int i = 0; i < block_size; i++) {
		thread_expression(block[i]);
		if (block[i]->type == AST_RETURN_STMT) {
			break;	// dead-code elimination
		}
	}
}

static void thread_expression(Node *expr)
{
	Node *aux;

	switch (expr->type)
	{
		case AST_FUNCTION_DEF:
			thread_block(expr->fnbody, expr->n_stmts);
			break;
		case AST_IDENT:
		case AST_INT:
		case AST_FLOAT:
		case AST_STRING:
		case AST_BOOL:
		case AST_FIELD_ACCESS:
		case AST_DECLARATION:
			last_node->successor = expr;
			last_node = expr;
			break;
		case AST_ARRAY:
			last_node->successor = expr;
			last_node = expr;
			for (int i = expr->array_size-1; i >= 0; i--) {
				thread_expression(expr->array_elems[i]);
			}
			break;
		case AST_IDX_ARRAY:
			for (int i = expr->ndim_index-1; i >= 0; i--) {
				thread_expression(expr->index_values[i]);
			}
			last_node->successor = expr;
			last_node = expr;
			break;
		case AST_ADD:
		case AST_SUB:
		case AST_MUL:
		case AST_DIV:
		case AST_ASSIGN:
		case AST_ADD_ASSIGN:
		case AST_SUB_ASSIGN:
		case AST_MUL_ASSIGN:
		case AST_DIV_ASSIGN:
		case AST_GT:
		case AST_LT:
		case AST_EQ:
		case AST_NE:
		case AST_GE:
		case AST_LE:
			thread_expression(expr->left);
			thread_expression(expr->right);
			last_node->successor = expr;
			last_node = expr;
			break;
		case AST_IF_STMT:
			thread_expression(expr->if_cond);
			last_node->successor = expr;

			Node *end_if = cfg_join_node();
			aux = cfg_aux_node();

			last_node = aux;
			thread_block(expr->if_body, expr->n_if_stmts);
			
			expr->successor = aux->successor;
			last_node->successor = end_if;

			if (expr->n_else_stmts > 0) {
				last_node = aux;
				thread_block(expr->else_body, expr->n_else_stmts);

				expr->false_successor = aux->successor;
				last_node->successor = end_if;
			} else {
				expr->false_successor = NULL;
			}

			last_node = end_if;

			free(aux);

			break;
		case AST_FUNCTION_CALL:
			Node *func = find_function(expr->call_label);
			if (func == NULL) {
				char *msg = malloc(128);
				sprintf(msg, "No function with name '%s' was found.", expr->call_label);
				c_error(msg, -1);
				free(msg);
			}
			last_node->successor = expr;
			last_node = expr;
			thread_expression(func);
			break;
		case AST_RETURN_STMT:
			last_node->successor = expr;
			last_node = expr;
			if (expr->retval == NULL) {
				aux = cfg_aux_node();
				last_node->successor = aux;
				last_node = aux;
			} else {
				thread_expression(expr->retval);
			}
			break;
		case AST_WHILE_STMT:
			thread_expression(expr->while_cond);
			last_node->successor = expr;

			aux = cfg_aux_node();
			last_node = aux;

			thread_block(expr->while_body, expr->n_while_stmts);

			expr->while_true_successor = aux->successor;

			last_node = expr;

			free(aux);

			break;
		case AST_FOR_STMT:
			last_node->successor = expr;

			aux = cfg_aux_node();
			last_node = aux;
			
			thread_block(expr->for_body, expr->n_for_stmts);

			expr->for_loop_successor = aux->successor;

			last_node = expr;

			free(aux);

			break;
	}
}

static void push(void ***top, void *n)
{
	(*top)++;
	**top = n;
}

static void *pop(void ***top)
{
	void *n = **top;
	(*top)--;

	return n;
}

typedef struct {
	char *var_name;
	int status;	// 0 -> Uninitialized, 1 -> Initialized
	int type;
	union {
		int ival;
		char *sval;
		float fval;
		int bval;
	};
} ValPropPair;

static ValPropPair *makeValPropPair(ValPropPair *tmp)
{
	ValPropPair *r = malloc(sizeof(ValPropPair));

	*r = *tmp;

	return r;
}

static ValPropPair *searchValueStack(ValPropPair ***stack, char *key)
{
	ValPropPair **current = *stack;
	while (*current != NULL) {
		if (!strcmp((*current)->var_name, key)) { 
			return *current; 
		} else {
			current--;
		}
	}

	return NULL;
}

static void sym_interpret(Node *cfg)
{
	Node **opstack = calloc(512, sizeof(Node *));
	Node **optop = opstack - 1;
	*optop = 0;

	ValPropPair **valstack = calloc(512, sizeof(ValPropPair *));
	ValPropPair **valtop = valstack - 1;
	*valtop = 0;

	Node *last = cfg;

	while (last != NULL) {
		interpret_expr(last, &optop, &valtop);
		last = last->successor;
	}

#if SYM_OUTPUT
	while (*valtop != NULL) {
		printf("NAME: %s | STATUS: %s | TYPE: %s | VALUE: ", (*valtop)->var_name,
			(*valtop)->status ? "Initialized" : "Uninitialized", datatypeToString((*valtop)->type));
		if ((*valtop)->status) {
			switch ((*valtop)->type)
			{
				case TYPE_INT:
					printf("%d\n", (*valtop)->ival);
					break;
				case TYPE_STRING:
					printf("%s\n", (*valtop)->sval);
					break;
				case TYPE_FLOAT:
					printf("%f\n", (*valtop)->fval);
					break;
				case TYPE_BOOL:
					printf("%s\n", (*valtop)->bval ? "true" : "false");
					break;
			}
		} else {
			printf("/\n");
		}

		valtop--;
	}
#endif
}

static void checkDataType(ValPropPair *pair, int type)
{
	if (pair->type != type) {
		char *msg = malloc(128);
		sprintf(msg, "Assignment of type '%s' to variable %s of type '%s'.", datatypeToString(type), pair->var_name, datatypeToString(pair->type));
		c_error(msg, -1);
	}
}

static void interpret_assignment_expr(Node *expr, Node ***opstack, ValPropPair ***valstack)
{
	Node *rhs = pop(opstack);
	Node *lhs = pop(opstack);

	if (lhs->type == AST_DECLARATION || lhs->type == AST_IDENT) {
		ValPropPair *pair;
		if (lhs->type == AST_DECLARATION) {
			pair = searchValueStack(valstack, lhs->vlabel);
			if (pair == NULL) {
				char *msg = malloc(128);
				sprintf(msg, "No variable with name %s found.", lhs->vlabel);
				c_error(msg, -1);
				free(msg);
			}
		} else {
			pair = searchValueStack(valstack, lhs->name);
			if (pair == NULL) {
				char *msg = malloc(128);
				sprintf(msg, "No variable with name %s found.", lhs->name);
				c_error(msg, -1);
			}
		}

		switch (rhs->type)
		{
			case AST_INT:
				checkDataType(pair, TYPE_INT);
				pair->ival = rhs->ival;
				break;
			case AST_STRING:
				checkDataType(pair, TYPE_STRING);
				pair->sval = rhs->sval;
				break;
			case AST_FLOAT:
				checkDataType(pair, TYPE_FLOAT);
				pair->fval = rhs->fval;
				break;
			case AST_BOOL:
				checkDataType(pair, TYPE_BOOL);
				pair->bval = rhs->bval;
				break;
			case AST_IDENT:
				ValPropPair *ident_pair = searchValueStack(valstack, rhs->name);
				if (ident_pair != NULL) {
					switch (ident_pair->type)
					{
						case TYPE_INT:
							checkDataType(pair, TYPE_INT);
							pair->ival = ident_pair->ival;
							break;
						case TYPE_STRING:
							checkDataType(pair, TYPE_STRING);
							pair->sval = ident_pair->sval;
							break;
						case TYPE_FLOAT:
							checkDataType(pair, TYPE_FLOAT);
							pair->fval = ident_pair->fval;
							break;
						case TYPE_BOOL:
							checkDataType(pair, TYPE_BOOL);
							pair->bval = ident_pair->bval;
							break;
					}
					break;
				} else {
					char *msg = malloc(128);
					sprintf(msg, "No variable with name %s found.", rhs->name);
					c_error(msg, -1);
				}
			default:
				c_error("Right-hand side of '=' invalid.", -1);
		}
		pair->status = 1;
	} else {
		c_error("Left-hand side of '=' invalid.", -1);
	}
}

static void interpret_declaration_expr(Node *expr, Node ***opstack, ValPropPair ***valstack)
{
	ValPropPair *pair = makeValPropPair(&(ValPropPair){expr->vlabel, 0, expr->vtype});

	push(valstack, pair);
	push(opstack, expr);
}

static void interpret_binary_expr(int op, Node ***opstack, ValPropPair ***valstack)
{
	Node *r_operand = pop(opstack);
	Node *l_operand = pop(opstack);

	if ((r_operand->type != l_operand->type) && !( (r_operand->type == AST_IDENT || r_operand->type == AST_IDX_ARRAY) 
				|| (l_operand->type == AST_IDENT || l_operand->type == AST_IDX_ARRAY) )) {
				c_error("Operands of binary operation must be of the same type.", -1);
	}

	switch (l_operand->type)
	{
		case AST_INT:
			if (r_operand->type != AST_INT) {
				;
			} else {
				switch (op)
				{
					case AST_ADD:
						push(opstack, ast_inttype(l_operand->ival + r_operand->ival));
						break;
					case AST_SUB:
						push(opstack, ast_inttype(l_operand->ival - r_operand->ival));
						break;
					case AST_MUL:
						push(opstack, ast_inttype(l_operand->ival * r_operand->ival));
						break;
					case AST_DIV:
						push(opstack, ast_inttype(l_operand->ival / r_operand->ival));
						break;
					case AST_GT:
						push(opstack, ast_booltype(l_operand->ival > r_operand->ival));
						break;
					case AST_LT:
						push(opstack, ast_booltype(l_operand->ival < r_operand->ival));
						break;
					case AST_EQ:
						push(opstack, ast_booltype(l_operand->ival == r_operand->ival));
						break;
					case AST_NE:
						push(opstack, ast_booltype(l_operand->ival != r_operand->ival));
						break;
					case AST_GE:
						push(opstack, ast_booltype(l_operand->ival >= r_operand->ival));
						break;
					case AST_LE:
						push(opstack, ast_booltype(l_operand->ival <= r_operand->ival));
						break;
				}
			}
			break;
		case AST_STRING:
			if (r_operand->type != AST_STRING) {
				;
			} else {
				switch (op)
				{
					case AST_ADD:
						char *comp_string = malloc(strlen(l_operand->sval) + strlen(r_operand->sval) + 1);
						strcpy(comp_string, l_operand->sval);
						push(opstack, ast_stringtype(strcat(comp_string, r_operand->sval)));
						break;
					case AST_EQ:
						push(opstack, ast_booltype(!strcmp(l_operand->sval, r_operand->sval)));
						break;
					default:
						c_error("Illegal operation on value with type 'string'.", -1);
						break;
				}
			}
			break;
	}
}

static void interpret_expr(Node *expr, Node ***opstack, ValPropPair ***valstack)
{
	switch (expr->type)
	{
		case AST_INT:
			push(opstack, expr);
			break;
		case AST_STRING:
			push(opstack, expr);
			break;
		case AST_FLOAT:
			push(opstack, expr);
			break;
		case AST_BOOL:
			push(opstack, expr);
			break;
		case AST_ARRAY:
			push(opstack, expr);
			break;
		case AST_IDENT:
			push(opstack, expr);
			break;
		case AST_IDX_ARRAY:
			push(opstack, expr);
			break;
		case AST_ADD:
		case AST_SUB:
		case AST_MUL:
		case AST_DIV:
		case AST_GT:
		case AST_LT:
		case AST_EQ:
		case AST_NE:
		case AST_GE:
		case AST_LE:
			interpret_binary_expr(expr->type, opstack, valstack);
			break;
		case AST_DECLARATION:
			interpret_declaration_expr(expr, opstack, valstack);
			break;
		case AST_ASSIGN:
			interpret_assignment_expr(expr, opstack, valstack);
			break;
	}
}
