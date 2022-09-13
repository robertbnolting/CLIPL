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
static Node *read_secondary_expr();
static Node *read_expr();

static Node *read_stmt();
static Node *read_if_stmt();
static Node *read_while_stmt();
static Node *read_for_stmt();
static Node *read_return_stmt();

static Node *read_assignment_expr();
static Node *read_relational_expr();
static Node *read_additive_expr();
static Node *read_multiplicative_expr();
static Node *read_declaration_expr();
static Node *read_ident();
static Node *read_int();
static Node *read_float();
static Node *read_string();
static Node *read_array_expr();
static Node *read_fn_def();
static Node *read_fn_call();
static Node **read_fn_parameters();
static Node **read_fn_body();

static void traverse();
static char *list_nodearray();
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
		printf("\n\n");
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

static void traverse(Node *root)
{
	if (root == NULL) {
		return;
	}

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
		case AST_ARRAY:
			printf("(ARRAY: [%s]) ", list_nodearray(root->array_size, root->array_elems));
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
			printf("(DECLARATION: %s | TYPE: %d) ", root->vlabel, root->vtype);
			break;
		case AST_FUNCTION_DEF:
			printf("(FUNCTION DEFINITION: %s | RETURNS: %d | PARAMS: %s | BODY: {\n", root->flabel, root->return_type, list_nodearray(root->n_params, root->fnparams));
			list_stmts(root->n_stmts, root->fnbody);
			printf("})");
			break;
		case AST_FUNCTION_CALL:
			printf("(FUNCTION CALL: %s | ARGS: %s) ", root->call_label, list_nodearray(root->n_args, root->callargs));
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
			printf("| THEN: {\n");
			list_stmts(root->n_while_stmts, root->while_body);
			printf("})");
			break;
		case AST_RETURN_STMT:
			printf("(RETURN STATEMENT: ");
			traverse(root->retval);
			printf(") ");
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

static char *list_nodearray(size_t n, Node **buffer)
{
	if (n == 0) {
		return "";
	}

	char *ret = (char *) malloc(1);
	size_t ret_size = 0;

	char *s = NULL;

	for (size_t i = 0; i < n; i++) {
		switch (buffer[i]->type)
		{
			case AST_IDENT:
				ret = realloc(ret, ret_size + strlen(buffer[i]->name) + 2);
				strcpy(&ret[ret_size], buffer[i]->name);
				ret_size += strlen(buffer[i]->name);
				break;
			case AST_INT:
				s = (char *) malloc(11);	// INT_MAX has 10 digits
				sprintf(s, "%d", buffer[i]->ival);
			        ret = realloc(ret, ret_size + strlen(s) + 2);
				strcpy(&ret[ret_size], s);
				ret_size += strlen(s);
				break;
			case AST_FLOAT:
				s = (char *) malloc(20);
				sprintf(s, "%f", buffer[i]->fval);
			        ret = realloc(ret, ret_size + strlen(s) + 2);
				strcpy(&ret[ret_size], s);
				ret_size += strlen(s);
				break;

			case AST_STRING:
#define sval buffer[i]->sval
				ret = realloc(ret, ret_size + strlen(sval) + 2);
				strcpy(&ret[ret_size], sval);
				ret_size += strlen(sval);
				break;
#undef sval
			case AST_ARRAY:
				s = (char *) calloc(3, 1);	// "[]"
				s[0] = '[';
				char *elems = list_nodearray(buffer[i]->array_size, buffer[i]->array_elems);
				s = realloc(s, strlen(elems) + 3);
				strcat(s+1, elems);
				s[strlen(s)] = ']';
				s[strlen(s)] = '\0';

				ret = realloc(ret, ret_size + strlen(s) + 2);
				strcpy(&ret[ret_size], s);
				ret_size += strlen(s);

				break;
			case AST_FUNCTION_CALL:
				char *label = (char *) malloc(strlen(buffer[i]->call_label));
				strcpy(label, buffer[i]->call_label);
				char *args = list_nodearray(buffer[i]->n_args, buffer[i]->callargs);
				s = (char *) malloc(29 + strlen(label) + strlen(args) + 1);

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

static Node *ast_arraytype(size_t sz, Node **arr)
{
	return makeNode(&(Node){AST_ARRAY, .array_size=sz, .array_elems=arr});
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

static Node *ast_decl(char *label, int type)
{
	return makeNode(&(Node){AST_DECLARATION, .vlabel=label, .vtype=type});
}
 
static Node *ast_funcdef(char *label, int ret_type, size_t params_n, size_t stmts_n, Node **params, Node **body)
{
	return makeNode(&(Node){AST_FUNCTION_DEF, .flabel=label, .return_type=ret_type, .n_params=params_n, .n_stmts=stmts_n, .fnparams=params, .fnbody=body});
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

static Node *ast_ret_stmt(Node *ret_val)
{
	return makeNode(&(Node){AST_RETURN_STMT, .retval=ret_val});
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

		return ast_funcdef(flabel, ret_type, params_n, stmts_n, params, body);
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
		Node *n = read_secondary_expr();
		if (n == NULL) {
			break;
		}
		
		if (!is_stmt_node(n))
		{
			Token_type *tok = get();
			if (tok->class != ';') {
				printf("Error: Missing ';'.\n");
				return NULL;
			}
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
		next();
		if (!expect(')')) {
			printf("Error: Expected ')' at end of expression.\n");
		}
		return r;
	}

	if (expect('[')) {
		Node *r = read_array_expr();
		return r;
	}

	switch (tok->class) {
		case INT: return read_int(tok);
		case FLOAT: return read_float(tok);
		case IDENTIFIER: return read_ident(tok);
		case STRING: return read_string(tok);
		default: 
			     unget();
			     return NULL;
	}
}

static Node *read_secondary_expr()
{
	Node *r = read_stmt();
	if (r == NULL) {
		r = read_declaration_expr();
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
	Token_type *tok = get();

	if (tok->class != '(') {
		printf("Error: '(' expected after keyword 'if'.\n");
		return NULL;
	}

	Node *cond = read_relational_expr();

	tok  = get();
	if (tok->class != ')') {
		printf("Error: ')' expected after keyword 'if'.\n");
		return NULL;
	}

	tok = get();
	if (tok->class != '{') {
		printf("Error: '{' expected after keyword 'if'.\n");
		return NULL;
	}

	Node **if_body = (Node **) malloc(1);
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
			Token_type *tok = get();
			if (tok->class != ';') {
				printf("Error: Missing ';'.\n");
				return NULL;
			}
		}
	}

	tok = get();
	if (tok->class != '}') {
		printf("Error: '}' expected after keyword 'if'.\n");
		return NULL;
	}

	Node **else_body = (Node **) malloc(1);
	size_t else_body_sz = 0;

	tok = get();
	if (!strcmp("else", tok->repr)) {
		tok = get();
		if (tok->class != '{') {
			printf("Error: '{' expected after keyword 'else'.\n");
			return NULL;
		}

		for (;;) {
			Node *n = read_secondary_expr();

			if (n == NULL) {
				break;
			}
			
			else_body = realloc(else_body, sizeof(Node *) * (else_body_sz + 1));
			else_body[else_body_sz] = n;
			else_body_sz++;

			if (!is_stmt_node(n)) {
				Token_type *tok = get();
				if (tok->class != ';') {
					printf("Error: Missing ';'.\n");
					return NULL;
				}
			}
		}

		tok = get();
		if (tok->class != '}') {
			printf("Error: '}' expected after keyword 'else'.\n");
			return NULL;
		}
	} else {
		unget();
		else_body = NULL;
	}

	return ast_if_stmt(cond, if_body_sz, else_body_sz, if_body, else_body);
}

static Node *read_while_stmt()
{
	Token_type *tok = get();

	if (tok->class != '(') {
		printf("Error: '(' expected after keyword 'if'.\n");
		return NULL;
	}

	Node *cond = read_relational_expr();

	tok  = get();
	if (tok->class != ')') {
		printf("Error: ')' expected after keyword 'if'.\n");
		return NULL;
	}

	tok = get();
	if (tok->class != '{') {
		printf("Error: '{' expected after keyword 'if'.\n");
		return NULL;
	}

	Node **while_body = (Node **) malloc(1);
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
			Token_type *tok = get();
			if (tok->class != ';') {
				printf("Error: Missing ';'.\n");
				return NULL;
			}
		}
	}

	tok = get();
	if (tok->class != '}') {
		printf("Error: '}' expected after keyword 'if'.\n");
		return NULL;
	}

	return ast_while_stmt(cond, while_body_sz, while_body);
}

static Node *read_for_stmt()
{
	return NULL;
}

static Node *read_return_stmt()
{
	Node *n = read_expr();
	
	if (n == NULL) {
		if (prev()->class != ';') {
			printf("Error: ';' expected.\n");
		}
	} else {
		Token_type *tok = get();
		if (tok->class != ';') {
			printf("Error: ';' expected.\n");
		}
	}

	return ast_ret_stmt(n);
}

static Node *read_fn_call()
{
	Token_type *tok = prev();
	if (tok->class == IDENTIFIER) {
		char *label = (char *) malloc(strlen(tok->repr));
		strcpy(label, tok->repr);

		tok = get();
		if (tok->class == '(') {
			Node **args = (Node **) malloc(1);
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
				printf("Error: Closing ')' expected.\n");
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

static Node *read_declaration_expr()
{
	Token_type *tok = get();
	int type;
	if ((type = is_type_specifier(tok))) {
		tok = get();
		if (tok->class == IDENTIFIER) {
			char *label = (char *) malloc(strlen(tok->repr));
			strcpy(label, tok->repr);
			Node *lhs = ast_decl(label, type);

			tok = get();
			if (tok->class == '=') {
				return ast_binop('=', lhs, read_expr());
			} else {
				unget();
				return lhs;
			}
		}
	}

	unget();
	return NULL;
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

static Node *read_array_expr()
{
	Node **array = (Node **) malloc(1);
	size_t array_sz = 0;
	Token_type *tok;
	for (;;) {
		Node *e = read_primary_expr();
		if (e == NULL) {
			printf("Error: Unexpected token in array expression.\n");
			return NULL;
		}

		array = realloc(array, sizeof(Node *) * (array_sz + 1));
		array[array_sz] = e;
		array_sz++;

		tok = get();
		if (tok->class != ',') {
			if (tok->class != ']') {
				printf("Error: Unexpected token in array expression.\n");
				return NULL;
			}
			break;
		}
	}

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
	char *s = (char *) malloc(strlen(tok->repr));
	strcpy(s, tok->repr);

	return ast_stringtype(s);
}

static Node *read_ident(Token_type *tok)
{
	if (curr()->class == '(') {
		return read_fn_call();
	}
	char *s = (char *) malloc(strlen(tok->repr));
	strcpy(s, tok->repr);

	return ast_identtype(s);
}
