enum {
	TYPE_VOID,
	TYPE_INT,
	TYPE_FLOAT,
	TYPE_STRING,
};

enum {
	AST_VOID,
	AST_IDENT,
	AST_INT,
	AST_ADD,
	AST_SUB,
	AST_MUL,
	AST_DIV,
	AST_ASSIGN,
};

typedef struct Node {
	int type;
	union {
		// int
		int ival;
		// float
		float fval;
		// string
		char *sval;
		// identifier
		char *name;
		// binary operator
		struct {
			struct Node *left;
			struct Node *right;
		};
	};
} Node;

void parser_init();
