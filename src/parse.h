enum {
	TYPE_VOID = 1,
	TYPE_INT,
	TYPE_FLOAT,
	TYPE_STRING,
};

enum {
	AST_IDENT,
	AST_INT,
	AST_STRING,
	AST_ADD,
	AST_SUB,
	AST_MUL,
	AST_DIV,
	AST_ASSIGN,
	AST_FUNCTION_DEF,
	AST_FUNCTION_CALL,
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
		// function definition
		struct {
			char *flabel;
			int return_type;
			size_t n_params;
			size_t n_stmts;
			struct Node **fnparams;
			struct Node **fnbody;
		};
		// function call
		struct {
			char *call_label;
			size_t n_args;
			struct Node **callargs;
		};
	};
} Node;

void parser_init();
