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
	AST_STRING,
	AST_ADD,
	AST_SUB,
	AST_MUL,
	AST_DIV,
	AST_ASSIGN,
	AST_FUNCTION,
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
		// function
		struct {
			char *flabel;
			size_t n_params;
			struct Node **fnparams;
			struct Node *fnbody;
		};
	};
} Node;

void parser_init();
