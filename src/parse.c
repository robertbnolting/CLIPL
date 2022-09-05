#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "parse.h"
#include "lex.h"

static int pos;

#define curr()	(&Token_stream[pos])
#define get()	(&Token_stream[pos++])
#define unget()	(pos--)
#define peek()	(&Token_stream[pos+1])
#define prev()	(&Token_stream[pos-1])

#define is_arithtype(t)	(AST_ADD <= t && t <= AST_DIV)

static int expect();

static Node *read_primary_expr();
static Node *read_expr();
static Node *read_assignment_expr();
static Node *read_additive_expr();
static Node *read_multiplicative_expr();
static Node *read_int();
static Node *read_ident();

static void traverse();

void parser_init()
{
	pos = 0;

	Node *root = read_primary_expr();

	traverse(root);
	printf("\n");
}

static void traverse(Node *root)
{
	switch (root->type)
	{
		case AST_INT:
			printf("(INT: %d) ", root->ival);
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
	}
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

static int expect(int tclass)
{
	Token_type *t = prev();
	if (t->class != tclass) {
		return 0;
	}
	
	return 1;
}

static Node *read_primary_expr()
{
	Token_type *tok = get();
	
	if (expect('(')) {
		Node *r = read_expr();
		get();
		if (!expect(')')) {
			printf("Error: Unexpected token.\n");
		}
		return r;
	}

	switch (tok->class) {
		case INT: return read_int(tok);
		case IDENTIFIER: return read_ident(tok);
		default: return NULL;
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

static Node *read_ident(Token_type *tok)
{
	char *s = (char *) malloc(strlen(tok->repr));
	strcpy(s, tok->repr);

	return ast_identtype(s);
}

static int eval_intexpr(Node *Node)
{
	switch (Node->type)
	{
		case TYPE_INT: return Node->ival;
#define L (eval_intexpr(Node->left))
#define R (eval_intexpr(Node->right))
		case '+': return L + R;
		case '-': return L - R;
		case '*': return L * R;
		case '/': return L / R;
#undef L
#undef R
		default: return 0;
	}
}
