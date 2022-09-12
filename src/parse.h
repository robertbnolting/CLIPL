enum {
	TYPE_VOID = 1,
	TYPE_INT,
	TYPE_FLOAT,
	TYPE_STRING,
};

enum {
	KEYWORD_IF = 1,
	KEYWORD_WHILE,
	KEYWORD_FOR,
	KEYWORD_RETURN,
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
	AST_EQ,
	AST_DECLARATION,
	AST_FUNCTION_DEF,
	AST_FUNCTION_CALL,
	AST_IF_STMT,
	AST_WHILE_STMT,
	AST_FOR_STMT,
	AST_RETURN_STMT,
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
		// declaration
		struct {
			char *vlabel;
			int vtype;
			//struct Node *value;
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
		// if statement
		struct {
			struct Node *if_cond;
			size_t n_if_stmts;
			size_t n_else_stmts;
			struct Node **if_body;
			struct Node **else_body;
		};
	};
} Node;

void parser_init();
