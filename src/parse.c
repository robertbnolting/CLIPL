#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "parse.h"
#include "lex.h"

static int pos;

#define curr()	(&Token_stream[pos])
#define get()	(&Token_stream[pos++])
#define next()	(pos++)
#define unget()	(pos--)
#define peek()	(&Token_stream[pos+1])
#define prev()	(&Token_stream[pos-1])

static int expect();

static Node *read_global_expr();
static Node *read_primary_expr();
static Node *read_expr();
static Node *read_assignment_expr();
static Node *read_additive_expr();
static Node *read_multiplicative_expr();
static Node *read_int();
static Node *read_string();
static Node *read_ident();
static Node *read_fn_def();
static Node **read_fn_parameters();
static Node **read_fn_body();

static void traverse();
static char *list_params();
static void list_stmts();

void parser_init()
{
	pos = 0;

	Node **node_array = (Node **) malloc(1);
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

	for (int i = 0; i < array_len; i++) {
		traverse(node_array[i]);
		printf("\n");
	}
}

static int is_type_specifier(Token_type *tok)
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
	} else {
		return 0;
	}
}

static void traverse(Node *root)
{
	switch (root->type)
	{
		case AST_INT:
			printf("(INT: %d) ", root->ival);
			break;
		case AST_STRING:
			printf("(STRING: %s) ", root->sval);
			break;
		case AST_IDENT:
			printf("(IDENT: %s) ", root->name);
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
		case AST_FUNCTION:
			printf("(FUNCTION: %s | RETURNS: %d | PARAMS: %s | BODY: {\n", root->flabel, root->return_type, list_params(root->n_params, root->fnparams));
			list_stmts(root->n_stmts, root->fnbody);
			printf("}");
			printf(")");
			break;
	}
}

static void list_stmts(size_t n, Node **body)
{
	for (size_t i = 0; i < n; i++) {
		traverse(body[i]);
		printf("\n");
	}
}

static char *list_params(size_t n, Node **params)
{
	char *ret = (char *) malloc(1);
	size_t ret_size = 0;
	for (size_t i = 0; i < n; i++) {
		ret = realloc(ret, ret_size + strlen(params[i]->name) + 2);
		strcpy(&ret[ret_size], params[i]->name);
		ret_size += strlen(params[i]->name);
		strcpy(&ret[ret_size], ", ");
		ret_size += 2;
	}

	return ret;
}

static Node *makeNode(Node *tmp)
{
	Node *r = (Node *) malloc(sizeof(Node));

	*r = *tmp;

	return r;
}

static Node *ast_inttype(int val)
{
	return makeNode(&(Node){AST_INT, .ival = val});
}

static Node *ast_identtype(char *n)
{
	return makeNode(&(Node){AST_IDENT, .name = n});
}

static Node *ast_stringtype(char *val)
{
	return makeNode(&(Node){AST_STRING, .sval = val});
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
		default:
			return NULL;
	}
}

static Node *ast_fntype(char *label, int ret_type, size_t params_n, size_t stmts_n, Node **params, Node **body)
{
	return makeNode(&(Node){AST_FUNCTION, .flabel=label, .return_type=ret_type, .n_params=params_n, .n_stmts=stmts_n, .fnparams=params, .fnbody=body});
}

static int expect(int tclass)
{
	Token_type *t = prev();
	if (t->class != tclass) {
		return 0;
	}
	
	return 1;
}

static Node *read_global_expr()
{
	Token_type *tok = get();
	if (!strcmp(tok->repr, "fn")) {
		return read_fn_def();
	} else {
		if (tok->class != EoF) {
			printf("Error: Unexpected global expression.\n");
		}
		return NULL;
	}
}

