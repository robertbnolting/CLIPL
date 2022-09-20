enum {
	TYPE_VOID = 1,
	TYPE_INT,
	TYPE_FLOAT,
	TYPE_STRING,
	TYPE_ARRAY,
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
	AST_FLOAT,
	AST_STRING,
	AST_ARRAY,
	AST_IDX_ARRAY,
	AST_ADD,
	AST_SUB,
	AST_MUL,
	AST_DIV,
	AST_ASSIGN,
	AST_ADD_ASSIGN,
	AST_SUB_ASSIGN,
	AST_MUL_ASSIGN,
	AST_DIV_ASSIGN,
	AST_GT,
	AST_LT,
	AST_EQ,
	AST_NE,
	AST_GE,
	AST_LE,
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
		// identifier
		char *name;
		// int
		int ival;
		// float
		float fval;
		// string
		char *sval;
		// array
		struct {
			size_t array_size;
			struct Node **array_elems;
		};
		// indexed array
		struct {
			char *ia_label;
			struct Node *index_value;
		};
		// binary operator
		struct {
			struct Node *left;
			struct Node *right;
		};
		// declaration
		struct {
			char *vlabel;
			int vtype;
			int v_array_dimensions;
			int *varray_size;
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
		// while statement
		struct {
			struct Node *while_cond;
			size_t n_while_stmts;
			struct Node **while_body;
		};
		// for statement
		struct {
			struct Node *for_iterator;
			struct Node *for_enum;
			size_t n_for_stmts;
			struct Node **for_body;
		};
		// return statement
		struct Node *retval;
	};
} Node;

void parser_init();
