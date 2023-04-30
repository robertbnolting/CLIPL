enum {
       KEYWORD_IF = 1,
       KEYWORD_WHILE,
       KEYWORD_FOR,
       KEYWORD_RETURN,
};

enum {
	TYPE_INT = 1,
	TYPE_FLOAT,
	TYPE_STRING,
	TYPE_BOOL,
	TYPE_ARRAY,
	TYPE_RECORD,
	TYPE_VOID,
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
	AST_MOD,
	AST_ASSIGN,
	AST_ADD_ASSIGN,
	AST_SUB_ASSIGN,
	AST_MUL_ASSIGN,
	AST_DIV_ASSIGN,
	AST_MOD_ASSIGN,
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
	CFG_AUXILIARY_NODE, 	// also serve as empty nodes
	CFG_JOIN_NODE,
};

typedef struct {
	void **start;
	void **top;
	size_t size;
} Stack;

typedef struct {
	void **array;
	size_t size;
} Vector;

struct Node;

typedef struct ValPropPair {
	char *var_name;
	int status;	// 0 -> Uninitialized, 1 -> Initialized, 2 -> MaybeInitialized, -1 -> Value not constant
	int type;
	struct Node *ref_node;
	union {
		int ival;
		struct {
			char *sval;
			size_t slen;
			int s_allocated;
		};
		float fval;
		int bval;
		struct {
			int array_type;
			int array_dims;
			int *array_size;
			struct Node **array_elems;
			// used in generation
			size_t array_len;
		};
		Vector record_vec;
	};
	// indexed array
	struct ValPropPair *ref_array;
	// generation
	int loff;
	char *asmlabel;
} ValPropPair;

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
		struct {
			char *sval;
			size_t slen;
			// used in generation
			char *slabel;
			int s_allocated;
		};
		// bool
		int bval;
		// array
		struct {
			size_t array_size;
			struct Node **array_elems;
			// used in optimizer
			int array_member_type;
			int array_dims;
			// used in generation
			char *alabel;
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
			// context checking and generation
			int result_type;
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
			int global_idx;
			// used in generation
			int is_fn_entrypoint;
			int is_called;
			int *called_to;
			size_t n_called_to;
			struct Node *return_stmt;
			int start_body;
			int end_body;
		};
		// function call
		struct {
			char *call_label;
			size_t n_args;
			struct Node **callargs;
			int global_function_idx;
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
			// cfg
			struct Node *while_true_successor;
		};
		// for statement
		struct {
			struct Node *for_iterator;
			struct Node *for_enum;
			size_t n_for_stmts;
			struct Node **for_body;
			// cfg
			struct Node *for_loop_successor;
		};
		// return statement
		struct {
			struct Node *retval;
			int rettype;
		};
	};
	ValPropPair *lvar_valproppair;
} Node;

void parser_init();
int numPlaces();

extern Node **global_functions;
extern size_t global_function_count;