static Node *read_fn_def()
{
	Token_type *tok = get();
	if (tok->class == IDENTIFIER) {
		char *flabel = (char *) malloc(strlen(tok->repr));
		strcpy(flabel, tok->repr);
		next();

		if (!expect('(')) {
			printf("Error: '(' expected.\n");
			return NULL;
		}

		size_t params_n;
		Node **params = read_fn_parameters(&params_n);

		if (!expect(ARROW_OP)) {
			printf("Error: Return type of function must be specified.\n");
			return NULL;
		}

		int ret_type;
		if (!(ret_type = is_type_specifier(curr()))) {
			printf("Error: -> operator must be followed by valid type specifier.\n");
			return NULL;
		}

		next();

		if (curr()->class != '{') {
			printf("Error: '{' expected.\n");
			return NULL;
		}

		next();

		size_t stmts_n;
		Node **body = read_fn_body(&stmts_n);
		next();

		if (!expect('}')) {
			printf("Error: '}' expected.\n");
			return NULL;
		}

		return ast_fntype(flabel, ret_type, params_n, stmts_n, params, body);
	}

	return NULL;
}

static Node **read_fn_parameters(size_t *n)
{
	Node **params = (Node **) malloc(1);
	size_t params_sz = 0;

	Token_type *tok;
	for (;;) {
		tok = get();
		if (is_type_specifier(tok)) {
			tok = get();
			if (tok->class != IDENTIFIER) {
				printf("Error: Identifier expected after type specifier.\n");
				break;
			}
			char *pname = (char *) malloc(strlen(tok->repr));
			strcpy(pname, tok->repr);
			Node *param = ast_identtype(pname);
			params = realloc(params, sizeof(Node *) * (params_sz+1));
			params[params_sz] = param;
			params_sz++;
		} else {
			if (tok->class != ')') {
				printf("Error: Type specifier expected.\n");
			}
			break;
		}
		tok = get();
		if (tok->class != ',') {
			if (tok->class == ')') {
				break;
			}
			printf("Error: ',' or ')' expected.\n");
			break;
		}
	}
	*n = params_sz;
	next();
	return params;
}

static Node **read_fn_body(size_t *n)
{
	Node **body = (Node **) malloc(1);
	size_t body_sz = 0;

	for (;;) {
		Node *n = read_primary_expr();
		if (n == NULL) {
			break;
		}
		Token_type *tok = get();
		if (tok->class != ';') {
			printf("Error: Missing ';'.\n");
			return NULL;
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
	
	if (expect('(')) {
		Node *r = read_expr();
		next(); if (!expect(')')) {
			printf("Error: Unexpected token.\n");
		}
		return r;
	}

	switch (tok->class) {
		case INT: return read_int(tok);
		case IDENTIFIER: return read_ident(tok);
		case STRING: return read_string(tok);
		default: 
			     unget();
			     return NULL;
	}
}

static Node *read_expr()
{
	Node *r = read_assignment_expr();

	return r;
}

static Node *read_assignment_expr()
{
	Node *r = read_additive_expr();

	if (curr()->class == '=') {
		pos++;
		r = ast_binop('=', r, read_assignment_expr());
	}

	return r;
}

static Node *read_multiplicative_expr()
{
	Node *r = read_primary_expr();

	for (;;) {
		if (curr()->class == '*') {
			pos++;
			r = ast_binop('*', r, read_primary_expr());
		} else if (curr()->class == '/') {
			pos++;
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
			pos++;
			r = ast_binop('+', r, read_multiplicative_expr());
		} else if (curr()->class == '-') {
			pos++;
			r = ast_binop('-', r, read_multiplicative_expr());
		} else {
			return r;
		}
	}
}

static Node *read_int(Token_type *tok)
{
	char *s = tok->repr;
	char *end;
	long v = strncasecmp(s, "0b", 2) ? strtol(s, &end, 0) : strtol(s, &end, 2);

	return ast_inttype(v);
}

static Node *read_string(Token_type *tok)
{
	char *s = (char *) malloc(strlen(tok->repr));
	strcpy(s, tok->repr);

	return ast_stringtype(s);
}

static Node *read_ident(Token_type *tok)
{
	char *s = (char *) malloc(strlen(tok->repr));
	strcpy(s, tok->repr);

	return ast_identtype(s);
}
