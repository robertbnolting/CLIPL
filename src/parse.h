enum {
	KEYWORD_IF = 1,
	KEYWORD_WHILE,
	KEYWORD_FOR,
	KEYWORD_RETURN,
};

enum {
	TYPE_VOID = 1,
	TYPE_INT,
	TYPE_FLOAT,
	TYPE_STRING,
	TYPE_BOOL,
	TYPE_ARRAY,
	TYPE_RECORD,
};

enum {
	AST_IDENT,
	AST_INT,
	AST_FLOAT,
	AST_STRING,
	AST_BOOL,
	AST_ARRAY,
	AST_IDX_ARRAY,
	AST_FIELD_ACCESS,
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
	AST_RECORD_DEF,
	AST_FUNCTION_DEF,
	AST_FUNCTION_CALL,
	AST_IF_STMT,
	AST_WHILE_STMT,
	AST_FOR_STMT,
	AST_RETURN_STMT,
	CFG_AUXILIARY_NODE,
	// only necessary for printing cfg
	CFG_JOIN_NODE,
};

typedef struct Node {
	int type;
	struct Node *successor;
	union {
		// identifier
		char *name;
		// int
		int ival;
		// float
		float fval;
		// string
		char *sval;
		// bool
		int bval;
		// array
		struct {
			size_t array_size;
			struct Node **array_elems;
		};
		// indexed array
		struct {
			char *ia_label;
			struct Node **index_values;
			int ndim_index;
		};
		// field access
		struct {
			struct Node *access_rlabel;
			struct Node *access_field;
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
			char *vrlabel;
			int v_array_dimensions;
			int *varray_size;
		};
		// record definition
		struct {
			char *rlabel;
			size_t n_rfields;
			struct Node **rfields;
		};
		// function definition
		struct {
			char *flabel;
			int return_type;
			int ret_array_dims;
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
			// cfg
			struct Node *false_successor;
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
